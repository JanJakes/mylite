#include "mylite_select_rowset_sort.h"

#include "mylite_diagnostics.h"
#include "mylite_select.h"

#include <stdlib.h>

// NOLINTNEXTLINE(misc-no-recursion)
static int merge_sort_table_select_rows(
    struct mylite_table_select_row *rows,
    struct mylite_table_select_row *scratch,
    size_t first,
    size_t last,
    const struct mylite_select_plan *plan
);

static void merge_table_select_rows(
    struct mylite_table_select_row *rows,
    struct mylite_table_select_row *scratch,
    size_t first,
    size_t middle,
    size_t last,
    const struct mylite_select_plan *plan
);

static int compare_table_select_rows(
    const struct mylite_table_select_row *left,
    const struct mylite_table_select_row *right,
    const struct mylite_select_plan *plan
);

int mylite_select_result_sort_rows(
    mylite_db *database,
    struct mylite_table_select_result *result,
    const struct mylite_select_plan *plan
) {
    size_t row_count = result->row_count;
    struct mylite_table_select_row *scratch = NULL;
    int status = MYLITE_OK;

    if (row_count < 2U) {
        return MYLITE_OK;
    }

    scratch = calloc(row_count, sizeof(*scratch));
    if (scratch == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = merge_sort_table_select_rows(result->rows, scratch, 0U, row_count, plan);
    free(scratch);
    return status;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int merge_sort_table_select_rows(
    struct mylite_table_select_row *rows,
    struct mylite_table_select_row *scratch,
    size_t first,
    size_t last,
    const struct mylite_select_plan *plan
) {
    size_t count = last - first;
    size_t middle = first + (count / 2U);

    if (count < 2U) {
        return MYLITE_OK;
    }

    int status = merge_sort_table_select_rows(rows, scratch, first, middle, plan);

    if (status == MYLITE_OK) {
        status = merge_sort_table_select_rows(rows, scratch, middle, last, plan);
    }
    if (status == MYLITE_OK) {
        merge_table_select_rows(rows, scratch, first, middle, last, plan);
    }
    return status;
}

static void merge_table_select_rows(
    struct mylite_table_select_row *rows,
    struct mylite_table_select_row *scratch,
    size_t first,
    size_t middle,
    size_t last,
    const struct mylite_select_plan *plan
) {
    size_t left = first;
    size_t right = middle;
    size_t output = first;

    while (left < middle && right < last) {
        if (compare_table_select_rows(&rows[left], &rows[right], plan) <= 0) {
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

static int compare_table_select_rows(
    const struct mylite_table_select_row *left,
    const struct mylite_table_select_row *right,
    const struct mylite_select_plan *plan
) {
    for (size_t index = 0U; index < plan->order_key_count; ++index) {
        int comparison =
            mylite_select_compare_values(&left->order_values[index], &right->order_values[index]);

        if (comparison != 0) {
            if (plan->order_keys[index].direction == MYLITE_SQL_AST_KEY_PART_ORDER_DESC) {
                comparison = -comparison;
            }
            return comparison;
        }
    }
    return 0;
}
