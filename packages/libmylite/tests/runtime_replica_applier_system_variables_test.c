#include <mylite/mylite.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

enum {
    sql_capacity = 2048,
    scalar_column_count = 32,
    show_variable_column_count = 2,
    show_variable_row_count = 32,
    mysql_error_parse = 1064,
    mysql_error_global_variable_scope = 1229,
    mysql_error_session_variable_only = 1238,
};

struct replica_applier_variable {
    const char *name;
    const char *scalar_value;
    const char *show_value;
    const char *exact_set_value;
    const char *bad_set_value;
    bool read_only;
};

struct deprecated_variable {
    const char *name;
    const char *scalar_value;
    const char *set_value;
    const char *message;
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

struct expected_warning {
    const char *message;
    const char *context;
};

static int test_replica_applier_values_show_and_scope(void);
static int test_replica_applier_set_and_diagnostics(void);
static int test_replica_applier_deprecation_warnings(void);
static int expect_query_values(mylite_db *database, struct expected_query query);
static int expect_warning(mylite_db *database, struct expected_warning expected);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
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
static int expect_text_or_null(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

static const struct replica_applier_variable replica_applier_variables[] = {
    {"replica_allow_batching", "1", "ON", "ON", "OFF", false},
    {"replica_checkpoint_group", "512", "512", "512", "1", false},
    {"replica_checkpoint_period", "300", "300", "300", "1", false},
    {"replica_compressed_protocol", "0", "OFF", "OFF", "ON", false},
    {"replica_exec_mode", "STRICT", "STRICT", "STRICT", "IDEMPOTENT", false},
    {"replica_load_tmpdir", "/tmp", "/tmp", NULL, NULL, true},
    {"replica_max_allowed_packet", "1073741824", "1073741824", "1073741824", "1", false},
    {"replica_net_timeout", "60", "60", "60", "1", false},
    {"replica_parallel_type",
     "LOGICAL_CLOCK",
     "LOGICAL_CLOCK",
     "LOGICAL_CLOCK",
     "LOGICAL_CLOCK_BAD",
     false},
    {"replica_parallel_workers", "4", "4", "4", "1", false},
    {"replica_pending_jobs_size_max", "134217728", "134217728", "134217728", "1", false},
    {"replica_preserve_commit_order", "1", "ON", "ON", "OFF", false},
    {"replica_skip_errors", "OFF", "OFF", NULL, NULL, true},
    {"replica_sql_verify_checksum", "1", "ON", "ON", "OFF", false},
    {"replica_transaction_retries", "10", "10", "10", "1", false},
    {"replica_type_conversions", "", "", "''", "ALL_NON_LOSSY", false},
    {"slave_allow_batching", "1", "ON", "ON", "OFF", false},
    {"slave_checkpoint_group", "512", "512", "512", "1", false},
    {"slave_checkpoint_period", "300", "300", "300", "1", false},
    {"slave_compressed_protocol", "0", "OFF", "OFF", "ON", false},
    {"slave_exec_mode", "STRICT", "STRICT", "STRICT", "IDEMPOTENT", false},
    {"slave_load_tmpdir", "/tmp", "/tmp", NULL, NULL, true},
    {"slave_max_allowed_packet", "1073741824", "1073741824", "1073741824", "1", false},
    {"slave_net_timeout", "60", "60", "60", "1", false},
    {"slave_parallel_type",
     "LOGICAL_CLOCK",
     "LOGICAL_CLOCK",
     "LOGICAL_CLOCK",
     "LOGICAL_CLOCK_BAD",
     false},
    {"slave_parallel_workers", "4", "4", "4", "1", false},
    {"slave_pending_jobs_size_max", "134217728", "134217728", "134217728", "1", false},
    {"slave_preserve_commit_order", "1", "ON", "ON", "OFF", false},
    {"slave_skip_errors", "OFF", "OFF", NULL, NULL, true},
    {"slave_sql_verify_checksum", "1", "ON", "ON", "OFF", false},
    {"slave_transaction_retries", "10", "10", "10", "1", false},
    {"slave_type_conversions", "", "", "''", "ALL_NON_LOSSY", false},
};

static const char replica_parallel_type_warning[] =
    "'@@replica_parallel_type' is deprecated and will be removed in a future release.";
static const char slave_allow_batching_warning[] =
    "'@@slave_allow_batching' is deprecated and will be removed in a future release. Please use "
    "replica_allow_batching instead.";
static const char slave_checkpoint_group_warning[] =
    "'@@slave_checkpoint_group' is deprecated and will be removed in a future release. Please "
    "use replica_checkpoint_group instead.";
static const char slave_checkpoint_period_warning[] =
    "'@@slave_checkpoint_period' is deprecated and will be removed in a future release. Please "
    "use replica_checkpoint_period instead.";
static const char slave_compressed_protocol_warning[] =
    "'@@slave_compressed_protocol' is deprecated and will be removed in a future release. Please "
    "use replica_compressed_protocol instead.";
static const char slave_exec_mode_warning[] =
    "'@@slave_exec_mode' is deprecated and will be removed in a future release. Please use "
    "replica_exec_mode instead.";
static const char slave_load_tmpdir_warning[] =
    "'@@slave_load_tmpdir' is deprecated and will be removed in a future release. Please use "
    "replica_load_tmpdir instead.";
static const char slave_max_allowed_packet_warning[] =
    "'@@slave_max_allowed_packet' is deprecated and will be removed in a future release. Please "
    "use replica_max_allowed_packet instead.";
static const char slave_net_timeout_warning[] =
    "'@@slave_net_timeout' is deprecated and will be removed in a future release. Please use "
    "replica_net_timeout instead.";
static const char slave_parallel_type_warning[] =
    "'@@slave_parallel_type' is deprecated and will be removed in a future release. Please use "
    "replica_parallel_type instead.";
static const char slave_parallel_workers_warning[] =
    "'@@slave_parallel_workers' is deprecated and will be removed in a future release. Please "
    "use replica_parallel_workers instead.";
static const char slave_pending_jobs_size_max_warning[] =
    "'@@slave_pending_jobs_size_max' is deprecated and will be removed in a future release. "
    "Please use replica_pending_jobs_size_max instead.";
static const char slave_preserve_commit_order_warning[] =
    "'@@slave_preserve_commit_order' is deprecated and will be removed in a future release. "
    "Please use replica_preserve_commit_order instead.";
static const char slave_skip_errors_warning[] =
    "'@@slave_skip_errors' is deprecated and will be removed in a future release. Please use "
    "replica_skip_errors instead.";
static const char slave_sql_verify_checksum_warning[] =
    "'@@slave_sql_verify_checksum' is deprecated and will be removed in a future release. "
    "Please use replica_sql_verify_checksum instead.";
static const char slave_transaction_retries_warning[] =
    "'@@slave_transaction_retries' is deprecated and will be removed in a future release. "
    "Please use replica_transaction_retries instead.";
static const char slave_type_conversions_warning[] =
    "'@@slave_type_conversions' is deprecated and will be removed in a future release. Please "
    "use replica_type_conversions instead.";

static const struct deprecated_variable scalar_deprecated_variables[] = {
    {"replica_parallel_type", "LOGICAL_CLOCK", "LOGICAL_CLOCK", replica_parallel_type_warning},
    {"slave_allow_batching", "1", "ON", slave_allow_batching_warning},
    {"slave_checkpoint_group", "512", "512", slave_checkpoint_group_warning},
    {"slave_checkpoint_period", "300", "300", slave_checkpoint_period_warning},
    {"slave_compressed_protocol", "0", "OFF", slave_compressed_protocol_warning},
    {"slave_exec_mode", "STRICT", "STRICT", slave_exec_mode_warning},
    {"slave_load_tmpdir", "/tmp", NULL, slave_load_tmpdir_warning},
    {"slave_max_allowed_packet", "1073741824", "1073741824", slave_max_allowed_packet_warning},
    {"slave_net_timeout", "60", "60", slave_net_timeout_warning},
    {"slave_parallel_type", "LOGICAL_CLOCK", "LOGICAL_CLOCK", slave_parallel_type_warning},
    {"slave_parallel_workers", "4", "4", slave_parallel_workers_warning},
    {"slave_pending_jobs_size_max", "134217728", "134217728", slave_pending_jobs_size_max_warning},
    {"slave_preserve_commit_order", "1", "ON", slave_preserve_commit_order_warning},
    {"slave_skip_errors", "OFF", NULL, slave_skip_errors_warning},
    {"slave_sql_verify_checksum", "1", "ON", slave_sql_verify_checksum_warning},
    {"slave_transaction_retries", "10", "10", slave_transaction_retries_warning},
    {"slave_type_conversions", "", "''", slave_type_conversions_warning},
};

static const char replica_applier_in_clause[] =
    "('replica_allow_batching','replica_checkpoint_group','replica_checkpoint_period',"
    "'replica_compressed_protocol','replica_exec_mode','replica_load_tmpdir',"
    "'replica_max_allowed_packet','replica_net_timeout','replica_parallel_type',"
    "'replica_parallel_workers','replica_pending_jobs_size_max','replica_preserve_commit_order',"
    "'replica_skip_errors','replica_sql_verify_checksum','replica_transaction_retries',"
    "'replica_type_conversions','slave_allow_batching','slave_checkpoint_group',"
    "'slave_checkpoint_period','slave_compressed_protocol','slave_exec_mode','slave_load_tmpdir',"
    "'slave_max_allowed_packet','slave_net_timeout','slave_parallel_type',"
    "'slave_parallel_workers','slave_pending_jobs_size_max','slave_preserve_commit_order',"
    "'slave_skip_errors','slave_sql_verify_checksum','slave_transaction_retries',"
    "'slave_type_conversions')";

int main(void) {
    int failures = 0;

    failures += test_replica_applier_values_show_and_scope();
    failures += test_replica_applier_set_and_diagnostics();
    failures += test_replica_applier_deprecation_warnings();

    return failures == 0 ? 0 : 1;
}

static int test_replica_applier_values_show_and_scope(void) {
    static const char *const scalar_values[] = {
        "1",
        "512",
        "300",
        "0",
        "STRICT",
        "/tmp",
        "1073741824",
        "60",
        "LOGICAL_CLOCK",
        "4",
        "134217728",
        "1",
        "OFF",
        "1",
        "10",
        "",
        "1",
        "512",
        "300",
        "0",
        "STRICT",
        "/tmp",
        "1073741824",
        "60",
        "LOGICAL_CLOCK",
        "4",
        "134217728",
        "1",
        "OFF",
        "1",
        "10",
        "",
    };
    static const char *const show_rows[] = {
        "replica_allow_batching",
        "ON",
        "replica_checkpoint_group",
        "512",
        "replica_checkpoint_period",
        "300",
        "replica_compressed_protocol",
        "OFF",
        "replica_exec_mode",
        "STRICT",
        "replica_load_tmpdir",
        "/tmp",
        "replica_max_allowed_packet",
        "1073741824",
        "replica_net_timeout",
        "60",
        "replica_parallel_type",
        "LOGICAL_CLOCK",
        "replica_parallel_workers",
        "4",
        "replica_pending_jobs_size_max",
        "134217728",
        "replica_preserve_commit_order",
        "ON",
        "replica_skip_errors",
        "OFF",
        "replica_sql_verify_checksum",
        "ON",
        "replica_transaction_retries",
        "10",
        "replica_type_conversions",
        "",
        "slave_allow_batching",
        "ON",
        "slave_checkpoint_group",
        "512",
        "slave_checkpoint_period",
        "300",
        "slave_compressed_protocol",
        "OFF",
        "slave_exec_mode",
        "STRICT",
        "slave_load_tmpdir",
        "/tmp",
        "slave_max_allowed_packet",
        "1073741824",
        "slave_net_timeout",
        "60",
        "slave_parallel_type",
        "LOGICAL_CLOCK",
        "slave_parallel_workers",
        "4",
        "slave_pending_jobs_size_max",
        "134217728",
        "slave_preserve_commit_order",
        "ON",
        "slave_skip_errors",
        "OFF",
        "slave_sql_verify_checksum",
        "ON",
        "slave_transaction_retries",
        "10",
        "slave_type_conversions",
        "",
    };
    struct expected_sql_error global_only_read = {
        .code = mysql_error_session_variable_only,
        .sqlstate = "HY000",
        .message_part = "is a GLOBAL variable",
    };
    mylite_db *database = NULL;
    char sql[sql_capacity];
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open replica applier db");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@replica_allow_batching, @@replica_checkpoint_group, "
                   "@@replica_checkpoint_period, @@replica_compressed_protocol, "
                   "@@replica_exec_mode, @@replica_load_tmpdir, @@replica_max_allowed_packet, "
                   "@@replica_net_timeout, @@replica_parallel_type, @@replica_parallel_workers, "
                   "@@replica_pending_jobs_size_max, @@replica_preserve_commit_order, "
                   "@@replica_skip_errors, @@replica_sql_verify_checksum, "
                   "@@replica_transaction_retries, @@replica_type_conversions, "
                   "@@slave_allow_batching, @@slave_checkpoint_group, @@slave_checkpoint_period, "
                   "@@slave_compressed_protocol, @@slave_exec_mode, @@slave_load_tmpdir, "
                   "@@slave_max_allowed_packet, @@slave_net_timeout, @@slave_parallel_type, "
                   "@@slave_parallel_workers, @@slave_pending_jobs_size_max, "
                   "@@slave_preserve_commit_order, @@slave_skip_errors, "
                   "@@slave_sql_verify_checksum, @@slave_transaction_retries, "
                   "@@slave_type_conversions",
            .values = scalar_values,
            .column_count = scalar_column_count,
            .row_count = 1U,
            .context = "replica applier scalar values",
        }
    );

    snprintf(
        sql,
        sizeof(sql),
        "SHOW VARIABLES WHERE Variable_name IN %s",
        replica_applier_in_clause
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = sql,
            .values = show_rows,
            .column_count = show_variable_column_count,
            .row_count = show_variable_row_count,
            .context = "replica applier SHOW VARIABLES rows",
        }
    );
    snprintf(
        sql,
        sizeof(sql),
        "SHOW GLOBAL VARIABLES WHERE Variable_name IN %s",
        replica_applier_in_clause
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = sql,
            .values = show_rows,
            .column_count = show_variable_column_count,
            .row_count = show_variable_row_count,
            .context = "replica applier SHOW GLOBAL VARIABLES rows",
        }
    );
    snprintf(
        sql,
        sizeof(sql),
        "SHOW SESSION VARIABLES WHERE Variable_name IN %s",
        replica_applier_in_clause
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = sql,
            .values = show_rows,
            .column_count = show_variable_column_count,
            .row_count = show_variable_row_count,
            .context = "replica applier SHOW SESSION VARIABLES rows",
        }
    );

    for (size_t index = 0U;
         index < sizeof(replica_applier_variables) / sizeof(replica_applier_variables[0]);
         ++index) {
        snprintf(sql, sizeof(sql), "SELECT @@SESSION.%s", replica_applier_variables[index].name);
        failures += execute_error(database, sql, global_only_read);
        snprintf(sql, sizeof(sql), "SELECT @@LOCAL.%s", replica_applier_variables[index].name);
        failures += execute_error(database, sql, global_only_read);
    }

    mylite_close(database);
    return failures;
}

static int test_replica_applier_set_and_diagnostics(void) {
    struct expected_sql_error global_only_set = {
        .code = mysql_error_global_variable_scope,
        .sqlstate = "HY000",
        .message_part = "is a GLOBAL variable and should be set with SET GLOBAL",
    };
    struct expected_sql_error read_only = {
        .code = mysql_error_session_variable_only,
        .sqlstate = "HY000",
        .message_part = "is a read only variable",
    };
    struct expected_sql_error unsupported_set = {
        .code = mysql_error_parse,
        .sqlstate = "42000",
        .message_part = "fixed no-op",
    };
    struct expected_sql_error unsupported_user_variable_set = {
        .code = mysql_error_parse,
        .sqlstate = "42000",
        .message_part = "replication system variables from user variables",
    };
    mylite_db *database = NULL;
    char sql[sql_capacity];
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open replica applier SET db");
    for (size_t index = 0U;
         index < sizeof(replica_applier_variables) / sizeof(replica_applier_variables[0]);
         ++index) {
        const struct replica_applier_variable *variable = &replica_applier_variables[index];

        snprintf(sql, sizeof(sql), "SET %s = DEFAULT", variable->name);
        failures += execute_error(database, sql, variable->read_only ? read_only : global_only_set);
        snprintf(sql, sizeof(sql), "SET GLOBAL %s = DEFAULT", variable->name);
        if (variable->read_only) {
            failures += execute_error(database, sql, read_only);
            continue;
        }

        failures += execute_statement_ok(database, sql);
        snprintf(sql, sizeof(sql), "SET GLOBAL %s = %s", variable->name, variable->exact_set_value);
        failures += execute_statement_ok(database, sql);
        snprintf(sql, sizeof(sql), "SELECT @@GLOBAL.%s", variable->name);
        failures += expect_query_values(
            database,
            (struct expected_query){
                .sql = sql,
                .values = (const char *[]){variable->scalar_value},
                .column_count = 1U,
                .row_count = 1U,
                .context = "replica applier global no-op value",
            }
        );

        snprintf(sql, sizeof(sql), "SET GLOBAL %s = %s", variable->name, variable->bad_set_value);
        failures += execute_error(database, sql, unsupported_set);
    }

    failures += execute_statement_ok(database, "SET @replica_applier = 512");
    failures += execute_error(
        database,
        "SET GLOBAL replica_checkpoint_group = @replica_applier",
        unsupported_user_variable_set
    );
    failures +=
        execute_error(database, "SET GLOBAL replica_load_tmpdir = @replica_applier", read_only);

    mylite_close(database);
    return failures;
}

static int test_replica_applier_deprecation_warnings(void) {
    mylite_db *database = NULL;
    char sql[sql_capacity];
    int failures = 0;

    failures +=
        expect_int(mylite_open_memory(&database), MYLITE_OK, "open replica applier warning db");
    for (size_t index = 0U;
         index < sizeof(scalar_deprecated_variables) / sizeof(scalar_deprecated_variables[0]);
         ++index) {
        const struct deprecated_variable *variable = &scalar_deprecated_variables[index];

        snprintf(sql, sizeof(sql), "SELECT @@GLOBAL.%s, @@warning_count", variable->name);
        failures += expect_query_values(
            database,
            (struct expected_query){
                .sql = sql,
                .values = (const char *[]){variable->scalar_value, "1"},
                .column_count = 2U,
                .row_count = 1U,
                .context = "replica applier scalar warning count",
            }
        );
        failures += expect_warning(
            database,
            (struct expected_warning){
                .message = variable->message,
                .context = "replica applier scalar warning",
            }
        );
    }

    for (size_t index = 0U;
         index < sizeof(scalar_deprecated_variables) / sizeof(scalar_deprecated_variables[0]);
         ++index) {
        const struct deprecated_variable *variable = &scalar_deprecated_variables[index];

        if (variable->set_value == NULL) {
            continue;
        }

        snprintf(sql, sizeof(sql), "SET GLOBAL %s = DEFAULT", variable->name);
        failures += execute_statement_ok(database, sql);
        failures += expect_warning(
            database,
            (struct expected_warning){
                .message = variable->message,
                .context = "replica applier SET warning",
            }
        );
    }

    mylite_close(database);
    return failures;
}

static int expect_query_values(mylite_db *database, struct expected_query query) {
    mylite_result *result = NULL;
    int failures = 0;

    failures += execute_ok(database, query.sql, &result);
    if (result == NULL) {
        return failures + 1;
    }

    failures += expect_size(mylite_result_column_count(result), query.column_count, query.context);
    failures += expect_size(mylite_result_row_count(result), query.row_count, query.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, query.context);
    for (size_t row = 0U; row < query.row_count; ++row) {
        for (size_t column = 0U; column < query.column_count; ++column) {
            failures += expect_result_value(
                result,
                row,
                column,
                query.values[(row * query.column_count) + column],
                query.context
            );
        }
    }

    mylite_result_free(result);
    return failures;
}

static int expect_warning(mylite_db *database, struct expected_warning expected) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, "SHOW WARNINGS LIMIT 1", &result);

    if (result != NULL) {
        failures += expect_size(mylite_result_column_count(result), 3U, expected.context);
        failures += expect_size(mylite_result_row_count(result), 1U, expected.context);
        failures += expect_text_or_null(
            mylite_result_value_text(result, 0U, 0U),
            "Warning",
            expected.context
        );
        failures +=
            expect_text_or_null(mylite_result_value_text(result, 0U, 1U), "1287", expected.context);
        failures += expect_text_or_null(
            mylite_result_value_text(result, 0U, 2U),
            expected.message,
            expected.context
        );
    } else {
        failures += 1;
    }

    mylite_result_free(result);
    return failures;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        fprintf(stderr, "expected OK for [%s], got %d %s\n", sql, rc, mylite_errmsg(database));
        return 1;
    }
    return 0;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, sql, &result);

    mylite_result_free(result);
    return failures;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);
    int failures = 0;

    mylite_result_free(result);
    if (rc == MYLITE_OK) {
        fprintf(stderr, "expected error for [%s], got OK\n", sql);
        return 1;
    }

    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text_or_null(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
    return failures;
}

static int expect_result_value(
    const mylite_result *result,
    size_t row,
    size_t column,
    const char *expected,
    const char *context
) {
    return expect_text_or_null(mylite_result_value_text(result, row, column), expected, context);
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
    if (actual == NULL || expected == NULL || strcmp(actual, expected) != 0) {
        fprintf(
            stderr,
            "%s: expected [%s], got [%s]\n",
            context,
            expected == NULL ? "NULL" : expected,
            actual == NULL ? "NULL" : actual
        );
        return 1;
    }
    return 0;
}

static int expect_contains(const char *actual, const char *needle, const char *context) {
    if (actual == NULL || needle == NULL || strstr(actual, needle) == NULL) {
        fprintf(
            stderr,
            "%s: expected [%s] to contain [%s]\n",
            context,
            actual == NULL ? "NULL" : actual,
            needle == NULL ? "NULL" : needle
        );
        return 1;
    }
    return 0;
}
