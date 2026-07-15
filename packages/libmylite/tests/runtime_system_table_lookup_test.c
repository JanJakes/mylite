#include "runtime/mylite_execution_catalog.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

enum { decimal_conversion_base = 10 };

static int expect_true(bool condition, const char *context);

int main(void) {
    size_t definition_count = mylite_execution_catalog_mysql_system_table_definition_count();
    int failures = 0;

    failures += expect_true(definition_count > 0U, "system table definitions exist");
    for (size_t index = 0U; index < definition_count; ++index) {
        const struct mylite_execution_catalog_mysql_system_table *definition =
            mylite_execution_catalog_mysql_system_table_definition_at(index);
        const struct mylite_execution_catalog_mysql_system_table *lookup = NULL;

        failures += expect_true(definition != NULL, "indexed system table definition");
        if (definition == NULL) {
            continue;
        }
        lookup = mylite_execution_catalog_mysql_system_table_definition_by_name(
            definition->schema_name,
            definition->query_definition.name
        );
        failures += expect_true(lookup == definition, "system table lookup preserves identity");
    }
    failures += expect_true(
        mylite_execution_catalog_mysql_system_table_definition_by_name("wp", "wp_options") == NULL,
        "application schema misses system table lookup"
    );
    failures += expect_true(
        mylite_execution_catalog_mysql_system_table_definition_by_name("mysql", "missing") == NULL,
        "built-in schema unknown table"
    );
    failures += expect_true(
        mylite_execution_catalog_mysql_system_table_definition_by_name(NULL, "user") == NULL,
        "null schema lookup"
    );
    failures += expect_true(
        mylite_execution_catalog_mysql_system_table_definition_by_name("mysql", NULL) == NULL,
        "null table lookup"
    );
    for (size_t index = 0U; index < mylite_execution_catalog_scalar_collation_count(); ++index) {
        const struct mylite_execution_catalog_scalar_collation *collation =
            mylite_execution_catalog_scalar_collation_at(index);
        const struct mylite_execution_catalog_character_set *character_set =
            collation == NULL ? NULL
                              : mylite_execution_catalog_character_set_by_name(collation->charset);
        unsigned long max_bytes =
            character_set == NULL ? 0UL
                                  : strtoul(character_set->maxlen, NULL, decimal_conversion_base);

        failures += expect_true(collation != NULL, "indexed scalar collation");
        failures += expect_true(character_set != NULL, "scalar collation character set");
        failures += expect_true(
            collation != NULL && max_bytes == (unsigned long)collation->max_bytes_per_character,
            "scalar collation cached width"
        );
    }

    return failures == 0 ? 0 : 1;
}

static int expect_true(bool condition, const char *context) {
    if (condition) {
        return 0;
    }
    fprintf(stderr, "%s: expectation failed\n", context);
    return 1;
}
