#ifndef MYLITE_RUNTIME_MYLITE_FOREIGN_KEY_CATALOG_H
#define MYLITE_RUNTIME_MYLITE_FOREIGN_KEY_CATALOG_H

#include <mylite/mylite.h>

#include <stdbool.h>

int mylite_foreign_key_catalog_child_constraint_exists(
    mylite_db *database,
    const char *schema_name,
    const char *constraint_name,
    bool *out_exists
);
int mylite_foreign_key_catalog_delete_child_constraint(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    const char *constraint_name,
    bool temporary
);
int mylite_foreign_key_catalog_index_dependency_exists(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    const char *index_name,
    bool temporary,
    bool *out_has_dependency
);
int mylite_foreign_key_catalog_rewrite_child_table(
    mylite_db *database,
    const char *source_schema_name,
    const char *source_table_name,
    const char *target_schema_name,
    const char *target_table_name
);
int mylite_foreign_key_catalog_rewrite_parent_table(
    mylite_db *database,
    const char *source_schema_name,
    const char *source_table_name,
    const char *target_schema_name,
    const char *target_table_name
);
int mylite_foreign_key_catalog_rewrite_child_column(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    const char *source_column_name,
    const char *target_column_name
);
int mylite_foreign_key_catalog_rewrite_parent_column(
    mylite_db *database,
    const char *schema_name,
    const char *table_name,
    const char *source_column_name,
    const char *target_column_name
);

#endif
