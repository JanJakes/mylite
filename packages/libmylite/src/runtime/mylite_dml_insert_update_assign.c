#include "mylite_dml.h"

#include "mylite_diagnostics.h"
#include "mylite_dml_insert_default.h"
#include "mylite_span.h"

#include <stdlib.h>
#include <string.h>

static int evaluate_insert_update_assignment_value(
    mylite_db *database, const char *selected_schema,
    const struct mylite_insert_values_plan *values_plan, const struct mylite_insert_table *table,
    size_t target_column, const struct mylite_insert_row_column_indexes *column_indexes,
    const struct mylite_insert_value *value, const struct mylite_insert_bound_value *target_values,
    const struct mylite_insert_bound_value *candidate_values,
    struct mylite_insert_bound_value *out_value);
static int resolve_insert_update_default_value(mylite_db *database,
                                               const struct mylite_insert_table_column *column,
                                               struct mylite_insert_bound_value *out_value);
static int evaluate_insert_update_expression(
    mylite_db *database, const char *selected_schema,
    const struct mylite_insert_values_plan *values_plan, const struct mylite_insert_table *table,
    const struct mylite_insert_row_column_indexes *column_indexes,
    const struct mylite_insert_value *value, const struct mylite_insert_bound_value *target_values,
    const struct mylite_insert_bound_value *candidate_values,
    struct mylite_insert_bound_value *out_value);
static int evaluate_insert_update_simple_expression(
    mylite_db *database, const char *selected_schema,
    const struct mylite_insert_values_plan *values_plan, const struct mylite_insert_table *table,
    const struct mylite_insert_row_column_indexes *column_indexes,
    const struct mylite_insert_value *value, const struct mylite_insert_bound_value *target_values,
    const struct mylite_insert_bound_value *candidate_values,
    struct mylite_insert_bound_value *out_value);
static int evaluate_insert_update_unary_expression(
    mylite_db *database, const char *selected_schema,
    const struct mylite_insert_values_plan *values_plan, const struct mylite_insert_table *table,
    const struct mylite_insert_row_column_indexes *column_indexes,
    const struct mylite_insert_value *value, const struct mylite_insert_bound_value *target_values,
    const struct mylite_insert_bound_value *candidate_values,
    struct mylite_insert_bound_value *out_value);
static int evaluate_insert_update_binary_expression(
    mylite_db *database, const char *selected_schema,
    const struct mylite_insert_values_plan *values_plan, const struct mylite_insert_table *table,
    const struct mylite_insert_row_column_indexes *column_indexes,
    const struct mylite_insert_value *value, const struct mylite_insert_bound_value *target_values,
    const struct mylite_insert_bound_value *candidate_values,
    struct mylite_insert_bound_value *out_value);
static int evaluate_insert_update_column_reference(
    mylite_db *database, const char *selected_schema,
    const struct mylite_insert_values_plan *values_plan, const struct mylite_insert_table *table,
    const struct mylite_insert_row_column_indexes *column_indexes,
    const struct mylite_insert_column_reference *ref,
    const struct mylite_insert_update_row_values *row_values,
    struct mylite_insert_bound_value *out_value);
static int evaluate_insert_values_function(mylite_db *database,
                                           const struct mylite_insert_table *table,
                                           const struct mylite_insert_column_reference *ref,
                                           const struct mylite_insert_bound_value *candidate_values,
                                           struct mylite_insert_bound_value *out_value);
static int validate_insert_update_assignment_result(mylite_db *database,
                                                    const struct mylite_insert_table_column *column,
                                                    struct mylite_insert_bound_value *value);
static int validate_insert_update_assignment_value(
    mylite_db *database, const struct mylite_insert_values_plan *values_plan,
    const struct mylite_insert_table *table, const char *schema_name,
    const size_t *source_column_indexes, size_t source_column_count,
    const struct mylite_insert_value *value);
static int resolve_insert_update_column_reference(
    mylite_db *database, const struct mylite_insert_values_plan *values_plan,
    const struct mylite_insert_table *table, const char *schema_name,
    const size_t *source_column_indexes, size_t source_column_count,
    const struct mylite_insert_column_reference *ref, bool *out_candidate,
    size_t *out_column_index);
static size_t insert_alias_column_index(const struct mylite_insert_values_plan *plan,
                                        const struct mylite_insert_table *table,
                                        const size_t *source_column_indexes,
                                        size_t source_column_count, const char *column_name);
static bool insert_row_alias_matches(const struct mylite_insert_values_plan *plan,
                                     const char *table_name);
static size_t insert_table_column_index(const struct mylite_insert_table *table,
                                        const char *column_name);
static size_t
insert_table_column_reference_index(const struct mylite_insert_table *table,
                                    const char *schema_name, const char *table_name,
                                    const struct mylite_insert_column_reference *reference);
static bool
insert_column_reference_qualifiers_match(const struct mylite_insert_column_reference *reference,
                                         const char *schema_name, const char *table_name);
static int set_insert_update_unknown_column_error(mylite_db *database, const char *column_name);
static int set_insert_update_ambiguous_column_error(mylite_db *database, const char *column_name);
static int set_insert_unsupported_expression_error(mylite_db *database);

int mylite_dml_apply_insert_update_assignments(
    mylite_db *database, const char *selected_schema,
    const struct mylite_insert_values_plan *values_plan,
    const struct mylite_insert_duplicate_update_plan *update_plan,
    const struct mylite_insert_table *table,
    const struct mylite_insert_row_column_indexes *column_indexes,
    const struct mylite_insert_bound_value *candidate_values,
    struct mylite_insert_bound_value *updated_values)
{
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
            database, selected_schema, values_plan, table, column_index, column_indexes,
            &update_plan->assignments[index].value, updated_values, candidate_values, &value);

        if (status != MYLITE_OK) {
            mylite_dml_insert_bound_value_deinit(&value);
            return status;
        }

        mylite_dml_insert_bound_value_deinit(&updated_values[column_index]);
        updated_values[column_index] = value;
    }
    return MYLITE_OK;
}

int mylite_dml_validate_insert_update_assignments(
    mylite_db *database, const struct mylite_insert_values_plan *values_plan,
    const struct mylite_insert_duplicate_update_plan *update_plan,
    const struct mylite_insert_table *table, const char *schema_name,
    const size_t *source_column_indexes, size_t source_column_count, size_t **out_column_indexes)
{
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
        size_t column_index = insert_table_column_reference_index(
            table, schema_name, values_plan->table_name, &assignment->target);
        int status = MYLITE_OK;

        if (column_index == table->column_count) {
            status =
                set_insert_update_unknown_column_error(database, assignment->target.column_name);
            free(column_indexes);
            return status;
        }
        column_indexes[index] = column_index;

        status = validate_insert_update_assignment_value(database, values_plan, table, schema_name,
                                                         source_column_indexes, source_column_count,
                                                         &assignment->value);
        if (status != MYLITE_OK) {
            free(column_indexes);
            return status;
        }
    }

    *out_column_indexes = column_indexes;
    return MYLITE_OK;
}

static int evaluate_insert_update_assignment_value(
    mylite_db *database, const char *selected_schema,
    const struct mylite_insert_values_plan *values_plan, const struct mylite_insert_table *table,
    size_t target_column, const struct mylite_insert_row_column_indexes *column_indexes,
    const struct mylite_insert_value *value, const struct mylite_insert_bound_value *target_values,
    const struct mylite_insert_bound_value *candidate_values,
    struct mylite_insert_bound_value *out_value)
{
    const struct mylite_insert_table_column *column = &table->columns[target_column];
    int status = MYLITE_OK;

    if (value->kind == MYLITE_INSERT_VALUE_DEFAULT) {
        status = resolve_insert_update_default_value(database, column, out_value);
    } else {
        status = evaluate_insert_update_expression(database, selected_schema, values_plan, table,
                                                   column_indexes, value, target_values,
                                                   candidate_values, out_value);
    }
    if (status == MYLITE_OK) {
        status = validate_insert_update_assignment_result(database, column, out_value);
    }
    return status;
}

static int resolve_insert_update_default_value(mylite_db *database,
                                               const struct mylite_insert_table_column *column,
                                               struct mylite_insert_bound_value *out_value)
{
    return mylite_dml_resolve_insert_default_bound_value(database, column, 0U, NULL, out_value);
}

static int evaluate_insert_update_expression(
    mylite_db *database, const char *selected_schema,
    const struct mylite_insert_values_plan *values_plan, const struct mylite_insert_table *table,
    const struct mylite_insert_row_column_indexes *column_indexes,
    const struct mylite_insert_value *value, const struct mylite_insert_bound_value *target_values,
    const struct mylite_insert_bound_value *candidate_values,
    struct mylite_insert_bound_value *out_value)
{
    if (value->kind == MYLITE_INSERT_VALUE_UNARY_EXPRESSION) {
        return evaluate_insert_update_unary_expression(database, selected_schema, values_plan,
                                                       table, column_indexes, value, target_values,
                                                       candidate_values, out_value);
    }
    if (value->kind == MYLITE_INSERT_VALUE_BINARY_EXPRESSION) {
        return evaluate_insert_update_binary_expression(database, selected_schema, values_plan,
                                                        table, column_indexes, value, target_values,
                                                        candidate_values, out_value);
    }
    return evaluate_insert_update_simple_expression(database, selected_schema, values_plan, table,
                                                    column_indexes, value, target_values,
                                                    candidate_values, out_value);
}

static int evaluate_insert_update_simple_expression(
    mylite_db *database, const char *selected_schema,
    const struct mylite_insert_values_plan *values_plan, const struct mylite_insert_table *table,
    const struct mylite_insert_row_column_indexes *column_indexes,
    const struct mylite_insert_value *value, const struct mylite_insert_bound_value *target_values,
    const struct mylite_insert_bound_value *candidate_values,
    struct mylite_insert_bound_value *out_value)
{
    int64_t integer_value = 0;
    double real_value = 0.0;
    const struct mylite_insert_update_row_values row_values = {
        .target_values = target_values,
        .candidate_values = candidate_values,
    };

    switch (value->kind) {
    case MYLITE_INSERT_VALUE_NULL:
        *out_value = (struct mylite_insert_bound_value){.kind = MYLITE_INSERT_BOUND_NULL};
        return MYLITE_OK;
    case MYLITE_INSERT_VALUE_INTEGER:
        if (!mylite_dml_parse_insert_integer_text(value->text, &integer_value)) {
            return set_insert_unsupported_expression_error(database);
        }
        *out_value = (struct mylite_insert_bound_value){
            .kind = MYLITE_INSERT_BOUND_INTEGER,
            .integer_value = integer_value,
        };
        return MYLITE_OK;
    case MYLITE_INSERT_VALUE_REAL:
        if (!mylite_dml_parse_insert_real_text(value->text, &real_value)) {
            return set_insert_unsupported_expression_error(database);
        }
        *out_value = (struct mylite_insert_bound_value){
            .kind = MYLITE_INSERT_BOUND_REAL,
            .real_value = real_value,
        };
        return MYLITE_OK;
    case MYLITE_INSERT_VALUE_TEXT:
        out_value->text_value =
            mylite_copy_span_text(value->text, value->text == NULL ? 0U : strlen(value->text));
        if (out_value->text_value == NULL) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
        out_value->kind = MYLITE_INSERT_BOUND_TEXT;
        return MYLITE_OK;
    case MYLITE_INSERT_VALUE_CURRENT_TIMESTAMP:
        return mylite_dml_resolve_insert_current_timestamp_bound_value(database, out_value);
    case MYLITE_INSERT_VALUE_COLUMN_REFERENCE:
        return evaluate_insert_update_column_reference(
            database, selected_schema, values_plan, table, column_indexes, &value->column_reference,
            &row_values, out_value);
    case MYLITE_INSERT_VALUE_VALUES_FUNCTION:
        return evaluate_insert_values_function(database, table, &value->column_reference,
                                               candidate_values, out_value);
    case MYLITE_INSERT_VALUE_DEFAULT:
    case MYLITE_INSERT_VALUE_UNSUPPORTED:
    case MYLITE_INSERT_VALUE_UNARY_EXPRESSION:
    case MYLITE_INSERT_VALUE_BINARY_EXPRESSION:
        return set_insert_unsupported_expression_error(database);
    }

    return set_insert_unsupported_expression_error(database);
}

static int evaluate_insert_update_unary_expression(
    mylite_db *database, const char *selected_schema,
    const struct mylite_insert_values_plan *values_plan, const struct mylite_insert_table *table,
    const struct mylite_insert_row_column_indexes *column_indexes,
    const struct mylite_insert_value *value, const struct mylite_insert_bound_value *target_values,
    const struct mylite_insert_bound_value *candidate_values,
    struct mylite_insert_bound_value *out_value)
{
    struct mylite_insert_bound_value operand = {0};
    double numeric_value = 0.0;
    bool is_integer = false;
    int status = evaluate_insert_update_simple_expression(
        database, selected_schema, values_plan, table, column_indexes, value->left, target_values,
        candidate_values, &operand);

    if (status != MYLITE_OK) {
        mylite_dml_insert_bound_value_deinit(&operand);
        return status;
    }
    if (operand.kind == MYLITE_INSERT_BOUND_NULL) {
        *out_value = (struct mylite_insert_bound_value){.kind = MYLITE_INSERT_BOUND_NULL};
        mylite_dml_insert_bound_value_deinit(&operand);
        return MYLITE_OK;
    }
    if (!mylite_dml_insert_bound_value_is_numeric(&operand, &numeric_value, &is_integer)) {
        mylite_dml_insert_bound_value_deinit(&operand);
        return set_insert_unsupported_expression_error(database);
    }

    if (is_integer) {
        int64_t integer = operand.kind == MYLITE_INSERT_BOUND_INTEGER ? operand.integer_value
                                                                      : (int64_t)numeric_value;

        out_value->kind = MYLITE_INSERT_BOUND_INTEGER;
        out_value->integer_value =
            value->operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE ? -integer : integer;
    } else {
        out_value->kind = MYLITE_INSERT_BOUND_REAL;
        out_value->real_value = value->operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE
                                    ? -numeric_value
                                    : numeric_value;
    }
    mylite_dml_insert_bound_value_deinit(&operand);
    return MYLITE_OK;
}

static int evaluate_insert_update_binary_expression(
    mylite_db *database, const char *selected_schema,
    const struct mylite_insert_values_plan *values_plan, const struct mylite_insert_table *table,
    const struct mylite_insert_row_column_indexes *column_indexes,
    const struct mylite_insert_value *value, const struct mylite_insert_bound_value *target_values,
    const struct mylite_insert_bound_value *candidate_values,
    struct mylite_insert_bound_value *out_value)
{
    struct mylite_insert_bound_value left = {0};
    struct mylite_insert_bound_value right = {0};
    double left_number = 0.0;
    double right_number = 0.0;
    bool left_is_integer = false;
    bool right_is_integer = false;
    int status = evaluate_insert_update_simple_expression(database, selected_schema, values_plan,
                                                          table, column_indexes, value->left,
                                                          target_values, candidate_values, &left);

    if (status == MYLITE_OK) {
        status = evaluate_insert_update_simple_expression(database, selected_schema, values_plan,
                                                          table, column_indexes, value->right,
                                                          target_values, candidate_values, &right);
    }
    if (status != MYLITE_OK) {
        mylite_dml_insert_bound_value_deinit(&left);
        mylite_dml_insert_bound_value_deinit(&right);
        return status;
    }
    if (left.kind == MYLITE_INSERT_BOUND_NULL || right.kind == MYLITE_INSERT_BOUND_NULL) {
        *out_value = (struct mylite_insert_bound_value){.kind = MYLITE_INSERT_BOUND_NULL};
        mylite_dml_insert_bound_value_deinit(&left);
        mylite_dml_insert_bound_value_deinit(&right);
        return MYLITE_OK;
    }
    if (!mylite_dml_insert_bound_value_is_numeric(&left, &left_number, &left_is_integer) ||
        !mylite_dml_insert_bound_value_is_numeric(&right, &right_number, &right_is_integer)) {
        mylite_dml_insert_bound_value_deinit(&left);
        mylite_dml_insert_bound_value_deinit(&right);
        return set_insert_unsupported_expression_error(database);
    }

    if (left_is_integer && right_is_integer &&
        value->operator_kind != MYLITE_SQL_AST_OPERATOR_DIVIDE) {
        int64_t left_int =
            left.kind == MYLITE_INSERT_BOUND_INTEGER ? left.integer_value : (int64_t)left_number;
        int64_t right_int =
            right.kind == MYLITE_INSERT_BOUND_INTEGER ? right.integer_value : (int64_t)right_number;

        out_value->kind = MYLITE_INSERT_BOUND_INTEGER;
        switch (value->operator_kind) {
        case MYLITE_SQL_AST_OPERATOR_ADD:
            out_value->integer_value = left_int + right_int;
            break;
        case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
            out_value->integer_value = left_int - right_int;
            break;
        case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
            out_value->integer_value = left_int * right_int;
            break;
        case MYLITE_SQL_AST_OPERATOR_DIVIDE:
        case MYLITE_SQL_AST_OPERATOR_NONE:
        case MYLITE_SQL_AST_OPERATOR_POSITIVE:
        case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
        default:
            mylite_dml_insert_bound_value_deinit(&left);
            mylite_dml_insert_bound_value_deinit(&right);
            return set_insert_unsupported_expression_error(database);
        }
    } else {
        if (value->operator_kind == MYLITE_SQL_AST_OPERATOR_DIVIDE && right_number == 0.0) {
            mylite_dml_insert_bound_value_deinit(&left);
            mylite_dml_insert_bound_value_deinit(&right);
            return set_insert_unsupported_expression_error(database);
        }
        out_value->kind = MYLITE_INSERT_BOUND_REAL;
        switch (value->operator_kind) {
        case MYLITE_SQL_AST_OPERATOR_ADD:
            out_value->real_value = left_number + right_number;
            break;
        case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
            out_value->real_value = left_number - right_number;
            break;
        case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
            out_value->real_value = left_number * right_number;
            break;
        case MYLITE_SQL_AST_OPERATOR_DIVIDE:
            out_value->real_value = left_number / right_number;
            break;
        case MYLITE_SQL_AST_OPERATOR_NONE:
        case MYLITE_SQL_AST_OPERATOR_POSITIVE:
        case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
        default:
            mylite_dml_insert_bound_value_deinit(&left);
            mylite_dml_insert_bound_value_deinit(&right);
            return set_insert_unsupported_expression_error(database);
        }
    }

    mylite_dml_insert_bound_value_deinit(&left);
    mylite_dml_insert_bound_value_deinit(&right);
    return MYLITE_OK;
}

static int evaluate_insert_update_column_reference(
    mylite_db *database, const char *selected_schema,
    const struct mylite_insert_values_plan *values_plan, const struct mylite_insert_table *table,
    const struct mylite_insert_row_column_indexes *column_indexes,
    const struct mylite_insert_column_reference *ref,
    const struct mylite_insert_update_row_values *row_values,
    struct mylite_insert_bound_value *out_value)
{
    const char *schema_name =
        values_plan->schema_name == NULL ? selected_schema : values_plan->schema_name;
    bool candidate = false;
    size_t column_index = table->column_count;
    int status = MYLITE_OK;

    status = resolve_insert_update_column_reference(
        database, values_plan, table, schema_name, column_indexes->insert_columns,
        column_indexes->source_column_count, ref, &candidate, &column_index);
    if (status != MYLITE_OK) {
        return status;
    }
    {
        const struct mylite_insert_bound_value *source_value =
            &row_values->target_values[column_index];

        if (candidate) {
            source_value = &row_values->candidate_values[column_index];
        }
        status = mylite_dml_copy_insert_bound_value(source_value, out_value);
    }
    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
    }
    return status;
}

static int evaluate_insert_values_function(mylite_db *database,
                                           const struct mylite_insert_table *table,
                                           const struct mylite_insert_column_reference *ref,
                                           const struct mylite_insert_bound_value *candidate_values,
                                           struct mylite_insert_bound_value *out_value)
{
    size_t column_index = insert_table_column_index(table, ref->column_name);
    int status = MYLITE_OK;

    if (column_index == table->column_count) {
        return set_insert_update_unknown_column_error(database, ref->column_name);
    }

    status = mylite_dml_copy_insert_bound_value(&candidate_values[column_index], out_value);
    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
    }
    return status;
}

static int validate_insert_update_assignment_result(mylite_db *database,
                                                    const struct mylite_insert_table_column *column,
                                                    struct mylite_insert_bound_value *value)
{
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
    if (value->kind == MYLITE_INSERT_BOUND_TEXT &&
        mylite_dml_parse_insert_integer_text(value->text_value, &integer_value) &&
        integer_value >= 0) {
        mylite_dml_insert_bound_value_deinit(value);
        *value = (struct mylite_insert_bound_value){
            .kind = MYLITE_INSERT_BOUND_INTEGER,
            .integer_value = integer_value,
        };
        return MYLITE_OK;
    }
    return set_insert_unsupported_expression_error(database);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int validate_insert_update_assignment_value(
    mylite_db *database, const struct mylite_insert_values_plan *values_plan,
    const struct mylite_insert_table *table, const char *schema_name,
    const size_t *source_column_indexes, size_t source_column_count,
    const struct mylite_insert_value *value)
{
    if (value == NULL) {
        return set_insert_unsupported_expression_error(database);
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
        return resolve_insert_update_column_reference(
            database, values_plan, table, schema_name, source_column_indexes, source_column_count,
            &value->column_reference, &candidate, &column_index);
    }
    case MYLITE_INSERT_VALUE_VALUES_FUNCTION: {
        size_t column_index = insert_table_column_index(table, value->column_reference.column_name);

        if (column_index == table->column_count) {
            return set_insert_update_unknown_column_error(database,
                                                          value->column_reference.column_name);
        }
        return MYLITE_OK;
    }
    case MYLITE_INSERT_VALUE_UNARY_EXPRESSION:
        return validate_insert_update_assignment_value(database, values_plan, table, schema_name,
                                                       source_column_indexes, source_column_count,
                                                       value->left);
    case MYLITE_INSERT_VALUE_BINARY_EXPRESSION: {
        int status = validate_insert_update_assignment_value(database, values_plan, table,
                                                             schema_name, source_column_indexes,
                                                             source_column_count, value->left);

        if (status != MYLITE_OK) {
            return status;
        }
        return validate_insert_update_assignment_value(database, values_plan, table, schema_name,
                                                       source_column_indexes, source_column_count,
                                                       value->right);
    }
    case MYLITE_INSERT_VALUE_UNSUPPORTED:
        return set_insert_unsupported_expression_error(database);
    }

    return set_insert_unsupported_expression_error(database);
}

static int resolve_insert_update_column_reference(
    mylite_db *database, const struct mylite_insert_values_plan *values_plan,
    const struct mylite_insert_table *table, const char *schema_name,
    const size_t *source_column_indexes, size_t source_column_count,
    const struct mylite_insert_column_reference *ref, bool *out_candidate, size_t *out_column_index)
{
    size_t target_index;
    size_t alias_index = table->column_count;

    *out_candidate = false;
    *out_column_index = table->column_count;
    if (ref->schema_name != NULL) {
        target_index =
            insert_table_column_reference_index(table, schema_name, values_plan->table_name, ref);
        if (target_index == table->column_count) {
            return set_insert_update_unknown_column_error(database, ref->column_name);
        }
        *out_column_index = target_index;
        return MYLITE_OK;
    }
    if (ref->table_name != NULL) {
        if (insert_row_alias_matches(values_plan, ref->table_name)) {
            alias_index = values_plan->alias_column_count == 0U
                              ? insert_table_column_index(table, ref->column_name)
                              : insert_alias_column_index(values_plan, table, source_column_indexes,
                                                          source_column_count, ref->column_name);
            if (alias_index == table->column_count) {
                return set_insert_update_unknown_column_error(database, ref->column_name);
            }
            *out_candidate = true;
            *out_column_index = alias_index;
            return MYLITE_OK;
        }

        target_index =
            insert_table_column_reference_index(table, schema_name, values_plan->table_name, ref);
        if (target_index == table->column_count) {
            return set_insert_update_unknown_column_error(database, ref->column_name);
        }
        *out_column_index = target_index;
        return MYLITE_OK;
    }

    target_index = insert_table_column_index(table, ref->column_name);
    if (values_plan->alias_column_count != 0U) {
        alias_index = insert_alias_column_index(values_plan, table, source_column_indexes,
                                                source_column_count, ref->column_name);
    }
    if (target_index != table->column_count && alias_index != table->column_count) {
        return set_insert_update_ambiguous_column_error(database, ref->column_name);
    }
    if (alias_index != table->column_count) {
        *out_candidate = true;
        *out_column_index = alias_index;
        return MYLITE_OK;
    }
    if (target_index != table->column_count) {
        *out_column_index = target_index;
        return MYLITE_OK;
    }
    return set_insert_update_unknown_column_error(database, ref->column_name);
}

static size_t insert_alias_column_index(const struct mylite_insert_values_plan *plan,
                                        const struct mylite_insert_table *table,
                                        const size_t *source_column_indexes,
                                        size_t source_column_count, const char *column_name)
{
    for (size_t index = 0U; index < plan->alias_column_count; ++index) {
        if (mylite_ascii_case_equal(plan->alias_columns[index], column_name)) {
            if (index >= source_column_count) {
                return table->column_count;
            }
            if (source_column_indexes != NULL) {
                return source_column_indexes[index];
            }
            if (plan->has_column_list) {
                return insert_table_column_index(table, plan->columns[index]);
            }
            return index;
        }
    }
    return table->column_count;
}

static bool insert_row_alias_matches(const struct mylite_insert_values_plan *plan,
                                     const char *table_name)
{
    if (plan->row_alias == NULL || table_name == NULL) {
        return false;
    }
    return mylite_ascii_case_equal(plan->row_alias, table_name);
}

static size_t insert_table_column_index(const struct mylite_insert_table *table,
                                        const char *column_name)
{
    for (size_t index = 0U; index < table->column_count; ++index) {
        if (mylite_ascii_case_equal(table->columns[index].name, column_name)) {
            return index;
        }
    }
    return table->column_count;
}

static size_t
insert_table_column_reference_index(const struct mylite_insert_table *table,
                                    const char *schema_name, const char *table_name,
                                    const struct mylite_insert_column_reference *reference)
{
    if (!insert_column_reference_qualifiers_match(reference, schema_name, table_name)) {
        return table->column_count;
    }
    return insert_table_column_index(table, reference->column_name);
}

static bool
insert_column_reference_qualifiers_match(const struct mylite_insert_column_reference *reference,
                                         const char *schema_name, const char *table_name)
{
    if (reference->schema_name != NULL &&
        !mylite_ascii_case_equal(reference->schema_name, schema_name)) {
        return false;
    }
    if (reference->table_name != NULL &&
        !mylite_ascii_case_equal(reference->table_name, table_name)) {
        return false;
    }
    return true;
}

static int set_insert_update_unknown_column_error(mylite_db *database, const char *column_name)
{
    int status = mylite_diagnostics_set_error_message_parts(database, "Unknown column '",
                                                            column_name, "' in 'field list'");

    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_insert_update_ambiguous_column_error(mylite_db *database, const char *column_name)
{
    int status = mylite_diagnostics_set_error_message_parts(database, "Column '", column_name,
                                                            "' in field list is ambiguous");

    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_insert_unsupported_expression_error(mylite_db *database)
{
    if (mylite_diagnostics_set_error_message(database, "Unsupported INSERT value expression") ==
        MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    return MYLITE_EXEC_ERROR;
}
