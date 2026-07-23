#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include "runtime/mylite_catalog.h"
#include "runtime/mylite_connection.h"
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
    test_path_suffix_capacity = 16,
    value_column_count = 5,
    variable_row_column_count = 2,
    warning_column_count = 3,
    mysql_error_parse = 1064,
    mysql_error_incorrect_argument_type = 1232,
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
    size_t warning_count;
    const char *context;
};

struct expected_variable_value {
    const char *expected;
    const char *context;
};

static int test_group_concat_max_len_values_and_assignment(void);
static int test_group_concat_max_len_truncation_warnings_and_file_safety(void);
static int test_group_concat_max_len_independent_handles(void);
static int seed_group_concat_data(mylite_db *database);
static int seed_utf8_data(mylite_db *database);
static int expect_query(mylite_db *database, struct expected_query query);
static int expect_statement_warning_count(
    mylite_db *database,
    const char *sql,
    size_t expected_warning_count,
    const char *context
);
static int expect_variable_value(mylite_db *database, struct expected_variable_value expected);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_discard(mylite_db *database, const char *sql);
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

    failures += test_group_concat_max_len_values_and_assignment();
    failures += test_group_concat_max_len_truncation_warnings_and_file_safety();
    failures += test_group_concat_max_len_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_group_concat_max_len_values_and_assignment(void) {
    static const char *const value_columns[value_column_count] = {
        "@@group_concat_max_len",
        "@@GLOBAL.group_concat_max_len",
        "@@SESSION.group_concat_max_len",
        "@@LOCAL.group_concat_max_len",
        "HEX(@@group_concat_max_len)",
    };
    static const char *const default_values[value_column_count] = {
        "1024",
        "1024",
        "1024",
        "1024",
        "400",
    };
    static const char *const session_values[value_column_count] = {
        "8",
        "1024",
        "8",
        "8",
        "8",
    };
    static const char *const default_variable_rows[variable_row_column_count] = {
        "group_concat_max_len",
        "1024",
    };
    static const char *const session_variable_rows[variable_row_column_count] = {
        "group_concat_max_len",
        "8",
    };
    static const char *const clamp_warning_values[warning_column_count] = {
        "Warning",
        "1292",
        "Truncated incorrect group_concat_max_len value: '1'",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open variable memory");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@group_concat_max_len, @@GLOBAL.group_concat_max_len, "
                   "@@SESSION.group_concat_max_len, @@LOCAL.group_concat_max_len, "
                   "HEX(@@group_concat_max_len)",
            .columns = value_columns,
            .column_count = value_column_count,
            .values = default_values,
            .row_count = 1U,
            .context = "default scalar values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW VARIABLES LIKE 'group_concat_max_len'",
            .columns = (const char *const[]){"Variable_name", "Value"},
            .column_count = variable_row_column_count,
            .values = default_variable_rows,
            .row_count = 1U,
            .context = "default show variables row",
        }
    );

    failures += expect_statement_warning_count(
        database,
        "SET SESSION group_concat_max_len = 8",
        0U,
        "set session value"
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@group_concat_max_len, @@GLOBAL.group_concat_max_len, "
                   "@@SESSION.group_concat_max_len, @@LOCAL.group_concat_max_len, "
                   "HEX(@@group_concat_max_len)",
            .columns = value_columns,
            .column_count = value_column_count,
            .values = session_values,
            .row_count = 1U,
            .context = "session scalar values",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW SESSION VARIABLES LIKE 'group_concat_max_len'",
            .columns = (const char *const[]){"Variable_name", "Value"},
            .column_count = variable_row_column_count,
            .values = session_variable_rows,
            .row_count = 1U,
            .context = "session show variables row",
        }
    );

    failures += expect_statement_warning_count(
        database,
        "SET group_concat_max_len = DEFAULT",
        0U,
        "set default"
    );
    failures += expect_variable_value(
        database,
        (struct expected_variable_value){.expected = "1024", .context = "default restore"}
    );
    failures += expect_statement_warning_count(
        database,
        "SET LOCAL group_concat_max_len = +7",
        0U,
        "set positive signed"
    );
    failures += expect_variable_value(
        database,
        (struct expected_variable_value){.expected = "7", .context = "positive signed assignment"}
    );
    failures += expect_statement_warning_count(
        database,
        "SET group_concat_max_len = (9)",
        0U,
        "set parenthesized integer"
    );
    failures += expect_variable_value(
        database,
        (struct expected_variable_value){
            .expected = "9",
            .context = "parenthesized integer assignment",
        }
    );
    failures += expect_statement_warning_count(
        database,
        "SET @@SESSION.group_concat_max_len = 10",
        0U,
        "set direct session system variable"
    );
    failures += expect_variable_value(
        database,
        (struct expected_variable_value){
            .expected = "10",
            .context = "direct session assignment",
        }
    );
    failures += expect_statement_warning_count(
        database,
        "SET @@LOCAL.group_concat_max_len = 11",
        0U,
        "set direct local system variable"
    );
    failures += expect_variable_value(
        database,
        (struct expected_variable_value){
            .expected = "11",
            .context = "direct local assignment",
        }
    );
    failures += expect_statement_warning_count(database, "SET @n = 12", 0U, "set integer user var");
    failures += expect_statement_warning_count(
        database,
        "SET group_concat_max_len = @n",
        0U,
        "set from integer user var"
    );
    failures += expect_variable_value(
        database,
        (struct expected_variable_value){.expected = "12", .context = "integer user var assignment"}
    );

    failures += expect_statement_warning_count(
        database,
        "SET group_concat_max_len = 1",
        1U,
        "minimum clamp warning"
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = (const char *const[]){"Level", "Code", "Message"},
            .column_count = warning_column_count,
            .values = clamp_warning_values,
            .row_count = 1U,
            .context = "minimum clamp warning row",
        }
    );
    failures += expect_variable_value(
        database,
        (struct expected_variable_value){.expected = "4", .context = "minimum clamp value"}
    );
    failures += expect_statement_warning_count(
        database,
        "SET @@group_concat_max_len = TRUE",
        1U,
        "direct true clamp warning"
    );
    failures += expect_variable_value(
        database,
        (struct expected_variable_value){.expected = "4", .context = "direct true clamp value"}
    );
    failures += expect_statement_warning_count(
        database,
        "SET group_concat_max_len = FALSE",
        1U,
        "false clamp warning"
    );
    failures += expect_variable_value(
        database,
        (struct expected_variable_value){.expected = "4", .context = "false clamp value"}
    );
    failures += expect_statement_warning_count(
        database,
        "SET group_concat_max_len = -1",
        1U,
        "negative clamp warning"
    );
    failures += expect_variable_value(
        database,
        (struct expected_variable_value){.expected = "4", .context = "negative clamp value"}
    );

    failures += expect_statement_warning_count(
        database,
        "SET group_concat_max_len = 12",
        0U,
        "restore before rollback test"
    );
    failures += execute_error(
        database,
        "SET group_concat_max_len = 8, group_concat_max_len = 'bad'",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_argument_type,
            .sqlstate = "42000",
            .message_part = "Incorrect argument type",
        }
    );
    failures += expect_variable_value(
        database,
        (struct expected_variable_value){
            .expected = "12",
            .context = "failed multi-assignment rollback",
        }
    );
    failures += execute_error(
        database,
        "SET group_concat_max_len = '8'",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_argument_type,
            .sqlstate = "42000",
            .message_part = "Incorrect argument type",
        }
    );
    failures += execute_error(
        database,
        "SET group_concat_max_len = 1.5",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_argument_type,
            .sqlstate = "42000",
            .message_part = "Incorrect argument type",
        }
    );
    failures += execute_error(
        database,
        "SET group_concat_max_len = NULL",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_argument_type,
            .sqlstate = "42000",
            .message_part = "Incorrect argument type",
        }
    );
    failures += execute_error(
        database,
        "SET group_concat_max_len = 18446744073709551616",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_argument_type,
            .sqlstate = "42000",
            .message_part = "Incorrect argument type",
        }
    );
    failures +=
        expect_statement_warning_count(database, "SET @s = '12'", 0U, "set string user var");
    failures += execute_error(
        database,
        "SET group_concat_max_len = @s",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_argument_type,
            .sqlstate = "42000",
            .message_part = "Incorrect argument type",
        }
    );
    failures += execute_error(
        database,
        "SET GLOBAL group_concat_max_len = 8",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "not supported",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_group_concat_max_len_truncation_warnings_and_file_safety(void) {
    static const char *const capped_columns[] = {"names"};
    static const char *const capped_values[] = {"alpha|be"};
    static const char *const grouped_columns[] = {
        "g",
        "GROUP_CONCAT(name ORDER BY id SEPARATOR '|')",
    };
    static const char *const grouped_values[] = {
        "1",
        "alpha|be",
        "2",
        "delta|ec",
        "3",
        NULL,
    };
    static const char *const grouped_warning_values[] = {
        "Warning",
        "1260",
        "Row 2 was cut by GROUP_CONCAT()",
        "Warning",
        "1260",
        "Row 4 was cut by GROUP_CONCAT()",
    };
    static const char *const separator_values[] = {"a---"};
    static const char *const utf8_values[] = {
        "\xE2\x82\xAC"
        "\xE2\x82\xAC",
    };
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    mylite_db *database = NULL;
    const struct mylite_catalog *catalog = NULL;
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation = 0U;
    uint64_t sqlite_schema_generation = 0U;
    int failures = 0;

    if (mylite_test_make_path(path, sizeof(path), "truncation") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "open truncation file");
    failures += execute_discard(database, "CREATE DATABASE app");
    failures += execute_discard(database, "USE app");
    failures += seed_group_concat_data(database);
    failures += seed_utf8_data(database);

    catalog = mylite_connection_catalog_for_test(database);
    session = mylite_connection_session_state(database);
    if (catalog != NULL) {
        catalog_generation = catalog->generation;
    }
    if (session != NULL) {
        sqlite_schema_generation = session->sqlite_schema_generation;
    }

    failures += expect_statement_warning_count(
        database,
        "SET group_concat_max_len = 8",
        0U,
        "set cap eight"
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT GROUP_CONCAT(name ORDER BY id SEPARATOR '|') AS names FROM items",
            .columns = capped_columns,
            .column_count = 1U,
            .values = capped_values,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "ungrouped truncation",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = (const char *const[]){"Level", "Code", "Message"},
            .column_count = warning_column_count,
            .values =
                (const char *const[]){
                    "Warning",
                    "1260",
                    "Row 2 was cut by GROUP_CONCAT()",
                },
            .row_count = 1U,
            .context = "ungrouped truncation warning",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT g, GROUP_CONCAT(name ORDER BY id SEPARATOR '|') "
                   "FROM items GROUP BY g ORDER BY g",
            .columns = grouped_columns,
            .column_count = 2U,
            .values = grouped_values,
            .row_count = 3U,
            .warning_count = 2U,
            .context = "grouped truncation",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .columns = (const char *const[]){"Level", "Code", "Message"},
            .column_count = warning_column_count,
            .values = grouped_warning_values,
            .row_count = 2U,
            .context = "grouped truncation warnings",
        }
    );

    failures += expect_statement_warning_count(
        database,
        "SET group_concat_max_len = 4",
        0U,
        "set cap four"
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT GROUP_CONCAT(value ORDER BY id SEPARATOR '---') FROM separator_values",
            .columns = (const char *const[]){"GROUP_CONCAT(value ORDER BY id SEPARATOR '---')"},
            .column_count = 1U,
            .values = separator_values,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "separator truncation",
        }
    );

    failures += expect_statement_warning_count(
        database,
        "SET group_concat_max_len = 7",
        0U,
        "set utf8 cap"
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT GROUP_CONCAT(s ORDER BY id SEPARATOR '') FROM utf8_values",
            .columns = (const char *const[]){"GROUP_CONCAT(s ORDER BY id SEPARATOR '')"},
            .column_count = 1U,
            .values = utf8_values,
            .row_count = 1U,
            .warning_count = 1U,
            .context = "utf8 boundary truncation",
        }
    );

    failures += expect_statement_warning_count(
        database,
        "SET group_concat_max_len = 1024",
        0U,
        "restore default cap"
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT GROUP_CONCAT(id ORDER BY id) FROM items",
            .columns = (const char *const[]){"GROUP_CONCAT(id ORDER BY id)"},
            .column_count = 1U,
            .values = (const char *const[]){"1,2,3,4,5,6"},
            .row_count = 1U,
            .context = "integer group concat uncapped",
        }
    );

    if (catalog != NULL) {
        failures += mylite_test_expect_int64(
            (int64_t)catalog->generation,
            (int64_t)catalog_generation,
            "catalog generation unchanged by truncation"
        );
    }
    if (session != NULL) {
        failures += mylite_test_expect_int64(
            (int64_t)session->sqlite_schema_generation,
            (int64_t)sqlite_schema_generation,
            "sqlite generation unchanged by truncation"
        );
    }
    failures += mylite_test_expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read truncation preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "truncation preamble unchanged"
    );

    mylite_close(database);
    database = NULL;
    failures +=
        mylite_test_expect_int(mylite_open(path, &database), MYLITE_OK, "reopen truncation file");
    failures += execute_discard(database, "USE app");
    failures += expect_variable_value(
        database,
        (struct expected_variable_value){
            .expected = "1024",
            .context = "reopen resets session variable",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT GROUP_CONCAT(name ORDER BY id SEPARATOR '|') AS names FROM items",
            .columns = capped_columns,
            .column_count = 1U,
            .values = (const char *const[]){"alpha|beta|delta|echo"},
            .row_count = 1U,
            .context = "reopen full group concat",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_group_concat_max_len_independent_handles(void) {
    static const char *const columns[] = {"names"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    if (mylite_test_make_path(first_path, sizeof(first_path), "first") != 0 ||
        mylite_test_make_path(second_path, sizeof(second_path), "second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures +=
        mylite_test_expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first handle");
    failures +=
        mylite_test_expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second handle");
    failures += execute_discard(first, "CREATE DATABASE app");
    failures += execute_discard(second, "CREATE DATABASE app");
    failures += execute_discard(first, "USE app");
    failures += execute_discard(second, "USE app");
    failures += seed_group_concat_data(first);
    failures += seed_group_concat_data(second);
    failures += expect_statement_warning_count(
        first,
        "SET group_concat_max_len = 8",
        0U,
        "set first handle cap"
    );

    failures += expect_query(
        first,
        (struct expected_query){
            .sql = "SELECT GROUP_CONCAT(name ORDER BY id SEPARATOR '|') AS names FROM items",
            .columns = columns,
            .column_count = 1U,
            .values = (const char *const[]){"alpha|be"},
            .row_count = 1U,
            .warning_count = 1U,
            .context = "first handle capped",
        }
    );
    failures += expect_query(
        second,
        (struct expected_query){
            .sql = "SELECT GROUP_CONCAT(name ORDER BY id SEPARATOR '|') AS names FROM items",
            .columns = columns,
            .column_count = 1U,
            .values = (const char *const[]){"alpha|beta|delta|echo"},
            .row_count = 1U,
            .context = "second handle default cap",
        }
    );
    failures += expect_variable_value(
        first,
        (struct expected_variable_value){.expected = "8", .context = "first handle variable"}
    );
    failures += expect_variable_value(
        second,
        (struct expected_variable_value){.expected = "1024", .context = "second handle variable"}
    );

    mylite_close(first);
    mylite_close(second);
    remove_related_files(first_path);
    remove_related_files(second_path);
    return failures;
}

static int seed_group_concat_data(mylite_db *database) {
    int failures = 0;

    failures +=
        execute_discard(database, "CREATE TABLE items (g INT, id INT NOT NULL, name VARCHAR(20))");
    failures += execute_discard(
        database,
        "INSERT INTO items VALUES "
        "(1, 1, 'alpha'), (1, 2, 'beta'), (1, 3, NULL), "
        "(2, 4, 'delta'), (2, 5, 'echo'), (3, 6, NULL)"
    );
    failures += execute_discard(
        database,
        "CREATE TABLE separator_values (id INT NOT NULL, value VARCHAR(10))"
    );
    failures += execute_discard(database, "INSERT INTO separator_values VALUES (1, 'a'), (2, 'b')");
    return failures;
}

static int seed_utf8_data(mylite_db *database) {
    int failures = 0;

    failures +=
        execute_discard(database, "CREATE TABLE utf8_values (id INT NOT NULL, s VARCHAR(20))");
    failures += execute_discard(
        database,
        "INSERT INTO utf8_values VALUES (1, '"
        "\xE2\x82\xAC"
        "\xE2\x82\xAC"
        "\xE2\x82\xAC"
        "'), (2, 'ab')"
    );
    return failures;
}

static int expect_query(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    if (failures == 0) {
        failures += mylite_test_expect_size(
            mylite_result_column_count(result),
            query.column_count,
            query.context
        );
        failures += mylite_test_expect_size(
            mylite_result_row_count(result),
            query.row_count,
            query.context
        );
        failures += mylite_test_expect_size(
            mylite_result_warning_count(result),
            query.warning_count,
            query.context
        );
    }
    for (size_t column = 0U; failures == 0 && column < query.column_count; ++column) {
        failures += mylite_test_expect_text_or_null(
            mylite_result_column_name(result, column),
            query.columns[column],
            query.context
        );
    }
    for (size_t row = 0U; failures == 0 && row < query.row_count; ++row) {
        for (size_t column = 0U; failures == 0 && column < query.column_count; ++column) {
            size_t index = (row * query.column_count) + column;

            failures += mylite_test_expect_text_or_null(
                mylite_result_value_text(result, row, column),
                query.values[index],
                query.context
            );
        }
    }
    if (failures == 0) {
        failures += mylite_test_expect_int64(mylite_result_affected_rows(result), 0, query.context);
    }

    mylite_result_free(result);
    return failures;
}

static int expect_statement_warning_count(
    mylite_db *database,
    const char *sql,
    size_t expected_warning_count,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures == 0) {
        failures += mylite_test_expect_size(
            mylite_result_warning_count(result),
            expected_warning_count,
            context
        );
    }
    mylite_result_free(result);
    return failures;
}

static int expect_variable_value(mylite_db *database, struct expected_variable_value expected) {
    return expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @@group_concat_max_len",
            .columns = (const char *const[]){"@@group_concat_max_len"},
            .column_count = 1U,
            .values = (const char *const[]){expected.expected},
            .row_count = 1U,
            .context = expected.context,
        }
    );
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected success, got %d/%s/%s\n",
            sql,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        return 1;
    }
    if (out_result == NULL || *out_result == NULL) {
        fprintf(stderr, "%s: expected result object\n", sql);
        return 1;
    }
    return 0;
}

static int execute_discard(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    mylite_result_free(result);
    return failures;
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
    failures += mylite_test_expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, sql);
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
    char buffer[test_path_capacity + test_path_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        (void)remove(buffer);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");
    size_t read_count = 0U;

    if (file == NULL) {
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        (void)fclose(file);
        return 1;
    }
    read_count = fread(buffer, 1U, size, file);
    if (fclose(file) != 0) {
        return 1;
    }
    return read_count == size ? 0 : 1;
}

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: bytes differ\n", context);
        return 1;
    }
    return 0;
}
