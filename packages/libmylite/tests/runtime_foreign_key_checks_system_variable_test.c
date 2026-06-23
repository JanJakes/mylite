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
    foreign_key_value_column_count = 6,
    foreign_key_label_column_count = 5,
    foreign_key_diagnostics_column_count = 4,
    foreign_key_selected_column_count = 2,
    foreign_key_independent_column_count = 3,
    foreign_key_mutation_column_count = 4,
    show_variable_column_count = 2,
    mysql_error_parse = 1064,
    mysql_error_unknown_system_variable = 1193,
    mysql_error_variable_cant_be_set = 1231,
    mysql_error_row_is_referenced = 1451,
    mysql_error_no_referenced_row = 1452,
    mysql_error_cannot_drop_index_needed_foreign_key = 1553,
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

static int test_foreign_key_checks_values_and_persistence(void);
static int test_mutable_foreign_key_checks_set_values(void);
static int test_disabled_foreign_key_checks_dml(void);
static int test_foreign_key_checks_qualifiers_and_errors(void);
static int test_independent_foreign_key_checks_handles(void);
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
static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows);
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

    failures += test_foreign_key_checks_values_and_persistence();
    failures += test_mutable_foreign_key_checks_set_values();
    failures += test_disabled_foreign_key_checks_dml();
    failures += test_foreign_key_checks_qualifiers_and_errors();
    failures += test_independent_foreign_key_checks_handles();

    return failures == 0 ? 0 : 1;
}

static int test_foreign_key_checks_values_and_persistence(void) {
    static const char *const value_columns[] = {
        "@@foreign_key_checks",
        "@@global.foreign_key_checks",
        "@@session.foreign_key_checks",
        "@@local.foreign_key_checks",
        "@@warning_count",
        "ROW_COUNT()",
    };
    static const char *const value_values[] = {"1", "1", "1", "1", "0", "-1"};
    static const char *const label_columns[] = {
        "@@FOREIGN_KEY_CHECKS",
        "@@Global.Foreign_Key_Checks",
        "@@session.`foreign_key_checks`",
        "@@`foreign_key_checks`",
        "(@@foreign_key_checks)",
    };
    static const char *const label_values[] = {"1", "1", "1", "1", "1"};
    static const char *const mixed_columns[] = {
        "@@foreign_key_checks",
        "@@sql_quote_show_create",
        "@@autocommit",
        "@@default_storage_engine",
        "@@character_set_server",
        "@@version_comment",
    };
    static const char *const mixed_values[] =
        {"1", "1", "1", "InnoDB", "utf8mb4", MYLITE_MYSQL_SERVER_VERSION_COMMENT_STRING};
    static const char *const diagnostics_columns[] = {
        "@@foreign_key_checks",
        "@@warning_count",
        "@@error_count",
        "ROW_COUNT()",
    };
    static const char *const warning_values[] = {"1", "1", "0", "-1"};
    static const char *const error_values[] = {"1", "1", "1", "-1"};
    static const char *const selected_columns[] = {"@@foreign_key_checks", "DATABASE()"};
    static const char *const selected_values[] = {"1", "app"};
    static const char *const table_columns[] = {"id"};
    static const char *const table_values[] = {"1"};
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

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open foreign key file");
    session = mylite_connection_session_state(database);
    catalog_generation = session->catalog_generation;
    sqlite_schema_generation = session->sqlite_schema_generation;

    failures += execute_ok(
        database,
        "SELECT @@foreign_key_checks, @@global.foreign_key_checks, "
        "@@session.foreign_key_checks, @@local.foreign_key_checks, @@warning_count, "
        "ROW_COUNT() FROM DUAL",
        &result
    );
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = value_columns,
            .values = value_values,
            .count = foreign_key_value_column_count,
            .context = "foreign key checks values",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "SELECT @@FOREIGN_KEY_CHECKS, @@Global.Foreign_Key_Checks, "
        "@@session.`foreign_key_checks`, @@`foreign_key_checks`, (@@foreign_key_checks)",
        &result
    );
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = label_columns,
            .values = label_values,
            .count = foreign_key_label_column_count,
            .context = "foreign key checks labels",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_ok(
        database,
        "SELECT @@foreign_key_checks, @@sql_quote_show_create, @@autocommit, "
        "@@default_storage_engine, @@character_set_server, @@version_comment",
        &result
    );
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = mixed_columns,
            .values = mixed_values,
            .count = sizeof(mixed_columns) / sizeof(mixed_columns[0]),
            .context = "mixed foreign key checks scalar values",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_statement_ok(database, "SHOW PROCESSLIST");
    failures += execute_ok(
        database,
        "SELECT @@foreign_key_checks, @@warning_count, @@error_count, ROW_COUNT()",
        &result
    );
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = diagnostics_columns,
            .values = warning_values,
            .count = foreign_key_diagnostics_column_count,
            .context = "foreign key checks warning diagnostics",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_show_count_warnings(database, "0", "foreign key checks clears warnings");

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
        "SELECT @@foreign_key_checks, @@warning_count, @@error_count, ROW_COUNT()",
        &result
    );
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = diagnostics_columns,
            .values = error_values,
            .count = foreign_key_diagnostics_column_count,
            .context = "foreign key checks error diagnostics",
        }
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_show_count_errors(database, "0", "foreign key checks clears errors");
    failures +=
        expect_show_count_warnings(database, "0", "foreign key checks clears error warnings");

    session = mylite_connection_session_state(database);
    failures += expect_int64(
        (int64_t)session->catalog_generation,
        (int64_t)catalog_generation,
        "catalog generation unchanged by foreign key checks reads"
    );
    failures += expect_int64(
        (int64_t)session->sqlite_schema_generation,
        (int64_t)sqlite_schema_generation,
        "sqlite schema generation unchanged by foreign key checks reads"
    );
    failures += expect_int(
        read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)),
        0,
        "read foreign key checks preamble"
    );
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "preamble after foreign key checks reads"
    );

    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(database, "CREATE TABLE child (id INT)");
    failures += execute_statement_ok(database, "INSERT INTO child (id) VALUES (1)");
    failures += expect_query_result(
        database,
        "SELECT @@foreign_key_checks, DATABASE()",
        (struct expected_result){
            .columns = selected_columns,
            .values = selected_values,
            .count = foreign_key_selected_column_count,
            .context = "foreign key checks with selected database",
        }
    );
    failures += expect_query_result(
        database,
        "SELECT id FROM child",
        (struct expected_result){
            .columns = table_columns,
            .values = table_values,
            .count = sizeof(table_columns) / sizeof(table_columns[0]),
            .context = "foreign key checks table DDL independence",
        }
    );
    failures += expect_dml_ok(database, "SET foreign_key_checks = 0", 0);

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen foreign key file");
    failures += expect_query_result(
        database,
        "SELECT @@foreign_key_checks, @@global.foreign_key_checks",
        (struct expected_result){
            .columns = value_columns,
            .values = value_values,
            .count = 2U,
            .context = "reopened foreign key checks values",
        }
    );
    failures += expect_query_result(
        database,
        "SELECT id FROM app.child",
        (struct expected_result){
            .columns = table_columns,
            .values = table_values,
            .count = sizeof(table_columns) / sizeof(table_columns[0]),
            .context = "reopened foreign key checks table row",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_mutable_foreign_key_checks_set_values(void) {
    static const char *const mutation_columns[] = {
        "@@foreign_key_checks",
        "@@global.foreign_key_checks",
        "@@warning_count",
        "ROW_COUNT()",
    };
    static const char *const show_columns[] = {"Variable_name", "Value"};
    static const char *const on_values[] = {"1", "1", "0", "0"};
    static const char *const off_values[] = {"0", "1", "0", "0"};
    static const char *const show_off_values[] = {"foreign_key_checks", "OFF"};
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open mutable FK memory");

    failures += expect_dml_ok(database, "SET foreign_key_checks = 0", 0);
    failures += expect_query_result(
        database,
        "SELECT @@foreign_key_checks, @@global.foreign_key_checks, @@warning_count, ROW_COUNT()",
        (struct expected_result){
            .columns = mutation_columns,
            .values = off_values,
            .count = foreign_key_mutation_column_count,
            .context = "SET foreign_key_checks zero",
        }
    );
    failures += expect_dml_ok(database, "SET SESSION foreign_key_checks = 1", 0);
    failures += expect_query_result(
        database,
        "SELECT @@foreign_key_checks, @@global.foreign_key_checks, @@warning_count, ROW_COUNT()",
        (struct expected_result){
            .columns = mutation_columns,
            .values = on_values,
            .count = foreign_key_mutation_column_count,
            .context = "SET SESSION foreign_key_checks one",
        }
    );
    failures += expect_dml_ok(database, "SET @@SESSION.foreign_key_checks = 0", 0);
    failures += expect_query_result(
        database,
        "SELECT @@foreign_key_checks, @@global.foreign_key_checks, @@warning_count, ROW_COUNT()",
        (struct expected_result){
            .columns = mutation_columns,
            .values = off_values,
            .count = foreign_key_mutation_column_count,
            .context = "SET @@session foreign_key_checks zero",
        }
    );
    failures += expect_dml_ok(database, "SET @@LOCAL.`foreign_key_checks` = 1", 0);
    failures += expect_dml_ok(database, "SET foreign_key_checks = OFF", 0);
    failures += expect_dml_ok(database, "SET foreign_key_checks = ON", 0);
    failures += expect_dml_ok(database, "SET foreign_key_checks = FALSE", 0);
    failures += expect_dml_ok(database, "SET foreign_key_checks = TRUE", 0);
    failures += expect_dml_ok(database, "SET foreign_key_checks = +0", 0);
    failures += expect_dml_ok(database, "SET foreign_key_checks = +1", 0);
    failures += expect_dml_ok(database, "SET foreign_key_checks = DEFAULT", 0);
    failures += expect_query_result(
        database,
        "SELECT @@foreign_key_checks, @@global.foreign_key_checks, @@warning_count, ROW_COUNT()",
        (struct expected_result){
            .columns = mutation_columns,
            .values = off_values,
            .count = foreign_key_mutation_column_count,
            .context = "SET foreign_key_checks default",
        }
    );
    failures += expect_query_result(
        database,
        "SHOW VARIABLES WHERE Variable_name = 'foreign_key_checks'",
        (struct expected_result){
            .columns = show_columns,
            .values = show_off_values,
            .count = show_variable_column_count,
            .context = "SHOW VARIABLES foreign_key_checks off",
        }
    );

    failures += execute_error(
        database,
        "SET GLOBAL foreign_key_checks = 0",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SET GLOBAL foreign_key_checks assignment is not supported",
        }
    );
    failures += execute_error(
        database,
        "SET foreign_key_checks = -1",
        (struct expected_sql_error){
            .code = mysql_error_variable_cant_be_set,
            .sqlstate = "42000",
            .message_part = "Variable 'foreign_key_checks' can't be set to the value of '-1'",
        }
    );
    failures += execute_error(
        database,
        "SET foreign_key_checks = 2",
        (struct expected_sql_error){
            .code = mysql_error_variable_cant_be_set,
            .sqlstate = "42000",
            .message_part = "Variable 'foreign_key_checks' can't be set to the value of '2'",
        }
    );
    failures += execute_error(
        database,
        "SET foreign_key_checks = '0'",
        (struct expected_sql_error){
            .code = mysql_error_variable_cant_be_set,
            .sqlstate = "42000",
            .message_part = "Variable 'foreign_key_checks' can't be set to the value of '0'",
        }
    );
    failures += execute_error(
        database,
        "SET foreign_key_checks = NULL",
        (struct expected_sql_error){
            .code = mysql_error_variable_cant_be_set,
            .sqlstate = "42000",
            .message_part = "Variable 'foreign_key_checks' can't be set to the value of 'NULL'",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_disabled_foreign_key_checks_dml(void) {
    static const char *const child_columns[] = {"id", "parent_id"};
    static const char *const orphan_values[] = {"30", "99"};
    static const char *const deleted_parent_child_values[] = {"10", "1"};
    static const char *const updated_parent_child_values[] = {"20", "2"};
    static const char *const ignored_insert_values[] = {"40", "100"};
    static const char *const updated_child_values[] = {"30", "777"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += make_test_path(path, sizeof(path), "disabled_checks");
    remove_related_files(path);
    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open disabled FK file");
    failures += execute_statement_ok(database, "CREATE DATABASE app");
    failures += execute_statement_ok(database, "USE app");
    failures += execute_statement_ok(database, "CREATE TABLE parent (id INT PRIMARY KEY)");
    failures += execute_statement_ok(
        database,
        "CREATE TABLE child (id INT PRIMARY KEY, parent_id INT, "
        "CONSTRAINT fk_child_parent FOREIGN KEY (parent_id) REFERENCES parent (id) "
        "ON DELETE CASCADE ON UPDATE CASCADE)"
    );

    failures += expect_dml_ok(database, "SET foreign_key_checks = 0", 0);
    failures += expect_dml_ok(database, "INSERT INTO parent VALUES (1), (2), (3)", 3);
    failures += expect_dml_ok(database, "INSERT INTO child VALUES (10,1), (20,2), (30,99)", 3);
    failures += expect_query_result(
        database,
        "SELECT id, parent_id FROM child WHERE id = 30",
        (struct expected_result){
            .columns = child_columns,
            .values = orphan_values,
            .count = 2U,
            .context = "disabled checks allow orphan insert",
        }
    );

    failures += expect_dml_ok(database, "DELETE FROM parent WHERE id = 1", 1);
    failures += expect_query_result(
        database,
        "SELECT id, parent_id FROM child WHERE id = 10",
        (struct expected_result){
            .columns = child_columns,
            .values = deleted_parent_child_values,
            .count = 2U,
            .context = "disabled checks skip delete cascade",
        }
    );
    failures += expect_dml_ok(database, "UPDATE parent SET id = 22 WHERE id = 2", 1);
    failures += expect_query_result(
        database,
        "SELECT id, parent_id FROM child WHERE id = 20",
        (struct expected_result){
            .columns = child_columns,
            .values = updated_parent_child_values,
            .count = 2U,
            .context = "disabled checks skip update cascade",
        }
    );
    failures += expect_dml_ok(database, "INSERT IGNORE INTO child VALUES (40,100)", 1);
    failures += expect_query_result(
        database,
        "SELECT id, parent_id FROM child WHERE id = 40",
        (struct expected_result){
            .columns = child_columns,
            .values = ignored_insert_values,
            .count = 2U,
            .context = "disabled checks insert ignore keeps orphan row",
        }
    );
    failures += expect_dml_ok(database, "UPDATE child SET parent_id = 777 WHERE id = 30", 1);
    failures += expect_query_result(
        database,
        "SELECT id, parent_id FROM child WHERE id = 30",
        (struct expected_result){
            .columns = child_columns,
            .values = updated_child_values,
            .count = 2U,
            .context = "disabled checks allow orphan update",
        }
    );

    failures += expect_dml_ok(database, "SET foreign_key_checks = 1", 0);
    failures += execute_error(
        database,
        "INSERT INTO child VALUES (50,200)",
        (struct expected_sql_error){
            .code = mysql_error_no_referenced_row,
            .sqlstate = "23000",
            .message_part = "child row",
        }
    );
    failures += execute_error(
        database,
        "UPDATE child SET parent_id = 888 WHERE id = 30",
        (struct expected_sql_error){
            .code = mysql_error_no_referenced_row,
            .sqlstate = "23000",
            .message_part = "child row",
        }
    );
    failures += expect_dml_ok(database, "SET foreign_key_checks = 0", 0);
    failures += execute_error(
        database,
        "DROP INDEX fk_child_parent ON child",
        (struct expected_sql_error){
            .code = mysql_error_cannot_drop_index_needed_foreign_key,
            .sqlstate = "HY000",
            .message_part = "needed in a foreign key constraint",
        }
    );
    failures += execute_error(
        database,
        "DROP TABLE parent",
        (struct expected_sql_error){
            .code = mysql_error_row_is_referenced,
            .sqlstate = "23000",
            .message_part = "parent row",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_foreign_key_checks_qualifiers_and_errors(void) {
    static const char *const scoped_columns[] = {
        "@@FOREIGN_KEY_CHECKS",
        "@@SESSION.FOREIGN_KEY_CHECKS",
        "@@Local.Foreign_Key_Checks",
        "@@global.`foreign_key_checks`",
        "(@@foreign_key_checks)",
    };
    static const char *const scoped_values[] = {"1", "1", "1", "1", "1"};
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open foreign key memory");
    failures += execute_ok(
        database,
        "SELECT @@FOREIGN_KEY_CHECKS, @@SESSION.FOREIGN_KEY_CHECKS, "
        "@@Local.Foreign_Key_Checks, @@global.`foreign_key_checks`, "
        "(@@foreign_key_checks)",
        &result
    );
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = scoped_columns,
            .values = scoped_values,
            .count = foreign_key_label_column_count,
            .context = "foreign key checks qualifiers",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += execute_error(
        database,
        "SELECT @@no_such_foreign_key_checks_variable",
        (struct expected_sql_error){
            .code = mysql_error_unknown_system_variable,
            .sqlstate = "HY000",
            .message_part = "Unknown system variable 'no_such_foreign_key_checks_variable'",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@global.no_such_foreign_key_checks_variable",
        (struct expected_sql_error){
            .code = mysql_error_unknown_system_variable,
            .sqlstate = "HY000",
            .message_part = "Unknown system variable 'no_such_foreign_key_checks_variable'",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@session.no_such_foreign_key_checks_variable",
        (struct expected_sql_error){
            .code = mysql_error_unknown_system_variable,
            .sqlstate = "HY000",
            .message_part = "Unknown system variable 'no_such_foreign_key_checks_variable'",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@`session`.foreign_key_checks",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "quoted system variable scope",
        }
    );
    failures += execute_statement_ok(database, "SELECT @@foreign_key_checks + 1");

    mylite_close(database);
    return failures;
}

static int test_independent_foreign_key_checks_handles(void) {
    static const char *const columns[] = {
        "@@foreign_key_checks",
        "@@warning_count",
        "@@error_count",
    };
    static const char *const first_values[] = {"1", "1", "0"};
    static const char *const second_values[] = {"1", "0", "0"};
    static const char *const first_disabled_values[] = {"0", "0", "0"};
    static const char *const second_still_enabled_values[] = {"1", "0", "0"};
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&first), MYLITE_OK, "open first foreign key handle");
    failures +=
        expect_int(mylite_open_memory(&second), MYLITE_OK, "open second foreign key handle");
    failures += execute_statement_ok(first, "SHOW PROCESSLIST");

    failures +=
        execute_ok(first, "SELECT @@foreign_key_checks, @@warning_count, @@error_count", &result);
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = columns,
            .values = first_values,
            .count = foreign_key_independent_column_count,
            .context = "first handle foreign key checks variables",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures +=
        execute_ok(second, "SELECT @@foreign_key_checks, @@warning_count, @@error_count", &result);
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = columns,
            .values = second_values,
            .count = foreign_key_independent_column_count,
            .context = "second handle foreign key checks variables",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures += expect_dml_ok(first, "SET foreign_key_checks = 0", 0);
    failures +=
        execute_ok(first, "SELECT @@foreign_key_checks, @@warning_count, @@error_count", &result);
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = columns,
            .values = first_disabled_values,
            .count = foreign_key_independent_column_count,
            .context = "first handle disabled foreign key checks variables",
        }
    );
    mylite_result_free(result);
    result = NULL;

    failures +=
        execute_ok(second, "SELECT @@foreign_key_checks, @@warning_count, @@error_count", &result);
    failures += expect_result(
        result,
        (struct expected_result){
            .columns = columns,
            .values = second_still_enabled_values,
            .count = foreign_key_independent_column_count,
            .context = "second handle remains enabled foreign key checks variables",
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

static int expect_dml_ok(mylite_db *database, const char *sql, int64_t affected_rows) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures == 0) {
        failures += expect_size(mylite_result_column_count(result), 0U, sql);
        failures += expect_size(mylite_result_row_count(result), 0U, sql);
        failures += expect_int64(mylite_result_affected_rows(result), affected_rows, sql);
        failures += expect_size(mylite_result_warning_count(result), 0U, sql);
    }
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
        "%s/mylite_foreign_key_checks_system_variable_%d_%s.mylite",
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
