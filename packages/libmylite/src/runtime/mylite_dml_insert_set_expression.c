#include "mylite_dml_insert_set_expression.h"

#include "mylite_diagnostics.h"
#include "mylite_dml_insert_bound_value.h"
#include "mylite_dml_insert_column_reference.h"
#include "mylite_dml_insert_default.h"
#include "mylite_dml_insert_diagnostics.h"
#include "mylite_span.h"

#include <stdint.h>
#include <string.h>

static int evaluate_insert_set_simple_expression(
    mylite_db *database,
    const char *schema_name,
    const struct mylite_insert_values_plan *plan,
    const struct mylite_insert_table *table,
    const struct mylite_insert_value *value,
    const struct mylite_insert_bound_value *values,
    struct mylite_insert_bound_value *out_value
);

static int evaluate_insert_set_column_reference(
    mylite_db *database,
    const char *schema_name,
    const struct mylite_insert_values_plan *plan,
    const struct mylite_insert_table *table,
    const struct mylite_insert_column_reference *ref,
    const struct mylite_insert_bound_value *values,
    struct mylite_insert_bound_value *out_value
);

static int evaluate_insert_set_unary_expression(
    mylite_db *database,
    const char *schema_name,
    const struct mylite_insert_values_plan *plan,
    const struct mylite_insert_table *table,
    const struct mylite_insert_value *value,
    const struct mylite_insert_bound_value *values,
    struct mylite_insert_bound_value *out_value
);

static int evaluate_insert_set_binary_expression(
    mylite_db *database,
    const char *schema_name,
    const struct mylite_insert_values_plan *plan,
    const struct mylite_insert_table *table,
    const struct mylite_insert_value *value,
    const struct mylite_insert_bound_value *values,
    struct mylite_insert_bound_value *out_value
);

int mylite_dml_evaluate_insert_set_expression(
    mylite_db *database,
    const char *schema_name,
    const struct mylite_insert_values_plan *plan,
    const struct mylite_insert_table *table,
    const struct mylite_insert_value *value,
    const struct mylite_insert_bound_value *values,
    struct mylite_insert_bound_value *out_value
) {
    if (value->kind == MYLITE_INSERT_VALUE_UNARY_EXPRESSION) {
        return evaluate_insert_set_unary_expression(
            database,
            schema_name,
            plan,
            table,
            value,
            values,
            out_value
        );
    }
    if (value->kind == MYLITE_INSERT_VALUE_BINARY_EXPRESSION) {
        return evaluate_insert_set_binary_expression(
            database,
            schema_name,
            plan,
            table,
            value,
            values,
            out_value
        );
    }
    return evaluate_insert_set_simple_expression(
        database,
        schema_name,
        plan,
        table,
        value,
        values,
        out_value
    );
}

static int evaluate_insert_set_unary_expression(
    mylite_db *database,
    const char *schema_name,
    const struct mylite_insert_values_plan *plan,
    const struct mylite_insert_table *table,
    const struct mylite_insert_value *value,
    const struct mylite_insert_bound_value *values,
    struct mylite_insert_bound_value *out_value
) {
    struct mylite_insert_bound_value operand = {0};
    double numeric_value = 0.0;
    bool is_integer = false;
    int status = evaluate_insert_set_simple_expression(
        database,
        schema_name,
        plan,
        table,
        value->left,
        values,
        &operand
    );

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
        return mylite_dml_insert_set_unsupported_expression_error(database);
    }

    if (is_integer) {
        int64_t integer_value = operand.kind == MYLITE_INSERT_BOUND_INTEGER
                                    ? operand.integer_value
                                    : (int64_t)numeric_value;

        out_value->kind = MYLITE_INSERT_BOUND_INTEGER;
        out_value->integer_value = value->operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE
                                       ? -integer_value
                                       : integer_value;
    } else {
        out_value->kind = MYLITE_INSERT_BOUND_REAL;
        out_value->real_value = value->operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE
                                    ? -numeric_value
                                    : numeric_value;
    }
    mylite_dml_insert_bound_value_deinit(&operand);
    return MYLITE_OK;
}

static int evaluate_insert_set_binary_expression(
    mylite_db *database,
    const char *schema_name,
    const struct mylite_insert_values_plan *plan,
    const struct mylite_insert_table *table,
    const struct mylite_insert_value *value,
    const struct mylite_insert_bound_value *values,
    struct mylite_insert_bound_value *out_value
) {
    struct mylite_insert_bound_value left = {0};
    struct mylite_insert_bound_value right = {0};
    double left_number = 0.0;
    double right_number = 0.0;
    bool left_is_integer = false;
    bool right_is_integer = false;
    int status = evaluate_insert_set_simple_expression(
        database,
        schema_name,
        plan,
        table,
        value->left,
        values,
        &left
    );

    if (status == MYLITE_OK) {
        status = evaluate_insert_set_simple_expression(
            database,
            schema_name,
            plan,
            table,
            value->right,
            values,
            &right
        );
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
        return mylite_dml_insert_set_unsupported_expression_error(database);
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
            return mylite_dml_insert_set_unsupported_expression_error(database);
        }
    } else {
        if (value->operator_kind == MYLITE_SQL_AST_OPERATOR_DIVIDE && right_number == 0.0) {
            mylite_dml_insert_bound_value_deinit(&left);
            mylite_dml_insert_bound_value_deinit(&right);
            return mylite_dml_insert_set_unsupported_expression_error(database);
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
            return mylite_dml_insert_set_unsupported_expression_error(database);
        }
    }

    mylite_dml_insert_bound_value_deinit(&left);
    mylite_dml_insert_bound_value_deinit(&right);
    return MYLITE_OK;
}

static int evaluate_insert_set_simple_expression(
    mylite_db *database,
    const char *schema_name,
    const struct mylite_insert_values_plan *plan,
    const struct mylite_insert_table *table,
    const struct mylite_insert_value *value,
    const struct mylite_insert_bound_value *values,
    struct mylite_insert_bound_value *out_value
) {
    int64_t integer_value = 0;
    double real_value = 0.0;

    switch (value->kind) {
    case MYLITE_INSERT_VALUE_NULL:
        *out_value = (struct mylite_insert_bound_value){.kind = MYLITE_INSERT_BOUND_NULL};
        return MYLITE_OK;
    case MYLITE_INSERT_VALUE_INTEGER:
        if (!mylite_dml_parse_insert_integer_text(value->text, &integer_value)) {
            return mylite_dml_insert_set_unsupported_expression_error(database);
        }
        *out_value = (struct mylite_insert_bound_value){
            .kind = MYLITE_INSERT_BOUND_INTEGER,
            .integer_value = integer_value,
        };
        return MYLITE_OK;
    case MYLITE_INSERT_VALUE_REAL:
        if (!mylite_dml_parse_insert_real_text(value->text, &real_value)) {
            return mylite_dml_insert_set_unsupported_expression_error(database);
        }
        *out_value = (struct mylite_insert_bound_value){
            .kind = MYLITE_INSERT_BOUND_REAL,
            .real_value = real_value,
        };
        return MYLITE_OK;
    case MYLITE_INSERT_VALUE_TEXT:
        out_value->text_value = mylite_copy_span_text(value->text, value->text_length);
        if (out_value->text_value == NULL) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
        out_value->kind = MYLITE_INSERT_BOUND_TEXT;
        out_value->text_length = value->text_length;
        return MYLITE_OK;
    case MYLITE_INSERT_VALUE_CURRENT_TIMESTAMP:
        return mylite_dml_resolve_insert_current_timestamp_bound_value(database, out_value);
    case MYLITE_INSERT_VALUE_COLUMN_REFERENCE:
        return evaluate_insert_set_column_reference(
            database,
            schema_name,
            plan,
            table,
            &value->column_reference,
            values,
            out_value
        );
    case MYLITE_INSERT_VALUE_DEFAULT:
    case MYLITE_INSERT_VALUE_UNSUPPORTED:
    case MYLITE_INSERT_VALUE_VALUES_FUNCTION:
    case MYLITE_INSERT_VALUE_UNARY_EXPRESSION:
    case MYLITE_INSERT_VALUE_BINARY_EXPRESSION:
    case MYLITE_INSERT_VALUE_EXPRESSION:
    case MYLITE_INSERT_VALUE_HEX_LITERAL:
    case MYLITE_INSERT_VALUE_BIT_LITERAL:
        return mylite_dml_insert_set_unsupported_expression_error(database);
    }

    return mylite_dml_insert_set_unsupported_expression_error(database);
}

static int evaluate_insert_set_column_reference(
    mylite_db *database,
    const char *schema_name,
    const struct mylite_insert_values_plan *plan,
    const struct mylite_insert_table *table,
    const struct mylite_insert_column_reference *ref,
    const struct mylite_insert_bound_value *values,
    struct mylite_insert_bound_value *out_value
) {
    size_t column_index =
        mylite_dml_insert_table_column_reference_index(table, schema_name, plan->table_name, ref);

    if (column_index == table->column_count) {
        int status = mylite_diagnostics_set_error_message_parts(
            database,
            "Unknown column '",
            ref->column_name,
            "' in 'field list'"
        );

        return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
    }
    int status = mylite_dml_copy_insert_bound_value(&values[column_index], out_value);

    if (status == MYLITE_NOMEM) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
    }
    return status;
}
