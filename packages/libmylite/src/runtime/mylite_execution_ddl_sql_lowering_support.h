#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_DDL_SQL_LOWERING_SUPPORT_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_DDL_SQL_LOWERING_SUPPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct integer_column_range;
struct mylite_catalog_column_descriptor;
struct mylite_db;
struct mylite_dynamic_string;
struct planned_column;
struct planned_alter_table_add_column;
struct planned_secondary_index;

int mylite_execution_ddl_integer_range_for_logical_type(
    struct mylite_db *database,
    const char *logical_type,
    const char *unsupported_message,
    struct integer_column_range *out_range
);
int mylite_execution_ddl_append_numbered_parameter(
    struct mylite_dynamic_string *string,
    size_t parameter_index
);
int mylite_execution_ddl_append_size_literal(struct mylite_dynamic_string *string, size_t value);
int mylite_execution_ddl_append_uint64_literal(
    struct mylite_dynamic_string *string,
    uint64_t value
);
bool mylite_execution_ddl_planned_secondary_index_is_fulltext(
    const struct planned_secondary_index *index
);
bool mylite_execution_ddl_planned_secondary_index_is_spatial(
    const struct planned_secondary_index *index
);
bool mylite_execution_ddl_planned_column_is_char_or_varchar(const struct planned_column *column);
bool mylite_execution_ddl_planned_column_is_string_family(const struct planned_column *column);
bool mylite_execution_ddl_column_descriptor_is_string_family(
    const struct mylite_catalog_column_descriptor *column
);
bool mylite_execution_ddl_column_descriptor_is_char_or_varchar(
    const struct mylite_catalog_column_descriptor *column
);
bool mylite_execution_ddl_text_equals_ascii_case_insensitive(const char *left, const char *right);
bool mylite_execution_ddl_loaded_index_part_requires_string_key_validation(
    const struct loaded_index_part *part
);
int mylite_execution_ddl_append_alter_table_add_column_default(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_column *plan
);

#endif
