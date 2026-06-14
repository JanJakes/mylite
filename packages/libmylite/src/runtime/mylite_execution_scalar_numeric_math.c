#include "mylite_execution_scalar.h"
#include "mylite_execution_scalar_numeric.h"

#include "mylite_ast.h"
#include "mylite_connection.h"
#include "mylite_diagnostics.h"

#include <mylite/mylite.h>

#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    mysql_error_bigint_out_of_range = 1690,
};

static const double angle_conversion_half_turn_degrees = 180.0;
static const double logarithm_base_two = 2.0;
static const double logarithm_base_ten = 10.0;

struct approximate_numeric_input_value {
    bool is_null;
    bool is_negative;
    uint64_t magnitude;
    size_t division_by_zero_warning_count;
};

static const char *direct_trig_function_name(const struct mylite_sql_ast_node *expression);
static int finish_direct_trig_function_value(
    struct mylite_db *database,
    double output,
    const char *function_name,
    size_t warning_count,
    struct session_scalar_cell *out_cell
);
static int set_cot_zero_out_of_range_error(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
);
static int atan_one_argument_function_value(
    struct mylite_db *database,
    const struct approximate_numeric_input_value *first,
    const char *function_name,
    struct session_scalar_cell *out_cell
);
static int atan_two_argument_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct approximate_numeric_input_value *first,
    const char *function_name,
    struct session_scalar_cell *out_cell
);
static int finish_atan_function_value(
    struct mylite_db *database,
    double output,
    const char *function_name,
    size_t warning_count,
    struct session_scalar_cell *out_cell
);
static int one_argument_logarithm_function_value(
    struct mylite_db *database,
    const struct approximate_numeric_input_value *value,
    const char *function_name,
    double log_base,
    struct session_scalar_cell *out_cell
);
static int two_argument_logarithm_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct approximate_numeric_input_value *base,
    struct session_scalar_cell *out_cell
);
static int finish_exp_log_power_function_value(
    struct mylite_db *database,
    double output,
    const char *function_name,
    size_t division_warning_count,
    struct session_scalar_cell *out_cell
);
static int set_double_out_of_range_error(struct mylite_db *database, const char *function_name);
static double approximate_numeric_input_to_double(
    const struct approximate_numeric_input_value *value
);
static int evaluate_approximate_numeric_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct approximate_numeric_input_value *out_value,
    void (*unsupported_error_callback)(struct mylite_db *)
);
static int evaluate_approximate_numeric_direct_value_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct approximate_numeric_input_value *out_value,
    void (*unsupported_error_callback)(struct mylite_db *),
    bool *out_handled
);

int mylite_execution_scalar_sqrt_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct approximate_numeric_input_value value = {
        .is_null = false,
        .is_negative = false,
        .magnitude = 0U,
        .division_by_zero_warning_count = 0U,
    };
    double result = 0.0;
    uint64_t integer_result = 0U;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_SQRT_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        mylite_execution_set_sqrt_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = evaluate_approximate_numeric_operand(
        database,
        mylite_execution_child_at(expression, 0U),
        &value,
        mylite_execution_set_sqrt_unsupported_error
    );
    if (rc != MYLITE_OK || value.is_null || value.is_negative) {
        if (rc == MYLITE_OK) {
            out_cell->staged_division_by_zero_warning_count = value.division_by_zero_warning_count;
        }
        return rc;
    }

    result = sqrt((double)value.magnitude);
    integer_result = (uint64_t)result;
    if ((double)integer_result == result) {
        rc = mylite_execution_format_uint64(
            database,
            integer_result,
            out_cell->double_text,
            sizeof(out_cell->double_text)
        );
    } else {
        rc = mylite_execution_format_double_text(
            database,
            result,
            "SQRT",
            out_cell->double_text,
            sizeof(out_cell->double_text)
        );
    }
    if (rc == MYLITE_OK) {
        out_cell->value = out_cell->double_text;
        out_cell->staged_division_by_zero_warning_count = value.division_by_zero_warning_count;
    }
    return rc;
}

int mylite_execution_scalar_angle_conversion_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct approximate_numeric_input_value value = {
        .is_null = false,
        .is_negative = false,
        .magnitude = 0U,
        .division_by_zero_warning_count = 0U,
    };
    bool is_degrees = false;
    const char *function_name = NULL;
    double input = 0.0;
    double output = 0.0;
    double pi = acos(-1.0);
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression == NULL ||
        (expression->kind != MYLITE_SQL_AST_DEGREES_FUNCTION &&
         expression->kind != MYLITE_SQL_AST_RADIANS_FUNCTION) ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        mylite_execution_set_angle_conversion_unsupported_error(database);
        return MYLITE_ERROR;
    }

    is_degrees = expression->kind == MYLITE_SQL_AST_DEGREES_FUNCTION;
    if (is_degrees) {
        function_name = "DEGREES";
    } else {
        function_name = "RADIANS";
    }
    rc = evaluate_approximate_numeric_operand(
        database,
        mylite_execution_child_at(expression, 0U),
        &value,
        mylite_execution_set_angle_conversion_unsupported_error
    );
    if (rc != MYLITE_OK || value.is_null) {
        if (rc == MYLITE_OK) {
            out_cell->staged_division_by_zero_warning_count = value.division_by_zero_warning_count;
        }
        return rc;
    }

    input = (double)value.magnitude;
    if (value.is_negative) {
        input = -input;
    }
    if (is_degrees) {
        output = input * (angle_conversion_half_turn_degrees / pi);
    } else {
        output = input * (pi / angle_conversion_half_turn_degrees);
    }
    rc = mylite_execution_format_double_text(
        database,
        output,
        function_name,
        out_cell->double_text,
        sizeof(out_cell->double_text)
    );
    if (rc == MYLITE_OK) {
        out_cell->value = out_cell->double_text;
        out_cell->staged_division_by_zero_warning_count = value.division_by_zero_warning_count;
    }
    return rc;
}

int mylite_execution_scalar_inverse_trig_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct approximate_numeric_input_value value = {
        .is_null = false,
        .is_negative = false,
        .magnitude = 0U,
        .division_by_zero_warning_count = 0U,
    };
    bool is_acos = false;
    const char *function_name = NULL;
    double input = 0.0;
    double output = 0.0;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression == NULL ||
        (expression->kind != MYLITE_SQL_AST_ACOS_FUNCTION &&
         expression->kind != MYLITE_SQL_AST_ASIN_FUNCTION) ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        mylite_execution_set_inverse_trig_unsupported_error(database);
        return MYLITE_ERROR;
    }

    is_acos = expression->kind == MYLITE_SQL_AST_ACOS_FUNCTION;
    if (is_acos) {
        function_name = "ACOS";
    } else {
        function_name = "ASIN";
    }
    rc = evaluate_approximate_numeric_operand(
        database,
        mylite_execution_child_at(expression, 0U),
        &value,
        mylite_execution_set_inverse_trig_unsupported_error
    );
    if (rc != MYLITE_OK || value.is_null || value.magnitude > 1U) {
        if (rc == MYLITE_OK) {
            out_cell->staged_division_by_zero_warning_count = value.division_by_zero_warning_count;
        }
        return rc;
    }

    input = (double)value.magnitude;
    if (value.is_negative) {
        input = -input;
    }
    if (is_acos) {
        output = acos(input);
    } else {
        output = asin(input);
    }
    rc = mylite_execution_format_double_text(
        database,
        output,
        function_name,
        out_cell->double_text,
        sizeof(out_cell->double_text)
    );
    if (rc == MYLITE_OK) {
        out_cell->value = out_cell->double_text;
        out_cell->staged_division_by_zero_warning_count = value.division_by_zero_warning_count;
    }
    return rc;
}

int mylite_execution_scalar_direct_trig_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct approximate_numeric_input_value value = {
        .is_null = false,
        .is_negative = false,
        .magnitude = 0U,
        .division_by_zero_warning_count = 0U,
    };
    const char *function_name = NULL;
    size_t warning_count = 0U;
    double input = 0.0;
    double output = 0.0;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    function_name = direct_trig_function_name(expression);
    if (function_name == NULL || mylite_sql_ast_node_child_count(expression) != 1U) {
        mylite_execution_set_direct_trig_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = evaluate_approximate_numeric_operand(
        database,
        mylite_execution_child_at(expression, 0U),
        &value,
        mylite_execution_set_direct_trig_unsupported_error
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_execution_accumulate_staged_warning_count(
        database,
        value.division_by_zero_warning_count,
        &warning_count
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (value.is_null) {
        out_cell->staged_division_by_zero_warning_count = warning_count;
        return MYLITE_OK;
    }

    input = approximate_numeric_input_to_double(&value);
    switch (expression->kind) {
    case MYLITE_SQL_AST_SIN_FUNCTION:
        output = sin(input);
        break;
    case MYLITE_SQL_AST_COS_FUNCTION:
        output = cos(input);
        break;
    case MYLITE_SQL_AST_TAN_FUNCTION:
        output = tan(input);
        break;
    case MYLITE_SQL_AST_COT_FUNCTION:
        if (input == 0.0) {
            rc = mylite_execution_append_division_by_zero_warnings(database, warning_count);
            if (rc != MYLITE_OK) {
                return rc;
            }
            return set_cot_zero_out_of_range_error(database, expression);
        }
        output = 1.0 / tan(input);
        break;
    default:
        mylite_execution_set_direct_trig_unsupported_error(database);
        return MYLITE_ERROR;
    }

    return finish_direct_trig_function_value(
        database,
        output,
        function_name,
        warning_count,
        out_cell
    );
}

static const char *direct_trig_function_name(const struct mylite_sql_ast_node *expression) {
    if (expression == NULL) {
        return NULL;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_SIN_FUNCTION:
        return "SIN";
    case MYLITE_SQL_AST_COS_FUNCTION:
        return "COS";
    case MYLITE_SQL_AST_TAN_FUNCTION:
        return "TAN";
    case MYLITE_SQL_AST_COT_FUNCTION:
        return "COT";
    default:
        return NULL;
    }
}

static int finish_direct_trig_function_value(
    struct mylite_db *database,
    double output,
    const char *function_name,
    size_t warning_count,
    struct session_scalar_cell *out_cell
) {
    int rc = MYLITE_OK;

    if (out_cell == NULL || function_name == NULL) {
        return MYLITE_MISUSE;
    }
    rc = mylite_execution_format_double_text(
        database,
        output,
        function_name,
        out_cell->double_text,
        sizeof(out_cell->double_text)
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    out_cell->value = out_cell->double_text;
    out_cell->staged_division_by_zero_warning_count = warning_count;
    return MYLITE_OK;
}

static int set_cot_zero_out_of_range_error(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
) {
    enum { cot_error_expression_text_capacity = 64 };

    char text[cot_error_expression_text_capacity];
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    size_t length = 0U;
    int written = 0;

    if (expression != NULL && expression->span.text != NULL && expression->span.length != 0U &&
        expression->span.length < sizeof(text)) {
        length = expression->span.length;
        for (size_t index = 0U; index < length; ++index) {
            char character = expression->span.text[index];

            if (character >= 'A' && character <= 'Z') {
                character = (char)(character - 'A' + 'a');
            }
            text[index] = character;
        }
        text[length] = '\0';
    } else {
        memcpy(text, "cot(0)", sizeof("cot(0)"));
    }

    written = snprintf(message, sizeof(message), "DOUBLE value is out of range in '%s'", text);
    if (written < 0 || (size_t)written >= sizeof(message)) {
        mylite_execution_set_runtime_error(database, "DOUBLE value is out of range");
        return MYLITE_ERROR;
    }

    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_bigint_out_of_range,
        "22003",
        message
    );
    return MYLITE_ERROR;
}

int mylite_execution_scalar_atan_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct approximate_numeric_input_value first = {
        .is_null = false,
        .is_negative = false,
        .magnitude = 0U,
        .division_by_zero_warning_count = 0U,
    };
    const char *function_name = NULL;
    size_t child_count = 0U;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression == NULL || (expression->kind != MYLITE_SQL_AST_ATAN_FUNCTION &&
                               expression->kind != MYLITE_SQL_AST_ATAN2_FUNCTION)) {
        mylite_execution_set_atan_unsupported_error(database);
        return MYLITE_ERROR;
    }
    child_count = mylite_sql_ast_node_child_count(expression);
    if (child_count != 1U && child_count != 2U) {
        mylite_execution_set_atan_unsupported_error(database);
        return MYLITE_ERROR;
    }

    function_name = expression->kind == MYLITE_SQL_AST_ATAN_FUNCTION ? "ATAN" : "ATAN2";
    rc = evaluate_approximate_numeric_operand(
        database,
        mylite_execution_child_at(expression, 0U),
        &first,
        mylite_execution_set_atan_unsupported_error
    );
    if (rc != MYLITE_OK || first.is_null) {
        if (rc == MYLITE_OK) {
            out_cell->staged_division_by_zero_warning_count = first.division_by_zero_warning_count;
        }
        return rc;
    }

    if (child_count == 1U) {
        return atan_one_argument_function_value(database, &first, function_name, out_cell);
    }

    return atan_two_argument_function_value(database, expression, &first, function_name, out_cell);
}

static int atan_one_argument_function_value(
    struct mylite_db *database,
    const struct approximate_numeric_input_value *first,
    const char *function_name,
    struct session_scalar_cell *out_cell
) {
    size_t warning_count = 0U;
    double output = 0.0;
    int rc = MYLITE_OK;

    if (first == NULL) {
        return MYLITE_MISUSE;
    }
    rc = mylite_execution_accumulate_staged_division_by_zero_warnings(
        database,
        first->division_by_zero_warning_count,
        &warning_count
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    output = atan2(approximate_numeric_input_to_double(first), 1.0);
    return finish_atan_function_value(database, output, function_name, warning_count, out_cell);
}

static int atan_two_argument_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct approximate_numeric_input_value *first,
    const char *function_name,
    struct session_scalar_cell *out_cell
) {
    struct approximate_numeric_input_value second = {
        .is_null = false,
        .is_negative = false,
        .magnitude = 0U,
        .division_by_zero_warning_count = 0U,
    };
    size_t warning_count = 0U;
    double output = 0.0;
    int rc = MYLITE_OK;

    if (expression == NULL || first == NULL || out_cell == NULL) {
        return MYLITE_MISUSE;
    }

    rc = evaluate_approximate_numeric_operand(
        database,
        mylite_execution_child_at(expression, 1U),
        &second,
        mylite_execution_set_atan_unsupported_error
    );
    if (rc == MYLITE_OK) {
        rc = mylite_execution_accumulate_staged_division_by_zero_warnings(
            database,
            first->division_by_zero_warning_count,
            &warning_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_accumulate_staged_division_by_zero_warnings(
            database,
            second.division_by_zero_warning_count,
            &warning_count
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (second.is_null) {
        out_cell->staged_division_by_zero_warning_count = warning_count;
        return MYLITE_OK;
    }

    output = atan2(
        approximate_numeric_input_to_double(first),
        approximate_numeric_input_to_double(&second)
    );
    return finish_atan_function_value(database, output, function_name, warning_count, out_cell);
}

static int finish_atan_function_value(
    struct mylite_db *database,
    double output,
    const char *function_name,
    size_t warning_count,
    struct session_scalar_cell *out_cell
) {
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    rc = mylite_execution_format_double_text(
        database,
        output,
        function_name,
        out_cell->double_text,
        sizeof(out_cell->double_text)
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    out_cell->value = out_cell->double_text;
    out_cell->staged_division_by_zero_warning_count = warning_count;
    return MYLITE_OK;
}

int mylite_execution_scalar_exp_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct approximate_numeric_input_value value = {
        .is_null = false,
        .is_negative = false,
        .magnitude = 0U,
        .division_by_zero_warning_count = 0U,
    };
    double output = 0.0;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_EXP_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        mylite_execution_set_exp_log_power_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = evaluate_approximate_numeric_operand(
        database,
        mylite_execution_child_at(expression, 0U),
        &value,
        mylite_execution_set_exp_log_power_unsupported_error
    );
    if (rc != MYLITE_OK || value.is_null) {
        if (rc == MYLITE_OK) {
            out_cell->staged_division_by_zero_warning_count = value.division_by_zero_warning_count;
        }
        return rc;
    }

    output = exp(approximate_numeric_input_to_double(&value));
    if (!isfinite(output)) {
        return set_double_out_of_range_error(database, "exp");
    }
    return finish_exp_log_power_function_value(
        database,
        output,
        "EXP",
        value.division_by_zero_warning_count,
        out_cell
    );
}

int mylite_execution_scalar_logarithm_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct approximate_numeric_input_value value = {
        .is_null = false,
        .is_negative = false,
        .magnitude = 0U,
        .division_by_zero_warning_count = 0U,
    };
    const char *function_name = NULL;
    double log_base = 0.0;
    size_t child_count = 0U;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression == NULL) {
        mylite_execution_set_exp_log_power_unsupported_error(database);
        return MYLITE_ERROR;
    }
    child_count = mylite_sql_ast_node_child_count(expression);
    if (expression->kind == MYLITE_SQL_AST_LN_FUNCTION && child_count == 1U) {
        function_name = "LN";
        log_base = 0.0;
    } else if (expression->kind == MYLITE_SQL_AST_LOG_FUNCTION &&
               (child_count == 1U || child_count == 2U)) {
        function_name = "LOG";
        log_base = 0.0;
    } else if (expression->kind == MYLITE_SQL_AST_LOG10_FUNCTION && child_count == 1U) {
        function_name = "LOG10";
        log_base = logarithm_base_ten;
    } else if (expression->kind == MYLITE_SQL_AST_LOG2_FUNCTION && child_count == 1U) {
        function_name = "LOG2";
        log_base = logarithm_base_two;
    } else {
        mylite_execution_set_exp_log_power_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = evaluate_approximate_numeric_operand(
        database,
        mylite_execution_child_at(expression, 0U),
        &value,
        mylite_execution_set_exp_log_power_unsupported_error
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (child_count == 2U) {
        return two_argument_logarithm_function_value(database, expression, &value, out_cell);
    }
    return one_argument_logarithm_function_value(
        database,
        &value,
        function_name,
        log_base,
        out_cell
    );
}

static int one_argument_logarithm_function_value(
    struct mylite_db *database,
    const struct approximate_numeric_input_value *value,
    const char *function_name,
    double log_base,
    struct session_scalar_cell *out_cell
) {
    size_t division_warning_count = 0U;
    double input = 0.0;
    double output = 0.0;
    int rc = MYLITE_OK;

    if (value == NULL || function_name == NULL || out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    rc = mylite_execution_accumulate_staged_warning_count(
        database,
        value->division_by_zero_warning_count,
        &division_warning_count
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (value->is_null) {
        out_cell->staged_division_by_zero_warning_count = division_warning_count;
        return MYLITE_OK;
    }

    input = approximate_numeric_input_to_double(value);
    if (input <= 0.0) {
        out_cell->staged_division_by_zero_warning_count = division_warning_count;
        out_cell->staged_invalid_logarithm_warning_count = 1U;
        return MYLITE_OK;
    }
    if (log_base == 0.0) {
        output = log(input);
    } else {
        output = log(input) / log(log_base);
    }
    return finish_exp_log_power_function_value(
        database,
        output,
        function_name,
        division_warning_count,
        out_cell
    );
}

static int two_argument_logarithm_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct approximate_numeric_input_value *base,
    struct session_scalar_cell *out_cell
) {
    struct approximate_numeric_input_value value = {
        .is_null = false,
        .is_negative = false,
        .magnitude = 0U,
        .division_by_zero_warning_count = 0U,
    };
    size_t division_warning_count = 0U;
    double base_input = 0.0;
    double value_input = 0.0;
    double output = 0.0;
    int rc = MYLITE_OK;

    if (expression == NULL || base == NULL || out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    rc = mylite_execution_accumulate_staged_warning_count(
        database,
        base->division_by_zero_warning_count,
        &division_warning_count
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (base->is_null) {
        out_cell->staged_division_by_zero_warning_count = division_warning_count;
        return MYLITE_OK;
    }

    base_input = approximate_numeric_input_to_double(base);
    if (base_input <= 0.0 || base_input == 1.0) {
        out_cell->staged_division_by_zero_warning_count = division_warning_count;
        out_cell->staged_invalid_logarithm_warning_count = 1U;
        return MYLITE_OK;
    }

    rc = evaluate_approximate_numeric_operand(
        database,
        mylite_execution_child_at(expression, 1U),
        &value,
        mylite_execution_set_exp_log_power_unsupported_error
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_execution_accumulate_staged_warning_count(
        database,
        value.division_by_zero_warning_count,
        &division_warning_count
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (value.is_null) {
        out_cell->staged_division_by_zero_warning_count = division_warning_count;
        return MYLITE_OK;
    }

    value_input = approximate_numeric_input_to_double(&value);
    if (value_input <= 0.0) {
        out_cell->staged_division_by_zero_warning_count = division_warning_count;
        out_cell->staged_invalid_logarithm_warning_count = 1U;
        return MYLITE_OK;
    }

    output = log(value_input) / log(base_input);
    return finish_exp_log_power_function_value(
        database,
        output,
        "LOG",
        division_warning_count,
        out_cell
    );
}

int mylite_execution_scalar_power_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    struct approximate_numeric_input_value value = {
        .is_null = false,
        .is_negative = false,
        .magnitude = 0U,
        .division_by_zero_warning_count = 0U,
    };
    struct approximate_numeric_input_value exponent = {
        .is_null = false,
        .is_negative = false,
        .magnitude = 0U,
        .division_by_zero_warning_count = 0U,
    };
    const char *function_name = NULL;
    size_t division_warning_count = 0U;
    double output = 0.0;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression == NULL ||
        (expression->kind != MYLITE_SQL_AST_POW_FUNCTION &&
         expression->kind != MYLITE_SQL_AST_POWER_FUNCTION) ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        mylite_execution_set_exp_log_power_unsupported_error(database);
        return MYLITE_ERROR;
    }

    function_name = expression->kind == MYLITE_SQL_AST_POW_FUNCTION ? "POW" : "POWER";
    rc = evaluate_approximate_numeric_operand(
        database,
        mylite_execution_child_at(expression, 0U),
        &value,
        mylite_execution_set_exp_log_power_unsupported_error
    );
    if (rc == MYLITE_OK) {
        rc = evaluate_approximate_numeric_operand(
            database,
            mylite_execution_child_at(expression, 1U),
            &exponent,
            mylite_execution_set_exp_log_power_unsupported_error
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_accumulate_staged_warning_count(
            database,
            value.division_by_zero_warning_count,
            &division_warning_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_accumulate_staged_warning_count(
            database,
            exponent.division_by_zero_warning_count,
            &division_warning_count
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (value.is_null || exponent.is_null) {
        out_cell->staged_division_by_zero_warning_count = division_warning_count;
        return MYLITE_OK;
    }

    output =
        pow(approximate_numeric_input_to_double(&value),
            approximate_numeric_input_to_double(&exponent));
    if (!isfinite(output)) {
        return set_double_out_of_range_error(database, function_name);
    }
    return finish_exp_log_power_function_value(
        database,
        output,
        function_name,
        division_warning_count,
        out_cell
    );
}

static int finish_exp_log_power_function_value(
    struct mylite_db *database,
    double output,
    const char *function_name,
    size_t division_warning_count,
    struct session_scalar_cell *out_cell
) {
    int rc = MYLITE_OK;

    if (out_cell == NULL || function_name == NULL) {
        return MYLITE_MISUSE;
    }
    rc = mylite_execution_format_double_text(
        database,
        output,
        function_name,
        out_cell->double_text,
        sizeof(out_cell->double_text)
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    out_cell->value = out_cell->double_text;
    out_cell->staged_division_by_zero_warning_count = division_warning_count;
    return MYLITE_OK;
}

static int set_double_out_of_range_error(struct mylite_db *database, const char *function_name) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "DOUBLE value is out of range in '%s()'",
        function_name == NULL ? "math" : function_name
    );

    if (written < 0 || (size_t)written >= sizeof(message)) {
        mylite_execution_set_runtime_error(database, "DOUBLE value is out of range");
        return MYLITE_ERROR;
    }

    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_bigint_out_of_range,
        "22003",
        message
    );
    return MYLITE_ERROR;
}

static double approximate_numeric_input_to_double(
    const struct approximate_numeric_input_value *value
) {
    double input = 0.0;

    if (value == NULL) {
        return 0.0;
    }

    input = (double)value->magnitude;
    if (value->is_negative) {
        input = -input;
    }
    return input;
}

static int evaluate_approximate_numeric_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct approximate_numeric_input_value *out_value,
    void (*unsupported_error_callback)(struct mylite_db *)
) {
    struct scalar_arithmetic_value arithmetic = {.is_null = false, .integer = 0};
    bool handled = false;
    int rc = MYLITE_OK;

    if (out_value == NULL || unsupported_error_callback == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = (struct approximate_numeric_input_value){.is_null = false, .is_negative = false};
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        unsupported_error_callback(database);
        return MYLITE_ERROR;
    }

    rc = evaluate_approximate_numeric_direct_value_operand(
        database,
        expression,
        out_value,
        unsupported_error_callback,
        &handled
    );
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    if (mylite_execution_is_scalar_bitwise_projection_expression(expression)) {
        struct scalar_bitwise_value bitwise = {.is_null = false, .integer = 0U};

        rc = mylite_execution_evaluate_scalar_bitwise_expression(database, expression, &bitwise);
        if (rc != MYLITE_OK) {
            return rc;
        }
        out_value->is_null = bitwise.is_null;
        out_value->magnitude = bitwise.integer;
        out_value->division_by_zero_warning_count = bitwise.division_by_zero_warning_count;
        return MYLITE_OK;
    }
    if (!mylite_execution_is_scalar_arithmetic_projection_expression(expression)) {
        unsupported_error_callback(database);
        return MYLITE_ERROR;
    }

    rc = mylite_execution_evaluate_scalar_arithmetic_expression(database, expression, &arithmetic);
    if (rc != MYLITE_OK) {
        return rc;
    }
    out_value->is_null = arithmetic.is_null;
    out_value->division_by_zero_warning_count = arithmetic.division_by_zero_warning_count;
    if (arithmetic.is_null) {
        return MYLITE_OK;
    }
    if (arithmetic.integer < 0) {
        out_value->is_negative = true;
        out_value->magnitude = arithmetic.integer == INT64_MIN ? ((uint64_t)INT64_MAX + 1U)
                                                               : (uint64_t)(-arithmetic.integer);
    } else {
        out_value->magnitude = (uint64_t)arithmetic.integer;
    }
    return MYLITE_OK;
}

static int evaluate_approximate_numeric_direct_value_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct approximate_numeric_input_value *out_value,
    void (*unsupported_error_callback)(struct mylite_db *),
    bool *out_handled
) {
    const struct mylite_sql_ast_node *literal = expression;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    bool is_negative = false;
    bool has_sign = false;
    uint64_t magnitude = 0U;

    if (out_value == NULL || unsupported_error_callback == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    if (expression == NULL) {
        return MYLITE_OK;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);

        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            return MYLITE_OK;
        }
        literal = mylite_execution_unwrap_parenthesized_expression(
            mylite_execution_child_at(expression, 0U)
        );
        if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
            mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
            return MYLITE_OK;
        }
        has_sign = true;
        is_negative = operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE;
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        return MYLITE_OK;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (!has_sign && literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        out_value->is_null = true;
        *out_handled = true;
        return MYLITE_OK;
    }
    if (!has_sign && literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        out_value->magnitude = 1U;
        *out_handled = true;
        return MYLITE_OK;
    }
    if (!has_sign && literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        out_value->magnitude = 0U;
        *out_handled = true;
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        return MYLITE_OK;
    }
    *out_handled = true;

    if (mylite_execution_parse_unsigned_integer_literal(&literal->span, &magnitude) != MYLITE_OK) {
        unsupported_error_callback(database);
        return MYLITE_ERROR;
    }
    out_value->is_negative = false;
    if (is_negative && magnitude != 0U) {
        out_value->is_negative = true;
    }
    out_value->magnitude = magnitude;
    return MYLITE_OK;
}
