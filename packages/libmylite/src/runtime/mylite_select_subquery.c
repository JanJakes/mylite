#include "mylite_select_subquery.h"

#include "mylite_runtime.h"
#include "mylite_sqlite_value.h"
#include "mylite_statement.h"

#include <stdlib.h>

int mylite_select_subquery_copy_row_values(mylite_stmt *stmt, size_t width,
                                           struct mylite_row_expression_values *out_values)
{
    *out_values = (struct mylite_row_expression_values){0};
    if (stmt == NULL || mylite_column_count(stmt) != (int)width) {
        return MYLITE_UNSUPPORTED;
    }
    if (width == 0U) {
        return MYLITE_UNSUPPORTED;
    }
    out_values->items = calloc(width, sizeof(*out_values->items));
    if (out_values->items == NULL) {
        return MYLITE_NOMEM;
    }
    out_values->count = width;
    for (size_t index = 0U; index < width; ++index) {
        int status = mylite_select_subquery_copy_row_value(stmt, index, &out_values->items[index]);

        if (status != MYLITE_OK) {
            mylite_select_subquery_row_values_deinit(out_values);
            return status;
        }
    }
    return MYLITE_OK;
}

int mylite_select_subquery_copy_row_value(mylite_stmt *stmt, size_t index,
                                          struct mylite_expression_value *out_value)
{
    const struct mylite_expression_value *value = NULL;

    if (stmt == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    if (stmt->kind == MYLITE_STMT_SCALAR_SELECT) {
        if (index >= stmt->scalar_result.value_count) {
            return MYLITE_UNSUPPORTED;
        }
        return mylite_expression_value_copy(&stmt->scalar_result.values[index], out_value) == 0
                   ? MYLITE_OK
                   : MYLITE_NOMEM;
    }

    value = mylite_statement_table_select_current_output_value(stmt, (int)index);
    if (value != NULL) {
        return mylite_expression_value_copy(value, out_value) == 0 ? MYLITE_OK : MYLITE_NOMEM;
    }
    if (stmt->sqlite_stmt != NULL) {
        return mylite_sqlite_copy_column_value(stmt->sqlite_stmt, index, out_value) == 0
                   ? MYLITE_OK
                   : MYLITE_NOMEM;
    }
    return MYLITE_UNSUPPORTED;
}

int mylite_select_subquery_copy_column_value(mylite_stmt *stmt,
                                             struct mylite_expression_value *out_value)
{
    const struct mylite_expression_value *value = NULL;

    if (stmt == NULL || mylite_column_count(stmt) != 1) {
        return MYLITE_UNSUPPORTED;
    }
    if (stmt->kind == MYLITE_STMT_SCALAR_SELECT) {
        return mylite_expression_value_copy(&stmt->scalar_result.values[0], out_value) == 0
                   ? MYLITE_OK
                   : MYLITE_NOMEM;
    }
    value = mylite_statement_table_select_current_output_value(stmt, 0);
    if (value != NULL) {
        return mylite_expression_value_copy(value, out_value) == 0 ? MYLITE_OK : MYLITE_NOMEM;
    }
    if (stmt->sqlite_stmt != NULL) {
        return mylite_sqlite_copy_column_value(stmt->sqlite_stmt, 0U, out_value) == 0
                   ? MYLITE_OK
                   : MYLITE_NOMEM;
    }
    return MYLITE_UNSUPPORTED;
}

int mylite_select_subquery_append_warnings(struct mylite_expression_warnings *destination,
                                           const struct mylite_expression_warnings *source)
{
    if (source == NULL) {
        return MYLITE_OK;
    }
    for (size_t index = 0U; index < source->count; ++index) {
        if (mylite_expression_warnings_append_condition(destination, source->items[index].level,
                                                        source->items[index].code,
                                                        source->items[index].message) != 0) {
            return MYLITE_NOMEM;
        }
    }
    return MYLITE_OK;
}

void mylite_select_subquery_row_values_deinit(struct mylite_row_expression_values *values)
{
    if (values == NULL) {
        return;
    }
    for (size_t index = 0U; index < values->count; ++index) {
        mylite_expression_value_deinit(&values->items[index]);
    }
    free(values->items);
    *values = (struct mylite_row_expression_values){0};
}
