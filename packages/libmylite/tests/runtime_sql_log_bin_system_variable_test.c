#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include "runtime/mylite_mysql_server_identity.h"

#include "runtime/mylite_connection.h"
#include "storage/mylite_file_format.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#else
#  include <unistd.h>
#endif

#ifndef P_tmpdir
#  define P_tmpdir "."
#endif

enum {
    test_path_capacity = 1024,
    path_suffix_capacity = 16,
    sql_log_bin_value_column_count = 5,
    sql_log_bin_label_column_count = 5,
    sql_log_bin_diagnostics_column_count = 4,
    sql_log_bin_selected_column_count = 2,
    sql_log_bin_independent_column_count = 3,
    mysql_error_parse = 1064,
    mysql_error_unknown_system_variable = 1193,
    mysql_error_session_variable_only = 1238,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_result {
    const char *const *columns;
    const char *const *values;
    size_t count;
    const char *context;
};

static int test_sql_log_bin_values_and_persistence(void);
static int test_sql_log_bin_qualifiers_and_errors(void);
static int test_independent_sql_log_bin_handles(void);
static int expect_result(const mylite_result *result, struct expected_result expected);
static int expect_query_result(
    mylite_db *database,
    const char *sql,
    struct expected_result expected
);
static int expect_show_count_warnings(
    mylite_db *database,
    const char *expected,
    const char *context
);
static int expect_show_count_errors(mylite_db *database, const char *expected, const char *context);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_sql_log_bin_values_and_persistence();
    failures += test_sql_log_bin_qualifiers_and_errors();
    failures += test_independent_sql_log_bin_handles();

    return failures == 0 ? 0 : 1;
}

static int test_sql_log_bin_values_and_persistence(void) {
    static const char *const value_columns[] = {
        "@@sql_log_bin",
        "@@session.sql_log_bin",
        "@@local.sql_log_bin",
        "@@warning_count",
        "ROW_COUNT()",
    };
    static const char *const value_values[] = {"1", "1", "1", "0", "-1"};
    static const char *const set_off_values[] = {"0", "0", "0", "0", "0"};
    static const char *const set_on_values[] = {"1", "1", "1", "0", "0"};
    static const char *const label_columns[] = {
        "@@SQL_LOG_BIN",
        "@@Session.Sql_Log_Bin",
        "@@session.`sql_log_bin`",
        "@@`sql_log_bin`",
        "(@@sql_log_bin)",
    };
    static const char *const label_values[] = {"1", "1", "1", "1", "1"};
    static const char *const mixed_columns[] = {
        "@@sql_log_bin",
        "@@foreign_key_checks",
        "@@unique_checks",
        "@@updatable_views_with_limit",
        "@@sql_auto_is_null",
        "@@sql_big_selects",
        "@@sql_generate_invisible_primary_key",
        "@@sql_buffer_result",
        "@@sql_safe_updates",
        "@@sql_select_limit",
        "@@sql_notes",
        "@@sql_warnings",
        "@@sql_quote_show_create",
        "@@autocommit",
        "@@default_storage_engine",
        "@@character_set_server",
        "@@version_comment",
    };
    static const char *const mixed_values[] = {
        "1",
        "1",
        "1",
        "YES",
        "0",
        "1",
        "0",
        "0",
        "0",
        "18446744073709551615",
        "1",
        "0",
        "1",
        "1",
        "InnoDB",
        "utf8mb4",
        MYLITE_MYSQL_SERVER_VERSION_COMMENT_STRING,
    };
    static const char *const diagnostics_columns[] = {
        "@@sql_log_bin",
        "@@warning_count",
        "@@error_count",
        "ROW_COUNT()",
    };
    static const char *const warning_values[] = {"1", "1", "0", "-1"};
    static const char *const error_values[] = {"1", "1", "1", "-1"};
    static const char *const selected_columns[] = {"@@sql_log_bin", "DATABASE()"};
    static const char *const selected_values[] = {"0", "app"};
    static const char *const show_variable_columns[] = {"Variable_name", "Value"};
    static const char *const show_session_values[] = {"sql_log_bin", "OFF"};
    static const char *const table_columns[] = {"id", "score"};
    static const char *const table_values[] = {"2", "30"};
    static const char *const null_count_columns[] = {"COUNT(*)"};
    static const char *const null_count_values[] = {"2"};
    static const char *const null_row_columns[] = {"id"};
    static const char *const null_row_values[] = {"1"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation = 0U;
    uint64_t sqlite_schema_generation = 0U;
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "lifecycle") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open sql log bin file");
    session = mylite_connection_session_state(database);
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;

    failures += execute_ok(
        database,
        "SELECT @@sql_log_bin, @@session.sql_log_bin, @@local.sql_log_bin, "
        "@@warning_count, ROW_COUNT() FROM DUAL",
        &result
    );
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = value_columns,
            .values = value_values,
            .count = sql_log_bin_value_column_count,
            .context = "sql log bin values",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "SELECT @@SQL_LOG_BIN, @@Session.Sql_Log_Bin, "
        "@@session.`sql_log_bin`, @@`sql_log_bin`, (@@sql_log_bin)",
        &result
    );
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = label_columns,
            .values = label_values,
            .count = sql_log_bin_label_column_count,
            .context = "sql log bin labels",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "SELECT @@sql_log_bin, @@foreign_key_checks, @@unique_checks, "
        "@@updatable_views_with_limit, @@sql_auto_is_null, @@sql_big_selects, "
        "@@sql_generate_invisible_primary_key, @@sql_buffer_result, "
        "@@sql_safe_updates, @@sql_select_limit, @@sql_notes, @@sql_warnings, "
        "@@sql_quote_show_create, @@autocommit, @@default_storage_engine, "
        "@@character_set_server, @@version_comment",
        &result
    );
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = mixed_columns,
            .values = mixed_values,
            .count = sizeof(mixed_columns) / sizeof(mixed_columns[0]),
            .context = "mixed sql log bin scalar values",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_statement_ok(database, "SHOW PROCESSLIST");
    failures += execute_ok(
        database,
        "SELECT @@sql_log_bin, @@warning_count, @@error_count, ROW_COUNT()",
        &result
    );
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = diagnostics_columns,
            .values = warning_values,
            .count = sql_log_bin_diagnostics_column_count,
            .context = "sql log bin warning diagnostics",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_show_count_warnings(database, "0", "sql log bin clears warnings");

    failures += execute_error(
        database,
        "BAD SQL",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "BAD",
        }
    );
    failures += execute_ok(
        database,
        "SELECT @@sql_log_bin, @@warning_count, @@error_count, ROW_COUNT()",
        &result
    );
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = diagnostics_columns,
            .values = error_values,
            .count = sql_log_bin_diagnostics_column_count,
            .context = "sql log bin error diagnostics",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_show_count_errors(database, "0", "sql log bin clears errors");
    failures += expect_show_count_warnings(database, "0", "sql log bin clears error warnings");

    session = mylite_connection_session_state(database);
    failures += mylite_test_expect_int64(
        (int64_t)session->catalog_generation,
        (int64_t)catalog_generation,
        "catalog generation unchanged by sql log bin reads"
    );
    failures += mylite_test_expect_int64(
        (int64_t)session->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "sqlite schema generation unchanged by sql log bin reads"
    );
    failures += mylite_test_expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read sql log bin preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after sql log bin reads"
    );

    failures += execute_statement_ok(database, "SET SESSION sql_log_bin=0");
    failures += expect_query_result(
        database,
        "SELECT @@sql_log_bin, @@session.sql_log_bin, @@local.sql_log_bin, "
        "@@warning_count, ROW_COUNT()",
        (struct expected_result){
            .columns = value_columns,
            .values = set_off_values,
            .count = sql_log_bin_value_column_count,
            .context = "set sql log bin values",
        }
    );
    failures += expect_query_result(
        database,
        "SHOW VARIABLES LIKE 'sql_log_bin'",
        (struct expected_result){
            .columns = show_variable_columns,
            .values = show_session_values,
            .count = sizeof(show_variable_columns) / sizeof(show_variable_columns[0]),
            .context = "show session sql log bin value",
        }
    );
    failures += execute_ok(database, "SHOW GLOBAL VARIABLES LIKE 'sql_log_bin'", &result);
    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        sizeof(show_variable_columns) / sizeof(show_variable_columns[0]),
        "show global sql log bin omitted column count"
    );
    failures += mylite_test_expect_size(
        mylite_result_row_count(result),
        0U,
        "show global sql log bin omitted row count"
    );
    failures += mylite_test_expect_text_or_null(
        mylite_result_column_name(result, 0U),
        show_variable_columns[0],
        "show global sql log bin omitted variable column"
    );
    failures += mylite_test_expect_text_or_null(
        mylite_result_column_name(result, 1U),
        show_variable_columns[1],
        "show global sql log bin omitted value column"
    );
    failures += mylite_test_expect_size(
        mylite_result_warning_count(result),
        0U,
        "show global sql log bin omitted warning count"
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_statement_ok(database, "SET LOCAL sql_log_bin=TRUE");
    failures += expect_query_result(
        database,
        "SELECT @@sql_log_bin, @@session.sql_log_bin, @@local.sql_log_bin, "
        "@@warning_count, ROW_COUNT()",
        (struct expected_result){
            .columns = value_columns,
            .values = set_on_values,
            .count = sql_log_bin_value_column_count,
            .context = "set local sql log bin TRUE values",
        }
    );
    failures += execute_statement_ok(database, "SET @@session.sql_log_bin=FALSE");
    failures += expect_query_result(
        database,
        "SELECT @@sql_log_bin, @@session.sql_log_bin, @@local.sql_log_bin, "
        "@@warning_count, ROW_COUNT()",
        (struct expected_result){
            .columns = value_columns,
            .values = set_off_values,
            .count = sql_log_bin_value_column_count,
            .context = "set @@session sql log bin FALSE values",
        }
    );
    failures += execute_statement_ok(database, "SET SESSION sql_log_bin=DEFAULT");
    failures += expect_query_result(
        database,
        "SELECT @@sql_log_bin, @@session.sql_log_bin, @@local.sql_log_bin, "
        "@@warning_count, ROW_COUNT()",
        (struct expected_result){
            .columns = value_columns,
            .values = set_on_values,
            .count = sql_log_bin_value_column_count,
            .context = "set sql log bin DEFAULT values",
        }
    );
    failures += execute_statement_ok(database, "SET SESSION sql_log_bin=OFF");

    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(database, "CREATE TABLE child (id INT, score INT)");
    failures += execute_statement_ok(
        database,
        "INSERT INTO child (id, score) VALUES (1, NULL),(2, 20),(3, NULL)"
    );
    failures += execute_statement_ok(database, "UPDATE child SET score = 30 WHERE id = 2");
    failures += expect_query_result(
        database,
        "SELECT @@sql_log_bin, DATABASE()",
        (struct expected_result){
            .columns = selected_columns,
            .values = selected_values,
            .count = sql_log_bin_selected_column_count,
            .context = "sql log bin with selected database",
        }
    );
    failures += expect_query_result(
        database,
        "SELECT id, score FROM child WHERE id = 2 ORDER BY id",
        (struct expected_result){
            .columns = table_columns,
            .values = table_values,
            .count = sizeof(table_columns) / sizeof(table_columns[0]),
            .context = "sql log bin does not alter descriptor select",
        }
    );
    failures += expect_query_result(
        database,
        "SELECT id, score FROM child WHERE id = 2 ORDER BY id LIMIT 1",
        (struct expected_result){
            .columns = table_columns,
            .values = table_values,
            .count = sizeof(table_columns) / sizeof(table_columns[0]),
            .context = "explicit descriptor select limit still applies with sql log bin",
        }
    );
    failures += expect_query_result(
        database,
        "SELECT COUNT(*) FROM child WHERE score IS NULL",
        (struct expected_result){
            .columns = null_count_columns,
            .values = null_count_values,
            .count = sizeof(null_count_columns) / sizeof(null_count_columns[0]),
            .context = "sql log bin does not reject descriptor count",
        }
    );
    failures += expect_query_result(
        database,
        "SELECT id FROM child WHERE score IS NULL ORDER BY id LIMIT 1",
        (struct expected_result){
            .columns = null_row_columns,
            .values = null_row_values,
            .count = sizeof(null_row_columns) / sizeof(null_row_columns[0]),
            .context = "sql log bin does not reject descriptor select",
        }
    );

    mylite_close(database);
    database = NULL;

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen sql log bin file");
    failures += expect_query_result(
        database,
        "SELECT @@sql_log_bin, @@session.sql_log_bin",
        (struct expected_result){
            .columns = value_columns,
            .values = value_values,
            .count = 2U,
            .context = "reopened sql log bin values",
        }
    );
    failures += expect_query_result(
        database,
        "SELECT id, score FROM app.child WHERE id = 2 ORDER BY id",
        (struct expected_result){
            .columns = table_columns,
            .values = table_values,
            .count = sizeof(table_columns) / sizeof(table_columns[0]),
            .context = "reopened sql log bin table rows",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_sql_log_bin_qualifiers_and_errors(void) {
    static const char *const scoped_columns[] = {
        "@@SQL_LOG_BIN",
        "@@SESSION.SQL_LOG_BIN",
        "@@Local.Sql_Log_Bin",
        "@@session.`sql_log_bin`",
        "(@@sql_log_bin)",
    };
    static const char *const scoped_values[] = {"1", "1", "1", "1", "1"};
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures +=
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open sql log bin memory");
    failures += execute_ok(
        database,
        "SELECT @@SQL_LOG_BIN, @@SESSION.SQL_LOG_BIN, "
        "@@Local.Sql_Log_Bin, @@session.`sql_log_bin`, "
        "(@@sql_log_bin)",
        &result
    );
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = scoped_columns,
            .values = scoped_values,
            .count = sql_log_bin_label_column_count,
            .context = "sql log bin qualifiers",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_error(
        database,
        "SELECT @@global.sql_log_bin",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "Variable 'sql_log_bin' is a SESSION variable",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@no_such_sql_log_bin_variable",
        (struct expected_sql_error){
            .code = mysql_error_unknown_system_variable,
            .sqlstate = "HY000",
            .message_part = "Unknown system variable 'no_such_sql_log_bin_variable'",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@session.no_such_sql_log_bin_variable",
        (struct expected_sql_error){
            .code = mysql_error_unknown_system_variable,
            .sqlstate = "HY000",
            .message_part = "Unknown system variable 'no_such_sql_log_bin_variable'",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@`session`.sql_log_bin",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "quoted system variable scope",
        }
    );
    failures += execute_statement_ok(database, "SELECT @@sql_log_bin + 1");

    mylite_close(database);
    return failures;
}

static int test_independent_sql_log_bin_handles(void) {
    static const char *const columns[] = {
        "@@sql_log_bin",
        "@@warning_count",
        "@@error_count",
    };
    static const char *const first_values[] = {"0", "1", "0"};
    static const char *const second_values[] = {"1", "0", "0"};
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += mylite_test_expect_int(
        mylite_open_memory(&first),
        MYLITE_OK,
        "open first sql log bin handle"
    );
    failures += mylite_test_expect_int(
        mylite_open_memory(&second),
        MYLITE_OK,
        "open second sql log bin handle"
    );
    failures += execute_statement_ok(first, "SET SESSION sql_log_bin=0");
    failures += execute_statement_ok(first, "SHOW PROCESSLIST");

    failures += execute_ok(first, "SELECT @@sql_log_bin, @@warning_count, @@error_count", &result);
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = columns,
            .values = first_values,
            .count = sql_log_bin_independent_column_count,
            .context = "first handle sql log bin variables",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(second, "SELECT @@sql_log_bin, @@warning_count, @@error_count", &result);
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = columns,
            .values = second_values,
            .count = sql_log_bin_independent_column_count,
            .context = "second handle sql log bin variables",
        }
    );
    mylite_result_free(result);

    mylite_close(second);
    mylite_close(first);
    return failures;
}

static int expect_result(const mylite_result *result, struct expected_result expected) {
    int failures = 0;

    if (result == NULL) {
        fprintf(stderr, "%s: expected result, got NULL\n", expected.context);
        return 1;
    }

    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        expected.count,
        expected.context
    );
    failures += mylite_test_expect_size(mylite_result_row_count(result), 1U, expected.context);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, expected.context);
    for (size_t index = 0U; index < expected.count; ++index) {
        failures += mylite_test_expect_text_or_null(
            mylite_result_column_name(result, index),
            expected.columns[index],
            expected.context
        );
        failures += mylite_test_expect_text_or_null(
            mylite_result_value_text(result, 0U, index),
            expected.values[index],
            expected.context
        );
    }

    return failures;
}

static int expect_query_result(
    mylite_db *database,
    const char *sql,
    struct expected_result expected
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_result(result, expected);
    mylite_result_free(result);
    return failures;
}

static int expect_show_count_warnings(
    mylite_db *database,
    const char *expected,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, "SHOW COUNT(*) WARNINGS", &result);

    failures += mylite_test_expect_size(mylite_result_row_count(result), 1U, context);
    failures += mylite_test_expect_text_or_null(
        mylite_result_value_text(result, 0U, 0U),
        expected,
        context
    );
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, context);

    mylite_result_free(result);
    return failures;
}

static int expect_show_count_errors(
    mylite_db *database,
    const char *expected,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, "SHOW COUNT(*) ERRORS", &result);

    failures += mylite_test_expect_size(mylite_result_row_count(result), 1U, context);
    failures += mylite_test_expect_text_or_null(
        mylite_result_value_text(result, 0U, 0U),
        expected,
        context
    );
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, context);

    mylite_result_free(result);
    return failures;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    mylite_result_free(result);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "expected success for [%s], got rc=%d err=%d state=%s message=%s\n",
            sql,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        return 1;
    }

    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    mylite_result_free(result);
    if (rc == MYLITE_OK) {
        fprintf(stderr, "expected error for [%s], got success\n", sql);
        return 1;
    }

    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, sql);
    failures += mylite_test_expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, sql);

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

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        return -1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fclose(file);
        return -1;
    }
    if (fread(buffer, 1U, size, file) != size) {
        fclose(file);
        return -1;
    }

    fclose(file);
    return 0;
}

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) == 0) {
        return 0;
    }

    fprintf(stderr, "%s: byte buffer mismatch\n", context);
    return 1;
}
