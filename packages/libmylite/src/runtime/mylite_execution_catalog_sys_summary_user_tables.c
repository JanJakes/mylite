#include "mylite_execution_catalog_sys_summary_tables_internal.h"

#define SYS_USER_COLUMN                                                                            \
    {"user",                                                                                       \
     NULL,                                                                                         \
     "YES",                                                                                        \
     "varchar",                                                                                    \
     "32",                                                                                         \
     "128",                                                                                        \
     NULL,                                                                                         \
     NULL,                                                                                         \
     NULL,                                                                                         \
     "utf8mb4",                                                                                    \
     "utf8mb4_bin",                                                                                \
     "varchar(32)"}

#define SYS_EVENT_NAME_COLUMN                                                                      \
    {"event_name",                                                                                 \
     NULL,                                                                                         \
     "NO",                                                                                         \
     "varchar",                                                                                    \
     "128",                                                                                        \
     "512",                                                                                        \
     NULL,                                                                                         \
     NULL,                                                                                         \
     NULL,                                                                                         \
     "utf8mb4",                                                                                    \
     "utf8mb4_0900_ai_ci",                                                                         \
     "varchar(128)"}

#define SYS_STATEMENT_COLUMN                                                                       \
    {"statement",                                                                                  \
     NULL,                                                                                         \
     "YES",                                                                                        \
     "varchar",                                                                                    \
     "128",                                                                                        \
     "512",                                                                                        \
     NULL,                                                                                         \
     NULL,                                                                                         \
     NULL,                                                                                         \
     "utf8mb4",                                                                                    \
     "utf8mb4_0900_ai_ci",                                                                         \
     "varchar(128)"}

#define SYS_FORMATTED_LATENCY_COLUMN(name)                                                         \
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

#define SYS_DECIMAL_COLUMN(name, default_value, nullable, precision, scale, type_text)             \
    {name,                                                                                         \
     default_value,                                                                                \
     nullable,                                                                                     \
     "decimal",                                                                                    \
     NULL,                                                                                         \
     NULL,                                                                                         \
     precision,                                                                                    \
     scale,                                                                                        \
     NULL,                                                                                         \
     NULL,                                                                                         \
     NULL,                                                                                         \
     type_text}

#define SYS_BIGINT_COLUMN(name, default_value, nullable)                                           \
    {name, default_value, nullable, "bigint", NULL, NULL, "19", "0", NULL, NULL, NULL, "bigint"}

#define SYS_BIGINT_UNSIGNED_COLUMN(name, default_value, nullable)                                  \
    {name,                                                                                         \
     default_value,                                                                                \
     nullable,                                                                                     \
     "bigint",                                                                                     \
     NULL,                                                                                         \
     NULL,                                                                                         \
     "20",                                                                                         \
     "0",                                                                                          \
     NULL,                                                                                         \
     NULL,                                                                                         \
     NULL,                                                                                         \
     "bigint unsigned"}

static const struct mylite_execution_catalog_column_definition sys_user_summary_columns[] = {
    SYS_USER_COLUMN,
    SYS_DECIMAL_COLUMN("statements", NULL, "YES", "64", "0", "decimal(64,0)"),
    SYS_FORMATTED_LATENCY_COLUMN("statement_latency"),
    SYS_FORMATTED_LATENCY_COLUMN("statement_avg_latency"),
    SYS_DECIMAL_COLUMN("table_scans", NULL, "YES", "65", "0", "decimal(65,0)"),
    SYS_DECIMAL_COLUMN("file_ios", NULL, "YES", "64", "0", "decimal(64,0)"),
    SYS_FORMATTED_LATENCY_COLUMN("file_io_latency"),
    SYS_DECIMAL_COLUMN("current_connections", NULL, "YES", "41", "0", "decimal(41,0)"),
    SYS_DECIMAL_COLUMN("total_connections", NULL, "YES", "41", "0", "decimal(41,0)"),
    SYS_BIGINT_COLUMN("unique_hosts", "0", "NO"),
    SYS_FORMATTED_LATENCY_COLUMN("current_memory"),
    SYS_FORMATTED_LATENCY_COLUMN("total_memory_allocated"),
};

static const struct mylite_execution_catalog_column_definition sys_x_user_summary_columns[] = {
    SYS_USER_COLUMN,
    SYS_DECIMAL_COLUMN("statements", NULL, "YES", "64", "0", "decimal(64,0)"),
    SYS_DECIMAL_COLUMN("statement_latency", NULL, "YES", "64", "0", "decimal(64,0)"),
    SYS_DECIMAL_COLUMN("statement_avg_latency", "0.0000", "NO", "65", "4", "decimal(65,4)"),
    SYS_DECIMAL_COLUMN("table_scans", NULL, "YES", "65", "0", "decimal(65,0)"),
    SYS_DECIMAL_COLUMN("file_ios", NULL, "YES", "64", "0", "decimal(64,0)"),
    SYS_DECIMAL_COLUMN("file_io_latency", NULL, "YES", "64", "0", "decimal(64,0)"),
    SYS_DECIMAL_COLUMN("current_connections", NULL, "YES", "41", "0", "decimal(41,0)"),
    SYS_DECIMAL_COLUMN("total_connections", NULL, "YES", "41", "0", "decimal(41,0)"),
    SYS_BIGINT_COLUMN("unique_hosts", "0", "NO"),
    SYS_DECIMAL_COLUMN("current_memory", NULL, "YES", "63", "0", "decimal(63,0)"),
    SYS_DECIMAL_COLUMN("total_memory_allocated", NULL, "YES", "64", "0", "decimal(64,0)"),
};

static const struct mylite_execution_catalog_column_definition
    sys_user_summary_by_file_io_columns[] = {
        SYS_USER_COLUMN,
        SYS_DECIMAL_COLUMN("ios", NULL, "YES", "42", "0", "decimal(42,0)"),
        SYS_FORMATTED_LATENCY_COLUMN("io_latency"),
};

static const struct mylite_execution_catalog_column_definition
    sys_x_user_summary_by_file_io_columns[] = {
        SYS_USER_COLUMN,
        SYS_DECIMAL_COLUMN("ios", NULL, "YES", "42", "0", "decimal(42,0)"),
        SYS_DECIMAL_COLUMN("io_latency", NULL, "YES", "42", "0", "decimal(42,0)"),
};

static const struct mylite_execution_catalog_column_definition
    sys_user_summary_by_file_io_type_columns[] = {
        SYS_USER_COLUMN,
        SYS_EVENT_NAME_COLUMN,
        SYS_BIGINT_UNSIGNED_COLUMN("total", NULL, "NO"),
        SYS_FORMATTED_LATENCY_COLUMN("latency"),
        SYS_FORMATTED_LATENCY_COLUMN("max_latency"),
};

static const struct mylite_execution_catalog_column_definition
    sys_x_user_summary_by_file_io_type_columns[] = {
        SYS_USER_COLUMN,
        SYS_EVENT_NAME_COLUMN,
        SYS_BIGINT_UNSIGNED_COLUMN("total", NULL, "NO"),
        SYS_BIGINT_UNSIGNED_COLUMN("latency", NULL, "NO"),
        SYS_BIGINT_UNSIGNED_COLUMN("max_latency", NULL, "NO"),
};

static const struct mylite_execution_catalog_column_definition
    sys_user_summary_by_stages_columns[] = {
        SYS_USER_COLUMN,
        SYS_EVENT_NAME_COLUMN,
        SYS_BIGINT_UNSIGNED_COLUMN("total", NULL, "NO"),
        SYS_FORMATTED_LATENCY_COLUMN("total_latency"),
        SYS_FORMATTED_LATENCY_COLUMN("avg_latency"),
};

static const struct mylite_execution_catalog_column_definition
    sys_x_user_summary_by_stages_columns[] = {
        SYS_USER_COLUMN,
        SYS_EVENT_NAME_COLUMN,
        SYS_BIGINT_UNSIGNED_COLUMN("total", NULL, "NO"),
        SYS_BIGINT_UNSIGNED_COLUMN("total_latency", NULL, "NO"),
        SYS_BIGINT_UNSIGNED_COLUMN("avg_latency", NULL, "NO"),
};

static const struct mylite_execution_catalog_column_definition
    sys_user_summary_by_statement_latency_columns[] = {
        SYS_USER_COLUMN,
        SYS_DECIMAL_COLUMN("total", NULL, "YES", "42", "0", "decimal(42,0)"),
        SYS_FORMATTED_LATENCY_COLUMN("total_latency"),
        SYS_FORMATTED_LATENCY_COLUMN("max_latency"),
        SYS_FORMATTED_LATENCY_COLUMN("lock_latency"),
        SYS_FORMATTED_LATENCY_COLUMN("cpu_latency"),
        SYS_DECIMAL_COLUMN("rows_sent", NULL, "YES", "42", "0", "decimal(42,0)"),
        SYS_DECIMAL_COLUMN("rows_examined", NULL, "YES", "42", "0", "decimal(42,0)"),
        SYS_DECIMAL_COLUMN("rows_affected", NULL, "YES", "42", "0", "decimal(42,0)"),
        SYS_DECIMAL_COLUMN("full_scans", NULL, "YES", "43", "0", "decimal(43,0)"),
};

static const struct mylite_execution_catalog_column_definition
    sys_x_user_summary_by_statement_latency_columns[] = {
        SYS_USER_COLUMN,
        SYS_DECIMAL_COLUMN("total", NULL, "YES", "42", "0", "decimal(42,0)"),
        SYS_DECIMAL_COLUMN("total_latency", NULL, "YES", "42", "0", "decimal(42,0)"),
        SYS_DECIMAL_COLUMN("max_latency", NULL, "YES", "42", "0", "decimal(42,0)"),
        SYS_DECIMAL_COLUMN("lock_latency", NULL, "YES", "42", "0", "decimal(42,0)"),
        SYS_DECIMAL_COLUMN("cpu_latency", NULL, "YES", "42", "0", "decimal(42,0)"),
        SYS_DECIMAL_COLUMN("rows_sent", NULL, "YES", "42", "0", "decimal(42,0)"),
        SYS_DECIMAL_COLUMN("rows_examined", NULL, "YES", "42", "0", "decimal(42,0)"),
        SYS_DECIMAL_COLUMN("rows_affected", NULL, "YES", "42", "0", "decimal(42,0)"),
        SYS_DECIMAL_COLUMN("full_scans", NULL, "YES", "43", "0", "decimal(43,0)"),
};

static const struct mylite_execution_catalog_column_definition
    sys_user_summary_by_statement_type_columns[] = {
        SYS_USER_COLUMN,
        SYS_STATEMENT_COLUMN,
        SYS_BIGINT_UNSIGNED_COLUMN("total", NULL, "NO"),
        SYS_FORMATTED_LATENCY_COLUMN("total_latency"),
        SYS_FORMATTED_LATENCY_COLUMN("max_latency"),
        SYS_FORMATTED_LATENCY_COLUMN("lock_latency"),
        SYS_FORMATTED_LATENCY_COLUMN("cpu_latency"),
        SYS_BIGINT_UNSIGNED_COLUMN("rows_sent", NULL, "NO"),
        SYS_BIGINT_UNSIGNED_COLUMN("rows_examined", NULL, "NO"),
        SYS_BIGINT_UNSIGNED_COLUMN("rows_affected", NULL, "NO"),
        SYS_BIGINT_UNSIGNED_COLUMN("full_scans", "0", "NO"),
};

static const struct mylite_execution_catalog_column_definition
    sys_x_user_summary_by_statement_type_columns[] = {
        SYS_USER_COLUMN,
        SYS_STATEMENT_COLUMN,
        SYS_BIGINT_UNSIGNED_COLUMN("total", NULL, "NO"),
        SYS_BIGINT_UNSIGNED_COLUMN("total_latency", NULL, "NO"),
        SYS_BIGINT_UNSIGNED_COLUMN("max_latency", NULL, "NO"),
        SYS_BIGINT_UNSIGNED_COLUMN("lock_latency", NULL, "NO"),
        SYS_BIGINT_UNSIGNED_COLUMN("cpu_latency", NULL, "NO"),
        SYS_BIGINT_UNSIGNED_COLUMN("rows_sent", NULL, "NO"),
        SYS_BIGINT_UNSIGNED_COLUMN("rows_examined", NULL, "NO"),
        SYS_BIGINT_UNSIGNED_COLUMN("rows_affected", NULL, "NO"),
        SYS_BIGINT_UNSIGNED_COLUMN("full_scans", "0", "NO"),
};

#undef SYS_USER_COLUMN
#undef SYS_EVENT_NAME_COLUMN
#undef SYS_STATEMENT_COLUMN
#undef SYS_FORMATTED_LATENCY_COLUMN
#undef SYS_DECIMAL_COLUMN
#undef SYS_BIGINT_COLUMN
#undef SYS_BIGINT_UNSIGNED_COLUMN

#define EMPTY_KEY ""
#define COLUMN_EXTRA ""
#define COLUMN_PRIVILEGES "select,insert,update,references"

static const char *const column_keys_3[] = {EMPTY_KEY, EMPTY_KEY, EMPTY_KEY};
static const char *const column_extras_3[] = {COLUMN_EXTRA, COLUMN_EXTRA, COLUMN_EXTRA};
static const char *const column_privileges_3[] = {
    COLUMN_PRIVILEGES,
    COLUMN_PRIVILEGES,
    COLUMN_PRIVILEGES,
};

static const char *const column_keys_5[] = {
    EMPTY_KEY,
    EMPTY_KEY,
    EMPTY_KEY,
    EMPTY_KEY,
    EMPTY_KEY,
};
static const char *const column_extras_5[] = {
    COLUMN_EXTRA,
    COLUMN_EXTRA,
    COLUMN_EXTRA,
    COLUMN_EXTRA,
    COLUMN_EXTRA,
};
static const char *const column_privileges_5[] = {
    COLUMN_PRIVILEGES,
    COLUMN_PRIVILEGES,
    COLUMN_PRIVILEGES,
    COLUMN_PRIVILEGES,
    COLUMN_PRIVILEGES,
};

static const char *const column_keys_10[] = {
    EMPTY_KEY,
    EMPTY_KEY,
    EMPTY_KEY,
    EMPTY_KEY,
    EMPTY_KEY,
    EMPTY_KEY,
    EMPTY_KEY,
    EMPTY_KEY,
    EMPTY_KEY,
    EMPTY_KEY,
};
static const char *const column_extras_10[] = {
    COLUMN_EXTRA,
    COLUMN_EXTRA,
    COLUMN_EXTRA,
    COLUMN_EXTRA,
    COLUMN_EXTRA,
    COLUMN_EXTRA,
    COLUMN_EXTRA,
    COLUMN_EXTRA,
    COLUMN_EXTRA,
    COLUMN_EXTRA,
};
static const char *const column_privileges_10[] = {
    COLUMN_PRIVILEGES,
    COLUMN_PRIVILEGES,
    COLUMN_PRIVILEGES,
    COLUMN_PRIVILEGES,
    COLUMN_PRIVILEGES,
    COLUMN_PRIVILEGES,
    COLUMN_PRIVILEGES,
    COLUMN_PRIVILEGES,
    COLUMN_PRIVILEGES,
    COLUMN_PRIVILEGES,
};

static const char *const column_keys_11[] = {
    EMPTY_KEY,
    EMPTY_KEY,
    EMPTY_KEY,
    EMPTY_KEY,
    EMPTY_KEY,
    EMPTY_KEY,
    EMPTY_KEY,
    EMPTY_KEY,
    EMPTY_KEY,
    EMPTY_KEY,
    EMPTY_KEY,
};
static const char *const column_extras_11[] = {
    COLUMN_EXTRA,
    COLUMN_EXTRA,
    COLUMN_EXTRA,
    COLUMN_EXTRA,
    COLUMN_EXTRA,
    COLUMN_EXTRA,
    COLUMN_EXTRA,
    COLUMN_EXTRA,
    COLUMN_EXTRA,
    COLUMN_EXTRA,
    COLUMN_EXTRA,
};
static const char *const column_privileges_11[] = {
    COLUMN_PRIVILEGES,
    COLUMN_PRIVILEGES,
    COLUMN_PRIVILEGES,
    COLUMN_PRIVILEGES,
    COLUMN_PRIVILEGES,
    COLUMN_PRIVILEGES,
    COLUMN_PRIVILEGES,
    COLUMN_PRIVILEGES,
    COLUMN_PRIVILEGES,
    COLUMN_PRIVILEGES,
    COLUMN_PRIVILEGES,
};

static const char *const column_keys_12[] = {
    EMPTY_KEY,
    EMPTY_KEY,
    EMPTY_KEY,
    EMPTY_KEY,
    EMPTY_KEY,
    EMPTY_KEY,
    EMPTY_KEY,
    EMPTY_KEY,
    EMPTY_KEY,
    EMPTY_KEY,
    EMPTY_KEY,
    EMPTY_KEY,
};
static const char *const column_extras_12[] = {
    COLUMN_EXTRA,
    COLUMN_EXTRA,
    COLUMN_EXTRA,
    COLUMN_EXTRA,
    COLUMN_EXTRA,
    COLUMN_EXTRA,
    COLUMN_EXTRA,
    COLUMN_EXTRA,
    COLUMN_EXTRA,
    COLUMN_EXTRA,
    COLUMN_EXTRA,
    COLUMN_EXTRA,
};
static const char *const column_privileges_12[] = {
    COLUMN_PRIVILEGES,
    COLUMN_PRIVILEGES,
    COLUMN_PRIVILEGES,
    COLUMN_PRIVILEGES,
    COLUMN_PRIVILEGES,
    COLUMN_PRIVILEGES,
    COLUMN_PRIVILEGES,
    COLUMN_PRIVILEGES,
    COLUMN_PRIVILEGES,
    COLUMN_PRIVILEGES,
    COLUMN_PRIVILEGES,
    COLUMN_PRIVILEGES,
};

#undef EMPTY_KEY
#undef COLUMN_EXTRA
#undef COLUMN_PRIVILEGES

static const struct mylite_execution_catalog_mysql_system_table
    sys_summary_user_system_table_definitions[] = {
        {"sys",
         {MYLITE_EXECUTION_CATALOG_TABLE_SYS_USER_SUMMARY,
          "user_summary",
          sys_user_summary_columns,
          sizeof(sys_user_summary_columns) / sizeof(sys_user_summary_columns[0])},
         MYLITE_EXECUTION_CATALOG_ARRAY(column_keys_12),
         MYLITE_EXECUTION_CATALOG_ARRAY(column_extras_12),
         MYLITE_EXECUTION_CATALOG_ARRAY(column_privileges_12),
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U,
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U},
        {"sys",
         {MYLITE_EXECUTION_CATALOG_TABLE_SYS_USER_SUMMARY_BY_FILE_IO,
          "user_summary_by_file_io",
          sys_user_summary_by_file_io_columns,
          sizeof(sys_user_summary_by_file_io_columns) /
              sizeof(sys_user_summary_by_file_io_columns[0])},
         MYLITE_EXECUTION_CATALOG_ARRAY(column_keys_3),
         MYLITE_EXECUTION_CATALOG_ARRAY(column_extras_3),
         MYLITE_EXECUTION_CATALOG_ARRAY(column_privileges_3),
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U,
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U},
        {"sys",
         {MYLITE_EXECUTION_CATALOG_TABLE_SYS_USER_SUMMARY_BY_FILE_IO_TYPE,
          "user_summary_by_file_io_type",
          sys_user_summary_by_file_io_type_columns,
          sizeof(sys_user_summary_by_file_io_type_columns) /
              sizeof(sys_user_summary_by_file_io_type_columns[0])},
         MYLITE_EXECUTION_CATALOG_ARRAY(column_keys_5),
         MYLITE_EXECUTION_CATALOG_ARRAY(column_extras_5),
         MYLITE_EXECUTION_CATALOG_ARRAY(column_privileges_5),
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U,
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U},
        {"sys",
         {MYLITE_EXECUTION_CATALOG_TABLE_SYS_USER_SUMMARY_BY_STAGES,
          "user_summary_by_stages",
          sys_user_summary_by_stages_columns,
          sizeof(sys_user_summary_by_stages_columns) / sizeof(sys_user_summary_by_stages_columns[0])
         },
         MYLITE_EXECUTION_CATALOG_ARRAY(column_keys_5),
         MYLITE_EXECUTION_CATALOG_ARRAY(column_extras_5),
         MYLITE_EXECUTION_CATALOG_ARRAY(column_privileges_5),
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U,
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U},
        {"sys",
         {MYLITE_EXECUTION_CATALOG_TABLE_SYS_USER_SUMMARY_BY_STATEMENT_LATENCY,
          "user_summary_by_statement_latency",
          sys_user_summary_by_statement_latency_columns,
          sizeof(sys_user_summary_by_statement_latency_columns) /
              sizeof(sys_user_summary_by_statement_latency_columns[0])},
         MYLITE_EXECUTION_CATALOG_ARRAY(column_keys_10),
         MYLITE_EXECUTION_CATALOG_ARRAY(column_extras_10),
         MYLITE_EXECUTION_CATALOG_ARRAY(column_privileges_10),
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U,
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U},
        {"sys",
         {MYLITE_EXECUTION_CATALOG_TABLE_SYS_USER_SUMMARY_BY_STATEMENT_TYPE,
          "user_summary_by_statement_type",
          sys_user_summary_by_statement_type_columns,
          sizeof(sys_user_summary_by_statement_type_columns) /
              sizeof(sys_user_summary_by_statement_type_columns[0])},
         MYLITE_EXECUTION_CATALOG_ARRAY(column_keys_11),
         MYLITE_EXECUTION_CATALOG_ARRAY(column_extras_11),
         MYLITE_EXECUTION_CATALOG_ARRAY(column_privileges_11),
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U,
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U},
};

size_t mylite_execution_catalog_sys_summary_user_system_table_definition_count(void) {
    return sizeof(sys_summary_user_system_table_definitions) /
           sizeof(sys_summary_user_system_table_definitions[0]);
}

const struct mylite_execution_catalog_mysql_system_table *mylite_execution_catalog_sys_summary_user_system_table_definition_at(
    size_t index
) {
    if (index >= mylite_execution_catalog_sys_summary_user_system_table_definition_count()) {
        return NULL;
    }
    return &sys_summary_user_system_table_definitions[index];
}

static const struct mylite_execution_catalog_mysql_system_table
    sys_summary_user_x_system_table_definitions[] = {
        {"sys",
         {MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_USER_SUMMARY,
          "x$user_summary",
          sys_x_user_summary_columns,
          sizeof(sys_x_user_summary_columns) / sizeof(sys_x_user_summary_columns[0])},
         MYLITE_EXECUTION_CATALOG_ARRAY(column_keys_12),
         MYLITE_EXECUTION_CATALOG_ARRAY(column_extras_12),
         MYLITE_EXECUTION_CATALOG_ARRAY(column_privileges_12),
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U,
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U},
        {"sys",
         {MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_USER_SUMMARY_BY_FILE_IO,
          "x$user_summary_by_file_io",
          sys_x_user_summary_by_file_io_columns,
          sizeof(sys_x_user_summary_by_file_io_columns) /
              sizeof(sys_x_user_summary_by_file_io_columns[0])},
         MYLITE_EXECUTION_CATALOG_ARRAY(column_keys_3),
         MYLITE_EXECUTION_CATALOG_ARRAY(column_extras_3),
         MYLITE_EXECUTION_CATALOG_ARRAY(column_privileges_3),
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U,
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U},
        {"sys",
         {MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_USER_SUMMARY_BY_FILE_IO_TYPE,
          "x$user_summary_by_file_io_type",
          sys_x_user_summary_by_file_io_type_columns,
          sizeof(sys_x_user_summary_by_file_io_type_columns) /
              sizeof(sys_x_user_summary_by_file_io_type_columns[0])},
         MYLITE_EXECUTION_CATALOG_ARRAY(column_keys_5),
         MYLITE_EXECUTION_CATALOG_ARRAY(column_extras_5),
         MYLITE_EXECUTION_CATALOG_ARRAY(column_privileges_5),
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U,
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U},
        {"sys",
         {MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_USER_SUMMARY_BY_STAGES,
          "x$user_summary_by_stages",
          sys_x_user_summary_by_stages_columns,
          sizeof(sys_x_user_summary_by_stages_columns) /
              sizeof(sys_x_user_summary_by_stages_columns[0])},
         MYLITE_EXECUTION_CATALOG_ARRAY(column_keys_5),
         MYLITE_EXECUTION_CATALOG_ARRAY(column_extras_5),
         MYLITE_EXECUTION_CATALOG_ARRAY(column_privileges_5),
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U,
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U},
        {"sys",
         {MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_USER_SUMMARY_BY_STATEMENT_LATENCY,
          "x$user_summary_by_statement_latency",
          sys_x_user_summary_by_statement_latency_columns,
          sizeof(sys_x_user_summary_by_statement_latency_columns) /
              sizeof(sys_x_user_summary_by_statement_latency_columns[0])},
         MYLITE_EXECUTION_CATALOG_ARRAY(column_keys_10),
         MYLITE_EXECUTION_CATALOG_ARRAY(column_extras_10),
         MYLITE_EXECUTION_CATALOG_ARRAY(column_privileges_10),
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U,
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U},
        {"sys",
         {MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_USER_SUMMARY_BY_STATEMENT_TYPE,
          "x$user_summary_by_statement_type",
          sys_x_user_summary_by_statement_type_columns,
          sizeof(sys_x_user_summary_by_statement_type_columns) /
              sizeof(sys_x_user_summary_by_statement_type_columns[0])},
         MYLITE_EXECUTION_CATALOG_ARRAY(column_keys_11),
         MYLITE_EXECUTION_CATALOG_ARRAY(column_extras_11),
         MYLITE_EXECUTION_CATALOG_ARRAY(column_privileges_11),
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U,
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U},
};

size_t mylite_execution_catalog_sys_summary_user_x_system_table_definition_count(void) {
    return sizeof(sys_summary_user_x_system_table_definitions) /
           sizeof(sys_summary_user_x_system_table_definitions[0]);
}

const struct mylite_execution_catalog_mysql_system_table *mylite_execution_catalog_sys_summary_user_x_system_table_definition_at(
    size_t index
) {
    if (index >= mylite_execution_catalog_sys_summary_user_x_system_table_definition_count()) {
        return NULL;
    }
    return &sys_summary_user_x_system_table_definitions[index];
}
