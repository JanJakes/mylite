#include <mylite/mylite.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#  include <process.h>
#else
#  include <unistd.h>
#endif

enum {
    test_path_capacity = 1024,
    metrics_column_count = 4,
    show_columns_column_count = 6,
    show_full_columns_column_count = 9,
    show_index_column_count = 15,
    information_schema_columns_column_count = 8,
    information_schema_tables_column_count = 7,
    information_schema_views_column_count = 6,
    information_schema_view_table_usage_column_count = 4,
    show_create_view_column_count = 4,
    show_table_status_column_count = 18,
    datetime_text_length = 19,
    datetime_year_month_separator = 4,
    datetime_month_day_separator = 7,
    datetime_date_time_separator = 10,
    datetime_hour_minute_separator = 13,
    datetime_minute_second_separator = 16,
};

struct expected_query {
    const char *sql;
    const char *const *column_names;
    size_t column_count;
    const char *const *values;
    size_t row_count;
    const char *context;
};

struct expected_query_contains {
    const char *sql;
    const char *const *column_names;
    size_t column_count;
    size_t row_count;
    size_t row_index;
    size_t column_index;
    const char *needle;
    const char *context;
};

static const char expected_datetime_value[] = "<datetime>";

static int test_sys_metrics_view(void);
static int expect_statement_ok(mylite_db *database, const char *sql);
static int expect_query(mylite_db *database, struct expected_query expected);
static int expect_query_contains(mylite_db *database, struct expected_query_contains expected);
static int expect_row_count_status(mylite_db *database, const char *context);
static int make_test_path(char *path, size_t path_size);
static int current_process_id(void);
static void remove_related_files(const char *path);
static void remove_with_suffix(const char *path, const char *suffix);
static int expect_int(int actual, int expected, const char *context);
static int expect_int64(int64_t actual, int64_t expected, const char *context);
static int expect_size(size_t actual, size_t expected, const char *context);
static int expect_text_or_null(const char *actual, const char *expected, const char *context);
static int expect_datetime_text(const char *actual, const char *context);

static const char *const metrics_columns[metrics_column_count] = {
    "Variable_name",
    "Variable_value",
    "Type",
    "Enabled",
};

static const char *const show_columns_columns[show_columns_column_count] = {
    "Field",
    "Type",
    "Null",
    "Key",
    "Default",
    "Extra",
};

static const char *const show_full_columns_columns[show_full_columns_column_count] = {
    "Field",
    "Type",
    "Collation",
    "Null",
    "Key",
    "Default",
    "Extra",
    "Privileges",
    "Comment",
};

static const char *const show_index_columns[show_index_column_count] = {
    "Table",
    "Non_unique",
    "Key_name",
    "Seq_in_index",
    "Column_name",
    "Collation",
    "Cardinality",
    "Sub_part",
    "Packed",
    "Null",
    "Index_type",
    "Comment",
    "Index_comment",
    "Visible",
    "Expression",
};

static const char
    *const information_schema_columns_columns[information_schema_columns_column_count] = {
        "COLUMN_NAME",
        "ORDINAL_POSITION",
        "IS_NULLABLE",
        "COLUMN_TYPE",
        "CHARACTER_SET_NAME",
        "COLLATION_NAME",
        "CHARACTER_MAXIMUM_LENGTH",
        "CHARACTER_OCTET_LENGTH",
};

static const char *const information_schema_tables_columns[information_schema_tables_column_count] =
    {
        "TABLE_SCHEMA",
        "TABLE_NAME",
        "TABLE_TYPE",
        "ENGINE",
        "TABLE_ROWS",
        "DATA_LENGTH",
        "TABLE_COMMENT",
};

static const char *const information_schema_views_columns[information_schema_views_column_count] = {
    "TABLE_SCHEMA",
    "TABLE_NAME",
    "CHECK_OPTION",
    "IS_UPDATABLE",
    "DEFINER",
    "SECURITY_TYPE",
};

static const char *const
    information_schema_view_table_usage_columns[information_schema_view_table_usage_column_count] =
        {
            "VIEW_SCHEMA",
            "VIEW_NAME",
            "TABLE_SCHEMA",
            "TABLE_NAME",
};

static const char *const show_create_view_columns[show_create_view_column_count] = {
    "View",
    "Create View",
    "character_set_client",
    "collation_connection",
};

static const char *const show_table_status_columns[show_table_status_column_count] = {
    "Name",
    "Engine",
    "Version",
    "Row_format",
    "Rows",
    "Avg_row_length",
    "Data_length",
    "Max_data_length",
    "Index_length",
    "Data_free",
    "Auto_increment",
    "Create_time",
    "Update_time",
    "Check_time",
    "Collation",
    "Checksum",
    "Create_options",
    "Comment",
};

int main(void) {
    return test_sys_metrics_view() == 0 ? 0 : 1;
}

static int test_sys_metrics_view(void) {
    enum {
        metrics_row_count = 423,
    };

    static const char *const count_column[] = {"COUNT(*)"};
    static const char *const count_zero[] = {"0"};
    static const char *const metrics_row_count_value[] = {"423"};
    static const char *const row_count_minus_one[] = {"-1"};
    static const char *const aborted_clients_row[] = {
        "aborted_clients",
        "0",
        "Global Status",
        "YES",
    };
    static const char *const binlog_cache_use_row[] = {
        "binlog_cache_use",
        "0",
        "Global Status",
        "YES",
    };
    static const char *const com_select_row[] = {
        "com_select",
        "0",
        "Global Status",
        "YES",
    };
    static const char *const connections_row[] = {
        "connections",
        "1",
        "Global Status",
        "YES",
    };
    static const char *const created_tmp_tables_row[] = {
        "created_tmp_tables",
        "0",
        "Global Status",
        "YES",
    };
    static const char *const current_tls_version_row[] = {
        "current_tls_version",
        "",
        "Global Status",
        "YES",
    };
    static const char *const deprecated_i_s_processlist_count_row[] = {
        "deprecated_use_i_s_processlist_count",
        "0",
        "Global Status",
        "YES",
    };
    static const char *const connection_errors_internal_row[] = {
        "connection_errors_internal",
        "0",
        "Global Status",
        "YES",
    };
    static const char *const error_log_buffered_events_row[] = {
        "error_log_buffered_events",
        "0",
        "Global Status",
        "YES",
    };
    static const char *const handler_commit_row[] = {
        "handler_commit",
        "0",
        "Global Status",
        "YES",
    };
    static const char *const innodb_page_size_row[] = {
        "innodb_page_size",
        "16384",
        "Global Status",
        "YES",
    };
    static const char *const innodb_redo_log_enabled_row[] = {
        "innodb_redo_log_enabled",
        "ON",
        "Global Status",
        "YES",
    };
    static const char *const key_reads_row[] = {
        "key_reads",
        "0",
        "Global Status",
        "YES",
    };
    static const char *const max_used_connections_row[] = {
        "max_used_connections",
        "1",
        "Global Status",
        "YES",
    };
    static const char *const open_tables_row[] = {
        "open_tables",
        "0",
        "Global Status",
        "YES",
    };
    static const char *const performance_schema_logger_lost_row[] = {
        "performance_schema_logger_lost",
        "0",
        "Global Status",
        "YES",
    };
    static const char *const select_range_check_row[] = {
        "select_range_check",
        "0",
        "Global Status",
        "YES",
    };
    static const char *const table_locks_waited_row[] = {
        "table_locks_waited",
        "0",
        "Global Status",
        "YES",
    };
    static const char *const ssl_version_row[] = {
        "ssl_version",
        "",
        "Global Status",
        "YES",
    };
    static const char *const sort_rows_row[] = {
        "sort_rows",
        "0",
        "Global Status",
        "YES",
    };
    static const char *const table_open_cache_hits_row[] = {
        "table_open_cache_hits",
        "0",
        "Global Status",
        "YES",
    };
    static const char *const tc_log_page_waits_row[] = {
        "tc_log_page_waits",
        "0",
        "Global Status",
        "YES",
    };
    static const char *const telemetry_metrics_supported_row[] = {
        "telemetry_metrics_supported",
        "OFF",
        "Global Status",
        "YES",
    };
    static const char *const threads_connected_row[] = {
        "threads_connected",
        "1",
        "Global Status",
        "YES",
    };
    static const char *const tls_library_version_row[] = {
        "tls_library_version",
        "",
        "Global Status",
        "YES",
    };
    static const char *const show_columns_values[] = {
        "Variable_name",  "varchar(193)", "NO",  "", "",   "",
        "Variable_value", "text",         "YES", "", NULL, "",
        "Type",           "varchar(210)", "NO",  "", "",   "",
        "Enabled",        "varchar(7)",   "NO",  "", "",   "",
    };
    static const char *const show_full_columns_values[] = {
        "Variable_name",
        "varchar(193)",
        "utf8mb4_0900_ai_ci",
        "NO",
        "",
        "",
        "",
        "select,insert,update,references",
        "",
        "Variable_value",
        "text",
        "utf8mb4_0900_ai_ci",
        "YES",
        "",
        NULL,
        "",
        "select,insert,update,references",
        "",
        "Type",
        "varchar(210)",
        "utf8mb3_general_ci",
        "NO",
        "",
        "",
        "",
        "select,insert,update,references",
        "",
        "Enabled",
        "varchar(7)",
        "utf8mb4_0900_ai_ci",
        "NO",
        "",
        "",
        "",
        "select,insert,update,references",
        "",
    };
    static const char *const information_schema_columns_values[] = {
        "Variable_name",
        "1",
        "NO",
        "varchar(193)",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "193",
        "772",
        "Variable_value",
        "2",
        "YES",
        "text",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "65535",
        "65535",
        "Type",
        "3",
        "NO",
        "varchar(210)",
        "utf8mb3",
        "utf8mb3_general_ci",
        "210",
        "630",
        "Enabled",
        "4",
        "NO",
        "varchar(7)",
        "utf8mb4",
        "utf8mb4_0900_ai_ci",
        "7",
        "28",
    };
    static const char *const information_schema_tables_values[] = {
        "sys",
        "metrics",
        "VIEW",
        NULL,
        NULL,
        NULL,
        "VIEW",
    };
    static const char *const information_schema_views_values[] = {
        "sys",
        "metrics",
        "NONE",
        "NO",
        "mysql.sys@localhost",
        "INVOKER",
    };
    static const char *const view_table_usage_values[] = {
        "sys",
        "metrics",
        "information_schema",
        "INNODB_METRICS",
        "sys",
        "metrics",
        "performance_schema",
        "global_status",
        "sys",
        "metrics",
        "performance_schema",
        "memory_summary_global_by_event_name",
        "sys",
        "metrics",
        "performance_schema",
        "setup_instruments",
    };
    static const char *const show_table_status_values[] = {
        "metrics",
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        expected_datetime_value,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        "VIEW",
    };
    char path[test_path_capacity];
    mylite_db *database = NULL;
    int failures = 0;

    if (make_test_path(path, sizeof(path)) != 0) {
        return 1;
    }
    remove_related_files(path);

    failures += expect_int(mylite_open(path, &database), MYLITE_OK, "open file database");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM sys.metrics",
            .column_names = count_column,
            .column_count = sizeof(count_column) / sizeof(count_column[0]),
            .values = metrics_row_count_value,
            .row_count = 1U,
            .context = "sys.metrics row count",
        }
    );
    failures += expect_row_count_status(database, "row count after sys.metrics read");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT Variable_name, Variable_value, Type, Enabled FROM sys.metrics "
                   "WHERE Variable_name = 'aborted_clients'",
            .column_names = metrics_columns,
            .column_count = metrics_column_count,
            .values = aborted_clients_row,
            .row_count = 1U,
            .context = "sys.metrics aborted_clients row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT Variable_name, Variable_value, Type, Enabled FROM sys.metrics "
                   "WHERE Variable_name = 'com_select'",
            .column_names = metrics_columns,
            .column_count = metrics_column_count,
            .values = com_select_row,
            .row_count = 1U,
            .context = "sys.metrics command counter row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT Variable_name, Variable_value, Type, Enabled FROM sys.metrics "
                   "WHERE Variable_name = 'binlog_cache_use'",
            .column_names = metrics_columns,
            .column_count = metrics_column_count,
            .values = binlog_cache_use_row,
            .row_count = 1U,
            .context = "sys.metrics binlog counter row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT Variable_name, Variable_value, Type, Enabled FROM sys.metrics "
                   "WHERE Variable_name = 'connections'",
            .column_names = metrics_columns,
            .column_count = metrics_column_count,
            .values = connections_row,
            .row_count = 1U,
            .context = "sys.metrics connections row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT Variable_name, Variable_value, Type, Enabled FROM sys.metrics "
                   "WHERE Variable_name = 'connection_errors_internal'",
            .column_names = metrics_columns,
            .column_count = metrics_column_count,
            .values = connection_errors_internal_row,
            .row_count = 1U,
            .context = "sys.metrics connection diagnostics row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT Variable_name, Variable_value, Type, Enabled FROM sys.metrics "
                   "WHERE Variable_name = 'created_tmp_tables'",
            .column_names = metrics_columns,
            .column_count = metrics_column_count,
            .values = created_tmp_tables_row,
            .row_count = 1U,
            .context = "sys.metrics created counter row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT Variable_name, Variable_value, Type, Enabled FROM sys.metrics "
                   "WHERE Variable_name = 'current_tls_version'",
            .column_names = metrics_columns,
            .column_count = metrics_column_count,
            .values = current_tls_version_row,
            .row_count = 1U,
            .context = "sys.metrics current tls row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT Variable_name, Variable_value, Type, Enabled FROM sys.metrics "
                   "WHERE Variable_name = 'deprecated_use_i_s_processlist_count'",
            .column_names = metrics_columns,
            .column_count = metrics_column_count,
            .values = deprecated_i_s_processlist_count_row,
            .row_count = 1U,
            .context = "sys.metrics deprecated processlist row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT Variable_name, Variable_value, Type, Enabled FROM sys.metrics "
                   "WHERE Variable_name = 'error_log_buffered_events'",
            .column_names = metrics_columns,
            .column_count = metrics_column_count,
            .values = error_log_buffered_events_row,
            .row_count = 1U,
            .context = "sys.metrics error log row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT Variable_name, Variable_value, Type, Enabled FROM sys.metrics "
                   "WHERE Variable_name = 'handler_commit'",
            .column_names = metrics_columns,
            .column_count = metrics_column_count,
            .values = handler_commit_row,
            .row_count = 1U,
            .context = "sys.metrics handler counter row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT Variable_name, Variable_value, Type, Enabled FROM sys.metrics "
                   "WHERE Variable_name = 'innodb_page_size'",
            .column_names = metrics_columns,
            .column_count = metrics_column_count,
            .values = innodb_page_size_row,
            .row_count = 1U,
            .context = "sys.metrics innodb page size row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT Variable_name, Variable_value, Type, Enabled FROM sys.metrics "
                   "WHERE Variable_name = 'innodb_redo_log_enabled'",
            .column_names = metrics_columns,
            .column_count = metrics_column_count,
            .values = innodb_redo_log_enabled_row,
            .row_count = 1U,
            .context = "sys.metrics innodb redo row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT Variable_name, Variable_value, Type, Enabled FROM sys.metrics "
                   "WHERE Variable_name = 'key_reads'",
            .column_names = metrics_columns,
            .column_count = metrics_column_count,
            .values = key_reads_row,
            .row_count = 1U,
            .context = "sys.metrics key counter row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT Variable_name, Variable_value, Type, Enabled FROM sys.metrics "
                   "WHERE Variable_name = 'max_used_connections'",
            .column_names = metrics_columns,
            .column_count = metrics_column_count,
            .values = max_used_connections_row,
            .row_count = 1U,
            .context = "sys.metrics max connection row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT Variable_name, Variable_value, Type, Enabled FROM sys.metrics "
                   "WHERE Variable_name = 'open_tables'",
            .column_names = metrics_columns,
            .column_count = metrics_column_count,
            .values = open_tables_row,
            .row_count = 1U,
            .context = "sys.metrics open counter row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT Variable_name, Variable_value, Type, Enabled FROM sys.metrics "
                   "WHERE Variable_name = 'performance_schema_logger_lost'",
            .column_names = metrics_columns,
            .column_count = metrics_column_count,
            .values = performance_schema_logger_lost_row,
            .row_count = 1U,
            .context = "sys.metrics performance schema loss counter row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT Variable_name, Variable_value, Type, Enabled FROM sys.metrics "
                   "WHERE Variable_name = 'select_range_check'",
            .column_names = metrics_columns,
            .column_count = metrics_column_count,
            .values = select_range_check_row,
            .row_count = 1U,
            .context = "sys.metrics select counter row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT Variable_name, Variable_value, Type, Enabled FROM sys.metrics "
                   "WHERE Variable_name = 'table_locks_waited'",
            .column_names = metrics_columns,
            .column_count = metrics_column_count,
            .values = table_locks_waited_row,
            .row_count = 1U,
            .context = "sys.metrics table lock counter row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT Variable_name, Variable_value, Type, Enabled FROM sys.metrics "
                   "WHERE Variable_name = 'ssl_version'",
            .column_names = metrics_columns,
            .column_count = metrics_column_count,
            .values = ssl_version_row,
            .row_count = 1U,
            .context = "sys.metrics ssl status row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT Variable_name, Variable_value, Type, Enabled FROM sys.metrics "
                   "WHERE Variable_name = 'sort_rows'",
            .column_names = metrics_columns,
            .column_count = metrics_column_count,
            .values = sort_rows_row,
            .row_count = 1U,
            .context = "sys.metrics sort counter row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT Variable_name, Variable_value, Type, Enabled FROM sys.metrics "
                   "WHERE Variable_name = 'table_open_cache_hits'",
            .column_names = metrics_columns,
            .column_count = metrics_column_count,
            .values = table_open_cache_hits_row,
            .row_count = 1U,
            .context = "sys.metrics table open cache counter row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT Variable_name, Variable_value, Type, Enabled FROM sys.metrics "
                   "WHERE Variable_name = 'tc_log_page_waits'",
            .column_names = metrics_columns,
            .column_count = metrics_column_count,
            .values = tc_log_page_waits_row,
            .row_count = 1U,
            .context = "sys.metrics tc log counter row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT Variable_name, Variable_value, Type, Enabled FROM sys.metrics "
                   "WHERE Variable_name = 'telemetry_metrics_supported'",
            .column_names = metrics_columns,
            .column_count = metrics_column_count,
            .values = telemetry_metrics_supported_row,
            .row_count = 1U,
            .context = "sys.metrics telemetry support row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT Variable_name, Variable_value, Type, Enabled FROM sys.metrics "
                   "WHERE Variable_name = 'threads_connected'",
            .column_names = metrics_columns,
            .column_count = metrics_column_count,
            .values = threads_connected_row,
            .row_count = 1U,
            .context = "sys.metrics threads_connected row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT Variable_name, Variable_value, Type, Enabled FROM sys.metrics "
                   "WHERE Variable_name = 'tls_library_version'",
            .column_names = metrics_columns,
            .column_count = metrics_column_count,
            .values = tls_library_version_row,
            .row_count = 1U,
            .context = "sys.metrics tls library row",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM sys.metrics WHERE Variable_name = 'compression'",
            .column_names = count_column,
            .column_count = sizeof(count_column) / sizeof(count_column[0]),
            .values = count_zero,
            .row_count = 1U,
            .context = "sys.metrics omits session-only compression",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM sys.metrics WHERE Type = 'Global Status'",
            .column_names = count_column,
            .column_count = sizeof(count_column) / sizeof(count_column[0]),
            .values = metrics_row_count_value,
            .row_count = 1U,
            .context = "sys.metrics global status count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM sys.metrics WHERE Type = 'Performance Schema'",
            .column_names = count_column,
            .column_count = sizeof(count_column) / sizeof(count_column[0]),
            .values = count_zero,
            .row_count = 1U,
            .context = "sys.metrics no Performance Schema rows",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM sys.metrics WHERE Type = 'System Time'",
            .column_names = count_column,
            .column_count = sizeof(count_column) / sizeof(count_column[0]),
            .values = count_zero,
            .row_count = 1U,
            .context = "sys.metrics no system time rows",
        }
    );
    failures += expect_statement_ok(database, "USE sys");
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM metrics",
            .column_names = count_column,
            .column_count = sizeof(count_column) / sizeof(count_column[0]),
            .values = metrics_row_count_value,
            .row_count = 1U,
            .context = "sys.metrics selected-schema row count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM metrics",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = show_columns_values,
            .row_count = metrics_column_count,
            .context = "sys.metrics selected-schema show columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW COLUMNS FROM sys.metrics",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = show_columns_values,
            .row_count = metrics_column_count,
            .context = "sys.metrics show columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "DESCRIBE sys.metrics",
            .column_names = show_columns_columns,
            .column_count = show_columns_column_count,
            .values = show_columns_values,
            .row_count = metrics_column_count,
            .context = "sys.metrics describe",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW FULL COLUMNS FROM sys.metrics",
            .column_names = show_full_columns_columns,
            .column_count = show_full_columns_column_count,
            .values = show_full_columns_values,
            .row_count = metrics_column_count,
            .context = "sys.metrics show full columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COLUMN_NAME, ORDINAL_POSITION, IS_NULLABLE, COLUMN_TYPE, "
                   "CHARACTER_SET_NAME, COLLATION_NAME, CHARACTER_MAXIMUM_LENGTH, "
                   "CHARACTER_OCTET_LENGTH FROM INFORMATION_SCHEMA.COLUMNS "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'metrics' "
                   "ORDER BY ORDINAL_POSITION",
            .column_names = information_schema_columns_columns,
            .column_count = information_schema_columns_column_count,
            .values = information_schema_columns_values,
            .row_count = metrics_column_count,
            .context = "sys.metrics information_schema.columns",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, TABLE_TYPE, ENGINE, TABLE_ROWS, DATA_LENGTH, "
                   "TABLE_COMMENT FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA = 'sys' "
                   "AND TABLE_NAME = 'metrics'",
            .column_names = information_schema_tables_columns,
            .column_count = information_schema_tables_column_count,
            .values = information_schema_tables_values,
            .row_count = 1U,
            .context = "sys.metrics information_schema.tables",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT TABLE_SCHEMA, TABLE_NAME, CHECK_OPTION, IS_UPDATABLE, DEFINER, "
                   "SECURITY_TYPE FROM INFORMATION_SCHEMA.VIEWS WHERE TABLE_SCHEMA = 'sys' "
                   "AND TABLE_NAME = 'metrics'",
            .column_names = information_schema_views_columns,
            .column_count = information_schema_views_column_count,
            .values = information_schema_views_values,
            .row_count = 1U,
            .context = "sys.metrics information_schema.views",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT VIEW_SCHEMA, VIEW_NAME, TABLE_SCHEMA, TABLE_NAME "
                   "FROM INFORMATION_SCHEMA.VIEW_TABLE_USAGE WHERE VIEW_SCHEMA = 'sys' "
                   "AND VIEW_NAME = 'metrics' ORDER BY VIEW_SCHEMA, VIEW_NAME, TABLE_SCHEMA, "
                   "TABLE_NAME",
            .column_names = information_schema_view_table_usage_columns,
            .column_count = information_schema_view_table_usage_column_count,
            .values = view_table_usage_values,
            .row_count = 4U,
            .context = "sys.metrics information_schema.view_table_usage",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.VIEW_ROUTINE_USAGE "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'metrics'",
            .column_names = count_column,
            .column_count = sizeof(count_column) / sizeof(count_column[0]),
            .values = count_zero,
            .row_count = 1U,
            .context = "sys.metrics information_schema.view_routine_usage count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW INDEX FROM sys.metrics",
            .column_names = show_index_columns,
            .column_count = show_index_column_count,
            .values = NULL,
            .row_count = 0U,
            .context = "sys.metrics show index",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.STATISTICS "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'metrics'",
            .column_names = count_column,
            .column_count = sizeof(count_column) / sizeof(count_column[0]),
            .values = count_zero,
            .row_count = 1U,
            .context = "sys.metrics information_schema.statistics count",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT COUNT(*) FROM INFORMATION_SCHEMA.TABLE_CONSTRAINTS "
                   "WHERE TABLE_SCHEMA = 'sys' AND TABLE_NAME = 'metrics'",
            .column_names = count_column,
            .column_count = sizeof(count_column) / sizeof(count_column[0]),
            .values = count_zero,
            .row_count = 1U,
            .context = "sys.metrics information_schema.table_constraints count",
        }
    );
    failures += expect_query_contains(
        database,
        (struct expected_query_contains){
            .sql = "SHOW CREATE VIEW sys.metrics",
            .column_names = show_create_view_columns,
            .column_count = show_create_view_column_count,
            .row_count = 1U,
            .row_index = 0U,
            .column_index = 1U,
            .needle = "select lower(`performance_schema`.`global_status`.`VARIABLE_NAME`) AS "
                      "`Variable_name`",
            .context = "sys.metrics show create global status source",
        }
    );
    failures += expect_query_contains(
        database,
        (struct expected_query_contains){
            .sql = "SHOW CREATE TABLE metrics",
            .column_names = show_create_view_columns,
            .column_count = show_create_view_column_count,
            .row_count = 1U,
            .row_index = 0U,
            .column_index = 1U,
            .needle = "CREATE ALGORITHM=TEMPTABLE DEFINER=`mysql.sys`@`localhost` SQL SECURITY "
                      "INVOKER VIEW `metrics`",
            .context = "sys.metrics selected-schema show create table",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SHOW TABLE STATUS FROM sys LIKE 'metrics'",
            .column_names = show_table_status_columns,
            .column_count = show_table_status_column_count,
            .values = show_table_status_values,
            .row_count = 1U,
            .context = "sys.metrics show table status",
        }
    );
    failures += expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT()",
            .column_names = (const char *const[]){"ROW_COUNT()"},
            .column_count = 1U,
            .values = row_count_minus_one,
            .row_count = 1U,
            .context = "row count after sys.metrics table status",
        }
    );
    mylite_close(database);
    remove_related_files(path);
    return failures;
}

static int expect_statement_ok(mylite_db *database, const char *sql) {
    mylite_result *result = NULL;
    int rc = mylite_execute(database, sql, strlen(sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected OK, got %d / %d %s %s\n",
            sql,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }
    mylite_result_free(result);
    return 0;
}

static int expect_query(mylite_db *database, struct expected_query expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected OK, got %d / %d %s %s\n",
            expected.context,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }
    if (result == NULL) {
        fprintf(stderr, "%s: expected result object\n", expected.context);
        return 1;
    }

    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    for (size_t column_index = 0U;
         expected.column_names != NULL && column_index < expected.column_count;
         ++column_index) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column_index),
            expected.column_names[column_index],
            expected.context
        );
    }
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    failures += expect_int64(mylite_result_affected_rows(result), 0, expected.context);
    failures += expect_size(mylite_result_warning_count(result), 0U, expected.context);

    if (expected.values != NULL) {
        for (size_t row_index = 0U; row_index < expected.row_count; ++row_index) {
            for (size_t column_index = 0U; column_index < expected.column_count; ++column_index) {
                const char *expected_value =
                    expected.values[(row_index * expected.column_count) + column_index];
                const char *actual_value =
                    mylite_result_value_text(result, row_index, column_index);

                if (expected_value == expected_datetime_value) {
                    failures += expect_datetime_text(actual_value, expected.context);
                } else {
                    failures += expect_text_or_null(actual_value, expected_value, expected.context);
                }
            }
        }
    }

    mylite_result_free(result);
    return failures;
}

static int expect_query_contains(mylite_db *database, struct expected_query_contains expected) {
    mylite_result *result = NULL;
    int failures = 0;
    int rc = mylite_execute(database, expected.sql, strlen(expected.sql), &result);

    if (rc != MYLITE_OK) {
        fprintf(
            stderr,
            "%s: expected OK, got %d / %d %s %s\n",
            expected.context,
            rc,
            mylite_errcode(database),
            mylite_sqlstate(database),
            mylite_errmsg(database)
        );
        mylite_result_free(result);
        return 1;
    }
    if (result == NULL) {
        fprintf(stderr, "%s: expected result object\n", expected.context);
        return 1;
    }

    failures +=
        expect_size(mylite_result_column_count(result), expected.column_count, expected.context);
    failures += expect_size(mylite_result_row_count(result), expected.row_count, expected.context);
    for (size_t column_index = 0U;
         expected.column_names != NULL && column_index < expected.column_count;
         ++column_index) {
        failures += expect_text_or_null(
            mylite_result_column_name(result, column_index),
            expected.column_names[column_index],
            expected.context
        );
    }

    if (failures == 0) {
        const char *value =
            mylite_result_value_text(result, expected.row_index, expected.column_index);

        if (value == NULL || strstr(value, expected.needle) == NULL) {
            fprintf(
                stderr,
                "%s: expected value to contain [%s], got [%s]\n",
                expected.context,
                expected.needle,
                value == NULL ? "NULL" : value
            );
            failures += 1;
        }
    }

    mylite_result_free(result);
    return failures;
}

static int expect_row_count_status(mylite_db *database, const char *context) {
    static const char *const column_names[] = {"ROW_COUNT()"};
    static const char *const values[] = {"-1"};

    return expect_query(
        database,
        (struct expected_query){
            .sql = "SELECT ROW_COUNT()",
            .column_names = column_names,
            .column_count = sizeof(column_names) / sizeof(column_names[0]),
            .values = values,
            .row_count = 1U,
            .context = context,
        }
    );
}

static int make_test_path(char *path, size_t path_size) {
    int written = snprintf(
        path,
        path_size,
        "/tmp/mylite_runtime_sys_metrics_view_%d.mylite",
        current_process_id()
    );

    if (written < 0 || (size_t)written >= path_size) {
        fprintf(stderr, "test path buffer too small\n");
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
    char buffer[test_path_capacity];
    int written = snprintf(buffer, sizeof(buffer), "%s%s", path, suffix);

    if (written >= 0 && (size_t)written < sizeof(buffer)) {
        (void)remove(buffer);
    }
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
    if (actual == NULL || expected == NULL) {
        if (actual != expected) {
            fprintf(
                stderr,
                "%s: expected %s, got %s\n",
                context,
                expected == NULL ? "NULL" : expected,
                actual == NULL ? "NULL" : actual
            );
            return 1;
        }
        return 0;
    }
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "%s: expected [%s], got [%s]\n", context, expected, actual);
        return 1;
    }
    return 0;
}

static int expect_datetime_text(const char *actual, const char *context) {
    if (actual == NULL) {
        fprintf(stderr, "%s: expected datetime text, got NULL\n", context);
        return 1;
    }
    if (strlen(actual) != datetime_text_length) {
        fprintf(
            stderr,
            "%s: expected datetime length %d, got [%s]\n",
            context,
            datetime_text_length,
            actual
        );
        return 1;
    }
    if (actual[datetime_year_month_separator] != '-' ||
        actual[datetime_month_day_separator] != '-' ||
        actual[datetime_date_time_separator] != ' ' ||
        actual[datetime_hour_minute_separator] != ':' ||
        actual[datetime_minute_second_separator] != ':') {
        fprintf(stderr, "%s: expected datetime shape, got [%s]\n", context, actual);
        return 1;
    }
    return 0;
}
