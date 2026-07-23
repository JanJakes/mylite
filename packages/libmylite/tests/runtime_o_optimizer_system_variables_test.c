#include "mylite_test_support.h"

#include <mylite/mylite.h>

#include <stdio.h>
#include <string.h>

enum {
    default_scalar_value_count = 12,
    assigned_scalar_value_count = 6,
    flag_scalar_value_count = 3,
    user_variable_value_count = 3,
    mysql_error_parse = 1064,
    mysql_error_invalid_value = 1231,
    mysql_error_incorrect_argument_type = 1232,
    mysql_error_session_variable_only = 1238,
    mysql_error_global_variable_only = 1229,
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_show_value {
    const char *sql;
    const char *name;
    const char *value;
    const char *context;
};

static const char optimizer_switch_default[] =
    "index_merge=on,index_merge_union=on,index_merge_sort_union=on,"
    "index_merge_intersection=on,engine_condition_pushdown=on,index_condition_pushdown=on,"
    "mrr=on,mrr_cost_based=on,block_nested_loop=on,batched_key_access=off,"
    "materialization=on,semijoin=on,loosescan=on,firstmatch=on,duplicateweedout=on,"
    "subquery_materialization_cost_based=on,use_index_extensions=on,"
    "condition_fanout_filter=on,derived_merge=on,use_invisible_indexes=off,skip_scan=on,"
    "hash_join=on,subquery_to_derived=off,prefer_ordering_index=on,"
    "hypergraph_optimizer=off,derived_condition_pushdown=on,hash_set_operations=on";

static const char optimizer_switch_modified[] =
    "index_merge=off,index_merge_union=on,index_merge_sort_union=on,"
    "index_merge_intersection=on,engine_condition_pushdown=on,index_condition_pushdown=on,"
    "mrr=on,mrr_cost_based=on,block_nested_loop=on,batched_key_access=on,"
    "materialization=on,semijoin=on,loosescan=on,firstmatch=on,duplicateweedout=on,"
    "subquery_materialization_cost_based=on,use_index_extensions=on,"
    "condition_fanout_filter=on,derived_merge=on,use_invisible_indexes=off,skip_scan=on,"
    "hash_join=on,subquery_to_derived=off,prefer_ordering_index=on,"
    "hypergraph_optimizer=off,derived_condition_pushdown=on,hash_set_operations=on";

static const char optimizer_switch_index_merge_off[] =
    "index_merge=off,index_merge_union=on,index_merge_sort_union=on,"
    "index_merge_intersection=on,engine_condition_pushdown=on,index_condition_pushdown=on,"
    "mrr=on,mrr_cost_based=on,block_nested_loop=on,batched_key_access=off,"
    "materialization=on,semijoin=on,loosescan=on,firstmatch=on,duplicateweedout=on,"
    "subquery_materialization_cost_based=on,use_index_extensions=on,"
    "condition_fanout_filter=on,derived_merge=on,use_invisible_indexes=off,skip_scan=on,"
    "hash_join=on,subquery_to_derived=off,prefer_ordering_index=on,"
    "hypergraph_optimizer=off,derived_condition_pushdown=on,hash_set_operations=on";

static const char optimizer_trace_features_default[] =
    "greedy_search=on,range_optimizer=on,dynamic_range=on,repeated_subselect=on";

static const char optimizer_trace_features_modified[] =
    "greedy_search=off,range_optimizer=off,dynamic_range=on,repeated_subselect=on";

static int test_values_show_and_scope(void);
static int test_set_session_values_and_interactions(void);
static int test_diagnostics(void);
static int expect_values(
    mylite_db *database,
    const char *sql,
    const char *const *expected,
    size_t expected_count,
    const char *context
);
static int expect_show_value(mylite_db *database, struct expected_show_value expected);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);

int main(void) {
    int failures = 0;

    failures += test_values_show_and_scope();
    failures += test_set_session_values_and_interactions();
    failures += test_diagnostics();

    return failures == 0 ? 0 : 1;
}

static int test_values_show_and_scope(void) {
    struct expected_sql_error global_only = {
        .code = mysql_error_session_variable_only,
        .sqlstate = "HY000",
        .message_part = "is a GLOBAL variable",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open O optimizer db");
    failures += expect_values(
        database,
        "SELECT @@optimizer_prune_level, @@GLOBAL.optimizer_prune_level, "
        "@@optimizer_search_depth, @@SESSION.optimizer_search_depth, "
        "@@optimizer_switch, @@optimizer_trace, @@optimizer_trace_features, "
        "@@optimizer_trace_limit, @@optimizer_trace_max_mem_size, @@optimizer_trace_offset, "
        "@@parser_max_mem_size, @@GLOBAL.partial_revokes",
        (const char *[]){"1",
                         "1",
                         "62",
                         "62",
                         optimizer_switch_default,
                         "enabled=off,one_line=off",
                         optimizer_trace_features_default,
                         "1",
                         "1048576",
                         "-1",
                         "18446744073709551615",
                         "0"},
        default_scalar_value_count,
        "O optimizer defaults"
    );

    failures += expect_show_value(
        database,
        (struct expected_show_value){.sql = "SHOW VARIABLES LIKE 'optimizer_switch'",
                                     .name = "optimizer_switch",
                                     .value = optimizer_switch_default,
                                     .context = "optimizer_switch show"}
    );
    failures += expect_show_value(
        database,
        (struct expected_show_value){.sql = "SHOW GLOBAL VARIABLES LIKE 'optimizer_trace'",
                                     .name = "optimizer_trace",
                                     .value = "enabled=off,one_line=off",
                                     .context = "optimizer_trace show global"}
    );
    failures += expect_show_value(
        database,
        (struct expected_show_value){.sql = "SHOW SESSION VARIABLES LIKE 'partial_revokes'",
                                     .name = "partial_revokes",
                                     .value = "OFF",
                                     .context = "partial_revokes show session"}
    );
    failures += execute_error(database, "SELECT @@SESSION.partial_revokes", global_only);

    mylite_close(database);
    return failures;
}

static int test_set_session_values_and_interactions(void) {
    struct expected_sql_error unsupported_global = {
        .code = mysql_error_parse,
        .sqlstate = "42000",
        .message_part = "fixed no-op",
    };
    struct expected_sql_error invalid_flag = {
        .code = mysql_error_invalid_value,
        .sqlstate = "42000",
        .message_part = "can't be set to the value of 'unknown_flag=on'",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open O SET db");

    failures += execute_statement_ok(database, "SET optimizer_prune_level = 2");
    failures += expect_values(
        database,
        "SHOW WARNINGS LIMIT 1",
        (const char *[]){"Warning", "1292", "Truncated incorrect optimizer_prune_level value: '2'"},
        3U,
        "optimizer_prune_level warning"
    );
    failures += execute_statement_ok(database, "SET optimizer_search_depth = -1");
    failures += execute_statement_ok(database, "SET optimizer_trace_limit = 18446744073709551615");
    failures +=
        execute_statement_ok(database, "SET optimizer_trace_max_mem_size = 18446744073709551615");
    failures += execute_statement_ok(database, "SET optimizer_trace_offset = -9223372036854775808");
    failures += execute_statement_ok(database, "SET parser_max_mem_size = 0");
    failures += expect_values(
        database,
        "SELECT @@optimizer_prune_level, @@optimizer_search_depth, @@optimizer_trace_limit, "
        "@@optimizer_trace_max_mem_size, @@optimizer_trace_offset, @@parser_max_mem_size",
        (const char *[]){"1",
                         "0",
                         "9223372036854775807",
                         "18446744073709551615",
                         "-9223372036854775807",
                         "10000000"},
        assigned_scalar_value_count,
        "O optimizer numeric assignments"
    );

    failures += execute_statement_ok(
        database,
        "SET optimizer_switch = 'index_merge=off,batched_key_access=on'"
    );
    failures += execute_statement_ok(database, "SET optimizer_trace = 'enabled=on,one_line=on'");
    failures += execute_statement_ok(
        database,
        "SET optimizer_trace_features = 'greedy_search=off,range_optimizer=off'"
    );
    failures += expect_values(
        database,
        "SELECT @@optimizer_switch, @@optimizer_trace, @@optimizer_trace_features",
        (const char *[]
        ){optimizer_switch_modified, "enabled=on,one_line=on", optimizer_trace_features_modified},
        flag_scalar_value_count,
        "O optimizer flag assignments"
    );
    failures += expect_values(
        database,
        "SHOW VARIABLES LIKE 'optimizer_switch'",
        (const char *[]){"optimizer_switch", optimizer_switch_modified},
        2U,
        "O optimizer flag assignment show"
    );

    failures += execute_statement_ok(database, "SET optimizer_switch = 'default'");
    failures += execute_statement_ok(database, "SET optimizer_trace = DEFAULT");
    failures += execute_statement_ok(database, "SET optimizer_trace_features = 'default'");
    failures += expect_values(
        database,
        "SELECT @@optimizer_switch, @@optimizer_trace, @@optimizer_trace_features",
        (const char *[]
        ){optimizer_switch_default, "enabled=off,one_line=off", optimizer_trace_features_default},
        flag_scalar_value_count,
        "O optimizer flag defaults"
    );

    failures += execute_statement_ok(database, "SET @opt_switch = 'index_merge=off'");
    failures += execute_statement_ok(database, "SET optimizer_switch = @opt_switch");
    failures += execute_statement_ok(database, "SET @opt_int = 2");
    failures += execute_statement_ok(database, "SET optimizer_search_depth = @opt_int");
    failures += execute_statement_ok(database, "SET @opt_offset = -2147483649");
    failures += execute_statement_ok(database, "SET optimizer_trace_offset = @opt_offset");
    failures += expect_values(
        database,
        "SELECT @@optimizer_switch, @@optimizer_search_depth, @@optimizer_trace_offset",
        (const char *[]){optimizer_switch_index_merge_off, "2", "-2147483649"},
        user_variable_value_count,
        "O optimizer user-variable assignments"
    );
    failures += expect_values(
        database,
        "SHOW VARIABLES LIKE 'optimizer_switch'",
        (const char *[]){"optimizer_switch", optimizer_switch_index_merge_off},
        2U,
        "O optimizer user-variable show"
    );

    failures += execute_statement_ok(database, "SET GLOBAL optimizer_prune_level = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL optimizer_trace_offset = -1");
    failures += execute_statement_ok(database, "SET GLOBAL optimizer_switch = 'default'");
    failures += execute_statement_ok(database, "SET GLOBAL partial_revokes = OFF");
    failures += execute_statement_ok(database, "SET GLOBAL partial_revokes = FALSE");
    failures += execute_statement_ok(database, "SET GLOBAL partial_revokes = 0");
    failures += execute_error(database, "SET GLOBAL optimizer_prune_level = 0", unsupported_global);
    failures +=
        execute_error(database, "SET GLOBAL optimizer_trace_offset = 0", unsupported_global);

    failures += execute_statement_ok(database, "SET optimizer_prune_level = 0");
    failures += execute_error(
        database,
        "SET optimizer_prune_level = 1, optimizer_switch = 'unknown_flag=on'",
        invalid_flag
    );
    failures += expect_values(
        database,
        "SELECT @@optimizer_prune_level",
        (const char *[]){"0"},
        1U,
        "O optimizer failed multi-SET rollback"
    );

    mylite_close(database);
    return failures;
}

static int test_diagnostics(void) {
    struct expected_sql_error incorrect_type = {
        .code = mysql_error_incorrect_argument_type,
        .sqlstate = "42000",
        .message_part = "Incorrect argument type",
    };
    struct expected_sql_error invalid_flag = {
        .code = mysql_error_invalid_value,
        .sqlstate = "42000",
        .message_part = "can't be set to the value of 'unknown_flag=on'",
    };
    struct expected_sql_error invalid_flag_value = {
        .code = mysql_error_invalid_value,
        .sqlstate = "42000",
        .message_part = "can't be set to the value of 'index_merge=maybe'",
    };
    struct expected_sql_error global_required = {
        .code = mysql_error_global_variable_only,
        .sqlstate = "HY000",
        .message_part = "is a GLOBAL variable and should be set with SET GLOBAL",
    };
    struct expected_sql_error unsupported_global = {
        .code = mysql_error_parse,
        .sqlstate = "42000",
        .message_part = "fixed no-op",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures +=
        mylite_test_expect_int(mylite_open_memory(&database), MYLITE_OK, "open O diagnostics db");

    failures += execute_error(database, "SET optimizer_prune_level = 'bogus'", incorrect_type);
    failures += execute_error(database, "SET parser_max_mem_size = NULL", incorrect_type);
    failures += execute_error(database, "SET optimizer_switch = 'unknown_flag=on'", invalid_flag);
    failures +=
        execute_error(database, "SET optimizer_switch = 'index_merge=maybe'", invalid_flag_value);
    failures += execute_statement_ok(database, "SET @bad_opt = 'bogus'");
    failures += execute_error(database, "SET optimizer_search_depth = @bad_opt", incorrect_type);
    failures += execute_error(database, "SET partial_revokes = ON", global_required);
    failures += execute_error(database, "SET GLOBAL partial_revokes = ON", unsupported_global);

    mylite_close(database);
    return failures;
}

static int expect_values(
    mylite_db *database,
    const char *sql,
    const char *const *expected,
    size_t expected_count,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = 0;

    failures += mylite_test_expect_int(
        mylite_execute(database, sql, strlen(sql), &result),
        MYLITE_OK,
        context
    );
    if (result == NULL) {
        fprintf(stderr, "%s: execute failed: %s\n", context, mylite_errmsg(database));
        fprintf(stderr, "%s: missing result\n", context);
        return failures + 1;
    }
    failures += mylite_test_expect_size(mylite_result_row_count(result), 1U, context);
    failures +=
        mylite_test_expect_size(mylite_result_column_count(result), expected_count, context);
    for (size_t index = 0U; index < expected_count; ++index) {
        failures += mylite_test_expect_text(
            mylite_result_value_text(result, 0U, index),
            expected[index],
            context
        );
    }
    mylite_result_free(result);
    return failures;
}

static int expect_show_value(mylite_db *database, struct expected_show_value expected) {
    mylite_result *result = NULL;
    int failures = 0;

    failures += mylite_test_expect_int(
        mylite_execute(database, expected.sql, strlen(expected.sql), &result),
        MYLITE_OK,
        expected.context
    );
    if (result == NULL) {
        fprintf(stderr, "%s: execute failed: %s\n", expected.context, mylite_errmsg(database));
        fprintf(stderr, "%s: missing result\n", expected.context);
        return failures + 1;
    }
    failures += mylite_test_expect_size(mylite_result_row_count(result), 1U, expected.context);
    failures += mylite_test_expect_size(mylite_result_column_count(result), 2U, expected.context);
    failures += mylite_test_expect_text(
        mylite_result_value_text(result, 0U, 0U),
        expected.name,
        expected.context
    );
    failures += mylite_test_expect_text(
        mylite_result_value_text(result, 0U, 1U),
        expected.value,
        expected.context
    );
    mylite_result_free(result);
    return failures;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures =
        mylite_test_expect_int(mylite_execute(database, sql, strlen(sql), &result), MYLITE_OK, sql);

    if (failures != 0) {
        fprintf(stderr, "expected OK for [%s], got %s\n", sql, mylite_errmsg(database));
    }

    mylite_result_free(result);
    return failures;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

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
