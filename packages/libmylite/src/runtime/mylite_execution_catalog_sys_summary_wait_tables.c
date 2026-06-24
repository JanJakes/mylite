#include "mylite_execution_catalog_sys_summary_tables_internal.h"

#define SYS_EVENT_CLASS_COLUMN                                                                     \
    {"event_class",                                                                                \
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

#define SYS_EVENT_COLUMN(name, nullable)                                                           \
    {name,                                                                                         \
     NULL,                                                                                         \
     nullable,                                                                                     \
     "varchar",                                                                                    \
     "128",                                                                                        \
     "512",                                                                                        \
     NULL,                                                                                         \
     NULL,                                                                                         \
     NULL,                                                                                         \
     "utf8mb4",                                                                                    \
     "utf8mb4_0900_ai_ci",                                                                         \
     "varchar(128)"}

#define SYS_HOST_COLUMN                                                                            \
    {"host",                                                                                       \
     NULL,                                                                                         \
     "YES",                                                                                        \
     "varchar",                                                                                    \
     "255",                                                                                        \
     "255",                                                                                        \
     NULL,                                                                                         \
     NULL,                                                                                         \
     NULL,                                                                                         \
     "ascii",                                                                                      \
     "ascii_general_ci",                                                                           \
     "varchar(255)"}

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

#define SYS_BIGINT_UNSIGNED_COLUMN(name, nullable)                                                 \
    {name, NULL, nullable, "bigint", NULL, NULL, "20", "0", NULL, NULL, NULL, "bigint unsigned"}

#define SYS_WAIT_CLASS_COLUMNS                                                                     \
    SYS_EVENT_CLASS_COLUMN, SYS_DECIMAL_COLUMN("total", NULL, "YES", "42", "0", "decimal(42,0)"),  \
        SYS_FORMATTED_LATENCY_COLUMN("total_latency"),                                             \
        SYS_FORMATTED_LATENCY_COLUMN("min_latency"), SYS_FORMATTED_LATENCY_COLUMN("avg_latency"),  \
        SYS_FORMATTED_LATENCY_COLUMN("max_latency")

#define SYS_X_WAIT_CLASS_COLUMNS                                                                   \
    SYS_EVENT_CLASS_COLUMN, SYS_DECIMAL_COLUMN("total", NULL, "YES", "42", "0", "decimal(42,0)"),  \
        SYS_DECIMAL_COLUMN("total_latency", NULL, "YES", "42", "0", "decimal(42,0)"),              \
        SYS_BIGINT_UNSIGNED_COLUMN("min_latency", "YES"),                                          \
        SYS_DECIMAL_COLUMN("avg_latency", "0.0000", "NO", "46", "4", "decimal(46,4)"),             \
        SYS_BIGINT_UNSIGNED_COLUMN("max_latency", "YES")

static const struct mylite_execution_catalog_column_definition
    sys_wait_classes_global_by_avg_latency_columns[] = {
        SYS_WAIT_CLASS_COLUMNS,
};

static const struct mylite_execution_catalog_column_definition
    sys_x_wait_classes_global_by_avg_latency_columns[] = {
        SYS_X_WAIT_CLASS_COLUMNS,
};

static const struct mylite_execution_catalog_column_definition
    sys_wait_classes_global_by_latency_columns[] = {
        SYS_WAIT_CLASS_COLUMNS,
};

static const struct mylite_execution_catalog_column_definition
    sys_x_wait_classes_global_by_latency_columns[] = {
        SYS_X_WAIT_CLASS_COLUMNS,
};

static const struct mylite_execution_catalog_column_definition
    sys_waits_by_host_by_latency_columns[] = {
        SYS_HOST_COLUMN,
        SYS_EVENT_COLUMN("event", "NO"),
        SYS_BIGINT_UNSIGNED_COLUMN("total", "NO"),
        SYS_FORMATTED_LATENCY_COLUMN("total_latency"),
        SYS_FORMATTED_LATENCY_COLUMN("avg_latency"),
        SYS_FORMATTED_LATENCY_COLUMN("max_latency"),
};

static const struct mylite_execution_catalog_column_definition
    sys_x_waits_by_host_by_latency_columns[] = {
        SYS_HOST_COLUMN,
        SYS_EVENT_COLUMN("event", "NO"),
        SYS_BIGINT_UNSIGNED_COLUMN("total", "NO"),
        SYS_BIGINT_UNSIGNED_COLUMN("total_latency", "NO"),
        SYS_BIGINT_UNSIGNED_COLUMN("avg_latency", "NO"),
        SYS_BIGINT_UNSIGNED_COLUMN("max_latency", "NO"),
};

static const struct mylite_execution_catalog_column_definition
    sys_waits_by_user_by_latency_columns[] = {
        SYS_USER_COLUMN,
        SYS_EVENT_COLUMN("event", "NO"),
        SYS_BIGINT_UNSIGNED_COLUMN("total", "NO"),
        SYS_FORMATTED_LATENCY_COLUMN("total_latency"),
        SYS_FORMATTED_LATENCY_COLUMN("avg_latency"),
        SYS_FORMATTED_LATENCY_COLUMN("max_latency"),
};

static const struct mylite_execution_catalog_column_definition
    sys_x_waits_by_user_by_latency_columns[] = {
        SYS_USER_COLUMN,
        SYS_EVENT_COLUMN("event", "NO"),
        SYS_BIGINT_UNSIGNED_COLUMN("total", "NO"),
        SYS_BIGINT_UNSIGNED_COLUMN("total_latency", "NO"),
        SYS_BIGINT_UNSIGNED_COLUMN("avg_latency", "NO"),
        SYS_BIGINT_UNSIGNED_COLUMN("max_latency", "NO"),
};

static const struct mylite_execution_catalog_column_definition
    sys_waits_global_by_latency_columns[] = {
        SYS_EVENT_COLUMN("events", "NO"),
        SYS_BIGINT_UNSIGNED_COLUMN("total", "NO"),
        SYS_FORMATTED_LATENCY_COLUMN("total_latency"),
        SYS_FORMATTED_LATENCY_COLUMN("avg_latency"),
        SYS_FORMATTED_LATENCY_COLUMN("max_latency"),
};

static const struct mylite_execution_catalog_column_definition
    sys_x_waits_global_by_latency_columns[] = {
        SYS_EVENT_COLUMN("events", "NO"),
        SYS_BIGINT_UNSIGNED_COLUMN("total", "NO"),
        SYS_BIGINT_UNSIGNED_COLUMN("total_latency", "NO"),
        SYS_BIGINT_UNSIGNED_COLUMN("avg_latency", "NO"),
        SYS_BIGINT_UNSIGNED_COLUMN("max_latency", "NO"),
};

#undef SYS_EVENT_CLASS_COLUMN
#undef SYS_EVENT_COLUMN
#undef SYS_HOST_COLUMN
#undef SYS_USER_COLUMN
#undef SYS_FORMATTED_LATENCY_COLUMN
#undef SYS_DECIMAL_COLUMN
#undef SYS_BIGINT_UNSIGNED_COLUMN
#undef SYS_WAIT_CLASS_COLUMNS
#undef SYS_X_WAIT_CLASS_COLUMNS

#define EMPTY_KEY ""
#define COLUMN_EXTRA ""
#define COLUMN_PRIVILEGES "select,insert,update,references"

static const char *const column_keys_5[] = {EMPTY_KEY, EMPTY_KEY, EMPTY_KEY, EMPTY_KEY, EMPTY_KEY};
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

static const char *const column_keys_6[] = {
    EMPTY_KEY,
    EMPTY_KEY,
    EMPTY_KEY,
    EMPTY_KEY,
    EMPTY_KEY,
    EMPTY_KEY,
};
static const char *const column_extras_6[] = {
    COLUMN_EXTRA,
    COLUMN_EXTRA,
    COLUMN_EXTRA,
    COLUMN_EXTRA,
    COLUMN_EXTRA,
    COLUMN_EXTRA,
};
static const char *const column_privileges_6[] = {
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
    sys_summary_wait_system_table_definitions[] = {
        {"sys",
         {MYLITE_EXECUTION_CATALOG_TABLE_SYS_WAIT_CLASSES_GLOBAL_BY_AVG_LATENCY,
          "wait_classes_global_by_avg_latency",
          sys_wait_classes_global_by_avg_latency_columns,
          sizeof(sys_wait_classes_global_by_avg_latency_columns) /
              sizeof(sys_wait_classes_global_by_avg_latency_columns[0])},
         column_keys_6,
         column_extras_6,
         column_privileges_6,
         NULL,
         NULL,
         0U,
         NULL,
         NULL,
         0U},
        {"sys",
         {MYLITE_EXECUTION_CATALOG_TABLE_SYS_WAIT_CLASSES_GLOBAL_BY_LATENCY,
          "wait_classes_global_by_latency",
          sys_wait_classes_global_by_latency_columns,
          sizeof(sys_wait_classes_global_by_latency_columns) /
              sizeof(sys_wait_classes_global_by_latency_columns[0])},
         column_keys_6,
         column_extras_6,
         column_privileges_6,
         NULL,
         NULL,
         0U,
         NULL,
         NULL,
         0U},
        {"sys",
         {MYLITE_EXECUTION_CATALOG_TABLE_SYS_WAITS_BY_HOST_BY_LATENCY,
          "waits_by_host_by_latency",
          sys_waits_by_host_by_latency_columns,
          sizeof(sys_waits_by_host_by_latency_columns) /
              sizeof(sys_waits_by_host_by_latency_columns[0])},
         column_keys_6,
         column_extras_6,
         column_privileges_6,
         NULL,
         NULL,
         0U,
         NULL,
         NULL,
         0U},
        {"sys",
         {MYLITE_EXECUTION_CATALOG_TABLE_SYS_WAITS_BY_USER_BY_LATENCY,
          "waits_by_user_by_latency",
          sys_waits_by_user_by_latency_columns,
          sizeof(sys_waits_by_user_by_latency_columns) /
              sizeof(sys_waits_by_user_by_latency_columns[0])},
         column_keys_6,
         column_extras_6,
         column_privileges_6,
         NULL,
         NULL,
         0U,
         NULL,
         NULL,
         0U},
        {"sys",
         {MYLITE_EXECUTION_CATALOG_TABLE_SYS_WAITS_GLOBAL_BY_LATENCY,
          "waits_global_by_latency",
          sys_waits_global_by_latency_columns,
          sizeof(sys_waits_global_by_latency_columns) /
              sizeof(sys_waits_global_by_latency_columns[0])},
         column_keys_5,
         column_extras_5,
         column_privileges_5,
         NULL,
         NULL,
         0U,
         NULL,
         NULL,
         0U},
};

size_t mylite_execution_catalog_sys_summary_wait_system_table_definition_count(void) {
    return sizeof(sys_summary_wait_system_table_definitions) /
           sizeof(sys_summary_wait_system_table_definitions[0]);
}

const struct mylite_execution_catalog_mysql_system_table *mylite_execution_catalog_sys_summary_wait_system_table_definition_at(
    size_t index
) {
    if (index >= mylite_execution_catalog_sys_summary_wait_system_table_definition_count()) {
        return NULL;
    }
    return &sys_summary_wait_system_table_definitions[index];
}

static const struct mylite_execution_catalog_mysql_system_table
    sys_summary_wait_x_system_table_definitions[] = {
        {"sys",
         {MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_WAIT_CLASSES_GLOBAL_BY_AVG_LATENCY,
          "x$wait_classes_global_by_avg_latency",
          sys_x_wait_classes_global_by_avg_latency_columns,
          sizeof(sys_x_wait_classes_global_by_avg_latency_columns) /
              sizeof(sys_x_wait_classes_global_by_avg_latency_columns[0])},
         column_keys_6,
         column_extras_6,
         column_privileges_6,
         NULL,
         NULL,
         0U,
         NULL,
         NULL,
         0U},
        {"sys",
         {MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_WAIT_CLASSES_GLOBAL_BY_LATENCY,
          "x$wait_classes_global_by_latency",
          sys_x_wait_classes_global_by_latency_columns,
          sizeof(sys_x_wait_classes_global_by_latency_columns) /
              sizeof(sys_x_wait_classes_global_by_latency_columns[0])},
         column_keys_6,
         column_extras_6,
         column_privileges_6,
         NULL,
         NULL,
         0U,
         NULL,
         NULL,
         0U},
        {"sys",
         {MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_WAITS_BY_HOST_BY_LATENCY,
          "x$waits_by_host_by_latency",
          sys_x_waits_by_host_by_latency_columns,
          sizeof(sys_x_waits_by_host_by_latency_columns) /
              sizeof(sys_x_waits_by_host_by_latency_columns[0])},
         column_keys_6,
         column_extras_6,
         column_privileges_6,
         NULL,
         NULL,
         0U,
         NULL,
         NULL,
         0U},
        {"sys",
         {MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_WAITS_BY_USER_BY_LATENCY,
          "x$waits_by_user_by_latency",
          sys_x_waits_by_user_by_latency_columns,
          sizeof(sys_x_waits_by_user_by_latency_columns) /
              sizeof(sys_x_waits_by_user_by_latency_columns[0])},
         column_keys_6,
         column_extras_6,
         column_privileges_6,
         NULL,
         NULL,
         0U,
         NULL,
         NULL,
         0U},
        {"sys",
         {MYLITE_EXECUTION_CATALOG_TABLE_SYS_X_WAITS_GLOBAL_BY_LATENCY,
          "x$waits_global_by_latency",
          sys_x_waits_global_by_latency_columns,
          sizeof(sys_x_waits_global_by_latency_columns) /
              sizeof(sys_x_waits_global_by_latency_columns[0])},
         column_keys_5,
         column_extras_5,
         column_privileges_5,
         NULL,
         NULL,
         0U,
         NULL,
         NULL,
         0U},
};

size_t mylite_execution_catalog_sys_summary_wait_x_system_table_definition_count(void) {
    return sizeof(sys_summary_wait_x_system_table_definitions) /
           sizeof(sys_summary_wait_x_system_table_definitions[0]);
}

const struct mylite_execution_catalog_mysql_system_table *mylite_execution_catalog_sys_summary_wait_x_system_table_definition_at(
    size_t index
) {
    if (index >= mylite_execution_catalog_sys_summary_wait_x_system_table_definition_count()) {
        return NULL;
    }
    return &sys_summary_wait_x_system_table_definitions[index];
}
