#include "mylite_dml_insert_set_row_resolve.h"

#include "mylite_connection.h"
#include "mylite_dml.h"
#include "mylite_dml_insert_bound_value.h"
#include "mylite_dml_insert_default.h"
#include "mylite_dml_insert_diagnostics.h"
#include "mylite_dml_insert_set_expression.h"

#include <stdint.h>

static int initialize_insert_set_row_values(
    mylite_db *database,
    const struct mylite_insert_table *table,
    uint64_t statement_row_count,
    struct mylite_insert_execution_state *state,
    struct mylite_insert_bound_value *values,
    struct mylite_insert_set_row_state *row_state
);

static int apply_insert_set_assignments(
    mylite_db *database,
    const char *schema_name,
    const struct mylite_insert_values_plan *values_plan,
    const struct mylite_insert_set_plan *set_plan,
    const struct mylite_insert_table *table,
    const size_t *column_indexes,
    size_t column_index_count,
    struct mylite_insert_bound_value *values,
    struct mylite_insert_set_row_state *row_state,
    const struct mylite_dml_expression_callbacks *callbacks
);

static int finish_insert_set_row_values(
    mylite_db *database,
    const struct mylite_insert_values_plan *plan,
    const struct mylite_insert_table *table,
    uint64_t statement_row_count,
    struct mylite_insert_execution_state *state,
    struct mylite_insert_bound_value *values,
    const struct mylite_insert_set_row_state *row_state
);

static int finish_insert_set_required_omission(
    mylite_db *database,
    const struct mylite_insert_values_plan *plan,
    const struct mylite_insert_table_column *column,
    struct mylite_insert_execution_state *state,
    size_t column_index,
    const struct mylite_insert_set_row_state *row_state
);

static int finish_insert_set_required_null(
    mylite_db *database,
    const struct mylite_insert_values_plan *plan,
    const struct mylite_insert_table_column *column,
    struct mylite_insert_bound_value *value
);

static int evaluate_insert_set_assignment_value(
    mylite_db *database,
    const char *schema_name,
    const struct mylite_insert_values_plan *plan,
    const struct mylite_insert_table *table,
    size_t target_column,
    const struct mylite_insert_value *value,
    const struct mylite_insert_bound_value *values,
    bool *out_generate_auto_increment,
    struct mylite_insert_bound_value *out_value,
    const struct mylite_dml_expression_callbacks *callbacks
);

static int set_insert_set_candidate_auto_value(struct mylite_insert_bound_value *out_value);

static bool insert_set_plan_coerces_missing_required_default(
    const mylite_db *database,
    const struct mylite_insert_values_plan *plan
);

// NOLINTBEGIN(bugprone-easily-swappable-parameters)
int mylite_dml_resolve_insert_set_row_values(
    mylite_db *database,
    const char *schema_name,
    const struct mylite_insert_values_plan *values_plan,
    const struct mylite_insert_set_plan *set_plan,
    const struct mylite_insert_table *table,
    const size_t *column_indexes,
    size_t column_index_count,
    uint64_t statement_row_count,
    struct mylite_insert_execution_state *state,
    struct mylite_insert_bound_value *values,
    struct mylite_insert_set_row_state *row_state,
    const struct mylite_dml_expression_callbacks *callbacks
) {
    int status = MYLITE_OK;

    if (database == NULL || schema_name == NULL || values_plan == NULL || set_plan == NULL ||
        table == NULL || state == NULL || values == NULL || row_state == NULL) {
        return MYLITE_MISUSE;
    }

    status = initialize_insert_set_row_values(
        database,
        table,
        statement_row_count,
        state,
        values,
        row_state
    );
    if (status == MYLITE_OK) {
        status = apply_insert_set_assignments(
            database,
            schema_name,
            values_plan,
            set_plan,
            table,
            column_indexes,
            column_index_count,
            values,
            row_state,
            callbacks
        );
    }
    if (status == MYLITE_OK) {
        status = finish_insert_set_row_values(
            database,
            values_plan,
            table,
            statement_row_count,
            state,
            values,
            row_state
        );
    }
    return status;
}

// NOLINTEND(bugprone-easily-swappable-parameters)

static int initialize_insert_set_row_values(
    mylite_db *database,
    const struct mylite_insert_table *table,
    uint64_t statement_row_count,
    struct mylite_insert_execution_state *state,
    struct mylite_insert_bound_value *values,
    struct mylite_insert_set_row_state *row_state
) {
    for (size_t column = 0U; column < table->column_count; ++column) {
        int status = MYLITE_OK;

        if (table->columns[column].auto_increment) {
            status = set_insert_set_candidate_auto_value(&values[column]);
            row_state->generate_auto_increment[column] = true;
        } else if (table->columns[column].default_text != NULL || table->columns[column].nullable) {
            status = mylite_dml_resolve_insert_default_bound_value(
                database,
                &table->columns[column],
                statement_row_count,
                state,
                &values[column]
            );
        } else {
            status = mylite_dml_resolve_insert_implicit_expression_default(
                database,
                &table->columns[column],
                &values[column]
            );
        }
        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int apply_insert_set_assignments(
    mylite_db *database,
    const char *schema_name,
    const struct mylite_insert_values_plan *values_plan,
    const struct mylite_insert_set_plan *set_plan,
    const struct mylite_insert_table *table,
    const size_t *column_indexes,
    size_t column_index_count,
    struct mylite_insert_bound_value *values,
    struct mylite_insert_set_row_state *row_state,
    const struct mylite_dml_expression_callbacks *callbacks
) {
    if (column_indexes == NULL || column_index_count != set_plan->assignment_count) {
        return MYLITE_MISUSE;
    }

    for (size_t index = 0U; index < column_index_count; ++index) {
        size_t column_index = column_indexes[index];
        struct mylite_insert_bound_value value = {0};
        bool generate_auto = false;
        int status = evaluate_insert_set_assignment_value(
            database,
            schema_name,
            values_plan,
            table,
            column_index,
            &set_plan->assignments[index].value,
            values,
            &generate_auto,
            &value,
            callbacks
        );

        if (status != MYLITE_OK) {
            mylite_dml_insert_bound_value_deinit(&value);
            return status;
        }

        mylite_dml_insert_bound_value_deinit(&values[column_index]);
        values[column_index] = value;
        row_state->generate_auto_increment[column_index] = generate_auto;
        row_state->assigned_columns[column_index] = true;
    }
    return MYLITE_OK;
}

static int finish_insert_set_row_values(
    mylite_db *database,
    const struct mylite_insert_values_plan *plan,
    const struct mylite_insert_table *table,
    uint64_t statement_row_count,
    struct mylite_insert_execution_state *state,
    struct mylite_insert_bound_value *values,
    const struct mylite_insert_set_row_state *row_state
) {
    for (size_t column = 0U; column < table->column_count; ++column) {
        const struct mylite_insert_table_column *table_column = &table->columns[column];
        int status = finish_insert_set_required_omission(
            database,
            plan,
            table_column,
            state,
            column,
            row_state
        );

        if (status != MYLITE_OK) {
            return status;
        }
        if (table_column->auto_increment && row_state->generate_auto_increment[column]) {
            mylite_dml_insert_bound_value_deinit(&values[column]);
            status = mylite_dml_allocate_insert_auto_increment(
                database,
                statement_row_count,
                state,
                &values[column]
            );

            if (status != MYLITE_OK) {
                return status;
            }
        }
        status = finish_insert_set_required_null(database, plan, table_column, &values[column]);
        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int finish_insert_set_required_omission(
    mylite_db *database,
    const struct mylite_insert_values_plan *plan,
    const struct mylite_insert_table_column *column,
    struct mylite_insert_execution_state *state,
    size_t column_index,
    const struct mylite_insert_set_row_state *row_state
) {
    if (column->auto_increment || column->nullable || column->default_text != NULL ||
        row_state->assigned_columns[column_index]) {
        return MYLITE_OK;
    }
    if (!insert_set_plan_coerces_missing_required_default(database, plan)) {
        return mylite_dml_insert_set_no_default_error(database, column->name);
    }
    return mylite_dml_insert_append_no_default_warning_once(database, column, state, column_index);
}

static int finish_insert_set_required_null(
    mylite_db *database,
    const struct mylite_insert_values_plan *plan,
    const struct mylite_insert_table_column *column,
    struct mylite_insert_bound_value *value
) {
    if (column->auto_increment || column->nullable || value->kind != MYLITE_INSERT_BOUND_NULL) {
        return MYLITE_OK;
    }
    if (!plan->ignore) {
        return mylite_dml_set_not_null_column_error(database, column->name);
    }

    int status = mylite_dml_insert_append_null_warning(database, column->name);

    if (status != MYLITE_OK) {
        return status;
    }
    mylite_dml_insert_bound_value_deinit(value);
    return mylite_dml_resolve_insert_implicit_expression_default(database, column, value);
}

static int evaluate_insert_set_assignment_value(
    mylite_db *database,
    const char *schema_name,
    const struct mylite_insert_values_plan *plan,
    const struct mylite_insert_table *table,
    size_t target_column,
    const struct mylite_insert_value *value,
    const struct mylite_insert_bound_value *values,
    bool *out_generate_auto_increment,
    struct mylite_insert_bound_value *out_value,
    const struct mylite_dml_expression_callbacks *callbacks
) {
    const struct mylite_insert_table_column *column = &table->columns[target_column];
    int status = MYLITE_OK;

    *out_generate_auto_increment = false;
    if (value->kind == MYLITE_INSERT_VALUE_DEFAULT) {
        if (column->auto_increment) {
            *out_generate_auto_increment = true;
            return set_insert_set_candidate_auto_value(out_value);
        }
        return mylite_dml_resolve_insert_explicit_default_value(
            database,
            plan,
            column,
            1U,
            NULL,
            out_value
        );
    }
    if (value->kind == MYLITE_INSERT_VALUE_EXPRESSION) {
        status = mylite_dml_resolve_insert_expression_bound_value(
            database,
            schema_name,
            plan,
            table,
            values,
            column,
            value->expression,
            1U,
            NULL,
            callbacks,
            out_value
        );
    } else {
        status = mylite_dml_evaluate_insert_set_expression(
            database,
            schema_name,
            plan,
            table,
            value,
            values,
            out_value
        );
    }

    if (status != MYLITE_OK) {
        return status;
    }

    if (column->auto_increment) {
        bool zero_generates = mylite_dml_insert_auto_increment_zero_generates(database, column);

        if (out_value->kind == MYLITE_INSERT_BOUND_NULL ||
            (zero_generates && out_value->kind == MYLITE_INSERT_BOUND_INTEGER &&
             out_value->integer_value == 0)) {
            mylite_dml_insert_bound_value_deinit(out_value);
            *out_generate_auto_increment = true;
            return set_insert_set_candidate_auto_value(out_value);
        }
        if (out_value->kind != MYLITE_INSERT_BOUND_INTEGER || out_value->integer_value < 0) {
            mylite_dml_insert_bound_value_deinit(out_value);
            return mylite_dml_insert_set_unsupported_expression_error(database);
        }
    } else if (!column->nullable && out_value->kind == MYLITE_INSERT_BOUND_NULL) {
        if (plan->ignore) {
            int warning_status = mylite_dml_insert_append_null_warning(database, column->name);

            if (warning_status != MYLITE_OK) {
                mylite_dml_insert_bound_value_deinit(out_value);
                return warning_status;
            }
            mylite_dml_insert_bound_value_deinit(out_value);
            return mylite_dml_resolve_insert_implicit_expression_default(
                database,
                column,
                out_value
            );
        }
        mylite_dml_insert_bound_value_deinit(out_value);
        return mylite_dml_set_not_null_column_error(database, column->name);
    }
    return MYLITE_OK;
}

static int set_insert_set_candidate_auto_value(struct mylite_insert_bound_value *out_value) {
    *out_value = (struct mylite_insert_bound_value){
        .kind = MYLITE_INSERT_BOUND_INTEGER,
        .integer_value = 0,
    };
    return MYLITE_OK;
}

static bool insert_set_plan_coerces_missing_required_default(
    const mylite_db *database,
    const struct mylite_insert_values_plan *plan
) {
    return plan != NULL && (plan->ignore || !mylite_connection_sql_mode_is_strict(database));
}
