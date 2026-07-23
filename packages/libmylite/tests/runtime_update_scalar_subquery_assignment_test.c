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
    family_copy_column_count = 10,
    temporal_family_copy_column_count = 5,
    mysql_error_bad_null = 1048,
    mysql_error_unknown_column = 1054,
    mysql_error_parse = 1064,
    mysql_error_update_table_used = 1093,
    mysql_error_table_does_not_exist = 1146,
    mysql_error_operand_should_contain_one_column = 1241,
    mysql_error_subquery_returns_more_than_one_row = 1242,
    mysql_error_data_too_long = 1406,
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

struct expected_bytes {
    const unsigned char *bytes;
    size_t size;
};

static int test_scalar_subquery_assignment_success_and_persistence(void);
static int test_scalar_subquery_assignment_errors(void);
static int open_seeded_database(const char *path, mylite_db **out_database);
static int seed_tables(mylite_db *database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_update_ok(mylite_db *database, const char *sql, int64_t affected_rows);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int expect_binary_cell(
    const mylite_result *result,
    size_t row,
    size_t column,
    struct expected_bytes expected,
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

    failures += test_scalar_subquery_assignment_success_and_persistence();
    failures += test_scalar_subquery_assignment_errors();

    return failures == 0 ? 0 : 1;
}

static int test_scalar_subquery_assignment_success_and_persistence(void) {
    static const unsigned char binary_padded[] = {0x51U, 0x00U, 0x00U};
    static const char *const after_string_copy[] = {
        "1",
        "old-18",
        "10",
        "2",
        "source-19",
        "20",
        "3",
        NULL,
        "30",
    };
    static const char *const after_outer_order_limit[] = {
        "1",
        NULL,
        "200",
        "2",
        NULL,
        "20",
        "3",
        "source-18",
        "30",
    };
    static const char *const after_family_copy[] = {
        "1",
        "42",
        "new",
        "new text",
        "12.34",
        "6.25",
        "2025-02-03",
        "04:05:06",
        "2025-02-03 04:05:06",
        "2025-02-03 04:05:06",
    };
    static const char *const after_text_temporal_copy[] = {
        "1",
        "2026-03-04",
        "05:06:07",
        "2026-03-04 05:06:07",
        "2026-03-04 05:06:07",
    };
    static const char *const after_row_null_copy[] = {"1", NULL};
    static const char *const persisted_values[] = {
        "1",
        NULL,
        "200",
        "2",
        NULL,
        "20",
        "3",
        "source-18",
        "30",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "success") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += open_seeded_database(path, &database);
    failures += expect_update_ok(
        database,
        "UPDATE target "
        "SET option_value = ("
        "SELECT option_value FROM source WHERE option_name = 'User 0000019'"
        ") "
        "WHERE option_name = 'User 0000019'",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, option_value, n FROM target ORDER BY id",
            .values = after_string_copy,
            .column_count = 3U,
            .row_count = 3U,
            .context = "string scalar subquery assignment",
        }
    );

    failures += expect_update_ok(
        database,
        "UPDATE target SET n = (SELECT n FROM source WHERE id = 11) WHERE id = 1",
        1
    );
    failures += expect_update_ok(
        database,
        "UPDATE target SET n = (SELECT n FROM source WHERE id = 11) WHERE id = 1",
        0
    );
    failures += expect_update_ok(
        database,
        "UPDATE target SET option_value = (SELECT option_value FROM source WHERE id = 999) "
        "WHERE id = 1",
        1
    );
    failures += expect_update_ok(
        database,
        "UPDATE target SET option_value = (SELECT option_value FROM source ORDER BY id ASC "
        "LIMIT 1) WHERE id = 2",
        1
    );
    failures += expect_update_ok(
        database,
        "UPDATE target SET option_value = (SELECT option_value FROM source ORDER BY id ASC "
        "LIMIT 1, 1) WHERE id = 2",
        1
    );
    failures += expect_update_ok(
        database,
        "UPDATE target SET option_value = (SELECT option_value FROM source ORDER BY id ASC "
        "LIMIT 0) WHERE id = 2",
        1
    );
    failures += expect_update_ok(
        database,
        "UPDATE family_target SET i = (SELECT i FROM family_source WHERE id = 10) WHERE id = 1",
        1
    );
    failures += expect_update_ok(
        database,
        "UPDATE family_target SET s = (SELECT s FROM family_source WHERE id = 10) WHERE id = 1",
        1
    );
    failures += expect_update_ok(
        database,
        "UPDATE family_target SET txt = (SELECT txt FROM family_source WHERE id = 10) "
        "WHERE id = 1",
        1
    );
    failures += expect_update_ok(
        database,
        "UPDATE family_target SET dec_value = (SELECT dec_value FROM family_source WHERE id = 10) "
        "WHERE id = 1",
        1
    );
    failures += expect_update_ok(
        database,
        "UPDATE family_target SET dbl_value = (SELECT dbl_value FROM family_source WHERE id = 10) "
        "WHERE id = 1",
        1
    );
    failures += expect_update_ok(
        database,
        "UPDATE family_target SET date_value = (SELECT date_value FROM family_source WHERE id = "
        "10) "
        "WHERE id = 1",
        1
    );
    failures += expect_update_ok(
        database,
        "UPDATE family_target SET time_value = (SELECT time_value FROM family_source WHERE id = "
        "10) "
        "WHERE id = 1",
        1
    );
    failures += expect_update_ok(
        database,
        "UPDATE family_target SET datetime_value = ("
        "SELECT datetime_value FROM family_source WHERE id = 10"
        ") WHERE id = 1",
        1
    );
    failures += expect_update_ok(
        database,
        "UPDATE family_target SET timestamp_value = ("
        "SELECT timestamp_value FROM family_source WHERE id = 10"
        ") WHERE id = 1",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, i, s, txt, dec_value, dbl_value, date_value, time_value, "
                   "datetime_value, timestamp_value FROM family_target WHERE id = 1",
            .values = after_family_copy,
            .column_count = family_copy_column_count,
            .row_count = 1U,
            .context = "compatible descriptor family scalar copy",
        }
    );
    failures += expect_update_ok(
        database,
        "UPDATE family_target SET date_value = (SELECT date_text FROM temporal_text_source "
        "WHERE id = 1) WHERE id = 1",
        1
    );
    failures += expect_update_ok(
        database,
        "UPDATE family_target SET time_value = (SELECT time_text FROM temporal_text_source "
        "WHERE id = 1) WHERE id = 1",
        1
    );
    failures += expect_update_ok(
        database,
        "UPDATE family_target SET datetime_value = (SELECT datetime_text FROM "
        "temporal_text_source WHERE id = 1) WHERE id = 1",
        1
    );
    failures += expect_update_ok(
        database,
        "UPDATE family_target SET timestamp_value = (SELECT timestamp_text FROM "
        "temporal_text_source WHERE id = 1) WHERE id = 1",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, date_value, time_value, datetime_value, timestamp_value "
                   "FROM family_target WHERE id = 1",
            .values = after_text_temporal_copy,
            .column_count = temporal_family_copy_column_count,
            .row_count = 1U,
            .context = "text scalar subquery temporal assignment",
        }
    );
    failures += expect_update_ok(
        database,
        "UPDATE family_target SET s = (SELECT s FROM family_source WHERE id = 11) WHERE id = 1",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, s FROM family_target WHERE id = 1",
            .values = after_row_null_copy,
            .column_count = 2U,
            .row_count = 1U,
            .context = "row NULL scalar assignment",
        }
    );
    failures += expect_update_ok(
        database,
        "UPDATE binary_target SET b = (SELECT v FROM binary_source WHERE id = 10) WHERE id = 1",
        1
    );
    failures += execute_ok(database, "SELECT b FROM binary_target WHERE id = 1", &result);
    failures += expect_binary_cell(
        result,
        0U,
        0U,
        (struct expected_bytes){.bytes = binary_padded, .size = sizeof(binary_padded)},
        "binary scalar subquery pads selected VARBINARY for BINARY target"
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_update_ok(
        database,
        "UPDATE binary_target SET b = (SELECT v FROM binary_source WHERE id = 10) WHERE id = 1",
        0
    );
    failures += expect_update_ok(
        database,
        "UPDATE target "
        "SET option_value = (SELECT option_value FROM source WHERE option_name = 'User 0000018') "
        "ORDER BY id DESC LIMIT 1",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, option_value, n FROM target ORDER BY id",
            .values = after_outer_order_limit,
            .column_count = 3U,
            .row_count = 3U,
            .context = "outer ordered limited scalar assignment",
        }
    );

    failures += mylite_test_expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read preamble after scalar update"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "scalar update preserves preamble"
    );

    mylite_close(database);
    database = NULL;
    failures += mylite_test_expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "reopen scalar update file"
    );
    failures += execute_ok(database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, option_value, n FROM target ORDER BY id",
            .values = persisted_values,
            .column_count = 3U,
            .row_count = 3U,
            .context = "persisted scalar assignment values",
        }
    );
    failures += execute_ok(database, "SELECT b FROM binary_target WHERE id = 1", &result);
    failures += expect_binary_cell(
        result,
        0U,
        0U,
        (struct expected_bytes){.bytes = binary_padded, .size = sizeof(binary_padded)},
        "persisted binary scalar subquery assignment"
    );
    mylite_result_free(result);
    result = NULL;

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int test_scalar_subquery_assignment_errors(void) {
    static const unsigned char binary_original[] = {0x41U};
    static const char *const unchanged_values[] = {
        "1",
        "old-18",
        "10",
        "7",
        "2",
        "old-19",
        "20",
        "7",
        "3",
        NULL,
        "30",
        "7",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "errors") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += open_seeded_database(path, &database);
    failures += expect_update_ok(
        database,
        "UPDATE target SET option_value = (SELECT option_value FROM source) WHERE id = 999",
        0
    );
    failures += execute_error(
        database,
        "UPDATE target SET option_value = (SELECT option_value FROM source) WHERE id = 1",
        (struct expected_sql_error){
            .code = mysql_error_subquery_returns_more_than_one_row,
            .sqlstate = "21000",
            .message_part = "Subquery returns more than 1 row",
        }
    );
    failures += execute_error(
        database,
        "UPDATE target SET option_value = (SELECT option_value, n FROM source WHERE id = 11) "
        "WHERE id = 999",
        (struct expected_sql_error){
            .code = mysql_error_operand_should_contain_one_column,
            .sqlstate = "21000",
            .message_part = "Operand should contain 1 column(s)",
        }
    );
    failures += execute_error(
        database,
        "UPDATE target SET option_value = (SELECT option_value FROM target WHERE id = 2) "
        "WHERE id = 999",
        (struct expected_sql_error){
            .code = mysql_error_update_table_used,
            .sqlstate = "HY000",
            .message_part = "You can't specify target table 'target' for update in FROM clause",
        }
    );
    failures += execute_error(
        database,
        "UPDATE target SET nn = (SELECT n FROM source WHERE id = 999) WHERE id = 1",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "Column 'nn' cannot be null",
        }
    );
    failures += execute_error(
        database,
        "UPDATE family_target SET nn_v = (SELECT s FROM family_source WHERE id = 11) "
        "WHERE id = 1",
        (struct expected_sql_error){
            .code = mysql_error_bad_null,
            .sqlstate = "23000",
            .message_part = "Column 'nn_v' cannot be null",
        }
    );
    failures += execute_error(
        database,
        "UPDATE target SET option_value = (SELECT missing_value FROM source WHERE id = 11) "
        "WHERE id = 999",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing_value' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "UPDATE target SET option_value = (SELECT option_value FROM source "
        "WHERE missing_predicate = 11) WHERE id = 999",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing_predicate' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "UPDATE target SET option_value = (SELECT option_value FROM source "
        "ORDER BY missing_order LIMIT 1) WHERE id = 999",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing_order' in 'order clause'",
        }
    );
    failures += execute_error(
        database,
        "UPDATE target SET option_value = (SELECT option_value FROM missing_source WHERE id = 11) "
        "WHERE id = 999",
        (struct expected_sql_error){
            .code = mysql_error_table_does_not_exist,
            .sqlstate = "42S02",
            .message_part = "doesn't exist",
        }
    );
    failures += execute_error(
        database,
        "UPDATE target SET n = (SELECT option_value FROM source WHERE id = 11) WHERE id = 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "UPDATE scalar subquery assignment does not support implicit integer conversion",
        }
    );
    failures += execute_error(
        database,
        "UPDATE binary_target SET v = (SELECT b FROM binary_source WHERE id = 10) WHERE id = 1",
        (struct expected_sql_error){
            .code = mysql_error_data_too_long,
            .sqlstate = "22001",
            .message_part = "Data too long",
        }
    );
    failures += execute_ok(database, "SELECT v FROM binary_target WHERE id = 1", &result);
    failures += expect_binary_cell(
        result,
        0U,
        0U,
        (struct expected_bytes){.bytes = binary_original, .size = sizeof(binary_original)},
        "failed binary scalar subquery assignment leaves target unchanged"
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_error(
        database,
        "UPDATE target SET option_value = (SELECT 'literal') WHERE id = 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "UPDATE scalar subquery assignment supports only one descriptor table source",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, option_value, n, nn FROM target ORDER BY id",
            .values = unchanged_values,
            .column_count = 4U,
            .row_count = 3U,
            .context = "scalar assignment errors leave target unchanged",
        }
    );

    mylite_close(database);
    remove_related_files(path);

    return failures;
}

static int open_seeded_database(const char *path, mylite_db **out_database) {
    mylite_result *result = NULL;
    int failures =
        mylite_test_expect_int(mylite_open(path, out_database), MYLITE_OK, "open scalar file");

    if (failures != 0) {
        return failures;
    }

    failures += execute_ok(*out_database, "CREATE DATABASE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(*out_database, "USE app", &result);
    mylite_result_free(result);
    result = NULL;
    failures += seed_tables(*out_database);

    return failures;
}

static int seed_tables(mylite_db *database) {
    mylite_result *result = NULL;
    int failures = execute_ok(
        database,
        "CREATE TABLE target ("
        "id INT NOT NULL, "
        "option_name VARCHAR(40), "
        "option_value VARCHAR(40) NULL, "
        "n INT NULL, "
        "nn INT NOT NULL DEFAULT 7)",
        &result
    );

    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "CREATE TABLE source ("
        "id INT NOT NULL, "
        "option_name VARCHAR(40), "
        "option_value VARCHAR(40) NULL, "
        "n INT NULL)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "CREATE TABLE family_target ("
        "id INT NOT NULL, "
        "i INT NULL, "
        "s VARCHAR(40) NULL, "
        "txt TEXT NULL, "
        "dec_value DECIMAL(6,2) NULL, "
        "dbl_value DOUBLE NULL, "
        "date_value DATE NULL, "
        "time_value TIME NULL, "
        "datetime_value DATETIME NULL, "
        "timestamp_value TIMESTAMP NULL, "
        "nn_v VARCHAR(40) NOT NULL DEFAULT 'nn')",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "CREATE TABLE family_source ("
        "id INT NOT NULL, "
        "i INT NULL, "
        "s VARCHAR(40) NULL, "
        "txt TEXT NULL, "
        "dec_value DECIMAL(6,2) NULL, "
        "dbl_value DOUBLE NULL, "
        "date_value DATE NULL, "
        "time_value TIME NULL, "
        "datetime_value DATETIME NULL, "
        "timestamp_value TIMESTAMP NULL)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "CREATE TABLE binary_target (id INT NOT NULL, b BINARY(3), v VARBINARY(1))",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "CREATE TABLE binary_source (id INT NOT NULL, v VARBINARY(1), b BINARY(3))",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "CREATE TABLE temporal_text_source ("
        "id INT NOT NULL, "
        "date_text VARCHAR(40), "
        "time_text VARCHAR(40), "
        "datetime_text VARCHAR(40), "
        "timestamp_text VARCHAR(40))",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "INSERT INTO target VALUES "
        "(1, 'User 0000018', 'old-18', 10, 7), "
        "(2, 'User 0000019', 'old-19', 20, 7), "
        "(3, 'User 0000020', NULL, 30, 7)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "INSERT INTO source VALUES "
        "(10, 'User 0000018', 'source-18', 100), "
        "(11, 'User 0000019', 'source-19', 200), "
        "(12, 'User 0000020', NULL, NULL)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "INSERT INTO family_target VALUES ("
        "1, 1, 'old', 'old text', 1.23, 1.5, '2024-01-01', '01:02:03', "
        "'2024-01-01 01:02:03', '2024-01-01 01:02:03', DEFAULT)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "INSERT INTO family_source VALUES "
        "(10, 42, 'new', 'new text', 12.34, 6.25, '2025-02-03', '04:05:06', "
        "'2025-02-03 04:05:06', '2025-02-03 04:05:06'), "
        "(11, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL)",
        &result
    );
    mylite_result_free(result);
    result = NULL;
    failures +=
        execute_ok(database, "INSERT INTO binary_target VALUES (1, X'410000', X'41')", &result);
    mylite_result_free(result);
    result = NULL;
    failures +=
        execute_ok(database, "INSERT INTO binary_source VALUES (10, X'51', X'515253')", &result);
    mylite_result_free(result);
    result = NULL;
    failures += execute_ok(
        database,
        "INSERT INTO temporal_text_source VALUES ("
        "1, '2026-03-04', '05:06:07', '2026-03-04 05:06:07', "
        "'2026-03-04 05:06:07')",
        &result
    );
    mylite_result_free(result);

    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "execute '%s': expected OK, got %d / %d %s %s\n",
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
    failures += mylite_test_expect_true(result == NULL, "failed execute leaves result null");
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, "error code");
    failures += mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, "SQLSTATE");
    failures += mylite_test_expect_contains(
        mylite_errmsg(database),
        expected.message_part,
        "error message"
    );
    mylite_result_free(result);

    return failures;
}

static int expect_update_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures +=
        mylite_test_expect_size(mylite_result_column_count(result), 0U, "update column count");
    failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, "update row count");
    failures += mylite_test_expect_int64(
        mylite_result_affected_rows(result),
        affected_rows,
        "update affected"
    );
    failures +=
        mylite_test_expect_size(mylite_result_warning_count(result), 0U, "update warning count");
    mylite_result_free(result);

    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    failures += mylite_test_expect_size(
        mylite_result_column_count(result),
        query.column_count,
        query.context
    );
    failures +=
        mylite_test_expect_size(mylite_result_row_count(result), query.row_count, query.context);
    for (size_t row = 0U; row < query.row_count; ++row) {
        for (size_t column = 0U; column < query.column_count; ++column) {
            size_t value_index = (row * query.column_count) + column;

            failures +=
                expect_result_value(result, row, column, query.values[value_index], query.context);
        }
    }
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, query.context);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, query.context);
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
        return mylite_test_expect_true(actual == NULL, context);
    }

    return mylite_test_expect_text(actual, expected, context);
}

static int expect_binary_cell(
    const mylite_result *result,
    size_t row,
    size_t column,
    struct expected_bytes expected,
    const char *context
) {
    const unsigned char *actual = mylite_result_value_bytes(result, row, column);
    size_t actual_size = mylite_result_value_size(result, row, column);
    int failures = mylite_test_expect_true(actual != NULL, context);

    failures += mylite_test_expect_size(actual_size, expected.size, context);
    if (actual != NULL && actual_size == expected.size) {
        failures += expect_bytes(actual, expected.bytes, expected.size, context);
    }

    return failures;
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
