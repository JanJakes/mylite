#include "mylite_dml.h"

#include "mylite_dml_insert_default.h"
#include "mylite_dml_insert_diagnostics.h"
#include "mylite_dml_insert_update_expression.h"

#include <string.h>

static int evaluate_insert_update_assignment_value(
    mylite_db *database,
    const char *selected_schema,
    const struct mylite_insert_values_plan *values_plan,
    const struct mylite_insert_table *table,
    size_t target_column,
    const struct mylite_insert_row_column_indexes *column_indexes,
    const struct mylite_insert_value *value,
    const struct mylite_insert_bound_value *target_values,
    const struct mylite_insert_bound_value *candidate_values,
    struct mylite_insert_bound_value *out_value
);

static int resolve_insert_update_default_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    struct mylite_insert_bound_value *out_value
);

static int validate_insert_update_assignment_result(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    struct mylite_insert_bound_value *value
);

int mylite_dml_apply_insert_update_assignments(
    mylite_db *database,
    const char *selected_schema,
    const struct mylite_insert_values_plan *values_plan,
    const struct mylite_insert_duplicate_update_plan *update_plan,
    const struct mylite_insert_table *table,
    const struct mylite_insert_row_column_indexes *column_indexes,
    const struct mylite_insert_bound_value *candidate_values,
    struct mylite_insert_bound_value *updated_values
) {
    if (database == NULL || values_plan == NULL || update_plan == NULL || table == NULL ||
        column_indexes == NULL || column_indexes->update_columns == NULL ||
        candidate_values == NULL || updated_values == NULL ||
        (values_plan->schema_name == NULL && selected_schema == NULL)) {
        return MYLITE_MISUSE;
    }

    for (size_t index = 0U; index < update_plan->assignment_count; ++index) {
        size_t column_index = column_indexes->update_columns[index];
        struct mylite_insert_bound_value value = {0};
        int status = evaluate_insert_update_assignment_value(
            database,
            selected_schema,
            values_plan,
            table,
            column_index,
            column_indexes,
            &update_plan->assignments[index].value,
            updated_values,
            candidate_values,
            &value
        );

        if (status != MYLITE_OK) {
            mylite_dml_insert_bound_value_deinit(&value);
            return status;
        }

        mylite_dml_insert_bound_value_deinit(&updated_values[column_index]);
        updated_values[column_index] = value;
    }
    return MYLITE_OK;
}

static int evaluate_insert_update_assignment_value(
    mylite_db *database,
    const char *selected_schema,
    const struct mylite_insert_values_plan *values_plan,
    const struct mylite_insert_table *table,
    size_t target_column,
    const struct mylite_insert_row_column_indexes *column_indexes,
    const struct mylite_insert_value *value,
    const struct mylite_insert_bound_value *target_values,
    const struct mylite_insert_bound_value *candidate_values,
    struct mylite_insert_bound_value *out_value
) {
    const struct mylite_insert_table_column *column = &table->columns[target_column];
    int status = MYLITE_OK;

    if (value->kind == MYLITE_INSERT_VALUE_DEFAULT) {
        status = resolve_insert_update_default_value(database, column, out_value);
    } else {
        status = mylite_dml_evaluate_insert_update_expression(
            database,
            selected_schema,
            values_plan,
            table,
            column_indexes,
            value,
            target_values,
            candidate_values,
            out_value
        );
    }
    if (status == MYLITE_OK) {
        status = validate_insert_update_assignment_result(database, column, out_value);
    }
    return status;
}

static int resolve_insert_update_default_value(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    struct mylite_insert_bound_value *out_value
) {
    return mylite_dml_resolve_insert_default_bound_value(database, column, 0U, NULL, out_value);
}

static int validate_insert_update_assignment_result(
    mylite_db *database,
    const struct mylite_insert_table_column *column,
    struct mylite_insert_bound_value *value
) {
    int64_t integer_value = 0;

    if (value->kind == MYLITE_INSERT_BOUND_NULL) {
        if (column->nullable) {
            return MYLITE_OK;
        }
        return mylite_dml_set_not_null_column_error(database, column->name);
    }
    if (!column->auto_increment) {
        return MYLITE_OK;
    }
    if (value->kind == MYLITE_INSERT_BOUND_INTEGER && value->integer_value >= 0) {
        return MYLITE_OK;
    }
    if (value->kind == MYLITE_INSERT_BOUND_TEXT && value->text_value != NULL &&
        memchr(value->text_value, '\0', value->text_length) == NULL &&
        mylite_dml_parse_insert_integer_text(value->text_value, &integer_value) &&
        integer_value >= 0) {
        mylite_dml_insert_bound_value_deinit(value);
        *value = (struct mylite_insert_bound_value){
            .kind = MYLITE_INSERT_BOUND_INTEGER,
            .integer_value = integer_value,
        };
        return MYLITE_OK;
    }
    return mylite_dml_insert_set_unsupported_expression_error(database);
}
