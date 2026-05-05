#include "mylite_expression_descriptor_function.h"

#include "mylite_diagnostics.h"
#include "mylite_error_codes.h"
#include "mylite_expression.h"
#include "mylite_expression_descriptor.h"
#include "mylite_expression_descriptor_compare.h"
#include "mylite_expression_descriptor_numeric.h"
#include "mylite_expression_descriptor_scalar.h"
#include "mylite_expression_descriptor_string.h"
#include "mylite_expression_descriptor_temporal.h"
#include "mylite_function_names.h"
#include "mylite_span.h"
#include "sql/mylite_ast.h"

#include <stdbool.h>
#include <stdlib.h>

static int infer_temporal_function_descriptor(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, const struct mylite_expression_value *value,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_function_callbacks *callbacks);
static int infer_default_function_descriptor(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_function_callbacks *callbacks);
static bool function_name_is_default(const struct mylite_sql_ast_node *name);
static int set_default_function_unknown_column_error(mylite_db *database,
                                                     const struct mylite_sql_ast_node *identifier);
static bool infer_common_scalar_function_descriptor(mylite_db *database,
                                                    const struct mylite_sql_ast_node *name,
                                                    bool arguments_nullable, bool result_nullable,
                                                    struct mylite_field_descriptor *out_descriptor);
static int infer_variadic_scalar_function_descriptor(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, const struct mylite_expression_value *value,
    bool result_nullable, struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_function_callbacks *callbacks);
static int infer_function_arguments_nullable(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *arguments, bool *out_nullable,
    const struct mylite_expression_descriptor_function_callbacks *callbacks);
static int validate_function_descriptor_callbacks(
    const struct mylite_expression_descriptor_function_callbacks *callbacks);

// NOLINTNEXTLINE(misc-no-recursion)
int mylite_expression_descriptor_infer_function_expression(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, const struct mylite_expression_value *value,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_function_callbacks *callbacks)
{
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    bool nullable = false;
    bool result_nullable = false;
    bool matched_string_encoding = false;
    bool matched_slice_string = false;
    struct mylite_expression_descriptor_string_callbacks string_callbacks = {0};
    int status = validate_function_descriptor_callbacks(callbacks);

    if (status != MYLITE_OK) {
        return status;
    }
    string_callbacks.infer_expression_descriptor = callbacks->infer_expression_descriptor;
    if (!mylite_expression_is_supported_function_call(expression)) {
        return MYLITE_UNSUPPORTED;
    }
    status =
        infer_default_function_descriptor(database, plan, expression, out_descriptor, callbacks);
    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }
    status = infer_function_arguments_nullable(database, plan, arguments, &nullable, callbacks);
    if (status != MYLITE_OK) {
        return status;
    }
    result_nullable = mylite_expression_descriptor_function_result_nullable(nullable, value);

    if (infer_common_scalar_function_descriptor(database, name, nullable, result_nullable,
                                                out_descriptor)) {
        return MYLITE_OK;
    }
    status = infer_temporal_function_descriptor(database, plan, expression, value, out_descriptor,
                                                callbacks);
    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }
    status = infer_variadic_scalar_function_descriptor(database, plan, expression, value,
                                                       result_nullable, out_descriptor, callbacks);
    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }
    status = mylite_expression_descriptor_infer_string_encoding_function(
        database, plan, expression, out_descriptor, &string_callbacks, &matched_string_encoding);
    if (status != MYLITE_OK || matched_string_encoding) {
        return status;
    }
    status = mylite_expression_descriptor_infer_slice_string_function(
        database, plan, expression, value, nullable, out_descriptor, &string_callbacks,
        &matched_slice_string);
    if (status != MYLITE_OK || matched_slice_string) {
        return status;
    }
    if (mylite_expression_descriptor_infer_text_function(database, name, value, result_nullable,
                                                         out_descriptor)) {
        return MYLITE_OK;
    }
    if (mylite_expression_descriptor_infer_fixed_integer_function(name, result_nullable,
                                                                  out_descriptor)) {
        return MYLITE_OK;
    }
    if (mylite_expression_descriptor_infer_code_search_function(name, result_nullable,
                                                                out_descriptor)) {
        return MYLITE_OK;
    }
    if (mylite_expression_descriptor_infer_list_index_function(name, result_nullable,
                                                               out_descriptor)) {
        return MYLITE_OK;
    }
    if (mylite_expression_descriptor_infer_scalar_numeric_function(name, value, result_nullable,
                                                                   out_descriptor)) {
        return MYLITE_OK;
    }

    *out_descriptor = mylite_expression_descriptor_from_value(value);
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_temporal_function_descriptor(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, const struct mylite_expression_value *value,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_function_callbacks *callbacks)
{
    struct mylite_expression_descriptor_temporal_callbacks temporal_callbacks = {
        .infer_expression_descriptor = callbacks->infer_expression_descriptor,
    };

    return mylite_expression_descriptor_infer_temporal_function(
        database, plan, expression, value, out_descriptor, &temporal_callbacks);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_default_function_descriptor(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_function_callbacks *callbacks)
{
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    const struct mylite_sql_ast_node *identifier =
        arguments == NULL ? NULL : mylite_ast_child_at(arguments, 0U);

    if (!function_name_is_default(name)) {
        return MYLITE_UNSUPPORTED;
    }
    if (identifier == NULL || (identifier->kind != MYLITE_SQL_AST_IDENTIFIER &&
                               identifier->kind != MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) {
        return MYLITE_UNSUPPORTED;
    }
    if (plan == NULL) {
        return set_default_function_unknown_column_error(database, identifier);
    }
    return callbacks->infer_expression_descriptor(database, plan, identifier, NULL, out_descriptor);
}

static bool function_name_is_default(const struct mylite_sql_ast_node *name)
{
    return name != NULL && mylite_span_equal_ci(name->span, "DEFAULT");
}

static int set_default_function_unknown_column_error(mylite_db *database,
                                                     const struct mylite_sql_ast_node *identifier)
{
    char *reference = NULL;
    int status = MYLITE_OK;

    if (database == NULL || identifier == NULL) {
        return MYLITE_UNSUPPORTED;
    }

    reference = mylite_copy_span_text(identifier->span.text, identifier->span.length);
    if (reference == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_set_error_message_parts(database, "Unknown column '", reference,
                                                        "' in 'field list'");
    free(reference);
    if (status == MYLITE_OK) {
        status = mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_BAD_FIELD_ERROR,
                                                 mylite_error_message(database));
    }
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static bool infer_common_scalar_function_descriptor(mylite_db *database,
                                                    const struct mylite_sql_ast_node *name,
                                                    bool arguments_nullable, bool result_nullable,
                                                    struct mylite_field_descriptor *out_descriptor)
{
    if (mylite_expression_descriptor_infer_session_or_inet_function(database, name,
                                                                    out_descriptor)) {
        return true;
    }
    if (mylite_expression_descriptor_infer_strcmp_function(name, result_nullable, out_descriptor)) {
        return true;
    }
    if (mylite_function_name_is_regexp_like(name)) {
        *out_descriptor = mylite_expression_descriptor_boolean(result_nullable);
        return true;
    }
    if (mylite_expression_descriptor_infer_uuid_function(database, name, out_descriptor)) {
        return true;
    }
    if (mylite_expression_descriptor_infer_math_function(name, result_nullable, out_descriptor)) {
        return true;
    }
    if (mylite_expression_descriptor_infer_temporal_scalar_function(name, arguments_nullable,
                                                                    out_descriptor)) {
        return true;
    }
    return mylite_expression_descriptor_infer_base_conversion_function(database, name,
                                                                       out_descriptor);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_variadic_scalar_function_descriptor(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, const struct mylite_expression_value *value,
    bool result_nullable, struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_function_callbacks *callbacks)
{
    struct mylite_expression_descriptor_numeric_callbacks numeric_callbacks = {
        .infer_expression_descriptor = callbacks->infer_expression_descriptor,
    };
    struct mylite_expression_descriptor_compare_callbacks compare_callbacks = {
        .infer_expression_descriptor = callbacks->infer_expression_descriptor,
    };
    int status = mylite_expression_descriptor_infer_numeric_variadic_function(
        database, plan, expression, value, result_nullable, out_descriptor, &numeric_callbacks);

    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }
    status = mylite_expression_descriptor_infer_greatest_least_function(
        database, plan, expression, result_nullable, out_descriptor, &compare_callbacks);
    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }
    return mylite_expression_descriptor_infer_char_function(database, expression, out_descriptor);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_function_arguments_nullable(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *arguments, bool *out_nullable,
    const struct mylite_expression_descriptor_function_callbacks *callbacks)
{
    bool nullable = false;

    for (const struct mylite_sql_ast_node *child = arguments == NULL ? NULL
                                                                     : arguments->first_child;
         child != NULL; child = child->next_sibling) {
        struct mylite_field_descriptor child_descriptor = mylite_expression_descriptor_defaults();
        int status =
            callbacks->infer_expression_descriptor(database, plan, child, NULL, &child_descriptor);

        if (status != MYLITE_OK) {
            return status;
        }
        if (mylite_expression_descriptor_is_nullable(&child_descriptor)) {
            nullable = true;
        }
    }
    *out_nullable = nullable;
    return MYLITE_OK;
}

static int validate_function_descriptor_callbacks(
    const struct mylite_expression_descriptor_function_callbacks *callbacks)
{
    if (callbacks == NULL || callbacks->infer_expression_descriptor == NULL) {
        return MYLITE_MISUSE;
    }
    return MYLITE_OK;
}
