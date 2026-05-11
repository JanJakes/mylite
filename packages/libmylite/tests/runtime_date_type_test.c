#include <mylite/mylite.h>

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

enum {
    test_path_capacity = 1024,
    show_columns_field_count = 6,
    information_schema_column_count = 9,
    mysql_error_parse = 1064,
    mysql_error_bad_null = 1048,
    mysql_error_invalid_default = 1067,
    mysql_error_incorrect_date_value = 1292,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_query {
    const char *sql;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

struct expected_dml_result {
    int64_t affected_rows;
    size_t warning_count;
};

static int test_date_success_metadata_dml_and_persistence(void);
static int test_date_diagnostics(void);
static int test_date_independent_handles(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_statement_result(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
);
static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows);
static int expect_dml_result(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_true(int condition, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_date_success_metadata_dml_and_persistence();
    failures += test_date_diagnostics();
    failures += test_date_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_date_success_metadata_dml_and_persistence(void) {
    static const char *const show_columns_rows[] = {
        "id",
        "int",
        "NO",
        "",
        NULL,
        "",
        "d",
        "date",
        "YES",
        "",
        NULL,
        "",
        "nn",
        "date",
        "NO",
        "",
        "2024-02-29",
        "",
    };
    static const char *const show_create_rows[] = {
        "dates",
        "CREATE TABLE `dates` (\n"
        "  `id` int NOT NULL,\n"
        "  `d` date DEFAULT NULL,\n"
        "  `nn` date NOT NULL DEFAULT '2024-02-29'\n"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci",
    };
    static const char *const information_schema_rows[] = {
        "d",
        "date",
        "date",
        "YES",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "nn",
        "date",
        "date",
        "NO",
        "2024-02-29",
        NULL,
        NULL,
        NULL,
        NULL,
    };
    static const char *const initial_rows[] = {
        "1",
        "1000-01-01",
        "2024-02-29",
        "2",
        "9999-12-31",
        "2024-02-29",
        "3",
        "2024-02-29",
        "2024-02-29",
        "4",
        NULL,
        "2024-02-29",
    };
    static const char *const predicate_rows[] = {"1", "3"};
    static const char *const null_rows[] = {"4"};
    static const char *const order_asc_rows[] = {"4", "3", "1", "2"};
    static const char *const order_desc_rows[] = {"2", "1", "3", "4"};
    static const char *const ignored_rows[] = {
        "1",
        "2024-01-01",
        "2",
        "0000-00-00",
        "3",
        "0000-00-00",
        "4",
        "0000-00-00",
    };
    static const char *const ordered_limit_rows[] = {"1", "1", "2", "1", "3", "0", "4", "0"};
    static const char *const updated_count_rows[] = {"1"};
    static const char *const delete_order_rows[] = {"2", "2024-01-01"};
    static const char *const added_rows[] = {
        "1",
        "2020-01-01",
        "2",
        "2020-01-01",
        "3",
        "2020-01-01",
        "4",
        "2020-01-01",
    };
    static const char *const copied_rows[] = {"1", "2025-01-02", "2024-02-29", "2020-01-01"};
    static const char *const created_select_rows[] = {"1", "2025-01-02"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "success") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open date success file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_statement_ok(
        database,
        "CREATE TABLE dates (id INT NOT NULL, d DATE, nn DATE NOT NULL DEFAULT '2024-02-29')"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM dates",
            .values = show_columns_rows,
            .column_count = show_columns_field_count,
            .row_count = 3U,
            .context = "date SHOW COLUMNS",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW CREATE TABLE dates",
            .values = show_create_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "date SHOW CREATE TABLE",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, DATA_TYPE, COLUMN_TYPE, IS_NULLABLE, COLUMN_DEFAULT, "
                   "DATETIME_PRECISION, CHARACTER_MAXIMUM_LENGTH, NUMERIC_PRECISION, "
                   "NUMERIC_SCALE FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'app' AND TABLE_NAME = 'dates' "
                   "AND COLUMN_NAME <> 'id' ORDER BY ORDINAL_POSITION",
            .values = information_schema_rows,
            .column_count = information_schema_column_count,
            .row_count = 2U,
            .context = "date information schema",
        }
    );

    failures += expect_dml_ok(
        database,
        "INSERT INTO dates (id, d) VALUES "
        "(1, '1000-01-01'), (2, '9999-12-31'), (3, '2024-02-29'), (4, NULL)",
        4
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, d, nn FROM dates ORDER BY id",
            .values = initial_rows,
            .column_count = 3U,
            .row_count = 4U,
            .context = "date initial rows",
        }
    );
    failures += expect_dml_ok(database, "UPDATE dates SET d = '1000-01-01' WHERE id = 1", 0);
    failures += expect_dml_ok(database, "UPDATE dates SET d = '2025-01-02' WHERE id = 1", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM dates WHERE d BETWEEN '2024-01-01' AND '2025-12-31' "
                   "ORDER BY id",
            .values = predicate_rows,
            .column_count = 1U,
            .row_count = 2U,
            .context = "date BETWEEN predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM dates WHERE d IN ('2025-01-02', NULL) ORDER BY id",
            .values = (const char *const[]){"1"},
            .column_count = 1U,
            .row_count = 1U,
            .context = "date IN predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM dates WHERE d IS NULL",
            .values = null_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "date IS NULL predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM dates ORDER BY d",
            .values = order_asc_rows,
            .column_count = 1U,
            .row_count = 4U,
            .context = "date ascending order",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM dates ORDER BY d DESC",
            .values = order_desc_rows,
            .column_count = 1U,
            .row_count = 4U,
            .context = "date descending order",
        }
    );

    failures +=
        expect_statement_ok(database, "CREATE TABLE ignored (id INT NOT NULL, d DATE NOT NULL)");
    failures += expect_dml_result(
        database,
        "INSERT IGNORE INTO ignored VALUES "
        "(1, '2024-01-01'), (2, '2024-02-31'), (3, NULL), (4, DEFAULT)",
        (struct expected_dml_result){.affected_rows = 4, .warning_count = 3U}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, d FROM ignored ORDER BY id",
            .values = ignored_rows,
            .column_count = 2U,
            .row_count = 4U,
            .context = "date INSERT IGNORE rows",
        }
    );

    failures += expect_statement_ok(
        database,
        "CREATE TABLE ordered (id INT NOT NULL, d DATE, flag INT NOT NULL DEFAULT 0)"
    );
    failures += expect_dml_ok(
        database,
        "INSERT INTO ordered VALUES "
        "(1, NULL, 0), (2, '2024-01-01', 0), (3, '2025-01-01', 0), (4, '2025-01-01', 0)",
        4
    );
    failures += expect_dml_ok(database, "UPDATE ordered SET flag = 1 ORDER BY d LIMIT 2", 2);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, flag FROM ordered ORDER BY id",
            .values = ordered_limit_rows,
            .column_count = 2U,
            .row_count = 4U,
            .context = "date ordered limit update",
        }
    );
    failures += expect_dml_ok(
        database,
        "UPDATE ordered SET flag = 2 WHERE flag = 0 ORDER BY d DESC LIMIT 1",
        1
    );
    failures += expect_dml_ok(database, "UPDATE ordered SET flag = 3 ORDER BY d DESC LIMIT 0", 0);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM ordered WHERE flag = 2",
            .values = updated_count_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "date duplicate-order tie count",
        }
    );

    failures +=
        expect_statement_ok(database, "CREATE TABLE delete_order (id INT NOT NULL, d DATE)");
    failures += expect_dml_ok(
        database,
        "INSERT INTO delete_order VALUES "
        "(1, NULL), (2, '2024-01-01'), (3, '2025-01-01')",
        3
    );
    failures += expect_dml_ok(database, "DELETE FROM delete_order ORDER BY d LIMIT 1", 1);
    failures += expect_dml_ok(database, "DELETE FROM delete_order ORDER BY d DESC LIMIT 1", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, d FROM delete_order",
            .values = delete_order_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "date delete order limit",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE alter_empty (id INT NOT NULL)");
    failures += expect_statement_ok(database, "ALTER TABLE alter_empty ADD COLUMN d DATE NOT NULL");
    failures += expect_statement_ok(database, "CREATE TABLE alter_nonempty (id INT NOT NULL)");
    failures += expect_dml_ok(database, "INSERT INTO alter_nonempty VALUES (1)", 1);
    failures += execute_error(
        database,
        "ALTER TABLE alter_nonempty ADD COLUMN d DATE NOT NULL",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_date_value,
            .sqlstate = "22007",
            .message_part = "Incorrect date value: '0000-00-00' for column 'd' at row 1",
        }
    );
    failures += expect_statement_ok(
        database,
        "ALTER TABLE dates ADD COLUMN added DATE DEFAULT '2020-01-01'"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, added FROM dates ORDER BY id",
            .values = added_rows,
            .column_count = 2U,
            .row_count = 4U,
            .context = "date alter add default backfill",
        }
    );

    failures += expect_statement_ok(database, "CREATE TABLE copied LIKE dates");
    failures += expect_dml_ok(
        database,
        "INSERT INTO copied (id, d, nn, added) "
        "SELECT id, d, nn, added FROM dates WHERE id = 1",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, d, nn, added FROM copied",
            .values = copied_rows,
            .column_count = 4U,
            .row_count = 1U,
            .context = "date insert select copy",
        }
    );
    failures += expect_statement_ok(database, "CREATE TABLE selected AS SELECT id, d FROM dates");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, d FROM selected WHERE id = 1",
            .values = created_select_rows,
            .column_count = 2U,
            .row_count = 1U,
            .context = "date create table select",
        }
    );
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        MYLITE_FILE_PREAMBLE_SIZE,
        "date file preamble"
    );

    mylite_close(database);
    database = NULL;
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen date success file");
    failures += expect_statement_ok(database, "USE app");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, d FROM dates ORDER BY d DESC LIMIT 1",
            .values = (const char *const[]){"2", "9999-12-31"},
            .column_count = 2U,
            .row_count = 1U,
            .context = "date reopened ordered select",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_date_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "diagnostics") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open date diagnostics file");
    failures += expect_statement_ok(database, "CREATE DATABASE app");
    failures += expect_statement_ok(database, "USE app");
    failures +=
        expect_statement_ok(database, "CREATE TABLE dates (id INT NOT NULL, d DATE NOT NULL)");
    failures += execute_error(
        database,
        "INSERT INTO dates VALUES (1, '2024-02-31')",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_date_value,
            .sqlstate = "22007",
            .message_part = "Incorrect date value: '2024-02-31' for column 'd' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO dates VALUES (1, '0000-00-00')",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_date_value,
            .sqlstate = "22007",
            .message_part = "Incorrect date value: '0000-00-00' for column 'd' at row 1",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO dates VALUES (1, '2024/01/02')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "DATE values support only canonical YYYY-MM-DD strings",
        }
    );
    failures += execute_error(
        database,
        "INSERT IGNORE INTO dates VALUES (1, '2024/01/02')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "DATE values support only canonical YYYY-MM-DD strings",
        }
    );
    failures += execute_error(
        database,
        "INSERT IGNORE INTO dates VALUES (1, '240102')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "DATE values support only canonical YYYY-MM-DD strings",
        }
    );
    failures += execute_error(
        database,
        "INSERT IGNORE INTO dates VALUES (1, 20240102)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "DATE values support only string, NULL, and DEFAULT values",
        }
    );
    failures += execute_error(
        database,
        "INSERT IGNORE INTO dates VALUES (1, DATE '2024-01-02')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "near 'DATE'",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM dates",
            .values = (const char *const[]){"0"},
            .column_count = 1U,
            .row_count = 1U,
            .context = "unsupported date inputs do not insert rows",
        }
    );
    failures += execute_error(
        database,
        "INSERT INTO dates VALUES (1, NULL)",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "Column 'd' cannot be null",
        }
    );
    failures += expect_dml_ok(database, "INSERT INTO dates VALUES (9, '2024-01-01')", 1);
    failures += execute_error(
        database,
        "CREATE TABLE bad_default (d DATE DEFAULT '2024-02-31')",
        (struct expected_sql_error){
            .code = mysql_error_invalid_default,
            .sqlstate = "42000",
            .message_part = "Invalid default value for 'd'",
        }
    );
    failures += execute_error(
        database,
        "UPDATE dates SET d = 1 WHERE id = 9",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "DATE values support only string, NULL, and DEFAULT values",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM dates WHERE d = 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "WHERE DATE predicates support only string literals",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_date_independent_handles(void) {
    static const char *const first_rows[] = {"2025-05-01"};
    static const char *const second_rows[] = {"2026-06-02"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first date file");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second date file");
    failures += expect_statement_ok(first, "CREATE DATABASE app");
    failures += expect_statement_ok(second, "CREATE DATABASE app");
    failures += expect_statement_ok(first, "USE app");
    failures += expect_statement_ok(second, "USE app");
    failures += expect_statement_ok(first, "CREATE TABLE dates (d DATE)");
    failures += expect_statement_ok(second, "CREATE TABLE dates (d DATE)");
    failures += expect_dml_ok(first, "INSERT INTO dates VALUES ('2025-05-01')", 1);
    failures += expect_dml_ok(second, "INSERT INTO dates VALUES ('2026-06-02')", 1);
    failures += expect_query_values(
        first,
        (struct expected_query){
            .sql = "SELECT d FROM dates",
            .values = first_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "first independent date file",
        }
    );
    failures += expect_query_values(
        second,
        (struct expected_query){
            .sql = "SELECT d FROM dates",
            .values = second_rows,
            .column_count = 1U,
            .row_count = 1U,
            .context = "second independent date file",
        }
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);

    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "execute '%s': expected MYLITE_OK, got %d (%d %s %s)\n",
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
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_ERROR) {
        fprintf(stderr, "execute '%s': expected MYLITE_ERROR, got %d\n", sql, rc);
        failures += 1;
    }
    failures += expect_true(result == NULL, "failed execute leaves result null");
    failures += expect_int(mylite_errcode(database), expected.code, "error code");
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, "SQLSTATE");
    failures += expect_contains(mylite_errmsg(database), expected.message_part, "error message");
    mylite_result_free(result);

    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    return expect_statement_result(
        database,
        sql,
        (struct expected_dml_result){
            .affected_rows = 0,
            .warning_count = 0U,
        }
    );
}

static int expect_statement_result(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, "statement column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "statement row count");
    failures += expect_size(
        mylite_result_warning_count(result),
        expected.warning_count,
        "statement warning count"
    );
    mylite_result_free(result);

    return failures;
}

static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    return expect_dml_result(
        database,
        sql,
        (struct expected_dml_result){
            .affected_rows = affected_rows,
            .warning_count = 0U,
        }
    );
}

static int expect_dml_result(
    mylite_db *database,
    const char *sql,
    struct expected_dml_result expected
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, "DML column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "DML row count");
    failures +=
        expect_int64(mylite_result_affected_rows(result), expected.affected_rows, "DML affected");
    failures +=
        expect_size(mylite_result_warning_count(result), expected.warning_count, "DML warnings");
    mylite_result_free(result);

    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    for (size_t row = 0U; row < query.row_count; ++row) {
        for (size_t column = 0U; column < query.column_count; ++column) {
            size_t value_index = (row * query.column_count) + column;

            failures +=
                expect_result_value(result, row, column, query.values[value_index], query.context);
        }
    }
    failures += expect_int64(mylite_result_affected_rows(result), 0, query.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, query.context);
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

    if (expected == NULL) {
        return expect_true(actual == NULL, context);
    }

    return expect_text(actual, expected, context);
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    const char *directory = getenv("TMPDIR");
    int written = 0;

    if (directory == NULL || directory[0] == '\0') {
        directory = getenv("TEMP");
    }
    if (directory == NULL || directory[0] == '\0') {
        directory = ".";
    }

    written = snprintf(
        path,
        path_size,
        "%s/mylite_date_type_%d_%s.mylite",
        directory,
        current_process_id(),
        name
    );
    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path is too long for %s\n", name);
        return 1;
    }

    return 0;
}

static int current_process_id(void) {
#ifdef _WIN32
    return _getpid();
#else
    return getpid();
#endif
}

static void remove_related_files(const char *path) {
    remove_with_suffix(path, "");
    remove_with_suffix(path, "-journal");
    remove_with_suffix(path, "-wal");
    remove_with_suffix(path, "-shm");
}

static void remove_with_suffix(const char *path, const char *suffix) {
    char related_path[test_path_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related_path)) {
        return;
    }

    (void)remove(related_path);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        fprintf(stderr, "failed to open %s for reading\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "failed to seek %s\n", path);
        (void)fclose(file);
        return 1;
    }
    if (fread(buffer, 1U, size, file) != size) {
        fprintf(stderr, "failed to read %zu bytes from %s\n", size, path);
        (void)fclose(file);
        return 1;
    }
    if (fclose(file) != 0) {
        fprintf(stderr, "failed to close %s\n", path);
        return 1;
    }

    return 0;
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
        return 1;
    }

    return 0;
}

static int expect_int64(int64_t actual, int64_t expected, const char *context) {
    if (actual != expected) {
        fprintf(
            stderr,
            "%s: expected %lld, got %lld\n",
            context,
            (long long)expected,
            (long long)actual
        );
        return 1;
    }

    return 0;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
        return 1;
    }

    return 0;
}

static int expect_true(int condition, const char *context) {
    if (!condition) {
        fprintf(stderr, "%s: expected true\n", context);
        return 1;
    }

    return 0;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected text '%s', got '%s'\n",
            context,
            expected == NULL ? "(null)" : expected,
            actual == NULL ? "(null)" : actual
        );
        return 1;
    }

    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected '%s' to contain '%s'\n",
            context,
            actual == NULL ? "(null)" : actual,
            needle == NULL ? "(null)" : needle
        );
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
        fprintf(stderr, "%s: byte comparison failed\n", context);
        return 1;
    }

    return 0;
}
