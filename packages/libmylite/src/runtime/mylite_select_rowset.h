#ifndef MYLITE_RUNTIME_MYLITE_SELECT_ROWSET_H
#define MYLITE_RUNTIME_MYLITE_SELECT_ROWSET_H

#include <mylite/mylite.h>

#include "mylite_select_rowset_sort.h"
#include "mylite_select_types.h"

#include <stddef.h>

void mylite_select_result_deinit(struct mylite_table_select_result *result);
void mylite_select_result_current_values_deinit(struct mylite_table_select_result *result);
void mylite_select_row_deinit(struct mylite_table_select_row *row);
int mylite_select_row_copy(const struct mylite_table_select_row *row,
                           struct mylite_table_select_row *out_row);
int mylite_select_result_append_row(mylite_db *database, struct mylite_table_select_result *result,
                                    struct mylite_table_select_row *row);
int mylite_select_result_append_row_copy(mylite_db *database,
                                         struct mylite_table_select_result *result,
                                         const struct mylite_table_select_row *row);
int mylite_select_result_apply_limit(struct mylite_table_select_result *result,
                                     const struct mylite_select_limit *limit);
int mylite_select_rowset_append_row(mylite_db *database,
                                    struct mylite_table_select_table_rowset *rowset,
                                    struct mylite_table_select_row *row);
int mylite_select_rowset_append_row_copy(mylite_db *database,
                                         struct mylite_table_select_table_rowset *rowset,
                                         const struct mylite_table_select_row *row);
void mylite_select_rowset_deinit(struct mylite_table_select_table_rowset *rowset);
void mylite_select_rowsets_deinit(struct mylite_table_select_table_rowset *rowsets,
                                  size_t rowset_count);

#endif
