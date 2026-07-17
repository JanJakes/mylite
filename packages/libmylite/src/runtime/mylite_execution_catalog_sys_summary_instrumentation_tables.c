#include "mylite_execution_catalog_sys_summary_tables_internal.h"

static const struct mylite_execution_catalog_column_definition
    sys_ps_check_lost_instrumentation_columns[] = {
        {"variable_name",
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
        {"variable_value",
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

static const char *const sys_ps_check_lost_instrumentation_column_keys[] = {
    "",
    "",
};

static const char *const sys_ps_check_lost_instrumentation_column_extras[] = {
    "",
    "",
};

static const char *const sys_ps_check_lost_instrumentation_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
};

static const struct mylite_execution_catalog_mysql_system_table
    sys_summary_instrumentation_system_table_definitions[] = {
        {"sys",
         {MYLITE_EXECUTION_CATALOG_TABLE_SYS_PS_CHECK_LOST_INSTRUMENTATION,
          "ps_check_lost_instrumentation",
          sys_ps_check_lost_instrumentation_columns,
          sizeof(sys_ps_check_lost_instrumentation_columns) /
              sizeof(sys_ps_check_lost_instrumentation_columns[0])},
         MYLITE_EXECUTION_CATALOG_ARRAY(sys_ps_check_lost_instrumentation_column_keys),
         MYLITE_EXECUTION_CATALOG_ARRAY(sys_ps_check_lost_instrumentation_column_extras),
         MYLITE_EXECUTION_CATALOG_ARRAY(sys_ps_check_lost_instrumentation_column_privileges),
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U,
         MYLITE_EXECUTION_CATALOG_NO_ARRAY,
         NULL,
         0U},
};

size_t mylite_execution_catalog_sys_summary_instrumentation_system_table_definition_count(void) {
    return sizeof(sys_summary_instrumentation_system_table_definitions) /
           sizeof(sys_summary_instrumentation_system_table_definitions[0]);
}

const struct mylite_execution_catalog_mysql_system_table *mylite_execution_catalog_sys_summary_instrumentation_system_table_definition_at(
    size_t index
) {
    if (index >=
        mylite_execution_catalog_sys_summary_instrumentation_system_table_definition_count()) {
        return NULL;
    }
    return &sys_summary_instrumentation_system_table_definitions[index];
}
