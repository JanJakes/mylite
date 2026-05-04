#ifndef MYLITE_RUNTIME_MYLITE_METADATA_H
#define MYLITE_RUNTIME_MYLITE_METADATA_H

#include <mylite/mylite.h>

#include "mylite_metadata_types.h"

const struct mylite_result_column_metadata *mylite_result_metadata_column(const mylite_stmt *stmt,
                                                                          int column);
int mylite_result_metadata_copy_text(mylite_db *database, char **out_text, const char *text);
void mylite_result_metadata_deinit(struct mylite_result_metadata *metadata);
size_t mylite_result_metadata_label_count(const struct mylite_result_metadata *metadata,
                                          const char *label, size_t *out_index);

#endif
