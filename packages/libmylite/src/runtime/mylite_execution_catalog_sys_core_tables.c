#include "mylite_execution_catalog_system_tables_internal.h"

static const struct mylite_execution_catalog_column_definition sys_sys_config_columns[] = {
    {"variable",
     NULL,
     "NO",
     "varchar",
     "128",
     "512",
     NULL,
     NULL,
     NULL,
     "utf8mb4",
     "utf8mb4_0900_ai_ci",
     "varchar(128)"},
    {"value",
     NULL,
     "YES",
     "varchar",
     "128",
     "512",
     NULL,
     NULL,
     NULL,
     "utf8mb4",
     "utf8mb4_0900_ai_ci",
     "varchar(128)"},
    {"set_time",
     "CURRENT_TIMESTAMP",
     "YES",
     "timestamp",
     NULL,
     NULL,
     NULL,
     NULL,
     "0",
     NULL,
     NULL,
     "timestamp"},
    {"set_by",
     NULL,
     "YES",
     "varchar",
     "128",
     "512",
     NULL,
     NULL,
     NULL,
     "utf8mb4",
     "utf8mb4_0900_ai_ci",
     "varchar(128)"},
};

static const char *const sys_sys_config_column_keys[] = {
    "PRI",
    "",
    "",
    "",
};

static const char *const sys_sys_config_column_extras[] = {
    "",
    "",
    "DEFAULT_GENERATED on update CURRENT_TIMESTAMP",
    "",
};

static const char *const sys_sys_config_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const char sys_sys_config_trigger_action_statement[] =
    "BEGIN\n"
    "    IF @sys.ignore_sys_config_triggers != true AND NEW.set_by IS NULL THEN\n"
    "        SET NEW.set_by = USER();\n"
    "    END IF;\n"
    "END";

static const char sys_sys_config_trigger_sql_mode[] =
    "ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,"
    "ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION";

static const struct mylite_execution_catalog_sys_config_trigger sys_sys_config_triggers[] = {
    {"sys_config_insert_set_user", "INSERT"},
    {"sys_config_update_set_user", "UPDATE"},
};

static const struct mylite_execution_catalog_column_definition sys_version_columns[] = {
    {"sys_version",
     "",
     "NO",
     "varchar",
     "5",
     "20",
     NULL,
     NULL,
     NULL,
     "utf8mb4",
     "utf8mb4_0900_ai_ci",
     "varchar(5)"},
    {"mysql_version",
     "",
     "NO",
     "varchar",
     "5",
     "15",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "varchar(5)"},
};

static const char *const sys_version_column_keys[] = {
    "",
    "",
};

static const char *const sys_version_column_extras[] = {
    "",
    "",
};

static const char *const sys_version_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
};

static const struct mylite_execution_catalog_column_definition sys_metrics_columns[] = {
    {"Variable_name",
     "",
     "NO",
     "varchar",
     "193",
     "772",
     NULL,
     NULL,
     NULL,
     "utf8mb4",
     "utf8mb4_0900_ai_ci",
     "varchar(193)"},
    {"Variable_value",
     NULL,
     "YES",
     "text",
     "65535",
     "65535",
     NULL,
     NULL,
     NULL,
     "utf8mb4",
     "utf8mb4_0900_ai_ci",
     "text"},
    {"Type",
     "",
     "NO",
     "varchar",
     "210",
     "630",
     NULL,
     NULL,
     NULL,
     "utf8mb3",
     "utf8mb3_general_ci",
     "varchar(210)"},
    {"Enabled",
     "",
     "NO",
     "varchar",
     "7",
     "28",
     NULL,
     NULL,
     NULL,
     "utf8mb4",
     "utf8mb4_0900_ai_ci",
     "varchar(7)"},
};

static const char *const sys_metrics_column_keys[] = {
    "",
    "",
    "",
    "",
};

static const char *const sys_metrics_column_extras[] = {
    "",
    "",
    "",
    "",
};

static const char *const sys_metrics_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const struct mylite_execution_catalog_mysql_system_table
    sys_core_system_table_definitions[] = {
        {"sys",
         {MYLITE_EXECUTION_CATALOG_TABLE_SYS_SYS_CONFIG,
          "sys_config",
          sys_sys_config_columns,
          sizeof(sys_sys_config_columns) / sizeof(sys_sys_config_columns[0])},
         sys_sys_config_column_keys,
         sys_sys_config_column_extras,
         sys_sys_config_column_privileges,
         NULL,
         NULL,
         0U,
         NULL,
         NULL,
         0U},
        {"sys",
         {MYLITE_EXECUTION_CATALOG_TABLE_SYS_VERSION,
          "version",
          sys_version_columns,
          sizeof(sys_version_columns) / sizeof(sys_version_columns[0])},
         sys_version_column_keys,
         sys_version_column_extras,
         sys_version_column_privileges,
         NULL,
         NULL,
         0U,
         NULL,
         NULL,
         0U},
        {"sys",
         {MYLITE_EXECUTION_CATALOG_TABLE_SYS_METRICS,
          "metrics",
          sys_metrics_columns,
          sizeof(sys_metrics_columns) / sizeof(sys_metrics_columns[0])},
         sys_metrics_column_keys,
         sys_metrics_column_extras,
         sys_metrics_column_privileges,
         NULL,
         NULL,
         0U,
         NULL,
         NULL,
         0U},
};

size_t mylite_execution_catalog_sys_core_system_table_definition_count(void) {
    return sizeof(sys_core_system_table_definitions) / sizeof(sys_core_system_table_definitions[0]);
}

const struct mylite_execution_catalog_mysql_system_table *mylite_execution_catalog_sys_core_system_table_definition_at(
    size_t index
) {
    if (index >= mylite_execution_catalog_sys_core_system_table_definition_count()) {
        return NULL;
    }
    return &sys_core_system_table_definitions[index];
}

const char *mylite_execution_catalog_sys_sys_config_trigger_action_statement(void) {
    return sys_sys_config_trigger_action_statement;
}

const char *mylite_execution_catalog_sys_sys_config_trigger_sql_mode(void) {
    return sys_sys_config_trigger_sql_mode;
}

size_t mylite_execution_catalog_sys_sys_config_trigger_count(void) {
    return sizeof(sys_sys_config_triggers) / sizeof(sys_sys_config_triggers[0]);
}

const struct mylite_execution_catalog_sys_config_trigger *mylite_execution_catalog_sys_sys_config_trigger_at(
    size_t index
) {
    if (index >= mylite_execution_catalog_sys_sys_config_trigger_count()) {
        return NULL;
    }
    return &sys_sys_config_triggers[index];
}
