#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_DDL_SQL_LOWERING_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_DDL_SQL_LOWERING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct loaded_index_part;
struct mylite_catalog_column_descriptor;
struct mylite_db;
struct mylite_dynamic_string;
struct planned_alter_table_add_column;
struct planned_alter_table_add_index;
struct planned_alter_table_add_primary_key;
struct planned_alter_table_auto_increment;
struct planned_alter_table_force;
struct planned_alter_table_order_by;
struct planned_create_table;
struct planned_truncate_table;

int build_physical_table_name(int64_t table_id, char *destination, size_t destination_size);
int build_physical_view_name(int64_t table_id, char *destination, size_t destination_size);
int build_physical_index_name(int64_t index_id, char *destination, size_t destination_size);
int build_physical_check_constraint_name(
    int64_t check_constraint_id,
    char *destination,
    size_t destination_size
);
int build_create_table_sql(
    const struct planned_create_table *plan,
    const char *physical_name,
    bool temporary,
    char **out_sql
);
int build_create_table_definition_sql(
    const struct planned_create_table *plan,
    const char *physical_name,
    bool temporary,
    char **out_sql
);
int build_create_table_indexes_sql(
    const struct planned_create_table *plan,
    const char *physical_name,
    char **out_sql
);
int append_loaded_key_part_sql(
    struct mylite_dynamic_string *string,
    const struct loaded_index_part *part,
    const char *qualifier
);
int append_loaded_key_part_parameter_sql(
    struct mylite_dynamic_string *string,
    const struct loaded_index_part *part,
    size_t parameter_index
);
bool column_descriptor_uses_string_key_collation(
    const struct mylite_catalog_column_descriptor *column,
    bool include_text_family
);
int append_string_key_collation_sql(struct mylite_dynamic_string *string);
int build_drop_table_sql(const char *physical_name, char **out_sql);
int build_drop_index_sql(const char *physical_name, char **out_sql);
int build_alter_table_add_column_sql(
    struct mylite_db *database,
    const struct planned_alter_table_add_column *plan,
    char **out_sql
);
int build_alter_table_add_primary_key_null_validation_sql(
    const struct planned_alter_table_add_primary_key *plan,
    char **out_sql
);
int build_alter_table_add_primary_key_duplicate_validation_sql(
    const struct planned_alter_table_add_primary_key *plan,
    char **out_sql
);
int build_alter_table_add_primary_key_string_validation_sql(
    const struct planned_alter_table_add_primary_key *plan,
    char **out_sql
);
int build_alter_table_add_primary_key_index_sql(
    const struct planned_alter_table_add_primary_key *plan,
    const char *index_physical_name,
    char **out_sql
);
int build_add_index_sql(
    const struct planned_alter_table_add_index *plan,
    const char *index_physical_name,
    char **out_sql
);
int build_create_unique_index_string_validation_sql(
    const struct planned_alter_table_add_index *plan,
    char **out_sql
);
int build_create_unique_index_duplicate_validation_sql(
    const struct planned_alter_table_add_index *plan,
    char **out_sql
);
int build_alter_table_auto_increment_max_sql(
    const struct planned_alter_table_auto_increment *plan,
    char **out_sql
);
int build_alter_table_order_temporary_physical_name(
    const struct planned_alter_table_order_by *plan,
    uint64_t sqlite_schema_generation,
    char *destination,
    size_t destination_size
);
int build_alter_table_order_create_sql(
    const struct planned_alter_table_order_by *plan,
    const char *temporary_physical_name,
    char **out_sql
);
int build_alter_table_order_copy_sql(
    const struct planned_alter_table_order_by *plan,
    const char *temporary_physical_name,
    char **out_sql
);
int build_alter_table_force_temporary_physical_name(
    const struct planned_alter_table_force *plan,
    uint64_t sqlite_schema_generation,
    char *destination,
    size_t destination_size
);
int build_alter_table_force_create_sql(
    const struct planned_alter_table_force *plan,
    const char *temporary_physical_name,
    char **out_sql
);
int build_alter_table_force_copy_sql(
    const struct planned_alter_table_force *plan,
    const char *temporary_physical_name,
    char **out_sql
);
int build_alter_table_rename_physical_table_sql(
    const char *source_physical_name,
    const char *target_physical_name,
    char **out_sql
);
int build_truncate_table_sql(const struct planned_truncate_table *plan, char **out_sql);

#endif
