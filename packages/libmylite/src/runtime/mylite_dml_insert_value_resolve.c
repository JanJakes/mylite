#include "mylite_dml.h"

#include "mylite_dml_insert_default.h"
#include "mylite_dml_insert_diagnostics.h"

#include <stdint.h>

static int resolve_insert_column_list_row_values(
    mylite_db *database, const struct mylite_insert_values_plan *plan,
    const struct mylite_insert_table *table, const struct mylite_insert_row *row,
    const size_t *column_indexes, uint64_t statement_row_count,
    struct mylite_insert_execution_state *state, struct mylite_insert_bound_value *values);
static const struct mylite_insert_value *
insert_column_list_value_for_column(const struct mylite_insert_values_plan *plan,
                                    const struct mylite_insert_row *row,
                                    const size_t *column_indexes, size_t column);
static int resolve_insert_positional_row_values(mylite_db *database,
                                                const struct mylite_insert_values_plan *plan,
                                                const struct mylite_insert_table *table,
                                                size_t row_index, uint64_t statement_row_count,
                                                struct mylite_insert_execution_state *state,
                                                struct mylite_insert_bound_value *values);
static int resolve_insert_default_row_values(mylite_db *database,
                                             const struct mylite_insert_values_plan *plan,
                                             const struct mylite_insert_table *table,
                                             uint64_t statement_row_count,
                                             struct mylite_insert_execution_state *state,
                                             struct mylite_insert_bound_value *values);
static int
resolve_insert_explicit_value(mylite_db *database, const struct mylite_insert_values_plan *plan,
                              const struct mylite_insert_table_column *column,
                              const struct mylite_insert_value *value, uint64_t statement_row_count,
                              struct mylite_insert_execution_state *state, size_t column_index,
                              struct mylite_insert_bound_value *out_value);
static bool insert_row_uses_all_defaults(const struct mylite_insert_values_plan *plan,
                                         size_t row_index);
static size_t insert_row_target_column_count(const struct mylite_insert_values_plan *plan,
                                             const struct mylite_insert_table *table,
                                             size_t row_index);

int mylite_dml_resolve_insert_row_values(mylite_db *database,
                                         const struct mylite_insert_values_plan *plan,
                                         const struct mylite_insert_table *table,
                                         const size_t *column_indexes, uint64_t statement_row_count,
                                         struct mylite_insert_execution_state *state,
                                         size_t row_index,
                                         struct mylite_insert_bound_value *out_values)
{
    const struct mylite_insert_row *row = NULL;
    size_t expected_count = 0U;

    if (database == NULL || plan == NULL || table == NULL || state == NULL || out_values == NULL ||
        row_index >= plan->row_count) {
        return MYLITE_MISUSE;
    }

    row = &plan->rows[row_index];
    expected_count = insert_row_target_column_count(plan, table, row_index);
    if (row->value_count != expected_count) {
        return mylite_dml_insert_set_wrong_value_count_error(database, row_index);
    }
    if (plan->has_column_list) {
        return resolve_insert_column_list_row_values(database, plan, table, row, column_indexes,
                                                     statement_row_count, state, out_values);
    }
    return resolve_insert_positional_row_values(database, plan, table, row_index,
                                                statement_row_count, state, out_values);
}

static int resolve_insert_column_list_row_values(
    mylite_db *database, const struct mylite_insert_values_plan *plan,
    const struct mylite_insert_table *table, const struct mylite_insert_row *row,
    const size_t *column_indexes, uint64_t statement_row_count,
    struct mylite_insert_execution_state *state, struct mylite_insert_bound_value *values)
{
    if (plan->column_count == 0U) {
        return resolve_insert_default_row_values(database, plan, table, statement_row_count, state,
                                                 values);
    }
    if (column_indexes == NULL) {
        return MYLITE_UNSUPPORTED;
    }

    for (size_t column = 0U; column < table->column_count; ++column) {
        const struct mylite_insert_value *explicit_value =
            insert_column_list_value_for_column(plan, row, column_indexes, column);
        int status = explicit_value == NULL
                         ? mylite_dml_resolve_insert_omitted_default_value(
                               database, plan, &table->columns[column], statement_row_count, state,
                               column, &values[column])
                         : resolve_insert_explicit_value(database, plan, &table->columns[column],
                                                         explicit_value, statement_row_count, state,
                                                         column, &values[column]);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static const struct mylite_insert_value *
insert_column_list_value_for_column(const struct mylite_insert_values_plan *plan,
                                    const struct mylite_insert_row *row,
                                    const size_t *column_indexes, size_t column)
{
    for (size_t target = 0U; target < plan->column_count; ++target) {
        if (column_indexes[target] == column) {
            return &row->values[target];
        }
    }
    return NULL;
}

static int resolve_insert_positional_row_values(mylite_db *database,
                                                const struct mylite_insert_values_plan *plan,
                                                const struct mylite_insert_table *table,
                                                size_t row_index, uint64_t statement_row_count,
                                                struct mylite_insert_execution_state *state,
                                                struct mylite_insert_bound_value *values)
{
    if (insert_row_uses_all_defaults(plan, row_index)) {
        return resolve_insert_default_row_values(database, plan, table, statement_row_count, state,
                                                 values);
    }

    for (size_t column = 0U; column < table->column_count; ++column) {
        int status = resolve_insert_explicit_value(
            database, plan, &table->columns[column], &plan->rows[row_index].values[column],
            statement_row_count, state, column, &values[column]);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int resolve_insert_default_row_values(mylite_db *database,
                                             const struct mylite_insert_values_plan *plan,
                                             const struct mylite_insert_table *table,
                                             uint64_t statement_row_count,
                                             struct mylite_insert_execution_state *state,
                                             struct mylite_insert_bound_value *values)
{
    for (size_t column = 0U; column < table->column_count; ++column) {
        int status = mylite_dml_resolve_insert_omitted_default_value(
            database, plan, &table->columns[column], statement_row_count, state, column,
            &values[column]);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int
resolve_insert_explicit_value(mylite_db *database, const struct mylite_insert_values_plan *plan,
                              const struct mylite_insert_table_column *column,
                              const struct mylite_insert_value *value, uint64_t statement_row_count,
                              struct mylite_insert_execution_state *state, size_t column_index,
                              struct mylite_insert_bound_value *out_value)
{
    switch (value->kind) {
    case MYLITE_INSERT_VALUE_DEFAULT:
        return mylite_dml_resolve_insert_explicit_default_value(
            database, plan, column, statement_row_count, state, out_value);
    case MYLITE_INSERT_VALUE_NULL:
        if (column->auto_increment) {
            return mylite_dml_allocate_insert_auto_increment(database, statement_row_count, state,
                                                             out_value);
        }
        if (!column->nullable) {
            if (plan->ignore) {
                int status = mylite_dml_insert_append_null_warning_once(database, column, state,
                                                                        column_index);

                if (status != MYLITE_OK) {
                    return status;
                }
                return mylite_dml_resolve_insert_implicit_expression_default(database, column,
                                                                             out_value);
            }
            return mylite_dml_set_not_null_column_error(database, column->name);
        }
        *out_value = (struct mylite_insert_bound_value){.kind = MYLITE_INSERT_BOUND_NULL};
        return MYLITE_OK;
    case MYLITE_INSERT_VALUE_INTEGER:
        return mylite_dml_resolve_insert_text_value(database, column, value->text,
                                                    statement_row_count, state, out_value);
    case MYLITE_INSERT_VALUE_REAL:
        if (column->auto_increment) {
            return mylite_dml_insert_set_unsupported_expression_error(database);
        }
        return mylite_dml_resolve_insert_text_value(database, column, value->text,
                                                    statement_row_count, state, out_value);
    case MYLITE_INSERT_VALUE_TEXT:
        return mylite_dml_resolve_insert_quoted_text_value(database, column, value->text,
                                                           statement_row_count, state, out_value);
    case MYLITE_INSERT_VALUE_CURRENT_TIMESTAMP:
        if (column->auto_increment) {
            return mylite_dml_insert_set_unsupported_expression_error(database);
        }
        return mylite_dml_resolve_insert_current_timestamp_bound_value(database, out_value);
    case MYLITE_INSERT_VALUE_UNSUPPORTED:
    case MYLITE_INSERT_VALUE_COLUMN_REFERENCE:
    case MYLITE_INSERT_VALUE_VALUES_FUNCTION:
    case MYLITE_INSERT_VALUE_UNARY_EXPRESSION:
    case MYLITE_INSERT_VALUE_BINARY_EXPRESSION:
        return mylite_dml_insert_set_unsupported_expression_error(database);
    }

    return mylite_dml_insert_set_unsupported_expression_error(database);
}

static bool insert_row_uses_all_defaults(const struct mylite_insert_values_plan *plan,
                                         size_t row_index)
{
    return plan->rows[row_index].value_count == 0U;
}

static size_t insert_row_target_column_count(const struct mylite_insert_values_plan *plan,
                                             const struct mylite_insert_table *table,
                                             size_t row_index)
{
    if (plan->has_column_list) {
        return plan->column_count;
    }
    if (plan->rows[row_index].value_count == 0U) {
        return 0U;
    }
    return table->column_count;
}
