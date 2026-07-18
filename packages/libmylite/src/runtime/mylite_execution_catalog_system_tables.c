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

static bool system_table_definition_is_valid(
    const char *provider_schema_name,
    const struct mylite_execution_catalog_mysql_system_table *definition
);
static bool system_table_definition_has_valid_shape(
    const char *provider_schema_name,
    const struct mylite_execution_catalog_mysql_system_table *definition
);
static bool system_table_column_definition_is_valid(
    const struct mylite_execution_catalog_column_definition *column
);
static bool system_table_definition_is_listed(
    const struct mylite_execution_catalog_mysql_system_table *definition
);

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
            const struct mylite_execution_catalog_mysql_system_table *definition =
                system_table_providers[provider_index].at(index);

            if (!system_table_definition_has_valid_shape(
                    system_table_providers[provider_index].schema_name,
                    definition
                )) {
                return NULL;
            }
            return definition;
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

            if (system_table_definition_has_valid_shape(provider->schema_name, definition) &&
                strcmp(table_name, definition->query_definition.name) == 0) {
                return definition;
            }
        }
    }

    return NULL;
}

bool mylite_execution_catalog_validate_system_table_definitions(void) {
    const size_t definition_count = mylite_execution_catalog_mysql_system_table_definition_count();

    for (size_t index = 0U; index < definition_count; ++index) {
        const struct mylite_execution_catalog_mysql_system_table *definition =
            mylite_execution_catalog_mysql_system_table_definition_at(index);

        if (definition == NULL ||
            !system_table_definition_is_valid(definition->schema_name, definition) ||
            !system_table_definition_is_listed(definition)) {
            return false;
        }
        for (size_t other_index = index + 1U; other_index < definition_count; ++other_index) {
            const struct mylite_execution_catalog_mysql_system_table *other =
                mylite_execution_catalog_mysql_system_table_definition_at(other_index);

            if (other == NULL ||
                definition->query_definition.kind == other->query_definition.kind ||
                (strcmp(definition->schema_name, other->schema_name) == 0 &&
                 strcmp(definition->query_definition.name, other->query_definition.name) == 0)) {
                return false;
            }
        }
        for (size_t column_index = 0U; column_index < definition->query_definition.column_count;
             ++column_index) {
            for (size_t other_column_index = column_index + 1U;
                 other_column_index < definition->query_definition.column_count;
                 ++other_column_index) {
                if (strcmp(
                        definition->query_definition.columns[column_index].name,
                        definition->query_definition.columns[other_column_index].name
                    ) == 0) {
                    return false;
                }
            }
        }
    }
    return true;
}

static bool system_table_definition_is_valid(
    const char *provider_schema_name,
    const struct mylite_execution_catalog_mysql_system_table *definition
) {
    const size_t column_count = definition == NULL ? 0U : definition->query_definition.column_count;

    if (!system_table_definition_has_valid_shape(provider_schema_name, definition)) {
        return false;
    }
    for (size_t index = 0U; index < column_count; ++index) {
        if (!system_table_column_definition_is_valid(&definition->query_definition.columns[index]
            ) ||
            definition->column_keys[index] == NULL || definition->column_extras[index] == NULL ||
            definition->column_privileges[index] == NULL ||
            (definition->column_comments != NULL && definition->column_comments[index] == NULL) ||
            (definition->column_generation_expressions != NULL &&
             definition->column_generation_expressions[index] == NULL)) {
            return false;
        }
    }
    for (size_t index = 0U; index < definition->primary_key_column_count; ++index) {
        size_t column_index = definition->primary_key_column_indexes[index];

        if (column_index >= column_count ||
            strcmp(definition->column_keys[column_index], "PRI") != 0) {
            return false;
        }
        for (size_t previous = 0U; previous < index; ++previous) {
            if (definition->primary_key_column_indexes[previous] == column_index) {
                return false;
            }
        }
    }
    for (size_t index = 0U; index < definition->secondary_index_count; ++index) {
        const struct mylite_execution_catalog_mysql_system_secondary_index *secondary =
            &definition->secondary_indexes[index];

        if (secondary->name == NULL || secondary->name[0] == '\0' ||
            secondary->column_index >= column_count || secondary->non_unique == NULL ||
            secondary->index_type == NULL) {
            return false;
        }
    }
    return true;
}

static bool system_table_definition_has_valid_shape(
    const char *provider_schema_name,
    const struct mylite_execution_catalog_mysql_system_table *definition
) {
    size_t column_count = 0U;

    if (provider_schema_name == NULL || definition == NULL || definition->schema_name == NULL ||
        strcmp(provider_schema_name, definition->schema_name) != 0 ||
        definition->query_definition.name == NULL || definition->query_definition.name[0] == '\0' ||
        definition->query_definition.columns == NULL ||
        definition->query_definition.column_count == 0U) {
        return false;
    }
    column_count = definition->query_definition.column_count;
    if (definition->column_keys == NULL || definition->column_key_count < column_count ||
        definition->column_extras == NULL || definition->column_extra_count < column_count ||
        definition->column_privileges == NULL ||
        definition->column_privilege_count < column_count ||
        ((definition->column_comments == NULL) != (definition->column_comment_count == 0U)) ||
        (definition->column_comment_count != 0U && definition->column_comment_count < column_count
        ) ||
        ((definition->column_generation_expressions == NULL) !=
         (definition->column_generation_expression_count == 0U)) ||
        (definition->column_generation_expression_count != 0U &&
         definition->column_generation_expression_count < column_count) ||
        ((definition->primary_key_column_indexes == NULL) !=
         (definition->primary_key_column_count == 0U)) ||
        ((definition->secondary_indexes == NULL) != (definition->secondary_index_count == 0U))) {
        return false;
    }
    return true;
}

static bool system_table_column_definition_is_valid(
    const struct mylite_execution_catalog_column_definition *column
) {
    if (column == NULL || column->name == NULL || column->name[0] == '\0' ||
        column->is_nullable == NULL || column->data_type == NULL || column->column_type == NULL) {
        return false;
    }
    return true;
}

static bool system_table_definition_is_listed(
    const struct mylite_execution_catalog_mysql_system_table *definition
) {
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory =
        mylite_execution_catalog_builtin_schema_table_directory_by_name(definition->schema_name);

    if (directory == NULL || directory->table_names == NULL) {
        return false;
    }
    for (size_t index = 0U; index < directory->table_count; ++index) {
        if (directory->table_names[index] != NULL &&
            strcmp(directory->table_names[index], definition->query_definition.name) == 0) {
            return true;
        }
    }
    return false;
}
