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
    sql_require_primary_key_value_column_count = 6,
    sql_require_primary_key_label_column_count = 5,
    sql_require_primary_key_diagnostics_column_count = 4,
    sql_require_primary_key_selected_column_count = 2,
    sql_require_primary_key_independent_column_count = 3,
    mysql_error_parse = 1064,
    mysql_error_variable_cant_be_set = 1231,
    mysql_error_primary_key_required = 3750,
    mysql_error_unknown_system_variable = 1193,
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

static int test_sql_require_primary_key_values_and_persistence(void);
static int test_sql_require_primary_key_set_and_ddl_enforcement(void);
static int test_sql_require_primary_key_qualifiers_and_errors(void);
static int test_independent_sql_require_primary_key_handles(void);
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

    failures += test_sql_require_primary_key_values_and_persistence();
    failures += test_sql_require_primary_key_set_and_ddl_enforcement();
    failures += test_sql_require_primary_key_qualifiers_and_errors();
    failures += test_independent_sql_require_primary_key_handles();

    return failures == 0 ? 0 : 1;
}

static int test_sql_require_primary_key_values_and_persistence(void) {
    static const char *const value_columns[] = {
        "@@sql_require_primary_key",
        "@@global.sql_require_primary_key",
        "@@session.sql_require_primary_key",
        "@@local.sql_require_primary_key",
        "@@warning_count",
        "ROW_COUNT()",
    };
    static const char *const value_values[] = {"0", "0", "0", "0", "0", "-1"};
    static const char *const label_columns[] = {
        "@@SQL_REQUIRE_PRIMARY_KEY",
        "@@Global.Sql_Require_Primary_Key",
        "@@session.`sql_require_primary_key`",
        "@@`sql_require_primary_key`",
        "(@@sql_require_primary_key)",
    };
    static const char *const label_values[] = {"0", "0", "0", "0", "0"};
    static const char *const mixed_columns[] = {
        "@@sql_require_primary_key",
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
        "0",
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
        "MyLite",
    };
    static const char *const diagnostics_columns[] = {
        "@@sql_require_primary_key",
        "@@warning_count",
        "@@error_count",
        "ROW_COUNT()",
    };
    static const char *const warning_values[] = {"0", "1", "0", "-1"};
    static const char *const error_values[] = {"0", "1", "1", "-1"};
    static const char *const selected_columns[] = {"@@sql_require_primary_key", "DATABASE()"};
    static const char *const selected_values[] = {"0", "app"};
    static const char *const table_columns[] = {"id", "score"};
    static const char *const table_values[] = {"2", "30"};
    static const char *const null_count_columns[] = {"COUNT(*)"};
    static const char *const null_count_values[] = {"2"};
    static const char *const null_count_after_delete_values[] = {"1"};
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

    failures +=
        expect_int(mylite_open(path, &database), MYLITE_OK, "open sql require primary key file");
    session = mylite_connection_session_state(database);
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;

    failures += execute_ok(
        database,
        "SELECT @@sql_require_primary_key, @@global.sql_require_primary_key, "
        "@@session.sql_require_primary_key, "
        "@@local.sql_require_primary_key, @@warning_count, ROW_COUNT() FROM DUAL",
        &result
    );
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = value_columns,
            .values = value_values,
            .count = sql_require_primary_key_value_column_count,
            .context = "sql require primary key values",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "SELECT @@SQL_REQUIRE_PRIMARY_KEY, @@Global.Sql_Require_Primary_Key, "
        "@@session.`sql_require_primary_key`, @@`sql_require_primary_key`, "
        "(@@sql_require_primary_key)",
        &result
    );
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = label_columns,
            .values = label_values,
            .count = sql_require_primary_key_label_column_count,
            .context = "sql require primary key labels",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "SELECT @@sql_require_primary_key, @@sql_log_bin, @@foreign_key_checks, "
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
            .context = "mixed sql require primary key scalar values",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_statement_ok(database, "SHOW PROCESSLIST");
    failures += execute_ok(
        database,
        "SELECT @@sql_require_primary_key, @@warning_count, @@error_count, ROW_COUNT()",
        &result
    );
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = diagnostics_columns,
            .values = warning_values,
            .count = sql_require_primary_key_diagnostics_column_count,
            .context = "sql require primary key warning diagnostics",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures +=
        expect_show_count_warnings(database, "0", "sql require primary key clears warnings");

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
        "SELECT @@sql_require_primary_key, @@warning_count, @@error_count, ROW_COUNT()",
        &result
    );
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = diagnostics_columns,
            .values = error_values,
            .count = sql_require_primary_key_diagnostics_column_count,
            .context = "sql require primary key error diagnostics",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_show_count_errors(database, "0", "sql require primary key clears errors");
    failures +=
        expect_show_count_warnings(database, "0", "sql require primary key clears error warnings");

    session = mylite_connection_session_state(database);
    failures += expect_int64(
        (int64_t)session->catalog_generation,
        (int64_t)catalog_generation,
        "catalog generation unchanged by sql require primary key reads"
    );
    failures += expect_int64(
        (int64_t)session->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "sqlite schema generation unchanged by sql require primary key reads"
    );
    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read sql require primary key preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after sql require primary key reads"
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
        "SELECT @@sql_require_primary_key, DATABASE()",
        (struct expected_result){
            .columns = selected_columns,
            .values = selected_values,
            .count = sql_require_primary_key_selected_column_count,
            .context = "sql require primary key with selected database",
        }
    );
    failures += expect_query_result(
        database,
        "SELECT id, score FROM child WHERE id = 2 ORDER BY id",
        (struct expected_result){
            .columns = table_columns,
            .values = table_values,
            .count = sizeof(table_columns) / sizeof(table_columns[0]),
            .context = "sql require primary key does not alter descriptor select",
        }
    );
    failures += expect_query_result(
        database,
        "SELECT id, score FROM child WHERE id = 2 ORDER BY id LIMIT 1",
        (struct expected_result){
            .columns = table_columns,
            .values = table_values,
            .count = sizeof(table_columns) / sizeof(table_columns[0]),
            .context =
                "explicit descriptor select limit still applies with sql require primary key",
        }
    );
    failures += expect_query_result(
        database,
        "SELECT COUNT(*) FROM child WHERE score IS NULL",
        (struct expected_result){
            .columns = null_count_columns,
            .values = null_count_values,
            .count = sizeof(null_count_columns) / sizeof(null_count_columns[0]),
            .context = "sql require primary key does not reject descriptor count",
        }
    );
    failures += expect_query_result(
        database,
        "SELECT id FROM child WHERE score IS NULL ORDER BY id LIMIT 1",
        (struct expected_result){
            .columns = null_row_columns,
            .values = null_row_values,
            .count = sizeof(null_row_columns) / sizeof(null_row_columns[0]),
            .context = "sql require primary key does not reject descriptor select",
        }
    );
    failures += execute_statement_ok(database, "DELETE FROM child WHERE id = 3");
    failures += expect_query_result(
        database,
        "SELECT COUNT(*) FROM child WHERE score IS NULL",
        (struct expected_result){
            .columns = null_count_columns,
            .values = null_count_after_delete_values,
            .count = sizeof(null_count_columns) / sizeof(null_count_columns[0]),
            .context = "sql require primary key does not reject descriptor delete",
        }
    );

    mylite_close(database);
    database = NULL;

    failures +=
        expect_int(mylite_open(path, &database), MYLITE_OK, "reopen sql require primary key file");
    failures += expect_query_result(
        database,
        "SELECT @@sql_require_primary_key, @@global.sql_require_primary_key",
        (struct expected_result){
            .columns = value_columns,
            .values = value_values,
            .count = 2U,
            .context = "reopened sql require primary key values",
        }
    );
    failures += expect_query_result(
        database,
        "SELECT id, score FROM app.child WHERE id = 2 ORDER BY id",
        (struct expected_result){
            .columns = table_columns,
            .values = table_values,
            .count = sizeof(table_columns) / sizeof(table_columns[0]),
            .context = "reopened sql require primary key table rows",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_sql_require_primary_key_set_and_ddl_enforcement(void) {
    static const char *const value_columns[] = {
        "@@sql_require_primary_key",
        "@@global.sql_require_primary_key",
        "@@session.sql_require_primary_key",
        "@@local.sql_require_primary_key",
        "@@warning_count",
    };
    static const char *const enabled_values[] = {"1", "0", "1", "1", "0"};
    static const char *const disabled_values[] = {"0", "0", "0", "0", "0"};
    static const char *const show_columns[] = {"Variable_name", "Value"};
    static const char *const show_enabled_values[] = {"sql_require_primary_key", "ON"};
    static const char *const show_global_values[] = {"sql_require_primary_key", "OFF"};
    static const struct expected_sql_error primary_key_required = {
        .code = mysql_error_primary_key_required,
        .sqlstate = "HY000",
        .message_part = "without a primary key",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "ddl") != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(
        mylite_open(path, &database),
        MYLITE_OK,
        "open sql require primary key DDL file"
    );
    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");

    failures += execute_statement_ok(database, "SET SESSION sql_require_primary_key = ON");
    failures += expect_query_result(
        database,
        "SELECT @@sql_require_primary_key, @@global.sql_require_primary_key, "
        "@@session.sql_require_primary_key, @@local.sql_require_primary_key, @@warning_count",
        (struct expected_result){
            .columns = value_columns,
            .values = enabled_values,
            .count = sizeof(value_columns) / sizeof(value_columns[0]),
            .context = "sql require primary key enabled values",
        }
    );
    failures += expect_query_result(
        database,
        "SHOW VARIABLES LIKE 'sql_require_primary_key'",
        (struct expected_result){
            .columns = show_columns,
            .values = show_enabled_values,
            .count = sizeof(show_columns) / sizeof(show_columns[0]),
            .context = "sql require primary key SHOW VARIABLES session value",
        }
    );
    failures += expect_query_result(
        database,
        "SHOW GLOBAL VARIABLES LIKE 'sql_require_primary_key'",
        (struct expected_result){
            .columns = show_columns,
            .values = show_global_values,
            .count = sizeof(show_columns) / sizeof(show_columns[0]),
            .context = "sql require primary key SHOW GLOBAL VARIABLES value",
        }
    );

    failures += execute_error(database, "CREATE TABLE no_pk (id INT)", primary_key_required);
    failures += execute_error(
        database,
        "CREATE TABLE unique_only (id INT NOT NULL, UNIQUE KEY u_id(id))",
        primary_key_required
    );
    failures += execute_statement_ok(database, "CREATE TABLE inline_pk (id INT PRIMARY KEY)");
    failures += execute_statement_ok(
        database,
        "CREATE TABLE table_pk (id INT NOT NULL, v INT NOT NULL, PRIMARY KEY(id))"
    );
    failures +=
        execute_error(database, "CREATE TEMPORARY TABLE temp_no_pk (id INT)", primary_key_required);
    failures +=
        execute_statement_ok(database, "CREATE TEMPORARY TABLE temp_pk (id INT, PRIMARY KEY(id))");

    failures += execute_statement_ok(database, "SET @@local.sql_require_primary_key = FALSE");
    failures += expect_query_result(
        database,
        "SELECT @@sql_require_primary_key, @@global.sql_require_primary_key, "
        "@@session.sql_require_primary_key, @@local.sql_require_primary_key, @@warning_count",
        (struct expected_result){
            .columns = value_columns,
            .values = disabled_values,
            .count = sizeof(value_columns) / sizeof(value_columns[0]),
            .context = "sql require primary key disabled values",
        }
    );
    failures += execute_statement_ok(database, "CREATE TABLE existing_no_pk (id INT)");
    failures += execute_statement_ok(database, "CREATE TABLE like_no_pk_source (id INT)");
    failures +=
        execute_statement_ok(database, "CREATE TABLE like_pk_source (id INT PRIMARY KEY, v INT)");
    failures += execute_statement_ok(database, "SET @@sql_require_primary_key = TRUE");
    failures += execute_statement_ok(
        database,
        "CREATE TABLE IF NOT EXISTS existing_no_pk (id INT PRIMARY KEY)"
    );
    failures += execute_error(
        database,
        "CREATE TABLE like_no_pk LIKE like_no_pk_source",
        primary_key_required
    );
    failures += execute_statement_ok(database, "CREATE TABLE like_pk LIKE like_pk_source");
    failures += execute_error(
        database,
        "CREATE TABLE ctas_no_pk AS SELECT id, v FROM like_pk_source",
        primary_key_required
    );
    failures += execute_error(
        database,
        "ALTER TABLE existing_no_pk ADD COLUMN v INT",
        primary_key_required
    );
    failures += execute_error(
        database,
        "ALTER TABLE existing_no_pk ADD KEY k_id(id)",
        primary_key_required
    );
    failures +=
        execute_error(database, "CREATE INDEX k_id ON existing_no_pk(id)", primary_key_required);
    failures += execute_error(
        database,
        "ALTER TABLE existing_no_pk COMMENT = 'blocked'",
        primary_key_required
    );
    failures +=
        execute_statement_ok(database, "ALTER TABLE existing_no_pk RENAME TO renamed_no_pk");
    failures += execute_statement_ok(database, "ALTER TABLE renamed_no_pk ADD PRIMARY KEY(id)");

    failures +=
        execute_error(database, "ALTER TABLE table_pk DROP PRIMARY KEY", primary_key_required);
    failures += execute_ok(database, "SHOW CREATE TABLE table_pk", &result);
    failures += expect_text_contains(
        mylite_result_value_text(result, 0U, 1U),
        "PRIMARY KEY (`id`)",
        "simple DROP PRIMARY KEY rejection keeps primary key"
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_error(
        database,
        "ALTER TABLE table_pk DROP PRIMARY KEY, ADD KEY k_v(v)",
        primary_key_required
    );
    failures += execute_ok(database, "SHOW CREATE TABLE table_pk", &result);
    failures += expect_text_contains(
        mylite_result_value_text(result, 0U, 1U),
        "PRIMARY KEY (`id`)",
        "multi-action DROP PRIMARY KEY rejection rolls back primary key"
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_error(
        database,
        "ALTER TABLE table_pk DROP CONSTRAINT `PRIMARY`",
        primary_key_required
    );
    failures += execute_ok(database, "SHOW CREATE TABLE table_pk", &result);
    failures += expect_text_contains(
        mylite_result_value_text(result, 0U, 1U),
        "PRIMARY KEY (`id`)",
        "DROP CONSTRAINT PRIMARY rejection keeps primary key"
    );
    mylite_result_free(result);
    result = NULL;

    failures +=
        execute_statement_ok(database, "ALTER TABLE table_pk DROP PRIMARY KEY, ADD PRIMARY KEY(v)");
    failures += execute_ok(database, "SHOW CREATE TABLE table_pk", &result);
    failures += expect_text_contains(
        mylite_result_value_text(result, 0U, 1U),
        "PRIMARY KEY (`v`)",
        "multi-action primary key replacement succeeds"
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_statement_ok(
        database,
        "CREATE TABLE ai_pk (id INT NOT NULL AUTO_INCREMENT PRIMARY KEY)"
    );
    failures += execute_error(database, "ALTER TABLE ai_pk DROP PRIMARY KEY", primary_key_required);
    failures +=
        execute_statement_ok(database, "CREATE TABLE drop_pk_column (id INT PRIMARY KEY, v INT)");
    failures +=
        execute_error(database, "ALTER TABLE drop_pk_column DROP COLUMN id", primary_key_required);

    failures += execute_error(
        database,
        "SET SESSION sql_require_primary_key = 2",
        (struct expected_sql_error){
            .code = mysql_error_variable_cant_be_set,
            .sqlstate = "42000",
            .message_part = "can't be set to the value of '2'",
        }
    );
    failures += execute_error(
        database,
        "SET GLOBAL sql_require_primary_key = 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SET GLOBAL sql_require_primary_key assignment is not supported",
        }
    );
    failures += execute_statement_ok(database, "SET GLOBAL sql_require_primary_key = 0");

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_sql_require_primary_key_qualifiers_and_errors(void) {
    static const char *const scoped_columns[] = {
        "@@SQL_REQUIRE_PRIMARY_KEY",
        "@@SESSION.SQL_REQUIRE_PRIMARY_KEY",
        "@@Local.Sql_Require_Primary_Key",
        "@@global.`sql_require_primary_key`",
        "(@@sql_require_primary_key)",
    };
    static const char *const scoped_values[] = {"0", "0", "0", "0", "0"};
    static const char *const scalar_columns[] = {"@@sql_require_primary_key"};
    static const char *const scalar_values[] = {"0"};
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures +=
        expect_int(mylite_open_memory(&database), MYLITE_OK, "open sql require primary key memory");
    failures += execute_ok(
        database,
        "SELECT @@SQL_REQUIRE_PRIMARY_KEY, @@SESSION.SQL_REQUIRE_PRIMARY_KEY, "
        "@@Local.Sql_Require_Primary_Key, @@global.`sql_require_primary_key`, "
        "(@@sql_require_primary_key)",
        &result
    );
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = scoped_columns,
            .values = scoped_values,
            .count = sql_require_primary_key_label_column_count,
            .context = "sql require primary key qualifiers",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_error(
        database,
        "SELECT @@no_such_sql_require_primary_key_variable",
        (struct expected_sql_error){
            .code = mysql_error_unknown_system_variable,
            .sqlstate = "HY000",
            .message_part = "Unknown system variable 'no_such_sql_require_primary_key_variable'",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@global.no_such_sql_require_primary_key_variable",
        (struct expected_sql_error){
            .code = mysql_error_unknown_system_variable,
            .sqlstate = "HY000",
            .message_part = "Unknown system variable 'no_such_sql_require_primary_key_variable'",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@session.no_such_sql_require_primary_key_variable",
        (struct expected_sql_error){
            .code = mysql_error_unknown_system_variable,
            .sqlstate = "HY000",
            .message_part = "Unknown system variable 'no_such_sql_require_primary_key_variable'",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@`session`.sql_require_primary_key",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "quoted system variable scope",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@sql_require_primary_key + 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "signed 64-bit +, binary -, and * arithmetic",
        }
    );
    failures += execute_statement_ok(database, "SET SESSION sql_require_primary_key = 1");
    failures += expect_query_result(
        database,
        "SELECT @@sql_require_primary_key",
        (struct expected_result){
            .columns = scalar_columns,
            .values = (const char *const[]){"1"},
            .count = sizeof(scalar_columns) / sizeof(scalar_columns[0]),
            .context = "sql require primary key changes after supported SET",
        }
    );
    failures += execute_statement_ok(database, "SET SESSION sql_require_primary_key = DEFAULT");
    failures += expect_query_result(
        database,
        "SELECT @@sql_require_primary_key",
        (struct expected_result){
            .columns = scalar_columns,
            .values = scalar_values,
            .count = sizeof(scalar_columns) / sizeof(scalar_columns[0]),
            .context = "sql require primary key resets to default",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_independent_sql_require_primary_key_handles(void) {
    static const char *const columns[] = {
        "@@sql_require_primary_key",
        "@@warning_count",
        "@@error_count",
    };
    static const char *const first_values[] = {"1", "0", "0"};
    static const char *const second_values[] = {"0", "0", "0"};
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += expect_int(
        mylite_open_memory(&first),
        MYLITE_OK,
        "open first sql require primary key handle"
    );
    failures += expect_int(
        mylite_open_memory(&second),
        MYLITE_OK,
        "open second sql require primary key handle"
    );
    failures += execute_statement_ok(first, "SHOW PROCESSLIST");
    failures += execute_statement_ok(first, "SET SESSION sql_require_primary_key = ON");

    failures += execute_ok(
        first,
        "SELECT @@sql_require_primary_key, @@warning_count, @@error_count",
        &result
    );
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = columns,
            .values = first_values,
            .count = sql_require_primary_key_independent_column_count,
            .context = "first handle sql require primary key variables",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        second,
        "SELECT @@sql_require_primary_key, @@warning_count, @@error_count",
        &result
    );
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = columns,
            .values = second_values,
            .count = sql_require_primary_key_independent_column_count,
            .context = "second handle sql require primary key variables",
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
        "%s/mylite_sql_require_primary_key_system_variable_%d_%s.mylite",
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
