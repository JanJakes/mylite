#include <mylite/mylite.h>

#include "runtime/mylite_connection.h"
#include "storage/mylite_file_format.h"

#include <inttypes.h>
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
    test_path_suffix_capacity = 16,
    row_count_text_capacity = 32,
    variable_column_count = 2,
    session_variable_row_count = 72,
    global_variable_row_count = 68,
    sql_log_variable_row_count = 2,
    on_variable_row_count = 2,
    gtid_default_variable_row_count = 5,
    gtid_global_variable_row_count = 4,
    gtid_session_variable_row_count = 6,
    empty_gtid_variable_row_count = 3,
    mysql_error_parse = 1064,
    mysql_error_unknown_column = 1054,
    mysql_error_session_variable_only = 1238,
};

struct expected_variable_row {
    const char *name;
    const char *value;
};

struct expected_sql_error {
    int code;
    const char *sqlstate;
    const char *message_part;
};

struct expected_scalar_text_query {
    const char *sql;
    const char *expected;
    const char *context;
};

static const char default_sql_mode[] =
    "ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,"
    "ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION";

static const char *const variable_columns[variable_column_count] = {
    "Variable_name",
    "Value",
};

static int test_show_variables_values_scopes_and_filters(void);
static int test_show_variables_state_and_file_safety(void);
static int test_show_variables_diagnostics(void);
static int test_show_variables_independent_handles(void);
static int expect_query_rows(
    mylite_db *database,
    const char *sql,
    const char *const expected_rows[][variable_column_count],
    size_t expected_row_count,
    const char *context
);
static int expect_single_row(
    mylite_db *database,
    const char *sql,
    struct expected_variable_row expected,
    const char *context
);
static int expect_scalar_text(mylite_db *database, struct expected_scalar_text_query query);
static int expect_row_count(mylite_db *database, int64_t expected, const char *context);
static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result);
static int execute_statement_ok(mylite_db *database, const char *sql);
static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected);
static int make_test_path(char *path, size_t path_size, const char *name);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int read_file_at(const char *path, long offset, void *buffer, size_t size);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_uint64(uint64_t actual, uint64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);
static int expect_contains(const char *actual, const char *needle, const char *context);
static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
);

int main(void) {
    int failures = 0;

    failures += test_show_variables_values_scopes_and_filters();
    failures += test_show_variables_state_and_file_safety();
    failures += test_show_variables_diagnostics();
    failures += test_show_variables_independent_handles();

    return failures == 0 ? 0 : 1;
}

static int test_show_variables_values_scopes_and_filters(void) {
    const char *const expected_session_rows[session_variable_row_count][variable_column_count] = {
        {"auto_increment_increment", "1"},
        {"auto_increment_offset", "1"},
        {"autocommit", "ON"},
        {"basedir", "/usr/"},
        {"big_tables", "OFF"},
        {"character_set_client", "utf8mb4"},
        {"character_set_connection", "utf8mb4"},
        {"character_set_database", "utf8mb4"},
        {"character_set_filesystem", "binary"},
        {"character_set_results", "utf8mb4"},
        {"character_set_server", "utf8mb4"},
        {"character_set_system", "utf8mb3"},
        {"collation_connection", "utf8mb4_0900_ai_ci"},
        {"collation_database", "utf8mb4_0900_ai_ci"},
        {"collation_server", "utf8mb4_0900_ai_ci"},
        {"datadir", "/var/lib/mysql/"},
        {"default_storage_engine", "InnoDB"},
        {"error_count", "0"},
        {"explicit_defaults_for_timestamp", "ON"},
        {"foreign_key_checks", "ON"},
        {"group_concat_max_len", "1024"},
        {"gtid_executed", ""},
        {"gtid_mode", "OFF"},
        {"gtid_owned", ""},
        {"gtid_purged", ""},
        {"hostname", "mylite"},
        {"information_schema_stats_expiry", "86400"},
        {"innodb_read_only", "OFF"},
        {"interactive_timeout", "28800"},
        {"license", "GPL"},
        {"log_bin", "ON"},
        {"log_bin_basename", "binlog"},
        {"log_bin_index", "binlog.index"},
        {"log_bin_trust_function_creators", "OFF"},
        {"lower_case_file_system", "OFF"},
        {"lower_case_table_names", "0"},
        {"max_allowed_packet", "67108864"},
        {"pid_file", "/var/run/mysqld/mysqld.pid"},
        {"plugin_dir", "/usr/lib64/mysql/plugin/"},
        {"port", "3306"},
        {"read_only", "OFF"},
        {"server_id", "1"},
        {"server_id_bits", "32"},
        {"server_uuid", "4d796c69-7465-4000-8000-000000000001"},
        {"socket", "/var/run/mysqld/mysqld.sock"},
        {"sql_auto_is_null", "OFF"},
        {"sql_big_selects", "ON"},
        {"sql_buffer_result", "OFF"},
        {"sql_generate_invisible_primary_key", "OFF"},
        {"sql_log_bin", "ON"},
        {"sql_log_off", "OFF"},
        {"sql_mode", default_sql_mode},
        {"sql_notes", "ON"},
        {"sql_quote_show_create", "ON"},
        {"sql_replica_skip_counter", "0"},
        {"sql_require_primary_key", "OFF"},
        {"sql_safe_updates", "OFF"},
        {"sql_select_limit", "18446744073709551615"},
        {"sql_slave_skip_counter", "0"},
        {"sql_warnings", "OFF"},
        {"super_read_only", "OFF"},
        {"system_time_zone", "UTC"},
        {"timestamp", "1700000000.000000"},
        {"time_zone", "SYSTEM"},
        {"transaction_isolation", "REPEATABLE-READ"},
        {"transaction_read_only", "OFF"},
        {"unique_checks", "ON"},
        {"updatable_views_with_limit", "YES"},
        {"version", mylite_version()},
        {"version_comment", "MyLite"},
        {"wait_timeout", "28800"},
        {"warning_count", "0"},
    };
    const char *const expected_global_rows[global_variable_row_count][variable_column_count] = {
        {"auto_increment_increment", "1"},
        {"auto_increment_offset", "1"},
        {"autocommit", "ON"},
        {"basedir", "/usr/"},
        {"big_tables", "OFF"},
        {"character_set_client", "utf8mb4"},
        {"character_set_connection", "utf8mb4"},
        {"character_set_database", "utf8mb4"},
        {"character_set_filesystem", "binary"},
        {"character_set_results", "utf8mb4"},
        {"character_set_server", "utf8mb4"},
        {"character_set_system", "utf8mb3"},
        {"collation_connection", "utf8mb4_0900_ai_ci"},
        {"collation_database", "utf8mb4_0900_ai_ci"},
        {"collation_server", "utf8mb4_0900_ai_ci"},
        {"datadir", "/var/lib/mysql/"},
        {"default_storage_engine", "InnoDB"},
        {"explicit_defaults_for_timestamp", "ON"},
        {"foreign_key_checks", "ON"},
        {"group_concat_max_len", "1024"},
        {"gtid_executed", ""},
        {"gtid_mode", "OFF"},
        {"gtid_owned", ""},
        {"gtid_purged", ""},
        {"hostname", "mylite"},
        {"information_schema_stats_expiry", "86400"},
        {"innodb_read_only", "OFF"},
        {"interactive_timeout", "28800"},
        {"license", "GPL"},
        {"log_bin", "ON"},
        {"log_bin_basename", "binlog"},
        {"log_bin_index", "binlog.index"},
        {"log_bin_trust_function_creators", "OFF"},
        {"lower_case_file_system", "OFF"},
        {"lower_case_table_names", "0"},
        {"max_allowed_packet", "67108864"},
        {"pid_file", "/var/run/mysqld/mysqld.pid"},
        {"plugin_dir", "/usr/lib64/mysql/plugin/"},
        {"port", "3306"},
        {"read_only", "OFF"},
        {"server_id", "1"},
        {"server_id_bits", "32"},
        {"server_uuid", "4d796c69-7465-4000-8000-000000000001"},
        {"socket", "/var/run/mysqld/mysqld.sock"},
        {"sql_auto_is_null", "OFF"},
        {"sql_big_selects", "ON"},
        {"sql_buffer_result", "OFF"},
        {"sql_generate_invisible_primary_key", "OFF"},
        {"sql_log_off", "OFF"},
        {"sql_mode", default_sql_mode},
        {"sql_notes", "ON"},
        {"sql_quote_show_create", "ON"},
        {"sql_replica_skip_counter", "0"},
        {"sql_require_primary_key", "OFF"},
        {"sql_safe_updates", "OFF"},
        {"sql_select_limit", "18446744073709551615"},
        {"sql_slave_skip_counter", "0"},
        {"sql_warnings", "OFF"},
        {"super_read_only", "OFF"},
        {"system_time_zone", "UTC"},
        {"time_zone", "SYSTEM"},
        {"transaction_isolation", "REPEATABLE-READ"},
        {"transaction_read_only", "OFF"},
        {"unique_checks", "ON"},
        {"updatable_views_with_limit", "YES"},
        {"version", mylite_version()},
        {"version_comment", "MyLite"},
        {"wait_timeout", "28800"},
    };
    const char *const expected_sql_log_rows[sql_log_variable_row_count][variable_column_count] = {
        {"sql_log_bin", "ON"},
        {"sql_log_off", "OFF"},
    };
    const char *const expected_on_rows[on_variable_row_count][variable_column_count] = {
        {"autocommit", "ON"},
        {"sql_log_bin", "ON"},
    };
    const char *const expected_gtid_default_rows[gtid_default_variable_row_count]
                                                [variable_column_count] = {
                                                    {"autocommit", "ON"},
                                                    {"gtid_executed", ""},
                                                    {"gtid_mode", "OFF"},
                                                    {"gtid_owned", ""},
                                                    {"gtid_purged", ""},
                                                };
    const char *const expected_gtid_global_rows[gtid_global_variable_row_count]
                                               [variable_column_count] = {
                                                   {"gtid_executed", ""},
                                                   {"gtid_mode", "OFF"},
                                                   {"gtid_owned", ""},
                                                   {"gtid_purged", ""},
                                               };
    const char *const expected_gtid_session_rows[gtid_session_variable_row_count]
                                                [variable_column_count] = {
                                                    {"gtid_executed", ""},
                                                    {"gtid_mode", "OFF"},
                                                    {"gtid_owned", ""},
                                                    {"gtid_purged", ""},
                                                    {"sql_log_bin", "ON"},
                                                    {"warning_count", "0"},
                                                };
    const char *const expected_empty_gtid_rows[empty_gtid_variable_row_count]
                                              [variable_column_count] = {
                                                  {"gtid_executed", ""},
                                                  {"gtid_owned", ""},
                                                  {"gtid_purged", ""},
                                              };
    mylite_db *database = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open variables memory");
    failures += execute_statement_ok(database, "SET timestamp = 1700000000");
    failures += expect_query_rows(
        database,
        "SHOW VARIABLES",
        expected_session_rows,
        session_variable_row_count,
        "show variables rows"
    );
    failures += expect_query_rows(
        database,
        "SHOW SESSION VARIABLES",
        expected_session_rows,
        session_variable_row_count,
        "show session variables rows"
    );
    failures += expect_query_rows(
        database,
        "SHOW LOCAL VARIABLES",
        expected_session_rows,
        session_variable_row_count,
        "show local variables rows"
    );
    failures += expect_query_rows(
        database,
        "SHOW GLOBAL VARIABLES",
        expected_global_rows,
        global_variable_row_count,
        "show global variables rows"
    );
    failures += expect_query_rows(
        database,
        "SHOW VARIABLES LIKE 'sql\\_log\\_%'",
        expected_sql_log_rows,
        sql_log_variable_row_count,
        "show variables escaped underscore"
    );
    failures += expect_query_rows(
        database,
        "SHOW VARIABLES LIKE 'SQL\\_LOG\\_%'",
        expected_sql_log_rows,
        sql_log_variable_row_count,
        "show variables case-insensitive like"
    );
    failures += expect_query_rows(
        database,
        "SHOW GLOBAL VARIABLES LIKE 'sql_log_bin'",
        NULL,
        0U,
        "show global omits session-only sql_log_bin"
    );
    failures += expect_single_row(
        database,
        "SHOW SESSION VARIABLES LIKE 'character_set_system'",
        (struct expected_variable_row){
            .name = "character_set_system",
            .value = "utf8mb3",
        },
        "show session includes global system charset"
    );
    failures += expect_single_row(
        database,
        "SHOW VARIABLES LIKE 'lower_case_table_names'",
        (struct expected_variable_row){
            .name = "lower_case_table_names",
            .value = "0",
        },
        "show variables lower case table names"
    );
    failures += expect_single_row(
        database,
        "SHOW VARIABLES LIKE 'lower_case_file_system'",
        (struct expected_variable_row){
            .name = "lower_case_file_system",
            .value = "OFF",
        },
        "show variables lower case file system"
    );
    failures += expect_single_row(
        database,
        "SHOW GLOBAL VARIABLES LIKE 'lower_case_table_names'",
        (struct expected_variable_row){
            .name = "lower_case_table_names",
            .value = "0",
        },
        "show global variables lower case table names"
    );
    failures += expect_single_row(
        database,
        "SHOW GLOBAL VARIABLES LIKE 'lower_case_file_system'",
        (struct expected_variable_row){
            .name = "lower_case_file_system",
            .value = "OFF",
        },
        "show global variables lower case file system"
    );
    failures += expect_single_row(
        database,
        "SHOW VARIABLES LIKE 'max_allowed_packet'",
        (struct expected_variable_row){
            .name = "max_allowed_packet",
            .value = "67108864",
        },
        "show variables max allowed packet"
    );
    failures += expect_single_row(
        database,
        "SHOW GLOBAL VARIABLES LIKE 'max_allowed_packet'",
        (struct expected_variable_row){
            .name = "max_allowed_packet",
            .value = "67108864",
        },
        "show global variables max allowed packet"
    );
    failures += expect_single_row(
        database,
        "SHOW VARIABLES LIKE 'read_only'",
        (struct expected_variable_row){
            .name = "read_only",
            .value = "OFF",
        },
        "show variables read only"
    );
    failures += expect_single_row(
        database,
        "SHOW GLOBAL VARIABLES WHERE Variable_name = 'super_read_only'",
        (struct expected_variable_row){
            .name = "super_read_only",
            .value = "OFF",
        },
        "show global variables super read only"
    );
    failures += expect_single_row(
        database,
        "SHOW VARIABLES LIKE 'innodb_read_only'",
        (struct expected_variable_row){
            .name = "innodb_read_only",
            .value = "OFF",
        },
        "show variables innodb read only"
    );
    failures += expect_single_row(
        database,
        "SHOW VARIABLES LIKE 'explicit_defaults_for_timestamp'",
        (struct expected_variable_row){
            .name = "explicit_defaults_for_timestamp",
            .value = "ON",
        },
        "show variables explicit defaults for timestamp"
    );
    failures += expect_single_row(
        database,
        "SHOW GLOBAL VARIABLES WHERE Variable_name = 'explicit_defaults_for_timestamp'",
        (struct expected_variable_row){
            .name = "explicit_defaults_for_timestamp",
            .value = "ON",
        },
        "show global variables explicit defaults for timestamp"
    );
    failures += expect_single_row(
        database,
        "SHOW VARIABLES LIKE 'transaction_isolation'",
        (struct expected_variable_row){
            .name = "transaction_isolation",
            .value = "REPEATABLE-READ",
        },
        "show variables transaction isolation"
    );
    failures += expect_single_row(
        database,
        "SHOW GLOBAL VARIABLES LIKE 'transaction_read_only'",
        (struct expected_variable_row){
            .name = "transaction_read_only",
            .value = "OFF",
        },
        "show global variables transaction read only"
    );
    failures += expect_single_row(
        database,
        "SHOW VARIABLES WHERE Value = '0' AND "
        "Variable_name IN ('autocommit','lower_case_table_names')",
        (struct expected_variable_row){
            .name = "lower_case_table_names",
            .value = "0",
        },
        "show variables where lower case table names"
    );
    failures += expect_single_row(
        database,
        "SHOW VARIABLES WHERE Value = 'OFF' AND "
        "Variable_name IN ('autocommit','lower_case_file_system')",
        (struct expected_variable_row){
            .name = "lower_case_file_system",
            .value = "OFF",
        },
        "show variables where lower case file system"
    );
    failures += expect_single_row(
        database,
        "SHOW VARIABLES WHERE Value = '67108864' AND "
        "Variable_name IN ('autocommit','max_allowed_packet')",
        (struct expected_variable_row){
            .name = "max_allowed_packet",
            .value = "67108864",
        },
        "show variables where max allowed packet"
    );
    failures += expect_single_row(
        database,
        "SHOW VARIABLES WHERE Variable_name = 'AUTOCOMMIT'",
        (struct expected_variable_row){
            .name = "autocommit",
            .value = "ON",
        },
        "show variables where case-insensitive equality"
    );
    failures += expect_single_row(
        database,
        "SHOW VARIABLES WHERE `Variable_name` <=> 'autocommit'",
        (struct expected_variable_row){
            .name = "autocommit",
            .value = "ON",
        },
        "show variables where null-safe equality"
    );
    failures += expect_query_rows(
        database,
        "SHOW VARIABLES WHERE Value = 'on' AND Variable_name IN "
        "('autocommit','sql_log_bin','sql_log_off')",
        expected_on_rows,
        on_variable_row_count,
        "show variables where value equality"
    );
    failures += expect_single_row(
        database,
        "SHOW VARIABLES WHERE Variable_name NOT LIKE 'sql\\_%' AND "
        "Variable_name IN ('autocommit','sql_mode','sql_log_bin')",
        (struct expected_variable_row){
            .name = "autocommit",
            .value = "ON",
        },
        "show variables where not like"
    );
    failures += expect_query_rows(
        database,
        "SHOW VARIABLES WHERE (Variable_name = 'autocommit' OR "
        "Variable_name = 'sql_log_bin') AND Value = 'ON'",
        expected_on_rows,
        on_variable_row_count,
        "show variables where or and"
    );
    failures += expect_single_row(
        database,
        "SHOW VARIABLES WHERE Variable_name <> 'autocommit' AND "
        "Variable_name IN ('autocommit','sql_mode')",
        (struct expected_variable_row){
            .name = "sql_mode",
            .value = default_sql_mode,
        },
        "show variables where not equal"
    );
    failures += expect_single_row(
        database,
        "SHOW VARIABLES WHERE Variable_name < 'b' AND Variable_name IN ('autocommit','version')",
        (struct expected_variable_row){
            .name = "autocommit",
            .value = "ON",
        },
        "show variables where less than"
    );
    failures += expect_single_row(
        database,
        "SHOW VARIABLES WHERE Variable_name > 's' AND Variable_name IN ('autocommit','version')",
        (struct expected_variable_row){
            .name = "version",
            .value = mylite_version(),
        },
        "show variables where greater than"
    );
    failures += expect_single_row(
        database,
        "SHOW VARIABLES WHERE Variable_name IN (NULL, 'autocommit')",
        (struct expected_variable_row){
            .name = "autocommit",
            .value = "ON",
        },
        "show variables where in null"
    );
    failures += expect_query_rows(
        database,
        "SHOW VARIABLES WHERE Variable_name NOT IN (NULL, 'autocommit') AND "
        "Variable_name IN ('autocommit','sql_mode')",
        NULL,
        0U,
        "show variables where not in null"
    );
    failures += expect_query_rows(
        database,
        "SHOW VARIABLES WHERE Value IS NULL OR Variable_name IS NULL",
        NULL,
        0U,
        "show variables where is null"
    );
    failures += expect_single_row(
        database,
        "SHOW VARIABLES WHERE Variable_name IS NOT NULL AND Variable_name = 'autocommit'",
        (struct expected_variable_row){
            .name = "autocommit",
            .value = "ON",
        },
        "show variables where is not null"
    );
    failures += expect_query_rows(
        database,
        "SHOW VARIABLES WHERE Variable_name IN "
        "('autocommit','gtid_purged','gtid_executed','gtid_owned','gtid_mode')",
        expected_gtid_default_rows,
        gtid_default_variable_row_count,
        "show variables where default gtid rows"
    );
    failures += expect_query_rows(
        database,
        "SHOW GLOBAL VARIABLES WHERE Variable_name IN "
        "('sql_log_bin','warning_count','gtid_purged','gtid_executed','gtid_owned','gtid_mode')",
        expected_gtid_global_rows,
        gtid_global_variable_row_count,
        "show global variables where gtid rows"
    );
    failures += expect_query_rows(
        database,
        "SHOW SESSION VARIABLES WHERE Variable_name IN "
        "('sql_log_bin','warning_count','gtid_purged','gtid_executed','gtid_owned','gtid_mode')",
        expected_gtid_session_rows,
        gtid_session_variable_row_count,
        "show session variables where gtid rows"
    );
    failures += expect_query_rows(
        database,
        "SHOW VARIABLES WHERE Value = '' AND Variable_name IN "
        "('gtid_purged','gtid_executed','gtid_owned','gtid_mode')",
        expected_empty_gtid_rows,
        empty_gtid_variable_row_count,
        "show variables where empty gtid values"
    );
    failures += expect_scalar_text(
        database,
        (struct expected_scalar_text_query){
            .sql = "SELECT @@gtid_mode",
            .expected = "OFF",
            .context = "gtid_mode scalar default",
        }
    );
    failures += expect_scalar_text(
        database,
        (struct expected_scalar_text_query){
            .sql = "SELECT @@GLOBAL.gtid_mode",
            .expected = "OFF",
            .context = "gtid_mode scalar global",
        }
    );
    failures += expect_scalar_text(
        database,
        (struct expected_scalar_text_query){
            .sql = "SELECT @@gtid_purged",
            .expected = "",
            .context = "gtid_purged scalar default",
        }
    );
    failures += expect_scalar_text(
        database,
        (struct expected_scalar_text_query){
            .sql = "SELECT @@GLOBAL.gtid_executed",
            .expected = "",
            .context = "gtid_executed scalar global",
        }
    );
    failures += expect_scalar_text(
        database,
        (struct expected_scalar_text_query){
            .sql = "SELECT @@SESSION.gtid_owned",
            .expected = "",
            .context = "gtid_owned scalar session",
        }
    );
    failures += expect_scalar_text(
        database,
        (struct expected_scalar_text_query){
            .sql = "SELECT @@LOCAL.gtid_owned",
            .expected = "",
            .context = "gtid_owned scalar local",
        }
    );
    failures += expect_scalar_text(
        database,
        (struct expected_scalar_text_query){
            .sql = "SELECT @@lower_case_file_system",
            .expected = "0",
            .context = "lower_case_file_system scalar default",
        }
    );
    failures += expect_scalar_text(
        database,
        (struct expected_scalar_text_query){
            .sql = "SELECT @@GLOBAL.lower_case_file_system",
            .expected = "0",
            .context = "lower_case_file_system scalar global",
        }
    );
    failures += expect_scalar_text(
        database,
        (struct expected_scalar_text_query){
            .sql = "SELECT @@LOWER_CASE_FILE_SYSTEM",
            .expected = "0",
            .context = "lower_case_file_system scalar case-insensitive",
        }
    );
    failures += expect_scalar_text(
        database,
        (struct expected_scalar_text_query){
            .sql = "SELECT @@global.`lower_case_file_system`",
            .expected = "0",
            .context = "lower_case_file_system scalar quoted name",
        }
    );
    failures += expect_scalar_text(
        database,
        (struct expected_scalar_text_query){
            .sql = "SELECT @@lower_case_table_names",
            .expected = "0",
            .context = "lower_case_table_names scalar default",
        }
    );
    failures += expect_scalar_text(
        database,
        (struct expected_scalar_text_query){
            .sql = "SELECT @@GLOBAL.lower_case_table_names",
            .expected = "0",
            .context = "lower_case_table_names scalar global",
        }
    );
    failures += expect_scalar_text(
        database,
        (struct expected_scalar_text_query){
            .sql = "SELECT @@LOWER_CASE_TABLE_NAMES",
            .expected = "0",
            .context = "lower_case_table_names scalar case-insensitive",
        }
    );
    failures += expect_scalar_text(
        database,
        (struct expected_scalar_text_query){
            .sql = "SELECT @@global.`lower_case_table_names`",
            .expected = "0",
            .context = "lower_case_table_names scalar quoted name",
        }
    );
    failures += expect_scalar_text(
        database,
        (struct expected_scalar_text_query){
            .sql = "SELECT @@read_only",
            .expected = "0",
            .context = "read_only scalar default",
        }
    );
    failures += expect_scalar_text(
        database,
        (struct expected_scalar_text_query){
            .sql = "SELECT @@GLOBAL.super_read_only",
            .expected = "0",
            .context = "super_read_only scalar global",
        }
    );
    failures += expect_scalar_text(
        database,
        (struct expected_scalar_text_query){
            .sql = "SELECT @@global.`innodb_read_only`",
            .expected = "0",
            .context = "innodb_read_only scalar quoted global",
        }
    );

    mylite_close(database);
    return failures;
}

static int test_show_variables_state_and_file_safety(void) {
    char path[test_path_capacity];
    unsigned char expected_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    unsigned char actual_preamble[MYLITE_FILE_PREAMBLE_SIZE];
    const struct mylite_session_state *session = NULL;
    uint64_t catalog_generation = 0U;
    uint64_t sqlite_schema_generation = 0U;
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path), "file_safety") != 0) {
        return 1;
    }
    remove_related_files(path);
    mylite_file_preamble_init(expected_preamble);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open variables file");
    session = mylite_connection_session_state(database);
    if (session != NULL) {
        catalog_generation = session->catalog_generation;
        sqlite_schema_generation = session->sqlite_schema_generation;
    }

    failures += expect_single_row(
        database,
        "SHOW VARIABLES LIKE 'autocommit'",
        (struct expected_variable_row){
            .name = "autocommit",
            .value = "ON",
        },
        "show variables file result"
    );
    failures += expect_single_row(
        database,
        "SHOW VARIABLES WHERE Variable_name = 'autocommit'",
        (struct expected_variable_row){
            .name = "autocommit",
            .value = "ON",
        },
        "show variables where file result"
    );
    session = mylite_connection_session_state(database);
    if (session != NULL) {
        failures += expect_uint64(
            session->catalog_generation,
            catalog_generation,
            "show variables leaves catalog generation"
        );
        failures += expect_uint64(
            session->sqlite_schema_generation,
            sqlite_schema_generation,
            "show variables leaves SQLite schema generation"
        );
    }
    failures +=
        expect_int(read_file_at(path, 0L, actual_preamble, sizeof(actual_preamble)), 0, "preamble");
    failures += expect_bytes(
        actual_preamble,
        expected_preamble,
        sizeof(expected_preamble),
        "show variables leaves preamble"
    );

    mylite_close(database);
    database = NULL;

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "reopen variables file");
    failures += expect_single_row(
        database,
        "SHOW VARIABLES LIKE 'autocommit'",
        (struct expected_variable_row){
            .name = "autocommit",
            .value = "ON",
        },
        "show variables after reopen"
    );

    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int test_show_variables_diagnostics(void) {
    mylite_db *database = NULL;
    mylite_result *result = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&database), MYLITE_OK, "open diagnostics memory");
    failures += execute_error(
        database,
        "SHOW FULL VARIABLES",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW VARIABLES LIKE 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW VARIABLES LIKE 'sql_%' LIMIT 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "You have an error in your SQL syntax",
        }
    );
    failures += execute_error(
        database,
        "SHOW VARIABLES WHERE missing = 'x'",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'missing' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "SHOW VARIABLES WHERE variables.Variable_name = 'autocommit'",
        (struct expected_sql_error){
            .code = mysql_error_unknown_column,
            .sqlstate = "42S22",
            .message_part = "Unknown column 'variables.Variable_name' in 'where clause'",
        }
    );
    failures += execute_error(
        database,
        "SHOW VARIABLES WHERE Variable_name = 1",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SHOW VARIABLES WHERE supports only string literal predicates",
        }
    );
    failures += execute_error(
        database,
        "SHOW VARIABLES WHERE (Variable_name = 'autocommit') XOR (Value = 'ON')",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SHOW VARIABLES WHERE does not support XOR predicates",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@SESSION.gtid_purged",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "Variable 'gtid_purged' is a GLOBAL variable",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@LOCAL.gtid_executed",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "Variable 'gtid_executed' is a GLOBAL variable",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@SESSION.gtid_mode",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "Variable 'gtid_mode' is a GLOBAL variable",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@SESSION.lower_case_table_names",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "Variable 'lower_case_table_names' is a GLOBAL variable",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@SESSION.lower_case_file_system",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "Variable 'lower_case_file_system' is a GLOBAL variable",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@LOCAL.lower_case_table_names",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "Variable 'lower_case_table_names' is a GLOBAL variable",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@LOCAL.lower_case_file_system",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "Variable 'lower_case_file_system' is a GLOBAL variable",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@SESSION.read_only",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "Variable 'read_only' is a GLOBAL variable",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@LOCAL.super_read_only",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "Variable 'super_read_only' is a GLOBAL variable",
        }
    );
    failures += execute_error(
        database,
        "SELECT @@SESSION.innodb_read_only",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "Variable 'innodb_read_only' is a GLOBAL variable",
        }
    );
    failures += execute_error(
        database,
        "SET SESSION gtid_owned = ''",
        (struct expected_sql_error){
            .code = mysql_error_session_variable_only,
            .sqlstate = "HY000",
            .message_part = "Variable 'gtid_owned' is a read only variable",
        }
    );

    failures += execute_error(
        database,
        "SELECT missing_column",
        (struct expected_sql_error){
            .code = mysql_error_parse,
            .sqlstate = "42000",
            .message_part = "SELECT supports only descriptor-backed table reads",
        }
    );
    failures += expect_single_row(
        database,
        "SHOW VARIABLES LIKE 'warning_count'",
        (struct expected_variable_row){
            .name = "warning_count",
            .value = "0",
        },
        "show variables warning count clears diagnostics"
    );
    failures += expect_row_count(database, -1, "show variables row count");
    failures += execute_ok(database, "SHOW VARIABLES LIKE 'sql_slave_skip_counter'", &result);
    failures += expect_size(
        mylite_result_warning_count(result),
        0U,
        "show variables deprecated alias warning count"
    );
    mylite_result_free(result);
    result = NULL;
    failures += expect_row_count(database, -1, "deprecated show variables row count");

    mylite_close(database);
    return failures;
}

static int test_show_variables_independent_handles(void) {
    mylite_db *first = NULL;
    mylite_db *second = NULL;
    int failures = 0;

    failures += expect_int(mylite_open_memory(&first), MYLITE_OK, "open first variables handle");
    failures += expect_int(mylite_open_memory(&second), MYLITE_OK, "open second variables handle");
    failures += execute_statement_ok(first, "SET NAMES utf8mb4");
    failures += expect_single_row(
        first,
        "SHOW VARIABLES LIKE 'character_set_client'",
        (struct expected_variable_row){
            .name = "character_set_client",
            .value = "utf8mb4",
        },
        "first handle character set"
    );
    failures += expect_single_row(
        second,
        "SHOW VARIABLES LIKE 'character_set_client'",
        (struct expected_variable_row){
            .name = "character_set_client",
            .value = "utf8mb4",
        },
        "second handle character set"
    );

    mylite_close(second);
    mylite_close(first);
    return failures;
}

static int expect_query_rows(
    mylite_db *database,
    const char *sql,
    const char *const expected_rows[][variable_column_count],
    size_t expected_row_count,
    const char *context
) {
    mylite_result *result = NULL;
    int failures = 0;

    failures += execute_ok(database, sql, &result);
    if (result == NULL) {
        return failures + 1;
    }

    failures += expect_size(mylite_result_column_count(result), variable_column_count, context);
    for (size_t column = 0U; column < variable_column_count; ++column) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column),
            variable_columns[column],
            context
        );
    }
    failures += expect_size(mylite_result_row_count(result), expected_row_count, context);
    failures += expect_size(mylite_result_warning_count(result), 0U, context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, context);
    for (size_t row = 0U; row < expected_row_count; ++row) {
        for (size_t column = 0U; column < variable_column_count; ++column) {
            failures += expect_text_or_null(
                mylite_result_value_text(result, row, column),
                expected_rows[row][column],
                context
            );
        }
    }

    mylite_result_free(result);
    return failures;
}

static int expect_single_row(
    mylite_db *database,
    const char *sql,
    struct expected_variable_row expected,
    const char *context
) {
    const char *const expected_rows[1][variable_column_count] = {{expected.name, expected.value}};

    return expect_query_rows(database, sql, expected_rows, 1U, context);
}

static int expect_scalar_text(mylite_db *database, struct expected_scalar_text_query query) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, query.sql, &result);

    if (result != NULL) {
        failures += expect_size(mylite_result_column_count(result), 1U, query.context);
        failures += expect_size(mylite_result_row_count(result), 1U, query.context);
        failures += expect_text_or_null(
            mylite_result_value_text(result, 0U, 0U),
            query.expected,
            query.context
        );
    } else {
        failures += 1;
    }

    mylite_result_free(result);
    return failures;
}

static int expect_row_count(mylite_db *database, int64_t expected, const char *context) {
    mylite_result *result = NULL;
    int failures = execute_ok(database, "SELECT ROW_COUNT()", &result);

    if (result != NULL) {
        char expected_text[row_count_text_capacity];
        int written = snprintf(expected_text, sizeof(expected_text), "%" PRId64, expected);

        if (written < 0 || (size_t)written >= sizeof(expected_text)) {
            fprintf(stderr, "%s: failed to format expected row count\n", context);
            failures += 1;
        } else {
            failures += expect_text_or_null(
                mylite_result_value_text(result, 0U, 0U),
                expected_text,
                context
            );
        }
    } else {
        failures += 1;
    }

    mylite_result_free(result);
    return failures;
}

static int execute_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int rc = execute_ok(database, sql, &result);

    mylite_result_free(result);
    return rc;
}

static int execute_ok(mylite_db *database, const char *sql, mylite_result **out_result) {
    int rc = mylite_execute(database, sql, strlen(sql), out_result);

    if (rc != MYLITE_OK) {
        const struct mylite_diagnostics *diagnostics = mylite_connection_diagnostics(database);

        fprintf(
            stderr,
            "%s: expected success, got %d/%s: %s\n",
            sql,
            mylite_diagnostics_errcode(diagnostics),
            mylite_diagnostics_sqlstate(diagnostics),
            mylite_diagnostics_errmsg(diagnostics)
        );
        return 1;
    }

    return 0;
}

static int execute_error(mylite_db *database, const char *sql, struct expected_sql_error expected) {
    mylite_result *result = NULL;
    const struct mylite_diagnostics *diagnostics = NULL;
    int failures = 0;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc == MYLITE_OK) {
        fprintf(stderr, "%s: expected error, got success\n", sql);
        mylite_result_free(result);
        return 1;
    }

    diagnostics = mylite_connection_diagnostics(database);
    failures += expect_int(mylite_diagnostics_errcode(diagnostics), expected.code, sql);
    failures +=
        expect_text_or_null(mylite_diagnostics_sqlstate(diagnostics), expected.sqlstate, sql);
    failures += expect_contains(mylite_diagnostics_errmsg(diagnostics), expected.message_part, sql);
    failures += expect_int(result == NULL, 1, "failed statement returned no result");

    return failures;
}

static int make_test_path(char *path, size_t path_size, const char *name) {
    int written = snprintf(
        path,
        path_size,
        "%s/mylite_show_variables_%d_%s.mylite",
        P_tmpdir,
        current_process_id(),
        name
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "failed to build test path\n");
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
    char buffer[test_path_capacity + test_path_suffix_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        (void)remove(buffer);
    }
}

static int read_file_at(const char *path, long offset, void *buffer, size_t size) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        return 1;
    }
    if (fseek(file, offset, SEEK_SET) != 0) {
        fclose(file);
        return 1;
    }
    if (fread(buffer, 1U, size, file) != size) {
        fclose(file);
        return 1;
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
        fprintf(stderr, "%s: expected %" PRId64 ", got %" PRId64 "\n", context, expected, actual);
        return 1;
    }

    return 0;
}

static int expect_uint64(uint64_t actual, uint64_t expected, const char *context) {
    if (actual != expected) {
        fprintf(stderr, "%s: expected %" PRIu64 ", got %" PRIu64 "\n", context, expected, actual);
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

static int expect_bytes(
    const unsigned char *actual,
    const void *expected,
    size_t size,
    const char *context
) {
    if (memcmp(actual, expected, size) != 0) {
        fprintf(stderr, "%s: bytes differ\n", context);
        return 1;
    }

    return 0;
}
