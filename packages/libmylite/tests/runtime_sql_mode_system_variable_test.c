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
#  define P_tmpdir "/tmp"
#endif

enum {
    test_path_capacity = 1024,
    path_suffix_capacity = 16,
    sql_mode_value_column_count = 6,
    sql_mode_label_column_count = 5,
    sql_mode_diagnostics_column_count = 4,
    sql_mode_selected_column_count = 2,
    sql_mode_independent_column_count = 3,
    mysql_error_parse = 1064,
    mysql_error_unknown_system_variable = 1193,
    mysql_error_variable_cant_be_set = 1231,
};

static const char default_sql_mode[] =
    "ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,"
    "ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION";

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

static int test_sql_mode_values_and_persistence(void);
static int test_sql_mode_qualifiers_and_errors(void);
static int test_sql_mode_assignment_and_effects(void);
static int test_independent_sql_mode_handles(void);
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

    failures += test_sql_mode_values_and_persistence();
    failures += test_sql_mode_qualifiers_and_errors();
    failures += test_sql_mode_assignment_and_effects();
    failures += test_independent_sql_mode_handles();

    return failures == 0 ? 0 : 1;
}

static int test_sql_mode_values_and_persistence(void) {
    static const char *const value_columns[] = {
        "@@sql_mode",
        "@@global.sql_mode",
        "@@session.sql_mode",
        "@@local.sql_mode",
        "@@warning_count",
        "ROW_COUNT()",
    };
    static const char *const value_values[] = {
        default_sql_mode,
        default_sql_mode,
        default_sql_mode,
        default_sql_mode,
        "0",
        "-1",
    };
    static const char *const label_columns[] = {
        "@@SQL_MODE",
        "@@Global.Sql_Mode",
        "@@session.`sql_mode`",
        "@@`sql_mode`",
        "(@@sql_mode)",
    };
    static const char *const label_values[] = {
        default_sql_mode,
        default_sql_mode,
        default_sql_mode,
        default_sql_mode,
        default_sql_mode,
    };
    static const char *const mixed_columns[] = {
        "@@sql_mode",
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
        default_sql_mode,
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
        "@@sql_mode",
        "@@warning_count",
        "@@error_count",
        "ROW_COUNT()",
    };
    static const char *const warning_values[] = {default_sql_mode, "1", "0", "-1"};
    static const char *const error_values[] = {default_sql_mode, "1", "1", "-1"};
    static const char *const selected_columns[] = {"@@sql_mode", "DATABASE()"};
    static const char *const selected_values[] = {default_sql_mode, "app"};
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

    if (make_test_path(path, sizeof(path), "lifecycle") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open sql mode file");
    session = mylite_connection_session_state(database);
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;
    failures += expect_int64(
        (int64_t)session->sql_mode,
        (int64_t)MYLITE_SESSION_SQL_MODE_DEFAULT_BITS,
        "initial sql mode default bits"
    );
    failures += expect_text_or_null(
        session->sql_mode_text,
        default_sql_mode,
        "initial sql mode default text"
    );
    failures +=
        expect_int((int)session->sql_mode_is_placeholder, 0, "initial sql mode placeholder flag");

    failures += execute_ok(
        database,
        "SELECT @@sql_mode, @@global.sql_mode, @@session.sql_mode, "
        "@@local.sql_mode, @@warning_count, ROW_COUNT() FROM DUAL",
        &result
    );
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = value_columns,
            .values = value_values,
            .count = sql_mode_value_column_count,
            .context = "sql mode values",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "SELECT @@SQL_MODE, @@Global.Sql_Mode, "
        "@@session.`sql_mode`, @@`sql_mode`, (@@sql_mode)",
        &result
    );
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = label_columns,
            .values = label_values,
            .count = sql_mode_label_column_count,
            .context = "sql mode labels",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "SELECT @@sql_mode, @@sql_log_bin, @@foreign_key_checks, "
        "@@unique_checks, @@updatable_views_with_limit, @@sql_auto_is_null, "
        "@@sql_big_selects, @@sql_generate_invisible_primary_key, "
        "@@sql_buffer_result, @@sql_safe_updates, @@sql_select_limit, "
        "@@sql_notes, @@sql_warnings, @@sql_quote_show_create, @@autocommit, "
        "@@default_storage_engine, @@character_set_server, @@version_comment",
        &result
    );
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = mixed_columns,
            .values = mixed_values,
            .count = sizeof(mixed_columns) / sizeof(mixed_columns[0]),
            .context = "mixed sql mode scalar values",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_statement_ok(database, "SHOW PROCESSLIST");
    failures += execute_ok(
        database,
        "SELECT @@sql_mode, @@warning_count, @@error_count, ROW_COUNT()",
        &result
    );
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = diagnostics_columns,
            .values = warning_values,
            .count = sql_mode_diagnostics_column_count,
            .context = "sql mode warning diagnostics",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_show_count_warnings(database, "0", "sql mode clears warnings");

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
        "SELECT @@sql_mode, @@warning_count, @@error_count, ROW_COUNT()",
        &result
    );
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = diagnostics_columns,
            .values = error_values,
            .count = sql_mode_diagnostics_column_count,
            .context = "sql mode error diagnostics",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_show_count_errors(database, "0", "sql mode clears errors");
    failures += expect_show_count_warnings(database, "0", "sql mode clears error warnings");

    session = mylite_connection_session_state(database);
    failures += expect_int64(
        (int64_t)session->sql_mode,
        (int64_t)MYLITE_SESSION_SQL_MODE_DEFAULT_BITS,
        "sql mode default bits unchanged by reads"
    );
    failures += expect_text_or_null(
        session->sql_mode_text,
        default_sql_mode,
        "sql mode default text unchanged by reads"
    );
    failures += expect_int(
        (int)session->sql_mode_is_placeholder,
        0,
        "sql mode placeholder flag unchanged by reads"
    );
    failures += expect_int64(
        (int64_t)session->catalog_generation,
        (int64_t)catalog_generation,
        "catalog generation unchanged by sql mode reads"
    );
    failures += expect_int64(
        (int64_t)session->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "sqlite schema generation unchanged by sql mode reads"
    );
    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read sql mode preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after sql mode reads"
    );

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
        "SELECT @@sql_mode, DATABASE()",
        (struct expected_result){
            .columns = selected_columns,
            .values = selected_values,
            .count = sql_mode_selected_column_count,
            .context = "sql mode with selected database",
        }
    );
    failures += expect_query_result(
        database,
        "SELECT id, score FROM child WHERE id = 2 ORDER BY id",
        (struct expected_result){
            .columns = table_columns,
            .values = table_values,
            .count = sizeof(table_columns) / sizeof(table_columns[0]),
            .context = "sql mode does not alter descriptor select",
        }
    );
    failures += expect_query_result(
        database,
        "SELECT id, score FROM child WHERE id = 2 ORDER BY id LIMIT 1",
        (struct expected_result){
            .columns = table_columns,
            .values = table_values,
            .count = sizeof(table_columns) / sizeof(table_columns[0]),
            .context = "explicit descriptor select limit still applies with sql mode",
        }
    );
    failures += expect_query_result(
        database,
        "SELECT COUNT(*) FROM child WHERE score IS NULL",
        (struct expected_result){
            .columns = null_count_columns,
            .values = null_count_values,
            .count = sizeof(null_count_columns) / sizeof(null_count_columns[0]),
            .context = "sql mode does not reject descriptor count",
        }
    );
    failures += expect_query_result(
        database,
        "SELECT id FROM child WHERE score IS NULL ORDER BY id LIMIT 1",
        (struct expected_result){
            .columns = null_row_columns,
            .values = null_row_values,
            .count = sizeof(null_row_columns) / sizeof(null_row_columns[0]),
            .context = "sql mode does not reject descriptor select",
        }
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen sql mode file");
    failures += expect_query_result(
        database,
        "SELECT @@sql_mode, @@global.sql_mode",
        (struct expected_result){
            .columns = value_columns,
            .values = value_values,
            .count = 2U,
            .context = "reopened sql mode values",
        }
    );
    failures += expect_query_result(
        database,
        "SELECT id, score FROM app.child WHERE id = 2 ORDER BY id",
        (struct expected_result){
            .columns = table_columns,
            .values = table_values,
            .count = sizeof(table_columns) / sizeof(table_columns[0]),
            .context = "reopened sql mode table rows",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_sql_mode_qualifiers_and_errors(void) {
    static const char *const scoped_columns[] = {
        "@@SQL_MODE",
        "@@SESSION.SQL_MODE",
        "@@Local.Sql_Mode",
        "@@global.`sql_mode`",
        "(@@sql_mode)",
    };
    static const char *const scoped_values[] = {
        default_sql_mode,
        default_sql_mode,
        default_sql_mode,
        default_sql_mode,
        default_sql_mode,
    };
    static const char *const scalar_columns[] = {"@@sql_mode", "@@global.sql_mode"};
    static const char *const scalar_values[] = {"ANSI_QUOTES", default_sql_mode};
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open sql mode memory");
    failures += execute_ok(
        database,
        "SELECT @@SQL_MODE, @@SESSION.SQL_MODE, "
        "@@Local.Sql_Mode, @@global.`sql_mode`, "
        "(@@sql_mode)",
        &result
    );
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = scoped_columns,
            .values = scoped_values,
            .count = sql_mode_label_column_count,
            .context = "sql mode qualifiers",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_error(
        database,
        "SELECT @@no_such_sql_mode_variable",
        (struct expected_sql_error){
            .code = mysql_error_unknown_system_variable,
            .sqlstate = "HY000",
            .message_part = "Unknown system variable 'no_such_sql_mode_variable'",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@global.no_such_sql_mode_variable",
        (struct expected_sql_error){
            .code = mysql_error_unknown_system_variable,
            .sqlstate = "HY000",
            .message_part = "Unknown system variable 'no_such_sql_mode_variable'",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@session.no_such_sql_mode_variable",
        (struct expected_sql_error){
            .code = mysql_error_unknown_system_variable,
            .sqlstate = "HY000",
            .message_part = "Unknown system variable 'no_such_sql_mode_variable'",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@`session`.sql_mode",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "quoted system variable scope",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@sql_mode + 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "signed 64-bit +, binary -, and * arithmetic",
        }
    );
    failures += execute_statement_ok(database, "SET SESSION sql_mode = 'ANSI_QUOTES'");
    failures += expect_query_result(
        database,
        "SELECT @@sql_mode, @@global.sql_mode",
        (struct expected_result){
            .columns = scalar_columns,
            .values = scalar_values,
            .count = sizeof(scalar_columns) / sizeof(scalar_columns[0]),
            .context = "session sql mode differs from global after SET",
        }
    );
    failures += execute_statement_ok(database, "SET SESSION sql_mode = DEFAULT");

    mylite_close(database);
    return failures;
}

static int test_sql_mode_assignment_and_effects(void) {
    static const char *const listed_columns[] = {
        "@@sql_mode",
        "@@global.sql_mode",
        "@@warning_count",
        "ROW_COUNT()",
    };
    static const char *const empty_values[] = {"", default_sql_mode, "0", "0"};
    static const char *const strict_values[] = {"STRICT_TRANS_TABLES", default_sql_mode, "1", "0"};
    static const char *const no_zero_date_values[] = {"NO_ZERO_DATE", default_sql_mode, "1", "0"};
    static const char *const no_zero_in_date_values[] = {
        "NO_ZERO_IN_DATE",
        default_sql_mode,
        "1",
        "0",
    };
    static const char *const no_auto_values[] = {
        "NO_AUTO_VALUE_ON_ZERO",
        default_sql_mode,
        "0",
        "0",
    };
    static const char *const no_engine_values[] = {
        "NO_ENGINE_SUBSTITUTION",
        default_sql_mode,
        "0",
        "0",
    };
    static const char *const only_full_group_by_values[] = {
        "ONLY_FULL_GROUP_BY",
        default_sql_mode,
        "0",
        "0",
    };
    static const char *const local_empty_element_values[] = {
        "ANSI_QUOTES,STRICT_TRANS_TABLES",
        default_sql_mode,
        "1",
        "0",
    };
    static const char *const local_values[] = {
        "ANSI_QUOTES",
        default_sql_mode,
        "0",
        "0",
    };
    static const char *const default_values[] = {
        default_sql_mode,
        default_sql_mode,
        "0",
        "0",
    };
    static const char *const reopened_default_values[] = {
        default_sql_mode,
        default_sql_mode,
        "0",
        "-1",
    };
    static const char *const no_backslash_values[] = {
        "NO_BACKSLASH_ESCAPES",
        default_sql_mode,
        "0",
        "0",
    };
    static const char *const show_warning_columns[] = {"Level", "Code", "Message"};
    static const char *const strict_warning_values[] = {
        "Warning",
        "3135",
        "'NO_ZERO_DATE', 'NO_ZERO_IN_DATE' and 'ERROR_FOR_DIVISION_BY_ZERO' sql modes "
        "should be used with strict mode. They will be merged with strict mode in a future "
        "release.",
    };
    static const char *const canonical_columns[] = {"@@sql_mode", "@@warning_count"};
    static const char *const canonical_values[] = {
        "REAL_AS_FLOAT,PIPES_AS_CONCAT,ANSI_QUOTES,IGNORE_SPACE,ONLY_FULL_GROUP_BY,"
        "NO_UNSIGNED_SUBTRACTION,NO_DIR_IN_CREATE,ANSI,NO_AUTO_VALUE_ON_ZERO,"
        "STRICT_TRANS_TABLES,STRICT_ALL_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,"
        "ERROR_FOR_DIVISION_BY_ZERO,TRADITIONAL,NO_ENGINE_SUBSTITUTION",
        "0",
    };
    static const char *const show_variables_columns[] = {"Variable_name", "Value"};
    static const char *const show_session_values[] = {"sql_mode", "NO_ENGINE_SUBSTITUTION"};
    static const char *const show_global_values[] = {"sql_mode", default_sql_mode};
    static const char *const string_value_columns[] = {"v"};
    static const char *const no_backslash_result[] = {"a\\"};
    static const char *const double_string_values[] = {"literal"};
    static const char *const ansi_identifier_columns[] = {"id"};
    static const char *const ansi_identifier_values[] = {"7"};
    static const char *const auto_zero_columns[] = {"id", "v"};
    static const char *const auto_zero_values[] = {"0", "20"};
    static const char *const auto_next_values[] = {"1", "30"};
    static const char *const real_show_columns[] =
        {"Field", "Type", "Null", "Key", "Default", "Extra"};
    static const char *const real_float_values[] = {"r", "float", "YES", "", NULL, ""};
    static const char *const real_double_values[] = {"r", "double", "YES", "", NULL, ""};
    const struct mylite_session_state *session = NULL;
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    uint64_t catalog_generation = 0U;
    uint64_t sqlite_schema_generation = 0U;
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "assignment") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open sql mode SET file");
    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    session = mylite_connection_session_state(database);
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;

    failures += execute_statement_ok(database, "SET sql_mode = ''");
    failures += expect_query_result(
        database,
        "SELECT @@sql_mode, @@global.sql_mode, @@warning_count, ROW_COUNT()",
        (struct expected_result){
            .columns = listed_columns,
            .values = empty_values,
            .count = sizeof(listed_columns) / sizeof(listed_columns[0]),
            .context = "empty sql_mode assignment",
        }
    );

    failures += execute_statement_ok(database, "SET sql_mode = 'STRICT_TRANS_TABLES'");
    failures += expect_query_result(
        database,
        "SELECT @@sql_mode, @@global.sql_mode, @@warning_count, ROW_COUNT()",
        (struct expected_result){
            .columns = listed_columns,
            .values = strict_values,
            .count = sizeof(listed_columns) / sizeof(listed_columns[0]),
            .context = "strict sql_mode assignment",
        }
    );
    failures += execute_statement_ok(database, "SET sql_mode = 'STRICT_TRANS_TABLES'");
    failures += expect_query_result(
        database,
        "SHOW WARNINGS",
        (struct expected_result){
            .columns = show_warning_columns,
            .values = strict_warning_values,
            .count = sizeof(show_warning_columns) / sizeof(show_warning_columns[0]),
            .context = "strict sql_mode warning",
        }
    );

    failures += execute_statement_ok(database, "SET sql_mode = 'NO_ZERO_DATE'");
    failures += expect_query_result(
        database,
        "SELECT @@sql_mode, @@global.sql_mode, @@warning_count, ROW_COUNT()",
        (struct expected_result){
            .columns = listed_columns,
            .values = no_zero_date_values,
            .count = sizeof(listed_columns) / sizeof(listed_columns[0]),
            .context = "NO_ZERO_DATE assignment",
        }
    );
    failures += execute_statement_ok(database, "SET sql_mode = 'NO_ZERO_IN_DATE'");
    failures += expect_query_result(
        database,
        "SELECT @@sql_mode, @@global.sql_mode, @@warning_count, ROW_COUNT()",
        (struct expected_result){
            .columns = listed_columns,
            .values = no_zero_in_date_values,
            .count = sizeof(listed_columns) / sizeof(listed_columns[0]),
            .context = "NO_ZERO_IN_DATE assignment",
        }
    );
    failures += execute_statement_ok(database, "SET sql_mode = 'NO_AUTO_VALUE_ON_ZERO'");
    failures += expect_query_result(
        database,
        "SELECT @@sql_mode, @@global.sql_mode, @@warning_count, ROW_COUNT()",
        (struct expected_result){
            .columns = listed_columns,
            .values = no_auto_values,
            .count = sizeof(listed_columns) / sizeof(listed_columns[0]),
            .context = "NO_AUTO_VALUE_ON_ZERO assignment",
        }
    );
    failures += execute_statement_ok(database, "SET @@sql_mode = \"NO_ENGINE_SUBSTITUTION\"");
    failures += expect_query_result(
        database,
        "SELECT @@sql_mode, @@global.sql_mode, @@warning_count, ROW_COUNT()",
        (struct expected_result){
            .columns = listed_columns,
            .values = no_engine_values,
            .count = sizeof(listed_columns) / sizeof(listed_columns[0]),
            .context = "@@sql_mode double-quoted assignment",
        }
    );
    failures += expect_query_result(
        database,
        "SHOW VARIABLES LIKE 'sql_mode'",
        (struct expected_result){
            .columns = show_variables_columns,
            .values = show_session_values,
            .count = sizeof(show_variables_columns) / sizeof(show_variables_columns[0]),
            .context = "SHOW session sql_mode",
        }
    );
    failures += expect_query_result(
        database,
        "SHOW GLOBAL VARIABLES LIKE 'sql_mode'",
        (struct expected_result){
            .columns = show_variables_columns,
            .values = show_global_values,
            .count = sizeof(show_variables_columns) / sizeof(show_variables_columns[0]),
            .context = "SHOW global sql_mode",
        }
    );

    failures += execute_statement_ok(database, "SET SESSION sql_mode = \"NO_ZERO_DATE\"");
    failures += expect_query_result(
        database,
        "SELECT @@sql_mode, @@global.sql_mode, @@warning_count, ROW_COUNT()",
        (struct expected_result){
            .columns = listed_columns,
            .values = no_zero_date_values,
            .count = sizeof(listed_columns) / sizeof(listed_columns[0]),
            .context = "SESSION double-quoted assignment",
        }
    );
    failures += execute_statement_ok(database, "SET @@SESSION.sql_mode = \"NO_ZERO_IN_DATE\"");
    failures += expect_query_result(
        database,
        "SELECT @@sql_mode, @@global.sql_mode, @@warning_count, ROW_COUNT()",
        (struct expected_result){
            .columns = listed_columns,
            .values = no_zero_in_date_values,
            .count = sizeof(listed_columns) / sizeof(listed_columns[0]),
            .context = "@@SESSION assignment",
        }
    );
    failures += execute_statement_ok(database, "SET @@session.SQL_mode = \"only_full_group_by\"");
    failures += expect_query_result(
        database,
        "SELECT @@sql_mode, @@global.sql_mode, @@warning_count, ROW_COUNT()",
        (struct expected_result){
            .columns = listed_columns,
            .values = only_full_group_by_values,
            .count = sizeof(listed_columns) / sizeof(listed_columns[0]),
            .context = "case-insensitive @@session assignment",
        }
    );
    failures +=
        execute_statement_ok(database, "SET LOCAL sql_mode = ',ansi_quotes,,strict_trans_tables,'");
    failures += expect_query_result(
        database,
        "SELECT @@sql_mode, @@global.sql_mode, @@warning_count, ROW_COUNT()",
        (struct expected_result){
            .columns = listed_columns,
            .values = local_empty_element_values,
            .count = sizeof(listed_columns) / sizeof(listed_columns[0]),
            .context = "LOCAL assignment with empty sql_mode elements",
        }
    );
    failures += execute_statement_ok(database, "SET @@LOCAL.sql_mode = 'ANSI_QUOTES'");
    failures += expect_query_result(
        database,
        "SELECT @@sql_mode, @@global.sql_mode, @@warning_count, ROW_COUNT()",
        (struct expected_result){
            .columns = listed_columns,
            .values = local_values,
            .count = sizeof(listed_columns) / sizeof(listed_columns[0]),
            .context = "@@LOCAL assignment",
        }
    );
    failures += execute_statement_ok(database, "SET @@LOCAL.sql_mode = DEFAULT");
    failures += expect_query_result(
        database,
        "SELECT @@sql_mode, @@global.sql_mode, @@warning_count, ROW_COUNT()",
        (struct expected_result){
            .columns = listed_columns,
            .values = default_values,
            .count = sizeof(listed_columns) / sizeof(listed_columns[0]),
            .context = "@@LOCAL DEFAULT assignment",
        }
    );
    failures += execute_statement_ok(database, "SET SESSION sql_mode = 'NO_ENGINE_SUBSTITUTION'");
    failures += expect_query_result(
        database,
        "SELECT @@sql_mode, @@global.sql_mode, @@warning_count, ROW_COUNT()",
        (struct expected_result){
            .columns = listed_columns,
            .values = no_engine_values,
            .count = sizeof(listed_columns) / sizeof(listed_columns[0]),
            .context = "SESSION assignment before DEFAULT",
        }
    );
    failures += execute_statement_ok(database, "SET SESSION sql_mode = DEFAULT");
    failures += expect_query_result(
        database,
        "SELECT @@sql_mode, @@global.sql_mode, @@warning_count, ROW_COUNT()",
        (struct expected_result){
            .columns = listed_columns,
            .values = default_values,
            .count = sizeof(listed_columns) / sizeof(listed_columns[0]),
            .context = "SESSION DEFAULT assignment",
        }
    );
    failures += execute_statement_ok(database, "SET sql_mode = 'NO_ENGINE_SUBSTITUTION'");
    failures += expect_query_result(
        database,
        "SELECT @@sql_mode, @@global.sql_mode, @@warning_count, ROW_COUNT()",
        (struct expected_result){
            .columns = listed_columns,
            .values = no_engine_values,
            .count = sizeof(listed_columns) / sizeof(listed_columns[0]),
            .context = "unqualified assignment before DEFAULT",
        }
    );
    failures += execute_statement_ok(database, "SET sql_mode = DEFAULT");
    failures += expect_query_result(
        database,
        "SELECT @@sql_mode, @@global.sql_mode, @@warning_count, ROW_COUNT()",
        (struct expected_result){
            .columns = listed_columns,
            .values = default_values,
            .count = sizeof(listed_columns) / sizeof(listed_columns[0]),
            .context = "unqualified DEFAULT assignment",
        }
    );
    session = mylite_connection_session_state(database);
    failures += expect_int64(
        (int64_t)session->catalog_generation,
        (int64_t)catalog_generation,
        "catalog generation unchanged by sql mode SET"
    );
    failures += expect_int64(
        (int64_t)session->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "sqlite schema generation unchanged by sql mode SET"
    );
    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read sql mode SET preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after sql mode SET"
    );
    failures += execute_statement_ok(database, "SET SESSION sql_mode = 'NO_BACKSLASH_ESCAPES'");
    failures += expect_query_result(
        database,
        "SELECT @@sql_mode, @@global.sql_mode, @@warning_count, ROW_COUNT()",
        (struct expected_result){
            .columns = listed_columns,
            .values = no_backslash_values,
            .count = sizeof(listed_columns) / sizeof(listed_columns[0]),
            .context = "NO_BACKSLASH_ESCAPES assignment",
        }
    );
    failures += execute_statement_ok(database, "CREATE TABLE slash_strings (v VARCHAR(10))");
    failures += execute_statement_ok(database, "INSERT INTO slash_strings VALUES ('a\\')");
    failures += expect_query_result(
        database,
        "SELECT v FROM slash_strings",
        (struct expected_result){
            .columns = string_value_columns,
            .values = no_backslash_result,
            .count = sizeof(string_value_columns) / sizeof(string_value_columns[0]),
            .context = "NO_BACKSLASH_ESCAPES string decoding",
        }
    );

    failures += execute_statement_ok(
        database,
        "SET sql_mode = "
        "'ANSI,TRADITIONAL,NO_AUTO_VALUE_ON_ZERO,NO_UNSIGNED_SUBTRACTION,NO_DIR_IN_CREATE'"
    );
    failures += expect_query_result(
        database,
        "SELECT @@sql_mode, @@warning_count",
        (struct expected_result){
            .columns = canonical_columns,
            .values = canonical_values,
            .count = sizeof(canonical_columns) / sizeof(canonical_columns[0]),
            .context = "combination sql_mode canonicalization",
        }
    );
    failures += execute_statement_ok(database, "CREATE TABLE \"ansi_mode_table\" (id INT)");
    failures += execute_statement_ok(database, "INSERT INTO \"ansi_mode_table\" VALUES (7)");
    failures += expect_query_result(
        database,
        "SELECT id FROM \"ansi_mode_table\"",
        (struct expected_result){
            .columns = ansi_identifier_columns,
            .values = ansi_identifier_values,
            .count = sizeof(ansi_identifier_columns) / sizeof(ansi_identifier_columns[0]),
            .context = "ANSI_QUOTES identifier parsing",
        }
    );

    failures += execute_statement_ok(database, "SET sql_mode = ''");
    failures += execute_statement_ok(database, "CREATE TABLE double_quote_strings (v VARCHAR(20))");
    failures +=
        execute_statement_ok(database, "INSERT INTO double_quote_strings VALUES (\"literal\")");
    failures += expect_query_result(
        database,
        "SELECT v FROM double_quote_strings",
        (struct expected_result){
            .columns = string_value_columns,
            .values = double_string_values,
            .count = sizeof(string_value_columns) / sizeof(string_value_columns[0]),
            .context = "double quotes return to string literals",
        }
    );
    failures += execute_statement_ok(database, "CREATE TABLE real_double (r REAL)");
    failures += expect_query_result(
        database,
        "SHOW COLUMNS FROM real_double",
        (struct expected_result){
            .columns = real_show_columns,
            .values = real_double_values,
            .count = sizeof(real_show_columns) / sizeof(real_show_columns[0]),
            .context = "REAL maps to DOUBLE without REAL_AS_FLOAT",
        }
    );
    failures += execute_statement_ok(database, "SET sql_mode = 'REAL_AS_FLOAT'");
    failures += execute_statement_ok(database, "CREATE TABLE real_float (r REAL)");
    failures += expect_query_result(
        database,
        "SHOW COLUMNS FROM real_float",
        (struct expected_result){
            .columns = real_show_columns,
            .values = real_float_values,
            .count = sizeof(real_show_columns) / sizeof(real_show_columns[0]),
            .context = "REAL maps to FLOAT with REAL_AS_FLOAT",
        }
    );

    failures += execute_statement_ok(database, "SET sql_mode = 'NO_AUTO_VALUE_ON_ZERO'");
    failures += execute_statement_ok(
        database,
        "CREATE TABLE auto_zero (id INT AUTO_INCREMENT PRIMARY KEY, v INT)"
    );
    failures += execute_statement_ok(database, "INSERT INTO auto_zero (id, v) VALUES (0, 20)");
    failures += execute_statement_ok(database, "INSERT INTO auto_zero (v) VALUES (30)");
    failures += expect_query_result(
        database,
        "SELECT id, v FROM auto_zero WHERE v = 20",
        (struct expected_result){
            .columns = auto_zero_columns,
            .values = auto_zero_values,
            .count = sizeof(auto_zero_columns) / sizeof(auto_zero_columns[0]),
            .context = "NO_AUTO_VALUE_ON_ZERO stores explicit zero",
        }
    );
    failures += expect_query_result(
        database,
        "SELECT id, v FROM auto_zero WHERE v = 30",
        (struct expected_result){
            .columns = auto_zero_columns,
            .values = auto_next_values,
            .count = sizeof(auto_zero_columns) / sizeof(auto_zero_columns[0]),
            .context = "NO_AUTO_VALUE_ON_ZERO permits later generated values",
        }
    );

    failures += execute_statement_ok(
        database,
        "SET sql_mode = 'STRICT_TRANS_TABLES,PAD_CHAR_TO_FULL_LENGTH'"
    );
    failures += expect_show_count_warnings(database, "2", "strict plus PAD warning count");
    failures += execute_error(
        database,
        "SET sql_mode = 'BOGUS'",
        (struct expected_sql_error){
            .code = mysql_error_variable_cant_be_set,
            .sqlstate = "42000",
            .message_part = "Variable 'sql_mode' can't be set to the value of 'BOGUS'",
        }
    );
    session = mylite_connection_session_state(database);
    failures += expect_text_or_null(
        session->sql_mode_text,
        "STRICT_TRANS_TABLES,PAD_CHAR_TO_FULL_LENGTH",
        "invalid sql_mode leaves previous state"
    );

    failures += execute_statement_ok(database, "SET sql_mode = 'ANSI_QUOTES'");
    mylite_close(database);
    database = NULL;
    failures +=
        expect_int(mylite_open(path, &database), MYLITE_OK, "reopen sql mode assignment file");
    failures += expect_query_result(
        database,
        "SELECT @@sql_mode, @@global.sql_mode, @@warning_count, ROW_COUNT()",
        (struct expected_result){
            .columns = listed_columns,
            .values = reopened_default_values,
            .count = sizeof(listed_columns) / sizeof(listed_columns[0]),
            .context = "reopen resets session sql_mode after SET",
        }
    );
    failures += execute_statement_ok(database, "USE app");
    failures += expect_query_result(
        database,
        "SELECT id, v FROM auto_zero WHERE v = 20",
        (struct expected_result){
            .columns = auto_zero_columns,
            .values = auto_zero_values,
            .count = sizeof(auto_zero_columns) / sizeof(auto_zero_columns[0]),
            .context = "reopen preserves rows after sql_mode SET",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_independent_sql_mode_handles(void) {
    static const char *const columns[] = {
        "@@sql_mode",
        "@@warning_count",
        "@@error_count",
    };
    static const char *const first_values[] = {"ANSI_QUOTES", "0", "0"};
    static const char *const second_values[] = {default_sql_mode, "0", "0"};
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&first), MYLITE_OK, "open first sql mode handle");
    failures += expect_int(mylite_open_memory(&second), MYLITE_OK, "open second sql mode handle");
    failures += execute_statement_ok(first, "SET sql_mode = 'ANSI_QUOTES'");

    failures += execute_ok(first, "SELECT @@sql_mode, @@warning_count, @@error_count", &result);
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = columns,
            .values = first_values,
            .count = sql_mode_independent_column_count,
            .context = "first handle sql mode variables",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(second, "SELECT @@sql_mode, @@warning_count, @@error_count", &result);
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = columns,
            .values = second_values,
            .count = sql_mode_independent_column_count,
            .context = "second handle sql mode variables",
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
        "%s/mylite_sql_mode_system_variable_%d_%s.mylite",
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
