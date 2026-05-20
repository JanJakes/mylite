#include <mylite/mylite.h>

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

enum {
    test_path_capacity = 1024,
    path_suffix_capacity = 16,
    sql_select_limit_value_column_count = 6,
    sql_select_limit_label_column_count = 5,
    sql_select_limit_diagnostics_column_count = 4,
    sql_select_limit_selected_column_count = 2,
    sql_select_limit_independent_column_count = 3,
    mysql_error_parse = 1064,
    mysql_error_unknown_system_variable = 1193,
    mysql_error_incorrect_argument_type = 1232,
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

struct expected_table_result {
    const char *const *columns;
    const char *const *values;
    size_t column_count;
    size_t row_count;
    const char *context;
};

static int test_sql_select_limit_values_and_persistence(void);
static int test_sql_select_limit_qualifiers_and_errors(void);
static int test_independent_sql_select_limit_handles(void);
static int expect_result(const mylite_result *result, struct expected_result expected);
static int expect_table_result(const mylite_result *result, struct expected_table_result expected);
static int expect_query_result(
    mylite_db *database,
    const char *sql,
    struct expected_result expected
);
static int expect_query_table_result(
    mylite_db *database,
    const char *sql,
    struct expected_table_result expected
);
static int expect_show_count_warnings(
    mylite_db *database,
    const char *expected,
    const char *context
);
static int expect_show_count_errors(mylite_db *database, const char *expected, const char *context);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);
static int expect_text_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_sql_select_limit_values_and_persistence();
    failures += test_sql_select_limit_qualifiers_and_errors();
    failures += test_independent_sql_select_limit_handles();

    return failures == 0 ? 0 : 1;
}

static int test_sql_select_limit_values_and_persistence(void) {
    static const char *const value_columns[] = {
        "@@sql_select_limit",
        "@@global.sql_select_limit",
        "@@session.sql_select_limit",
        "@@local.sql_select_limit",
        "@@warning_count",
        "ROW_COUNT()",
    };
    static const char *const value_values[] = {
        "18446744073709551615",
        "18446744073709551615",
        "18446744073709551615",
        "18446744073709551615",
        "0",
        "-1",
    };
    static const char *const session_two_values[] = {
        "2",
        "18446744073709551615",
        "2",
        "2",
        "0",
        "0",
    };
    static const char *const session_default_values[] = {
        "18446744073709551615",
        "18446744073709551615",
        "18446744073709551615",
        "18446744073709551615",
        "0",
        "0",
    };
    static const char *const hex_columns[] = {
        "HEX(@@sql_select_limit)",
        "HEX(@@global.sql_select_limit)",
    };
    static const char *const hex_values[] = {"2", "FFFFFFFFFFFFFFFF"};
    static const char *const label_columns[] = {
        "@@SQL_SELECT_LIMIT",
        "@@Global.Sql_Select_Limit",
        "@@session.`sql_select_limit`",
        "@@`sql_select_limit`",
        "(@@sql_select_limit)",
    };
    static const char *const label_values[] = {
        "18446744073709551615",
        "18446744073709551615",
        "18446744073709551615",
        "18446744073709551615",
        "18446744073709551615",
    };
    static const char *const mixed_columns[] = {
        "@@sql_select_limit",
        "@@foreign_key_checks",
        "@@unique_checks",
        "@@updatable_views_with_limit",
        "@@sql_safe_updates",
        "@@sql_warnings",
        "@@sql_quote_show_create",
        "@@autocommit",
        "@@default_storage_engine",
        "@@character_set_server",
        "@@version_comment",
    };
    static const char *const mixed_values[] = {
        "18446744073709551615",
        "1",
        "1",
        "YES",
        "0",
        "0",
        "1",
        "1",
        "InnoDB",
        "utf8mb4",
        "MyLite",
    };
    static const char *const diagnostics_columns[] = {
        "@@sql_select_limit",
        "@@warning_count",
        "@@error_count",
        "ROW_COUNT()",
    };
    static const char *const warning_values[] = {
        "18446744073709551615",
        "1",
        "0",
        "-1",
    };
    static const char *const error_values[] = {
        "18446744073709551615",
        "1",
        "1",
        "-1",
    };
    static const char *const selected_columns[] = {"@@sql_select_limit", "DATABASE()"};
    static const char *const selected_values[] = {"18446744073709551615", "app"};
    static const char *const table_columns[] = {"id", "score"};
    static const char *const table_values[] = {"1", "10", "2", "20", "3", "30"};
    static const char *const limited_table_values[] = {"1", "10", "2", "20"};
    static const char *const explicit_limit_table_values[] = {"1", "10", "2", "20", "3", "30"};
    static const char *const internal_source_table_values[] = {"1", "10", "2", "20", "3", "30"};
    static const char *const show_variable_columns[] = {"Variable_name", "Value"};
    static const char *const show_zero_values[] = {"sql_select_limit", "0"};
    static const char *const scalar_one_column[] = {"1"};
    static const char *const aggregate_columns[] = {"c"};
    static const char *const group_columns[] = {"score", "c"};
    static const char *const group_values[] = {"10", "1"};
    static const char *const union_columns[] = {"id"};
    static const char *const union_values[] = {"1"};
    static const char *const information_schema_columns[] = {"TABLE_NAME"};
    static const char *const information_schema_limited_values[] = {"child"};
    static const char *const information_schema_explicit_values[] = {"child", "ctas_copy"};
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation = 0U;
    uint64_t sqlite_schema_generation = 0U;
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "lifecycle") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open sql select limit file");
    session = mylite_connection_session_state(database);
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;

    failures += execute_ok(
        database,
        "SELECT @@sql_select_limit, @@global.sql_select_limit, "
        "@@session.sql_select_limit, @@local.sql_select_limit, @@warning_count, "
        "ROW_COUNT() FROM DUAL",
        &result
    );
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = value_columns,
            .values = value_values,
            .count = sql_select_limit_value_column_count,
            .context = "sql select limit values",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "SELECT @@SQL_SELECT_LIMIT, @@Global.Sql_Select_Limit, "
        "@@session.`sql_select_limit`, @@`sql_select_limit`, (@@sql_select_limit)",
        &result
    );
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = label_columns,
            .values = label_values,
            .count = sql_select_limit_label_column_count,
            .context = "sql select limit labels",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "SELECT @@sql_select_limit, @@foreign_key_checks, @@unique_checks, "
        "@@updatable_views_with_limit, @@sql_safe_updates, @@sql_warnings, "
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
            .context = "mixed sql select limit scalar values",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_statement_ok(database, "SHOW PROCESSLIST");
    failures += execute_ok(
        database,
        "SELECT @@sql_select_limit, @@warning_count, @@error_count, ROW_COUNT()",
        &result
    );
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = diagnostics_columns,
            .values = warning_values,
            .count = sql_select_limit_diagnostics_column_count,
            .context = "sql select limit warning diagnostics",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_show_count_warnings(database, "0", "sql select limit clears warnings");

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
        "SELECT @@sql_select_limit, @@warning_count, @@error_count, ROW_COUNT()",
        &result
    );
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = diagnostics_columns,
            .values = error_values,
            .count = sql_select_limit_diagnostics_column_count,
            .context = "sql select limit error diagnostics",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_show_count_errors(database, "0", "sql select limit clears errors");
    failures += expect_show_count_warnings(database, "0", "sql select limit clears error warnings");

    session = mylite_connection_session_state(database);
    failures += expect_int64(
        (int64_t)session->catalog_generation,
        (int64_t)catalog_generation,
        "catalog generation unchanged by sql select limit reads"
    );
    failures += expect_int64(
        (int64_t)session->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "sqlite schema generation unchanged by sql select limit reads"
    );
    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read sql select limit preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after sql select limit reads"
    );

    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(database, "CREATE TABLE child (id INT, score INT)");
    failures += execute_statement_ok(database, "CREATE TABLE sibling (id INT)");
    failures += execute_statement_ok(
        database,
        "INSERT INTO child (id, score) VALUES (1, 10),(2, 20),(3, 30)"
    );
    failures += expect_query_result(
        database,
        "SELECT @@sql_select_limit, DATABASE()",
        (struct expected_result){
            .columns = selected_columns,
            .values = selected_values,
            .count = sql_select_limit_selected_column_count,
            .context = "sql select limit with selected database",
        }
    );
    failures += expect_query_table_result(
        database,
        "SELECT id, score FROM child ORDER BY id",
        (struct expected_table_result){
            .columns = table_columns,
            .values = table_values,
            .column_count = sizeof(table_columns) / sizeof(table_columns[0]),
            .row_count = 3U,
            .context = "default sql select limit does not cap descriptor select",
        }
    );

    failures += execute_statement_ok(database, "SET SESSION sql_select_limit = 2");
    session = mylite_connection_session_state(database);
    failures += expect_int64(
        (int64_t)session->sql_select_limit,
        2,
        "session sql select limit updates state"
    );
    failures += expect_query_result(
        database,
        "SELECT @@sql_select_limit, @@global.sql_select_limit, "
        "@@session.sql_select_limit, @@local.sql_select_limit, @@warning_count, ROW_COUNT()",
        (struct expected_result){
            .columns = value_columns,
            .values = session_two_values,
            .count = sql_select_limit_value_column_count,
            .context = "mutable session sql select limit values",
        }
    );
    failures += expect_query_result(
        database,
        "SELECT HEX(@@sql_select_limit), HEX(@@global.sql_select_limit)",
        (struct expected_result){
            .columns = hex_columns,
            .values = hex_values,
            .count = sizeof(hex_columns) / sizeof(hex_columns[0]),
            .context = "mutable sql select limit in numeric scalar function",
        }
    );
    failures += expect_query_table_result(
        database,
        "SELECT id, score FROM child ORDER BY id",
        (struct expected_table_result){
            .columns = table_columns,
            .values = limited_table_values,
            .column_count = sizeof(table_columns) / sizeof(table_columns[0]),
            .row_count = 2U,
            .context = "session sql select limit caps descriptor select",
        }
    );
    failures += expect_query_table_result(
        database,
        "SELECT id, score FROM child ORDER BY id LIMIT 3",
        (struct expected_table_result){
            .columns = table_columns,
            .values = explicit_limit_table_values,
            .column_count = sizeof(table_columns) / sizeof(table_columns[0]),
            .row_count = 3U,
            .context = "explicit descriptor select limit overrides sql select limit",
        }
    );
    failures += execute_statement_ok(database, "CREATE TABLE insert_copy (id INT, score INT)");
    failures += expect_dml_ok(
        database,
        "INSERT INTO insert_copy SELECT id, score FROM child ORDER BY id",
        3
    );
    failures += expect_query_table_result(
        database,
        "SELECT id, score FROM insert_copy ORDER BY id LIMIT 3",
        (struct expected_table_result){
            .columns = table_columns,
            .values = internal_source_table_values,
            .column_count = sizeof(table_columns) / sizeof(table_columns[0]),
            .row_count = 3U,
            .context = "sql select limit does not cap insert select source rows",
        }
    );
    failures += execute_statement_ok(
        database,
        "CREATE TABLE ctas_copy AS SELECT id, score FROM child ORDER BY id"
    );
    failures += expect_query_table_result(
        database,
        "SELECT id, score FROM ctas_copy ORDER BY id LIMIT 3",
        (struct expected_table_result){
            .columns = table_columns,
            .values = internal_source_table_values,
            .column_count = sizeof(table_columns) / sizeof(table_columns[0]),
            .row_count = 3U,
            .context = "sql select limit does not cap create table select source rows",
        }
    );

    failures += execute_statement_ok(database, "SET LOCAL sql_select_limit = 1");
    failures += expect_query_table_result(
        database,
        "SELECT TABLE_NAME FROM information_schema.tables WHERE TABLE_SCHEMA = 'app' "
        "ORDER BY TABLE_NAME",
        (struct expected_table_result){
            .columns = information_schema_columns,
            .values = information_schema_limited_values,
            .column_count =
                sizeof(information_schema_columns) / sizeof(information_schema_columns[0]),
            .row_count = 1U,
            .context = "sql select limit caps information schema select",
        }
    );
    failures += expect_query_table_result(
        database,
        "SELECT TABLE_NAME FROM information_schema.tables WHERE TABLE_SCHEMA = 'app' "
        "ORDER BY TABLE_NAME LIMIT 2",
        (struct expected_table_result){
            .columns = information_schema_columns,
            .values = information_schema_explicit_values,
            .column_count =
                sizeof(information_schema_columns) / sizeof(information_schema_columns[0]),
            .row_count = 2U,
            .context = "information schema explicit limit overrides sql select limit",
        }
    );
    failures += expect_query_table_result(
        database,
        "SELECT score, COUNT(*) AS c FROM child GROUP BY score ORDER BY score",
        (struct expected_table_result){
            .columns = group_columns,
            .values = group_values,
            .column_count = sizeof(group_columns) / sizeof(group_columns[0]),
            .row_count = 1U,
            .context = "sql select limit caps grouped select",
        }
    );
    failures += expect_query_table_result(
        database,
        "SELECT id FROM child WHERE id = 1 UNION ALL SELECT id FROM child WHERE id = 2",
        (struct expected_table_result){
            .columns = union_columns,
            .values = union_values,
            .column_count = sizeof(union_columns) / sizeof(union_columns[0]),
            .row_count = 1U,
            .context = "sql select limit caps compound select",
        }
    );

    failures += execute_statement_ok(database, "SET @@SESSION.sql_select_limit = 0");
    failures += expect_query_table_result(
        database,
        "SHOW SESSION VARIABLES LIKE 'sql_select_limit'",
        (struct expected_table_result){
            .columns = show_variable_columns,
            .values = show_zero_values,
            .column_count = sizeof(show_variable_columns) / sizeof(show_variable_columns[0]),
            .row_count = 1U,
            .context = "show variables reports zero sql select limit",
        }
    );
    failures += expect_query_table_result(
        database,
        "SELECT 1",
        (struct expected_table_result){
            .columns = scalar_one_column,
            .values = NULL,
            .column_count = sizeof(scalar_one_column) / sizeof(scalar_one_column[0]),
            .row_count = 0U,
            .context = "zero sql select limit caps scalar select",
        }
    );
    failures += expect_query_table_result(
        database,
        "SELECT COUNT(*) AS c FROM child",
        (struct expected_table_result){
            .columns = aggregate_columns,
            .values = NULL,
            .column_count = sizeof(aggregate_columns) / sizeof(aggregate_columns[0]),
            .row_count = 0U,
            .context = "zero sql select limit caps aggregate select",
        }
    );

    failures += execute_statement_ok(database, "SET @@sql_select_limit = DEFAULT");
    failures += expect_query_result(
        database,
        "SELECT @@sql_select_limit, @@global.sql_select_limit, "
        "@@session.sql_select_limit, @@local.sql_select_limit, @@warning_count, ROW_COUNT()",
        (struct expected_result){
            .columns = value_columns,
            .values = session_default_values,
            .count = sql_select_limit_value_column_count,
            .context = "default resets sql select limit",
        }
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen sql select limit file");
    failures += expect_query_result(
        database,
        "SELECT @@sql_select_limit, @@global.sql_select_limit",
        (struct expected_result){
            .columns = value_columns,
            .values = value_values,
            .count = 2U,
            .context = "reopened sql select limit values",
        }
    );
    failures += expect_query_table_result(
        database,
        "SELECT id, score FROM app.child ORDER BY id",
        (struct expected_table_result){
            .columns = table_columns,
            .values = table_values,
            .column_count = sizeof(table_columns) / sizeof(table_columns[0]),
            .row_count = 3U,
            .context = "reopened sql select limit table rows",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_sql_select_limit_qualifiers_and_errors(void) {
    static const char *const scoped_columns[] = {
        "@@SQL_SELECT_LIMIT",
        "@@SESSION.SQL_SELECT_LIMIT",
        "@@Local.Sql_Select_Limit",
        "@@global.`sql_select_limit`",
        "(@@sql_select_limit)",
    };
    static const char *const scoped_values[] = {
        "18446744073709551615",
        "18446744073709551615",
        "18446744073709551615",
        "18446744073709551615",
        "18446744073709551615",
    };
    static const char *const show_variable_columns[] = {"Variable_name", "Value"};
    static const char *const show_zero_values[] = {"sql_select_limit", "0"};
    static const char *const show_one_values[] = {"sql_select_limit", "1"};
    static const char *const show_two_values[] = {"sql_select_limit", "2"};
    static const char *const show_default_values[] = {
        "sql_select_limit",
        "18446744073709551615",
    };
    const struct expected_sql_error incorrect_argument = {
        .code = mysql_error_incorrect_argument_type,
        .sqlstate = "42000",
        .message_part = "Incorrect argument type to variable 'sql_select_limit'",
    };
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures +=
        expect_int(mylite_open_memory(&database), MYLITE_OK, "open sql select limit memory");
    failures += execute_ok(
        database,
        "SELECT @@SQL_SELECT_LIMIT, @@SESSION.SQL_SELECT_LIMIT, "
        "@@Local.Sql_Select_Limit, @@global.`sql_select_limit`, "
        "(@@sql_select_limit)",
        &result
    );
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = scoped_columns,
            .values = scoped_values,
            .count = sql_select_limit_label_column_count,
            .context = "sql select limit qualifiers",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_error(
        database,
        "SELECT @@no_such_sql_select_limit_variable",
        (struct expected_sql_error){
            .code = mysql_error_unknown_system_variable,
            .sqlstate = "HY000",
            .message_part = "Unknown system variable 'no_such_sql_select_limit_variable'",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@global.no_such_sql_select_limit_variable",
        (struct expected_sql_error){
            .code = mysql_error_unknown_system_variable,
            .sqlstate = "HY000",
            .message_part = "Unknown system variable 'no_such_sql_select_limit_variable'",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@session.no_such_sql_select_limit_variable",
        (struct expected_sql_error){
            .code = mysql_error_unknown_system_variable,
            .sqlstate = "HY000",
            .message_part = "Unknown system variable 'no_such_sql_select_limit_variable'",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@`session`.sql_select_limit",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "quoted system variable scope",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@sql_select_limit + 0",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "signed 64-bit +, binary -, and * arithmetic",
        }
    );

    failures += execute_statement_ok(database, "SET @@SESSION.sql_select_limit = +2");
    failures += expect_query_table_result(
        database,
        "SHOW SESSION VARIABLES LIKE 'sql_select_limit'",
        (struct expected_table_result){
            .columns = show_variable_columns,
            .values = show_two_values,
            .column_count = sizeof(show_variable_columns) / sizeof(show_variable_columns[0]),
            .row_count = 1U,
            .context = "session-qualified sql select limit assignment",
        }
    );
    failures += execute_statement_ok(database, "SET LOCAL sql_select_limit = TRUE");
    failures += expect_query_table_result(
        database,
        "SHOW SESSION VARIABLES LIKE 'sql_select_limit'",
        (struct expected_table_result){
            .columns = show_variable_columns,
            .values = show_one_values,
            .column_count = sizeof(show_variable_columns) / sizeof(show_variable_columns[0]),
            .row_count = 1U,
            .context = "local sql select limit boolean assignment",
        }
    );
    failures += execute_ok(database, "SET SESSION sql_select_limit = -1", &result);
    failures += expect_size(mylite_result_column_count(result), 0U, "negative sql select limit");
    failures += expect_size(mylite_result_row_count(result), 0U, "negative sql select limit");
    failures +=
        expect_size(mylite_result_warning_count(result), 1U, "negative sql select limit warning");
    mylite_result_free(result);
    result = NULL;
    failures +=
        expect_show_count_warnings(database, "1", "negative sql select limit warning count");
    failures += expect_query_table_result(
        database,
        "SHOW SESSION VARIABLES LIKE 'sql_select_limit'",
        (struct expected_table_result){
            .columns = show_variable_columns,
            .values = show_zero_values,
            .column_count = sizeof(show_variable_columns) / sizeof(show_variable_columns[0]),
            .row_count = 1U,
            .context = "negative sql select limit clamps to zero",
        }
    );

    failures += execute_statement_ok(database, "SET SESSION sql_select_limit = DEFAULT");
    failures += expect_query_table_result(
        database,
        "SHOW SESSION VARIABLES LIKE 'sql_select_limit'",
        (struct expected_table_result){
            .columns = show_variable_columns,
            .values = show_default_values,
            .column_count = sizeof(show_variable_columns) / sizeof(show_variable_columns[0]),
            .row_count = 1U,
            .context = "sql select limit default assignment",
        }
    );
    failures += execute_error(database, "SET SESSION sql_select_limit = '2'", incorrect_argument);
    failures += execute_error(database, "SET SESSION sql_select_limit = 1.5", incorrect_argument);
    failures += execute_error(database, "SET SESSION sql_select_limit = NULL", incorrect_argument);
    failures += execute_error(database, "SET SESSION sql_select_limit = ON", incorrect_argument);
    failures += execute_error(
        database,
        "SET SESSION sql_select_limit = 18446744073709551616",
        incorrect_argument
    );

    failures += execute_statement_ok(database, "SET SESSION sql_select_limit = 2");
    failures += execute_error(
        database,
        "SET sql_select_limit = 1, sql_select_limit = 'bad'",
        incorrect_argument
    );
    failures += expect_query_table_result(
        database,
        "SHOW SESSION VARIABLES LIKE 'sql_select_limit'",
        (struct expected_table_result){
            .columns = show_variable_columns,
            .values = show_two_values,
            .column_count = sizeof(show_variable_columns) / sizeof(show_variable_columns[0]),
            .row_count = 1U,
            .context = "sql select limit rollback after multi-assignment failure",
        }
    );
    failures += execute_statement_ok(database, "SET @limit_value = 1");
    failures += execute_statement_ok(database, "SET sql_select_limit = @limit_value");
    failures += expect_query_table_result(
        database,
        "SHOW SESSION VARIABLES LIKE 'sql_select_limit'",
        (struct expected_table_result){
            .columns = show_variable_columns,
            .values = show_one_values,
            .column_count = sizeof(show_variable_columns) / sizeof(show_variable_columns[0]),
            .row_count = 1U,
            .context = "sql select limit integer user variable assignment",
        }
    );
    failures += execute_statement_ok(database, "SET @limit_text = '2'");
    failures += execute_error(database, "SET sql_select_limit = @limit_text", incorrect_argument);
    failures += execute_error(
        database,
        "SET GLOBAL sql_select_limit = 7",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SET GLOBAL sql_select_limit assignment is not supported",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_independent_sql_select_limit_handles(void) {
    static const char *const columns[] = {
        "@@sql_select_limit",
        "@@warning_count",
        "@@error_count",
    };
    static const char *const first_values[] = {"1", "0", "0"};
    static const char *const second_values[] = {"18446744073709551615", "0", "0"};
    static const char *const table_columns[] = {"id"};
    static const char *const first_table_values[] = {"1"};
    static const char *const second_table_values[] = {"1", "2"};
    char first_path[test_path_capacity];
    char second_path[test_path_capacity];
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(first_path, sizeof(first_path), "first") != 0 ||
        make_test_path(second_path, sizeof(second_path), "second") != 0) {
        return 1;
    }
    remove_related_files(first_path);
    remove_related_files(second_path);

    failures += expect_int(mylite_open(first_path, &first), MYLITE_OK, "open first handle");
    failures += expect_int(mylite_open(second_path, &second), MYLITE_OK, "open second handle");
    failures += execute_statement_ok(first, "CREATE DATABASE app");
    failures += execute_statement_ok(first, "USE app");
    failures += execute_statement_ok(second, "CREATE DATABASE app");
    failures += execute_statement_ok(second, "USE app");
    failures += execute_statement_ok(first, "CREATE TABLE t (id INT)");
    failures += execute_statement_ok(first, "INSERT INTO t (id) VALUES (1),(2)");
    failures += execute_statement_ok(second, "CREATE TABLE t (id INT)");
    failures += execute_statement_ok(second, "INSERT INTO t (id) VALUES (1),(2)");
    failures += execute_statement_ok(first, "SET sql_select_limit = 1");

    failures +=
        execute_ok(first, "SELECT @@sql_select_limit, @@warning_count, @@error_count", &result);
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = columns,
            .values = first_values,
            .count = sql_select_limit_independent_column_count,
            .context = "first handle sql select limit variables",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_table_result(
        first,
        "SELECT id FROM t ORDER BY id",
        (struct expected_table_result){
            .columns = table_columns,
            .values = first_table_values,
            .column_count = sizeof(table_columns) / sizeof(table_columns[0]),
            .row_count = 1U,
            .context = "first handle sql select limit caps rows",
        }
    );

    failures +=
        execute_ok(second, "SELECT @@sql_select_limit, @@warning_count, @@error_count", &result);
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = columns,
            .values = second_values,
            .count = sql_select_limit_independent_column_count,
            .context = "second handle sql select limit variables",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_query_table_result(
        second,
        "SELECT id FROM t ORDER BY id",
        (struct expected_table_result){
            .columns = table_columns,
            .values = second_table_values,
            .column_count = sizeof(table_columns) / sizeof(table_columns[0]),
            .row_count = 2U,
            .context = "second handle sql select limit remains default",
        }
    );

    mylite_close(second);
    mylite_close(first);
    remove_related_files(second_path);
    remove_related_files(first_path);
    return failures;
}

static int expect_result(const mylite_result *result, struct expected_result expected) {
    int failures = 0;

    if (result == NULL) {
        fprintf(stderr, "%s: expected result, got NULL\n", expected.context);
        return 1;
    }

    failures += expect_size(mylite_result_column_count(result), expected.count, expected.context);
    failures += expect_size(mylite_result_row_count(result), 1U, expected.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);
    for (size_t index = 0U; index < expected.count; ++index) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, index),
            expected.columns[index],
            expected.context
        );
        failures += expect_text_or_null(
            mylite_result_value_text(result, 0U, index),
            expected.values[index],
            expected.context
        );
    }

    return failures;
}

static int expect_table_result(const mylite_result *result, struct expected_table_result expected) {
    int failures = 0;

    if (result == NULL) {
        fprintf(stderr, "%s: expected result, got NULL\n", expected.context);
        return 1;
    }

    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);
    for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column_index),
            expected.columns[column_index],
            expected.context
        );
    }
    for (size_t row_index = 0U; row_index < expected.row_count; ++row_index) {
        for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
            const size_t value_index = (row_index * expected.column_count) + column_index;
            failures += expect_text_or_null(
                mylite_result_value_text(result, row_index, column_index),
                expected.values[value_index],
                expected.context
            );
        }
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

static int expect_query_table_result(
    mylite_db *database,
    const char *sql,
    struct expected_table_result expected
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_table_result(result, expected);
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

    failures += expect_size(mylite_result_row_count(result), 1U, context);
    failures += expect_text_or_null(mylite_result_value_text(result, 0U, 0U), expected, context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);

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

    failures += expect_size(mylite_result_row_count(result), 1U, context);
    failures += expect_text_or_null(mylite_result_value_text(result, 0U, 0U), expected, context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);

    mylite_result_free(result);
    return failures;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    mylite_result_free(result);
    return failures;
}

static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures += expect_size(mylite_result_column_count(result), 0U, sql);
    failures += expect_size(mylite_result_row_count(result), 0U, sql);
    failures += expect_int64(mylite_result_affected_rows(result), affected_rows, sql);
    failures += expect_size(mylite_result_warning_count(result), 0U, sql);
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

    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_text_contains(mylite_errmsg(database), expected.message_part, sql);

    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "%s/mylite_sql_select_limit_system_variable_%d_%s.mylite",
        P_tmpdir,
        current_process_id(),
        name
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path is too long\n");
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

static int expect_text_or_null(const char *actual, const char *expected, const char *context) {
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }

    fprintf(
        stderr,
        "%s: expected text [%s], got [%s]\n",
        context,
        expected == NULL ? "(null)" : expected,
        actual == NULL ? "(null)" : actual
    );
    return 1;
}

static int expect_text_contains(const char *actual, const char *needle, const char *context) {
    if (actual != NULL && needle != NULL && strstr(actual, needle) != NULL) {
        return 0;
    }

    fprintf(
        stderr,
        "%s: expected [%s] to contain [%s]\n",
        context,
        actual == NULL ? "(null)" : actual,
        needle == NULL ? "(null)" : needle
    );
    return 1;
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
