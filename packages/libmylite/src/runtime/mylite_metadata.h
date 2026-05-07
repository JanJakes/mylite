#ifndef MYLITE_RUNTIME_MYLITE_METADATA_H
#define MYLITE_RUNTIME_MYLITE_METADATA_H

#include <mylite/mylite.h>

#include "mylite_metadata_types.h"

struct mylite_result_column_metadata_spec {
    const char *name;
    const char *schema_name;
    const char *table_name;
    const char *origin_schema_name;
    const char *origin_table_name;
    const char *origin_column_name;
    struct mylite_field_descriptor descriptor;
};

const struct mylite_result_column_metadata *mylite_result_metadata_column(
    const mylite_stmt *stmt,
    int column
);
int mylite_result_metadata_attach_columns(
    mylite_db *database,
    mylite_stmt *stmt,
    const struct mylite_result_column_metadata_spec *columns,
    size_t column_count
);
int mylite_result_metadata_copy_text(mylite_db *database, char **out_text, const char *text);
void mylite_result_metadata_deinit(struct mylite_result_metadata *metadata);
size_t mylite_result_metadata_label_count(
    const struct mylite_result_metadata *metadata,
    const char *label,
    size_t *out_index
);

#endif
