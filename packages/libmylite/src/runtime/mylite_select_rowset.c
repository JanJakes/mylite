#include "mylite_select_rowset.h"

#include "mylite_diagnostics.h"
#include "mylite_select.h"

#include <stdlib.h>
#include <string.h>

static int allocate_table_select_row_copy(struct mylite_table_select_row *out_row);
static int allocate_table_select_expression_values(struct mylite_expression_value **out_values,
                                                   size_t value_count);
static int allocate_table_select_source_row_indexes(size_t **out_indexes, size_t index_count);
static int copy_table_select_row_copy_values(const struct mylite_table_select_row *row,
                                             struct mylite_table_select_row *out_row);
static int copy_table_select_expression_values(const struct mylite_expression_value *values,
                                               struct mylite_expression_value *out_values,
                                               size_t value_count);

void mylite_select_result_deinit(struct mylite_table_select_result *result)
{
    if (result == NULL) {
        return;
    }

    mylite_select_result_current_values_deinit(result);
    for (size_t index = 0U; index < result->row_count; ++index) {
        mylite_select_row_deinit(&result->rows[index]);
    }
    free(result->rows);
    *result = (struct mylite_table_select_result){0};
}

void mylite_select_result_current_values_deinit(struct mylite_table_select_result *result)
{
    if (result == NULL) {
        return;
    }
    for (size_t index = 0U; index < result->current_value_count; ++index) {
        mylite_expression_value_deinit(&result->current_values[index]);
        free(result->current_texts[index]);
    }
    free(result->current_values);
    free((void *)result->current_texts);
    result->current_values = NULL;
    result->current_texts = NULL;
    result->current_value_count = 0U;
    result->has_current_row = false;
}

void mylite_select_row_deinit(struct mylite_table_select_row *row)
{
    if (row == NULL) {
        return;
    }

    for (size_t index = 0U; index < row->value_count; ++index) {
        mylite_expression_value_deinit(&row->values[index]);
    }
    for (size_t index = 0U; index < row->output_value_count; ++index) {
        mylite_expression_value_deinit(&row->output_values[index]);
    }
    for (size_t index = 0U; index < row->order_value_count; ++index) {
        mylite_expression_value_deinit(&row->order_values[index]);
    }
    for (size_t index = 0U; index < row->aggregate_value_count; ++index) {
        mylite_expression_value_deinit(&row->aggregate_values[index]);
    }
    free(row->values);
    free(row->output_values);
    free(row->order_values);
    free(row->aggregate_values);
    free(row->source_row_indexes);
    *row = (struct mylite_table_select_row){0};
}

int mylite_select_row_copy(const struct mylite_table_select_row *row,
                           struct mylite_table_select_row *out_row)
{
    int status = MYLITE_OK;

    *out_row = (struct mylite_table_select_row){0};
    out_row->value_count = row->value_count;
    out_row->output_value_count = row->output_value_count;
    out_row->order_value_count = row->order_value_count;
    out_row->aggregate_value_count = row->aggregate_value_count;
    out_row->source_row_index_count = row->source_row_index_count;

    status = allocate_table_select_row_copy(out_row);
    if (status == MYLITE_OK) {
        status = copy_table_select_row_copy_values(row, out_row);
    }
    if (status != MYLITE_OK) {
        mylite_select_row_deinit(out_row);
    }
    return status;
}

int mylite_select_result_append_row(mylite_db *database, struct mylite_table_select_result *result,
                                    struct mylite_table_select_row *row)
{
    struct mylite_table_select_row *rows =
        realloc(result->rows, (result->row_count + 1U) * sizeof(*result->rows));

    if (rows == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    result->rows = rows;
    result->rows[result->row_count++] = *row;
    *row = (struct mylite_table_select_row){0};
    return MYLITE_OK;
}

int mylite_select_result_append_row_copy(mylite_db *database,
                                         struct mylite_table_select_result *result,
                                         const struct mylite_table_select_row *row)
{
    struct mylite_table_select_row copy = {0};
    int status = mylite_select_row_copy(row, &copy);

    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
    }
    if (status == MYLITE_OK) {
        status = mylite_select_result_append_row(database, result, &copy);
    }
    mylite_select_row_deinit(&copy);
    return status;
}

int mylite_select_result_apply_limit(struct mylite_table_select_result *result,
                                     const struct mylite_select_limit *limit)
{
    size_t kept = 0U;

    if (!limit->has_limit) {
        return MYLITE_OK;
    }

    for (size_t index = 0U; index < result->row_count; ++index) {
        if (mylite_select_limit_row_is_kept(limit, (struct mylite_select_limit_position){
                                                       .matched_row = (uint64_t)index,
                                                       .kept_count = kept,
                                                   })) {
            if (kept != index) {
                result->rows[kept] = result->rows[index];
                result->rows[index] = (struct mylite_table_select_row){0};
            }
            ++kept;
        } else {
            mylite_select_row_deinit(&result->rows[index]);
        }
    }
    result->row_count = kept;
    return MYLITE_OK;
}

int mylite_select_rowset_append_row(mylite_db *database,
                                    struct mylite_table_select_table_rowset *rowset,
                                    struct mylite_table_select_row *row)
{
    struct mylite_table_select_row *rows =
        realloc(rowset->rows, (rowset->row_count + 1U) * sizeof(*rowset->rows));

    if (rows == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    rowset->rows = rows;
    rowset->rows[rowset->row_count++] = *row;
    *row = (struct mylite_table_select_row){0};
    return MYLITE_OK;
}

int mylite_select_rowset_append_row_copy(mylite_db *database,
                                         struct mylite_table_select_table_rowset *rowset,
                                         const struct mylite_table_select_row *row)
{
    struct mylite_table_select_row copy = {0};
    int status = mylite_select_row_copy(row, &copy);

    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = mylite_select_rowset_append_row(database, rowset, &copy);
    if (status != MYLITE_OK) {
        mylite_select_row_deinit(&copy);
    }
    return status;
}

void mylite_select_rowset_deinit(struct mylite_table_select_table_rowset *rowset)
{
    if (rowset == NULL) {
        return;
    }
    for (size_t row_index = 0U; row_index < rowset->row_count; ++row_index) {
        mylite_select_row_deinit(&rowset->rows[row_index]);
    }
    free(rowset->rows);
    *rowset = (struct mylite_table_select_table_rowset){0};
}

void mylite_select_rowsets_deinit(struct mylite_table_select_table_rowset *rowsets,
                                  size_t rowset_count)
{
    if (rowsets == NULL) {
        return;
    }
    for (size_t rowset_index = 0U; rowset_index < rowset_count; ++rowset_index) {
        mylite_select_rowset_deinit(&rowsets[rowset_index]);
    }
    free(rowsets);
}

static int allocate_table_select_row_copy(struct mylite_table_select_row *out_row)
{
    int status = allocate_table_select_expression_values(&out_row->values, out_row->value_count);

    if (status == MYLITE_OK) {
        status = allocate_table_select_expression_values(&out_row->output_values,
                                                         out_row->output_value_count);
    }
    if (status == MYLITE_OK) {
        status = allocate_table_select_expression_values(&out_row->order_values,
                                                         out_row->order_value_count);
    }
    if (status == MYLITE_OK) {
        status = allocate_table_select_expression_values(&out_row->aggregate_values,
                                                         out_row->aggregate_value_count);
    }
    if (status == MYLITE_OK) {
        status = allocate_table_select_source_row_indexes(&out_row->source_row_indexes,
                                                          out_row->source_row_index_count);
    }
    return status;
}

static int allocate_table_select_expression_values(struct mylite_expression_value **out_values,
                                                   size_t value_count)
{
    if (value_count == 0U) {
        return MYLITE_OK;
    }
    *out_values = calloc(value_count, sizeof(**out_values));
    return *out_values == NULL ? MYLITE_NOMEM : MYLITE_OK;
}

static int allocate_table_select_source_row_indexes(size_t **out_indexes, size_t index_count)
{
    if (index_count == 0U) {
        return MYLITE_OK;
    }
    *out_indexes = calloc(index_count, sizeof(**out_indexes));
    return *out_indexes == NULL ? MYLITE_NOMEM : MYLITE_OK;
}

static int copy_table_select_row_copy_values(const struct mylite_table_select_row *row,
                                             struct mylite_table_select_row *out_row)
{
    int status =
        copy_table_select_expression_values(row->values, out_row->values, out_row->value_count);

    if (status == MYLITE_OK) {
        status = copy_table_select_expression_values(row->output_values, out_row->output_values,
                                                     out_row->output_value_count);
    }
    if (status == MYLITE_OK) {
        status = copy_table_select_expression_values(row->order_values, out_row->order_values,
                                                     out_row->order_value_count);
    }
    if (status == MYLITE_OK) {
        status = copy_table_select_expression_values(
            row->aggregate_values, out_row->aggregate_values, out_row->aggregate_value_count);
    }
    if (status == MYLITE_OK && out_row->source_row_index_count != 0U) {
        memcpy(out_row->source_row_indexes, row->source_row_indexes,
               out_row->source_row_index_count * sizeof(*out_row->source_row_indexes));
    }
    return status;
}

static int copy_table_select_expression_values(const struct mylite_expression_value *values,
                                               struct mylite_expression_value *out_values,
                                               size_t value_count)
{
    for (size_t index = 0U; index < value_count; ++index) {
        if (mylite_expression_value_copy(&values[index], &out_values[index]) != 0) {
            return MYLITE_NOMEM;
        }
    }
    return MYLITE_OK;
}
