#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include <stdio.h>
#include <string.h>

enum {
    scalar_global_column_count = 33,
    session_only_column_count = 8,
    show_variable_column_count = 2,
    show_session_row_count = 15,
    show_global_row_count = 11,
    session_set_column_count = 8,
    global_noop_set_column_count = 7,
    diagnostic_context_capacity = 512,
    mysql_error_parse = 1064,
    mysql_error_variable_global_assignment_wrong_scope = 1228,
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

static int test_session_tracking_variable_values_and_show_rows(void);
static int test_session_tracking_variable_set_and_diagnostics(void);
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

    failures += test_session_tracking_variable_values_and_show_rows();
    failures += test_session_tracking_variable_set_and_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_session_tracking_variable_values_and_show_rows(void) {
    static const char *const scalar_global_values[] = {
        "utf8mb4_0900_ai_ci",
        "utf8mb4_0900_ai_ci",
        "utf8mb4_0900_ai_ci",
        "0",
        "0",
        "0",
        "0",
        "0",
        "0",
        "0",
        "0",
        "0",
        "0",
        "0",
        "0",
        "0",
        "0",
        "0",
        "OFF",
        "OFF",
        "OFF",
        "1",
        "1",
        "1",
        "0",
        "0",
        "0",
        "OFF",
        "OFF",
        "OFF",
        "0",
        "0",
        "0",
    };
    static const char *const session_only_values[] = {
        "0",
        "0",
        "FULL",
        "FULL",
        "0",
        "0",
        "ON",
        "ON",
    };
    static const char *const show_session_rows[] = {
        "default_collation_for_utf8mb4",
        "utf8mb4_0900_ai_ci",
        "end_markers_in_json",
        "OFF",
        "keep_files_on_create",
        "OFF",
        "old_alter_table",
        "OFF",
        "print_identified_with_as_hex",
        "OFF",
        "require_row_format",
        "OFF",
        "resultset_metadata",
        "FULL",
        "select_into_disk_sync",
        "OFF",
        "session_track_gtids",
        "OFF",
        "session_track_schema",
        "ON",
        "session_track_state_change",
        "OFF",
        "session_track_transaction_info",
        "OFF",
        "show_create_table_skip_secondary_engine",
        "OFF",
        "show_create_table_verbosity",
        "OFF",
        "use_secondary_engine",
        "ON",
    };
    static const char *const show_global_rows[] = {
        "default_collation_for_utf8mb4",
        "utf8mb4_0900_ai_ci",
        "end_markers_in_json",
        "OFF",
        "keep_files_on_create",
        "OFF",
        "old_alter_table",
        "OFF",
        "print_identified_with_as_hex",
        "OFF",
        "select_into_disk_sync",
        "OFF",
        "session_track_gtids",
        "OFF",
        "session_track_schema",
        "ON",
        "session_track_state_change",
        "OFF",
        "session_track_transaction_info",
        "OFF",
        "show_create_table_verbosity",
        "OFF",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += mylite_test_expect_int(
        mylite_open_memory(&database),
        MYLITE_OK,
        "open session tracking db"
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@default_collation_for_utf8mb4, "
                   "@@GLOBAL.default_collation_for_utf8mb4, "
                   "@@SESSION.default_collation_for_utf8mb4, "
                   "@@end_markers_in_json, @@GLOBAL.end_markers_in_json, "
                   "@@SESSION.end_markers_in_json, "
                   "@@keep_files_on_create, @@GLOBAL.keep_files_on_create, "
                   "@@SESSION.keep_files_on_create, "
                   "@@old_alter_table, @@GLOBAL.old_alter_table, "
                   "@@SESSION.old_alter_table, "
                   "@@print_identified_with_as_hex, "
                   "@@GLOBAL.print_identified_with_as_hex, "
                   "@@SESSION.print_identified_with_as_hex, "
                   "@@select_into_disk_sync, @@GLOBAL.select_into_disk_sync, "
                   "@@SESSION.select_into_disk_sync, "
                   "@@session_track_gtids, @@GLOBAL.session_track_gtids, "
                   "@@SESSION.session_track_gtids, "
                   "@@session_track_schema, @@GLOBAL.session_track_schema, "
                   "@@SESSION.session_track_schema, "
                   "@@session_track_state_change, @@GLOBAL.session_track_state_change, "
                   "@@SESSION.session_track_state_change, "
                   "@@session_track_transaction_info, "
                   "@@GLOBAL.session_track_transaction_info, "
                   "@@SESSION.session_track_transaction_info, "
                   "@@show_create_table_verbosity, @@GLOBAL.show_create_table_verbosity, "
                   "@@SESSION.show_create_table_verbosity",
            .values = scalar_global_values,
            .column_count = scalar_global_column_count,
            .row_count = 1U,
            .context = "session tracking global-capable scalar values",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@require_row_format, @@SESSION.require_row_format, "
                   "@@resultset_metadata, @@SESSION.resultset_metadata, "
                   "@@show_create_table_skip_secondary_engine, "
                   "@@SESSION.show_create_table_skip_secondary_engine, "
                   "@@use_secondary_engine, @@SESSION.use_secondary_engine",
            .values = session_only_values,
            .column_count = session_only_column_count,
            .row_count = 1U,
            .context = "session tracking session-only scalar values",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW VARIABLES WHERE Variable_name IN "
                   "('default_collation_for_utf8mb4','end_markers_in_json',"
                   "'keep_files_on_create','old_alter_table',"
                   "'print_identified_with_as_hex','require_row_format',"
                   "'resultset_metadata','select_into_disk_sync',"
                   "'session_track_gtids','session_track_schema',"
                   "'session_track_state_change','session_track_transaction_info',"
                   "'show_create_table_skip_secondary_engine',"
                   "'show_create_table_verbosity','use_secondary_engine')",
            .values = show_session_rows,
            .column_count = show_variable_column_count,
            .row_count = show_session_row_count,
            .context = "session tracking SHOW VARIABLES rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW GLOBAL VARIABLES WHERE Variable_name IN "
                   "('default_collation_for_utf8mb4','end_markers_in_json',"
                   "'keep_files_on_create','old_alter_table',"
                   "'print_identified_with_as_hex','require_row_format',"
                   "'resultset_metadata','select_into_disk_sync',"
                   "'session_track_gtids','session_track_schema',"
                   "'session_track_state_change','session_track_transaction_info',"
                   "'show_create_table_skip_secondary_engine',"
                   "'show_create_table_verbosity','use_secondary_engine')",
            .values = show_global_rows,
            .column_count = show_variable_column_count,
            .row_count = show_global_row_count,
            .context = "session tracking SHOW GLOBAL VARIABLES rows",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_session_tracking_variable_set_and_diagnostics(void) {
    static const char *const session_set_values[] = {
        "1",
        "FULL",
        "OWN_GTID",
        "STATE",
        "FORCED",
        "0",
        "0",
        "0",
    };
    static const char *const global_noop_values[] = {
        "utf8mb4_0900_ai_ci",
        "0",
        "1",
        "OFF",
        "0",
        "0",
        "0",
    };
    struct expected_sql_error session_only_global_read = {
        .code = mysql_error_session_variable_only,
        .sqlstate = "HY000",
        .message_part = "Variable 'resultset_metadata' is a SESSION variable",
    };
    struct expected_sql_error session_only_global_set = {
        .code = mysql_error_variable_global_assignment_wrong_scope,
        .sqlstate = "HY000",
        .message_part =
            "Variable 'resultset_metadata' is a SESSION variable and can't be used with SET GLOBAL",
    };
    struct expected_sql_error unsupported_global_set = {
        .code = mysql_error_parse,
        .sqlstate = "42000",
        .message_part = "SET supports only fixed no-op system variable assignments",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += mylite_test_expect_int(
        mylite_open_memory(&database),
        MYLITE_OK,
        "open session tracking SET db"
    );

    failures += execute_statement_ok(database, "SET require_row_format = ON");
    failures += execute_statement_ok(database, "SET resultset_metadata = FULL");
    failures += execute_statement_ok(database, "SET session_track_gtids = OWN_GTID");
    failures += execute_statement_ok(database, "SET session_track_transaction_info = STATE");
    failures += execute_statement_ok(database, "SET use_secondary_engine = FORCED");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@require_row_format, @@resultset_metadata, "
                   "@@session_track_gtids, @@session_track_transaction_info, "
                   "@@use_secondary_engine, @@warning_count, @@error_count, ROW_COUNT()",
            .values = session_set_values,
            .column_count = session_set_column_count,
            .row_count = 1U,
            .context = "session tracking SET readback",
        }
    );

    failures +=
        execute_statement_ok(database, "SET GLOBAL default_collation_for_utf8mb4 = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL end_markers_in_json = OFF");
    failures += execute_statement_ok(database, "SET GLOBAL session_track_schema = ON");
    failures += execute_statement_ok(database, "SET GLOBAL session_track_gtids = OFF");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@GLOBAL.default_collation_for_utf8mb4, "
                   "@@GLOBAL.end_markers_in_json, @@GLOBAL.session_track_schema, "
                   "@@GLOBAL.session_track_gtids, @@warning_count, @@error_count, ROW_COUNT()",
            .values = global_noop_values,
            .column_count = global_noop_set_column_count,
            .row_count = 1U,
            .context = "session tracking global no-op SET readback",
        }
    );

    failures +=
        execute_error(database, "SELECT @@GLOBAL.resultset_metadata", session_only_global_read);
    failures +=
        execute_error(database, "SET GLOBAL resultset_metadata = DEFAULT", session_only_global_set);
    failures +=
        execute_error(database, "SET GLOBAL end_markers_in_json = ON", unsupported_global_set);
    failures +=
        execute_error(database, "SET GLOBAL session_track_gtids = ON", unsupported_global_set);

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
