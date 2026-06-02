#include "mylite_execution_catalog_information_schema_internal.h"

#include <stdbool.h>
#include <stddef.h>

static bool catalog_text_equals_ascii_case_insensitive(const char *left, const char *right);
static char catalog_ascii_lower(char byte);

struct information_schema_table_provider {
    size_t (*count)(void);
    const struct mylite_execution_catalog_table_definition *(*at)(size_t index);
};

static const struct information_schema_table_provider information_schema_table_providers[] = {
    {mylite_execution_catalog_information_schema_extension_table_definition_count,
     mylite_execution_catalog_information_schema_extension_table_definition_at},
    {mylite_execution_catalog_information_schema_innodb_table_definition_count,
     mylite_execution_catalog_information_schema_innodb_table_definition_at},
    {mylite_execution_catalog_information_schema_metadata_table_definition_count,
     mylite_execution_catalog_information_schema_metadata_table_definition_at},
    {mylite_execution_catalog_information_schema_routine_table_definition_count,
     mylite_execution_catalog_information_schema_routine_table_definition_at},
    {mylite_execution_catalog_information_schema_access_table_definition_count,
     mylite_execution_catalog_information_schema_access_table_definition_at},
    {mylite_execution_catalog_information_schema_view_table_definition_count,
     mylite_execution_catalog_information_schema_view_table_definition_at},
};

static bool catalog_text_equals_ascii_case_insensitive(const char *left, const char *right) {
    if (left == NULL || right == NULL) {
        return left == right;
    }
    while (*left != '\0' && *right != '\0') {
        if (catalog_ascii_lower(*left) != catalog_ascii_lower(*right)) {
            return false;
        }
        ++left;
        ++right;
    }
    return *left == '\0' && *right == '\0';
}

static char catalog_ascii_lower(char byte) {
    if (byte >= 'A' && byte <= 'Z') {
        return (char)(byte - 'A' + 'a');
    }
    return byte;
}

size_t mylite_execution_catalog_information_schema_table_definition_count(void) {
    size_t total_count = 0U;

    for (size_t provider_index = 0U;
         provider_index <
         sizeof(information_schema_table_providers) / sizeof(information_schema_table_providers[0]);
         ++provider_index) {
        total_count += information_schema_table_providers[provider_index].count();
    }

    return total_count;
}

const struct mylite_execution_catalog_table_definition *mylite_execution_catalog_information_schema_table_definition_at(
    size_t index
) {
    for (size_t provider_index = 0U;
         provider_index <
         sizeof(information_schema_table_providers) / sizeof(information_schema_table_providers[0]);
         ++provider_index) {
        const size_t provider_count = information_schema_table_providers[provider_index].count();
        if (index < provider_count) {
            return information_schema_table_providers[provider_index].at(index);
        }
        index -= provider_count;
    }

    return NULL;
}

const struct mylite_execution_catalog_table_definition *mylite_execution_catalog_information_schema_table_definition_by_name(
    const char *table_name
) {
    if (table_name == NULL) {
        return NULL;
    }
    for (size_t index = 0U;
         index < mylite_execution_catalog_information_schema_table_definition_count();
         ++index) {
        const struct mylite_execution_catalog_table_definition *definition =
            mylite_execution_catalog_information_schema_table_definition_at(index);
        if (catalog_text_equals_ascii_case_insensitive(
                definition == NULL ? NULL : definition->name,
                table_name
            )) {
            return definition;
        }
    }
    return NULL;
}
