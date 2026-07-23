#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include <stdio.h>
#include <string.h>

enum {
    show_variable_column_count = 2,
    default_scalar_column_count = 25,
    mutated_scalar_column_count = 21,
    deprecated_scalar_column_count = 5,
    show_variable_row_count = 26,
    warning_column_count = 3,
    diagnostic_context_capacity = 512,
    mysql_error_global_variable_set_global = 1229,
    mysql_error_session_variable_only = 1238,
    mysql_error_cant_set_value = 1231,
    mysql_error_incorrect_argument_type = 1232,
    mysql_error_parse = 1064,
    mysql_error_session_variable_read_only = 1621,
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

static int test_resource_tuning_defaults_and_show_rows(void);
static int test_resource_tuning_set_and_warnings(void);
static int test_resource_tuning_diagnostics_and_isolation(void);
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
static const char session_track_default[] =
    "time_zone,autocommit,character_set_client,character_set_results,"
    "character_set_connection";

static const char *const resource_tuning_show_rows[] = {
    "lc_time_names",
    "en_US",
    "net_buffer_length",
    "16384",
    "preload_buffer_size",
    "32768",
    "profiling",
    "OFF",
    "profiling_history_size",
    "15",
    "query_alloc_block_size",
    "8192",
    "query_prealloc_size",
    "8192",
    "range_alloc_block_size",
    "4096",
    "range_optimizer_max_mem_size",
    "8388608",
    "read_buffer_size",
    "131072",
    "read_rnd_buffer_size",
    "262144",
    "regexp_stack_limit",
    "8000000",
    "regexp_time_limit",
    "32",
    "restrict_fk_on_non_standard_key",
    "ON",
    "secondary_engine_cost_threshold",
    "100000.000000",
    "select_into_buffer_size",
    "131072",
    "select_into_disk_sync_delay",
    "0",
    "session_track_system_variables",
    session_track_default,
    "set_operations_buffer_size",
    "262144",
    "show_gipk_in_create_table_and_information_schema",
    "ON",
    "terminology_use_previous",
    "NONE",
    "tmp_table_size",
    "16777216",
    "transaction_alloc_block_size",
    "8192",
    "transaction_prealloc_size",
    "4096",
    "windowing_use_high_precision",
    "ON",
    "xa_detach_on_prepare",
    "ON",
};

int main(void) {
    int failures = 0;

    failures += test_resource_tuning_defaults_and_show_rows();
    failures += test_resource_tuning_set_and_warnings();
    failures += test_resource_tuning_diagnostics_and_isolation();

    return failures == 0 ? 0 : 1;
}

static int test_resource_tuning_defaults_and_show_rows(void) {
    static const char *const scalar_values[] = {
        "en_US",
        "en_US",
        "en_US",
        "16384",
        "16384",
        "16384",
        "32768",
        "8192",
        "4096",
        "8388608",
        "131072",
        "262144",
        "8000000",
        "32",
        "1",
        "100000.000000",
        "131072",
        "0",
        session_track_default,
        "262144",
        "1",
        "16777216",
        "8192",
        "1",
        "1",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open defaults db");
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@lc_time_names, @@GLOBAL.lc_time_names, @@SESSION.lc_time_names, "
                   "@@net_buffer_length, @@GLOBAL.net_buffer_length, "
                   "@@SESSION.net_buffer_length, @@preload_buffer_size, "
                   "@@query_alloc_block_size, @@range_alloc_block_size, "
                   "@@range_optimizer_max_mem_size, @@read_buffer_size, "
                   "@@read_rnd_buffer_size, @@GLOBAL.regexp_stack_limit, "
                   "@@GLOBAL.regexp_time_limit, @@restrict_fk_on_non_standard_key, "
                   "@@secondary_engine_cost_threshold, @@select_into_buffer_size, "
                   "@@select_into_disk_sync_delay, @@session_track_system_variables, "
                   "@@set_operations_buffer_size, "
                   "@@show_gipk_in_create_table_and_information_schema, "
                   "@@tmp_table_size, @@transaction_alloc_block_size, "
                   "@@windowing_use_high_precision, @@xa_detach_on_prepare",
            .values = scalar_values,
            .column_count = default_scalar_column_count,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "resource tuning default scalar values",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW VARIABLES WHERE Variable_name IN "
                   "('lc_time_names','net_buffer_length','preload_buffer_size','profiling',"
                   "'profiling_history_size','query_alloc_block_size','query_prealloc_size',"
                   "'range_alloc_block_size','range_optimizer_max_mem_size','read_buffer_size',"
                   "'read_rnd_buffer_size','regexp_stack_limit','regexp_time_limit',"
                   "'restrict_fk_on_non_standard_key','secondary_engine_cost_threshold',"
                   "'select_into_buffer_size','select_into_disk_sync_delay',"
                   "'session_track_system_variables','set_operations_buffer_size',"
                   "'show_gipk_in_create_table_and_information_schema',"
                   "'tmp_table_size','transaction_alloc_block_size',"
                   "'transaction_prealloc_size','windowing_use_high_precision',"
                   "'xa_detach_on_prepare','terminology_use_previous')",
            .values = resource_tuning_show_rows,
            .column_count = show_variable_column_count,
            .row_count = show_variable_row_count,
            .warning_count = 0U,
            .context = "resource tuning SHOW VARIABLES rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW GLOBAL VARIABLES WHERE Variable_name IN "
                   "('lc_time_names','net_buffer_length','preload_buffer_size','profiling',"
                   "'profiling_history_size','query_alloc_block_size','query_prealloc_size',"
                   "'range_alloc_block_size','range_optimizer_max_mem_size','read_buffer_size',"
                   "'read_rnd_buffer_size','regexp_stack_limit','regexp_time_limit',"
                   "'restrict_fk_on_non_standard_key','secondary_engine_cost_threshold',"
                   "'select_into_buffer_size','select_into_disk_sync_delay',"
                   "'session_track_system_variables','set_operations_buffer_size',"
                   "'show_gipk_in_create_table_and_information_schema',"
                   "'tmp_table_size','transaction_alloc_block_size',"
                   "'transaction_prealloc_size','windowing_use_high_precision',"
                   "'xa_detach_on_prepare','terminology_use_previous')",
            .values = resource_tuning_show_rows,
            .column_count = show_variable_column_count,
            .row_count = show_variable_row_count,
            .warning_count = 0U,
            .context = "resource tuning SHOW GLOBAL VARIABLES rows",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW SESSION VARIABLES WHERE Variable_name IN "
                   "('lc_time_names','net_buffer_length','preload_buffer_size','profiling',"
                   "'profiling_history_size','query_alloc_block_size','query_prealloc_size',"
                   "'range_alloc_block_size','range_optimizer_max_mem_size','read_buffer_size',"
                   "'read_rnd_buffer_size','regexp_stack_limit','regexp_time_limit',"
                   "'restrict_fk_on_non_standard_key','secondary_engine_cost_threshold',"
                   "'select_into_buffer_size','select_into_disk_sync_delay',"
                   "'session_track_system_variables','set_operations_buffer_size',"
                   "'show_gipk_in_create_table_and_information_schema',"
                   "'tmp_table_size','transaction_alloc_block_size',"
                   "'transaction_prealloc_size','windowing_use_high_precision',"
                   "'xa_detach_on_prepare','terminology_use_previous')",
            .values = resource_tuning_show_rows,
            .column_count = show_variable_column_count,
            .row_count = show_variable_row_count,
            .warning_count = 0U,
            .context = "resource tuning SHOW SESSION VARIABLES rows",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_resource_tuning_set_and_warnings(void) {
    static const char *const mutated_scalar_values[] = {
        "en_GB",
        "en_US",
        "en_GB",
        "65536",
        "16384",
        "8192",
        "123456",
        "262144",
        "524288",
        "0",
        "100000.000000",
        "12345.500000",
        "262144",
        "1",
        "time_zone,autocommit",
        "524288",
        "0",
        "33554432",
        "16384",
        "0",
        "0",
    };
    static const char *const deprecated_values[] = {"1", "20", "16384", "8192", "BEFORE_8_0_26"};
    static const char *const mutated_show_rows[] = {
        "lc_time_names",
        "en_GB",
        "net_buffer_length",
        "16384",
        "preload_buffer_size",
        "65536",
        "profiling",
        "ON",
        "profiling_history_size",
        "20",
        "query_alloc_block_size",
        "16384",
        "query_prealloc_size",
        "16384",
        "range_alloc_block_size",
        "8192",
        "range_optimizer_max_mem_size",
        "123456",
        "read_buffer_size",
        "262144",
        "read_rnd_buffer_size",
        "524288",
        "regexp_stack_limit",
        "8000000",
        "regexp_time_limit",
        "32",
        "restrict_fk_on_non_standard_key",
        "OFF",
        "secondary_engine_cost_threshold",
        "12345.500000",
        "select_into_buffer_size",
        "262144",
        "select_into_disk_sync_delay",
        "1",
        "session_track_system_variables",
        "time_zone,autocommit",
        "set_operations_buffer_size",
        "524288",
        "show_gipk_in_create_table_and_information_schema",
        "OFF",
        "terminology_use_previous",
        "BEFORE_8_0_26",
        "tmp_table_size",
        "33554432",
        "transaction_alloc_block_size",
        "16384",
        "transaction_prealloc_size",
        "8192",
        "windowing_use_high_precision",
        "OFF",
        "xa_detach_on_prepare",
        "OFF",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open SET db");
    failures += execute_statement_ok(database, "SET SESSION lc_time_names = 'en_GB'");
    failures += execute_statement_ok(database, "SET SESSION preload_buffer_size = 65536");
    failures += execute_statement_with_warnings(database, "SET SESSION profiling = ON", 1U);
    failures += execute_statement_with_warnings(database, "SET profiling_history_size = 20", 1U);
    failures += execute_statement_ok(database, "SET query_alloc_block_size = 16384");
    failures += execute_statement_with_warnings(database, "SET query_prealloc_size = 16384", 1U);
    failures += execute_statement_ok(database, "SET range_alloc_block_size = 8192");
    failures += execute_statement_ok(database, "SET range_optimizer_max_mem_size = 123456");
    failures += execute_statement_ok(database, "SET read_buffer_size = 262144");
    failures += execute_statement_ok(database, "SET read_rnd_buffer_size = 524288");
    failures +=
        execute_statement_with_warnings(database, "SET restrict_fk_on_non_standard_key = OFF", 1U);
    failures += expect_query_values(
        database,
        (struct expected_query){.sql = "SHOW WARNINGS",
                                .values = (const char *[]){"Warning",
                                                           "4166",
                                                           "'restrict_fk_on_non_standard_key' is "
                                                           "deprecated and will be removed in a "
                                                           "future release. Foreign key referring "
                                                           "to non-unique or partial keys is "
                                                           "unsafe and may break replication."},
                                .column_count = warning_column_count,
                                .row_count = 1U,
                                .warning_count = 0U,
                                .context = "restrict_fk_on_non_standard_key SET warning"}
    );
    failures += execute_statement_ok(database, "SET secondary_engine_cost_threshold = 12345.5");
    failures += execute_statement_ok(database, "SET select_into_buffer_size = 262144");
    failures += execute_statement_ok(database, "SET select_into_disk_sync_delay = 1");
    failures += execute_statement_ok(
        database,
        "SET session_track_system_variables = 'time_zone,autocommit'"
    );
    failures += execute_statement_ok(database, "SET set_operations_buffer_size = 524288");
    failures += execute_statement_ok(
        database,
        "SET show_gipk_in_create_table_and_information_schema = OFF"
    );
    failures += execute_statement_ok(database, "SET tmp_table_size = 33554432");
    failures += execute_statement_ok(database, "SET transaction_alloc_block_size = 16384");
    failures +=
        execute_statement_with_warnings(database, "SET transaction_prealloc_size = 8192", 1U);
    failures += execute_statement_ok(database, "SET windowing_use_high_precision = OFF");
    failures += execute_statement_ok(database, "SET xa_detach_on_prepare = OFF");
    failures += execute_statement_with_warnings(
        database,
        "SET terminology_use_previous = 'BEFORE_8_0_26'",
        1U
    );
    failures += expect_query_values(
        database,
        (struct expected_query){.sql = "SHOW WARNINGS",
                                .values = (const char *[]){"Warning",
                                                           "1287",
                                                           "'@@terminology_use_previous' is "
                                                           "deprecated and will be removed in a "
                                                           "future release."},
                                .column_count = warning_column_count,
                                .row_count = 1U,
                                .warning_count = 0U,
                                .context = "terminology_use_previous SET warning"}
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@lc_time_names, @@GLOBAL.lc_time_names, "
                   "@@SESSION.lc_time_names, @@preload_buffer_size, "
                   "@@query_alloc_block_size, @@range_alloc_block_size, "
                   "@@range_optimizer_max_mem_size, @@read_buffer_size, "
                   "@@read_rnd_buffer_size, @@restrict_fk_on_non_standard_key, "
                   "@@GLOBAL.secondary_engine_cost_threshold, "
                   "@@secondary_engine_cost_threshold, @@select_into_buffer_size, "
                   "@@select_into_disk_sync_delay, @@session_track_system_variables, "
                   "@@set_operations_buffer_size, "
                   "@@show_gipk_in_create_table_and_information_schema, "
                   "@@tmp_table_size, @@transaction_alloc_block_size, "
                   "@@windowing_use_high_precision, @@xa_detach_on_prepare",
            .values = mutated_scalar_values,
            .column_count = mutated_scalar_column_count,
            .row_count = 1U,
            .warning_count = 0U,
            .context = "resource tuning mutated scalar values",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SELECT @@profiling, @@profiling_history_size, @@query_prealloc_size, "
                   "@@transaction_prealloc_size, @@terminology_use_previous",
            .values = deprecated_values,
            .column_count = deprecated_scalar_column_count,
            .row_count = 1U,
            .warning_count = deprecated_scalar_column_count,
            .context = "resource tuning deprecated scalar values",
        }
    );
    failures += expect_query_values(
        database,
        (struct expected_query){
            .sql = "SHOW VARIABLES WHERE Variable_name IN "
                   "('lc_time_names','net_buffer_length','preload_buffer_size','profiling',"
                   "'profiling_history_size','query_alloc_block_size','query_prealloc_size',"
                   "'range_alloc_block_size','range_optimizer_max_mem_size','read_buffer_size',"
                   "'read_rnd_buffer_size','regexp_stack_limit','regexp_time_limit',"
                   "'restrict_fk_on_non_standard_key','secondary_engine_cost_threshold',"
                   "'select_into_buffer_size','select_into_disk_sync_delay',"
                   "'session_track_system_variables','set_operations_buffer_size',"
                   "'show_gipk_in_create_table_and_information_schema',"
                   "'tmp_table_size','transaction_alloc_block_size',"
                   "'transaction_prealloc_size','windowing_use_high_precision',"
                   "'xa_detach_on_prepare','terminology_use_previous')",
            .values = mutated_show_rows,
            .column_count = show_variable_column_count,
            .row_count = show_variable_row_count,
            .warning_count = 0U,
            .context = "resource tuning mutated SHOW VARIABLES rows",
        }
    );

    failures += execute_statement_ok(database, "SET @resource_text = 'en_US'");
    failures += execute_statement_ok(database, "SET lc_time_names = @resource_text");
    failures += execute_statement_ok(database, "SET @resource_int = 1");
    failures += execute_statement_ok(database, "SET windowing_use_high_precision = @resource_int");
    failures += execute_statement_ok(database, "SET @resource_decimal = 100000");
    failures +=
        execute_statement_ok(database, "SET secondary_engine_cost_threshold = @resource_decimal");
    failures += expect_query_values(
        database,
        (struct expected_query){.sql = "SELECT @@lc_time_names, "
                                       "@@windowing_use_high_precision, "
                                       "@@secondary_engine_cost_threshold",
                                .values = (const char *[]){"en_US", "1", "100000.000000"},
                                .column_count = 3U,
                                .row_count = 1U,
                                .warning_count = 0U,
                                .context = "resource tuning user variable assignment"}
    );

    failures += execute_statement_with_warnings(database, "SET GLOBAL profiling = DEFAULT", 1U);
    failures += execute_statement_with_warnings(
        database,
        "SET GLOBAL restrict_fk_on_non_standard_key = DEFAULT",
        1U
    );
    failures += execute_statement_ok(database, "SET GLOBAL regexp_time_limit = DEFAULT");
    failures +=
        execute_statement_ok(database, "SET GLOBAL secondary_engine_cost_threshold = 100000");
    failures += execute_statement_ok(database, "SET GLOBAL lc_time_names = DEFAULT");

    mylite_close(database);
    return failures;
}

static int test_resource_tuning_diagnostics_and_isolation(void) {
    struct expected_sql_error global_only_read = {
        .code = mysql_error_session_variable_only,
        .sqlstate = "HY000",
        .message_part = "is a GLOBAL variable",
    };
    struct expected_sql_error set_global_required = {
        .code = mysql_error_global_variable_set_global,
        .sqlstate = "HY000",
        .message_part = "is a GLOBAL variable and should be set with SET GLOBAL",
    };
    struct expected_sql_error session_read_only = {
        .code = mysql_error_session_variable_read_only,
        .sqlstate = "HY000",
        .message_part = "is read-only. Use SET GLOBAL",
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
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    failures +=
        mylite_test_expect_int(mylite_open_memory(&first), MYLITE_OK, "open first diagnostics db");
    failures += mylite_test_expect_int(
        mylite_open_memory(&second),
        MYLITE_OK,
        "open second diagnostics db"
    );

    failures += execute_error(first, "SELECT @@SESSION.regexp_stack_limit", global_only_read);
    failures += execute_error(first, "SELECT @@LOCAL.regexp_time_limit", global_only_read);
    failures += execute_error(first, "SET regexp_stack_limit = DEFAULT", set_global_required);
    failures +=
        execute_error(first, "SET SESSION regexp_time_limit = DEFAULT", set_global_required);
    failures += execute_error(first, "SET SESSION net_buffer_length = DEFAULT", session_read_only);
    failures += execute_error(first, "SET terminology_use_previous = 'bad'", invalid_value);
    failures += execute_error(first, "SET read_buffer_size = 1.5", incorrect_type);
    failures += execute_error(first, "SET read_rnd_buffer_size = -1", incorrect_type);
    failures += execute_error(
        first,
        "SET GLOBAL secondary_engine_cost_threshold = 1",
        (struct expected_sql_error){.code = mysql_error_parse,
                                    .sqlstate = "42000",
                                    .message_part = "supports only fixed no-op global assignments"}
    );

    failures += execute_statement_ok(first, "SET lc_time_names = 'en_GB'");
    failures += execute_statement_ok(first, "SET tmp_table_size = 33554432");
    failures += expect_query_values(
        first,
        (struct expected_query){.sql = "SELECT @@lc_time_names, @@tmp_table_size",
                                .values = (const char *[]){"en_GB", "33554432"},
                                .column_count = 2U,
                                .row_count = 1U,
                                .warning_count = 0U,
                                .context = "first handle resource tuning values"}
    );
    failures += expect_query_values(
        second,
        (struct expected_query){.sql = "SELECT @@lc_time_names, @@tmp_table_size",
                                .values = (const char *[]){"en_US", "16777216"},
                                .column_count = 2U,
                                .row_count = 1U,
                                .warning_count = 0U,
                                .context = "second handle resource tuning values"}
    );

    mylite_close(first);
    mylite_close(second);
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
    failures +=
        mylite_test_expect_size(mylite_result_column_count(result), 0U, "statement column count");
    failures += mylite_test_expect_size(mylite_result_row_count(result), 0U, "statement row count");
    failures +=
        mylite_test_expect_int64(mylite_result_affected_rows(result), 0, "statement affected rows");
    failures += mylite_test_expect_size(
        mylite_result_warning_count(result),
        warning_count,
        "statement warning count"
    );
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

    if (failures != 0) {
        mylite_result_free(result);
        return failures;
    }
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
    failures += mylite_test_expect_size(
        mylite_result_warning_count(result),
        query.warning_count,
        query.context
    );
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
