#include "mylite_execution_catalog_sys_summary_tables_internal.h"
#include "mylite_execution_catalog_system_tables_internal.h"

#include <stddef.h>

struct mylite_execution_catalog_sys_summary_system_table_provider {
    size_t (*count)(void);
    const struct mylite_execution_catalog_mysql_system_table *(*at)(size_t index);
};

struct mylite_execution_catalog_sys_summary_system_table_provider_list {
    const struct mylite_execution_catalog_sys_summary_system_table_provider *providers;
    size_t provider_count;
};

static size_t count_sys_summary_system_table_definitions(
    struct mylite_execution_catalog_sys_summary_system_table_provider_list provider_list
);
static const struct mylite_execution_catalog_mysql_system_table *sys_summary_system_table_definition_at(
    struct mylite_execution_catalog_sys_summary_system_table_provider_list provider_list,
    size_t index
);

static const struct mylite_execution_catalog_sys_summary_system_table_provider
    sys_summary_system_table_providers[] = {
        {mylite_execution_catalog_sys_summary_host_system_table_definition_count,
         mylite_execution_catalog_sys_summary_host_system_table_definition_at},
        {mylite_execution_catalog_sys_summary_user_system_table_definition_count,
         mylite_execution_catalog_sys_summary_user_system_table_definition_at},
        {mylite_execution_catalog_sys_summary_innodb_system_table_definition_count,
         mylite_execution_catalog_sys_summary_innodb_system_table_definition_at},
        {mylite_execution_catalog_sys_summary_io_system_table_definition_count,
         mylite_execution_catalog_sys_summary_io_system_table_definition_at},
        {mylite_execution_catalog_sys_summary_memory_system_table_definition_count,
         mylite_execution_catalog_sys_summary_memory_system_table_definition_at},
        {mylite_execution_catalog_sys_summary_statement_system_table_definition_count,
         mylite_execution_catalog_sys_summary_statement_system_table_definition_at},
        {mylite_execution_catalog_sys_summary_instrumentation_system_table_definition_count,
         mylite_execution_catalog_sys_summary_instrumentation_system_table_definition_at},
};

static const struct mylite_execution_catalog_sys_summary_system_table_provider
    sys_summary_x_system_table_providers[] = {
        {mylite_execution_catalog_sys_summary_host_x_system_table_definition_count,
         mylite_execution_catalog_sys_summary_host_x_system_table_definition_at},
        {mylite_execution_catalog_sys_summary_user_x_system_table_definition_count,
         mylite_execution_catalog_sys_summary_user_x_system_table_definition_at},
        {mylite_execution_catalog_sys_summary_innodb_x_system_table_definition_count,
         mylite_execution_catalog_sys_summary_innodb_x_system_table_definition_at},
        {mylite_execution_catalog_sys_summary_io_x_system_table_definition_count,
         mylite_execution_catalog_sys_summary_io_x_system_table_definition_at},
        {mylite_execution_catalog_sys_summary_memory_x_system_table_definition_count,
         mylite_execution_catalog_sys_summary_memory_x_system_table_definition_at},
        {mylite_execution_catalog_sys_summary_statement_x_system_table_definition_count,
         mylite_execution_catalog_sys_summary_statement_x_system_table_definition_at},
};

size_t mylite_execution_catalog_sys_summary_system_table_definition_count(void) {
    return count_sys_summary_system_table_definitions(
        (struct mylite_execution_catalog_sys_summary_system_table_provider_list){
            .providers = sys_summary_system_table_providers,
            .provider_count = sizeof(sys_summary_system_table_providers) /
                              sizeof(sys_summary_system_table_providers[0]),
        }
    );
}

const struct mylite_execution_catalog_mysql_system_table *mylite_execution_catalog_sys_summary_system_table_definition_at(
    size_t index
) {
    return sys_summary_system_table_definition_at(
        (struct mylite_execution_catalog_sys_summary_system_table_provider_list){
            .providers = sys_summary_system_table_providers,
            .provider_count = sizeof(sys_summary_system_table_providers) /
                              sizeof(sys_summary_system_table_providers[0]),
        },
        index
    );
}

size_t mylite_execution_catalog_sys_summary_x_system_table_definition_count(void) {
    return count_sys_summary_system_table_definitions(
        (struct mylite_execution_catalog_sys_summary_system_table_provider_list){
            .providers = sys_summary_x_system_table_providers,
            .provider_count = sizeof(sys_summary_x_system_table_providers) /
                              sizeof(sys_summary_x_system_table_providers[0]),
        }
    );
}

const struct mylite_execution_catalog_mysql_system_table *mylite_execution_catalog_sys_summary_x_system_table_definition_at(
    size_t index
) {
    return sys_summary_system_table_definition_at(
        (struct mylite_execution_catalog_sys_summary_system_table_provider_list){
            .providers = sys_summary_x_system_table_providers,
            .provider_count = sizeof(sys_summary_x_system_table_providers) /
                              sizeof(sys_summary_x_system_table_providers[0]),
        },
        index
    );
}

static size_t count_sys_summary_system_table_definitions(
    struct mylite_execution_catalog_sys_summary_system_table_provider_list provider_list
) {
    size_t total_count = 0U;

    for (size_t provider_index = 0U; provider_index < provider_list.provider_count;
         ++provider_index) {
        total_count += provider_list.providers[provider_index].count();
    }

    return total_count;
}

static const struct mylite_execution_catalog_mysql_system_table *sys_summary_system_table_definition_at(
    struct mylite_execution_catalog_sys_summary_system_table_provider_list provider_list,
    size_t index
) {
    for (size_t provider_index = 0U; provider_index < provider_list.provider_count;
         ++provider_index) {
        const size_t provider_table_count = provider_list.providers[provider_index].count();
        if (index < provider_table_count) {
            return provider_list.providers[provider_index].at(index);
        }
        index -= provider_table_count;
    }

    return NULL;
}
