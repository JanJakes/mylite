#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include "storage/mylite_file_format.h"

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
    related_file_suffix_capacity = 16,
    mysql_error_parse = 1064,
    mysql_error_no_database_selected = 1046,
    mysql_error_unknown_column = 1054,
    mysql_error_unknown_database = 1049,
    mysql_error_incorrect_database_name = 1102,
    mysql_error_incorrect_table_name = 1103,
    mysql_error_table_does_not_exist = 1146,
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

static int test_table_statement_success_and_session_state(void);
static int test_table_statement_persistence_shadowing_rename_and_drop(void);
static int test_table_statement_diagnostics(void);
static int test_independent_table_statement_handles(void);
static int seed_numbers_table(mylite_db *database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query_shape(mylite_db *database, struct expected_query expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
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

    failures += test_table_statement_success_and_session_state();
    failures += test_table_statement_persistence_shadowing_rename_and_drop();
    failures += test_table_statement_diagnostics();
    failures += test_independent_table_statement_handles();

    return failures == 0 ? 0 : 1;
}

static int test_table_statement_success_and_session_state(void) {
    static const char *const columns[] = {"id", "v", "n", "s"};
    static const char *const all_values[] = {
        "1",
        "20",
        NULL,
        "b",
        "2",
        "10",
        "7",
        "a",
        "3",
        "15",
        NULL,
        "c",
        "4",
        NULL,
        "9",
        "d",
    };
    static const char *const order_asc_values[] = {
        "4",
        NULL,
        "9",
        "d",
        "2",
        "10",
        "7",
        "a",
        "3",
        "15",
        NULL,
        "c",
        "1",
        "20",
        NULL,
        "b",
    };
    static const char *const order_desc_values[] = {
        "1",
        "20",
        NULL,
        "b",
        "3",
        "15",
        NULL,
        "c",
        "2",
        "10",
        "7",
        "a",
        "4",
        NULL,
        "9",
        "d",
    };
    static const char *const first_two_values[] = {
        "1",
        "20",
        NULL,
        "b",
        "2",
        "10",
        "7",
        "a",
    };
    static const char *const second_third_values[] = {
        "2",
        "10",
        "7",
        "a",
        "3",
        "15",
        NULL,
        "c",
    };
    static const char *const row_count_columns[] = {"ROW_COUNT()", "@@warning_count"};
    static const char *const row_count_values[] = {"-1", "0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "success") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open table file");
    failures += seed_numbers_table(database);

    failures += expect_query_shape(
        database,
        (struct expected_query){
            .sql = "TABLE numbers",
            .columns = columns,
            .column_count = 4U,
            .row_count = 4U,
            .context = "plain table statement",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "TABLE numbers ORDER BY v",
            .columns = columns,
            .column_count = 4U,
            .values = order_asc_values,
            .row_count = 4U,
            .context = "table default asc order",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "TABLE numbers ORDER BY v ASC",
            .columns = columns,
            .column_count = 4U,
            .values = order_asc_values,
            .row_count = 4U,
            .context = "table explicit asc order",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "TABLE numbers ORDER BY v DESC",
            .columns = columns,
            .column_count = 4U,
            .values = order_desc_values,
            .row_count = 4U,
            .context = "table explicit desc order",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "TABLE numbers ORDER BY v, id",
            .columns = columns,
            .column_count = 4U,
            .values = order_asc_values,
            .row_count = 4U,
            .context = "table multiple order keys",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "TABLE app.numbers ORDER BY id LIMIT 2",
            .columns = columns,
            .column_count = 4U,
            .values = first_two_values,
            .row_count = 2U,
            .context = "schema-qualified table statement",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "TABLE numbers ORDER BY numbers.id LIMIT 2",
            .columns = columns,
            .column_count = 4U,
            .values = first_two_values,
            .row_count = 2U,
            .context = "table qualified order key",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "TABLE numbers ORDER BY id LIMIT 0",
            .columns = columns,
            .column_count = 4U,
            .values = NULL,
            .row_count = 0U,
            .context = "table limit zero",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "TABLE numbers ORDER BY id LIMIT 2",
            .columns = columns,
            .column_count = 4U,
            .values = first_two_values,
            .row_count = 2U,
            .context = "table exact limit",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "TABLE numbers ORDER BY id LIMIT 10",
            .columns = columns,
            .column_count = 4U,
            .values = all_values,
            .row_count = 4U,
            .context = "table oversized limit",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "TABLE numbers ORDER BY id LIMIT 2 OFFSET 1",
            .columns = columns,
            .column_count = 4U,
            .values = second_third_values,
            .row_count = 2U,
            .context = "table limit offset",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "TABLE numbers ORDER BY id LIMIT 1, 2",
            .columns = columns,
            .column_count = 4U,
            .values = second_third_values,
            .row_count = 2U,
            .context = "table comma limit",
        }
    );

    failures += execute_ok(database, "TABLE numbers ORDER BY id LIMIT 1", &result);
    failures +=
        mylite_test_expect_int64(mylite_result_affected_rows(result), 0, "table affected rows");
    failures +=
        mylite_test_expect_size(mylite_result_warning_count(result), 0U, "table warning count");
    mylite_result_free(result);
    result = NULL;
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .columns = row_count_columns,
            .column_count = 2U,
            .values = row_count_values,
            .row_count = 1U,
            .context = "row count after table",
        }
    );

    failures += execute_ok(database, "SET sql_select_limit = 1", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "TABLE numbers ORDER BY id",
            .columns = columns,
            .column_count = 4U,
            .values = first_two_values,
            .row_count = 1U,
            .context = "sql_select_limit caps table",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "TABLE numbers ORDER BY id LIMIT 2",
            .columns = columns,
            .column_count = 4U,
            .values = first_two_values,
            .row_count = 2U,
            .context = "explicit table limit overrides sql_select_limit",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_statement_persistence_shadowing_rename_and_drop(void) {
    static const char *const columns[] = {"id", "v", "n", "s"};
    static const char *const temp_columns[] = {"id", "v"};
    static const char *const temp_values[] = {"9", "9"};
    static const char *const first_persistent_values[] = {"1", "20", NULL, "b"};
    static const char *const renamed_values[] = {
        "1",
        "20",
        NULL,
        "b",
        "2",
        "10",
        "7",
        "a",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "persistence") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open table file");
    failures += seed_numbers_table(database);
    failures += execute_ok(database, "CREATE TEMPORARY TABLE numbers (id INT, v INT)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(database, "INSERT INTO numbers VALUES (9, 9)", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "TABLE numbers",
            .columns = temp_columns,
            .column_count = 2U,
            .values = temp_values,
            .row_count = 1U,
            .context = "temporary table shadows persistent table",
        }
    );
    failures += execute_ok(database, "DROP TEMPORARY TABLE numbers", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "TABLE numbers ORDER BY id LIMIT 1",
            .columns = columns,
            .column_count = 4U,
            .values = first_persistent_values,
            .row_count = 1U,
            .context = "persistent table visible after temporary drop",
        }
    );
    mylite_close(database);
    database = NULL;

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen table file");
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "TABLE numbers ORDER BY id LIMIT 1",
            .columns = columns,
            .column_count = 4U,
            .values = first_persistent_values,
            .row_count = 1U,
            .context = "table statement persists after reopen",
        }
    );
    failures += execute_ok(database, "RENAME TABLE numbers TO renamed_numbers", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "TABLE numbers",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.numbers' doesn't exist",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "TABLE renamed_numbers ORDER BY id LIMIT 2",
            .columns = columns,
            .column_count = 4U,
            .values = renamed_values,
            .row_count = 2U,
            .context = "table statement after rename",
        }
    );
    failures += execute_ok(database, "DROP TABLE renamed_numbers", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "TABLE renamed_numbers",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.renamed_numbers' doesn't exist",
        }
    );

    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "table statement preserves preamble"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_statement_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open diagnostics file");
    failures += execute_error(
        database,
        "TABLE numbers",
        (struct expected_sql_error){
            .code = mysql_error_no_database_selected,
            .sqlstate = "3D000",
            .message_part = "No database selected",
        }
    );
    failures += execute_ok(database, "CREATE DATABASE app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += seed_numbers_table(database);
    failures += execute_error(
        database,
        "TABLE missing_schema.numbers",
        (struct expected_sql_error){
            .code = mysql_error_unknown_database,
            .sqlstate = "42000",
            .message_part = "Unknown database",
        }
    );
    failures += execute_error(
        database,
        "TABLE _mylite_reserved.numbers",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_database_name,
            .sqlstate = "42000",
            .message_part = "Incorrect database name",
        }
    );
    failures += execute_error(
        database,
        "TABLE missing_table",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "Table 'app.missing_table' doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "TABLE _mylite_reserved",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_table_name,
            .sqlstate = "42000",
            .message_part = "Incorrect table name",
        }
    );
    failures += execute_error(
        database,
        "TABLE numbers ORDER BY missing",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'order clause'",
        }
    );
    failures += execute_error(
        database,
        "TABLE numbers ORDER BY FIELD(s, 'a', 'b')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "utility statement is not supported",
        }
    );
    failures += execute_error(
        database,
        "TABLE numbers WHERE id = 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error",
        }
    );
    failures += execute_error(
        database,
        "TABLE numbers AS n",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error",
        }
    );
    failures += execute_error(
        database,
        "TABLE numbers LIMIT +1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error",
        }
    );
    failures += execute_error(
        database,
        "TABLE numbers LIMIT 9223372036854775808",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "LIMIT literal is outside the supported range",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_independent_table_statement_handles(void) {
    static const char *const columns[] = {"id", "v"};
    static const char *const first_values[] = {"1", "10"};
    static const char *const second_values[] = {"2", "20"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (mylite_test_make_path(first_path, sizeof(first_path), "independent_first") != 0 ||
        mylite_test_make_path(second_path, sizeof(second_path), "independent_second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures +=
        mylite_test_expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first table file");
    failures += mylite_test_expect_int(
        mylite_open(second_path, &second),
        MYLITE_OK,
        "open second table file"
    );
    failures += execute_ok(first, "CREATE DATABASE app", NULL);
    failures += execute_ok(first, "USE app", NULL);
    failures += execute_ok(first, "CREATE TABLE t (id INT, v INT)", NULL);
    failures += execute_ok(first, "INSERT INTO t VALUES (1, 10)", NULL);
    failures += execute_ok(second, "CREATE DATABASE app", NULL);
    failures += execute_ok(second, "USE app", NULL);
    failures += execute_ok(second, "CREATE TABLE t (id INT, v INT)", NULL);
    failures += execute_ok(second, "INSERT INTO t VALUES (2, 20)", NULL);

    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "TABLE t",
            .columns = columns,
            .column_count = 2U,
            .values = first_values,
            .row_count = 1U,
            .context = "first independent table state",
        }
    );
    failures += expect_query(
        second,
        (struct expected_query){
            .sql = "TABLE t",
            .columns = columns,
            .column_count = 2U,
            .values = second_values,
            .row_count = 1U,
            .context = "second independent table state",
        }
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int seed_numbers_table(mylite_db *database) {
    int failures = 0;

    failures += execute_ok(database, "CREATE DATABASE IF NOT EXISTS app", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += execute_ok(
        database,
        "CREATE TABLE numbers (id INT NOT NULL, v INT, n INT NULL, s VARCHAR(10), hidden INT)",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO numbers (id, v, n, s, hidden) VALUES "
        "(1, 20, NULL, 'b', 100), "
        "(2, 10, 7, 'a', 200), "
        "(3, 15, NULL, 'c', 300), "
        "(4, NULL, 9, 'd', 400)",
        NULL
    );
    failures += execute_ok(database, "ALTER TABLE numbers ALTER hidden SET INVISIBLE", NULL);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        (void)fprintf(
            stderr,
            "%s: expected success, got rc=%d error=%d sqlstate=%s message=%s\n",
            sql,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
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
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc == MYLITE_OK) {
        (void)fprintf(stderr, "%s: expected error, got success\n", sql);
        mylite_result_free(result);
        return 1;
    }
    if (result != NULL) {
        (void)fprintf(stderr, "%s: error returned non-null result\n", sql);
        failures += 1;
        mylite_result_free(result);
    }
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, "error code");
    failures += mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, "sqlstate");
    failures += mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, sql);
    return failures;
}

static int expect_query_shape(mylite_db *database, struct expected_query expected) {
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
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, expected.context);
    for (size_t column = 0U; column < expected.column_count; ++column) {
        failures += mylite_test_expect_text(
            mylite_result_column_name(result, column),
            expected.columns[column],
            expected.context
        );
    }

    mylite_result_free(result);
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
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
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, expected.context);

    for (size_t column = 0U; column < expected.column_count; ++column) {
        failures += mylite_test_expect_text(
            mylite_result_column_name(result, column),
            expected.columns[column],
            expected.context
        );
    }
    for (size_t row = 0U; row < expected.row_count; ++row) {
        for (size_t column = 0U; column < expected.column_count; ++column) {
            failures += expect_result_value(
                result,
                row,
                column,
                expected.values[(row * expected.column_count) + column],
                expected.context
            );
        }
    }

    mylite_result_free(result);
    return failures;
}

static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
) {
    const char *actual = mylite_result_value_text(result, row, column);

    return mylite_test_expect_text(actual, expected, context);
}

static void remove_related_files(const char *path) {
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related[test_path_capacity + related_file_suffix_capacity];
    int written = snprintf(related, sizeof(related), "%s%s", path, suffix);

    if (written > 0 && (size_t)written < sizeof(related)) {
        (void)remove(related);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t read_count = 0U;

    if (file == NULL) {
        perror("fopen");
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        perror("fseek");
        (void)fclose(file);
        return 1;
    }

    read_count = fread(buffer, 1U, size, file);
    if (fclose(file) != 0) {
        perror("fclose");
        return 1;
    }
    if (read_count != size) {
        (void)fprintf(stderr, "short read: expected %zu got %zu\n", size, read_count);
        return 1;
    }
    return 0;
}

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) != 0) {
        (void)fprintf(stderr, "%s: bytes differ\n", context);
        return 1;
    }
    return 0;
}
