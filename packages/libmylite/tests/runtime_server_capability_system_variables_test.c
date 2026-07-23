#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include <stdio.h>
#include <string.h>

enum {
    scalar_column_count = 19,
    show_variable_column_count = 2,
    show_variable_row_count = 8,
    diagnostic_context_capacity = 512,
    mysql_error_session_variable_only = 1238,
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

static int test_server_capability_values_and_show_rows(void);
static int test_server_capability_set_and_diagnostics(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_server_capability_values_and_show_rows();
    failures += test_server_capability_set_and_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_server_capability_values_and_show_rows(void) {
    static const char *const scalar_values[] = {
        "YES", "YES", "YES", "YES", "YES",      "YES",      "YES", "YES", "NO", "NO",
        "YES", "YES", "YES", "YES", "DISABLED", "DISABLED", "0",   "0",   "-1",
    };
    static const char *const show_rows[] = {
        "have_compress",
        "YES",
        "have_dynamic_loading",
        "YES",
        "have_geometry",
        "YES",
        "have_profiling",
        "YES",
        "have_query_cache",
        "NO",
        "have_rtree_keys",
        "YES",
        "have_statement_timeout",
        "YES",
        "have_symlink",
        "DISABLED",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open capability db");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@have_compress, @@GLOBAL.have_compress, "
                   "@@have_dynamic_loading, @@GLOBAL.have_dynamic_loading, "
                   "@@have_geometry, @@GLOBAL.have_geometry, "
                   "@@have_profiling, @@GLOBAL.have_profiling, "
                   "@@have_query_cache, @@GLOBAL.have_query_cache, "
                   "@@have_rtree_keys, @@GLOBAL.have_rtree_keys, "
                   "@@have_statement_timeout, @@GLOBAL.have_statement_timeout, "
                   "@@have_symlink, @@GLOBAL.have_symlink, "
                   "@@warning_count, @@error_count, ROW_COUNT()",
            .values = scalar_values,
            .column_count = scalar_column_count,
            .row_count = 1U,
            .context = "capability scalar values",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW VARIABLES WHERE Variable_name IN "
                   "('have_compress','have_dynamic_loading','have_geometry',"
                   "'have_profiling','have_query_cache','have_rtree_keys',"
                   "'have_statement_timeout','have_symlink')",
            .values = show_rows,
            .column_count = show_variable_column_count,
            .row_count = show_variable_row_count,
            .context = "capability SHOW VARIABLES rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW GLOBAL VARIABLES WHERE Variable_name IN "
                   "('have_compress','have_dynamic_loading','have_geometry',"
                   "'have_profiling','have_query_cache','have_rtree_keys',"
                   "'have_statement_timeout','have_symlink')",
            .values = show_rows,
            .column_count = show_variable_column_count,
            .row_count = show_variable_row_count,
            .context = "capability SHOW GLOBAL VARIABLES rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW SESSION VARIABLES WHERE Variable_name IN "
                   "('have_compress','have_dynamic_loading','have_geometry',"
                   "'have_profiling','have_query_cache','have_rtree_keys',"
                   "'have_statement_timeout','have_symlink')",
            .values = show_rows,
            .column_count = show_variable_column_count,
            .row_count = show_variable_row_count,
            .context = "capability SHOW SESSION VARIABLES rows",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_server_capability_set_and_diagnostics(void) {
    static const char *const global_only_variables[] = {
        "have_compress",
        "have_dynamic_loading",
        "have_geometry",
        "have_profiling",
        "have_query_cache",
        "have_rtree_keys",
        "have_statement_timeout",
        "have_symlink",
    };
    struct expected_sql_error global_only_read = {
        .code = mysql_error_session_variable_only,
        .sqlstate = "HY000",
        .message_part = "is a GLOBAL variable",
    };
    struct expected_sql_error read_only = {
        .code = mysql_error_session_variable_only,
        .sqlstate = "HY000",
        .message_part = "is a read only variable",
    };
    mylite_db *database = NULL;
    char sql[diagnostic_context_capacity];
    int failures = 0;

    failures +=
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open capability SET db");

    for (size_t index = 0U;
         index < sizeof(global_only_variables) / sizeof(global_only_variables[0]);
         ++index) {
        int written =
            snprintf(sql, sizeof(sql), "SELECT @@SESSION.%s", global_only_variables[index]);

        if (written < 0 || (size_t)written >= sizeof(sql)) {
            failures += 1;
        } else {
            failures += execute_error(database, sql, global_only_read);
        }
        written = snprintf(sql, sizeof(sql), "SELECT @@LOCAL.%s", global_only_variables[index]);
        if (written < 0 || (size_t)written >= sizeof(sql)) {
            failures += 1;
        } else {
            failures += execute_error(database, sql, global_only_read);
        }
        written =
            snprintf(sql, sizeof(sql), "SET GLOBAL %s = DEFAULT", global_only_variables[index]);
        if (written < 0 || (size_t)written >= sizeof(sql)) {
            failures += 1;
        } else {
            failures += execute_error(database, sql, read_only);
        }
        written = snprintf(sql, sizeof(sql), "SET %s = DEFAULT", global_only_variables[index]);
        if (written < 0 || (size_t)written >= sizeof(sql)) {
            failures += 1;
        } else {
            failures += execute_error(database, sql, read_only);
        }
    }

    failures += execute_statement_ok(database, "SET @capability_value = 'YES'");
    failures += execute_error(database, "SET GLOBAL have_compress = @capability_value", read_only);

    mylite_close(database);
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

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    failures +=
        mylite_test_expect_size(mylite_result_column_count(result), 0U, "statement column count");
    failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, "statement row count");
    failures +=
        mylite_test_expect_int64(mylite_result_affected_rows(result), 0, "statement affected rows");
    failures +=
        mylite_test_expect_size(mylite_result_warning_count(result), 0U, "statement warning count");
    mylite_result_free(result);
    return failures;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    char context[diagnostic_context_capacity];
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_ERROR) {
        fprintf(stderr, "execute '%s': expected MYLITE_ERROR, got %d\n", sql, rc);
        failures += 1;
    }
    failures += mylite_test_expect_true(result == NULL, "failed execute leaves result null");
    snprintf(context, sizeof(context), "%s error code", sql);
    failures += mylite_test_expect_int(mylite_errcode(database), expected.code, context);
    snprintf(context, sizeof(context), "%s SQLSTATE", sql);
    failures += mylite_test_expect_text(mylite_sqlstate(database), expected.sqlstate, context);
    snprintf(context, sizeof(context), "%s error message", sql);
    failures +=
        mylite_test_expect_contains(mylite_errmsg(database), expected.message_part, context);
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

    return mylite_test_expect_text(actual, expected, context);
}
