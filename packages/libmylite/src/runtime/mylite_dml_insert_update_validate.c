#include "mylite_dml.h"

#include "mylite_diagnostics.h"
#include "mylite_dml_insert_column_reference.h"
#include "mylite_dml_insert_diagnostics.h"
#include "mylite_dml_insert_update_reference.h"

#include <stdlib.h>

static int validate_insert_update_assignment_value(
    mylite_db *database,
    const struct mylite_insert_values_plan *values_plan,
    const struct mylite_insert_table *table,
    const char *schema_name,
    const size_t *source_column_indexes,
    size_t source_column_count,
    const struct mylite_insert_value *value
);

int mylite_dml_validate_insert_update_assignments(
    mylite_db *database,
    const struct mylite_insert_values_plan *values_plan,
    const struct mylite_insert_duplicate_update_plan *update_plan,
    const struct mylite_insert_table *table,
    const char *schema_name,
    const size_t *source_column_indexes,
    size_t source_column_count,
    size_t **out_column_indexes
) {
    size_t assignment_count;
    size_t *column_indexes = NULL;

    if (database == NULL || values_plan == NULL || update_plan == NULL || table == NULL ||
        schema_name == NULL || out_column_indexes == NULL) {
        return MYLITE_MISUSE;
    }

    *out_column_indexes = NULL;
    if (!update_plan->has_clause) {
        return MYLITE_OK;
    }

    assignment_count = update_plan->assignment_count;
    if (assignment_count == 0U) {
        return MYLITE_UNSUPPORTED;
    }

    column_indexes = calloc(assignment_count, sizeof(*column_indexes));
    if (column_indexes == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    for (size_t index = 0U; index < assignment_count; ++index) {
        const struct mylite_insert_update_assignment *assignment = &update_plan->assignments[index];
        size_t column_index = mylite_dml_insert_table_column_reference_index(
            table,
            schema_name,
            values_plan->table_name,
            &assignment->target
        );
        int status = MYLITE_OK;

        if (column_index == table->column_count) {
            status = mylite_dml_set_insert_update_unknown_column_error(
                database,
                assignment->target.column_name
            );
            free(column_indexes);
            return status;
        }
        column_indexes[index] = column_index;

        status = validate_insert_update_assignment_value(
            database,
            values_plan,
            table,
            schema_name,
            source_column_indexes,
            source_column_count,
            &assignment->value
        );
        if (status != MYLITE_OK) {
            free(column_indexes);
            return status;
        }
    }

    *out_column_indexes = column_indexes;
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int validate_insert_update_assignment_value(
    mylite_db *database,
    const struct mylite_insert_values_plan *values_plan,
    const struct mylite_insert_table *table,
    const char *schema_name,
    const size_t *source_column_indexes,
    size_t source_column_count,
    const struct mylite_insert_value *value
) {
    if (value == NULL) {
        return mylite_dml_insert_set_unsupported_expression_error(database);
    }

    switch (value->kind) {
    case MYLITE_INSERT_VALUE_DEFAULT:
    case MYLITE_INSERT_VALUE_NULL:
    case MYLITE_INSERT_VALUE_INTEGER:
    case MYLITE_INSERT_VALUE_REAL:
    case MYLITE_INSERT_VALUE_TEXT:
    case MYLITE_INSERT_VALUE_CURRENT_TIMESTAMP:
        return MYLITE_OK;
    case MYLITE_INSERT_VALUE_COLUMN_REFERENCE: {
        bool candidate = false;
        size_t column_index = table->column_count;

        (void)candidate;
        return mylite_dml_resolve_insert_update_column_reference(
            database,
            values_plan,
            table,
            schema_name,
            source_column_indexes,
            source_column_count,
            &value->column_reference,
            &candidate,
            &column_index
        );
    }
    case MYLITE_INSERT_VALUE_VALUES_FUNCTION: {
        size_t column_index =
            mylite_dml_insert_table_column_index(table, value->column_reference.column_name);

        if (column_index == table->column_count) {
            return mylite_dml_set_insert_update_unknown_column_error(
                database,
                value->column_reference.column_name
            );
        }
        return MYLITE_OK;
    }
    case MYLITE_INSERT_VALUE_UNARY_EXPRESSION:
        return validate_insert_update_assignment_value(
            database,
            values_plan,
            table,
            schema_name,
            source_column_indexes,
            source_column_count,
            value->left
        );
    case MYLITE_INSERT_VALUE_BINARY_EXPRESSION: {
        int status = validate_insert_update_assignment_value(
            database,
            values_plan,
            table,
            schema_name,
            source_column_indexes,
            source_column_count,
            value->left
        );

        if (status != MYLITE_OK) {
            return status;
        }
        return validate_insert_update_assignment_value(
            database,
            values_plan,
            table,
            schema_name,
            source_column_indexes,
            source_column_count,
            value->right
        );
    }
    case MYLITE_INSERT_VALUE_UNSUPPORTED:
        return mylite_dml_insert_set_unsupported_expression_error(database);
    case MYLITE_INSERT_VALUE_EXPRESSION:
        return MYLITE_OK;
    }

    return mylite_dml_insert_set_unsupported_expression_error(database);
}
