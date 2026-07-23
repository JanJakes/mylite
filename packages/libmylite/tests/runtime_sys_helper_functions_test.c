#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#else
#  include <unistd.h>
#endif

enum {
    test_path_capacity = 1024,
    path_suffix_capacity = 16,
    mysql_error_incorrect_parameter_count = 1582,
    mysql_error_invalid_use_of_null = 1138,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_query {
    const char *sql;
    const char *const *columns;
    size_t column_count;
    const char *const *values;
    size_t row_count;
    const char *context;
};

static int test_sys_helper_no_source_and_dual(void);
static int test_sys_helper_table_backed_contexts(void);
static int test_sys_helper_diagnostics(void);
static int test_native_format_helpers(void);
static int open_app_database(
    mylite_db **out_database,
    const char *name,
    char *path,
    size_t path_size
);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_query_with_warning_count(
    mylite_db *database,
    struct expected_query expected,
    size_t expected_warning_count
);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_sys_helper_no_source_and_dual();
    failures += test_sys_helper_table_backed_contexts();
    failures += test_sys_helper_diagnostics();
    failures += test_native_format_helpers();

    return failures == 0 ? 0 : 1;
}

static int test_sys_helper_no_source_and_dual(void) {
    static const char *const columns[] = {
        "b0",
        "b1",
        "bk",
        "tm0",
        "tm1",
        "schema_name",
        "table_name",
        "identifier",
        "cfg",
        "cfg_fallback",
        "cfg_null_name",
        "cfg_null_value",
        "major",
        "minor",
        "patch",
        "dropped_first",
        "dropped_middle",
    };
    static const char *const values[] = {
        "   0 bytes",
        "   1 bytes",
        "1.50 KiB",
        "0 ps",
        "3.5 ns",
        "world",
        "City",
        "`a``b`",
        "64",
        "fallback",
        "fallback",
        "fallback",
        "8",
        "4",
        "9",
        " b, c",
        "a, c",
    };
    static const char *const qualified_columns[] = {"fb", "ft", "qi", "major"};
    static const char *const qualified_values[] = {
        "1.00 KiB",
        "1 ms",
        "`plain`",
        "8",
    };
    static const char *const statement_columns[] = {
        "short_statement",
        "long_statement",
        "config_after_user_var",
    };
    static const char *const statement_values[] = {
        "SELECT 1",
        "SELECT variabl ... ROM sys_config",
        "64",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "no-source", path, sizeof(path));
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT "
                   "format_bytes(0) AS b0,"
                   "format_bytes(1) AS b1,"
                   "format_bytes(1536) AS bk,"
                   "format_time(0) AS tm0,"
                   "format_time(3501) AS tm1,"
                   "extract_schema_from_file_name('/usr/local/mysql/data/world/City.ibd') "
                   "AS schema_name,"
                   "extract_table_from_file_name('/usr/local/mysql/data/world/City.ibd') "
                   "AS table_name,"
                   "quote_identifier('a`b') AS identifier,"
                   "sys_get_config('statement_truncate_len','fallback') AS cfg,"
                   "sys_get_config('missing','fallback') AS cfg_fallback,"
                   "sys_get_config(NULL,'fallback') AS cfg_null_name,"
                   "sys_get_config('statement_performance_analyzer.view','fallback') "
                   "AS cfg_null_value,"
                   "version_major() AS major,"
                   "version_minor() AS minor,"
                   "version_patch() AS patch,"
                   "list_drop('a, b, c','a') AS dropped_first,"
                   "list_drop('a, b, c','b') AS dropped_middle",
            .columns = columns,
            .column_count = sizeof(columns) / sizeof(columns[0]),
            .values = values,
            .row_count = 1U,
            .context = "sys helper direct values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT sys.format_bytes(1024) AS fb, sys.format_time(1000000000) AS ft, "
                   "`sys`.`quote_identifier`('plain') AS qi, sys.version_major() AS major "
                   "FROM DUAL",
            .columns = qualified_columns,
            .column_count = sizeof(qualified_columns) / sizeof(qualified_columns[0]),
            .values = qualified_values,
            .row_count = 1U,
            .context = "sys helper qualified dual values",
        }
    );
    failures += execute_ok(database, "SET @sys.statement_truncate_len = 32", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT format_statement('SELECT 1') AS short_statement, "
                   "format_statement('SELECT variable, value, set_time FROM sys_config') "
                   "AS long_statement, "
                   "sys_get_config('statement_truncate_len','fallback') AS config_after_user_var",
            .columns = statement_columns,
            .column_count = sizeof(statement_columns) / sizeof(statement_columns[0]),
            .values = statement_values,
            .row_count = 1U,
            .context = "sys helper format statement",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_sys_helper_table_backed_contexts(void) {
    static const char *const row_columns[] = {"id", "bytes_text", "identifier", "table_name"};
    static const char *const row_values[] = {
        "1",
        "1.00 KiB",
        "`alpha`",
        "City",
        "2",
        "1.50 KiB",
        "`beta``name`",
        "Country",
    };
    static const char *const where_columns[] = {"id"};
    static const char *const where_values[] = {"2"};
    static const char *const order_columns[] = {"id", "time_text"};
    static const char *const order_values[] = {
        "2",
        "1 ms",
        "1",
        "1 ns",
    };
    static const char *const update_columns[] = {"id", "stored_value"};
    static const char *const update_values[] = {
        "1",
        "alpha,gamma",
        "2",
        "beta`name,gamma",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "table", path, sizeof(path));
    failures += execute_ok(
        database,
        "CREATE TABLE samples("
        "id INT,"
        "bytes_value INT,"
        "time_value BIGINT,"
        "path_value VARCHAR(128),"
        "identifier_value VARCHAR(32),"
        "list_value VARCHAR(64),"
        "stored_value VARCHAR(128)"
        ")",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO samples VALUES "
        "(1,1024,1000,'/usr/local/mysql/data/world/City.ibd','alpha','alpha,gamma',''),"
        "(2,1536,1000000000,'/usr/local/mysql/data/world/Country.ibd','beta`name',"
        "'alpha,beta','')",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, format_bytes(bytes_value) AS bytes_text, "
                   "quote_identifier(identifier_value) AS identifier, "
                   "extract_table_from_file_name(path_value) AS table_name "
                   "FROM samples ORDER BY id",
            .columns = row_columns,
            .column_count = sizeof(row_columns) / sizeof(row_columns[0]),
            .values = row_values,
            .row_count = 2U,
            .context = "sys helper row projection",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM samples "
                   "WHERE sys.list_drop(list_value, 'alpha') = 'beta' ORDER BY id",
            .columns = where_columns,
            .column_count = sizeof(where_columns) / sizeof(where_columns[0]),
            .values = where_values,
            .row_count = 1U,
            .context = "sys helper row predicate",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, format_time(time_value) AS time_text "
                   "FROM samples ORDER BY format_time(time_value), id",
            .columns = order_columns,
            .column_count = sizeof(order_columns) / sizeof(order_columns[0]),
            .values = order_values,
            .row_count = 2U,
            .context = "sys helper row order",
        }
    );
    failures += execute_ok(
        database,
        "UPDATE samples SET stored_value = list_add(identifier_value, 'gamma')",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, stored_value FROM samples ORDER BY id",
            .columns = update_columns,
            .column_count = sizeof(update_columns) / sizeof(update_columns[0]),
            .values = update_values,
            .row_count = 2U,
            .context = "sys helper update assignment",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_sys_helper_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += execute_error(
        database,
        "SELECT sys.list_add('a,b', NULL)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_use_of_null,
            .sqlstate = "02200",
            .message_part = "Function sys.list_add: in_add_value input variable should not be NULL",
        }
    );
    failures += execute_error(
        database,
        "SELECT sys.list_drop('a,b', NULL)",
        (struct expected_sql_error){
            .code = mysql_error_invalid_use_of_null,
            .sqlstate = "02200",
            .message_part =
                "Function sys.list_drop: in_drop_value input variable should not be NULL",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_native_format_helpers(void) {
    static const char *const columns[] = {
        "null_bytes",
        "small_bytes",
        "kib_bytes",
        "null_time",
        "zero_time",
        "ps_time",
        "ns_time",
        "fractional_time",
        "minute_time",
        "fractional_minute_time",
    };
    static const char *const values[] = {
        NULL,
        " 512 bytes",
        "1.00 KiB",
        NULL,
        "  0 ps",
        "999 ps",
        "1.00 ns",
        "3.50 ns",
        "1.00 min",
        "3.15 min",
    };
    static const char *const row_columns[] = {"id", "bytes_text", "time_text"};
    static const char *const row_values[] = {
        "1",
        "1.00 KiB",
        "1.00 ns",
        "2",
        "1.50 KiB",
        "1.00 ms",
    };
    static const char *const invalid_columns[] = {"bad_bytes", "bad_time"};
    static const char *const invalid_values[] = {"   0 bytes", "  0 ps"};
    static const char *const warning_columns[] = {"Level", "Code", "Message"};
    static const char *const warning_values[] = {
        "Warning",
        "1292",
        "Truncated incorrect DOUBLE value: 'abc'",
        "Warning",
        "1292",
        "Truncated incorrect DOUBLE value: 'abc'",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "native-format", path, sizeof(path));
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT "
                   "FORMAT_BYTES(NULL) AS null_bytes,"
                   "FORMAT_BYTES(512) AS small_bytes,"
                   "FORMAT_BYTES(1024) AS kib_bytes,"
                   "FORMAT_PICO_TIME(NULL) AS null_time,"
                   "FORMAT_PICO_TIME(0) AS zero_time,"
                   "FORMAT_PICO_TIME(999) AS ps_time,"
                   "FORMAT_PICO_TIME(1000) AS ns_time,"
                   "FORMAT_PICO_TIME(3501) AS fractional_time,"
                   "FORMAT_PICO_TIME(60000000000000) AS minute_time,"
                   "FORMAT_PICO_TIME(188732396662000) AS fractional_minute_time",
            .columns = columns,
            .column_count = sizeof(columns) / sizeof(columns[0]),
            .values = values,
            .row_count = 1U,
            .context = "native format direct values",
        }
    );
    failures += execute_ok(
        database,
        "CREATE TABLE format_samples(id INT, bytes_value BIGINT, time_value BIGINT)",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO format_samples VALUES (1,1024,1000),(2,1536,1000000000)",
        NULL
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT id, FORMAT_BYTES(bytes_value) AS bytes_text, "
                   "FORMAT_PICO_TIME(time_value) AS time_text "
                   "FROM format_samples ORDER BY id",
            .columns = row_columns,
            .column_count = sizeof(row_columns) / sizeof(row_columns[0]),
            .values = row_values,
            .row_count = 2U,
            .context = "native format row values",
        }
    );
    failures += expect_query_with_warning_count(
        database,
        (struct expected_query){
            .sql = "SELECT FORMAT_BYTES('abc') AS bad_bytes, "
                   "FORMAT_PICO_TIME('abc') AS bad_time",
            .columns = invalid_columns,
            .column_count = sizeof(invalid_columns) / sizeof(invalid_columns[0]),
            .values = invalid_values,
            .row_count = 1U,
            .context = "native format invalid values",
        },
        2U
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = warning_columns,
            .column_count = sizeof(warning_columns) / sizeof(warning_columns[0]),
            .values = warning_values,
            .row_count = 2U,
            .context = "native format warnings",
        }
    );
    failures += execute_error(
        database,
        "SELECT FORMAT_BYTES()",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_parameter_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'FORMAT_BYTES'",
        }
    );
    failures += execute_error(
        database,
        "SELECT FORMAT_PICO_TIME()",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_parameter_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count in the call to native function "
                            "'FORMAT_PICO_TIME'",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int open_app_database(
    mylite_db **out_database,
    const char *name,
    char *path,
    size_t path_size
) {
    int failures = 0;

    if (mylite_test_make_path(path, path_size, name) != 0) {
        return 1;
    }
    remove_related_files(path);
    failures += mylite_test_expect_int(mylite_open(path, out_database), MYLITE_OK, name);
    if (failures == 0) {
        failures += execute_ok(*out_database, "CREATE DATABASE app", NULL);
    }
    if (failures == 0) {
        failures += execute_ok(*out_database, "USE app", NULL);
    }
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(stderr, "%s: expected success, got %d: %s\n", sql, rc, mylite_errmsg(database));
        return 1;
    }
    if (out_result != NULL) {
        *out_result = result;
    } else {
        mylite_result_free(result);
    }
    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error, got success\n", sql);
        mylite_result_free(result);
        return 1;
    }
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, sql);
    failures += mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    return expect_query_with_warning_count(database, expected, 0U);
}

static int expect_query_with_warning_count(
    mylite_db *database,
    struct expected_query expected,
    size_t expected_warning_count
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, expected.sql, &result);

    if (failures != 0) {
        return failures;
    }
    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        expected.column_count,
        expected.context
    );
    failures += mylite_test_expect_size(
        mylite_result_row_count(result),
        expected.row_count,
        expected.context
    );
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += mylite_test_expect_size(
        mylite_result_warning_count(result),
        expected_warning_count,
        expected.context
    );

    for (size_t column = 0U; column < expected.column_count; ++column) {
        failures += mylite_test_expect_text(
            mylite_result_column_name(result, column),
            expected.columns[column],
            expected.context
        );
    }
    for (size_t row = 0U; row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            size_t value_index = (row * expected.column_count) + column;

            failures += expect_result_value(
                result,
                row,
                column,
                expected.values[value_index],
                expected.context
            );
        }
    }

    mylite_result_free(result);
    return failures;
}

static void remove_related_files(const char *path) {
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related_path[test_path_capacity + path_suffix_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related_path)) {
        return;
    }
    (void)remove(related_path);
}

static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
) {
    const char *actual = mylite_result_value_text(result, row, column);

    if (expected == NULL) {
        if (actual != NULL) {
            fprintf(
                stderr,
                "%s: row %zu column %zu expected NULL, got [%s]\n",
                context,
                row,
                column,
                actual
            );
            return 1;
        }
        return 0;
    }
    if (actual == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: row %zu column %zu expected [%s], got [%s]\n",
            context,
            row,
            column,
            expected,
            actual == NULL ? "NULL" : actual
        );
        return 1;
    }
    return 0;
}
