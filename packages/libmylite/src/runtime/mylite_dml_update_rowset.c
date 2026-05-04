#include "mylite_dml.h"

#include "mylite_diagnostics.h"
#include "mylite_field_descriptor.h"
#include "mylite_select.h"
#include "mylite_select_types.h"
#include "mylite_span.h"
#include "sql/mylite_expression.h"
#include "sqlite3.h"

#include <stdlib.h>

static int copy_update_sqlite_column_value(sqlite3_stmt *scan, int column,
                                           const struct mylite_field_descriptor *descriptor,
                                           struct mylite_expression_value *out_value);
static int merge_sort_update_rows(struct mylite_update_row *rows, struct mylite_update_row *scratch,
                                  size_t first, size_t last,
                                  const struct mylite_update_order_plan *order_plan);
static void merge_update_rows(struct mylite_update_row *rows, struct mylite_update_row *scratch,
                              size_t first, size_t middle, size_t last,
                              const struct mylite_update_order_plan *order_plan);
static int compare_update_rows(const struct mylite_update_row *left,
                               const struct mylite_update_row *right,
                               const struct mylite_update_order_plan *order_plan);

int mylite_dml_copy_update_sqlite_row(mylite_db *database, const struct mylite_select_table *table,
                                      sqlite3_stmt *scan, struct mylite_update_row *out_row)
{
    if (database == NULL || table == NULL || scan == NULL || out_row == NULL) {
        return MYLITE_MISUSE;
    }
    if (table->column_count == 0U) {
        return mylite_diagnostics_set_table_doesnt_exist_error(database, table->schema_name,
                                                               table->table_name);
    }

    out_row->rowid = sqlite3_column_int64(scan, 0);
    out_row->values = calloc(table->column_count, sizeof(*out_row->values));
    if (out_row->values == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    out_row->value_count = table->column_count;

    for (size_t index = 0U; index < table->column_count; ++index) {
        if (copy_update_sqlite_column_value(scan, (int)index + 1, &table->columns[index].descriptor,
                                            &out_row->values[index]) != 0) {
            mylite_dml_update_row_deinit(out_row);
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
    }
    return MYLITE_OK;
}

int mylite_dml_append_update_row(mylite_db *database, struct mylite_update_rowset *rowset,
                                 struct mylite_update_row *row)
{
    struct mylite_update_row *rows = NULL;

    if (database == NULL || rowset == NULL || row == NULL) {
        return MYLITE_MISUSE;
    }

    rows = realloc(rowset->rows, (rowset->row_count + 1U) * sizeof(*rowset->rows));
    if (rows == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    rowset->rows = rows;
    rowset->rows[rowset->row_count++] = *row;
    *row = (struct mylite_update_row){0};
    return MYLITE_OK;
}

int mylite_dml_sort_update_rowset(struct mylite_update_rowset *rowset,
                                  const struct mylite_update_order_plan *order_plan)
{
    struct mylite_update_row *scratch = NULL;
    int status = MYLITE_OK;

    if (rowset == NULL || order_plan == NULL) {
        return MYLITE_MISUSE;
    }
    if (order_plan->order_key_count == 0U || rowset->row_count < 2U) {
        return MYLITE_OK;
    }

    scratch = calloc(rowset->row_count, sizeof(*scratch));
    if (scratch == NULL) {
        return MYLITE_NOMEM;
    }
    status = merge_sort_update_rows(rowset->rows, scratch, 0U, rowset->row_count, order_plan);
    free(scratch);
    return status;
}

void mylite_dml_apply_update_limit(const struct mylite_sql_ast_node *limit_clause,
                                   struct mylite_update_rowset *rowset)
{
    const struct mylite_sql_ast_node *bound = mylite_ast_child_at(limit_clause, 0U);
    size_t keep_count = 0U;

    if (limit_clause == NULL || rowset == NULL) {
        return;
    }
    if (bound == NULL || !bound->has_limit_bound_value) {
        return;
    }
    if (bound->limit_bound_value > (uint64_t)SIZE_MAX) {
        return;
    }

    keep_count = (size_t)bound->limit_bound_value;
    if (keep_count >= rowset->row_count) {
        return;
    }
    for (size_t index = keep_count; index < rowset->row_count; ++index) {
        mylite_dml_update_row_deinit(&rowset->rows[index]);
    }
    rowset->row_count = keep_count;
}

static int copy_update_sqlite_column_value(sqlite3_stmt *scan, int column,
                                           const struct mylite_field_descriptor *descriptor,
                                           struct mylite_expression_value *out_value)
{
    int sqlite_type = sqlite3_column_type(scan, column);

    switch (sqlite_type) {
    case SQLITE_NULL:
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        return 0;
    case SQLITE_INTEGER:
        *out_value = (struct mylite_expression_value){
            .kind = MYLITE_EXPRESSION_VALUE_INT64,
            .int64_value = sqlite3_column_int64(scan, column),
        };
        return 0;
    case SQLITE_FLOAT:
        *out_value = (struct mylite_expression_value){
            .kind = MYLITE_EXPRESSION_VALUE_REAL,
            .real_value = sqlite3_column_double(scan, column),
        };
        return 0;
    case SQLITE_TEXT:
    case SQLITE_BLOB: {
        const unsigned char *text = sqlite3_column_text(scan, column);
        int bytes = sqlite3_column_bytes(scan, column);

        out_value->kind = MYLITE_EXPRESSION_VALUE_TEXT;
        out_value->text_length = bytes < 0 ? 0U : (size_t)bytes;
        out_value->text_value = mylite_copy_span_text((const char *)text, out_value->text_length);
        out_value->preserve_temporal_fraction_digits =
            mylite_field_descriptor_preserves_temporal_fraction_digits(descriptor);
        out_value->temporal_type = mylite_field_descriptor_expression_temporal_type(descriptor);
        return out_value->text_value == NULL ? -1 : 0;
    }
    default:
        break;
    }
    return -1;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int merge_sort_update_rows(struct mylite_update_row *rows, struct mylite_update_row *scratch,
                                  size_t first, size_t last,
                                  const struct mylite_update_order_plan *order_plan)
{
    size_t count = last - first;
    size_t middle = first + (count / 2U);
    int status = MYLITE_OK;

    if (count < 2U) {
        return MYLITE_OK;
    }

    status = merge_sort_update_rows(rows, scratch, first, middle, order_plan);
    if (status == MYLITE_OK) {
        status = merge_sort_update_rows(rows, scratch, middle, last, order_plan);
    }
    if (status == MYLITE_OK) {
        merge_update_rows(rows, scratch, first, middle, last, order_plan);
    }
    return status;
}

static void merge_update_rows(struct mylite_update_row *rows, struct mylite_update_row *scratch,
                              size_t first, size_t middle, size_t last,
                              const struct mylite_update_order_plan *order_plan)
{
    size_t left = first;
    size_t right = middle;
    size_t output = first;

    while (left < middle && right < last) {
        if (compare_update_rows(&rows[left], &rows[right], order_plan) <= 0) {
            scratch[output++] = rows[left++];
        } else {
            scratch[output++] = rows[right++];
        }
    }
    while (left < middle) {
        scratch[output++] = rows[left++];
    }
    while (right < last) {
        scratch[output++] = rows[right++];
    }
    for (size_t index = first; index < last; ++index) {
        rows[index] = scratch[index];
    }
}

static int compare_update_rows(const struct mylite_update_row *left,
                               const struct mylite_update_row *right,
                               const struct mylite_update_order_plan *order_plan)
{
    for (size_t index = 0U; index < order_plan->order_key_count; ++index) {
        int comparison =
            mylite_select_compare_values(&left->order_values[index], &right->order_values[index]);

        if (comparison != 0) {
            if (order_plan->order_keys[index].direction == MYLITE_SQL_AST_KEY_PART_ORDER_DESC) {
                comparison = -comparison;
            }
            return comparison;
        }
    }
    return 0;
}
