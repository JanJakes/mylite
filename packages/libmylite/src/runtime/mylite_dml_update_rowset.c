#include "mylite_dml.h"

#include "mylite_select.h"
#include "mylite_select_types.h"
#include "mylite_span.h"

#include <stdlib.h>

static int merge_sort_update_rows(struct mylite_update_row *rows, struct mylite_update_row *scratch,
                                  size_t first, size_t last,
                                  const struct mylite_update_order_plan *order_plan);
static void merge_update_rows(struct mylite_update_row *rows, struct mylite_update_row *scratch,
                              size_t first, size_t middle, size_t last,
                              const struct mylite_update_order_plan *order_plan);
static int compare_update_rows(const struct mylite_update_row *left,
                               const struct mylite_update_row *right,
                               const struct mylite_update_order_plan *order_plan);

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
