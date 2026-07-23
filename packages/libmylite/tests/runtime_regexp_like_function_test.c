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
    path_suffix_capacity = 16,
    mysql_error_native_function_argument_count = 1582,
    mysql_error_unknown_column = 1054,
    mysql_error_parse = 1064,
    mysql_error_regexp_illegal_argument = 3685,
    mysql_error_regular_expression = 3696,
    mysql_error_regular_expression_character_range = 3697,
    string_row_count = 6,
    non_null_string_row_count = 5,
    remaining_row_count = 5,
    regexp_string_projection_column_count = 5,
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

static int test_scalar_regexp_like_values(void);
static int test_regexp_string_function_values(void);
static int test_table_backed_regexp_like_predicates_and_dml(void);
static int test_regexp_like_diagnostics(void);
static int populate_strings(mylite_db *database);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows);
static int expect_query_values(mylite_db *database, struct expected_query expected);
static int open_app_database(
    mylite_db **out_database,
    const char *name,
    char *path,
    size_t path_size
);
static int reopen_app_database(mylite_db **out_database, const char *path);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_scalar_regexp_like_values();
    failures += test_regexp_string_function_values();
    failures += test_table_backed_regexp_like_predicates_and_dml();
    failures += test_regexp_like_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_scalar_regexp_like_values(void) {
    static const char *const values[] = {
        "1",
        "0",
        "1",
        "1",
        "0",
        NULL,
        NULL,
        NULL,
        "1",
        "1",
        NULL,
        "0",
        "0",
    };
    static const char *const row_status_columns[] = {"ROW_COUNT()", "@@warning_count"};
    static const char *const values_after_select[] = {"-1", "0"};
    static const char *const values_after_do[] = {"0", "0"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_app_database(&database, "scalar", path, sizeof(path));
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT REGEXP_LIKE('abc', 'ABC'), REGEXP_LIKE('abc', 'ABC', 'c'), "
                   "REGEXP_LIKE('abc', 'ABC', 'i'), REGEXP_LIKE('abc', 'ABC', 'ci'), "
                   "REGEXP_LIKE('abc', 'ABC', 'ic'), REGEXP_LIKE(NULL, 'a'), "
                   "REGEXP_LIKE('a', NULL), REGEXP_LIKE('a', 'a', NULL), "
                   "REGEXP_LIKE(123, '23'), REGEXP_LIKE(TRUE, '^1$'), "
                   "REGEXP_LIKE('a', '[', NULL), REGEXP_LIKE('a\\nb', 'a.b'), "
                   "REGEXP_LIKE('a\\nb', '^b')",
            .values = values,
            .column_count = sizeof(values) / sizeof(values[0]),
            .row_count = 1U,
            .context = "scalar REGEXP_LIKE values",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .values = values_after_select,
            .column_count = sizeof(row_status_columns) / sizeof(row_status_columns[0]),
            .row_count = 1U,
            .context = "row count after REGEXP_LIKE select",
        }
    );

    failures +=
        execute_ok(database, "DO REGEXP_LIKE('abc', 'abc'), REGEXP_LIKE('abc','ABC','c')", &result);
    if (failures == 0) {
        failures += mylite_test_expect_size(
            mylite_result_column_count(result),
            0U,
            "regexp_like do columns"
        );
        failures +=
            mylite_test_expect_size(mylite_result_row_count(result), 0U, "regexp_like do rows");
        failures += mylite_test_expect_int64(
            mylite_result_affected_rows(result),
            0,
            "regexp_like do affected"
        );
        failures += mylite_test_expect_size(
            mylite_result_warning_count(result),
            0U,
            "regexp_like do warnings"
        );
    }
    mylite_result_free(result);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .values = values_after_do,
            .column_count = sizeof(row_status_columns) / sizeof(row_status_columns[0]),
            .row_count = 1U,
            .context = "row count after REGEXP_LIKE do",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_regexp_string_function_values(void) {
    static const char *const scalar_values[] = {
        "2",
        "bc",
        "aXaX",
        "0",
        NULL,
        "abc",
        "1",
        "",
        "abcX",
        "XXbXcX",
        "",
        "",
        "",
        "",
    };
    static const char *const empty_row_replace_values[] = {"", ""};
    static const char *const null_values[] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL};
    static const char *const status_values[] = {"-1", "0"};
    static const char *const after_do_values[] = {"0", "0"};
    static const char *const row_values[] = {
        "1", "2", "bc", "aX",   "0", "2", "2", "BC", "AX",  "1", "3", "0",  NULL, "rss_a", "0",
        "4", "0", NULL, "rss_", "0", "5", "0", NULL, "1+2", "0", "6", NULL, NULL, NULL,    "0",
    };
    static const char *const reopen_values[] = {"1", "aX", "2", "AX", "3", "rss_a"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += open_app_database(&database, "regexp-string", path, sizeof(path));
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT REGEXP_INSTR('abcabc', 'b'), REGEXP_SUBSTR('abcabc', 'b.'), "
                   "REGEXP_REPLACE('abcabc', 'b.', 'X'), REGEXP_INSTR('abc', 'z'), "
                   "REGEXP_SUBSTR('abc', 'z'), REGEXP_REPLACE('abc', 'z', 'X'), "
                   "REGEXP_INSTR('AbC', 'a'), REGEXP_SUBSTR('abc', '$'), "
                   "REGEXP_REPLACE('abc', '$', 'X'), REGEXP_REPLACE('abc', 'a*', 'X'), "
                   "REGEXP_REPLACE('', 'a*', 'X'), REGEXP_REPLACE('', '.*', 'X'), "
                   "REGEXP_REPLACE('', '^', 'X'), REGEXP_REPLACE('', '$', 'X')",
            .values = scalar_values,
            .column_count = sizeof(scalar_values) / sizeof(scalar_values[0]),
            .row_count = 1U,
            .context = "scalar REGEXP string function values",
        }
    );
    failures += execute_ok(database, "CREATE TABLE empty_strings(v VARCHAR(16))", NULL);
    failures += execute_ok(database, "INSERT INTO empty_strings VALUES ('')", NULL);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT REGEXP_REPLACE(v, 'a*', 'X'), REGEXP_REPLACE(v, '$', 'X') "
                   "FROM empty_strings",
            .values = empty_row_replace_values,
            .column_count = 2U,
            .row_count = 1U,
            .context = "row REGEXP_REPLACE empty input zero-length match",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT REGEXP_INSTR(NULL, 'a'), REGEXP_INSTR('a', NULL), "
                   "REGEXP_SUBSTR(NULL, 'a'), REGEXP_SUBSTR('a', NULL), "
                   "REGEXP_REPLACE(NULL, 'a', 'x'), REGEXP_REPLACE('a', NULL, 'x'), "
                   "REGEXP_REPLACE('a', 'a', NULL)",
            .values = null_values,
            .column_count = sizeof(null_values) / sizeof(null_values[0]),
            .row_count = 1U,
            .context = "scalar REGEXP string function NULL values",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .values = status_values,
            .column_count = 2U,
            .row_count = 1U,
            .context = "row count after REGEXP string select",
        }
    );

    failures += execute_ok(
        database,
        "DO REGEXP_INSTR('abc', 'b'), REGEXP_SUBSTR('abc', 'b'), "
        "REGEXP_REPLACE('abc', 'b', 'X')",
        &result
    );
    if (failures == 0) {
        failures += mylite_test_expect_size(
            mylite_result_column_count(result),
            0U,
            "regexp string do columns"
        );
        failures +=
            mylite_test_expect_size(mylite_result_row_count(result), 0U, "regexp string do rows");
        failures += mylite_test_expect_int64(
            mylite_result_affected_rows(result),
            0,
            "regexp string do rows"
        );
        failures += mylite_test_expect_size(
            mylite_result_warning_count(result),
            0U,
            "regexp string do warnings"
        );
    }
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT(), @@warning_count",
            .values = after_do_values,
            .column_count = 2U,
            .row_count = 1U,
            .context = "row count after REGEXP string do",
        }
    );

    failures += populate_strings(database);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, REGEXP_INSTR(v, 'b.'), REGEXP_SUBSTR(v, 'b.'), "
                   "REGEXP_REPLACE(v, 'b.', 'X'), REGEXP_INSTR(id, '2') "
                   "FROM strings ORDER BY id",
            .values = row_values,
            .column_count = regexp_string_projection_column_count,
            .row_count = string_row_count,
            .context = "table-backed REGEXP string projection",
        }
    );

    failures += execute_ok(
        database,
        "SELECT REGEXP_INSTR('abc', 'b') AS pos, REGEXP_SUBSTR('abc', 'b') AS sub",
        &result
    );
    if (failures == 0) {
        failures += mylite_test_expect_int(
            mylite_result_column_type(result, 0U),
            MYLITE_RESULT_COLUMN_TYPE_LONGLONG,
            "regexp_instr metadata type"
        );
        failures += mylite_test_expect_int(
            mylite_result_column_type(result, 1U),
            MYLITE_RESULT_COLUMN_TYPE_VAR_STRING,
            "regexp_substr metadata type"
        );
    }
    mylite_result_free(result);
    result = NULL;

    mylite_close(database);
    database = NULL;
    failures += reopen_app_database(&database, path);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, REGEXP_REPLACE(v, 'b.', 'X') FROM strings WHERE id <= 3 ORDER BY id",
            .values = reopen_values,
            .column_count = 2U,
            .row_count = 3U,
            .context = "REGEXP string projection after reopen",
        }
    );
    failures += execute_error(
        database,
        "SELECT REGEXP_INSTR('abc', '')",
        (struct expected_sql_error){
            .code = mysql_error_regexp_illegal_argument,
            .sqlstate = "HY000",
            .message_part = "Illegal argument to a regular expression",
        }
    );
    failures += execute_error(
        database,
        "SELECT REGEXP_SUBSTR('abc', '[')",
        (struct expected_sql_error){
            .code = mysql_error_regular_expression,
            .sqlstate = "HY000",
            .message_part = "unclosed bracket",
        }
    );
    failures += execute_error(
        database,
        "SELECT REGEXP_REPLACE('abc', 'b', 'X', 1)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "optional arguments are not supported",
        }
    );
    failures += execute_error(
        database,
        "SELECT REGEXP_REPLACE()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect parameter count",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_table_backed_regexp_like_predicates_and_dml(void) {
    static const char *const projection_values[] = {
        "1",
        "0",
        "2",
        "0",
        "3",
        "1",
        "4",
        "0",
        "5",
        "0",
        "6",
        NULL,
    };
    static const char *const prefix_ids[] = {"1", "2"};
    static const char *const not_prefix_ids[] = {"3", "4", "5"};
    static const char *const case_sensitive_ids[] = {"2"};
    static const char *const rss_ids[] = {"3"};
    static const char *const no_match_zero_ids[] = {"1", "2", "3", "4", "5"};
    static const char *const null_id[] = {"6"};
    static const char *const after_update_rows[] = {
        "1",
        "abc",
        "seed",
        "2",
        "ABC",
        "seed",
        "3",
        "rss_a",
        "hit",
        "4",
        "rss_",
        "seed",
        "5",
        "1+2",
        "seed",
        "6",
        NULL,
        "seed",
    };
    static const char *const after_delete_rows[] = {
        "1",
        "abc",
        "seed",
        "2",
        "ABC",
        "seed",
        "3",
        "rss_a",
        "hit",
        "4",
        "rss_",
        "seed",
        "6",
        NULL,
        "seed",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "table", path, sizeof(path));
    failures += populate_strings(database);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, REGEXP_LIKE(v, '^rss_.+$') FROM strings ORDER BY id",
            .values = projection_values,
            .column_count = 2U,
            .row_count = string_row_count,
            .context = "REGEXP_LIKE projection over rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE REGEXP_LIKE(v, '^ab') ORDER BY id",
            .values = prefix_ids,
            .column_count = 1U,
            .row_count = 2U,
            .context = "REGEXP_LIKE truth predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE NOT REGEXP_LIKE(v, '^ab') ORDER BY id",
            .values = not_prefix_ids,
            .column_count = 1U,
            .row_count = 3U,
            .context = "NOT REGEXP_LIKE excludes NULL rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE REGEXP_LIKE(v, '^AB', 'c') ORDER BY id",
            .values = case_sensitive_ids,
            .column_count = 1U,
            .row_count = 1U,
            .context = "REGEXP_LIKE c flag is case-sensitive",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE REGEXP_LIKE(v, '^rss_.+$') <=> TRUE ORDER BY id",
            .values = rss_ids,
            .column_count = 1U,
            .row_count = 1U,
            .context = "REGEXP_LIKE null-safe comparison predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE REGEXP_LIKE(v, '^no') = 0 ORDER BY id",
            .values = no_match_zero_ids,
            .column_count = 1U,
            .row_count = non_null_string_row_count,
            .context = "REGEXP_LIKE false comparison predicate",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id FROM strings WHERE REGEXP_LIKE(v, '^no') IS NULL",
            .values = null_id,
            .column_count = 1U,
            .row_count = 1U,
            .context = "REGEXP_LIKE IS NULL predicate",
        }
    );
    failures += expect_dml_ok(
        database,
        "UPDATE strings SET note = 'hit' WHERE REGEXP_LIKE(v, '^rss_.+$')",
        1
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, note FROM strings ORDER BY id",
            .values = after_update_rows,
            .column_count = 3U,
            .row_count = string_row_count,
            .context = "updated rows after REGEXP_LIKE predicate",
        }
    );
    failures += expect_dml_ok(database, "DELETE FROM strings WHERE REGEXP_LIKE(v, '1\\\\+2')", 1);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, note FROM strings ORDER BY id",
            .values = after_delete_rows,
            .column_count = 3U,
            .row_count = remaining_row_count,
            .context = "remaining rows after REGEXP_LIKE delete",
        }
    );

    mylite_close(database);
    database = NULL;

    mylite_file_preamble_init(expected_preamble);
    failures += read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble));
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "REGEXP_LIKE DML preserves preamble"
    );

    failures += reopen_app_database(&database, path);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT id, v, note FROM strings ORDER BY id",
            .values = after_delete_rows,
            .column_count = 3U,
            .row_count = remaining_row_count,
            .context = "REGEXP_LIKE DML persists after reopen",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_regexp_like_diagnostics(void) {
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "diagnostics", path, sizeof(path));
    failures += populate_strings(database);
    failures += execute_error(
        database,
        "SELECT REGEXP_LIKE()",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part =
                "Incorrect parameter count in the call to native function 'REGEXP_LIKE'",
        }
    );
    failures += execute_error(
        database,
        "SELECT REGEXP_LIKE('a')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part =
                "Incorrect parameter count in the call to native function 'REGEXP_LIKE'",
        }
    );
    failures += execute_error(
        database,
        "SELECT REGEXP_LIKE('a', 'a', 'i', 'extra')",
        (struct expected_sql_error){
            .code = mysql_error_native_function_argument_count,
            .sqlstate = "42000",
            .message_part =
                "Incorrect parameter count in the call to native function 'REGEXP_LIKE'",
        }
    );
    failures += execute_error(
        database,
        "SELECT REGEXP_LIKE('a', 'a', 'z')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "REGEXP_LIKE() match_type supports only c and i flags",
        }
    );
    failures += execute_error(
        database,
        "SELECT REGEXP_LIKE('a', 'a', 'm')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "REGEXP_LIKE() match_type supports only c and i flags",
        }
    );
    failures += execute_error(
        database,
        "SELECT REGEXP_LIKE('a', 'a', 'I')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "REGEXP_LIKE() match_type supports only c and i flags",
        }
    );
    failures += execute_error(
        database,
        "SELECT REGEXP_LIKE(NULL, 'a', 'z')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "REGEXP_LIKE() match_type supports only c and i flags",
        }
    );
    failures += execute_error(
        database,
        "SELECT REGEXP_LIKE('a', '[')",
        (struct expected_sql_error){
            .code = mysql_error_regular_expression,
            .sqlstate = "HY000",
            .message_part = "unclosed bracket expression",
        }
    );
    failures += execute_error(
        database,
        "SELECT REGEXP_LIKE(NULL, '[')",
        (struct expected_sql_error){
            .code = mysql_error_regular_expression,
            .sqlstate = "HY000",
            .message_part = "unclosed bracket expression",
        }
    );
    failures += execute_error(
        database,
        "SELECT REGEXP_LIKE('a', '[z-a]')",
        (struct expected_sql_error){
            .code = mysql_error_regular_expression_character_range,
            .sqlstate = "HY000",
            .message_part = "invalid character range",
        }
    );
    failures += execute_error(
        database,
        "SELECT REGEXP_LIKE(CAST('a' AS BINARY), 'A', 'i')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "REGEXP_LIKE() does not support binary values",
        }
    );
    failures += execute_error(
        database,
        "SELECT REGEXP_LIKE(CONCAT(CAST('a' AS BINARY)), 'A', 'i')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "REGEXP_LIKE() does not support binary values",
        }
    );
    failures += execute_error(
        database,
        "SELECT REGEXP_LIKE(id, '^1$') FROM strings",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "REGEXP_LIKE() supports only nonbinary string columns",
        }
    );
    failures += execute_error(
        database,
        "SELECT REGEXP_LIKE(missing, '^a') FROM strings",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'field list'",
        }
    );
    failures += execute_error(
        database,
        "SELECT id FROM strings WHERE REGEXP_LIKE(v, note)",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part =
                "REGEXP_LIKE() supports only string, integer, boolean, and NULL pattern",
        }
    );
    failures += execute_ok(
        database,
        "INSERT INTO strings VALUES (7, '"
        "\xC3"
        "\xA9"
        "', 'seed')",
        NULL
    );
    failures += execute_error(
        database,
        "SELECT REGEXP_LIKE(v, '^.$') FROM strings WHERE id = 7",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "regular expression input supports only ASCII text",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int populate_strings(mylite_db *database) {
    int failures = 0;

    failures += execute_ok(
        database,
        "CREATE TABLE strings (id INT, v VARCHAR(16), note VARCHAR(16))",
        NULL
    );
    failures += execute_ok(
        database,
        "INSERT INTO strings VALUES "
        "(1, 'abc', 'seed'), "
        "(2, 'ABC', 'seed'), "
        "(3, 'rss_a', 'seed'), "
        "(4, 'rss_', 'seed'), "
        "(5, '1+2', 'seed'), "
        "(6, NULL, 'seed')",
        NULL
    );
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

static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures != 0) {
        return failures;
    }
    failures += mylite_test_expect_size(mylite_result_column_count(result), 0U, sql);
    failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, sql);
    failures += mylite_test_expect_int64(mylite_result_affected_rows(result), affected_rows, sql);
    failures += mylite_test_expect_size(mylite_result_warning_count(result), 0U, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query expected) {
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

static int reopen_app_database(mylite_db **out_database, const char *path) {
    int failures =
        mylite_test_expect_int(mylite_open(path, out_database), MYLITE_OK, "reopen app database");

    if (failures == 0) {
        failures += execute_ok(*out_database, "USE app", NULL);
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
    char related_path[test_path_capacity + path_suffix_capacity];
    int written = snprintf(related_path, sizeof(related_path), "%s%s", path, suffix);

    if (written < 0 || (size_t)written >= sizeof(related_path)) {
        return;
    }
    (void)remove(related_path);
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t bytes_read = 0U;
    int failures = 0;

    if (file == NULL) {
        fprintf(stderr, "%s: failed to open file\n", path);
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fprintf(stderr, "%s: failed to seek file\n", path);
        failures += 1;
    }
    if (failures == 0) {
        bytes_read = fread(buffer, 1U, size, file);
        if (bytes_read != size) {
            fprintf(stderr, "%s: expected %zu bytes, read %zu\n", path, size, bytes_read);
            failures += 1;
        }
    }
    if (fclose(file) != 0) {
        fprintf(stderr, "%s: failed to close file\n", path);
        failures += 1;
    }

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

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: byte sequence mismatch\n", context);
        return 1;
    }
    return 0;
}
