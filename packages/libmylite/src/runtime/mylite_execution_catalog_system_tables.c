#include "mylite_execution_catalog_system_tables_internal.h"

#include <stddef.h>
#include <string.h>

struct mylite_execution_catalog_system_table_provider {
    size_t (*count)(void);
    const struct mylite_execution_catalog_mysql_system_table *(*at)(size_t index);
};

static const struct mylite_execution_catalog_system_table_provider system_table_providers[] = {
    {mylite_execution_catalog_mysql_auth_system_table_definition_count,
     mylite_execution_catalog_mysql_auth_system_table_definition_at},
    {mylite_execution_catalog_mysql_service_system_table_definition_count,
     mylite_execution_catalog_mysql_service_system_table_definition_at},
    {mylite_execution_catalog_mysql_replication_system_table_definition_count,
     mylite_execution_catalog_mysql_replication_system_table_definition_at},
    {mylite_execution_catalog_mysql_misc_system_table_definition_count,
     mylite_execution_catalog_mysql_misc_system_table_definition_at},
    {mylite_execution_catalog_performance_schema_system_table_definition_count,
     mylite_execution_catalog_performance_schema_system_table_definition_at},
    {mylite_execution_catalog_sys_core_system_table_definition_count,
     mylite_execution_catalog_sys_core_system_table_definition_at},
    {mylite_execution_catalog_sys_summary_system_table_definition_count,
     mylite_execution_catalog_sys_summary_system_table_definition_at},
    {mylite_execution_catalog_sys_schema_system_table_definition_count,
     mylite_execution_catalog_sys_schema_system_table_definition_at},
    {mylite_execution_catalog_sys_summary_x_system_table_definition_count,
     mylite_execution_catalog_sys_summary_x_system_table_definition_at},
    {mylite_execution_catalog_sys_schema_x_system_table_definition_count,
     mylite_execution_catalog_sys_schema_x_system_table_definition_at},
};

size_t mylite_execution_catalog_mysql_system_table_definition_count(void) {
    size_t total_count = 0U;

    for (size_t provider_index = 0U;
         provider_index < sizeof(system_table_providers) / sizeof(system_table_providers[0]);
         ++provider_index) {
        total_count += system_table_providers[provider_index].count();
    }

    return total_count;
}

const struct mylite_execution_catalog_mysql_system_table *mylite_execution_catalog_mysql_system_table_definition_at(
    size_t index
) {
    for (size_t provider_index = 0U;
         provider_index < sizeof(system_table_providers) / sizeof(system_table_providers[0]);
         ++provider_index) {
        const size_t provider_count = system_table_providers[provider_index].count();
        if (index < provider_count) {
            return system_table_providers[provider_index].at(index);
        }
        index -= provider_count;
    }

    return NULL;
}

const struct mylite_execution_catalog_mysql_system_table *mylite_execution_catalog_mysql_system_table_definition_by_name(
    const char *schema_name,
    const char *table_name
) {
    if (schema_name == NULL || table_name == NULL) {
        return NULL;
    }

    for (size_t index = 0U; index < mylite_execution_catalog_mysql_system_table_definition_count();
         ++index) {
        const struct mylite_execution_catalog_mysql_system_table *definition =
            mylite_execution_catalog_mysql_system_table_definition_at(index);
        if (definition != NULL && strcmp(schema_name, definition->schema_name) == 0 &&
            strcmp(table_name, definition->query_definition.name) == 0) {
            return definition;
        }
    }

    return NULL;
}
