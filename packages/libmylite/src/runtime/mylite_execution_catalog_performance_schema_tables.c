#include "mylite_execution_catalog_system_tables_internal.h"

static const struct mylite_execution_catalog_column_definition
    performance_schema_variable_status_columns[] = {
        {"VARIABLE_NAME",
         NULL,
         "NO",
         "varchar",
         "64",
         "256",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(64)"},
        {"VARIABLE_VALUE",
         NULL,
         "YES",
         "varchar",
         "1024",
         "4096",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(1024)"},
};

static const char *const performance_schema_variable_status_column_keys[] = {
    "PRI",
    "",
};

static const char *const performance_schema_variable_status_column_extras[] = {
    "",
    "",
};

static const char *const performance_schema_variable_status_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
};

static const size_t performance_schema_variable_status_primary_key_columns[] = {
    0U,
};

static const struct mylite_execution_catalog_column_definition
    performance_schema_performance_timer_columns[] = {
        {"TIMER_NAME",
         NULL,
         "NO",
         "enum",
         "11",
         "44",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "enum('CYCLE','NANOSECOND','MICROSECOND','MILLISECOND','THREAD_CPU')"},
        {"TIMER_FREQUENCY", NULL, "YES", "bigint", NULL, NULL, "19", "0", NULL, NULL, NULL, "bigint"
        },
        {"TIMER_RESOLUTION",
         NULL,
         "YES",
         "bigint",
         NULL,
         NULL,
         "19",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint"},
        {"TIMER_OVERHEAD", NULL, "YES", "bigint", NULL, NULL, "19", "0", NULL, NULL, NULL, "bigint"
        },
};

static const char *const performance_schema_performance_timer_column_keys[] = {
    "",
    "",
    "",
    "",
};

static const char *const performance_schema_performance_timer_column_extras[] = {
    "",
    "",
    "",
    "",
};

static const char *const performance_schema_performance_timer_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const struct mylite_execution_catalog_mysql_system_table
    performance_schema_system_table_definitions[] = {
        {"performance_schema",
         {MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_GLOBAL_STATUS,
          "global_status",
          performance_schema_variable_status_columns,
          sizeof(performance_schema_variable_status_columns) /
              sizeof(performance_schema_variable_status_columns[0])},
         performance_schema_variable_status_column_keys,
         performance_schema_variable_status_column_extras,
         performance_schema_variable_status_column_privileges,
         NULL,
         performance_schema_variable_status_primary_key_columns,
         sizeof(performance_schema_variable_status_primary_key_columns) /
             sizeof(performance_schema_variable_status_primary_key_columns[0]),
         NULL,
         NULL,
         0U},
        {"performance_schema",
         {MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_GLOBAL_VARIABLES,
          "global_variables",
          performance_schema_variable_status_columns,
          sizeof(performance_schema_variable_status_columns) /
              sizeof(performance_schema_variable_status_columns[0])},
         performance_schema_variable_status_column_keys,
         performance_schema_variable_status_column_extras,
         performance_schema_variable_status_column_privileges,
         NULL,
         performance_schema_variable_status_primary_key_columns,
         sizeof(performance_schema_variable_status_primary_key_columns) /
             sizeof(performance_schema_variable_status_primary_key_columns[0]),
         NULL,
         NULL,
         0U},
        {"performance_schema",
         {MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_PERFORMANCE_TIMERS,
          "performance_timers",
          performance_schema_performance_timer_columns,
          sizeof(performance_schema_performance_timer_columns) /
              sizeof(performance_schema_performance_timer_columns[0])},
         performance_schema_performance_timer_column_keys,
         performance_schema_performance_timer_column_extras,
         performance_schema_performance_timer_column_privileges,
         NULL,
         NULL,
         0U,
         NULL,
         NULL,
         0U},
        {"performance_schema",
         {MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_SESSION_STATUS,
          "session_status",
          performance_schema_variable_status_columns,
          sizeof(performance_schema_variable_status_columns) /
              sizeof(performance_schema_variable_status_columns[0])},
         performance_schema_variable_status_column_keys,
         performance_schema_variable_status_column_extras,
         performance_schema_variable_status_column_privileges,
         NULL,
         performance_schema_variable_status_primary_key_columns,
         sizeof(performance_schema_variable_status_primary_key_columns) /
             sizeof(performance_schema_variable_status_primary_key_columns[0]),
         NULL,
         NULL,
         0U},
        {"performance_schema",
         {MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_SESSION_VARIABLES,
          "session_variables",
          performance_schema_variable_status_columns,
          sizeof(performance_schema_variable_status_columns) /
              sizeof(performance_schema_variable_status_columns[0])},
         performance_schema_variable_status_column_keys,
         performance_schema_variable_status_column_extras,
         performance_schema_variable_status_column_privileges,
         NULL,
         performance_schema_variable_status_primary_key_columns,
         sizeof(performance_schema_variable_status_primary_key_columns) /
             sizeof(performance_schema_variable_status_primary_key_columns[0]),
         NULL,
         NULL,
         0U},
};

size_t mylite_execution_catalog_performance_schema_system_table_definition_count(void) {
    return sizeof(performance_schema_system_table_definitions) /
           sizeof(performance_schema_system_table_definitions[0]);
}

const struct mylite_execution_catalog_mysql_system_table *mylite_execution_catalog_performance_schema_system_table_definition_at(
    size_t index
) {
    if (index >= mylite_execution_catalog_performance_schema_system_table_definition_count()) {
        return NULL;
    }
    return &performance_schema_system_table_definitions[index];
}
