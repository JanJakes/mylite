#include <mylite/mylite.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

enum {
    sql_capacity = 384,
    mysql_error_parse = 1064,
    mysql_error_session_variable_global_assignment = 1228,
    mysql_error_global_variable_scope = 1229,
    mysql_error_variable_no_default = 1230,
    mysql_error_incorrect_argument_type = 1232,
    mysql_error_session_variable_only = 1238,
};

struct compatibility_variable {
    const char *name;
    const char *scalar_value;
    const char *show_value;
    bool session_scope;
    bool read_warning;
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

struct expected_warning {
    const char *code;
    const char *message_part;
    const char *context;
};

static int test_compatibility_values_show_scope_and_warnings(void);
static int test_compatibility_set_and_diagnostics(void);
static int test_session_only_compatibility_system_variables(void);
static int test_last_insert_id_system_variables(void);
static int expect_values(
    mylite_db *database,
    const char *sql,
    const char *const *expected,
    size_t expected_count,
    const char *context
);
static int expect_show_value(mylite_db *database, struct expected_show_value expected);
static int expect_empty_result(mylite_db *database, const char *sql, const char *context);
static int expect_warning(mylite_db *database, struct expected_warning expected);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int expect_int(int actual, int expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);

static const struct compatibility_variable compatibility_variables[] = {
    {"completion_type", "NO_CHAIN", "NO_CHAIN", true, false},
    {"concurrent_insert", "AUTO", "AUTO", false, false},
    {"core_file", "0", "OFF", false, false},
    {"cte_max_recursion_depth", "1000", "1000", true, false},
    {"default_table_encryption", "0", "OFF", true, false},
    {"default_week_format", "0", "0", true, false},
    {"delay_key_write", "ON", "ON", false, false},
    {"delayed_insert_limit", "100", "100", false, true},
    {"delayed_insert_timeout", "300", "300", false, true},
    {"delayed_queue_size", "1000", "1000", false, true},
    {"disabled_storage_engines", "", "", false, false},
    {"div_precision_increment", "4", "4", true, false},
    {"enforce_gtid_consistency", "OFF", "OFF", false, false},
    {"eq_range_index_dive_limit", "200", "200", true, false},
    {"event_scheduler", "ON", "ON", false, false},
    {"explain_format", "TRADITIONAL", "TRADITIONAL", true, false},
    {"explain_json_format_version", "1", "1", true, false},
    {"flush", "0", "OFF", false, false},
    {"flush_time", "0", "0", false, false},
    {"ft_boolean_syntax", "+ -><()~*:\"\"&|", "+ -><()~*:\"\"&|", false, false},
    {"ft_max_word_len", "84", "84", false, false},
    {"ft_min_word_len", "4", "4", false, false},
    {"ft_query_expansion_limit", "20", "20", false, false},
    {"ft_stopword_file", "(built-in)", "(built-in)", false, false},
    {"generated_random_password_length", "20", "20", true, false},
    {"group_replication_consistency",
     "BEFORE_ON_PRIMARY_FAILOVER",
     "BEFORE_ON_PRIMARY_FAILOVER",
     true,
     false},
    {"gtid_executed_compression_period", "0", "0", false, false},
    {"histogram_generation_max_mem_size", "20000000", "20000000", true, false},
    {"init_connect", "", "", false, false},
    {"init_file", NULL, "", false, false},
    {"init_replica", "", "", false, false},
    {"init_slave", "", "", false, true},
};

int main(void) {
    int failures = 0;

    failures += test_compatibility_values_show_scope_and_warnings();
    failures += test_compatibility_set_and_diagnostics();
    failures += test_session_only_compatibility_system_variables();
    failures += test_last_insert_id_system_variables();

    return failures == 0 ? 0 : 1;
}

static int test_compatibility_values_show_scope_and_warnings(void) {
    struct expected_sql_error global_only_read = {
        .code = mysql_error_session_variable_only,
        .sqlstate = "HY000",
        .message_part = "is a GLOBAL variable",
    };
    mylite_db *database = NULL;
    char sql[sql_capacity];
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open compatibility db");
    for (size_t index = 0U;
         index < sizeof(compatibility_variables) / sizeof(compatibility_variables[0]);
         ++index) {
        const struct compatibility_variable *variable = &compatibility_variables[index];

        snprintf(sql, sizeof(sql), "SELECT @@%s, @@GLOBAL.%s", variable->name, variable->name);
        failures += expect_values(
            database,
            sql,
            (const char *[]){variable->scalar_value, variable->scalar_value},
            2U,
            "compatibility scalar/global"
        );

        if (variable->read_warning) {
            failures += expect_warning(
                database,
                (struct expected_warning){.code = "1287",
                                          .message_part = variable->name,
                                          .context = "compatibility warning"}
            );
        }

        snprintf(sql, sizeof(sql), "SHOW VARIABLES LIKE '%s'", variable->name);
        failures += expect_show_value(
            database,
            (struct expected_show_value){.sql = sql,
                                         .name = variable->name,
                                         .value = variable->show_value,
                                         .context = "compatibility show"}
        );
        snprintf(sql, sizeof(sql), "SHOW GLOBAL VARIABLES LIKE '%s'", variable->name);
        failures += expect_show_value(
            database,
            (struct expected_show_value){.sql = sql,
                                         .name = variable->name,
                                         .value = variable->show_value,
                                         .context = "compatibility show global"}
        );
        snprintf(sql, sizeof(sql), "SHOW SESSION VARIABLES LIKE '%s'", variable->name);
        failures += expect_show_value(
            database,
            (struct expected_show_value){.sql = sql,
                                         .name = variable->name,
                                         .value = variable->show_value,
                                         .context = "compatibility show session"}
        );

        snprintf(sql, sizeof(sql), "SELECT @@SESSION.%s", variable->name);
        if (variable->session_scope) {
            failures += expect_values(
                database,
                sql,
                (const char *[]){variable->scalar_value},
                1U,
                "compatibility session scalar"
            );
        } else {
            failures += execute_error(database, sql, global_only_read);
        }
    }

    failures += expect_values(
        database,
        "SELECT @@external_user, @@SESSION.external_user",
        (const char *[]){NULL, NULL},
        2U,
        "external_user scalar/session"
    );
    failures += execute_error(
        database,
        "SELECT @@GLOBAL.external_user",
        (struct expected_sql_error){.code = mysql_error_session_variable_only,
                                    .sqlstate = "HY000",
                                    .message_part = "Variable 'external_user' is a SESSION variable"
        }
    );
    failures += expect_show_value(
        database,
        (struct expected_show_value){.sql = "SHOW VARIABLES LIKE 'external_user'",
                                     .name = "external_user",
                                     .value = "",
                                     .context = "external_user show"}
    );
    failures += expect_show_value(
        database,
        (struct expected_show_value){.sql = "SHOW SESSION VARIABLES LIKE 'external_user'",
                                     .name = "external_user",
                                     .value = "",
                                     .context = "external_user show session"}
    );
    failures += expect_empty_result(
        database,
        "SHOW GLOBAL VARIABLES LIKE 'external_user'",
        "external_user show global"
    );

    mylite_close(database);
    return failures;
}

static int test_compatibility_set_and_diagnostics(void) {
    struct expected_sql_error global_only_set = {
        .code = mysql_error_global_variable_scope,
        .sqlstate = "HY000",
        .message_part = "is a GLOBAL variable and should be set with SET GLOBAL",
    };
    struct expected_sql_error read_only_set = {
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
        .message_part = "compatibility system variables from user variables",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open compatibility SET db");

    failures += execute_statement_ok(database, "SET completion_type = DEFAULT");
    failures += execute_statement_ok(database, "SET SESSION completion_type = NO_CHAIN");
    failures += execute_statement_ok(database, "SET GLOBAL completion_type = 'NO_CHAIN'");
    failures += execute_error(database, "SET completion_type = CHAIN", unsupported_set);

    failures += execute_error(database, "SET concurrent_insert = DEFAULT", global_only_set);
    failures += execute_statement_ok(database, "SET GLOBAL concurrent_insert = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL concurrent_insert = AUTO");
    failures += execute_error(database, "SET GLOBAL concurrent_insert = NEVER", unsupported_set);

    failures += execute_error(database, "SET GLOBAL core_file = DEFAULT", read_only_set);
    failures += execute_error(database, "SET SESSION core_file = DEFAULT", read_only_set);

    failures += execute_statement_ok(database, "SET cte_max_recursion_depth = DEFAULT");
    failures += execute_statement_ok(database, "SET SESSION cte_max_recursion_depth = 1000");
    failures += execute_statement_ok(database, "SET GLOBAL cte_max_recursion_depth = 1000");
    failures += execute_error(database, "SET cte_max_recursion_depth = 1001", unsupported_set);

    failures += execute_statement_ok(database, "SET default_table_encryption = DEFAULT");
    failures += execute_statement_ok(database, "SET SESSION default_table_encryption = OFF");
    failures += execute_statement_ok(database, "SET GLOBAL default_table_encryption = 0");
    failures += execute_error(database, "SET default_table_encryption = ON", unsupported_set);

    failures += execute_statement_ok(database, "SET default_week_format = DEFAULT");
    failures += execute_statement_ok(database, "SET SESSION default_week_format = 0");
    failures += execute_statement_ok(database, "SET GLOBAL default_week_format = 0");
    failures += execute_error(database, "SET default_week_format = 1", unsupported_set);

    failures += execute_error(database, "SET delay_key_write = DEFAULT", global_only_set);
    failures += execute_statement_ok(database, "SET GLOBAL delay_key_write = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL delay_key_write = ON");
    failures += execute_error(database, "SET GLOBAL delay_key_write = OFF", unsupported_set);

    failures += execute_error(database, "SET delayed_insert_limit = DEFAULT", global_only_set);
    failures += execute_statement_ok(database, "SET GLOBAL delayed_insert_limit = DEFAULT");
    failures += expect_warning(
        database,
        (struct expected_warning
        ){.code = "1287", .message_part = "delayed_insert_limit", .context = "delayed limit SET"}
    );
    failures += execute_statement_ok(database, "SET GLOBAL delayed_insert_timeout = 300");
    failures += expect_warning(
        database,
        (struct expected_warning){.code = "1287",
                                  .message_part = "delayed_insert_timeout",
                                  .context = "delayed timeout SET"}
    );
    failures += execute_statement_ok(database, "SET GLOBAL delayed_queue_size = 1000");
    failures += expect_warning(
        database,
        (struct expected_warning
        ){.code = "1287", .message_part = "delayed_queue_size", .context = "delayed queue SET"}
    );
    failures += execute_error(database, "SET GLOBAL delayed_insert_limit = 101", unsupported_set);

    failures +=
        execute_error(database, "SET GLOBAL disabled_storage_engines = DEFAULT", read_only_set);
    failures +=
        execute_error(database, "SET SESSION disabled_storage_engines = DEFAULT", read_only_set);

    failures += execute_statement_ok(database, "SET div_precision_increment = DEFAULT");
    failures += execute_statement_ok(database, "SET SESSION div_precision_increment = 4");
    failures += execute_statement_ok(database, "SET GLOBAL div_precision_increment = 4");
    failures += execute_error(database, "SET div_precision_increment = 5", unsupported_set);

    failures += execute_error(database, "SET enforce_gtid_consistency = DEFAULT", global_only_set);
    failures += execute_statement_ok(database, "SET GLOBAL enforce_gtid_consistency = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL enforce_gtid_consistency = OFF");
    failures +=
        execute_error(database, "SET GLOBAL enforce_gtid_consistency = WARN", unsupported_set);

    failures += execute_statement_ok(database, "SET eq_range_index_dive_limit = DEFAULT");
    failures += execute_statement_ok(database, "SET SESSION eq_range_index_dive_limit = 200");
    failures += execute_statement_ok(database, "SET GLOBAL eq_range_index_dive_limit = 200");
    failures += execute_error(database, "SET eq_range_index_dive_limit = 201", unsupported_set);

    failures += execute_error(database, "SET event_scheduler = DEFAULT", global_only_set);
    failures += execute_statement_ok(database, "SET GLOBAL event_scheduler = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL event_scheduler = ON");
    failures += execute_error(database, "SET GLOBAL event_scheduler = OFF", unsupported_set);

    failures += execute_statement_ok(database, "SET explain_format = DEFAULT");
    failures += execute_statement_ok(database, "SET SESSION explain_format = TRADITIONAL");
    failures += execute_statement_ok(database, "SET GLOBAL explain_format = 'TRADITIONAL'");
    failures += execute_error(database, "SET explain_format = TREE", unsupported_set);

    failures += execute_statement_ok(database, "SET explain_json_format_version = DEFAULT");
    failures += execute_statement_ok(database, "SET SESSION explain_json_format_version = 1");
    failures += execute_statement_ok(database, "SET GLOBAL explain_json_format_version = 1");
    failures += execute_error(database, "SET explain_json_format_version = 2", unsupported_set);

    failures += execute_error(database, "SET external_user = DEFAULT", read_only_set);
    failures += execute_error(database, "SET GLOBAL external_user = DEFAULT", read_only_set);

    failures += execute_error(database, "SET flush = DEFAULT", global_only_set);
    failures += execute_statement_ok(database, "SET GLOBAL flush = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL flush = OFF");
    failures += execute_statement_ok(database, "SET GLOBAL flush = 0");
    failures += execute_error(database, "SET GLOBAL flush = ON", unsupported_set);

    failures += execute_error(database, "SET flush_time = DEFAULT", global_only_set);
    failures += execute_statement_ok(database, "SET GLOBAL flush_time = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL flush_time = 0");
    failures += execute_error(database, "SET GLOBAL flush_time = 1", unsupported_set);

    failures += execute_error(database, "SET ft_boolean_syntax = DEFAULT", global_only_set);
    failures += execute_statement_ok(database, "SET GLOBAL ft_boolean_syntax = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL ft_boolean_syntax = '+ -><()~*:\"\"&|'");
    failures += execute_error(
        database,
        "SET GLOBAL ft_boolean_syntax = '+ -><()~*:\"\"&?'",
        unsupported_set
    );

    failures += execute_error(database, "SET GLOBAL ft_max_word_len = DEFAULT", read_only_set);
    failures += execute_error(database, "SET GLOBAL ft_min_word_len = DEFAULT", read_only_set);
    failures +=
        execute_error(database, "SET GLOBAL ft_query_expansion_limit = DEFAULT", read_only_set);
    failures += execute_error(database, "SET GLOBAL ft_stopword_file = DEFAULT", read_only_set);

    failures += execute_statement_ok(database, "SET generated_random_password_length = DEFAULT");
    failures += execute_statement_ok(database, "SET SESSION generated_random_password_length = 20");
    failures += execute_statement_ok(database, "SET GLOBAL generated_random_password_length = 20");
    failures +=
        execute_error(database, "SET generated_random_password_length = 21", unsupported_set);

    failures += execute_statement_ok(database, "SET group_replication_consistency = DEFAULT");
    failures += execute_statement_ok(
        database,
        "SET SESSION group_replication_consistency = 'BEFORE_ON_PRIMARY_FAILOVER'"
    );
    failures += execute_statement_ok(
        database,
        "SET GLOBAL group_replication_consistency = BEFORE_ON_PRIMARY_FAILOVER"
    );
    failures +=
        execute_error(database, "SET group_replication_consistency = EVENTUAL", unsupported_set);

    failures +=
        execute_error(database, "SET gtid_executed_compression_period = DEFAULT", global_only_set);
    failures +=
        execute_statement_ok(database, "SET GLOBAL gtid_executed_compression_period = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL gtid_executed_compression_period = 0");
    failures +=
        execute_error(database, "SET GLOBAL gtid_executed_compression_period = 1", unsupported_set);

    failures += execute_statement_ok(database, "SET histogram_generation_max_mem_size = DEFAULT");
    failures +=
        execute_statement_ok(database, "SET SESSION histogram_generation_max_mem_size = 20000000");
    failures +=
        execute_statement_ok(database, "SET GLOBAL histogram_generation_max_mem_size = 20000000");
    failures += execute_error(
        database,
        "SET histogram_generation_max_mem_size = 20000001",
        unsupported_set
    );

    failures += execute_error(database, "SET init_connect = DEFAULT", global_only_set);
    failures += execute_statement_ok(database, "SET GLOBAL init_connect = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL init_connect = ''");
    failures += execute_error(database, "SET GLOBAL init_connect = 'SELECT 1'", unsupported_set);

    failures += execute_error(database, "SET GLOBAL init_file = DEFAULT", read_only_set);
    failures += execute_error(database, "SET SESSION init_file = DEFAULT", read_only_set);

    failures += execute_error(database, "SET init_replica = DEFAULT", global_only_set);
    failures += execute_statement_ok(database, "SET GLOBAL init_replica = DEFAULT");
    failures += execute_statement_ok(database, "SET GLOBAL init_replica = ''");
    failures += execute_error(database, "SET GLOBAL init_replica = 'SELECT 1'", unsupported_set);

    failures += execute_error(database, "SET init_slave = DEFAULT", global_only_set);
    failures += execute_statement_ok(database, "SET GLOBAL init_slave = DEFAULT");
    failures += expect_warning(
        database,
        (struct expected_warning
        ){.code = "1287", .message_part = "init_slave", .context = "init_slave default SET"}
    );
    failures += execute_statement_ok(database, "SET GLOBAL init_slave = ''");
    failures += expect_warning(
        database,
        (struct expected_warning
        ){.code = "1287", .message_part = "init_slave", .context = "init_slave exact SET"}
    );
    failures += execute_error(database, "SET GLOBAL init_slave = 'SELECT 1'", unsupported_set);

    failures += execute_statement_ok(database, "SET @compatibility_value = 'NO_CHAIN'");
    failures += execute_error(
        database,
        "SET SESSION completion_type = @compatibility_value",
        unsupported_user_variable_set
    );
    failures += execute_error(
        database,
        "SET GLOBAL concurrent_insert = @compatibility_value",
        unsupported_user_variable_set
    );

    mylite_close(database);
    return failures;
}

static int test_session_only_compatibility_system_variables(void) {
    struct expected_sql_error session_only_read = {
        .code = mysql_error_session_variable_only,
        .sqlstate = "HY000",
        .message_part = "is a SESSION variable",
    };
    struct expected_sql_error session_only_global_set = {
        .code = mysql_error_session_variable_global_assignment,
        .sqlstate = "HY000",
        .message_part = "can't be used with SET GLOBAL",
    };
    struct expected_sql_error unsupported_set = {
        .code = mysql_error_parse,
        .sqlstate = "42000",
        .message_part = "fixed no-op",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open session-only db");

    failures += expect_values(
        database,
        "SELECT @@gtid_next, @@SESSION.gtid_next",
        (const char *[]){"AUTOMATIC", "AUTOMATIC"},
        2U,
        "gtid_next scalar/session"
    );
    failures += execute_error(database, "SELECT @@GLOBAL.gtid_next", session_only_read);
    failures += expect_show_value(
        database,
        (struct expected_show_value){.sql = "SHOW VARIABLES LIKE 'gtid_next'",
                                     .name = "gtid_next",
                                     .value = "AUTOMATIC",
                                     .context = "gtid_next show"}
    );
    failures += expect_show_value(
        database,
        (struct expected_show_value){.sql = "SHOW SESSION VARIABLES LIKE 'gtid_next'",
                                     .name = "gtid_next",
                                     .value = "AUTOMATIC",
                                     .context = "gtid_next show session"}
    );
    failures +=
        expect_empty_result(database, "SHOW GLOBAL VARIABLES LIKE 'gtid_next'", "gtid_next global");
    failures += execute_statement_ok(database, "SET gtid_next = DEFAULT");
    failures += execute_statement_ok(database, "SET SESSION gtid_next = AUTOMATIC");
    failures += execute_error(database, "SET GLOBAL gtid_next = DEFAULT", session_only_global_set);
    failures += execute_error(database, "SET gtid_next = ANONYMOUS", unsupported_set);

    failures += expect_values(
        database,
        "SELECT @@immediate_server_version, @@SESSION.immediate_server_version",
        (const char *[]){"999999", "999999"},
        2U,
        "immediate_server_version scalar/session"
    );
    failures +=
        execute_error(database, "SELECT @@GLOBAL.immediate_server_version", session_only_read);
    failures += expect_show_value(
        database,
        (struct expected_show_value){.sql = "SHOW VARIABLES LIKE 'immediate_server_version'",
                                     .name = "immediate_server_version",
                                     .value = "999999",
                                     .context = "immediate_server_version show"}
    );
    failures += expect_show_value(
        database,
        (struct expected_show_value){.sql =
                                         "SHOW SESSION VARIABLES LIKE 'immediate_server_version'",
                                     .name = "immediate_server_version",
                                     .value = "999999",
                                     .context = "immediate_server_version show session"}
    );
    failures += expect_empty_result(
        database,
        "SHOW GLOBAL VARIABLES LIKE 'immediate_server_version'",
        "immediate_server_version global"
    );
    failures += execute_statement_ok(database, "SET immediate_server_version = DEFAULT");
    failures += execute_statement_ok(database, "SET SESSION immediate_server_version = 999999");
    failures += execute_error(
        database,
        "SET GLOBAL immediate_server_version = DEFAULT",
        session_only_global_set
    );
    failures += execute_error(database, "SET immediate_server_version = 80000", unsupported_set);

    mylite_close(database);
    return failures;
}

static int test_last_insert_id_system_variables(void) {
    const size_t last_insert_id_initial_projection_count = 5U;
    struct expected_sql_error session_only_read = {
        .code = mysql_error_session_variable_only,
        .sqlstate = "HY000",
        .message_part = "is a SESSION variable",
    };
    struct expected_sql_error session_only_global_set = {
        .code = mysql_error_session_variable_global_assignment,
        .sqlstate = "HY000",
        .message_part = "can't be used with SET GLOBAL",
    };
    struct expected_sql_error no_default_set = {
        .code = mysql_error_variable_no_default,
        .sqlstate = "42000",
        .message_part = "doesn't have a default value",
    };
    struct expected_sql_error incorrect_type_set = {
        .code = mysql_error_incorrect_argument_type,
        .sqlstate = "42000",
        .message_part = "Incorrect argument type",
    };
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open last insert id db");

    failures += expect_values(
        database,
        "SELECT @@identity, @@SESSION.identity, @@last_insert_id, @@SESSION.last_insert_id, "
        "LAST_INSERT_ID()",
        (const char *[]){"0", "0", "0", "0", "0"},
        last_insert_id_initial_projection_count,
        "initial identity/last_insert_id"
    );
    failures += execute_error(database, "SELECT @@GLOBAL.identity", session_only_read);
    failures += execute_error(database, "SELECT @@GLOBAL.last_insert_id", session_only_read);
    failures += expect_show_value(
        database,
        (struct expected_show_value){.sql = "SHOW VARIABLES LIKE 'identity'",
                                     .name = "identity",
                                     .value = "0",
                                     .context = "identity show"}
    );
    failures += expect_show_value(
        database,
        (struct expected_show_value){.sql = "SHOW SESSION VARIABLES LIKE 'last_insert_id'",
                                     .name = "last_insert_id",
                                     .value = "0",
                                     .context = "last_insert_id show session"}
    );
    failures +=
        expect_empty_result(database, "SHOW GLOBAL VARIABLES LIKE 'identity'", "identity global");
    failures += expect_empty_result(
        database,
        "SHOW GLOBAL VARIABLES LIKE 'last_insert_id'",
        "last_insert_id global"
    );

    failures += execute_statement_ok(database, "SET last_insert_id = 9");
    failures += expect_values(
        database,
        "SELECT @@identity, @@last_insert_id, LAST_INSERT_ID()",
        (const char *[]){"9", "9", "9"},
        3U,
        "last_insert_id updates shared state"
    );
    failures += execute_statement_ok(database, "SET identity = 7");
    failures += expect_values(
        database,
        "SELECT @@identity, @@last_insert_id, LAST_INSERT_ID()",
        (const char *[]){"7", "7", "7"},
        3U,
        "identity updates shared state"
    );
    failures += execute_statement_ok(database, "SET last_insert_id = +9");
    failures += expect_values(
        database,
        "SELECT LAST_INSERT_ID()",
        (const char *[]){"9"},
        1U,
        "positive last_insert_id"
    );
    failures += execute_statement_ok(database, "SET last_insert_id = -1");
    failures += expect_warning(
        database,
        (struct expected_warning){.code = "1292",
                                  .message_part = "last_insert_id value: '-1'",
                                  .context = "negative last_insert_id warning"}
    );
    failures += expect_values(
        database,
        "SELECT @@identity, @@last_insert_id, LAST_INSERT_ID()",
        (const char *[]){"0", "0", "0"},
        3U,
        "negative last_insert_id clamps"
    );
    failures += execute_statement_ok(database, "SET identity = TRUE");
    failures += execute_statement_ok(database, "SET last_insert_id = FALSE");
    failures += expect_values(
        database,
        "SELECT @@identity, @@last_insert_id, LAST_INSERT_ID()",
        (const char *[]){"0", "0", "0"},
        3U,
        "boolean last_insert_id"
    );

    failures += execute_statement_ok(database, "SET @compatibility_integer = 12");
    failures += execute_statement_ok(database, "SET last_insert_id = @compatibility_integer");
    failures += expect_values(
        database,
        "SELECT @@identity, @@last_insert_id, LAST_INSERT_ID()",
        (const char *[]){"12", "12", "12"},
        3U,
        "user variable integer last_insert_id"
    );
    failures += execute_statement_ok(database, "SET @compatibility_string = '13'");
    failures +=
        execute_error(database, "SET last_insert_id = @compatibility_string", incorrect_type_set);
    failures += execute_statement_ok(database, "SET @compatibility_null = NULL");
    failures += execute_error(database, "SET identity = @compatibility_null", incorrect_type_set);

    failures += execute_error(database, "SET identity = DEFAULT", no_default_set);
    failures += execute_error(database, "SET last_insert_id = DEFAULT", no_default_set);
    failures += execute_error(database, "SET GLOBAL identity = DEFAULT", session_only_global_set);
    failures +=
        execute_error(database, "SET GLOBAL last_insert_id = DEFAULT", session_only_global_set);
    failures += execute_error(database, "SET identity = '14'", incorrect_type_set);
    failures += execute_error(database, "SET last_insert_id = NULL", incorrect_type_set);

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

    failures += expect_int(mylite_execute(database, sql, strlen(sql), &result), MYLITE_OK, context);
    failures += expect_size(mylite_result_row_count(result), 1U, context);
    failures += expect_size(mylite_result_column_count(result), expected_count, context);
    for (size_t index = 0U; index < expected_count; ++index) {
        failures +=
            expect_text(mylite_result_value_text(result, 0U, index), expected[index], context);
    }
    mylite_result_free(result);
    return failures;
}

static int expect_show_value(mylite_db *database, struct expected_show_value expected) {
    return expect_values(
        database,
        expected.sql,
        (const char *[]){expected.name, expected.value},
        2U,
        expected.context
    );
}

static int expect_empty_result(mylite_db *database, const char *sql, const char *context) {
    mylite_result *result = NULL;
    int failures = 0;

    failures += expect_int(mylite_execute(database, sql, strlen(sql), &result), MYLITE_OK, context);
    failures += expect_size(mylite_result_row_count(result), 0U, context);
    mylite_result_free(result);
    return failures;
}

static int expect_warning(mylite_db *database, struct expected_warning expected) {
    mylite_result *result = NULL;
    int failures = 0;

    failures += expect_int(
        mylite_execute(database, "SHOW WARNINGS LIMIT 1", strlen("SHOW WARNINGS LIMIT 1"), &result),
        MYLITE_OK,
        expected.context
    );
    failures += expect_size(mylite_result_row_count(result), 1U, expected.context);
    failures += expect_size(mylite_result_column_count(result), 3U, expected.context);
    failures += expect_text(mylite_result_value_text(result, 0U, 0U), "Warning", expected.context);
    failures +=
        expect_text(mylite_result_value_text(result, 0U, 1U), expected.code, expected.context);
    failures += expect_contains(
        mylite_result_value_text(result, 0U, 2U),
        expected.message_part,
        expected.context
    );
    mylite_result_free(result);
    return failures;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int failures = 0;

    failures += expect_int(mylite_execute(database, sql, strlen(sql), &result), MYLITE_OK, sql);
    failures += expect_size(mylite_result_row_count(result), 0U, sql);
    mylite_result_free(result);
    return failures;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    int failures = 0;

    failures += expect_int(mylite_execute(database, sql, strlen(sql), &result), MYLITE_ERROR, sql);
    failures += expect_int(mylite_errcode(database), expected.code, sql);
    failures += expect_text(mylite_sqlstate(database), expected.sqlstate, sql);
    failures += expect_contains(mylite_errmsg(database), expected.message_part, sql);
    mylite_result_free(result);
    return failures;
}

static int expect_int(int actual, int expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(stderr, "%s: expected %d, got %d\n", context, expected, actual);
    return 1;
}

static int expect_size(size_t actual, size_t expected, const char *context) {
    if (actual == expected) {
        return 0;
    }
    fprintf(stderr, "%s: expected %zu, got %zu\n", context, expected, actual);
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
