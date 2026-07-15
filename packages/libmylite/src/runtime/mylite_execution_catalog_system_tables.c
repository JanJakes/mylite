#include "mylite_execution_catalog_system_tables_internal.h"

#include <stddef.h>
#include <string.h>

struct mylite_execution_catalog_system_table_provider {
    const char *schema_name;
    size_t (*count)(void);
    const struct mylite_execution_catalog_mysql_system_table *(*at)(size_t index);
};

static const struct mylite_execution_catalog_system_table_provider system_table_providers[] = {
    {"mysql",
     mylite_execution_catalog_mysql_auth_system_table_definition_count,
     mylite_execution_catalog_mysql_auth_system_table_definition_at},
    {"mysql",
     mylite_execution_catalog_mysql_service_system_table_definition_count,
     mylite_execution_catalog_mysql_service_system_table_definition_at},
    {"mysql",
     mylite_execution_catalog_mysql_replication_system_table_definition_count,
     mylite_execution_catalog_mysql_replication_system_table_definition_at},
    {"mysql",
     mylite_execution_catalog_mysql_misc_system_table_definition_count,
     mylite_execution_catalog_mysql_misc_system_table_definition_at},
    {"performance_schema",
     mylite_execution_catalog_performance_schema_system_table_definition_count,
     mylite_execution_catalog_performance_schema_system_table_definition_at},
    {"sys",
     mylite_execution_catalog_sys_core_system_table_definition_count,
     mylite_execution_catalog_sys_core_system_table_definition_at},
    {"sys",
     mylite_execution_catalog_sys_summary_system_table_definition_count,
     mylite_execution_catalog_sys_summary_system_table_definition_at},
    {"sys",
     mylite_execution_catalog_sys_schema_system_table_definition_count,
     mylite_execution_catalog_sys_schema_system_table_definition_at},
    {"sys",
     mylite_execution_catalog_sys_summary_x_system_table_definition_count,
     mylite_execution_catalog_sys_summary_x_system_table_definition_at},
    {"sys",
     mylite_execution_catalog_sys_schema_x_system_table_definition_count,
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
    if (strcmp(schema_name, "mysql") != 0 && strcmp(schema_name, "performance_schema") != 0 &&
        strcmp(schema_name, "sys") != 0) {
        return NULL;
    }

    for (size_t provider_index = 0U;
         provider_index < sizeof(system_table_providers) / sizeof(system_table_providers[0]);
         ++provider_index) {
        const struct mylite_execution_catalog_system_table_provider *provider =
            &system_table_providers[provider_index];
        size_t provider_count = 0U;

        if (strcmp(schema_name, provider->schema_name) != 0) {
            continue;
        }
        provider_count = provider->count();
        for (size_t index = 0U; index < provider_count; ++index) {
            const struct mylite_execution_catalog_mysql_system_table *definition =
                provider->at(index);

            if (definition != NULL && strcmp(table_name, definition->query_definition.name) == 0) {
                return definition;
            }
        }
    }

    return NULL;
}
