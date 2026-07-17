#include "mylite_execution_catalog_sys_summary_tables_internal.h"

#define REPEAT_2(value) value, value
#define REPEAT_4(value) REPEAT_2(value), REPEAT_2(value)
#define REPEAT_8(value) REPEAT_4(value), REPEAT_4(value)
#define REPEAT_10(value) REPEAT_8(value), REPEAT_2(value)
#define REPEAT_11(value) REPEAT_10(value), value
#define REPEAT_13(value) REPEAT_10(value), REPEAT_2(value), value
#define REPEAT_14(value) REPEAT_10(value), REPEAT_4(value)
#define REPEAT_16(value) REPEAT_8(value), REPEAT_8(value)
#define REPEAT_26(value) REPEAT_16(value), REPEAT_10(value)
#define REPEAT_27(value) REPEAT_26(value), value

#define SYS_STATEMENT_QUERY_COLUMN                                                                 \
    {"query",                                                                                      \
     NULL,                                                                                         \
     "YES",                                                                                        \
     "longtext",                                                                                   \
     "4294967295",                                                                                 \
     "4294967295",                                                                                 \
     NULL,                                                                                         \
     NULL,                                                                                         \
     NULL,                                                                                         \
     "utf8mb4",                                                                                    \
     "utf8mb4_0900_ai_ci",                                                                         \
     "longtext"}

#define SYS_STATEMENT_DB_COLUMN                                                                    \
    {"db",                                                                                         \
     NULL,                                                                                         \
     "YES",                                                                                        \
     "varchar",                                                                                    \
     "64",                                                                                         \
     "256",                                                                                        \
     NULL,                                                                                         \
     NULL,                                                                                         \
     NULL,                                                                                         \
     "utf8mb4",                                                                                    \
     "utf8mb4_0900_ai_ci",                                                                         \
     "varchar(64)"}

#define SYS_STATEMENT_FULL_SCAN_COLUMN                                                             \
    {"full_scan",                                                                                  \
     "",                                                                                           \
     "NO",                                                                                         \
     "varchar",                                                                                    \
     "1",                                                                                          \
     "4",                                                                                          \
     NULL,                                                                                         \
     NULL,                                                                                         \
     NULL,                                                                                         \
     "utf8mb4",                                                                                    \
     "utf8mb4_0900_ai_ci",                                                                         \
     "varchar(1)"}

#define SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN(name)                                                 \
    {name, NULL, "NO", "bigint", NULL, NULL, "20", "0", NULL, NULL, NULL, "bigint unsigned"}

#define SYS_STATEMENT_FORMATTED_TIME_COLUMN(name)                                                  \
    {name,                                                                                         \
     NULL,                                                                                         \
     "YES",                                                                                        \
     "varchar",                                                                                    \
     "11",                                                                                         \
     "33",                                                                                         \
     NULL,                                                                                         \
     NULL,                                                                                         \
     NULL,                                                                                         \
     "utf8mb3",                                                                                    \
     "utf8mb3_general_ci",                                                                         \
     "varchar(11)"}

#define SYS_STATEMENT_FORMATTED_BYTES_COLUMN(name) SYS_STATEMENT_FORMATTED_TIME_COLUMN(name)

#define SYS_STATEMENT_DECIMAL_21_NOT_NULL(name)                                                    \
    {name, "0", "NO", "decimal", NULL, NULL, "21", "0", NULL, NULL, NULL, "decimal(21,0)"}

#define SYS_STATEMENT_DECIMAL_21_NULL(name)                                                        \
    {name, NULL, "YES", "decimal", NULL, NULL, "21", "0", NULL, NULL, NULL, "decimal(21,0)"}

#define SYS_STATEMENT_DECIMAL_24_NOT_NULL(name)                                                    \
    {name, "0", "NO", "decimal", NULL, NULL, "24", "0", NULL, NULL, NULL, "decimal(24,0)"}

#define SYS_STATEMENT_DECIMAL_27_4_NOT_NULL(name)                                                  \
    {name, "0.0000", "NO", "decimal", NULL, NULL, "27", "4", NULL, NULL, NULL, "decimal(27,4)"}

#define SYS_STATEMENT_DIGEST_COLUMN                                                                \
    {"digest",                                                                                     \
     NULL,                                                                                         \
     "YES",                                                                                        \
     "varchar",                                                                                    \
     "64",                                                                                         \
     "256",                                                                                        \
     NULL,                                                                                         \
     NULL,                                                                                         \
     NULL,                                                                                         \
     "utf8mb4",                                                                                    \
     "utf8mb4_0900_ai_ci",                                                                         \
     "varchar(64)"}

#define SYS_STATEMENT_TIMESTAMP_6_COLUMN(name)                                                     \
    {name, NULL, "NO", "timestamp", NULL, NULL, NULL, NULL, "6", NULL, NULL, "timestamp(6)"}

static const struct mylite_execution_catalog_column_definition sys_statement_analysis_columns[] = {
    SYS_STATEMENT_QUERY_COLUMN,
    SYS_STATEMENT_DB_COLUMN,
    SYS_STATEMENT_FULL_SCAN_COLUMN,
    SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("exec_count"),
    SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("err_count"),
    SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("warn_count"),
    SYS_STATEMENT_FORMATTED_TIME_COLUMN("total_latency"),
    SYS_STATEMENT_FORMATTED_TIME_COLUMN("max_latency"),
    SYS_STATEMENT_FORMATTED_TIME_COLUMN("avg_latency"),
    SYS_STATEMENT_FORMATTED_TIME_COLUMN("lock_latency"),
    SYS_STATEMENT_FORMATTED_TIME_COLUMN("cpu_latency"),
    SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("rows_sent"),
    SYS_STATEMENT_DECIMAL_21_NOT_NULL("rows_sent_avg"),
    SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("rows_examined"),
    SYS_STATEMENT_DECIMAL_21_NOT_NULL("rows_examined_avg"),
    SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("rows_affected"),
    SYS_STATEMENT_DECIMAL_21_NOT_NULL("rows_affected_avg"),
    SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("tmp_tables"),
    SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("tmp_disk_tables"),
    SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("rows_sorted"),
    SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("sort_merge_passes"),
    SYS_STATEMENT_FORMATTED_BYTES_COLUMN("max_controlled_memory"),
    SYS_STATEMENT_FORMATTED_BYTES_COLUMN("max_total_memory"),
    SYS_STATEMENT_DIGEST_COLUMN,
    SYS_STATEMENT_TIMESTAMP_6_COLUMN("first_seen"),
    SYS_STATEMENT_TIMESTAMP_6_COLUMN("last_seen"),
};

static const struct mylite_execution_catalog_column_definition sys_x_statement_analysis_columns[] =
    {
        SYS_STATEMENT_QUERY_COLUMN,
        SYS_STATEMENT_DB_COLUMN,
        SYS_STATEMENT_FULL_SCAN_COLUMN,
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("exec_count"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("exec_secondary_count"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("err_count"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("warn_count"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("total_latency"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("max_latency"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("avg_latency"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("lock_latency"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("cpu_latency"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("rows_sent"),
        SYS_STATEMENT_DECIMAL_21_NOT_NULL("rows_sent_avg"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("rows_examined"),
        SYS_STATEMENT_DECIMAL_21_NOT_NULL("rows_examined_avg"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("rows_affected"),
        SYS_STATEMENT_DECIMAL_21_NOT_NULL("rows_affected_avg"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("tmp_tables"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("tmp_disk_tables"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("rows_sorted"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("sort_merge_passes"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("max_controlled_memory"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("max_total_memory"),
        SYS_STATEMENT_DIGEST_COLUMN,
        SYS_STATEMENT_TIMESTAMP_6_COLUMN("first_seen"),
        SYS_STATEMENT_TIMESTAMP_6_COLUMN("last_seen"),
};

static const struct mylite_execution_catalog_column_definition
    sys_statements_with_errors_or_warnings_columns[] = {
        SYS_STATEMENT_QUERY_COLUMN,
        SYS_STATEMENT_DB_COLUMN,
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("exec_count"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("errors"),
        SYS_STATEMENT_DECIMAL_27_4_NOT_NULL("error_pct"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("warnings"),
        SYS_STATEMENT_DECIMAL_27_4_NOT_NULL("warning_pct"),
        SYS_STATEMENT_TIMESTAMP_6_COLUMN("first_seen"),
        SYS_STATEMENT_TIMESTAMP_6_COLUMN("last_seen"),
        SYS_STATEMENT_DIGEST_COLUMN,
};

static const struct mylite_execution_catalog_column_definition
    sys_statements_with_full_table_scans_columns[] = {
        SYS_STATEMENT_QUERY_COLUMN,
        SYS_STATEMENT_DB_COLUMN,
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("exec_count"),
        SYS_STATEMENT_FORMATTED_TIME_COLUMN("total_latency"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("no_index_used_count"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("no_good_index_used_count"),
        SYS_STATEMENT_DECIMAL_24_NOT_NULL("no_index_used_pct"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("rows_sent"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("rows_examined"),
        SYS_STATEMENT_DECIMAL_21_NULL("rows_sent_avg"),
        SYS_STATEMENT_DECIMAL_21_NULL("rows_examined_avg"),
        SYS_STATEMENT_TIMESTAMP_6_COLUMN("first_seen"),
        SYS_STATEMENT_TIMESTAMP_6_COLUMN("last_seen"),
        SYS_STATEMENT_DIGEST_COLUMN,
};

static const struct mylite_execution_catalog_column_definition
    sys_x_statements_with_full_table_scans_columns[] = {
        SYS_STATEMENT_QUERY_COLUMN,
        SYS_STATEMENT_DB_COLUMN,
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("exec_count"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("total_latency"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("no_index_used_count"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("no_good_index_used_count"),
        SYS_STATEMENT_DECIMAL_24_NOT_NULL("no_index_used_pct"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("rows_sent"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("rows_examined"),
        SYS_STATEMENT_DECIMAL_21_NULL("rows_sent_avg"),
        SYS_STATEMENT_DECIMAL_21_NULL("rows_examined_avg"),
        SYS_STATEMENT_TIMESTAMP_6_COLUMN("first_seen"),
        SYS_STATEMENT_TIMESTAMP_6_COLUMN("last_seen"),
        SYS_STATEMENT_DIGEST_COLUMN,
};

static const struct mylite_execution_catalog_column_definition
    sys_statements_with_runtimes_in_95th_percentile_columns[] = {
        SYS_STATEMENT_QUERY_COLUMN,
        SYS_STATEMENT_DB_COLUMN,
        SYS_STATEMENT_FULL_SCAN_COLUMN,
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("exec_count"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("err_count"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("warn_count"),
        SYS_STATEMENT_FORMATTED_TIME_COLUMN("total_latency"),
        SYS_STATEMENT_FORMATTED_TIME_COLUMN("max_latency"),
        SYS_STATEMENT_FORMATTED_TIME_COLUMN("avg_latency"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("rows_sent"),
        SYS_STATEMENT_DECIMAL_21_NOT_NULL("rows_sent_avg"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("rows_examined"),
        SYS_STATEMENT_DECIMAL_21_NOT_NULL("rows_examined_avg"),
        SYS_STATEMENT_TIMESTAMP_6_COLUMN("first_seen"),
        SYS_STATEMENT_TIMESTAMP_6_COLUMN("last_seen"),
        SYS_STATEMENT_DIGEST_COLUMN,
};

static const struct mylite_execution_catalog_column_definition
    sys_x_statements_with_runtimes_in_95th_percentile_columns[] = {
        SYS_STATEMENT_QUERY_COLUMN,
        SYS_STATEMENT_DB_COLUMN,
        SYS_STATEMENT_FULL_SCAN_COLUMN,
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("exec_count"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("err_count"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("warn_count"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("total_latency"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("max_latency"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("avg_latency"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("rows_sent"),
        SYS_STATEMENT_DECIMAL_21_NOT_NULL("rows_sent_avg"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("rows_examined"),
        SYS_STATEMENT_DECIMAL_21_NOT_NULL("rows_examined_avg"),
        SYS_STATEMENT_TIMESTAMP_6_COLUMN("first_seen"),
        SYS_STATEMENT_TIMESTAMP_6_COLUMN("last_seen"),
        SYS_STATEMENT_DIGEST_COLUMN,
};

static const struct mylite_execution_catalog_column_definition
    sys_statements_with_sorting_columns[] = {
        SYS_STATEMENT_QUERY_COLUMN,
        SYS_STATEMENT_DB_COLUMN,
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("exec_count"),
        SYS_STATEMENT_FORMATTED_TIME_COLUMN("total_latency"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("sort_merge_passes"),
        SYS_STATEMENT_DECIMAL_21_NOT_NULL("avg_sort_merges"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("sorts_using_scans"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("sort_using_range"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("rows_sorted"),
        SYS_STATEMENT_DECIMAL_21_NOT_NULL("avg_rows_sorted"),
        SYS_STATEMENT_TIMESTAMP_6_COLUMN("first_seen"),
        SYS_STATEMENT_TIMESTAMP_6_COLUMN("last_seen"),
        SYS_STATEMENT_DIGEST_COLUMN,
};

static const struct mylite_execution_catalog_column_definition
    sys_x_statements_with_sorting_columns[] = {
        SYS_STATEMENT_QUERY_COLUMN,
        SYS_STATEMENT_DB_COLUMN,
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("exec_count"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("total_latency"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("sort_merge_passes"),
        SYS_STATEMENT_DECIMAL_21_NOT_NULL("avg_sort_merges"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("sorts_using_scans"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("sort_using_range"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("rows_sorted"),
        SYS_STATEMENT_DECIMAL_21_NOT_NULL("avg_rows_sorted"),
        SYS_STATEMENT_TIMESTAMP_6_COLUMN("first_seen"),
        SYS_STATEMENT_TIMESTAMP_6_COLUMN("last_seen"),
        SYS_STATEMENT_DIGEST_COLUMN,
};

static const struct mylite_execution_catalog_column_definition
    sys_statements_with_temp_tables_columns[] = {
        SYS_STATEMENT_QUERY_COLUMN,
        SYS_STATEMENT_DB_COLUMN,
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("exec_count"),
        SYS_STATEMENT_FORMATTED_TIME_COLUMN("total_latency"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("memory_tmp_tables"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("disk_tmp_tables"),
        SYS_STATEMENT_DECIMAL_21_NOT_NULL("avg_tmp_tables_per_query"),
        SYS_STATEMENT_DECIMAL_24_NOT_NULL("tmp_tables_to_disk_pct"),
        SYS_STATEMENT_TIMESTAMP_6_COLUMN("first_seen"),
        SYS_STATEMENT_TIMESTAMP_6_COLUMN("last_seen"),
        SYS_STATEMENT_DIGEST_COLUMN,
};

static const struct mylite_execution_catalog_column_definition
    sys_x_statements_with_temp_tables_columns[] = {
        SYS_STATEMENT_QUERY_COLUMN,
        SYS_STATEMENT_DB_COLUMN,
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("exec_count"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("total_latency"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("memory_tmp_tables"),
        SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN("disk_tmp_tables"),
        SYS_STATEMENT_DECIMAL_21_NOT_NULL("avg_tmp_tables_per_query"),
        SYS_STATEMENT_DECIMAL_24_NOT_NULL("tmp_tables_to_disk_pct"),
        SYS_STATEMENT_TIMESTAMP_6_COLUMN("first_seen"),
        SYS_STATEMENT_TIMESTAMP_6_COLUMN("last_seen"),
        SYS_STATEMENT_DIGEST_COLUMN,
};

#undef SYS_STATEMENT_QUERY_COLUMN
#undef SYS_STATEMENT_DB_COLUMN
#undef SYS_STATEMENT_FULL_SCAN_COLUMN
#undef SYS_STATEMENT_UNSIGNED_BIGINT_COLUMN
#undef SYS_STATEMENT_FORMATTED_TIME_COLUMN
#undef SYS_STATEMENT_FORMATTED_BYTES_COLUMN
#undef SYS_STATEMENT_DECIMAL_21_NOT_NULL
#undef SYS_STATEMENT_DECIMAL_21_NULL
#undef SYS_STATEMENT_DECIMAL_24_NOT_NULL
#undef SYS_STATEMENT_DECIMAL_27_4_NOT_NULL
#undef SYS_STATEMENT_DIGEST_COLUMN
#undef SYS_STATEMENT_TIMESTAMP_6_COLUMN

static const char *const sys_statement_analysis_column_keys[] = {REPEAT_26("")};
static const char *const sys_statement_analysis_column_extras[] = {REPEAT_26("")};
static const char *const sys_statement_analysis_column_privileges[] = {
    REPEAT_26("select,insert,update,references")
};

static const char *const sys_x_statement_analysis_column_keys[] = {REPEAT_27("")};
static const char *const sys_x_statement_analysis_column_extras[] = {REPEAT_27("")};
static const char *const sys_x_statement_analysis_column_privileges[] = {
    REPEAT_27("select,insert,update,references")
};

static const char *const sys_statements_with_errors_or_warnings_column_keys[] = {REPEAT_10("")};
static const char *const sys_statements_with_errors_or_warnings_column_extras[] = {REPEAT_10("")};
static const char *const sys_statements_with_errors_or_warnings_column_privileges[] = {
    REPEAT_10("select,insert,update,references")
};

static const char *const sys_statements_with_full_table_scans_column_keys[] = {REPEAT_14("")};
static const char *const sys_statements_with_full_table_scans_column_extras[] = {REPEAT_14("")};
static const char *const sys_statements_with_full_table_scans_column_privileges[] = {
    REPEAT_14("select,insert,update,references")
};

static const char *const sys_statements_with_runtimes_in_95th_percentile_column_keys[] = {
    REPEAT_16("")
};
static const char *const sys_statements_with_runtimes_in_95th_percentile_column_extras[] = {
    REPEAT_16("")
};
static const char *const sys_statements_with_runtimes_in_95th_percentile_column_privileges[] = {
    REPEAT_16("select,insert,update,references")
};

static const char *const sys_statements_with_sorting_column_keys[] = {REPEAT_13("")};
static const char *const sys_statements_with_sorting_column_extras[] = {REPEAT_13("")};
static const char *const sys_statements_with_sorting_column_privileges[] = {
    REPEAT_13("select,insert,update,references")
};

static const char *const sys_statements_with_temp_tables_column_keys[] = {REPEAT_11("")};
static const char *const sys_statements_with_temp_tables_column_extras[] = {REPEAT_11("")};
static const char *const sys_statements_with_temp_tables_column_privileges[] = {
    REPEAT_11("select,insert,update,references")
};

static const struct mylite_execution_catalog_mysql_system_table
    sys_summary_statement_system_table_definitions[] = {
        {"sys",
         {MYLITE_EXECUTION_CATALOG_TABLE_SYS_STATEMENT_ANALYSIS,
          "statement_analysis",
          sys_statement_analysis_columns,
          sizeof(sys_statement_analysis_columns) / sizeof(sys_statement_analysis_columns[0])},
         MYLITE_EXECUTION_CATALOG_ARRAY(sys_statement_analysis_column_keys),
         MYLITE_EXECUTION_CATALOG_ARRAY(sys_statement_analysis_column_extras),
         MYLITE_EXECUTION_CATALOG_ARRAY(sys_statement_analysis_column_privileges),
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U,
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U},
        {"sys",
         {MYLITE_EXECUTION_CATALOG_TABLE_SYS_STATEMENTS_WITH_ERRORS_OR_WARNINGS,
          "statements_with_errors_or_warnings",
          sys_statements_with_errors_or_warnings_columns,
          sizeof(sys_statements_with_errors_or_warnings_columns) /
              sizeof(sys_statements_with_errors_or_warnings_columns[0])},
         MYLITE_EXECUTION_CATALOG_ARRAY(sys_statements_with_errors_or_warnings_column_keys),
         MYLITE_EXECUTION_CATALOG_ARRAY(sys_statements_with_errors_or_warnings_column_extras),
         MYLITE_EXECUTION_CATALOG_ARRAY(sys_statements_with_errors_or_warnings_column_privileges),
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U,
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U},
        {"sys",
         {MYLITE_EXECUTION_CATALOG_TABLE_SYS_STATEMENTS_WITH_FULL_TABLE_SCANS,
          "statements_with_full_table_scans",
          sys_statements_with_full_table_scans_columns,
          sizeof(sys_statements_with_full_table_scans_columns) /
              sizeof(sys_statements_with_full_table_scans_columns[0])},
         MYLITE_EXECUTION_CATALOG_ARRAY(sys_statements_with_full_table_scans_column_keys),
         MYLITE_EXECUTION_CATALOG_ARRAY(sys_statements_with_full_table_scans_column_extras),
         MYLITE_EXECUTION_CATALOG_ARRAY(sys_statements_with_full_table_scans_column_privileges),
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U,
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U},
        {"sys",
         {MYLITE_EXECUTION_CATALOG_TABLE_SYS_STATEMENTS_WITH_RUNTIMES_IN_95TH_PERCENTILE,
          "statements_with_runtimes_in_95th_percentile",
          sys_statements_with_runtimes_in_95th_percentile_columns,
          sizeof(sys_statements_with_runtimes_in_95th_percentile_columns) /
              sizeof(sys_statements_with_runtimes_in_95th_percentile_columns[0])},
         MYLITE_EXECUTION_CATALOG_ARRAY(
             sys_statements_with_runtimes_in_95th_percentile_column_keys
         ),
         MYLITE_EXECUTION_CATALOG_ARRAY(
             sys_statements_with_runtimes_in_95th_percentile_column_extras
         ),
         MYLITE_EXECUTION_CATALOG_ARRAY(
             sys_statements_with_runtimes_in_95th_percentile_column_privileges
         ),
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U,
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U},
        {"sys",
         {MYLITE_EXECUTION_CATALOG_TABLE_SYS_STATEMENTS_WITH_SORTING,
          "statements_with_sorting",
          sys_statements_with_sorting_columns,
          sizeof(sys_statements_with_sorting_columns) /
              sizeof(sys_statements_with_sorting_columns[0])},
         MYLITE_EXECUTION_CATALOG_ARRAY(sys_statements_with_sorting_column_keys),
         MYLITE_EXECUTION_CATALOG_ARRAY(sys_statements_with_sorting_column_extras),
         MYLITE_EXECUTION_CATALOG_ARRAY(sys_statements_with_sorting_column_privileges),
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U,
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U},
        {"sys",
         {MYLITE_EXECUTION_CATALOG_TABLE_SYS_STATEMENTS_WITH_TEMP_TABLES,
          "statements_with_temp_tables",
          sys_statements_with_temp_tables_columns,
          sizeof(sys_statements_with_temp_tables_columns) /
              sizeof(sys_statements_with_temp_tables_columns[0])},
         MYLITE_EXECUTION_CATALOG_ARRAY(sys_statements_with_temp_tables_column_keys),
         MYLITE_EXECUTION_CATALOG_ARRAY(sys_statements_with_temp_tables_column_extras),
         MYLITE_EXECUTION_CATALOG_ARRAY(sys_statements_with_temp_tables_column_privileges),
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U,
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U},
};

static const struct mylite_execution_catalog_mysql_system_table
    sys_summary_statement_x_system_table_definitions[] = {
        {"sys",
         {MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_STATEMENT_ANALYSIS,
          "x$statement_analysis",
          sys_x_statement_analysis_columns,
          sizeof(sys_x_statement_analysis_columns) / sizeof(sys_x_statement_analysis_columns[0])},
         MYLITE_EXECUTION_CATALOG_ARRAY(sys_x_statement_analysis_column_keys),
         MYLITE_EXECUTION_CATALOG_ARRAY(sys_x_statement_analysis_column_extras),
         MYLITE_EXECUTION_CATALOG_ARRAY(sys_x_statement_analysis_column_privileges),
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U,
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U},
        {"sys",
         {MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_STATEMENTS_WITH_ERRORS_OR_WARNINGS,
          "x$statements_with_errors_or_warnings",
          sys_statements_with_errors_or_warnings_columns,
          sizeof(sys_statements_with_errors_or_warnings_columns) /
              sizeof(sys_statements_with_errors_or_warnings_columns[0])},
         MYLITE_EXECUTION_CATALOG_ARRAY(sys_statements_with_errors_or_warnings_column_keys),
         MYLITE_EXECUTION_CATALOG_ARRAY(sys_statements_with_errors_or_warnings_column_extras),
         MYLITE_EXECUTION_CATALOG_ARRAY(sys_statements_with_errors_or_warnings_column_privileges),
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U,
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U},
        {"sys",
         {MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_STATEMENTS_WITH_FULL_TABLE_SCANS,
          "x$statements_with_full_table_scans",
          sys_x_statements_with_full_table_scans_columns,
          sizeof(sys_x_statements_with_full_table_scans_columns) /
              sizeof(sys_x_statements_with_full_table_scans_columns[0])},
         MYLITE_EXECUTION_CATALOG_ARRAY(sys_statements_with_full_table_scans_column_keys),
         MYLITE_EXECUTION_CATALOG_ARRAY(sys_statements_with_full_table_scans_column_extras),
         MYLITE_EXECUTION_CATALOG_ARRAY(sys_statements_with_full_table_scans_column_privileges),
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U,
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U},
        {"sys",
         {MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_STATEMENTS_WITH_RUNTIMES_IN_95TH_PERCENTILE,
          "x$statements_with_runtimes_in_95th_percentile",
          sys_x_statements_with_runtimes_in_95th_percentile_columns,
          sizeof(sys_x_statements_with_runtimes_in_95th_percentile_columns) /
              sizeof(sys_x_statements_with_runtimes_in_95th_percentile_columns[0])},
         MYLITE_EXECUTION_CATALOG_ARRAY(
             sys_statements_with_runtimes_in_95th_percentile_column_keys
         ),
         MYLITE_EXECUTION_CATALOG_ARRAY(
             sys_statements_with_runtimes_in_95th_percentile_column_extras
         ),
         MYLITE_EXECUTION_CATALOG_ARRAY(
             sys_statements_with_runtimes_in_95th_percentile_column_privileges
         ),
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U,
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U},
        {"sys",
         {MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_STATEMENTS_WITH_SORTING,
          "x$statements_with_sorting",
          sys_x_statements_with_sorting_columns,
          sizeof(sys_x_statements_with_sorting_columns) /
              sizeof(sys_x_statements_with_sorting_columns[0])},
         MYLITE_EXECUTION_CATALOG_ARRAY(sys_statements_with_sorting_column_keys),
         MYLITE_EXECUTION_CATALOG_ARRAY(sys_statements_with_sorting_column_extras),
         MYLITE_EXECUTION_CATALOG_ARRAY(sys_statements_with_sorting_column_privileges),
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U,
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U},
        {"sys",
         {MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_STATEMENTS_WITH_TEMP_TABLES,
          "x$statements_with_temp_tables",
          sys_x_statements_with_temp_tables_columns,
          sizeof(sys_x_statements_with_temp_tables_columns) /
              sizeof(sys_x_statements_with_temp_tables_columns[0])},
         MYLITE_EXECUTION_CATALOG_ARRAY(sys_statements_with_temp_tables_column_keys),
         MYLITE_EXECUTION_CATALOG_ARRAY(sys_statements_with_temp_tables_column_extras),
         MYLITE_EXECUTION_CATALOG_ARRAY(sys_statements_with_temp_tables_column_privileges),
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U,
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U},
};

#undef REPEAT_2
#undef REPEAT_4
#undef REPEAT_8
#undef REPEAT_10
#undef REPEAT_11
#undef REPEAT_13
#undef REPEAT_14
#undef REPEAT_16
#undef REPEAT_26
#undef REPEAT_27

size_t mylite_execution_catalog_sys_summary_statement_system_table_definition_count(void) {
    return sizeof(sys_summary_statement_system_table_definitions) /
           sizeof(sys_summary_statement_system_table_definitions[0]);
}

const struct mylite_execution_catalog_mysql_system_table *mylite_execution_catalog_sys_summary_statement_system_table_definition_at(
    size_t index
) {
    if (index >= mylite_execution_catalog_sys_summary_statement_system_table_definition_count()) {
        return NULL;
    }
    return &sys_summary_statement_system_table_definitions[index];
}

size_t mylite_execution_catalog_sys_summary_statement_x_system_table_definition_count(void) {
    return sizeof(sys_summary_statement_x_system_table_definitions) /
           sizeof(sys_summary_statement_x_system_table_definitions[0]);
}

const struct mylite_execution_catalog_mysql_system_table *mylite_execution_catalog_sys_summary_statement_x_system_table_definition_at(
    size_t index
) {
    if (index >= mylite_execution_catalog_sys_summary_statement_x_system_table_definition_count()) {
        return NULL;
    }
    return &sys_summary_statement_x_system_table_definitions[index];
}
