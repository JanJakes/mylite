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

static const struct mylite_execution_catalog_column_definition
    performance_schema_setup_actor_columns[] = {
        {"HOST",
         "%",
         "NO",
         "char",
         "255",
         "255",
         NULL,
         NULL,
         NULL,
         "ascii",
         "ascii_general_ci",
         "char(255)"},
        {"USER",
         "%",
         "NO",
         "char",
         "32",
         "128",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_bin",
         "char(32)"},
        {"ROLE",
         "%",
         "NO",
         "char",
         "32",
         "128",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_bin",
         "char(32)"},
        {"ENABLED",
         "YES",
         "NO",
         "enum",
         "3",
         "12",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "enum('YES','NO')"},
        {"HISTORY",
         "YES",
         "NO",
         "enum",
         "3",
         "12",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "enum('YES','NO')"},
};

static const char *const performance_schema_setup_actor_column_keys[] = {
    "PRI",
    "PRI",
    "PRI",
    "",
    "",
};

static const char *const performance_schema_setup_actor_column_extras[] = {
    "",
    "",
    "",
    "",
    "",
};

static const char *const performance_schema_setup_actor_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const size_t performance_schema_setup_actor_primary_key_columns[] = {
    0U,
    1U,
    2U,
};

static const struct mylite_execution_catalog_column_definition
    performance_schema_setup_consumer_columns[] = {
        {"NAME",
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
        {"ENABLED",
         NULL,
         "NO",
         "enum",
         "3",
         "12",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "enum('YES','NO')"},
};

static const char *const performance_schema_setup_consumer_column_keys[] = {
    "PRI",
    "",
};

static const char *const performance_schema_setup_consumer_column_extras[] = {
    "",
    "",
};

static const char *const performance_schema_setup_consumer_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
};

static const size_t performance_schema_setup_consumer_primary_key_columns[] = {
    0U,
};

static const struct mylite_execution_catalog_column_definition
    performance_schema_setup_object_columns[] = {
        {"OBJECT_TYPE",
         "TABLE",
         "NO",
         "enum",
         "9",
         "36",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "enum('EVENT','FUNCTION','PROCEDURE','TABLE','TRIGGER')"},
        {"OBJECT_SCHEMA",
         "%",
         "YES",
         "varchar",
         "64",
         "256",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(64)"},
        {"OBJECT_NAME",
         "%",
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
        {"ENABLED",
         "YES",
         "NO",
         "enum",
         "3",
         "12",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "enum('YES','NO')"},
        {"TIMED",
         "YES",
         "NO",
         "enum",
         "3",
         "12",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "enum('YES','NO')"},
};

static const char *const performance_schema_setup_object_column_keys[] = {
    "MUL",
    "",
    "",
    "",
    "",
};

static const char *const performance_schema_setup_object_column_extras[] = {
    "",
    "",
    "",
    "",
    "",
};

static const char *const performance_schema_setup_object_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const struct mylite_execution_catalog_mysql_system_secondary_index
    performance_schema_setup_object_secondary_indexes[] = {
        {"OBJECT", 0U, NULL, "0", true, NULL, "HASH"},
        {"OBJECT", 1U, NULL, "0", true, NULL, "HASH"},
        {"OBJECT", 2U, NULL, "0", true, NULL, "HASH"},
};

static const struct mylite_execution_catalog_column_definition
    performance_schema_setup_thread_columns[] = {
        {"NAME",
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
        {"ENABLED",
         NULL,
         "NO",
         "enum",
         "3",
         "12",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "enum('YES','NO')"},
        {"HISTORY",
         NULL,
         "NO",
         "enum",
         "3",
         "12",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "enum('YES','NO')"},
        {"PROPERTIES",
         NULL,
         "NO",
         "set",
         "14",
         "56",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "set('singleton','user')"},
        {"VOLATILITY", NULL, "NO", "int", NULL, NULL, "10", "0", NULL, NULL, NULL, "int"},
        {"DOCUMENTATION",
         NULL,
         "YES",
         "longtext",
         "4294967295",
         "4294967295",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "longtext"},
};

static const char *const performance_schema_setup_thread_column_keys[] = {
    "PRI",
    "",
    "",
    "",
    "",
    "",
};

static const char *const performance_schema_setup_thread_column_extras[] = {
    "",
    "",
    "",
    "",
    "",
    "",
};

static const char *const performance_schema_setup_thread_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const size_t performance_schema_setup_thread_primary_key_columns[] = {
    0U,
};

static const struct mylite_execution_catalog_column_definition
    performance_schema_user_variable_by_thread_columns[] = {
        {"THREAD_ID",
         NULL,
         "NO",
         "bigint",
         NULL,
         NULL,
         "20",
         "0",
         NULL,
         NULL,
         NULL,
         "bigint unsigned"},
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
         "longblob",
         "4294967295",
         "4294967295",
         NULL,
         NULL,
         NULL,
         NULL,
         NULL,
         "longblob"},
};

static const char *const performance_schema_user_variable_by_thread_column_keys[] = {
    "PRI",
    "PRI",
    "",
};

static const char *const performance_schema_user_variable_by_thread_column_extras[] = {
    "",
    "",
    "",
};

static const char *const performance_schema_user_variable_by_thread_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const size_t performance_schema_user_variable_by_thread_primary_key_columns[] = {
    0U,
    1U,
};

static const struct mylite_execution_catalog_column_definition
    performance_schema_user_defined_function_columns[] = {
        {"UDF_NAME",
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
        {"UDF_RETURN_TYPE",
         NULL,
         "NO",
         "varchar",
         "20",
         "80",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(20)"},
        {"UDF_TYPE",
         NULL,
         "NO",
         "varchar",
         "20",
         "80",
         NULL,
         NULL,
         NULL,
         "utf8mb4",
         "utf8mb4_0900_ai_ci",
         "varchar(20)"},
        {"UDF_LIBRARY",
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
        {"UDF_USAGE_COUNT", NULL, "YES", "bigint", NULL, NULL, "19", "0", NULL, NULL, NULL, "bigint"
        },
};

static const char *const performance_schema_user_defined_function_column_keys[] = {
    "PRI",
    "",
    "",
    "",
    "",
};

static const char *const performance_schema_user_defined_function_column_extras[] = {
    "",
    "",
    "",
    "",
    "",
};

static const char *const performance_schema_user_defined_function_column_privileges[] = {
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
    "select,insert,update,references",
};

static const size_t performance_schema_user_defined_function_primary_key_columns[] = {
    0U,
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
         {MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_SETUP_ACTORS,
          "setup_actors",
          performance_schema_setup_actor_columns,
          sizeof(performance_schema_setup_actor_columns) /
              sizeof(performance_schema_setup_actor_columns[0])},
         performance_schema_setup_actor_column_keys,
         performance_schema_setup_actor_column_extras,
         performance_schema_setup_actor_column_privileges,
         NULL,
         performance_schema_setup_actor_primary_key_columns,
         sizeof(performance_schema_setup_actor_primary_key_columns) /
             sizeof(performance_schema_setup_actor_primary_key_columns[0]),
         NULL,
         NULL,
         0U},
        {"performance_schema",
         {MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_SETUP_CONSUMERS,
          "setup_consumers",
          performance_schema_setup_consumer_columns,
          sizeof(performance_schema_setup_consumer_columns) /
              sizeof(performance_schema_setup_consumer_columns[0])},
         performance_schema_setup_consumer_column_keys,
         performance_schema_setup_consumer_column_extras,
         performance_schema_setup_consumer_column_privileges,
         NULL,
         performance_schema_setup_consumer_primary_key_columns,
         sizeof(performance_schema_setup_consumer_primary_key_columns) /
             sizeof(performance_schema_setup_consumer_primary_key_columns[0]),
         NULL,
         NULL,
         0U},
        {"performance_schema",
         {MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_SETUP_OBJECTS,
          "setup_objects",
          performance_schema_setup_object_columns,
          sizeof(performance_schema_setup_object_columns) /
              sizeof(performance_schema_setup_object_columns[0])},
         performance_schema_setup_object_column_keys,
         performance_schema_setup_object_column_extras,
         performance_schema_setup_object_column_privileges,
         NULL,
         NULL,
         0U,
         NULL,
         performance_schema_setup_object_secondary_indexes,
         sizeof(performance_schema_setup_object_secondary_indexes) /
             sizeof(performance_schema_setup_object_secondary_indexes[0])},
        {"performance_schema",
         {MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_SETUP_THREADS,
          "setup_threads",
          performance_schema_setup_thread_columns,
          sizeof(performance_schema_setup_thread_columns) /
              sizeof(performance_schema_setup_thread_columns[0])},
         performance_schema_setup_thread_column_keys,
         performance_schema_setup_thread_column_extras,
         performance_schema_setup_thread_column_privileges,
         NULL,
         performance_schema_setup_thread_primary_key_columns,
         sizeof(performance_schema_setup_thread_primary_key_columns) /
             sizeof(performance_schema_setup_thread_primary_key_columns[0]),
         NULL,
         NULL,
         0U},
        {"performance_schema",
         {MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_USER_DEFINED_FUNCTIONS,
          "user_defined_functions",
          performance_schema_user_defined_function_columns,
          sizeof(performance_schema_user_defined_function_columns) /
              sizeof(performance_schema_user_defined_function_columns[0])},
         performance_schema_user_defined_function_column_keys,
         performance_schema_user_defined_function_column_extras,
         performance_schema_user_defined_function_column_privileges,
         NULL,
         performance_schema_user_defined_function_primary_key_columns,
         sizeof(performance_schema_user_defined_function_primary_key_columns) /
             sizeof(performance_schema_user_defined_function_primary_key_columns[0]),
         NULL,
         NULL,
         0U},
        {"performance_schema",
         {MYLITE_EXECUTION_CATALOG_TABLE_PERFORMANCE_SCHEMA_USER_VARIABLES_BY_THREAD,
          "user_variables_by_thread",
          performance_schema_user_variable_by_thread_columns,
          sizeof(performance_schema_user_variable_by_thread_columns) /
              sizeof(performance_schema_user_variable_by_thread_columns[0])},
         performance_schema_user_variable_by_thread_column_keys,
         performance_schema_user_variable_by_thread_column_extras,
         performance_schema_user_variable_by_thread_column_privileges,
         NULL,
         performance_schema_user_variable_by_thread_primary_key_columns,
         sizeof(performance_schema_user_variable_by_thread_primary_key_columns) /
             sizeof(performance_schema_user_variable_by_thread_primary_key_columns[0]),
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
