#include "mylite_statement_strcmp.h"

#include "mylite_charset.h"
#include "mylite_diagnostics.h"
#include "mylite_expression.h"
#include "mylite_expression_collation.h"
#include "mylite_metadata_constants.h"
#include "mylite_runtime.h"
#include "mylite_span.h"
#include "sql/mylite_ast.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct mylite_statement_strcmp_compare_options {
    bool ignore_trailing_spaces;
    bool case_sensitive;
};

static int
infer_strcmp_collation_info(mylite_stmt *stmt, const struct mylite_sql_ast_node *function_call,
                            const struct mylite_select_table *table,
                            const struct mylite_expression_collation_callbacks *collation_callbacks,
                            struct mylite_charset_collation_info *out_info);
static int set_strcmp_function_result(mylite_db *database,
                                      const struct mylite_expression_value *left,
                                      const struct mylite_sql_ast_node *left_argument,
                                      const struct mylite_expression_value *right,
                                      const struct mylite_sql_ast_node *right_argument,
                                      const struct mylite_charset_collation_info *collation_info,
                                      struct mylite_expression_value *out_value);
static int strcmp_value_to_text(mylite_db *database, const struct mylite_expression_value *value,
                                const struct mylite_sql_ast_node *argument, char **out_text,
                                size_t *out_length);
static const struct mylite_sql_ast_node *
strcmp_decimal_literal_argument(const struct mylite_sql_ast_node *argument, bool *out_negative);
static int strcmp_decimal_literal_to_text(mylite_db *database,
                                          const struct mylite_sql_ast_node *literal, bool negative,
                                          char **out_text, size_t *out_length);
static bool decimal_literal_span_is_zero(const char *text, size_t length);
static int compare_strcmp_texts(const char *left, size_t left_length, const char *right,
                                size_t right_length,
                                struct mylite_statement_strcmp_compare_options options);
static void trim_strcmp_trailing_spaces(const char *text, size_t *length);
static unsigned char strcmp_compare_byte(unsigned char value,
                                         struct mylite_statement_strcmp_compare_options options);
static struct mylite_statement_strcmp_compare_options
strcmp_compare_options_for_collation(const struct mylite_charset_collation_info *info);
static bool strcmp_collation_ignores_trailing_spaces(const char *collation_name);
static bool strcmp_collation_is_case_sensitive(const struct mylite_charset_collation_info *info);

int mylite_statement_evaluate_strcmp_function(
    mylite_stmt *stmt, const struct mylite_sql_ast_node *function_call,
    const struct mylite_expression_eval_context *expression_context,
    struct mylite_expression_warnings *warnings, const struct mylite_select_table *table,
    const struct mylite_expression_collation_callbacks *collation_callbacks,
    struct mylite_expression_value *out_value)
{
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(function_call, 1U);
    const struct mylite_sql_ast_node *left_argument = mylite_ast_child_at(arguments, 0U);
    const struct mylite_sql_ast_node *right_argument = mylite_ast_child_at(arguments, 1U);
    struct mylite_charset_collation_info collation_info =
        mylite_expression_binary_collation_info(mylite_mysql_coercibility_ignorable);
    struct mylite_expression_value left = {0};
    struct mylite_expression_value right = {0};
    int status = MYLITE_OK;

    if (stmt == NULL || stmt->database == NULL ||
        mylite_sql_ast_node_child_count(arguments) != 2U) {
        return -1;
    }

    status =
        mylite_expression_eval_with_context(left_argument, expression_context, warnings, &left);
    if (status != MYLITE_OK) {
        goto cleanup;
    }
    if (left.kind == MYLITE_EXPRESSION_VALUE_NULL) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }

    status =
        mylite_expression_eval_with_context(right_argument, expression_context, warnings, &right);
    if (status != MYLITE_OK) {
        goto cleanup;
    }
    if (right.kind == MYLITE_EXPRESSION_VALUE_NULL) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }

    status = infer_strcmp_collation_info(stmt, function_call, table, collation_callbacks,
                                         &collation_info);
    if (status == MYLITE_OK) {
        status = set_strcmp_function_result(stmt->database, &left, left_argument, &right,
                                            right_argument, &collation_info, out_value);
    }

cleanup:
    mylite_expression_value_deinit(&right);
    mylite_expression_value_deinit(&left);
    return status;
}

static int
infer_strcmp_collation_info(mylite_stmt *stmt, const struct mylite_sql_ast_node *function_call,
                            const struct mylite_select_table *table,
                            const struct mylite_expression_collation_callbacks *collation_callbacks,
                            struct mylite_charset_collation_info *out_info)
{
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(function_call, 1U);
    struct mylite_expression_collation_context context = {
        .plan = stmt == NULL ? NULL : &stmt->select_plan,
        .table = table,
    };
    int status = MYLITE_OK;

    if (stmt == NULL || stmt->database == NULL) {
        return -1;
    }

    status = mylite_expression_infer_function_arguments_collation_info(
        stmt->database, &context, arguments, 0U, true, collation_callbacks, out_info);
    if (status != MYLITE_OK) {
        return status;
    }
    if (out_info->coercibility == mylite_mysql_coercibility_ignorable ||
        out_info->collation == NULL) {
        *out_info = mylite_expression_connection_collation_info(
            stmt->database, mylite_mysql_coercibility_coercible);
    }
    return MYLITE_OK;
}

static int set_strcmp_function_result(mylite_db *database,
                                      const struct mylite_expression_value *left,
                                      const struct mylite_sql_ast_node *left_argument,
                                      const struct mylite_expression_value *right,
                                      const struct mylite_sql_ast_node *right_argument,
                                      const struct mylite_charset_collation_info *collation_info,
                                      struct mylite_expression_value *out_value)
{
    struct mylite_statement_strcmp_compare_options options =
        strcmp_compare_options_for_collation(collation_info);
    char *left_text = NULL;
    char *right_text = NULL;
    size_t left_length = 0U;
    size_t right_length = 0U;
    int status = strcmp_value_to_text(database, left, left_argument, &left_text, &left_length);

    if (status == MYLITE_OK) {
        status = strcmp_value_to_text(database, right, right_argument, &right_text, &right_length);
    }
    if (status == MYLITE_OK) {
        *out_value = (struct mylite_expression_value){
            .kind = MYLITE_EXPRESSION_VALUE_INT64,
            .int64_value =
                compare_strcmp_texts(left_text, left_length, right_text, right_length, options),
        };
    }

    free(right_text);
    free(left_text);
    return status;
}

static int strcmp_value_to_text(mylite_db *database, const struct mylite_expression_value *value,
                                const struct mylite_sql_ast_node *argument, char **out_text,
                                size_t *out_length)
{
    enum { strcmp_text_buffer_size = 64 };
    const struct mylite_sql_ast_node *decimal_literal = NULL;
    char buffer[strcmp_text_buffer_size];
    bool negative_decimal = false;
    int length = 0;

    if (value == NULL || value->kind == MYLITE_EXPRESSION_VALUE_NULL) {
        return -1;
    }
    decimal_literal = strcmp_decimal_literal_argument(argument, &negative_decimal);
    if (decimal_literal != NULL) {
        return strcmp_decimal_literal_to_text(database, decimal_literal, negative_decimal, out_text,
                                              out_length);
    }

    switch (value->kind) {
    case MYLITE_EXPRESSION_VALUE_INT64:
        length = snprintf(buffer, sizeof(buffer), "%lld", (long long)value->int64_value);
        break;
    case MYLITE_EXPRESSION_VALUE_UINT64:
        length = snprintf(buffer, sizeof(buffer), "%llu", (unsigned long long)value->uint64_value);
        break;
    case MYLITE_EXPRESSION_VALUE_REAL:
        length = snprintf(buffer, sizeof(buffer), "%.15g", value->real_value);
        break;
    case MYLITE_EXPRESSION_VALUE_TEXT:
        *out_length = value->text_value == NULL ? 0U : value->text_length;
        *out_text =
            mylite_copy_span_text(value->text_value == NULL ? "" : value->text_value, *out_length);
        if (*out_text != NULL) {
            return MYLITE_OK;
        }
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    case MYLITE_EXPRESSION_VALUE_NULL:
        return -1;
    }

    if (length <= 0 || (size_t)length >= sizeof(buffer)) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    *out_length = (size_t)length;
    *out_text = mylite_copy_span_text(buffer, *out_length);
    if (*out_text == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    return MYLITE_OK;
}

static const struct mylite_sql_ast_node *
strcmp_decimal_literal_argument(const struct mylite_sql_ast_node *argument, bool *out_negative)
{
    bool negative = false;

    argument = mylite_sql_ast_unwrap_parenthesized_expression(argument);
    if (argument != NULL && argument->kind == MYLITE_SQL_AST_UNARY_EXPRESSION &&
        (argument->operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE ||
         argument->operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE)) {
        negative = argument->operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE;
        argument =
            mylite_sql_ast_unwrap_parenthesized_expression(mylite_ast_child_at(argument, 0U));
    }
    if (out_negative != NULL) {
        *out_negative = negative;
    }
    if (argument == NULL || argument->kind != MYLITE_SQL_AST_LITERAL ||
        argument->literal_kind != MYLITE_SQL_AST_LITERAL_DECIMAL) {
        return NULL;
    }
    return argument;
}

static int strcmp_decimal_literal_to_text(mylite_db *database,
                                          const struct mylite_sql_ast_node *literal, bool negative,
                                          char **out_text, size_t *out_length)
{
    const char *text = literal == NULL ? NULL : literal->span.text;
    size_t length = literal == NULL ? 0U : literal->span.length;
    const char *dot = NULL;
    size_t integer_length = 0U;
    size_t fractional_length = 0U;
    size_t integer_offset = 0U;
    size_t sign_length = 0U;
    size_t normalized_integer_length = 0U;
    size_t fractional_output_length = 0U;
    size_t result_length = 0U;
    size_t output = 0U;
    char *result = NULL;
    bool zero = false;

    if (text == NULL) {
        return -1;
    }
    dot = memchr(text, '.', length);
    if (dot == NULL) {
        return -1;
    }
    integer_length = (size_t)(dot - text);
    fractional_length = length - integer_length - 1U;
    zero = decimal_literal_span_is_zero(text, length);

    while (integer_offset < integer_length && text[integer_offset] == '0') {
        ++integer_offset;
    }
    normalized_integer_length = integer_length - integer_offset;
    if (normalized_integer_length == 0U) {
        normalized_integer_length = 1U;
    }
    sign_length = negative && !zero ? 1U : 0U;
    if (fractional_length > 0U) {
        if (fractional_length == SIZE_MAX) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
        fractional_output_length = 1U + fractional_length;
    }
    if (normalized_integer_length > SIZE_MAX - sign_length) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    result_length = sign_length + normalized_integer_length;
    if (result_length == SIZE_MAX || fractional_output_length >= SIZE_MAX - result_length) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    result_length += fractional_output_length;
    result = malloc(result_length + 1U);
    if (result == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    if (negative && !zero) {
        result[output++] = '-';
    }
    if (integer_length == integer_offset) {
        result[output++] = '0';
    } else {
        memcpy(result + output, text + integer_offset, normalized_integer_length);
        output += normalized_integer_length;
    }
    if (fractional_length > 0U) {
        result[output++] = '.';
        memcpy(result + output, dot + 1, fractional_length);
        output += fractional_length;
    }
    result[output] = '\0';
    *out_text = result;
    *out_length = output;
    return MYLITE_OK;
}

static bool decimal_literal_span_is_zero(const char *text, size_t length)
{
    for (size_t index = 0U; index < length; ++index) {
        if (text[index] >= '1' && text[index] <= '9') {
            return false;
        }
    }
    return true;
}

static int compare_strcmp_texts(const char *left, size_t left_length, const char *right,
                                size_t right_length,
                                struct mylite_statement_strcmp_compare_options options)
{
    size_t compare_length = 0U;

    if (left == NULL) {
        left = "";
        left_length = 0U;
    }
    if (right == NULL) {
        right = "";
        right_length = 0U;
    }
    if (options.ignore_trailing_spaces) {
        trim_strcmp_trailing_spaces(left, &left_length);
        trim_strcmp_trailing_spaces(right, &right_length);
    }

    compare_length = left_length < right_length ? left_length : right_length;
    for (size_t index = 0U; index < compare_length; ++index) {
        unsigned char left_byte = strcmp_compare_byte((unsigned char)left[index], options);
        unsigned char right_byte = strcmp_compare_byte((unsigned char)right[index], options);

        if (left_byte != right_byte) {
            return left_byte > right_byte ? 1 : -1;
        }
    }
    return (left_length > right_length) - (left_length < right_length);
}

static void trim_strcmp_trailing_spaces(const char *text, size_t *length)
{
    if (text == NULL || length == NULL) {
        return;
    }
    while (*length > 0U && text[*length - 1U] == ' ') {
        *length -= 1U;
    }
}

static unsigned char strcmp_compare_byte(unsigned char value,
                                         struct mylite_statement_strcmp_compare_options options)
{
    if (!options.case_sensitive && value >= 'A' && value <= 'Z') {
        return (unsigned char)(value - 'A' + 'a');
    }
    return value;
}

static struct mylite_statement_strcmp_compare_options
strcmp_compare_options_for_collation(const struct mylite_charset_collation_info *info)
{
    const char *collation_name = info == NULL || info->collation == NULL
                                     ? mylite_charset_default_collation_name()
                                     : info->collation;

    return (struct mylite_statement_strcmp_compare_options){
        .ignore_trailing_spaces = strcmp_collation_ignores_trailing_spaces(collation_name),
        .case_sensitive = strcmp_collation_is_case_sensitive(info),
    };
}

static bool strcmp_collation_ignores_trailing_spaces(const char *collation_name)
{
    const struct mylite_collation *collation = mylite_collation_lookup(collation_name);

    if (collation == NULL) {
        return false;
    }
    return mylite_ascii_case_equal(collation->pad_attribute, "PAD SPACE");
}

static bool strcmp_collation_is_case_sensitive(const struct mylite_charset_collation_info *info)
{
    const char *collation_name = info == NULL || info->collation == NULL
                                     ? mylite_charset_default_collation_name()
                                     : info->collation;
    size_t collation_length = strlen(collation_name);

    if (info != NULL &&
        mylite_ascii_case_equal(info->character_set, mylite_mysql_binary_charset_name)) {
        return true;
    }
    if (mylite_ascii_case_equal(collation_name, mylite_mysql_binary_charset_name)) {
        return true;
    }
    if (collation_length < 4U) {
        return false;
    }
    return mylite_ascii_case_equal(collation_name + collation_length - 4U, "_bin");
}
