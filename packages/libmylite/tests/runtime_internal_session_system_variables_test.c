#include <mylite/mylite.h>

#include <stdio.h>
#include <string.h>

enum {
    scalar_column_count = 9,
    show_variable_column_count = 2,
    session_show_variable_row_count = 7,
    global_show_variable_row_count = 1,
    mutated_column_count = 6,
    reset_column_count = 5,
    warning_column_count = 3,
    diagnostic_context_capacity = 512,
    mysql_error_session_variable_set_global = 1228,
    mysql_error_session_variable_only = 1238,
    mysql_error_cant_set_value = 1231,
    mysql_error_incorrect_argument_type = 1232,
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
    size_t warning_count;
    const char *context;
};

static int test_internal_session_values_and_show_rows(void);
static int test_internal_session_set_and_diagnostics(void);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_statement_with_warnings(
    mylite_db *database,
    const char *sql,
    size_t warning_count
);
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
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_true(int condition, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

static const char *const session_only_variables[] = {
    "original_commit_timestamp",
    "original_server_version",
    "proxy_user",
    "pseudo_replica_mode",
    "pseudo_slave_mode",
    "transaction_allow_batching",
};

static const char *const set_global_session_variables[] = {
    "original_commit_timestamp",
    "original_server_version",
    "pseudo_replica_mode",
    "pseudo_slave_mode",
    "transaction_allow_batching",
};

int main(void) {
    int failures = 0;

    failures += test_internal_session_values_and_show_rows();
    failures += test_internal_session_set_and_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_internal_session_values_and_show_rows(void) {
    static const char *const scalar_values[] = {
        "36028797018963968",
        "36028797018963968",
        "999999",
        "999999",
        NULL,
        "0",
        "STRICT",
        "STRICT",
        "0",
    };
    static const char *const session_show_rows[] = {
        "original_commit_timestamp",
        "36028797018963968",
        "original_server_version",
        "999999",
        "proxy_user",
        "",
        "pseudo_replica_mode",
        "OFF",
        "pseudo_slave_mode",
        "OFF",
        "rbr_exec_mode",
        "STRICT",
        "transaction_allow_batching",
        "OFF",
    };
    static const char *const global_show_rows[] = {
        "rbr_exec_mode",
        "STRICT",
    };
    struct expected_sql_error session_only = {
        .code = mysql_error_session_variable_only,
        .sqlstate = "HY000",
        .message_part = "is a SESSION variable",
    };
    mylite_db *database = NULL;
    char sql[diagnostic_context_capacity];
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open internal session db");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@original_commit_timestamp, @@SESSION.original_commit_timestamp, "
                   "@@original_server_version, @@LOCAL.original_server_version, "
                   "@@proxy_user, @@pseudo_replica_mode, @@rbr_exec_mode, "
                   "@@GLOBAL.rbr_exec_mode, @@transaction_allow_batching",
            .values = scalar_values,
            .column_count = scalar_column_count,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "internal session scalar values",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW VARIABLES WHERE Variable_name IN "
                   "('original_commit_timestamp','original_server_version','proxy_user',"
                   "'pseudo_replica_mode','pseudo_slave_mode','rbr_exec_mode',"
                   "'transaction_allow_batching')",
            .values = session_show_rows,
            .column_count = show_variable_column_count,
            .row_count = session_show_variable_row_count,
            .warning_count = 0U,
            .context = "internal session SHOW VARIABLES rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW SESSION VARIABLES WHERE Variable_name IN "
                   "('original_commit_timestamp','original_server_version','proxy_user',"
                   "'pseudo_replica_mode','pseudo_slave_mode','rbr_exec_mode',"
                   "'transaction_allow_batching')",
            .values = session_show_rows,
            .column_count = show_variable_column_count,
            .row_count = session_show_variable_row_count,
            .warning_count = 0U,
            .context = "internal session SHOW SESSION VARIABLES rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW GLOBAL VARIABLES WHERE Variable_name IN "
                   "('original_commit_timestamp','original_server_version','proxy_user',"
                   "'pseudo_replica_mode','pseudo_slave_mode','rbr_exec_mode',"
                   "'transaction_allow_batching')",
            .values = global_show_rows,
            .column_count = show_variable_column_count,
            .row_count = global_show_variable_row_count,
            .warning_count = 0U,
            .context = "internal session SHOW GLOBAL VARIABLES rows",
        }
    );

    for (size_t index = 0U;
         index < sizeof(session_only_variables) / sizeof(session_only_variables[0]);
         ++index) {
        int written =
            snprintf(sql, sizeof(sql), "SELECT @@GLOBAL.%s", session_only_variables[index]);

        if (written < 0 || (size_t)written >= sizeof(sql)) {
            failures += 1;
        } else {
            failures += execute_error(database, sql, session_only);
        }
    }

    mylite_close(database);
    return failures;
}

static int test_internal_session_set_and_diagnostics(void) {
    static const char *const mutated_values[] = {
        "0",
        "80000",
        "1",
        "IDEMPOTENT",
        "STRICT",
        "1",
    };
    static const char *const reset_values[] = {
        "36028797018963968",
        "999999",
        "0",
        "STRICT",
        "0",
    };
    struct expected_sql_error session_set_global = {
        .code = mysql_error_session_variable_set_global,
        .sqlstate = "HY000",
        .message_part = "is a SESSION variable and can't be used with SET GLOBAL",
    };
    struct expected_sql_error read_only = {
        .code = mysql_error_session_variable_only,
        .sqlstate = "HY000",
        .message_part = "is a read only variable",
    };
    struct expected_sql_error invalid_value = {
        .code = mysql_error_cant_set_value,
        .sqlstate = "42000",
        .message_part = "can't be set to the value",
    };
    struct expected_sql_error incorrect_type = {
        .code = mysql_error_incorrect_argument_type,
        .sqlstate = "42000",
        .message_part = "Incorrect argument type",
    };
    mylite_db *database = NULL;
    char sql[diagnostic_context_capacity];
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open internal SET db");

    failures += execute_statement_ok(database, "SET SESSION original_commit_timestamp = 0");
    failures += execute_statement_ok(database, "SET SESSION original_server_version = 80000");
    failures += execute_statement_ok(database, "SET SESSION pseudo_replica_mode = ON");
    failures += execute_statement_ok(database, "SET SESSION rbr_exec_mode = IDEMPOTENT");
    failures += execute_statement_ok(database, "SET SESSION transaction_allow_batching = ON");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@original_commit_timestamp, @@original_server_version, "
                   "@@pseudo_replica_mode, @@rbr_exec_mode, @@GLOBAL.rbr_exec_mode, "
                   "@@transaction_allow_batching",
            .values = mutated_values,
            .column_count = mutated_column_count,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "internal session mutated readback",
        }
    );

    failures += execute_statement_ok(database, "SET SESSION original_commit_timestamp = DEFAULT");
    failures += execute_statement_ok(database, "SET SESSION original_server_version = DEFAULT");
    failures += execute_statement_ok(database, "SET SESSION pseudo_replica_mode = DEFAULT");
    failures += execute_statement_ok(database, "SET SESSION rbr_exec_mode = DEFAULT");
    failures += execute_statement_ok(database, "SET SESSION transaction_allow_batching = DEFAULT");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@original_commit_timestamp, @@original_server_version, "
                   "@@pseudo_replica_mode, @@rbr_exec_mode, @@transaction_allow_batching",
            .values = reset_values,
            .column_count = reset_column_count,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "internal session reset readback",
        }
    );

    failures += execute_statement_with_warnings(database, "SET SESSION pseudo_slave_mode = ON", 1U);
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW WARNINGS",
            .values = (const char *[]){"Warning",
                                       "1287",
                                       "'@@pseudo_slave_mode' is deprecated and will be removed "
                                       "in a future release. Please use pseudo_replica_mode "
                                       "instead."},
            .column_count = warning_column_count,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "pseudo_slave_mode SET warning",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){.sql = "SELECT @@pseudo_slave_mode",
                                .values = (const char *[]){"1"},
                                .column_count = 1U,
                                .row_count = 1U,
                                .warning_count = 1U,
                                .context = "pseudo_slave_mode scalar warning"}
    );

    failures += execute_statement_ok(database, "SET @m = 'IDEMPOTENT'");
    failures += execute_statement_ok(database, "SET @b = 0");
    failures += execute_statement_ok(database, "SET rbr_exec_mode = @m");
    failures += execute_statement_ok(database, "SET transaction_allow_batching = @b");
    failures += expect_query_values(
        database,
        (struct expected_query){.sql = "SELECT @@rbr_exec_mode, @@transaction_allow_batching",
                                .values = (const char *[]){"IDEMPOTENT", "0"},
                                .column_count = 2U,
                                .row_count = 1U,
                                .warning_count = 0U,
                                .context = "internal session user-variable SET"}
    );

    for (size_t index = 0U;
         index < sizeof(set_global_session_variables) / sizeof(set_global_session_variables[0]);
         ++index) {
        int written = snprintf(
            sql,
            sizeof(sql),
            "SET GLOBAL %s = DEFAULT",
            set_global_session_variables[index]
        );

        if (written < 0 || (size_t)written >= sizeof(sql)) {
            failures += 1;
        } else {
            failures += execute_error(database, sql, session_set_global);
        }
    }
    failures += execute_error(database, "SET GLOBAL rbr_exec_mode = DEFAULT", session_set_global);
    failures += execute_error(database, "SET SESSION proxy_user = DEFAULT", read_only);
    failures += execute_error(database, "SET GLOBAL proxy_user = DEFAULT", read_only);
    failures += execute_error(database, "SET SESSION rbr_exec_mode = 'bad'", invalid_value);
    failures += execute_error(database, "SET SESSION pseudo_replica_mode = 2", invalid_value);
    failures +=
        execute_error(database, "SET SESSION original_commit_timestamp = 'bad'", incorrect_type);

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

static int execute_statement_with_warnings(
    mylite_db *database,
    const char *sql,
    size_t warning_count
) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    if (failures != 0) {
        mylite_result_free(result);
        return failures;
    }
    failures += expect_size(mylite_result_column_count(result), 0U, "statement column count");
    failures += expect_size(mylite_result_row_count(result), 0U, "statement row count");
    failures += expect_int64(mylite_result_affected_rows(result), 0, "statement affected rows");
    failures +=
        expect_size(mylite_result_warning_count(result), warning_count, "statement warning count");
    mylite_result_free(result);
    return failures;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    return execute_statement_with_warnings(database, sql, 0U);
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
    failures += expect_true(result == NULL, "failed execute leaves result null");
    snprintf(context, sizeof(context), "%s error code", sql);
    failures += expect_int(mylite_errcode(database), expected.code, context);
    snprintf(context, sizeof(context), "%s SQLSTATE", sql);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, context);
    snprintf(context, sizeof(context), "%s error message", sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, context);
    mylite_result_free(result);

    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    if (failures != 0) {
        mylite_result_free(result);
        return failures;
    }
    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    for (size_t row = 0U; row < query.row_count; ++row) {
        for (size_t column = 0U; column < query.column_count; ++column) {
            size_t value_index = (row * query.column_count) + column;

            failures +=
                expect_result_value(result, row, column, query.values[value_index], query.context);
        }
    }
    failures += expect_int64(mylite_result_affected_rows(result), 0, query.context);
    failures +=
        expect_size(mylite_result_warning_count(result), query.warning_count, query.context);
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

    return expect_text(actual, expected, context);
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
    return 1;
}

static int expect_int64(int64_t actual, int64_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected %lld, got %lld\n",
        context,
        (long long)expected,
        (long long)actual
    );
    return 1;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
    return 1;
}

static int expect_true(int condition, const char *context) {
    if (condition) {
        return 0;
    }
    fprintf(stderr, "%s: expected true\n", context);
    return 1;
}

static int expect_text(const char *actual, const char *expected, const char *context) {
    if (actual != NULL && expected != NULL && strcmp(actual, expected) == 0) {
        return 0;
    }
    if (actual == NULL && expected == NULL) {
        return 0;
    }
    fprintf(
        stderr,
        "%s: expected [%s], got [%s]\n",
        context,
        expected == NULL ? "(null)" : expected,
        actual == NULL ? "(null)" : actual
    );
    return 1;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
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
