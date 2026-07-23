#include "mylite_test_support.h"

#include <mylite/mylite.h>

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
    mysql_error_incorrect_routine_argument_count = 1318,
    mysql_error_not_supported_yet = 1235,
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

static int test_sys_table_exists_procedure(void);
static int test_sys_execute_prepared_stmt_procedure(void);
static int test_sys_placeholder_procedures(void);
static int open_app_database(
    mylite_db **out_database,
    const char *name,
    char *path,
    size_t path_size
);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query(mylite_db *database, struct expected_query expected);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_sys_table_exists_procedure();
    failures += test_sys_execute_prepared_stmt_procedure();
    failures += test_sys_placeholder_procedures();

    return failures == 0 ? 0 : 1;
}

static int test_sys_table_exists_procedure(void) {
    static const char *const exists_columns[] = {
        "base_value",
        "view_value",
        "temporary_value",
        "missing_value",
        "missing_schema_value",
        "selected_sys_value",
    };
    static const char *const exists_values[] = {
        "BASE TABLE",
        "VIEW",
        "TEMPORARY",
        "",
        "",
        "BASE TABLE",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "table-exists", path, sizeof(path));
    failures += execute_ok(database, "CREATE TABLE base_table(id INT PRIMARY KEY)", NULL);
    failures += execute_ok(database, "CREATE VIEW view_table AS SELECT id FROM base_table", NULL);
    failures += execute_ok(database, "CREATE TEMPORARY TABLE temp_table(id INT PRIMARY KEY)", NULL);
    failures += execute_ok(database, "CALL sys.table_exists('app','base_table',@base)", NULL);
    failures += execute_ok(database, "CALL sys.table_exists('app','view_table',@view)", NULL);
    failures += execute_ok(database, "CALL sys.table_exists('app','temp_table',@temp)", NULL);
    failures += execute_ok(database, "CALL sys.table_exists('app','missing_table',@missing)", NULL);
    failures +=
        execute_ok(database, "CALL sys.table_exists('missing','base_table',@missing_schema)", NULL);
    failures += execute_ok(database, "USE sys", NULL);
    failures += execute_ok(database, "CALL table_exists('app','base_table',@selected_sys)", NULL);
    failures += execute_ok(database, "USE app", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @base AS base_value,"
                   "@view AS view_value,"
                   "@temp AS temporary_value,"
                   "@missing AS missing_value,"
                   "@missing_schema AS missing_schema_value,"
                   "@selected_sys AS selected_sys_value",
            .columns = exists_columns,
            .column_count = sizeof(exists_columns) / sizeof(exists_columns[0]),
            .values = exists_values,
            .row_count = 1U,
            .context = "sys.table_exists results",
        }
    );
    failures += execute_error(
        database,
        "CALL sys.table_exists('app','base_table')",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_routine_argument_count,
            .sqlstate = "42000",
            .message_part =
                "Incorrect number of arguments for PROCEDURE sys.table_exists; expected 3, got 2",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_sys_execute_prepared_stmt_procedure(void) {
    static const char *const prepared_columns[] = {"db", "n"};
    static const char *const prepared_values[] = {"sys", "7"};
    static const char *const current_schema_columns[] = {"db"};
    static const char *const current_schema_values[] = {"app"};
    static const char *const dynamic_columns[] = {"dynamic_value"};
    static const char *const dynamic_values[] = {"BASE TABLE"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "execute-prepared", path, sizeof(path));
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "CALL sys.execute_prepared_stmt('SELECT DATABASE() AS db, 7 AS n')",
            .columns = prepared_columns,
            .column_count = sizeof(prepared_columns) / sizeof(prepared_columns[0]),
            .values = prepared_values,
            .row_count = 1U,
            .context = "sys.execute_prepared_stmt select",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT DATABASE() AS db",
            .columns = current_schema_columns,
            .column_count = sizeof(current_schema_columns) / sizeof(current_schema_columns[0]),
            .values = current_schema_values,
            .row_count = 1U,
            .context = "sys.execute_prepared_stmt schema restore",
        }
    );
    failures += execute_ok(
        database,
        "CALL sys.execute_prepared_stmt('CREATE TABLE app.dynamic_table(id INT)')",
        NULL
    );
    failures += execute_ok(database, "CALL sys.table_exists('app','dynamic_table',@dynamic)", NULL);
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT @dynamic AS dynamic_value",
            .columns = dynamic_columns,
            .column_count = sizeof(dynamic_columns) / sizeof(dynamic_columns[0]),
            .values = dynamic_values,
            .row_count = 1U,
            .context = "sys.execute_prepared_stmt ddl",
        }
    );
    failures += execute_error(
        database,
        "CALL sys.execute_prepared_stmt()",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_routine_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect number of arguments for PROCEDURE "
                            "sys.execute_prepared_stmt; expected 1, got 0",
        }
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_sys_placeholder_procedures(void) {
    static const char *const summary_columns[] = {"summary"};
    static const char *const summary_values[] = {"Enabled 0 consumers"};
    static const char *const empty_columns[] = {"disabled_consumers"};
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    failures += open_app_database(&database, "placeholders", path, sizeof(path));
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "CALL sys.ps_setup_enable_consumer('events_waits_current')",
            .columns = summary_columns,
            .column_count = sizeof(summary_columns) / sizeof(summary_columns[0]),
            .values = summary_values,
            .row_count = 1U,
            .context = "sys ps setup summary",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "CALL sys.ps_setup_show_disabled_consumers()",
            .columns = empty_columns,
            .column_count = sizeof(empty_columns) / sizeof(empty_columns[0]),
            .values = NULL,
            .row_count = 0U,
            .context = "sys ps setup empty consumers",
        }
    );
    failures += execute_error(
        database,
        "CALL sys.ps_setup_show_enabled()",
        (struct expected_sql_error){
            .code = mysql_error_incorrect_routine_argument_count,
            .sqlstate = "42000",
            .message_part = "Incorrect number of arguments for PROCEDURE "
                            "sys.ps_setup_show_enabled; expected 2, got 0",
        }
    );
    failures += execute_error(
        database,
        "CALL sys.diagnostics(1,1,'current')",
        (struct expected_sql_error){
            .code = mysql_error_not_supported_yet,
            .sqlstate = "42000",
            .message_part = "sys.diagnostics() requires live Performance Schema instrumentation",
        }
    );

    mylite_close(database);
    remove_related_files(path);
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

static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
) {
    const char *actual = mylite_result_value_text(result, row, column);

    if (expected == NULL) {
        if (actual != NULL) {
            fprintf(
                stderr,
                "%s: row %zu column %zu expected NULL, got [%s]\n",
                context,
                row,
                column,
                actual
            );
            return 1;
        }
        return 0;
    }
    if (actual == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: row %zu column %zu expected [%s], got [%s]\n",
            context,
            row,
            column,
            expected,
            actual == NULL ? "NULL" : actual
        );
        return 1;
    }
    return 0;
}
