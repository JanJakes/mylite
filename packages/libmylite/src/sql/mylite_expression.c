#include "mylite_expression.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// NOLINTBEGIN(misc-no-recursion, readability-implicit-bool-conversion)

enum {
    MYLITE_WARNING_UNKNOWN = 1105,
    MYLITE_WARNING_INCORRECT_ESCAPE_ARGUMENTS = 1210,
    MYLITE_WARNING_TRUNCATED_WRONG_VALUE = 1292,
    MYLITE_WARNING_DIVISION_BY_ZERO = 1365,
    MYLITE_EXPRESSION_TEXT_BUFFER_SIZE = 64,
    MYLITE_EXPRESSION_DECIMAL_TEXT_BUFFER_SIZE = 128,
    MYLITE_EXPRESSION_DECIMAL_BASE = 10,
    MYLITE_EXPRESSION_UINT64_DIGITS = 19,
    MYLITE_EXPRESSION_BITS_PER_UINT64 = 64,
    MYLITE_EXPRESSION_WARNING_MESSAGE_SIZE = 256,
    MYLITE_EXPRESSION_WARNING_TEXT_PREVIEW = 160,
    MYLITE_UTF8_CONTINUATION_MASK = 0xC0U,
    MYLITE_UTF8_CONTINUATION_MARKER = 0x80U,
};

static const char mylite_pi_text[] = "3.141593";
static const uint64_t mylite_expression_int64_min_magnitude = (uint64_t)INT64_MAX + UINT64_C(1);
static const double mylite_expression_round_half = 0.5;

struct numeric_value {
    double real_value;
    int64_t int64_value;
    uint64_t uint64_value;
    bool is_integer;
    bool is_unsigned;
};

struct between_truth {
    int low;
    int high;
};

struct cast_integer_parse {
    uint64_t magnitude;
    bool negative;
    bool saw_digit;
    bool trailing_garbage;
    bool overflow;
};

enum mylite_scalar_function_id {
    MYLITE_SCALAR_FUNCTION_UNKNOWN = 0,
    MYLITE_SCALAR_FUNCTION_CONCAT = 1,
    MYLITE_SCALAR_FUNCTION_LENGTH = 2,
    MYLITE_SCALAR_FUNCTION_CHAR_LENGTH = 3,
    MYLITE_SCALAR_FUNCTION_LOWER = 4,
    MYLITE_SCALAR_FUNCTION_UPPER = 5,
    MYLITE_SCALAR_FUNCTION_LEFT = 6,
    MYLITE_SCALAR_FUNCTION_RIGHT = 7,
    MYLITE_SCALAR_FUNCTION_REPLACE = 8,
    MYLITE_SCALAR_FUNCTION_ABS = 9,
    MYLITE_SCALAR_FUNCTION_SIGN = 10,
    MYLITE_SCALAR_FUNCTION_FLOOR = 11,
    MYLITE_SCALAR_FUNCTION_CEIL = 12,
    MYLITE_SCALAR_FUNCTION_MOD = 13,
    MYLITE_SCALAR_FUNCTION_PI = 14,
    MYLITE_SCALAR_FUNCTION_IF = 15,
    MYLITE_SCALAR_FUNCTION_IFNULL = 16,
    MYLITE_SCALAR_FUNCTION_NULLIF = 17,
    MYLITE_SCALAR_FUNCTION_COALESCE = 18,
    MYLITE_SCALAR_FUNCTION_ISNULL = 19,
    MYLITE_SCALAR_FUNCTION_DATABASE = 20,
    MYLITE_SCALAR_FUNCTION_SCHEMA = 21,
    MYLITE_SCALAR_FUNCTION_VERSION = 22,
    MYLITE_SCALAR_FUNCTION_LAST_INSERT_ID = 23,
    MYLITE_SCALAR_FUNCTION_ROW_COUNT = 24,
};

static int eval_node(const struct mylite_sql_ast_node *node,
                     const struct mylite_expression_eval_context *context,
                     struct mylite_expression_warnings *warnings,
                     struct mylite_expression_value *out_value);
static int eval_unary(const struct mylite_sql_ast_node *node,
                      const struct mylite_expression_eval_context *context,
                      struct mylite_expression_warnings *warnings,
                      struct mylite_expression_value *out_value);
static int eval_binary(const struct mylite_sql_ast_node *node,
                       const struct mylite_expression_eval_context *context,
                       struct mylite_expression_warnings *warnings,
                       struct mylite_expression_value *out_value);
static int eval_logical_and(const struct mylite_sql_ast_node *node,
                            const struct mylite_expression_eval_context *context,
                            struct mylite_expression_warnings *warnings,
                            struct mylite_expression_value *out_value);
static int eval_logical_or(const struct mylite_sql_ast_node *node,
                           const struct mylite_expression_eval_context *context,
                           struct mylite_expression_warnings *warnings,
                           struct mylite_expression_value *out_value);
static int validate_boolean_shortcut_operand(const struct mylite_sql_ast_node *node,
                                             const struct mylite_expression_eval_context *context,
                                             struct mylite_expression_warnings *warnings);
static int
validate_like_escape_before_shortcut(const struct mylite_sql_ast_node *node,
                                     const struct mylite_expression_eval_context *context,
                                     struct mylite_expression_warnings *warnings);
static bool expression_is_constant_boolean(const struct mylite_sql_ast_node *node,
                                           bool expected_value);
static int eval_ternary(const struct mylite_sql_ast_node *node,
                        const struct mylite_expression_eval_context *context,
                        struct mylite_expression_warnings *warnings,
                        struct mylite_expression_value *out_value);
static int eval_case_expression(const struct mylite_sql_ast_node *node,
                                const struct mylite_expression_eval_context *context,
                                struct mylite_expression_warnings *warnings,
                                struct mylite_expression_value *out_value);
static int eval_simple_case_expression(const struct mylite_sql_ast_node *node,
                                       const struct mylite_expression_eval_context *context,
                                       struct mylite_expression_warnings *warnings,
                                       struct mylite_expression_value *out_value);
static int eval_searched_case_expression(const struct mylite_sql_ast_node *node,
                                         const struct mylite_expression_eval_context *context,
                                         struct mylite_expression_warnings *warnings,
                                         struct mylite_expression_value *out_value);
static int eval_case_default(const struct mylite_sql_ast_node *expression,
                             const struct mylite_expression_eval_context *context,
                             struct mylite_expression_warnings *warnings,
                             struct mylite_expression_value *out_value);
static int eval_cast_expression(const struct mylite_sql_ast_node *node,
                                const struct mylite_expression_eval_context *context,
                                struct mylite_expression_warnings *warnings,
                                struct mylite_expression_value *out_value);
static int eval_signed_cast(const struct mylite_expression_value *value,
                            struct mylite_expression_warnings *warnings,
                            struct mylite_expression_value *out_value);
static int eval_unsigned_cast(const struct mylite_expression_value *value,
                              struct mylite_expression_warnings *warnings,
                              struct mylite_expression_value *out_value);
static int eval_decimal_cast(const struct mylite_sql_ast_node *target,
                             const struct mylite_expression_value *value,
                             struct mylite_expression_warnings *warnings,
                             struct mylite_expression_value *out_value);
static int eval_char_cast(const struct mylite_sql_ast_node *target,
                          const struct mylite_expression_value *value,
                          struct mylite_expression_warnings *warnings,
                          struct mylite_expression_value *out_value);
static int eval_binary_cast(const struct mylite_expression_value *value,
                            struct mylite_expression_value *out_value);
static int eval_function_call(const struct mylite_sql_ast_node *node,
                              const struct mylite_expression_eval_context *context,
                              struct mylite_expression_warnings *warnings,
                              struct mylite_expression_value *out_value);
static int eval_concat_function(const struct mylite_sql_ast_node *arguments,
                                const struct mylite_expression_eval_context *context,
                                struct mylite_expression_warnings *warnings,
                                struct mylite_expression_value *out_value);
static int eval_unary_string_function(enum mylite_scalar_function_id function_id,
                                      const struct mylite_sql_ast_node *arguments,
                                      const struct mylite_expression_eval_context *context,
                                      struct mylite_expression_warnings *warnings,
                                      struct mylite_expression_value *out_value);
static int eval_left_right_function(enum mylite_scalar_function_id function_id,
                                    const struct mylite_sql_ast_node *arguments,
                                    const struct mylite_expression_eval_context *context,
                                    struct mylite_expression_warnings *warnings,
                                    struct mylite_expression_value *out_value);
static int eval_replace_function(const struct mylite_sql_ast_node *arguments,
                                 const struct mylite_expression_eval_context *context,
                                 struct mylite_expression_warnings *warnings,
                                 struct mylite_expression_value *out_value);
static int eval_mod_function(const struct mylite_sql_ast_node *arguments,
                             const struct mylite_expression_eval_context *context,
                             struct mylite_expression_warnings *warnings,
                             struct mylite_expression_value *out_value);
static int eval_numeric_unary_function(enum mylite_scalar_function_id function_id,
                                       const struct mylite_sql_ast_node *arguments,
                                       const struct mylite_expression_eval_context *context,
                                       struct mylite_expression_warnings *warnings,
                                       struct mylite_expression_value *out_value);
static void eval_abs_function(const struct numeric_value *number,
                              struct mylite_expression_value *out_value);
static int eval_if_function(const struct mylite_sql_ast_node *arguments,
                            const struct mylite_expression_eval_context *context,
                            struct mylite_expression_warnings *warnings,
                            struct mylite_expression_value *out_value);
static int eval_ifnull_function(const struct mylite_sql_ast_node *arguments,
                                const struct mylite_expression_eval_context *context,
                                struct mylite_expression_warnings *warnings,
                                struct mylite_expression_value *out_value);
static int eval_nullif_function(const struct mylite_sql_ast_node *arguments,
                                const struct mylite_expression_eval_context *context,
                                struct mylite_expression_warnings *warnings,
                                struct mylite_expression_value *out_value);
static int eval_coalesce_function(const struct mylite_sql_ast_node *arguments,
                                  const struct mylite_expression_eval_context *context,
                                  struct mylite_expression_warnings *warnings,
                                  struct mylite_expression_value *out_value);
static int eval_isnull_function(const struct mylite_sql_ast_node *arguments,
                                const struct mylite_expression_eval_context *context,
                                struct mylite_expression_warnings *warnings,
                                struct mylite_expression_value *out_value);
static int eval_literal(const struct mylite_sql_ast_node *node,
                        struct mylite_expression_value *out_value);
static int eval_is_expression(enum mylite_sql_ast_operator operator_kind,
                              const struct mylite_sql_ast_node *operand,
                              const struct mylite_expression_eval_context *context,
                              struct mylite_expression_warnings *warnings,
                              struct mylite_expression_value *out_value);
static int eval_between(enum mylite_sql_ast_operator operator_kind,
                        const struct mylite_sql_ast_node *node,
                        const struct mylite_expression_eval_context *context,
                        struct mylite_expression_warnings *warnings,
                        struct mylite_expression_value *out_value);
static int eval_between_bound_truth(const struct mylite_expression_value *value,
                                    const struct mylite_expression_value *bound, bool lower_bound,
                                    struct mylite_expression_warnings *warnings, int *out_truth);
static void set_between_result(enum mylite_sql_ast_operator operator_kind,
                               struct between_truth truth,
                               struct mylite_expression_value *out_value);
static int eval_like(enum mylite_sql_ast_operator operator_kind,
                     const struct mylite_sql_ast_node *node,
                     const struct mylite_expression_eval_context *context,
                     struct mylite_expression_warnings *warnings,
                     struct mylite_expression_value *out_value);
static int eval_in(enum mylite_sql_ast_operator operator_kind,
                   const struct mylite_sql_ast_node *node,
                   const struct mylite_expression_eval_context *context,
                   struct mylite_expression_warnings *warnings,
                   struct mylite_expression_value *out_value);
static bool binary_expression_is_row_subquery(const struct mylite_sql_ast_node *node);
static bool binary_expression_is_row_scalar_subquery(const struct mylite_sql_ast_node *node);
static bool
row_subquery_comparison_operator_is_supported(enum mylite_sql_ast_operator operator_kind);
static bool quantified_comparison_has_row_left(const struct mylite_sql_ast_node *node);
static int eval_quantified_comparison(const struct mylite_sql_ast_node *node,
                                      const struct mylite_expression_eval_context *context,
                                      struct mylite_expression_warnings *warnings,
                                      struct mylite_expression_value *out_value);
static int eval_numeric_unary(enum mylite_sql_ast_operator operator_kind,
                              const struct mylite_expression_value *operand,
                              struct mylite_expression_warnings *warnings,
                              struct mylite_expression_value *out_value);
static int eval_arithmetic(enum mylite_sql_ast_operator operator_kind,
                           const struct mylite_expression_value *left,
                           const struct mylite_expression_value *right,
                           struct mylite_expression_warnings *warnings,
                           struct mylite_expression_value *out_value);
static int eval_bitwise(enum mylite_sql_ast_operator operator_kind,
                        const struct mylite_expression_value *left,
                        const struct mylite_expression_value *right,
                        struct mylite_expression_warnings *warnings,
                        struct mylite_expression_value *out_value);
static int eval_comparison(enum mylite_sql_ast_operator operator_kind,
                           const struct mylite_expression_value *left,
                           const struct mylite_expression_value *right,
                           struct mylite_expression_warnings *warnings,
                           struct mylite_expression_value *out_value);
static int eval_logical(enum mylite_sql_ast_operator operator_kind,
                        const struct mylite_expression_value *left,
                        const struct mylite_expression_value *right,
                        struct mylite_expression_warnings *warnings,
                        struct mylite_expression_value *out_value);
static int truth_value(const struct mylite_expression_value *value,
                       struct mylite_expression_warnings *warnings, int *out_truth);
static int compare_values(const struct mylite_expression_value *left,
                          const struct mylite_expression_value *right,
                          struct mylite_expression_warnings *warnings, int *out_compare);
static int value_to_numeric(const struct mylite_expression_value *value,
                            struct mylite_expression_warnings *warnings,
                            struct numeric_value *out_numeric);
static int cast_value_to_signed_integer(const struct mylite_expression_value *value,
                                        struct mylite_expression_warnings *warnings,
                                        int64_t *out_integer);
static int cast_value_to_unsigned_integer(const struct mylite_expression_value *value,
                                          struct mylite_expression_warnings *warnings,
                                          uint64_t *out_integer);
static int cast_string_to_signed_integer(const char *text,
                                         struct mylite_expression_warnings *warnings,
                                         int64_t *out_integer);
static int cast_string_to_unsigned_integer(const char *text,
                                           struct mylite_expression_warnings *warnings,
                                           uint64_t *out_integer);
static struct cast_integer_parse parse_cast_integer_text(const char *text);
static int64_t signed_integer_from_uint64(uint64_t value);
static uint64_t unsigned_complement_from_magnitude(uint64_t magnitude);
static int cast_value_to_decimal_double(const struct mylite_expression_value *value,
                                        struct mylite_expression_warnings *warnings,
                                        double *out_number);
static int cast_string_to_decimal_double(const char *text,
                                         struct mylite_expression_warnings *warnings,
                                         double *out_number);
static int cast_value_to_string(const struct mylite_expression_value *value, char **out_text);
static int cast_real_to_string(double value, char **out_text);
static int64_t cast_real_to_signed_integer(double value);
static unsigned int cast_decimal_scale(const struct mylite_sql_ast_node *target);
static double absolute_real_value(double value);
static int64_t floor_real_value(double value);
static int64_t ceil_real_value(double value);
static int value_to_string(const struct mylite_expression_value *value, char **out_text);
static int set_text_value(const char *text, size_t length,
                          struct mylite_expression_value *out_value);
static int append_text(char **text, size_t *length, const char *addition, size_t addition_length);
static int utf8_char_count(const char *text, int64_t *out_count);
static size_t utf8_offset_for_chars(const char *text, int64_t char_count);
static int append_warning(struct mylite_expression_warnings *warnings, unsigned int code,
                          const char *message);
static int append_truncation_warning(struct mylite_expression_warnings *warnings, const char *text);
static int append_cast_truncation_warning(struct mylite_expression_warnings *warnings,
                                          const char *type_name, const char *text);
static int append_char_truncation_warning(struct mylite_expression_warnings *warnings,
                                          uint64_t length, const char *text);
static int append_signed_complement_warning(struct mylite_expression_warnings *warnings);
static int append_unsigned_complement_warning(struct mylite_expression_warnings *warnings);
static char *copy_span_text(const char *text, size_t length);
static char *decode_string_literal(const struct mylite_sql_ast_node *node);
static bool decode_string_escape(char escaped, char *out_character);
static const struct mylite_sql_ast_node *
unwrap_parenthesized_node(const struct mylite_sql_ast_node *node);
static const struct mylite_sql_ast_node *child_at(const struct mylite_sql_ast_node *node,
                                                  size_t index);
static size_t child_count(const struct mylite_sql_ast_node *node);
static bool expression_is_supported_no_table(const struct mylite_sql_ast_node *expression,
                                             bool require_cacheable);
static enum mylite_scalar_function_id scalar_function_id(const struct mylite_sql_ast_node *node);
static enum mylite_scalar_function_id
scalar_function_id_from_span(struct mylite_sql_source_span span);
static bool scalar_function_depends_on_session(enum mylite_scalar_function_id function_id);
static bool ascii_span_equals(struct mylite_sql_source_span span, const char *text);
static bool is_null(const struct mylite_expression_value *value);
static bool is_numeric_kind(enum mylite_expression_value_kind kind);
static bool like_match(const char *value, const char *pattern, char escape);
static bool like_match_here(const char *value, const char *pattern, char escape);
static int ascii_case_fold(int character);

void mylite_expression_value_deinit(struct mylite_expression_value *value)
{
    if (value == NULL) {
        return;
    }

    free(value->text_value);
    *value = (struct mylite_expression_value){0};
}

void mylite_expression_warnings_deinit(struct mylite_expression_warnings *warnings)
{
    if (warnings == NULL) {
        return;
    }

    for (size_t index = 0U; index < warnings->count; ++index) {
        free(warnings->items[index].message);
    }
    free(warnings->items);
    *warnings = (struct mylite_expression_warnings){0};
}

int mylite_expression_warnings_append(struct mylite_expression_warnings *warnings,
                                      unsigned int code, const char *message)
{
    return mylite_expression_warnings_append_condition(
        warnings, MYLITE_EXPRESSION_WARNING_LEVEL_WARNING, code, message);
}

int mylite_expression_warnings_append_condition(struct mylite_expression_warnings *warnings,
                                                enum mylite_expression_warning_level level,
                                                unsigned int code, const char *message)
{
    struct mylite_expression_warning *items = NULL;
    char *copy = NULL;

    if (warnings == NULL) {
        return 0;
    }
    copy = copy_span_text(message == NULL ? "" : message, message == NULL ? 0U : strlen(message));
    if (copy == NULL) {
        return -1;
    }
    items = realloc(warnings->items, (warnings->count + 1U) * sizeof(*warnings->items));
    if (items == NULL) {
        free(copy);
        return -1;
    }
    warnings->items = items;
    warnings->items[warnings->count++] =
        (struct mylite_expression_warning){.code = code, .message = copy, .level = level};
    return 0;
}

int mylite_expression_eval(const struct mylite_sql_ast_node *expression,
                           struct mylite_expression_warnings *warnings,
                           struct mylite_expression_value *out_value)
{
    return mylite_expression_eval_with_context(expression, NULL, warnings, out_value);
}

int mylite_expression_eval_with_context(const struct mylite_sql_ast_node *expression,
                                        const struct mylite_expression_eval_context *context,
                                        struct mylite_expression_warnings *warnings,
                                        struct mylite_expression_value *out_value)
{
    if (out_value == NULL) {
        return -1;
    }

    *out_value = (struct mylite_expression_value){0};
    return eval_node(expression, context, warnings, out_value);
}

int mylite_expression_value_copy(const struct mylite_expression_value *value,
                                 struct mylite_expression_value *out_value)
{
    *out_value = *value;
    out_value->text_value = NULL;
    if (value->text_value != NULL) {
        out_value->text_value = copy_span_text(value->text_value, strlen(value->text_value));
        if (out_value->text_value == NULL) {
            return -1;
        }
    }
    return 0;
}

char *mylite_expression_value_to_text(const struct mylite_expression_value *value)
{
    char buffer[MYLITE_EXPRESSION_TEXT_BUFFER_SIZE];

    if (value == NULL || value->kind == MYLITE_EXPRESSION_VALUE_NULL) {
        return NULL;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_TEXT) {
        return copy_span_text(value->text_value,
                              value->text_value == NULL ? 0U : strlen(value->text_value));
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_INT64) {
        int length = snprintf(buffer, sizeof(buffer), "%lld", (long long)value->int64_value);
        return length < 0 ? NULL : copy_span_text(buffer, (size_t)length);
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_UINT64) {
        int length =
            snprintf(buffer, sizeof(buffer), "%llu", (unsigned long long)value->uint64_value);
        return length < 0 ? NULL : copy_span_text(buffer, (size_t)length);
    }

    int length = snprintf(buffer, sizeof(buffer), "%.4f", value->real_value);
    return length < 0 ? NULL : copy_span_text(buffer, (size_t)length);
}

int64_t mylite_expression_value_to_int64(const struct mylite_expression_value *value)
{
    if (value == NULL || value->kind == MYLITE_EXPRESSION_VALUE_NULL) {
        return 0;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_INT64) {
        return value->int64_value;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_UINT64) {
        return value->uint64_value > (uint64_t)INT64_MAX ? INT64_MAX : (int64_t)value->uint64_value;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_REAL) {
        return (int64_t)value->real_value;
    }
    return value->text_value == NULL
               ? 0
               : strtoll(value->text_value, NULL, MYLITE_EXPRESSION_DECIMAL_BASE);
}

int mylite_expression_value_compare(const struct mylite_expression_value *left,
                                    const struct mylite_expression_value *right,
                                    struct mylite_expression_warnings *warnings, int *out_compare)
{
    if (out_compare == NULL) {
        return -1;
    }
    return compare_values(left, right, warnings, out_compare);
}

int mylite_expression_value_truth(const struct mylite_expression_value *value,
                                  struct mylite_expression_warnings *warnings, int *out_truth)
{
    if (out_truth == NULL) {
        return -1;
    }
    return truth_value(value, warnings, out_truth);
}

bool mylite_expression_is_supported_no_table(const struct mylite_sql_ast_node *expression)
{
    return expression_is_supported_no_table(expression, false);
}

bool mylite_expression_is_cacheable_no_table(const struct mylite_sql_ast_node *expression)
{
    return expression_is_supported_no_table(expression, true);
}

static bool expression_is_supported_no_table(const struct mylite_sql_ast_node *expression,
                                             bool require_cacheable)
{
    if (expression == NULL) {
        return false;
    }
    switch (expression->kind) {
    case MYLITE_SQL_AST_LITERAL:
        switch (expression->literal_kind) {
        case MYLITE_SQL_AST_LITERAL_NULL:
        case MYLITE_SQL_AST_LITERAL_TRUE:
        case MYLITE_SQL_AST_LITERAL_FALSE:
        case MYLITE_SQL_AST_LITERAL_INTEGER:
        case MYLITE_SQL_AST_LITERAL_DECIMAL:
        case MYLITE_SQL_AST_LITERAL_FLOAT:
        case MYLITE_SQL_AST_LITERAL_STRING:
        case MYLITE_SQL_AST_LITERAL_NATIONAL_STRING:
            return true;
        case MYLITE_SQL_AST_LITERAL_HEX:
        case MYLITE_SQL_AST_LITERAL_BIT:
        case MYLITE_SQL_AST_LITERAL_NONE:
            return false;
        }
        return false;
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_TERNARY_EXPRESSION:
    case MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION:
    case MYLITE_SQL_AST_EXPRESSION_LIST:
    case MYLITE_SQL_AST_CASE_EXPRESSION:
    case MYLITE_SQL_AST_CASE_WHEN_LIST:
    case MYLITE_SQL_AST_CASE_WHEN:
        for (const struct mylite_sql_ast_node *child = expression->first_child; child != NULL;
             child = child->next_sibling) {
            if (!expression_is_supported_no_table(child, require_cacheable)) {
                return false;
            }
        }
        return true;
    case MYLITE_SQL_AST_CAST_EXPRESSION:
        return expression_is_supported_no_table(child_at(expression, 0U), require_cacheable);
    case MYLITE_SQL_AST_FUNCTION_CALL:
        if (!mylite_expression_is_supported_function_call(expression)) {
            return false;
        }
        if (require_cacheable &&
            scalar_function_depends_on_session(scalar_function_id(expression))) {
            return false;
        }
        for (const struct mylite_sql_ast_node *child =
                 child_at(expression, 1U) == NULL ? NULL : child_at(expression, 1U)->first_child;
             child != NULL; child = child->next_sibling) {
            if (!expression_is_supported_no_table(child, require_cacheable)) {
                return false;
            }
        }
        return true;
    case MYLITE_SQL_AST_IDENTIFIER:
    case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER:
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP:
    default:
        return false;
    }
}

bool mylite_expression_is_supported_function_call(const struct mylite_sql_ast_node *expression)
{
    const struct mylite_sql_ast_node *arguments = child_at(expression, 1U);
    enum mylite_scalar_function_id function_id = scalar_function_id(expression);
    size_t arity = child_count(arguments);

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_FUNCTION_CALL ||
        arguments == NULL || arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) {
        return false;
    }

    switch (function_id) {
    case MYLITE_SCALAR_FUNCTION_CONCAT:
        return arity >= 1U;
    case MYLITE_SCALAR_FUNCTION_LENGTH:
    case MYLITE_SCALAR_FUNCTION_CHAR_LENGTH:
    case MYLITE_SCALAR_FUNCTION_LOWER:
    case MYLITE_SCALAR_FUNCTION_UPPER:
    case MYLITE_SCALAR_FUNCTION_ABS:
    case MYLITE_SCALAR_FUNCTION_SIGN:
    case MYLITE_SCALAR_FUNCTION_FLOOR:
    case MYLITE_SCALAR_FUNCTION_CEIL:
    case MYLITE_SCALAR_FUNCTION_ISNULL:
        return arity == 1U;
    case MYLITE_SCALAR_FUNCTION_LEFT:
    case MYLITE_SCALAR_FUNCTION_RIGHT:
    case MYLITE_SCALAR_FUNCTION_MOD:
    case MYLITE_SCALAR_FUNCTION_IFNULL:
    case MYLITE_SCALAR_FUNCTION_NULLIF:
        return arity == 2U;
    case MYLITE_SCALAR_FUNCTION_REPLACE:
    case MYLITE_SCALAR_FUNCTION_IF:
        return arity == 3U;
    case MYLITE_SCALAR_FUNCTION_PI:
    case MYLITE_SCALAR_FUNCTION_DATABASE:
    case MYLITE_SCALAR_FUNCTION_SCHEMA:
    case MYLITE_SCALAR_FUNCTION_VERSION:
    case MYLITE_SCALAR_FUNCTION_ROW_COUNT:
        return arity == 0U;
    case MYLITE_SCALAR_FUNCTION_LAST_INSERT_ID:
        return arity == 0U || arity == 1U;
    case MYLITE_SCALAR_FUNCTION_COALESCE:
        return arity >= 1U;
    case MYLITE_SCALAR_FUNCTION_UNKNOWN:
        return false;
    }
    return false;
}

static int eval_node(const struct mylite_sql_ast_node *node,
                     const struct mylite_expression_eval_context *context,
                     struct mylite_expression_warnings *warnings,
                     struct mylite_expression_value *out_value)
{
    if (node == NULL) {
        return -1;
    }
    while (node->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
        node = child_at(node, 0U);
        if (node == NULL) {
            return -1;
        }
    }
    if (context != NULL && context->eval_constant != NULL &&
        mylite_expression_is_cacheable_no_table(node)) {
        return context->eval_constant(context->user_data, node, warnings, out_value);
    }

    switch (node->kind) {
    case MYLITE_SQL_AST_LITERAL:
        return eval_literal(node, out_value);
    case MYLITE_SQL_AST_IDENTIFIER:
    case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER:
        if (context != NULL && context->resolve_identifier != NULL) {
            return context->resolve_identifier(context->user_data, node, out_value);
        }
        return -1;
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
        return eval_unary(node, context, warnings, out_value);
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
        return eval_binary(node, context, warnings, out_value);
    case MYLITE_SQL_AST_TERNARY_EXPRESSION:
        return eval_ternary(node, context, warnings, out_value);
    case MYLITE_SQL_AST_CASE_EXPRESSION:
        return eval_case_expression(node, context, warnings, out_value);
    case MYLITE_SQL_AST_CAST_EXPRESSION:
        return eval_cast_expression(node, context, warnings, out_value);
    case MYLITE_SQL_AST_AGGREGATE_CALL:
        if (context != NULL && context->eval_aggregate != NULL) {
            return context->eval_aggregate(context->user_data, node, out_value);
        }
        return -1;
    case MYLITE_SQL_AST_SUBQUERY_EXPRESSION:
    case MYLITE_SQL_AST_EXISTS_EXPRESSION:
        if (context != NULL && context->eval_subquery != NULL) {
            return context->eval_subquery(context->user_data, node, warnings, out_value);
        }
        return -1;
    case MYLITE_SQL_AST_QUANTIFIED_COMPARISON:
        return eval_quantified_comparison(node, context, warnings, out_value);
    case MYLITE_SQL_AST_FUNCTION_CALL:
        return eval_function_call(node, context, warnings, out_value);
    default:
        return -1;
    }
}

static int eval_unary(const struct mylite_sql_ast_node *node,
                      const struct mylite_expression_eval_context *context,
                      struct mylite_expression_warnings *warnings,
                      struct mylite_expression_value *out_value)
{
    struct mylite_expression_value operand = {0};
    int status = 0;

    switch (node->operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN:
        return eval_is_expression(node->operator_kind, child_at(node, 0U), context, warnings,
                                  out_value);
    default:
        break;
    }

    status = eval_node(child_at(node, 0U), context, warnings, &operand);
    if (status != 0) {
        return status;
    }
    status = eval_numeric_unary(node->operator_kind, &operand, warnings, out_value);
    mylite_expression_value_deinit(&operand);
    return status;
}

static int eval_binary(const struct mylite_sql_ast_node *node,
                       const struct mylite_expression_eval_context *context,
                       struct mylite_expression_warnings *warnings,
                       struct mylite_expression_value *out_value)
{
    struct mylite_expression_value left = {0};
    struct mylite_expression_value right = {0};
    int status = 0;

    if (binary_expression_is_row_subquery(node)) {
        return context == NULL || context->eval_row_subquery == NULL
                   ? -1
                   : context->eval_row_subquery(context->user_data, node, context, warnings,
                                                out_value);
    }
    if (node->operator_kind == MYLITE_SQL_AST_OPERATOR_IN ||
        node->operator_kind == MYLITE_SQL_AST_OPERATOR_NOT_IN) {
        return eval_in(node->operator_kind, node, context, warnings, out_value);
    }
    if (node->operator_kind == MYLITE_SQL_AST_OPERATOR_LIKE ||
        node->operator_kind == MYLITE_SQL_AST_OPERATOR_NOT_LIKE) {
        return eval_like(node->operator_kind, node, context, warnings, out_value);
    }
    if (node->operator_kind == MYLITE_SQL_AST_OPERATOR_LOGICAL_AND) {
        return eval_logical_and(node, context, warnings, out_value);
    }
    if (node->operator_kind == MYLITE_SQL_AST_OPERATOR_LOGICAL_OR) {
        return eval_logical_or(node, context, warnings, out_value);
    }

    status = eval_node(child_at(node, 0U), context, warnings, &left);
    if (status == 0) {
        status = eval_node(child_at(node, 1U), context, warnings, &right);
    }
    if (status != 0) {
        goto cleanup;
    }

    switch (node->operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_ADD:
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
    case MYLITE_SQL_AST_OPERATOR_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_MODULO:
        status = eval_arithmetic(node->operator_kind, &left, &right, warnings, out_value);
        break;
    case MYLITE_SQL_AST_OPERATOR_BITWISE_AND:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_XOR:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_OR:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_LEFT:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_RIGHT:
        status = eval_bitwise(node->operator_kind, &left, &right, warnings, out_value);
        break;
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LESS:
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_GREATER:
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
        status = eval_comparison(node->operator_kind, &left, &right, warnings, out_value);
        break;
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
        status = eval_logical(node->operator_kind, &left, &right, warnings, out_value);
        break;
    default:
        status = -1;
        break;
    }

cleanup:
    mylite_expression_value_deinit(&left);
    mylite_expression_value_deinit(&right);
    return status;
}

static int eval_logical_and(const struct mylite_sql_ast_node *node,
                            const struct mylite_expression_eval_context *context,
                            struct mylite_expression_warnings *warnings,
                            struct mylite_expression_value *out_value)
{
    struct mylite_expression_value left = {0};
    struct mylite_expression_value right = {0};
    int left_truth = -1;
    int right_truth = -1;
    int status = 0;

    if (context != NULL && !mylite_expression_is_supported_no_table(child_at(node, 0U)) &&
        expression_is_constant_boolean(child_at(node, 1U), false)) {
        status = validate_boolean_shortcut_operand(child_at(node, 0U), context, warnings);
        if (status != 0) {
            return status;
        }
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = 0};
        return 0;
    }

    status = eval_node(child_at(node, 0U), context, warnings, &left);
    if (status == 0) {
        status = truth_value(&left, warnings, &left_truth);
    }
    if (status != 0 || left_truth == 0) {
        mylite_expression_value_deinit(&left);
        if (status == 0) {
            status = validate_boolean_shortcut_operand(child_at(node, 1U), context, warnings);
        }
        if (status == 0) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                          .int64_value = 0};
        }
        return status;
    }

    status = eval_node(child_at(node, 1U), context, warnings, &right);
    if (status == 0) {
        status = truth_value(&right, warnings, &right_truth);
    }
    if (status == 0) {
        if (right_truth == 0) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                          .int64_value = 0};
        } else if (left_truth < 0 || right_truth < 0) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        } else {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                          .int64_value = 1};
        }
    }

    mylite_expression_value_deinit(&left);
    mylite_expression_value_deinit(&right);
    return status;
}

static int eval_logical_or(const struct mylite_sql_ast_node *node,
                           const struct mylite_expression_eval_context *context,
                           struct mylite_expression_warnings *warnings,
                           struct mylite_expression_value *out_value)
{
    struct mylite_expression_value left = {0};
    struct mylite_expression_value right = {0};
    int left_truth = -1;
    int right_truth = -1;
    int status = 0;

    if (context != NULL && !mylite_expression_is_supported_no_table(child_at(node, 0U)) &&
        expression_is_constant_boolean(child_at(node, 1U), true)) {
        status = validate_boolean_shortcut_operand(child_at(node, 0U), context, warnings);
        if (status != 0) {
            return status;
        }
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = 1};
        return 0;
    }

    status = eval_node(child_at(node, 0U), context, warnings, &left);
    if (status == 0) {
        status = truth_value(&left, warnings, &left_truth);
    }
    if (status != 0 || left_truth == 1) {
        mylite_expression_value_deinit(&left);
        if (status == 0) {
            status = validate_boolean_shortcut_operand(child_at(node, 1U), context, warnings);
        }
        if (status == 0) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                          .int64_value = 1};
        }
        return status;
    }

    status = eval_node(child_at(node, 1U), context, warnings, &right);
    if (status == 0) {
        status = truth_value(&right, warnings, &right_truth);
    }
    if (status == 0) {
        if (right_truth == 1) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                          .int64_value = 1};
        } else if (left_truth < 0 || right_truth < 0) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        } else {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                          .int64_value = 0};
        }
    }

    mylite_expression_value_deinit(&left);
    mylite_expression_value_deinit(&right);
    return status;
}

static int validate_boolean_shortcut_operand(const struct mylite_sql_ast_node *node,
                                             const struct mylite_expression_eval_context *context,
                                             struct mylite_expression_warnings *warnings)
{
    int status = 0;

    if (node == NULL) {
        return 0;
    }
    while (node->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
        node = child_at(node, 0U);
        if (node == NULL) {
            return 0;
        }
    }
    if ((node->kind == MYLITE_SQL_AST_BINARY_EXPRESSION ||
         node->kind == MYLITE_SQL_AST_TERNARY_EXPRESSION) &&
        (node->operator_kind == MYLITE_SQL_AST_OPERATOR_LIKE ||
         node->operator_kind == MYLITE_SQL_AST_OPERATOR_NOT_LIKE)) {
        status = validate_like_escape_before_shortcut(node, context, warnings);
        if (status != 0) {
            return status;
        }
    }

    for (const struct mylite_sql_ast_node *child = node->first_child; child != NULL;
         child = child->next_sibling) {
        status = validate_boolean_shortcut_operand(child, context, warnings);
        if (status != 0) {
            return status;
        }
    }
    return 0;
}

static int
validate_like_escape_before_shortcut(const struct mylite_sql_ast_node *node,
                                     const struct mylite_expression_eval_context *context,
                                     struct mylite_expression_warnings *warnings)
{
    const struct mylite_sql_ast_node *escape = child_at(node, 2U);
    struct mylite_expression_value escape_value = {0};
    char *escape_text = NULL;
    int status = 0;

    if (escape == NULL) {
        return 0;
    }
    status = eval_node(escape, context, warnings, &escape_value);
    if (status != 0 || is_null(&escape_value)) {
        mylite_expression_value_deinit(&escape_value);
        return status;
    }

    status = value_to_string(&escape_value, &escape_text);
    if (status == 0 && strlen(escape_text) != 1U) {
        status = append_warning(warnings, MYLITE_WARNING_INCORRECT_ESCAPE_ARGUMENTS,
                                "Incorrect arguments to ESCAPE");
        if (status == 0) {
            status = -1;
        }
    }

    free(escape_text);
    mylite_expression_value_deinit(&escape_value);
    return status;
}

static bool expression_is_constant_boolean(const struct mylite_sql_ast_node *node,
                                           bool expected_value)
{
    while (node != NULL && node->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
        node = child_at(node, 0U);
    }
    if (node == NULL || node->kind != MYLITE_SQL_AST_LITERAL) {
        return false;
    }
    if (node->literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        return expected_value;
    }
    if (node->literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        return !expected_value;
    }
    if (node->literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER) {
        char *text = copy_span_text(node->span.text, node->span.length);
        char *end = NULL;
        long long value = 0;
        bool matches = false;

        if (text == NULL) {
            return false;
        }
        errno = 0;
        value = strtoll(text, &end, MYLITE_EXPRESSION_DECIMAL_BASE);
        if (errno == 0 && end != text && *end == '\0') {
            matches = (value != 0) == expected_value;
        }
        free(text);
        return matches;
    }
    return false;
}

static int eval_ternary(const struct mylite_sql_ast_node *node,
                        const struct mylite_expression_eval_context *context,
                        struct mylite_expression_warnings *warnings,
                        struct mylite_expression_value *out_value)
{
    if (node->operator_kind == MYLITE_SQL_AST_OPERATOR_BETWEEN ||
        node->operator_kind == MYLITE_SQL_AST_OPERATOR_NOT_BETWEEN) {
        return eval_between(node->operator_kind, node, context, warnings, out_value);
    }
    if (node->operator_kind == MYLITE_SQL_AST_OPERATOR_LIKE ||
        node->operator_kind == MYLITE_SQL_AST_OPERATOR_NOT_LIKE) {
        return eval_like(node->operator_kind, node, context, warnings, out_value);
    }
    return -1;
}

static int eval_case_expression(const struct mylite_sql_ast_node *node,
                                const struct mylite_expression_eval_context *context,
                                struct mylite_expression_warnings *warnings,
                                struct mylite_expression_value *out_value)
{
    if (node->case_expression_simple) {
        return eval_simple_case_expression(node, context, warnings, out_value);
    }
    return eval_searched_case_expression(node, context, warnings, out_value);
}

static int eval_simple_case_expression(const struct mylite_sql_ast_node *node,
                                       const struct mylite_expression_eval_context *context,
                                       struct mylite_expression_warnings *warnings,
                                       struct mylite_expression_value *out_value)
{
    const struct mylite_sql_ast_node *base_expression = child_at(node, 0U);
    const struct mylite_sql_ast_node *when_list = child_at(node, 1U);
    const struct mylite_sql_ast_node *else_expression = child_at(node, 2U);
    struct mylite_expression_value base = {0};
    int status = eval_node(base_expression, context, warnings, &base);

    if (status != 0 || when_list == NULL || when_list->kind != MYLITE_SQL_AST_CASE_WHEN_LIST) {
        mylite_expression_value_deinit(&base);
        return status == 0 ? -1 : status;
    }

    for (const struct mylite_sql_ast_node *arm = when_list->first_child; arm != NULL;
         arm = arm->next_sibling) {
        const struct mylite_sql_ast_node *compare_expression = child_at(arm, 0U);
        const struct mylite_sql_ast_node *result_expression = child_at(arm, 1U);
        struct mylite_expression_value compare = {0};
        int comparison = 0;
        bool matches = false;

        status = eval_node(compare_expression, context, warnings, &compare);
        if (status == 0 && !is_null(&base) && !is_null(&compare)) {
            status = compare_values(&base, &compare, warnings, &comparison);
            matches = status == 0 && comparison == 0;
        }
        mylite_expression_value_deinit(&compare);
        if (status != 0) {
            break;
        }
        if (matches) {
            status = eval_node(result_expression, context, warnings, out_value);
            mylite_expression_value_deinit(&base);
            return status;
        }
    }

    mylite_expression_value_deinit(&base);
    if (status != 0) {
        return status;
    }
    return eval_case_default(else_expression, context, warnings, out_value);
}

static int eval_searched_case_expression(const struct mylite_sql_ast_node *node,
                                         const struct mylite_expression_eval_context *context,
                                         struct mylite_expression_warnings *warnings,
                                         struct mylite_expression_value *out_value)
{
    const struct mylite_sql_ast_node *when_list = child_at(node, 0U);
    const struct mylite_sql_ast_node *else_expression = child_at(node, 1U);

    if (when_list == NULL || when_list->kind != MYLITE_SQL_AST_CASE_WHEN_LIST) {
        return -1;
    }

    for (const struct mylite_sql_ast_node *arm = when_list->first_child; arm != NULL;
         arm = arm->next_sibling) {
        const struct mylite_sql_ast_node *condition_expression = child_at(arm, 0U);
        const struct mylite_sql_ast_node *result_expression = child_at(arm, 1U);
        struct mylite_expression_value condition = {0};
        int truth = -1;
        int status = eval_node(condition_expression, context, warnings, &condition);

        if (status == 0) {
            status = truth_value(&condition, warnings, &truth);
        }
        mylite_expression_value_deinit(&condition);
        if (status != 0) {
            return status;
        }
        if (truth == 1) {
            return eval_node(result_expression, context, warnings, out_value);
        }
    }

    return eval_case_default(else_expression, context, warnings, out_value);
}

static int eval_case_default(const struct mylite_sql_ast_node *expression,
                             const struct mylite_expression_eval_context *context,
                             struct mylite_expression_warnings *warnings,
                             struct mylite_expression_value *out_value)
{
    if (expression == NULL) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        return 0;
    }
    return eval_node(expression, context, warnings, out_value);
}

static int eval_cast_expression(const struct mylite_sql_ast_node *node,
                                const struct mylite_expression_eval_context *context,
                                struct mylite_expression_warnings *warnings,
                                struct mylite_expression_value *out_value)
{
    const struct mylite_sql_ast_node *source = child_at(node, 0U);
    const struct mylite_sql_ast_node *target = child_at(node, 1U);
    struct mylite_expression_value value = {0};
    int status = eval_node(source, context, warnings, &value);

    if (status != 0) {
        return status;
    }
    if (is_null(&value)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        mylite_expression_value_deinit(&value);
        return 0;
    }
    if (target == NULL || target->kind != MYLITE_SQL_AST_COLUMN_TYPE) {
        mylite_expression_value_deinit(&value);
        return -1;
    }

    switch (target->column_type) {
    case MYLITE_SQL_AST_COLUMN_TYPE_BIGINT:
        if (target->column_type_unsigned) {
            status = eval_unsigned_cast(&value, warnings, out_value);
        } else {
            status = eval_signed_cast(&value, warnings, out_value);
        }
        break;
    case MYLITE_SQL_AST_COLUMN_TYPE_DECIMAL:
        status = eval_decimal_cast(target, &value, warnings, out_value);
        break;
    case MYLITE_SQL_AST_COLUMN_TYPE_CHAR:
        status = eval_char_cast(target, &value, warnings, out_value);
        break;
    case MYLITE_SQL_AST_COLUMN_TYPE_BINARY:
        status = eval_binary_cast(&value, out_value);
        break;
    case MYLITE_SQL_AST_COLUMN_TYPE_NONE:
    case MYLITE_SQL_AST_COLUMN_TYPE_TINYINT:
    case MYLITE_SQL_AST_COLUMN_TYPE_SMALLINT:
    case MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMINT:
    case MYLITE_SQL_AST_COLUMN_TYPE_INT:
    case MYLITE_SQL_AST_COLUMN_TYPE_BOOL:
    case MYLITE_SQL_AST_COLUMN_TYPE_BOOLEAN:
    case MYLITE_SQL_AST_COLUMN_TYPE_VARCHAR:
    case MYLITE_SQL_AST_COLUMN_TYPE_TINYTEXT:
    case MYLITE_SQL_AST_COLUMN_TYPE_TEXT:
    case MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMTEXT:
    case MYLITE_SQL_AST_COLUMN_TYPE_LONGTEXT:
    case MYLITE_SQL_AST_COLUMN_TYPE_VARBINARY:
    case MYLITE_SQL_AST_COLUMN_TYPE_TINYBLOB:
    case MYLITE_SQL_AST_COLUMN_TYPE_BLOB:
    case MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMBLOB:
    case MYLITE_SQL_AST_COLUMN_TYPE_LONGBLOB:
    case MYLITE_SQL_AST_COLUMN_TYPE_FLOAT:
    case MYLITE_SQL_AST_COLUMN_TYPE_DOUBLE:
    case MYLITE_SQL_AST_COLUMN_TYPE_DATE:
    case MYLITE_SQL_AST_COLUMN_TYPE_TIME:
    case MYLITE_SQL_AST_COLUMN_TYPE_DATETIME:
    case MYLITE_SQL_AST_COLUMN_TYPE_TIMESTAMP:
    case MYLITE_SQL_AST_COLUMN_TYPE_YEAR:
        status = -1;
        break;
    }

    mylite_expression_value_deinit(&value);
    return status;
}

static int eval_signed_cast(const struct mylite_expression_value *value,
                            struct mylite_expression_warnings *warnings,
                            struct mylite_expression_value *out_value)
{
    int64_t integer = 0;
    int status = cast_value_to_signed_integer(value, warnings, &integer);

    if (status != 0) {
        return status;
    }
    *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                  .int64_value = integer};
    return 0;
}

static int eval_unsigned_cast(const struct mylite_expression_value *value,
                              struct mylite_expression_warnings *warnings,
                              struct mylite_expression_value *out_value)
{
    uint64_t integer = 0;
    int status = cast_value_to_unsigned_integer(value, warnings, &integer);

    if (status != 0) {
        return status;
    }
    *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_UINT64,
                                                  .uint64_value = integer};
    return 0;
}

static int eval_decimal_cast(const struct mylite_sql_ast_node *target,
                             const struct mylite_expression_value *value,
                             struct mylite_expression_warnings *warnings,
                             struct mylite_expression_value *out_value)
{
    char buffer[MYLITE_EXPRESSION_DECIMAL_TEXT_BUFFER_SIZE];
    unsigned int scale = cast_decimal_scale(target);
    double number = 0.0;
    int length = 0;
    int status = cast_value_to_decimal_double(value, warnings, &number);

    if (status != 0) {
        return status;
    }
    length = snprintf(buffer, sizeof(buffer), "%.*f", (int)scale, number);
    if (length < 0 || (size_t)length >= sizeof(buffer)) {
        return -1;
    }
    return set_text_value(buffer, (size_t)length, out_value);
}

static int eval_char_cast(const struct mylite_sql_ast_node *target,
                          const struct mylite_expression_value *value,
                          struct mylite_expression_warnings *warnings,
                          struct mylite_expression_value *out_value)
{
    char *text = NULL;
    int status = cast_value_to_string(value, &text);

    if (status != 0) {
        return status;
    }
    if (target->has_column_length) {
        int64_t character_count = 0;

        status = utf8_char_count(text, &character_count);
        if (status == 0 && character_count > (int64_t)target->column_length) {
            size_t offset = utf8_offset_for_chars(text, (int64_t)target->column_length);

            status = append_char_truncation_warning(warnings, target->column_length, text);
            if (status == 0) {
                text[offset] = '\0';
            }
        }
    }
    if (status == 0) {
        status = set_text_value(text, strlen(text), out_value);
    }
    free(text);
    return status;
}

static int eval_binary_cast(const struct mylite_expression_value *value,
                            struct mylite_expression_value *out_value)
{
    char *text = NULL;
    int status = cast_value_to_string(value, &text);

    if (status == 0) {
        status = set_text_value(text, strlen(text), out_value);
    }
    free(text);
    return status;
}

static int eval_function_call(const struct mylite_sql_ast_node *node,
                              const struct mylite_expression_eval_context *context,
                              struct mylite_expression_warnings *warnings,
                              struct mylite_expression_value *out_value)
{
    const struct mylite_sql_ast_node *arguments = child_at(node, 1U);
    enum mylite_scalar_function_id function_id = scalar_function_id(node);

    if (function_id == MYLITE_SCALAR_FUNCTION_UNKNOWN ||
        !mylite_expression_is_supported_function_call(node)) {
        return -1;
    }
    switch (function_id) {
    case MYLITE_SCALAR_FUNCTION_CONCAT:
        return eval_concat_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_LENGTH:
    case MYLITE_SCALAR_FUNCTION_CHAR_LENGTH:
    case MYLITE_SCALAR_FUNCTION_LOWER:
    case MYLITE_SCALAR_FUNCTION_UPPER:
        return eval_unary_string_function(function_id, arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_LEFT:
    case MYLITE_SCALAR_FUNCTION_RIGHT:
        return eval_left_right_function(function_id, arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_REPLACE:
        return eval_replace_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_MOD:
        return eval_mod_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_ABS:
    case MYLITE_SCALAR_FUNCTION_SIGN:
    case MYLITE_SCALAR_FUNCTION_FLOOR:
    case MYLITE_SCALAR_FUNCTION_CEIL:
        return eval_numeric_unary_function(function_id, arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_PI:
        return set_text_value(mylite_pi_text, strlen(mylite_pi_text), out_value);
    case MYLITE_SCALAR_FUNCTION_IF:
        return eval_if_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_IFNULL:
        return eval_ifnull_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_NULLIF:
        return eval_nullif_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_COALESCE:
        return eval_coalesce_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_ISNULL:
        return eval_isnull_function(arguments, context, warnings, out_value);
    case MYLITE_SCALAR_FUNCTION_DATABASE:
    case MYLITE_SCALAR_FUNCTION_SCHEMA:
    case MYLITE_SCALAR_FUNCTION_VERSION:
    case MYLITE_SCALAR_FUNCTION_LAST_INSERT_ID:
    case MYLITE_SCALAR_FUNCTION_ROW_COUNT:
        return context == NULL || context->eval_session_function == NULL
                   ? -1
                   : context->eval_session_function(context->user_data, node, context, warnings,
                                                    out_value);
    case MYLITE_SCALAR_FUNCTION_UNKNOWN:
        break;
    }
    return -1;
}

static int eval_concat_function(const struct mylite_sql_ast_node *arguments,
                                const struct mylite_expression_eval_context *context,
                                struct mylite_expression_warnings *warnings,
                                struct mylite_expression_value *out_value)
{
    char *result = copy_span_text("", 0U);
    size_t result_length = 0U;

    if (result == NULL) {
        return -1;
    }
    for (const struct mylite_sql_ast_node *argument = arguments->first_child; argument != NULL;
         argument = argument->next_sibling) {
        struct mylite_expression_value value = {0};
        char *text = NULL;
        int status = eval_node(argument, context, warnings, &value);

        if (status == 0 && is_null(&value)) {
            free(result);
            mylite_expression_value_deinit(&value);
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
            return 0;
        }
        if (status == 0) {
            status = value_to_string(&value, &text);
        }
        if (status == 0) {
            status = append_text(&result, &result_length, text, strlen(text));
        }
        free(text);
        mylite_expression_value_deinit(&value);
        if (status != 0) {
            free(result);
            return status;
        }
    }
    out_value->kind = MYLITE_EXPRESSION_VALUE_TEXT;
    out_value->text_value = result;
    return 0;
}

static int eval_unary_string_function(enum mylite_scalar_function_id function_id,
                                      const struct mylite_sql_ast_node *arguments,
                                      const struct mylite_expression_eval_context *context,
                                      struct mylite_expression_warnings *warnings,
                                      struct mylite_expression_value *out_value)
{
    struct mylite_expression_value value = {0};
    char *text = NULL;
    int status = eval_node(arguments->first_child, context, warnings, &value);

    if (status != 0 || is_null(&value)) {
        mylite_expression_value_deinit(&value);
        if (status == 0) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        }
        return status;
    }
    status = value_to_string(&value, &text);
    mylite_expression_value_deinit(&value);
    if (status != 0) {
        return status;
    }

    switch (function_id) {
    case MYLITE_SCALAR_FUNCTION_LENGTH:
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = (int64_t)strlen(text)};
        free(text);
        return 0;
    case MYLITE_SCALAR_FUNCTION_CHAR_LENGTH: {
        int64_t count = 0;

        status = utf8_char_count(text, &count);
        free(text);
        if (status == 0) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                          .int64_value = count};
        }
        return status;
    }
    case MYLITE_SCALAR_FUNCTION_LOWER:
    case MYLITE_SCALAR_FUNCTION_UPPER: {
        bool lower = function_id == MYLITE_SCALAR_FUNCTION_LOWER;

        for (char *cursor = text; *cursor != '\0'; ++cursor) {
            unsigned char character = (unsigned char)*cursor;

            if (character >= 'A' && character <= 'Z' && lower) {
                *cursor = (char)(character - 'A' + 'a');
            } else if (character >= 'a' && character <= 'z' && !lower) {
                *cursor = (char)(character - 'a' + 'A');
            }
        }
        out_value->kind = MYLITE_EXPRESSION_VALUE_TEXT;
        out_value->text_value = text;
        return 0;
    }
    default:
        free(text);
        return -1;
    }
}

static int eval_left_right_function(enum mylite_scalar_function_id function_id,
                                    const struct mylite_sql_ast_node *arguments,
                                    const struct mylite_expression_eval_context *context,
                                    struct mylite_expression_warnings *warnings,
                                    struct mylite_expression_value *out_value)
{
    struct mylite_expression_value value = {0};
    struct mylite_expression_value count = {0};
    char *text = NULL;
    int64_t char_count = 0;
    int64_t requested = 0;
    int status = eval_node(child_at(arguments, 0U), context, warnings, &value);

    if (status == 0) {
        status = eval_node(child_at(arguments, 1U), context, warnings, &count);
    }
    if (status != 0 || is_null(&value) || is_null(&count)) {
        mylite_expression_value_deinit(&value);
        mylite_expression_value_deinit(&count);
        if (status == 0) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        }
        return status;
    }
    status = value_to_string(&value, &text);
    if (status == 0) {
        status = utf8_char_count(text, &char_count);
    }
    if (status == 0) {
        requested = mylite_expression_value_to_int64(&count);
        if (requested <= 0) {
            status = set_text_value("", 0U, out_value);
        } else if (function_id == MYLITE_SCALAR_FUNCTION_LEFT) {
            status = set_text_value(text, utf8_offset_for_chars(text, requested), out_value);
        } else {
            int64_t skip = requested >= char_count ? 0 : char_count - requested;
            size_t offset = utf8_offset_for_chars(text, skip);

            status = set_text_value(text + offset, strlen(text + offset), out_value);
        }
    }
    free(text);
    mylite_expression_value_deinit(&value);
    mylite_expression_value_deinit(&count);
    return status;
}

static int eval_replace_function(const struct mylite_sql_ast_node *arguments,
                                 const struct mylite_expression_eval_context *context,
                                 struct mylite_expression_warnings *warnings,
                                 struct mylite_expression_value *out_value)
{
    struct mylite_expression_value values[3] = {{0}, {0}, {0}};
    char *text = NULL;
    char *from = NULL;
    char *replacement = NULL;
    char *result = NULL;
    size_t result_length = 0U;
    int status = 0;

    for (size_t index = 0U; index < 3U; ++index) {
        status = eval_node(child_at(arguments, index), context, warnings, &values[index]);
        if (status != 0) {
            goto cleanup;
        }
        if (is_null(&values[index])) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
            goto cleanup;
        }
    }
    status = value_to_string(&values[0], &text);
    if (status == 0) {
        status = value_to_string(&values[1], &from);
    }
    if (status == 0) {
        status = value_to_string(&values[2], &replacement);
    }
    if (status != 0) {
        goto cleanup;
    }
    result = copy_span_text("", 0U);
    if (result == NULL) {
        status = -1;
        goto cleanup;
    }
    for (const char *cursor = text; *cursor != '\0';) {
        size_t from_length = strlen(from);

        if (from_length != 0U && strncmp(cursor, from, from_length) == 0) {
            status = append_text(&result, &result_length, replacement, strlen(replacement));
            cursor += from_length;
        } else {
            status = append_text(&result, &result_length, cursor, 1U);
            ++cursor;
        }
        if (status != 0) {
            break;
        }
    }
    if (status == 0) {
        out_value->kind = MYLITE_EXPRESSION_VALUE_TEXT;
        out_value->text_value = result;
        result = NULL;
    }

cleanup:
    free(text);
    free(from);
    free(replacement);
    free(result);
    for (size_t index = 0U; index < 3U; ++index) {
        mylite_expression_value_deinit(&values[index]);
    }
    return status;
}

static int eval_mod_function(const struct mylite_sql_ast_node *arguments,
                             const struct mylite_expression_eval_context *context,
                             struct mylite_expression_warnings *warnings,
                             struct mylite_expression_value *out_value)
{
    struct mylite_expression_value left = {0};
    struct mylite_expression_value right = {0};
    int status = eval_node(child_at(arguments, 0U), context, warnings, &left);

    if (status == 0) {
        status = eval_node(child_at(arguments, 1U), context, warnings, &right);
    }
    if (status == 0) {
        status =
            eval_arithmetic(MYLITE_SQL_AST_OPERATOR_MODULO, &left, &right, warnings, out_value);
    }
    mylite_expression_value_deinit(&left);
    mylite_expression_value_deinit(&right);
    return status;
}

static int eval_numeric_unary_function(enum mylite_scalar_function_id function_id,
                                       const struct mylite_sql_ast_node *arguments,
                                       const struct mylite_expression_eval_context *context,
                                       struct mylite_expression_warnings *warnings,
                                       struct mylite_expression_value *out_value)
{
    struct mylite_expression_value value = {0};
    struct numeric_value number = {0};
    int status = eval_node(arguments->first_child, context, warnings, &value);

    if (status != 0 || is_null(&value)) {
        mylite_expression_value_deinit(&value);
        if (status == 0) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        }
        return status;
    }
    status = value_to_numeric(&value, warnings, &number);
    mylite_expression_value_deinit(&value);
    if (status != 0) {
        return status;
    }

    switch (function_id) {
    case MYLITE_SCALAR_FUNCTION_ABS:
        eval_abs_function(&number, out_value);
        return 0;
    case MYLITE_SCALAR_FUNCTION_SIGN:
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = (number.real_value > 0.0) -
                                                                     (number.real_value < 0.0)};
        return 0;
    case MYLITE_SCALAR_FUNCTION_FLOOR:
        *out_value =
            (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                             .int64_value = floor_real_value(number.real_value)};
        return 0;
    case MYLITE_SCALAR_FUNCTION_CEIL:
        *out_value =
            (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                             .int64_value = ceil_real_value(number.real_value)};
        return 0;
    default:
        return -1;
    }
}

static void eval_abs_function(const struct numeric_value *number,
                              struct mylite_expression_value *out_value)
{
    if (number->is_integer && !number->is_unsigned && number->int64_value >= 0) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = number->int64_value};
    } else if (number->is_integer && !number->is_unsigned && number->int64_value < 0 &&
               number->int64_value != INT64_MIN) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = -number->int64_value};
    } else if (number->is_integer && number->is_unsigned) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_UINT64,
                                                      .uint64_value = number->uint64_value};
    } else {
        *out_value =
            (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_REAL,
                                             .real_value = absolute_real_value(number->real_value)};
    }
}

static int eval_if_function(const struct mylite_sql_ast_node *arguments,
                            const struct mylite_expression_eval_context *context,
                            struct mylite_expression_warnings *warnings,
                            struct mylite_expression_value *out_value)
{
    struct mylite_expression_value condition = {0};
    size_t branch_index = 2U;
    int truth = -1;
    int status = eval_node(child_at(arguments, 0U), context, warnings, &condition);

    if (status == 0) {
        status = truth_value(&condition, warnings, &truth);
    }
    mylite_expression_value_deinit(&condition);
    if (status != 0) {
        return status;
    }
    if (truth == 1) {
        branch_index = 1U;
    }
    return eval_node(child_at(arguments, branch_index), context, warnings, out_value);
}

static int eval_ifnull_function(const struct mylite_sql_ast_node *arguments,
                                const struct mylite_expression_eval_context *context,
                                struct mylite_expression_warnings *warnings,
                                struct mylite_expression_value *out_value)
{
    struct mylite_expression_value value = {0};
    int status = eval_node(child_at(arguments, 0U), context, warnings, &value);

    if (status != 0) {
        mylite_expression_value_deinit(&value);
        return status;
    }
    if (is_null(&value)) {
        mylite_expression_value_deinit(&value);
        return eval_node(child_at(arguments, 1U), context, warnings, out_value);
    }
    *out_value = value;
    return 0;
}

static int eval_nullif_function(const struct mylite_sql_ast_node *arguments,
                                const struct mylite_expression_eval_context *context,
                                struct mylite_expression_warnings *warnings,
                                struct mylite_expression_value *out_value)
{
    struct mylite_expression_value left = {0};
    struct mylite_expression_value right = {0};
    bool moved_left = false;
    int comparison = 0;
    int status = eval_node(child_at(arguments, 0U), context, warnings, &left);

    if (status == 0) {
        status = eval_node(child_at(arguments, 1U), context, warnings, &right);
    }
    if (status == 0 && !is_null(&left) && !is_null(&right)) {
        status = compare_values(&left, &right, warnings, &comparison);
    }
    if (status == 0 && comparison == 0 && !is_null(&left) && !is_null(&right)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
    } else if (status == 0) {
        *out_value = left;
        moved_left = true;
    }
    if (!moved_left) {
        mylite_expression_value_deinit(&left);
    }
    mylite_expression_value_deinit(&right);
    return status;
}

static int eval_coalesce_function(const struct mylite_sql_ast_node *arguments,
                                  const struct mylite_expression_eval_context *context,
                                  struct mylite_expression_warnings *warnings,
                                  struct mylite_expression_value *out_value)
{
    for (const struct mylite_sql_ast_node *argument = arguments->first_child; argument != NULL;
         argument = argument->next_sibling) {
        struct mylite_expression_value value = {0};
        int status = eval_node(argument, context, warnings, &value);

        if (status != 0) {
            mylite_expression_value_deinit(&value);
            return status;
        }
        if (!is_null(&value)) {
            *out_value = value;
            return 0;
        }
        mylite_expression_value_deinit(&value);
    }
    *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
    return 0;
}

static int eval_isnull_function(const struct mylite_sql_ast_node *arguments,
                                const struct mylite_expression_eval_context *context,
                                struct mylite_expression_warnings *warnings,
                                struct mylite_expression_value *out_value)
{
    struct mylite_expression_value value = {0};
    int status = eval_node(arguments->first_child, context, warnings, &value);

    if (status == 0) {
        int null_result = is_null(&value) ? 1 : 0;

        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = null_result};
    }
    mylite_expression_value_deinit(&value);
    return status;
}

static int eval_literal(const struct mylite_sql_ast_node *node,
                        struct mylite_expression_value *out_value)
{
    char *text = NULL;
    char *end = NULL;
    errno = 0;

    switch (node->literal_kind) {
    case MYLITE_SQL_AST_LITERAL_NULL:
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        return 0;
    case MYLITE_SQL_AST_LITERAL_TRUE:
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = 1};
        return 0;
    case MYLITE_SQL_AST_LITERAL_FALSE:
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = 0};
        return 0;
    case MYLITE_SQL_AST_LITERAL_INTEGER:
        text = copy_span_text(node->span.text, node->span.length);
        if (text == NULL) {
            return -1;
        }
        if (text[0] != '-' && strlen(text) >= MYLITE_EXPRESSION_UINT64_DIGITS) {
            unsigned long long unsigned_value =
                strtoull(text, &end, MYLITE_EXPRESSION_DECIMAL_BASE);
            if (errno == 0 && end != text && *end == '\0' &&
                unsigned_value > (unsigned long long)INT64_MAX) {
                out_value->kind = MYLITE_EXPRESSION_VALUE_UINT64;
                out_value->uint64_value = (uint64_t)unsigned_value;
                free(text);
                return 0;
            }
        }
        out_value->kind = MYLITE_EXPRESSION_VALUE_INT64;
        out_value->int64_value = strtoll(text, NULL, MYLITE_EXPRESSION_DECIMAL_BASE);
        free(text);
        return 0;
    case MYLITE_SQL_AST_LITERAL_DECIMAL:
    case MYLITE_SQL_AST_LITERAL_FLOAT:
        text = copy_span_text(node->span.text, node->span.length);
        if (text == NULL) {
            return -1;
        }
        out_value->kind = MYLITE_EXPRESSION_VALUE_REAL;
        out_value->real_value = strtod(text, NULL);
        free(text);
        return 0;
    case MYLITE_SQL_AST_LITERAL_STRING:
    case MYLITE_SQL_AST_LITERAL_NATIONAL_STRING:
        out_value->kind = MYLITE_EXPRESSION_VALUE_TEXT;
        out_value->text_value = decode_string_literal(node);
        return out_value->text_value == NULL ? -1 : 0;
    case MYLITE_SQL_AST_LITERAL_HEX:
    case MYLITE_SQL_AST_LITERAL_BIT:
    case MYLITE_SQL_AST_LITERAL_NONE:
        return -1;
    }
    return -1;
}

static int eval_is_expression(enum mylite_sql_ast_operator operator_kind,
                              const struct mylite_sql_ast_node *operand,
                              const struct mylite_expression_eval_context *context,
                              struct mylite_expression_warnings *warnings,
                              struct mylite_expression_value *out_value)
{
    struct mylite_expression_value value = {0};
    int truth = -1;
    int result = 0;
    int status = eval_node(operand, context, warnings, &value);

    if (status != 0) {
        return status;
    }

    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN:
        result = value.kind == MYLITE_EXPRESSION_VALUE_NULL;
        break;
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN:
        result = value.kind != MYLITE_EXPRESSION_VALUE_NULL;
        break;
    case MYLITE_SQL_AST_OPERATOR_IS_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE:
        status = truth_value(&value, warnings, &truth);
        if (status != 0) {
            break;
        }
        result = operator_kind == MYLITE_SQL_AST_OPERATOR_IS_TRUE ||
                         operator_kind == MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE
                     ? truth == 1
                     : truth == 0;
        if (operator_kind == MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE ||
            operator_kind == MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE) {
            result = !result;
        }
        break;
    default:
        status = -1;
        break;
    }

    mylite_expression_value_deinit(&value);
    if (status != 0) {
        return status;
    }
    *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                  .int64_value = result ? 1 : 0};
    return 0;
}

static int eval_between(enum mylite_sql_ast_operator operator_kind,
                        const struct mylite_sql_ast_node *node,
                        const struct mylite_expression_eval_context *context,
                        struct mylite_expression_warnings *warnings,
                        struct mylite_expression_value *out_value)
{
    struct mylite_expression_value value = {0};
    struct mylite_expression_value low = {0};
    struct mylite_expression_value high = {0};
    struct between_truth truth = {.low = -1, .high = -1};
    int status = eval_node(child_at(node, 0U), context, warnings, &value);

    if (status == 0) {
        status = eval_node(child_at(node, 1U), context, warnings, &low);
    }
    if (status == 0) {
        status = eval_node(child_at(node, 2U), context, warnings, &high);
    }
    if (status != 0) {
        goto cleanup;
    }
    status = eval_between_bound_truth(&value, &low, true, warnings, &truth.low);
    if (status == 0) {
        status = eval_between_bound_truth(&value, &high, false, warnings, &truth.high);
    }
    if (status == 0) {
        set_between_result(operator_kind, truth, out_value);
    }

cleanup:
    mylite_expression_value_deinit(&value);
    mylite_expression_value_deinit(&low);
    mylite_expression_value_deinit(&high);
    return status;
}

static int eval_between_bound_truth(const struct mylite_expression_value *value,
                                    const struct mylite_expression_value *bound, bool lower_bound,
                                    struct mylite_expression_warnings *warnings, int *out_truth)
{
    int comparison = 0;
    int status = 0;

    *out_truth = -1;
    if (is_null(value) || is_null(bound)) {
        return 0;
    }

    status = compare_values(value, bound, warnings, &comparison);
    if (status != 0) {
        return status;
    }
    *out_truth = lower_bound ? comparison >= 0 : comparison <= 0;
    return 0;
}

static void set_between_result(enum mylite_sql_ast_operator operator_kind,
                               struct between_truth truth,
                               struct mylite_expression_value *out_value)
{
    bool between = operator_kind == MYLITE_SQL_AST_OPERATOR_BETWEEN;

    if (truth.low == 0 || truth.high == 0) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = between ? 0 : 1};
    } else if (truth.low < 0 || truth.high < 0) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
    } else {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = between ? 1 : 0};
    }
}

static int eval_like(enum mylite_sql_ast_operator operator_kind,
                     const struct mylite_sql_ast_node *node,
                     const struct mylite_expression_eval_context *context,
                     struct mylite_expression_warnings *warnings,
                     struct mylite_expression_value *out_value)
{
    struct mylite_expression_value value = {0};
    struct mylite_expression_value pattern = {0};
    struct mylite_expression_value escape_value = {0};
    char *value_text = NULL;
    char *pattern_text = NULL;
    char *escape_text = NULL;
    char escape = '\\';
    int status = eval_node(child_at(node, 0U), context, warnings, &value);

    if (status == 0) {
        status = eval_node(child_at(node, 1U), context, warnings, &pattern);
    }
    if (status == 0 && child_at(node, 2U) != NULL) {
        status = eval_node(child_at(node, 2U), context, warnings, &escape_value);
    }
    if (status != 0) {
        goto cleanup;
    }
    if (is_null(&value) || is_null(&pattern) ||
        (child_at(node, 2U) != NULL && is_null(&escape_value))) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }

    status = value_to_string(&value, &value_text);
    if (status == 0) {
        status = value_to_string(&pattern, &pattern_text);
    }
    if (status == 0 && child_at(node, 2U) != NULL) {
        status = value_to_string(&escape_value, &escape_text);
        if (status == 0 && strlen(escape_text) != 1U) {
            status = append_warning(warnings, MYLITE_WARNING_INCORRECT_ESCAPE_ARGUMENTS,
                                    "Incorrect arguments to ESCAPE");
            if (status == 0) {
                status = -1;
            }
        }
        if (status == 0) {
            escape = escape_text[0];
        }
    }
    if (status == 0) {
        bool result = like_match(value_text, pattern_text, escape);
        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NOT_LIKE) {
            result = !result;
        }
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                      .int64_value = result ? 1 : 0};
    }

cleanup:
    free(value_text);
    free(pattern_text);
    free(escape_text);
    mylite_expression_value_deinit(&value);
    mylite_expression_value_deinit(&pattern);
    mylite_expression_value_deinit(&escape_value);
    return status;
}

static int eval_in(enum mylite_sql_ast_operator operator_kind,
                   const struct mylite_sql_ast_node *node,
                   const struct mylite_expression_eval_context *context,
                   struct mylite_expression_warnings *warnings,
                   struct mylite_expression_value *out_value)
{
    struct mylite_expression_value value = {0};
    const struct mylite_sql_ast_node *list = child_at(node, 1U);
    bool saw_null = false;
    int status = eval_node(child_at(node, 0U), context, warnings, &value);

    if (status != 0) {
        return status;
    }
    if (list != NULL && list->kind == MYLITE_SQL_AST_SELECT_STATEMENT) {
        status =
            context == NULL || context->eval_in_subquery == NULL
                ? -1
                : context->eval_in_subquery(context->user_data, node, &value, warnings, out_value);
        mylite_expression_value_deinit(&value);
        return status;
    }
    if (is_null(&value)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        mylite_expression_value_deinit(&value);
        return 0;
    }

    for (const struct mylite_sql_ast_node *item = list == NULL ? NULL : list->first_child;
         item != NULL; item = item->next_sibling) {
        struct mylite_expression_value candidate = {0};
        int comparison = 0;

        status = eval_node(item, context, warnings, &candidate);
        if (status != 0) {
            mylite_expression_value_deinit(&candidate);
            break;
        }
        if (is_null(&candidate)) {
            saw_null = true;
            mylite_expression_value_deinit(&candidate);
            continue;
        }
        status = compare_values(&value, &candidate, warnings, &comparison);
        mylite_expression_value_deinit(&candidate);
        if (status != 0) {
            break;
        }
        if (comparison == 0) {
            bool result = operator_kind == MYLITE_SQL_AST_OPERATOR_IN;
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                          .int64_value = result ? 1 : 0};
            mylite_expression_value_deinit(&value);
            return 0;
        }
    }
    mylite_expression_value_deinit(&value);
    if (status != 0) {
        return status;
    }
    if (saw_null) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        return 0;
    }
    *out_value = (struct mylite_expression_value){
        .kind = MYLITE_EXPRESSION_VALUE_INT64,
        .int64_value = operator_kind == MYLITE_SQL_AST_OPERATOR_IN ? 0 : 1};
    return 0;
}

static bool binary_expression_is_row_subquery(const struct mylite_sql_ast_node *node)
{
    const struct mylite_sql_ast_node *left = unwrap_parenthesized_node(child_at(node, 0U));
    const struct mylite_sql_ast_node *right = unwrap_parenthesized_node(child_at(node, 1U));

    if (node == NULL || node->kind != MYLITE_SQL_AST_BINARY_EXPRESSION || left == NULL ||
        left->kind != MYLITE_SQL_AST_ROW_CONSTRUCTOR || right == NULL) {
        return false;
    }
    if ((node->operator_kind == MYLITE_SQL_AST_OPERATOR_IN ||
         node->operator_kind == MYLITE_SQL_AST_OPERATOR_NOT_IN) &&
        right->kind == MYLITE_SQL_AST_SELECT_STATEMENT) {
        return true;
    }
    return binary_expression_is_row_scalar_subquery(node);
}

static bool binary_expression_is_row_scalar_subquery(const struct mylite_sql_ast_node *node)
{
    const struct mylite_sql_ast_node *left = unwrap_parenthesized_node(child_at(node, 0U));
    const struct mylite_sql_ast_node *right = unwrap_parenthesized_node(child_at(node, 1U));

    if (node == NULL || node->kind != MYLITE_SQL_AST_BINARY_EXPRESSION || left == NULL ||
        left->kind != MYLITE_SQL_AST_ROW_CONSTRUCTOR || right == NULL ||
        right->kind != MYLITE_SQL_AST_SUBQUERY_EXPRESSION) {
        return false;
    }
    return row_subquery_comparison_operator_is_supported(node->operator_kind);
}

static bool
row_subquery_comparison_operator_is_supported(enum mylite_sql_ast_operator operator_kind)
{
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LESS:
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_GREATER:
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
        return true;
    case MYLITE_SQL_AST_OPERATOR_NONE:
    case MYLITE_SQL_AST_OPERATOR_ADD:
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
    case MYLITE_SQL_AST_OPERATOR_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_NOT:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_AND:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_XOR:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_OR:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_LEFT:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_RIGHT:
    case MYLITE_SQL_AST_OPERATOR_POSITIVE:
    case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
    case MYLITE_SQL_AST_OPERATOR_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_NOT_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_LIKE:
    case MYLITE_SQL_AST_OPERATOR_NOT_LIKE:
    case MYLITE_SQL_AST_OPERATOR_IN:
    case MYLITE_SQL_AST_OPERATOR_NOT_IN:
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_MODULO:
        break;
    }
    return false;
}

static int eval_quantified_comparison(const struct mylite_sql_ast_node *node,
                                      const struct mylite_expression_eval_context *context,
                                      struct mylite_expression_warnings *warnings,
                                      struct mylite_expression_value *out_value)
{
    struct mylite_expression_value value = {0};
    int status = 0;

    if (quantified_comparison_has_row_left(node)) {
        return context == NULL || context->eval_row_subquery == NULL
                   ? -1
                   : context->eval_row_subquery(context->user_data, node, context, warnings,
                                                out_value);
    }

    status = eval_node(child_at(node, 0U), context, warnings, &value);

    if (status != 0) {
        return status;
    }
    status = context == NULL || context->eval_quantified_subquery == NULL
                 ? -1
                 : context->eval_quantified_subquery(context->user_data, node, &value, warnings,
                                                     out_value);
    mylite_expression_value_deinit(&value);
    return status;
}

static bool quantified_comparison_has_row_left(const struct mylite_sql_ast_node *node)
{
    const struct mylite_sql_ast_node *left = unwrap_parenthesized_node(child_at(node, 0U));

    return node != NULL && node->kind == MYLITE_SQL_AST_QUANTIFIED_COMPARISON && left != NULL &&
           left->kind == MYLITE_SQL_AST_ROW_CONSTRUCTOR;
}

static int eval_numeric_unary(enum mylite_sql_ast_operator operator_kind,
                              const struct mylite_expression_value *operand,
                              struct mylite_expression_warnings *warnings,
                              struct mylite_expression_value *out_value)
{
    struct numeric_value number = {0};
    int truth = -1;
    int status = 0;

    if (is_null(operand)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        return 0;
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT) {
        status = truth_value(operand, warnings, &truth);
        if (status != 0) {
            return status;
        }
        if (truth < 0) {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        } else {
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                          .int64_value = truth == 0 ? 1 : 0};
        }
        return 0;
    }

    status = value_to_numeric(operand, warnings, &number);
    if (status != 0) {
        return status;
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_BITWISE_NOT) {
        uint64_t value = number.is_unsigned ? number.uint64_value : (uint64_t)number.int64_value;
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_UINT64,
                                                      .uint64_value = ~value};
        return 0;
    }
    if (number.is_integer && !number.is_unsigned) {
        *out_value = (struct mylite_expression_value){
            .kind = MYLITE_EXPRESSION_VALUE_INT64,
            .int64_value = operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE ? -number.int64_value
                                                                             : number.int64_value};
        return 0;
    }
    *out_value = (struct mylite_expression_value){
        .kind = MYLITE_EXPRESSION_VALUE_REAL,
        .real_value = operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE ? -number.real_value
                                                                        : number.real_value};
    return 0;
}

static int eval_arithmetic(enum mylite_sql_ast_operator operator_kind,
                           const struct mylite_expression_value *left,
                           const struct mylite_expression_value *right,
                           struct mylite_expression_warnings *warnings,
                           struct mylite_expression_value *out_value)
{
    struct numeric_value left_number = {0};
    struct numeric_value right_number = {0};
    int status = 0;

    if (is_null(left) || is_null(right)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        return 0;
    }
    status = value_to_numeric(left, warnings, &left_number);
    if (status == 0) {
        status = value_to_numeric(right, warnings, &right_number);
    }
    if (status != 0) {
        return status;
    }
    if ((operator_kind == MYLITE_SQL_AST_OPERATOR_DIVIDE ||
         operator_kind == MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE ||
         operator_kind == MYLITE_SQL_AST_OPERATOR_MODULO) &&
        right_number.real_value == 0.0) {
        status = append_warning(warnings, MYLITE_WARNING_DIVISION_BY_ZERO, "Division by 0");
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        return status;
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_DIVIDE) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_REAL,
                                                      .real_value = left_number.real_value /
                                                                    right_number.real_value};
        return 0;
    }

    if (left_number.is_integer && right_number.is_integer) {
        int64_t left_int =
            left_number.is_unsigned ? (int64_t)left_number.uint64_value : left_number.int64_value;
        int64_t right_int = right_number.is_unsigned ? (int64_t)right_number.uint64_value
                                                     : right_number.int64_value;
        out_value->kind = MYLITE_EXPRESSION_VALUE_INT64;
        switch (operator_kind) {
        case MYLITE_SQL_AST_OPERATOR_ADD:
            out_value->int64_value = left_int + right_int;
            return 0;
        case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
            out_value->int64_value = left_int - right_int;
            return 0;
        case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
            out_value->int64_value = left_int * right_int;
            return 0;
        case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
            out_value->int64_value = left_int / right_int;
            return 0;
        case MYLITE_SQL_AST_OPERATOR_MODULO:
            out_value->int64_value = left_int % right_int;
            return 0;
        default:
            break;
        }
    }

    out_value->kind = MYLITE_EXPRESSION_VALUE_REAL;
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_ADD:
        out_value->real_value = left_number.real_value + right_number.real_value;
        return 0;
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
        out_value->real_value = left_number.real_value - right_number.real_value;
        return 0;
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
        out_value->real_value = left_number.real_value * right_number.real_value;
        return 0;
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
        out_value->kind = MYLITE_EXPRESSION_VALUE_INT64;
        out_value->int64_value = (int64_t)(left_number.real_value / right_number.real_value);
        return 0;
    case MYLITE_SQL_AST_OPERATOR_MODULO:
        out_value->real_value =
            left_number.real_value -
            ((double)((int64_t)(left_number.real_value / right_number.real_value)) *
             right_number.real_value);
        return 0;
    default:
        break;
    }
    return -1;
}

static int eval_bitwise(enum mylite_sql_ast_operator operator_kind,
                        const struct mylite_expression_value *left,
                        const struct mylite_expression_value *right,
                        struct mylite_expression_warnings *warnings,
                        struct mylite_expression_value *out_value)
{
    struct numeric_value left_number = {0};
    struct numeric_value right_number = {0};
    uint64_t left_int = 0U;
    uint64_t right_int = 0U;
    int status = 0;

    if (is_null(left) || is_null(right)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        return 0;
    }
    status = value_to_numeric(left, warnings, &left_number);
    if (status == 0) {
        status = value_to_numeric(right, warnings, &right_number);
    }
    if (status != 0) {
        return status;
    }
    left_int =
        left_number.is_unsigned ? left_number.uint64_value : (uint64_t)left_number.int64_value;
    right_int =
        right_number.is_unsigned ? right_number.uint64_value : (uint64_t)right_number.int64_value;
    out_value->kind = MYLITE_EXPRESSION_VALUE_UINT64;
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_BITWISE_AND:
        out_value->uint64_value = left_int & right_int;
        return 0;
    case MYLITE_SQL_AST_OPERATOR_BITWISE_XOR:
        out_value->uint64_value = left_int ^ right_int;
        return 0;
    case MYLITE_SQL_AST_OPERATOR_BITWISE_OR:
        out_value->uint64_value = left_int | right_int;
        return 0;
    case MYLITE_SQL_AST_OPERATOR_SHIFT_LEFT:
        out_value->uint64_value =
            right_int >= MYLITE_EXPRESSION_BITS_PER_UINT64 ? 0U : left_int << right_int;
        return 0;
    case MYLITE_SQL_AST_OPERATOR_SHIFT_RIGHT:
        out_value->uint64_value =
            right_int >= MYLITE_EXPRESSION_BITS_PER_UINT64 ? 0U : left_int >> right_int;
        return 0;
    default:
        break;
    }
    return -1;
}

static int eval_comparison(enum mylite_sql_ast_operator operator_kind,
                           const struct mylite_expression_value *left,
                           const struct mylite_expression_value *right,
                           struct mylite_expression_warnings *warnings,
                           struct mylite_expression_value *out_value)
{
    int comparison = 0;
    bool result = false;

    if (operator_kind == MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL) {
        if (is_null(left) || is_null(right)) {
            result = is_null(left) && is_null(right);
            *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                          .int64_value = result ? 1 : 0};
            return 0;
        }
    } else if (is_null(left) || is_null(right)) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        return 0;
    }
    if (compare_values(left, right, warnings, &comparison) != 0) {
        return -1;
    }

    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
        result = comparison == 0;
        break;
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
        result = comparison != 0;
        break;
    case MYLITE_SQL_AST_OPERATOR_LESS:
        result = comparison < 0;
        break;
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
        result = comparison <= 0;
        break;
    case MYLITE_SQL_AST_OPERATOR_GREATER:
        result = comparison > 0;
        break;
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
        result = comparison >= 0;
        break;
    default:
        return -1;
    }
    *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_INT64,
                                                  .int64_value = result ? 1 : 0};
    return 0;
}

static int eval_logical(enum mylite_sql_ast_operator operator_kind,
                        const struct mylite_expression_value *left,
                        const struct mylite_expression_value *right,
                        struct mylite_expression_warnings *warnings,
                        struct mylite_expression_value *out_value)
{
    int left_truth = -1;
    int right_truth = -1;
    int status = truth_value(left, warnings, &left_truth);

    if (status == 0) {
        status = truth_value(right, warnings, &right_truth);
    }
    if (status != 0) {
        return status;
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_LOGICAL_AND) {
        if (left_truth == 0 || right_truth == 0) {
            out_value->kind = MYLITE_EXPRESSION_VALUE_INT64;
            out_value->int64_value = 0;
        } else if (left_truth < 0 || right_truth < 0) {
            out_value->kind = MYLITE_EXPRESSION_VALUE_NULL;
        } else {
            out_value->kind = MYLITE_EXPRESSION_VALUE_INT64;
            out_value->int64_value = 1;
        }
        return 0;
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_LOGICAL_OR) {
        if (left_truth == 1 || right_truth == 1) {
            out_value->kind = MYLITE_EXPRESSION_VALUE_INT64;
            out_value->int64_value = 1;
        } else if (left_truth < 0 || right_truth < 0) {
            out_value->kind = MYLITE_EXPRESSION_VALUE_NULL;
        } else {
            out_value->kind = MYLITE_EXPRESSION_VALUE_INT64;
            out_value->int64_value = 0;
        }
        return 0;
    }
    if (left_truth < 0 || right_truth < 0) {
        out_value->kind = MYLITE_EXPRESSION_VALUE_NULL;
    } else {
        out_value->kind = MYLITE_EXPRESSION_VALUE_INT64;
        out_value->int64_value = left_truth != right_truth ? 1 : 0;
    }
    return 0;
}

static int truth_value(const struct mylite_expression_value *value,
                       struct mylite_expression_warnings *warnings, int *out_truth)
{
    struct numeric_value number = {0};
    int status = 0;

    if (is_null(value)) {
        *out_truth = -1;
        return 0;
    }
    status = value_to_numeric(value, warnings, &number);
    if (status != 0) {
        return status;
    }
    *out_truth = number.real_value == 0.0 ? 0 : 1;
    return 0;
}

static int compare_values(const struct mylite_expression_value *left,
                          const struct mylite_expression_value *right,
                          struct mylite_expression_warnings *warnings, int *out_compare)
{
    if (is_numeric_kind(left->kind) || is_numeric_kind(right->kind)) {
        struct numeric_value left_number = {0};
        struct numeric_value right_number = {0};
        int status = value_to_numeric(left, warnings, &left_number);

        if (status == 0) {
            status = value_to_numeric(right, warnings, &right_number);
        }
        if (status != 0) {
            return status;
        }
        *out_compare = (left_number.real_value > right_number.real_value) -
                       (left_number.real_value < right_number.real_value);
        return 0;
    }

    char *left_text = NULL;
    char *right_text = NULL;
    int status = value_to_string(left, &left_text);

    if (status == 0) {
        status = value_to_string(right, &right_text);
    }
    if (status == 0) {
        int comparison = strcmp(left_text, right_text);
        *out_compare = (comparison > 0) - (comparison < 0);
    }
    free(left_text);
    free(right_text);
    return status;
}

static int value_to_numeric(const struct mylite_expression_value *value,
                            struct mylite_expression_warnings *warnings,
                            struct numeric_value *out_numeric)
{
    char *text = NULL;
    char *end = NULL;
    bool saw_digit = false;

    *out_numeric = (struct numeric_value){0};
    if (value->kind == MYLITE_EXPRESSION_VALUE_INT64) {
        out_numeric->int64_value = value->int64_value;
        out_numeric->uint64_value = (uint64_t)value->int64_value;
        out_numeric->real_value = (double)value->int64_value;
        out_numeric->is_integer = true;
        return 0;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_UINT64) {
        out_numeric->uint64_value = value->uint64_value;
        out_numeric->int64_value = (int64_t)value->uint64_value;
        out_numeric->real_value = (double)value->uint64_value;
        out_numeric->is_integer = true;
        out_numeric->is_unsigned = true;
        return 0;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_REAL) {
        out_numeric->real_value = value->real_value;
        out_numeric->int64_value = (int64_t)value->real_value;
        return 0;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_NULL) {
        return -1;
    }

    text = value->text_value == NULL ? copy_span_text("", 0U)
                                     : copy_span_text(value->text_value, strlen(value->text_value));
    if (text == NULL) {
        return -1;
    }
    char *start = text;
    while (isspace((unsigned char)*start)) {
        ++start;
    }
    char *scan = start;
    if (*scan == '+' || *scan == '-') {
        ++scan;
    }
    for (; *scan != '\0'; ++scan) {
        if (isdigit((unsigned char)*scan)) {
            saw_digit = true;
            break;
        }
        if (*scan != '.') {
            break;
        }
    }
    if (!saw_digit) {
        int status = append_truncation_warning(warnings, text);
        free(text);
        return status;
    }
    errno = 0;
    out_numeric->real_value = strtod(start, &end);
    out_numeric->int64_value = (int64_t)out_numeric->real_value;
    while (end != NULL && isspace((unsigned char)*end)) {
        ++end;
    }
    if (end == start || (end != NULL && *end != '\0')) {
        int status = append_truncation_warning(warnings, text);
        free(text);
        return status;
    }
    free(text);
    return 0;
}

static int cast_value_to_signed_integer(const struct mylite_expression_value *value,
                                        struct mylite_expression_warnings *warnings,
                                        int64_t *out_integer)
{
    if (out_integer == NULL) {
        return -1;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_INT64) {
        *out_integer = value->int64_value;
        return 0;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_UINT64) {
        *out_integer = signed_integer_from_uint64(value->uint64_value);
        return 0;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_REAL) {
        *out_integer = cast_real_to_signed_integer(value->real_value);
        return 0;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_TEXT) {
        return cast_string_to_signed_integer(value->text_value == NULL ? "" : value->text_value,
                                             warnings, out_integer);
    }
    return -1;
}

static int cast_value_to_unsigned_integer(const struct mylite_expression_value *value,
                                          struct mylite_expression_warnings *warnings,
                                          uint64_t *out_integer)
{
    if (out_integer == NULL) {
        return -1;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_INT64) {
        *out_integer = (uint64_t)value->int64_value;
        return 0;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_UINT64) {
        *out_integer = value->uint64_value;
        return 0;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_REAL) {
        *out_integer = (uint64_t)cast_real_to_signed_integer(value->real_value);
        return 0;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_TEXT) {
        return cast_string_to_unsigned_integer(value->text_value == NULL ? "" : value->text_value,
                                               warnings, out_integer);
    }
    return -1;
}

static int cast_string_to_signed_integer(const char *text,
                                         struct mylite_expression_warnings *warnings,
                                         int64_t *out_integer)
{
    struct cast_integer_parse parsed = parse_cast_integer_text(text);
    bool truncated = !parsed.saw_digit || parsed.trailing_garbage || parsed.overflow;

    if (truncated) {
        int status = append_cast_truncation_warning(warnings, "INTEGER", text);

        if (status != 0) {
            return status;
        }
    }
    if (!parsed.saw_digit) {
        *out_integer = 0;
        return 0;
    }
    if (parsed.negative) {
        *out_integer = parsed.magnitude == mylite_expression_int64_min_magnitude
                           ? INT64_MIN
                           : -(int64_t)parsed.magnitude;
        return 0;
    }
    *out_integer = signed_integer_from_uint64(parsed.magnitude);
    if (!parsed.overflow && parsed.magnitude > (uint64_t)INT64_MAX) {
        return append_signed_complement_warning(warnings);
    }
    return 0;
}

static int cast_string_to_unsigned_integer(const char *text,
                                           struct mylite_expression_warnings *warnings,
                                           uint64_t *out_integer)
{
    struct cast_integer_parse parsed = parse_cast_integer_text(text);
    bool truncated = !parsed.saw_digit || parsed.trailing_garbage || parsed.overflow;

    if (truncated) {
        int status = append_cast_truncation_warning(warnings, "INTEGER", text);

        if (status != 0) {
            return status;
        }
    }
    if (!parsed.saw_digit) {
        *out_integer = 0;
        return 0;
    }
    if (parsed.negative) {
        *out_integer = unsigned_complement_from_magnitude(parsed.magnitude);
        if (!parsed.overflow && parsed.magnitude != 0U) {
            return append_unsigned_complement_warning(warnings);
        }
        return 0;
    }
    *out_integer = parsed.magnitude;
    return 0;
}

static struct cast_integer_parse parse_cast_integer_text(const char *text)
{
    const char *start = text == NULL ? "" : text;
    const char *scan = NULL;
    const uint64_t radix = (uint64_t)MYLITE_EXPRESSION_DECIMAL_BASE;
    struct cast_integer_parse parsed = {0};

    while (isspace((unsigned char)*start)) {
        ++start;
    }
    if (*start == '+' || *start == '-') {
        parsed.negative = *start == '-';
        ++start;
    }
    scan = start;
    while (isdigit((unsigned char)*scan)) {
        uint64_t digit = (uint64_t)(*scan - '0');
        uint64_t limit = parsed.negative ? mylite_expression_int64_min_magnitude : UINT64_MAX;

        parsed.saw_digit = true;
        if (parsed.magnitude > (limit - digit) / radix) {
            parsed.magnitude = limit;
            parsed.overflow = true;
        } else if (!parsed.overflow) {
            parsed.magnitude = (parsed.magnitude * radix) + digit;
        }
        ++scan;
    }
    while (isspace((unsigned char)*scan)) {
        ++scan;
    }
    parsed.trailing_garbage = *scan != '\0';
    return parsed;
}

static int64_t signed_integer_from_uint64(uint64_t value)
{
    if (value <= (uint64_t)INT64_MAX) {
        return (int64_t)value;
    }
    return INT64_MIN + (int64_t)(value - mylite_expression_int64_min_magnitude);
}

static uint64_t unsigned_complement_from_magnitude(uint64_t magnitude)
{
    if (magnitude == 0U) {
        return 0U;
    }
    return (UINT64_MAX - magnitude) + 1U;
}

static int cast_value_to_decimal_double(const struct mylite_expression_value *value,
                                        struct mylite_expression_warnings *warnings,
                                        double *out_number)
{
    if (out_number == NULL) {
        return -1;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_INT64) {
        *out_number = (double)value->int64_value;
        return 0;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_UINT64) {
        *out_number = (double)value->uint64_value;
        return 0;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_REAL) {
        *out_number = value->real_value;
        return 0;
    }
    if (value->kind == MYLITE_EXPRESSION_VALUE_TEXT) {
        return cast_string_to_decimal_double(value->text_value == NULL ? "" : value->text_value,
                                             warnings, out_number);
    }
    return -1;
}

static int cast_string_to_decimal_double(const char *text,
                                         struct mylite_expression_warnings *warnings,
                                         double *out_number)
{
    char *copy = copy_span_text(text == NULL ? "" : text, strlen(text == NULL ? "" : text));
    char *start = NULL;
    char *end = NULL;

    if (copy == NULL) {
        return -1;
    }
    start = copy;
    while (isspace((unsigned char)*start)) {
        ++start;
    }
    errno = 0;
    *out_number = strtod(start, &end);
    while (end != NULL && isspace((unsigned char)*end)) {
        ++end;
    }
    if (end == start || (end != NULL && *end != '\0')) {
        int status = append_cast_truncation_warning(warnings, "DECIMAL", text);

        if (end == start) {
            *out_number = 0.0;
        }
        free(copy);
        return status;
    }
    free(copy);
    return 0;
}

static int cast_value_to_string(const struct mylite_expression_value *value, char **out_text)
{
    char buffer[MYLITE_EXPRESSION_TEXT_BUFFER_SIZE];
    int length = 0;

    if (out_text == NULL || value == NULL) {
        return -1;
    }
    switch (value->kind) {
    case MYLITE_EXPRESSION_VALUE_INT64:
        length = snprintf(buffer, sizeof(buffer), "%lld", (long long)value->int64_value);
        *out_text = length < 0 ? NULL : copy_span_text(buffer, (size_t)length);
        return *out_text == NULL ? -1 : 0;
    case MYLITE_EXPRESSION_VALUE_UINT64:
        length = snprintf(buffer, sizeof(buffer), "%llu", (unsigned long long)value->uint64_value);
        *out_text = length < 0 ? NULL : copy_span_text(buffer, (size_t)length);
        return *out_text == NULL ? -1 : 0;
    case MYLITE_EXPRESSION_VALUE_REAL:
        return cast_real_to_string(value->real_value, out_text);
    case MYLITE_EXPRESSION_VALUE_TEXT:
        *out_text = copy_span_text(value->text_value == NULL ? "" : value->text_value,
                                   value->text_value == NULL ? 0U : strlen(value->text_value));
        return *out_text == NULL ? -1 : 0;
    case MYLITE_EXPRESSION_VALUE_NULL:
        return -1;
    }
    return -1;
}

static int cast_real_to_string(double value, char **out_text)
{
    char buffer[MYLITE_EXPRESSION_TEXT_BUFFER_SIZE];
    int length = snprintf(buffer, sizeof(buffer), "%.15g", value);

    if (length < 0 || (size_t)length >= sizeof(buffer)) {
        return -1;
    }
    *out_text = copy_span_text(buffer, (size_t)length);
    return *out_text == NULL ? -1 : 0;
}

static int64_t cast_real_to_signed_integer(double value)
{
    double rounded = 0.0;

    if (value >= (double)INT64_MAX) {
        return INT64_MAX;
    }
    if (value <= (double)INT64_MIN) {
        return INT64_MIN;
    }
    if (value >= 0.0) {
        rounded = value + mylite_expression_round_half;
    } else {
        rounded = value - mylite_expression_round_half;
    }
    if (rounded >= (double)INT64_MAX) {
        return INT64_MAX;
    }
    if (rounded <= (double)INT64_MIN) {
        return INT64_MIN;
    }
    return (int64_t)rounded;
}

static unsigned int cast_decimal_scale(const struct mylite_sql_ast_node *target)
{
    if (target != NULL && target->has_column_scale) {
        return (unsigned int)target->column_scale;
    }
    return 0U;
}

static double absolute_real_value(double value)
{
    return value < 0.0 ? -value : value;
}

static int64_t floor_real_value(double value)
{
    int64_t truncated = (int64_t)value;

    return (double)truncated > value ? truncated - 1 : truncated;
}

static int64_t ceil_real_value(double value)
{
    int64_t truncated = (int64_t)value;

    return (double)truncated < value ? truncated + 1 : truncated;
}

static int value_to_string(const struct mylite_expression_value *value, char **out_text)
{
    *out_text = mylite_expression_value_to_text(value);
    return *out_text == NULL && !is_null(value) ? -1 : 0;
}

static int set_text_value(const char *text, size_t length,
                          struct mylite_expression_value *out_value)
{
    out_value->text_value = copy_span_text(text, length);
    if (out_value->text_value == NULL) {
        return -1;
    }
    out_value->kind = MYLITE_EXPRESSION_VALUE_TEXT;
    return 0;
}

static int append_text(char **text, size_t *length, const char *addition, size_t addition_length)
{
    char *updated = realloc(*text, *length + addition_length + 1U);

    if (updated == NULL) {
        return -1;
    }
    if (addition_length != 0U) {
        memcpy(updated + *length, addition, addition_length);
    }
    *length += addition_length;
    updated[*length] = '\0';
    *text = updated;
    return 0;
}

static int utf8_char_count(const char *text, int64_t *out_count)
{
    int64_t count = 0;

    if (text == NULL) {
        *out_count = 0;
        return 0;
    }

    for (const unsigned char *cursor = (const unsigned char *)text; *cursor != '\0'; ++cursor) {
        if ((*cursor & MYLITE_UTF8_CONTINUATION_MASK) != MYLITE_UTF8_CONTINUATION_MARKER) {
            ++count;
        }
    }
    *out_count = count;
    return 0;
}

static size_t utf8_offset_for_chars(const char *text, int64_t char_count)
{
    const unsigned char *cursor = (const unsigned char *)(text == NULL ? "" : text);
    int64_t count = 0;

    while (*cursor != '\0' && count < char_count) {
        ++cursor;
        while (*cursor != '\0' &&
               (*cursor & MYLITE_UTF8_CONTINUATION_MASK) == MYLITE_UTF8_CONTINUATION_MARKER) {
            ++cursor;
        }
        ++count;
    }
    return (size_t)((const char *)cursor - (text == NULL ? "" : text));
}

static int append_warning(struct mylite_expression_warnings *warnings, unsigned int code,
                          const char *message)
{
    return mylite_expression_warnings_append(warnings, code, message);
}

static int append_truncation_warning(struct mylite_expression_warnings *warnings, const char *text)
{
    char message[MYLITE_EXPRESSION_WARNING_MESSAGE_SIZE];
    int length = snprintf(message, sizeof(message), "Truncated incorrect DOUBLE value: '%.*s'",
                          MYLITE_EXPRESSION_WARNING_TEXT_PREVIEW, text == NULL ? "" : text);

    if (length < 0) {
        return -1;
    }
    return append_warning(warnings, MYLITE_WARNING_TRUNCATED_WRONG_VALUE, message);
}

static int append_cast_truncation_warning(struct mylite_expression_warnings *warnings,
                                          const char *type_name, const char *text)
{
    char message[MYLITE_EXPRESSION_WARNING_MESSAGE_SIZE];
    int length = snprintf(message, sizeof(message), "Truncated incorrect %s value: '%.*s'",
                          type_name == NULL ? "" : type_name,
                          MYLITE_EXPRESSION_WARNING_TEXT_PREVIEW, text == NULL ? "" : text);

    if (length < 0) {
        return -1;
    }
    return append_warning(warnings, MYLITE_WARNING_TRUNCATED_WRONG_VALUE, message);
}

static int append_char_truncation_warning(struct mylite_expression_warnings *warnings,
                                          uint64_t length, const char *text)
{
    char message[MYLITE_EXPRESSION_WARNING_MESSAGE_SIZE];
    int written = snprintf(message, sizeof(message), "Truncated incorrect CHAR(%llu) value: '%.*s'",
                           (unsigned long long)length, MYLITE_EXPRESSION_WARNING_TEXT_PREVIEW,
                           text == NULL ? "" : text);

    if (written < 0) {
        return -1;
    }
    return append_warning(warnings, MYLITE_WARNING_TRUNCATED_WRONG_VALUE, message);
}

static int append_signed_complement_warning(struct mylite_expression_warnings *warnings)
{
    return append_warning(warnings, MYLITE_WARNING_UNKNOWN,
                          "Cast to signed converted positive out-of-range integer to its negative "
                          "complement");
}

static int append_unsigned_complement_warning(struct mylite_expression_warnings *warnings)
{
    return append_warning(warnings, MYLITE_WARNING_UNKNOWN,
                          "Cast to unsigned converted negative integer to its positive "
                          "complement");
}

static char *copy_span_text(const char *text, size_t length)
{
    char *copy = malloc(length + 1U);

    if (copy == NULL) {
        return NULL;
    }
    if (length != 0U) {
        memcpy(copy, text, length);
    }
    copy[length] = '\0';
    return copy;
}

static char *decode_string_literal(const struct mylite_sql_ast_node *node)
{
    const char *text = node->span.text;
    size_t length = node->span.length;
    size_t start = 0U;
    size_t end = length;
    char *decoded = NULL;
    size_t output = 0U;

    if (length >= 2U && (text[0] == '\'' || text[0] == '"')) {
        start = 1U;
        end = length - 1U;
    } else if (length >= 3U && (text[0] == 'N' || text[0] == 'n') &&
               (text[1] == '\'' || text[1] == '"')) {
        start = 2U;
        end = length - 1U;
    }

    decoded = malloc(end >= start ? end - start + 1U : 1U);
    if (decoded == NULL) {
        return NULL;
    }
    for (size_t index = start; index < end; ++index) {
        if (text[index] == '\\' && index + 1U < end) {
            char escaped = '\0';

            if (decode_string_escape(text[index + 1U], &escaped)) {
                decoded[output++] = escaped;
                ++index;
            } else {
                decoded[output++] = text[index];
            }
        } else if ((text[index] == '\'' || text[index] == '"') && index + 1U < end &&
                   text[index + 1U] == text[index]) {
            decoded[output++] = text[index++];
        } else {
            decoded[output++] = text[index];
        }
    }
    decoded[output] = '\0';
    return decoded;
}

static bool decode_string_escape(char escaped, char *out_character)
{
    switch (escaped) {
    case '\'':
    case '"':
    case '\\':
        *out_character = escaped;
        return true;
    case 'b':
        *out_character = '\b';
        return true;
    case 'n':
        *out_character = '\n';
        return true;
    case 'r':
        *out_character = '\r';
        return true;
    case 't':
        *out_character = '\t';
        return true;
    case 'Z':
        *out_character = '\x1A';
        return true;
    default:
        return false;
    }
}

static const struct mylite_sql_ast_node *
unwrap_parenthesized_node(const struct mylite_sql_ast_node *node)
{
    while (node != NULL && node->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
        node = child_at(node, 0U);
    }
    return node;
}

static const struct mylite_sql_ast_node *child_at(const struct mylite_sql_ast_node *node,
                                                  size_t index)
{
    const struct mylite_sql_ast_node *child = node == NULL ? NULL : node->first_child;

    for (size_t current = 0U; current < index && child != NULL; ++current) {
        child = child->next_sibling;
    }
    return child;
}

static size_t child_count(const struct mylite_sql_ast_node *node)
{
    size_t count = 0U;

    for (const struct mylite_sql_ast_node *child = node == NULL ? NULL : node->first_child;
         child != NULL; child = child->next_sibling) {
        ++count;
    }
    return count;
}

static enum mylite_scalar_function_id scalar_function_id(const struct mylite_sql_ast_node *node)
{
    const struct mylite_sql_ast_node *name = child_at(node, 0U);

    if (name == NULL || name->kind != MYLITE_SQL_AST_IDENTIFIER) {
        return MYLITE_SCALAR_FUNCTION_UNKNOWN;
    }
    return scalar_function_id_from_span(name->span);
}

static enum mylite_scalar_function_id
scalar_function_id_from_span(struct mylite_sql_source_span span)
{
    static const struct {
        const char *name;
        enum mylite_scalar_function_id id;
    } functions[] = {
        {"CONCAT", MYLITE_SCALAR_FUNCTION_CONCAT},
        {"LENGTH", MYLITE_SCALAR_FUNCTION_LENGTH},
        {"OCTET_LENGTH", MYLITE_SCALAR_FUNCTION_LENGTH},
        {"CHAR_LENGTH", MYLITE_SCALAR_FUNCTION_CHAR_LENGTH},
        {"CHARACTER_LENGTH", MYLITE_SCALAR_FUNCTION_CHAR_LENGTH},
        {"LOWER", MYLITE_SCALAR_FUNCTION_LOWER},
        {"LCASE", MYLITE_SCALAR_FUNCTION_LOWER},
        {"UPPER", MYLITE_SCALAR_FUNCTION_UPPER},
        {"UCASE", MYLITE_SCALAR_FUNCTION_UPPER},
        {"LEFT", MYLITE_SCALAR_FUNCTION_LEFT},
        {"RIGHT", MYLITE_SCALAR_FUNCTION_RIGHT},
        {"REPLACE", MYLITE_SCALAR_FUNCTION_REPLACE},
        {"ABS", MYLITE_SCALAR_FUNCTION_ABS},
        {"SIGN", MYLITE_SCALAR_FUNCTION_SIGN},
        {"FLOOR", MYLITE_SCALAR_FUNCTION_FLOOR},
        {"CEIL", MYLITE_SCALAR_FUNCTION_CEIL},
        {"CEILING", MYLITE_SCALAR_FUNCTION_CEIL},
        {"MOD", MYLITE_SCALAR_FUNCTION_MOD},
        {"PI", MYLITE_SCALAR_FUNCTION_PI},
        {"IF", MYLITE_SCALAR_FUNCTION_IF},
        {"IFNULL", MYLITE_SCALAR_FUNCTION_IFNULL},
        {"NULLIF", MYLITE_SCALAR_FUNCTION_NULLIF},
        {"COALESCE", MYLITE_SCALAR_FUNCTION_COALESCE},
        {"ISNULL", MYLITE_SCALAR_FUNCTION_ISNULL},
        {"DATABASE", MYLITE_SCALAR_FUNCTION_DATABASE},
        {"SCHEMA", MYLITE_SCALAR_FUNCTION_SCHEMA},
        {"VERSION", MYLITE_SCALAR_FUNCTION_VERSION},
        {"LAST_INSERT_ID", MYLITE_SCALAR_FUNCTION_LAST_INSERT_ID},
        {"ROW_COUNT", MYLITE_SCALAR_FUNCTION_ROW_COUNT},
    };

    for (size_t index = 0U; index < sizeof(functions) / sizeof(functions[0]); ++index) {
        if (ascii_span_equals(span, functions[index].name)) {
            return functions[index].id;
        }
    }
    return MYLITE_SCALAR_FUNCTION_UNKNOWN;
}

static bool scalar_function_depends_on_session(enum mylite_scalar_function_id function_id)
{
    switch (function_id) {
    case MYLITE_SCALAR_FUNCTION_DATABASE:
    case MYLITE_SCALAR_FUNCTION_SCHEMA:
    case MYLITE_SCALAR_FUNCTION_VERSION:
    case MYLITE_SCALAR_FUNCTION_LAST_INSERT_ID:
    case MYLITE_SCALAR_FUNCTION_ROW_COUNT:
        return true;
    case MYLITE_SCALAR_FUNCTION_UNKNOWN:
    case MYLITE_SCALAR_FUNCTION_CONCAT:
    case MYLITE_SCALAR_FUNCTION_LENGTH:
    case MYLITE_SCALAR_FUNCTION_CHAR_LENGTH:
    case MYLITE_SCALAR_FUNCTION_LOWER:
    case MYLITE_SCALAR_FUNCTION_UPPER:
    case MYLITE_SCALAR_FUNCTION_LEFT:
    case MYLITE_SCALAR_FUNCTION_RIGHT:
    case MYLITE_SCALAR_FUNCTION_REPLACE:
    case MYLITE_SCALAR_FUNCTION_ABS:
    case MYLITE_SCALAR_FUNCTION_SIGN:
    case MYLITE_SCALAR_FUNCTION_FLOOR:
    case MYLITE_SCALAR_FUNCTION_CEIL:
    case MYLITE_SCALAR_FUNCTION_MOD:
    case MYLITE_SCALAR_FUNCTION_PI:
    case MYLITE_SCALAR_FUNCTION_IF:
    case MYLITE_SCALAR_FUNCTION_IFNULL:
    case MYLITE_SCALAR_FUNCTION_NULLIF:
    case MYLITE_SCALAR_FUNCTION_COALESCE:
    case MYLITE_SCALAR_FUNCTION_ISNULL:
        return false;
    }
    return false;
}

static bool ascii_span_equals(struct mylite_sql_source_span span, const char *text)
{
    size_t text_length = text == NULL ? 0U : strlen(text);

    if (span.length != text_length || span.text == NULL || text == NULL) {
        return false;
    }
    for (size_t index = 0U; index < span.length; ++index) {
        if (ascii_case_fold((unsigned char)span.text[index]) !=
            ascii_case_fold((unsigned char)text[index])) {
            return false;
        }
    }
    return true;
}

static bool is_null(const struct mylite_expression_value *value)
{
    return value == NULL || value->kind == MYLITE_EXPRESSION_VALUE_NULL;
}

static bool is_numeric_kind(enum mylite_expression_value_kind kind)
{
    return kind == MYLITE_EXPRESSION_VALUE_INT64 || kind == MYLITE_EXPRESSION_VALUE_UINT64 ||
           kind == MYLITE_EXPRESSION_VALUE_REAL;
}

static bool like_match(const char *value, const char *pattern, char escape)
{
    return like_match_here(value == NULL ? "" : value, pattern == NULL ? "" : pattern, escape);
}

static bool like_match_here(const char *value, const char *pattern, char escape)
{
    if (*pattern == '\0') {
        return *value == '\0';
    }
    if (*pattern == '%') {
        do {
            if (like_match_here(value, pattern + 1, escape)) {
                return true;
            }
        } while (*value++ != '\0');
        return false;
    }
    if (*pattern == escape && pattern[1] != '\0') {
        return *value != '\0' &&
               ascii_case_fold((unsigned char)*value) ==
                   ascii_case_fold((unsigned char)pattern[1]) &&
               like_match_here(value + 1, pattern + 2, escape);
    }
    if (*pattern == '_') {
        return *value != '\0' && like_match_here(value + 1, pattern + 1, escape);
    }
    return *value != '\0' &&
           ascii_case_fold((unsigned char)*value) == ascii_case_fold((unsigned char)*pattern) &&
           like_match_here(value + 1, pattern + 1, escape);
}

static int ascii_case_fold(int character)
{
    return character >= 'A' && character <= 'Z' ? character - 'A' + 'a' : character;
}

// NOLINTEND(misc-no-recursion, readability-implicit-bool-conversion)
