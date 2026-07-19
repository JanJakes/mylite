#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_SQL_LOWERING_SUPPORT_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_SQL_LOWERING_SUPPORT_H

#include <stdbool.h>
#include <stddef.h>

struct mylite_catalog_column_descriptor;
struct mylite_dynamic_string;
struct planned_select_source;
bool mylite_execution_column_descriptor_is_time(
    const struct mylite_catalog_column_descriptor *column
);

int mylite_execution_append_json_table_source_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_source *source,
    size_t source_index,
    size_t *next_parameter
);
bool mylite_execution_column_descriptor_uses_string_key_collation(
    const struct mylite_catalog_column_descriptor *column,
    bool include_text_family
);
int mylite_execution_append_string_key_collation_sql(struct mylite_dynamic_string *string);
int mylite_execution_append_mysql_quoted_text(
    struct mylite_dynamic_string *string,
    const char *text
);

#endif
