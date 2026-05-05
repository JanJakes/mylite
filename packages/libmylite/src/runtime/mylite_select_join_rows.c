#include "mylite_select_join_rows.h"

#include "mylite_diagnostics.h"
#include "mylite_expression.h"
#include "mylite_runtime.h"
#include "mylite_select.h"
#include "mylite_select_rowset.h"

#include <stdint.h>
#include <stdlib.h>

static int append_null_extended_right_row(mylite_stmt *stmt,
                                          const struct mylite_select_join_step *step,
                                          const struct mylite_table_select_row *right_row,
                                          size_t right_row_index,
                                          struct mylite_table_select_table_rowset *out_rowset);

int mylite_select_join_rowset_append_null_extended_left(
    mylite_stmt *stmt, const struct mylite_table_select_row *left_row,
    struct mylite_table_select_table_rowset *out_rowset)
{
    return mylite_select_rowset_append_row_copy(stmt->database, out_rowset, left_row);
}

int mylite_select_join_rowset_append_null_extended_right_unmatched(
    mylite_stmt *stmt, const struct mylite_select_join_step *step,
    const struct mylite_table_select_table_rowset *right, const bool *right_matched,
    struct mylite_table_select_table_rowset *out_rowset)
{
    for (size_t right_index = 0U; right_index < right->row_count; ++right_index) {
        if (right_matched != NULL && right_matched[right_index]) {
            continue;
        }
        int status = append_null_extended_right_row(stmt, step, &right->rows[right_index],
                                                    right_index, out_rowset);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

int mylite_select_join_rowset_append_empty(mylite_stmt *stmt,
                                           struct mylite_table_select_table_rowset *rowset,
                                           struct mylite_table_select_row **out_row)
{
    struct mylite_table_select_row row = {
        .value_count = mylite_select_plan_column_count(&stmt->select_plan),
        .source_row_index_count = mylite_select_plan_table_count(&stmt->select_plan),
        .source_rowid_count = mylite_select_plan_table_count(&stmt->select_plan),
    };
    int status = MYLITE_OK;

    *out_row = NULL;
    if (row.value_count != 0U) {
        row.values = calloc(row.value_count, sizeof(*row.values));
        if (row.values == NULL) {
            status = MYLITE_NOMEM;
        }
    }
    if (status == MYLITE_OK && row.source_row_index_count != 0U) {
        row.source_row_indexes =
            calloc(row.source_row_index_count, sizeof(*row.source_row_indexes));
        if (row.source_row_indexes == NULL) {
            status = MYLITE_NOMEM;
        }
    }
    if (status == MYLITE_OK && row.source_rowid_count != 0U) {
        row.source_rowids = calloc(row.source_rowid_count, sizeof(*row.source_rowids));
        if (row.source_rowids == NULL) {
            status = MYLITE_NOMEM;
        }
    }
    if (status != MYLITE_OK) {
        mylite_select_row_deinit(&row);
        (void)mylite_diagnostics_set_error_message(stmt->database, "out of memory");
        return status;
    }
    for (size_t index = 0U; index < row.source_row_index_count; ++index) {
        row.source_row_indexes[index] = SIZE_MAX;
    }

    status = mylite_select_rowset_append_row(stmt->database, rowset, &row);
    if (status != MYLITE_OK) {
        mylite_select_row_deinit(&row);
        return status;
    }
    *out_row = &rowset->rows[rowset->row_count - 1U];
    return MYLITE_OK;
}

int mylite_select_join_row_copy_base_table_values(mylite_db *database,
                                                  struct mylite_table_select_row *row,
                                                  const struct mylite_select_table *table,
                                                  size_t table_index,
                                                  const struct mylite_table_select_row *source,
                                                  size_t source_row_index)
{
    if (table == NULL || row->source_row_index_count <= table_index) {
        return MYLITE_UNSUPPORTED;
    }
    for (size_t index = 0U; index < table->column_count; ++index) {
        size_t column_index = table->first_column_index + index;

        if (column_index >= row->value_count || index >= source->value_count) {
            return MYLITE_UNSUPPORTED;
        }
        mylite_expression_value_deinit(&row->values[column_index]);
        if (mylite_expression_value_copy(&source->values[index], &row->values[column_index]) != 0) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
    }
    row->source_row_indexes[table_index] = source_row_index;
    if (row->source_rowid_count <= table_index || source->source_rowid_count <= table_index) {
        return MYLITE_UNSUPPORTED;
    }
    row->source_rowids[table_index] = source->source_rowids[table_index];
    return MYLITE_OK;
}

int mylite_select_join_row_copy_range_values(struct mylite_table_select_row *target,
                                             const struct mylite_table_select_row *source,
                                             struct mylite_select_table_range range,
                                             const struct mylite_select_plan *plan)
{
    size_t last_table = range.first_table + range.table_count;

    for (size_t table_index = range.first_table; table_index < last_table; ++table_index) {
        const struct mylite_select_table *table = mylite_select_plan_table_const(plan, table_index);

        if (table == NULL || table_index >= target->source_row_index_count ||
            table_index >= source->source_row_index_count) {
            return MYLITE_UNSUPPORTED;
        }
        target->source_row_indexes[table_index] = source->source_row_indexes[table_index];
        if (table_index >= target->source_rowid_count ||
            table_index >= source->source_rowid_count) {
            return MYLITE_UNSUPPORTED;
        }
        target->source_rowids[table_index] = source->source_rowids[table_index];
        for (size_t column = 0U; column < table->column_count; ++column) {
            size_t column_index = table->first_column_index + column;

            if (column_index >= target->value_count || column_index >= source->value_count) {
                return MYLITE_UNSUPPORTED;
            }
            mylite_expression_value_deinit(&target->values[column_index]);
            if (mylite_expression_value_copy(&source->values[column_index],
                                             &target->values[column_index]) != 0) {
                return MYLITE_NOMEM;
            }
        }
    }
    return MYLITE_OK;
}

int mylite_select_join_row_copy_table_values(struct mylite_table_select_row *row,
                                             const struct mylite_select_table *table,
                                             const struct mylite_table_select_row *source)
{
    for (size_t index = 0U; index < table->column_count; ++index) {
        size_t column_index = table->first_column_index + index;

        if (column_index >= row->value_count || index >= source->value_count ||
            mylite_expression_value_copy(&source->values[index], &row->values[column_index]) != 0) {
            for (size_t copied = 0U; copied < index; ++copied) {
                mylite_expression_value_deinit(&row->values[table->first_column_index + copied]);
            }
            return MYLITE_NOMEM;
        }
    }
    return MYLITE_OK;
}

void mylite_select_join_row_clear_table_values(struct mylite_table_select_row *row,
                                               const struct mylite_select_table *table)
{
    for (size_t index = 0U; index < table->column_count; ++index) {
        size_t column_index = table->first_column_index + index;

        if (column_index < row->value_count) {
            mylite_expression_value_deinit(&row->values[column_index]);
        }
    }
}

static int append_null_extended_right_row(mylite_stmt *stmt,
                                          const struct mylite_select_join_step *step,
                                          const struct mylite_table_select_row *right_row,
                                          size_t right_row_index,
                                          struct mylite_table_select_table_rowset *out_rowset)
{
    const struct mylite_select_table *right_table =
        mylite_select_plan_table_const(&stmt->select_plan, step->right_range.first_table);
    struct mylite_table_select_row *row = NULL;
    int status = mylite_select_join_rowset_append_empty(stmt, out_rowset, &row);

    if (status == MYLITE_OK && right_table == NULL) {
        status = MYLITE_UNSUPPORTED;
    }
    if (status == MYLITE_OK) {
        status = mylite_select_join_row_copy_base_table_values(stmt->database, row, right_table,
                                                               step->right_range.first_table,
                                                               right_row, right_row_index);
    }
    return status;
}
