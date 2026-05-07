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
#include "mylite_metadata_constants.h"
#include "mylite_span.h"
#include "sql/mylite_ast.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

static int infer_temporal_function_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *value,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_function_callbacks *callbacks
);

static int infer_default_function_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_function_callbacks *callbacks
);

static bool function_name_is_default(const struct mylite_sql_ast_node *name);

static int set_default_function_unknown_column_error(
    mylite_db *database,
    const struct mylite_sql_ast_node *identifier
);

static int infer_conditional_function_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_function_callbacks *callbacks
);

static int infer_json_function_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_function_callbacks *callbacks
);

static bool function_name_is_if(const struct mylite_sql_ast_node *name);

static bool function_name_is_ifnull(const struct mylite_sql_ast_node *name);

static bool function_name_is_nullif(const struct mylite_sql_ast_node *name);

static bool function_name_is_coalesce(const struct mylite_sql_ast_node *name);

static int infer_if_function_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *arguments,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_function_callbacks *callbacks
);

static int infer_ifnull_function_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *arguments,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_function_callbacks *callbacks
);

static int infer_nullif_function_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *arguments,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_function_callbacks *callbacks
);

static int infer_coalesce_function_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *arguments,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_function_callbacks *callbacks
);

static int infer_conditional_result_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *first_result,
    size_t result_count,
    bool nullable,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_function_callbacks *callbacks
);

static int conditional_results_use_string_domain(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *first_result,
    size_t result_count,
    bool *out_string_domain,
    const struct mylite_expression_descriptor_function_callbacks *callbacks
);

static int infer_conditional_string_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *first_result,
    size_t result_count,
    bool nullable,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_function_callbacks *callbacks
);

static uint64_t conditional_string_argument_length(
    mylite_db *database,
    const struct mylite_field_descriptor *descriptor
);

static int infer_conditional_numeric_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *first_result,
    size_t result_count,
    bool nullable,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_function_callbacks *callbacks
);

static void aggregate_conditional_numeric_descriptor(
    const struct mylite_field_descriptor *argument,
    struct mylite_field_descriptor *aggregate,
    bool *out_saw_nonnull
);

static bool conditional_descriptor_has_supported_numeric_result(
    const struct mylite_field_descriptor *descriptor
);

static bool conditional_descriptor_has_integer_result(
    const struct mylite_field_descriptor *descriptor
);

static bool infer_common_scalar_function_descriptor(
    mylite_db *database,
    const struct mylite_sql_ast_node *name,
    bool arguments_nullable,
    bool result_nullable,
    struct mylite_field_descriptor *out_descriptor
);

static struct mylite_field_descriptor json_text_descriptor(mylite_db *database, uint64_t length);

static struct mylite_field_descriptor json_document_descriptor(mylite_db *database, bool nullable);

static struct mylite_field_descriptor json_unquote_json_descriptor(mylite_db *database);

static uint64_t json_document_result_length(mylite_db *database);

static uint64_t json_unquote_json_result_length(mylite_db *database);

static uint64_t json_quote_result_length(const struct mylite_field_descriptor *argument);

static int infer_json_argument_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_function_callbacks *callbacks
);

static int infer_variadic_scalar_function_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *value,
    bool result_nullable,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_function_callbacks *callbacks
);

static int infer_case_conversion_function_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_function_callbacks *callbacks
);

static bool function_name_is_case_conversion(const struct mylite_sql_ast_node *name);

static struct mylite_field_descriptor case_conversion_result_descriptor(
    mylite_db *database,
    const struct mylite_field_descriptor *argument
);

static uint64_t case_conversion_result_length(
    mylite_db *database,
    const struct mylite_field_descriptor *argument
);

static int case_conversion_result_type(uint64_t length);

static int infer_function_arguments_nullable(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *arguments,
    bool *out_nullable,
    const struct mylite_expression_descriptor_function_callbacks *callbacks
);

static int infer_function_arguments_all_nullable(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *arguments,
    bool *out_nullable,
    const struct mylite_expression_descriptor_function_callbacks *callbacks
);

static int validate_function_descriptor_callbacks(
    const struct mylite_expression_descriptor_function_callbacks *callbacks
);

// NOLINTNEXTLINE(misc-no-recursion)
int mylite_expression_descriptor_infer_function_expression(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *value,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_function_callbacks *callbacks
) {
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

    if (mylite_expression_descriptor_infer_hash_function(
            database,
            expression,
            value,
            out_descriptor
        )) {
        return MYLITE_OK;
    }
    status = infer_conditional_function_descriptor(
        database,
        plan,
        expression,
        out_descriptor,
        callbacks
    );
    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }
    status = infer_json_function_descriptor(database, plan, expression, out_descriptor, callbacks);
    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }
    if (infer_common_scalar_function_descriptor(
            database,
            name,
            nullable,
            result_nullable,
            out_descriptor
        )) {
        return MYLITE_OK;
    }
    status = infer_temporal_function_descriptor(
        database,
        plan,
        expression,
        value,
        out_descriptor,
        callbacks
    );
    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }
    status = infer_variadic_scalar_function_descriptor(
        database,
        plan,
        expression,
        value,
        result_nullable,
        out_descriptor,
        callbacks
    );
    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }
    status = mylite_expression_descriptor_infer_string_encoding_function(
        database,
        plan,
        expression,
        out_descriptor,
        &string_callbacks,
        &matched_string_encoding
    );
    if (status != MYLITE_OK || matched_string_encoding) {
        return status;
    }
    status = mylite_expression_descriptor_infer_slice_string_function(
        database,
        plan,
        expression,
        value,
        nullable,
        out_descriptor,
        &string_callbacks,
        &matched_slice_string
    );
    if (status != MYLITE_OK || matched_slice_string) {
        return status;
    }
    status = infer_case_conversion_function_descriptor(
        database,
        plan,
        expression,
        out_descriptor,
        callbacks
    );
    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }
    if (mylite_expression_descriptor_infer_text_function(
            database,
            name,
            value,
            result_nullable,
            out_descriptor
        )) {
        return MYLITE_OK;
    }
    if (mylite_expression_descriptor_infer_fixed_integer_function(
            name,
            result_nullable,
            out_descriptor
        )) {
        return MYLITE_OK;
    }
    if (mylite_expression_descriptor_infer_code_search_function(
            name,
            result_nullable,
            out_descriptor
        )) {
        return MYLITE_OK;
    }
    if (mylite_expression_descriptor_infer_list_index_function(
            name,
            result_nullable,
            out_descriptor
        )) {
        return MYLITE_OK;
    }
    if (mylite_expression_descriptor_infer_scalar_numeric_function(
            name,
            value,
            result_nullable,
            out_descriptor
        )) {
        return MYLITE_OK;
    }

    *out_descriptor = mylite_expression_descriptor_from_value(value);
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_case_conversion_function_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_function_callbacks *callbacks
) {
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    const struct mylite_sql_ast_node *argument = mylite_ast_child_at(arguments, 0U);
    struct mylite_field_descriptor argument_descriptor = mylite_expression_descriptor_defaults();
    int status = MYLITE_OK;

    if (!function_name_is_case_conversion(name)) {
        return MYLITE_UNSUPPORTED;
    }
    if (argument == NULL) {
        return MYLITE_UNSUPPORTED;
    }

    status =
        callbacks
            ->infer_expression_descriptor(database, plan, argument, NULL, &argument_descriptor);
    if (status != MYLITE_OK) {
        return status;
    }
    *out_descriptor = case_conversion_result_descriptor(database, &argument_descriptor);
    return MYLITE_OK;
}

static bool function_name_is_case_conversion(const struct mylite_sql_ast_node *name) {
    return name != NULL &&
           (mylite_span_equal_ci(name->span, "LOWER") ||
            mylite_span_equal_ci(name->span, "LCASE") ||
            mylite_span_equal_ci(name->span, "UPPER") || mylite_span_equal_ci(name->span, "UCASE"));
}

static struct mylite_field_descriptor case_conversion_result_descriptor(
    mylite_db *database,
    const struct mylite_field_descriptor *argument
) {
    uint64_t length = case_conversion_result_length(database, argument);
    unsigned int charset_id = mylite_expression_descriptor_connection_charset_id(database);
    unsigned int flags = 0U;

    if (argument != NULL &&
        (mylite_expression_descriptor_has_text_result(argument) ||
         argument->type == MYLITE_FIELD_TYPE_BLOB || argument->type == MYLITE_FIELD_TYPE_NULL)) {
        charset_id = argument->charset_id;
    }
    if (argument != NULL && ((argument->flags & MYLITE_FIELD_FLAG_BINARY) != 0U ||
                             argument->charset_id == mylite_mysql_binary_charset_id)) {
        flags |= MYLITE_FIELD_FLAG_BINARY;
    }

    return (struct mylite_field_descriptor){
        .type = case_conversion_result_type(length),
        .flags = flags,
        .length = length,
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = charset_id,
        .nullable = true,
    };
}

static uint64_t case_conversion_result_length(
    mylite_db *database,
    const struct mylite_field_descriptor *argument
) {
    uint64_t max_bytes_per_character =
        mylite_expression_descriptor_connection_character_max_length(database);

    if (argument == NULL || argument->type == MYLITE_FIELD_TYPE_NULL) {
        return 0U;
    }
    if ((argument->flags & MYLITE_FIELD_FLAG_NUM) != 0U) {
        if (max_bytes_per_character != 0U &&
            argument->length > UINT64_MAX / max_bytes_per_character) {
            return mylite_mysql_long_text_length;
        }
        return argument->length * max_bytes_per_character;
    }
    if (argument->type == MYLITE_FIELD_TYPE_BLOB &&
        argument->charset_id != mylite_mysql_binary_charset_id) {
        if (max_bytes_per_character != 0U &&
            argument->length > UINT64_MAX / max_bytes_per_character) {
            return mylite_mysql_long_text_length;
        }
        return argument->length * max_bytes_per_character;
    }
    return argument->length;
}

static int case_conversion_result_type(uint64_t length) {
    if (length > mylite_mysql_medium_text_length) {
        return MYLITE_FIELD_TYPE_LONG_BLOB;
    }
    if (length > mylite_mysql_text_length) {
        return MYLITE_FIELD_TYPE_MEDIUM_BLOB;
    }
    return MYLITE_FIELD_TYPE_VAR_STRING;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_temporal_function_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *value,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_function_callbacks *callbacks
) {
    struct mylite_expression_descriptor_temporal_callbacks temporal_callbacks = {
        .infer_expression_descriptor = callbacks->infer_expression_descriptor,
    };

    return mylite_expression_descriptor_infer_temporal_function(
        database,
        plan,
        expression,
        value,
        out_descriptor,
        &temporal_callbacks
    );
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_default_function_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_function_callbacks *callbacks
) {
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

static bool function_name_is_default(const struct mylite_sql_ast_node *name) {
    return name != NULL && mylite_span_equal_ci(name->span, "DEFAULT");
}

static int set_default_function_unknown_column_error(
    mylite_db *database,
    const struct mylite_sql_ast_node *identifier
) {
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
    status = mylite_diagnostics_set_error_message_parts(
        database,
        "Unknown column '",
        reference,
        "' in 'field list'"
    );
    free(reference);
    if (status == MYLITE_OK) {
        status = mylite_diagnostics_append_error(
            database,
            MYLITE_MYSQL_ER_BAD_FIELD_ERROR,
            mylite_error_message(database)
        );
    }
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_conditional_function_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_function_callbacks *callbacks
) {
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);

    if (function_name_is_if(name)) {
        return infer_if_function_descriptor(database, plan, arguments, out_descriptor, callbacks);
    }
    if (function_name_is_ifnull(name)) {
        return infer_ifnull_function_descriptor(
            database,
            plan,
            arguments,
            out_descriptor,
            callbacks
        );
    }
    if (function_name_is_nullif(name)) {
        return infer_nullif_function_descriptor(
            database,
            plan,
            arguments,
            out_descriptor,
            callbacks
        );
    }
    if (function_name_is_coalesce(name)) {
        return infer_coalesce_function_descriptor(
            database,
            plan,
            arguments,
            out_descriptor,
            callbacks
        );
    }
    return MYLITE_UNSUPPORTED;
}

static bool function_name_is_if(const struct mylite_sql_ast_node *name) {
    return name != NULL && mylite_span_equal_ci(name->span, "IF");
}

static bool function_name_is_ifnull(const struct mylite_sql_ast_node *name) {
    return name != NULL && mylite_span_equal_ci(name->span, "IFNULL");
}

static bool function_name_is_nullif(const struct mylite_sql_ast_node *name) {
    return name != NULL && mylite_span_equal_ci(name->span, "NULLIF");
}

static bool function_name_is_coalesce(const struct mylite_sql_ast_node *name) {
    return name != NULL && mylite_span_equal_ci(name->span, "COALESCE");
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_if_function_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *arguments,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_function_callbacks *callbacks
) {
    const struct mylite_sql_ast_node *true_result =
        arguments == NULL ? NULL : mylite_ast_child_at(arguments, 1U);
    const struct mylite_sql_ast_node *false_result =
        arguments == NULL ? NULL : mylite_ast_child_at(arguments, 2U);
    struct mylite_field_descriptor true_descriptor = mylite_expression_descriptor_defaults();
    struct mylite_field_descriptor false_descriptor = mylite_expression_descriptor_defaults();
    int status =
        callbacks->infer_expression_descriptor(database, plan, true_result, NULL, &true_descriptor);
    bool nullable = true;

    if (status != MYLITE_OK) {
        return status;
    }
    status =
        callbacks
            ->infer_expression_descriptor(database, plan, false_result, NULL, &false_descriptor);
    if (status != MYLITE_OK) {
        return status;
    }
    nullable = mylite_expression_descriptor_is_nullable(&true_descriptor) ||
               mylite_expression_descriptor_is_nullable(&false_descriptor);
    return infer_conditional_result_descriptor(
        database,
        plan,
        true_result,
        2U,
        nullable,
        out_descriptor,
        callbacks
    );
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_ifnull_function_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *arguments,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_function_callbacks *callbacks
) {
    bool nullable = true;
    int status =
        infer_function_arguments_all_nullable(database, plan, arguments, &nullable, callbacks);

    if (status != MYLITE_OK) {
        return status;
    }
    return infer_conditional_result_descriptor(
        database,
        plan,
        arguments == NULL ? NULL : arguments->first_child,
        2U,
        nullable,
        out_descriptor,
        callbacks
    );
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_nullif_function_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *arguments,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_function_callbacks *callbacks
) {
    const struct mylite_sql_ast_node *first_result =
        arguments == NULL ? NULL : arguments->first_child;

    return infer_conditional_result_descriptor(
        database,
        plan,
        first_result,
        1U,
        true,
        out_descriptor,
        callbacks
    );
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_coalesce_function_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *arguments,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_function_callbacks *callbacks
) {
    bool nullable = true;
    int status =
        infer_function_arguments_all_nullable(database, plan, arguments, &nullable, callbacks);

    if (status != MYLITE_OK) {
        return status;
    }
    return infer_conditional_result_descriptor(
        database,
        plan,
        arguments == NULL ? NULL : arguments->first_child,
        0U,
        nullable,
        out_descriptor,
        callbacks
    );
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_conditional_result_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *first_result,
    size_t result_count,
    bool nullable,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_function_callbacks *callbacks
) {
    bool string_domain = false;
    int status = conditional_results_use_string_domain(
        database,
        plan,
        first_result,
        result_count,
        &string_domain,
        callbacks
    );

    if (status != MYLITE_OK) {
        return status;
    }
    if (string_domain) {
        return infer_conditional_string_descriptor(
            database,
            plan,
            first_result,
            result_count,
            nullable,
            out_descriptor,
            callbacks
        );
    }
    return infer_conditional_numeric_descriptor(
        database,
        plan,
        first_result,
        result_count,
        nullable,
        out_descriptor,
        callbacks
    );
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_json_function_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_function_callbacks *callbacks
) {
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(expression, 0U);
    struct mylite_field_descriptor argument = mylite_expression_descriptor_defaults();
    int status = MYLITE_OK;

    if (mylite_function_name_is_json_valid(name) ||
        mylite_function_name_is_json_contains_path(name) ||
        mylite_function_name_is_json_length(name)) {
        *out_descriptor = mylite_expression_descriptor_signed_longlong(true);
        out_descriptor->length = mylite_mysql_signed_longlong_display_length;
        return MYLITE_OK;
    }
    if (mylite_function_name_is_json_type(name)) {
        *out_descriptor = json_text_descriptor(
            database,
            17U * mylite_expression_descriptor_connection_character_max_length(database)
        );
        return MYLITE_OK;
    }
    if (mylite_function_name_is_json_quote(name)) {
        status = infer_json_argument_descriptor(database, plan, expression, &argument, callbacks);
        if (status != MYLITE_OK) {
            return status;
        }
        *out_descriptor = json_text_descriptor(database, json_quote_result_length(&argument));
        return MYLITE_OK;
    }
    if (mylite_function_name_is_json_unquote(name)) {
        status = infer_json_argument_descriptor(database, plan, expression, &argument, callbacks);
        if (status != MYLITE_OK) {
            return status;
        }
        *out_descriptor = argument.type == MYLITE_FIELD_TYPE_JSON
                              ? json_unquote_json_descriptor(database)
                              : json_text_descriptor(database, argument.length);
        return MYLITE_OK;
    }
    if (mylite_function_name_is_json_creation(name) || mylite_function_name_is_json_extract(name) ||
        mylite_function_name_is_json_keys(name)) {
        *out_descriptor = json_document_descriptor(database, true);
        return MYLITE_OK;
    }
    return MYLITE_UNSUPPORTED;
}

static struct mylite_field_descriptor json_text_descriptor(mylite_db *database, uint64_t length) {
    struct mylite_field_descriptor descriptor = {
        .type = MYLITE_FIELD_TYPE_VAR_STRING,
        .flags = MYLITE_FIELD_FLAG_BINARY,
        .length = length,
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = mylite_expression_descriptor_connection_charset_id(database),
        .nullable = true,
    };

    mylite_field_descriptor_set_nullable(&descriptor, true);
    return descriptor;
}

static struct mylite_field_descriptor json_document_descriptor(mylite_db *database, bool nullable) {
    struct mylite_field_descriptor descriptor = {
        .type = MYLITE_FIELD_TYPE_JSON,
        .flags = MYLITE_FIELD_FLAG_BINARY,
        .length = json_document_result_length(database),
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = mylite_expression_descriptor_connection_charset_id(database),
        .nullable = nullable,
    };

    mylite_field_descriptor_set_nullable(&descriptor, nullable);
    return descriptor;
}

static struct mylite_field_descriptor json_unquote_json_descriptor(mylite_db *database) {
    struct mylite_field_descriptor descriptor = {
        .type = MYLITE_FIELD_TYPE_LONG_BLOB,
        .flags = MYLITE_FIELD_FLAG_BINARY,
        .length = json_unquote_json_result_length(database),
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = mylite_expression_descriptor_connection_charset_id(database),
        .nullable = true,
    };

    mylite_field_descriptor_set_nullable(&descriptor, true);
    return descriptor;
}

static uint64_t json_document_result_length(mylite_db *database) {
    uint64_t max_bytes_per_character =
        mylite_expression_descriptor_connection_character_max_length(database);

    if (max_bytes_per_character > UINT64_MAX / mylite_mysql_json_document_length) {
        return mylite_mysql_long_text_length;
    }
    return mylite_mysql_json_document_length * max_bytes_per_character;
}

static uint64_t json_unquote_json_result_length(mylite_db *database) {
    uint64_t max_bytes_per_character =
        mylite_expression_descriptor_connection_character_max_length(database);

    if (max_bytes_per_character > 1U) {
        return mylite_mysql_long_text_length;
    }
    return mylite_mysql_json_document_length * 4U;
}

static uint64_t json_quote_result_length(const struct mylite_field_descriptor *argument) {
    uint64_t length = argument == NULL ? 0U : argument->length;

    if (length > (mylite_mysql_long_text_length - 8U) / 6U) {
        return mylite_mysql_long_text_length;
    }
    return (length * 6U) + 8U;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_json_argument_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_function_callbacks *callbacks
) {
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    const struct mylite_sql_ast_node *argument =
        arguments == NULL ? NULL : mylite_ast_child_at(arguments, 0U);

    return callbacks->infer_expression_descriptor(database, plan, argument, NULL, out_descriptor);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int conditional_results_use_string_domain(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *first_result,
    size_t result_count,
    bool *out_string_domain,
    const struct mylite_expression_descriptor_function_callbacks *callbacks
) {
    size_t processed = 0U;

    *out_string_domain = false;
    for (const struct mylite_sql_ast_node *result = first_result; result != NULL;
         result = result->next_sibling) {
        struct mylite_field_descriptor descriptor = mylite_expression_descriptor_defaults();
        int status = MYLITE_OK;

        if (result_count != 0U && processed >= result_count) {
            break;
        }
        ++processed;
        status = callbacks->infer_expression_descriptor(database, plan, result, NULL, &descriptor);
        if (status != MYLITE_OK) {
            return status;
        }
        if (mylite_expression_descriptor_has_text_result(&descriptor) ||
            descriptor.type == MYLITE_FIELD_TYPE_BLOB) {
            *out_string_domain = true;
            return MYLITE_OK;
        }
    }
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_conditional_string_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *first_result,
    size_t result_count,
    bool nullable,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_function_callbacks *callbacks
) {
    uint64_t length = 0U;
    size_t processed = 0U;
    bool saw_nonnull = false;

    for (const struct mylite_sql_ast_node *result = first_result; result != NULL;
         result = result->next_sibling) {
        struct mylite_field_descriptor descriptor = mylite_expression_descriptor_defaults();
        int status = MYLITE_OK;

        if (result_count != 0U && processed >= result_count) {
            break;
        }
        ++processed;
        status = callbacks->infer_expression_descriptor(database, plan, result, NULL, &descriptor);
        if (status != MYLITE_OK) {
            return status;
        }
        if (descriptor.type == MYLITE_FIELD_TYPE_NULL) {
            continue;
        }
        if (!mylite_expression_descriptor_has_text_result(&descriptor) &&
            descriptor.type != MYLITE_FIELD_TYPE_BLOB &&
            !mylite_expression_descriptor_has_numeric_result(&descriptor)) {
            return MYLITE_UNSUPPORTED;
        }
        saw_nonnull = true;
        length = mylite_expression_descriptor_max_u64(
            length,
            conditional_string_argument_length(database, &descriptor)
        );
    }

    if (!saw_nonnull) {
        *out_descriptor = mylite_expression_descriptor_null();
        return MYLITE_OK;
    }
    *out_descriptor = (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_VAR_STRING,
        .flags = 0U,
        .length = length,
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = mylite_expression_descriptor_connection_charset_id(database),
        .nullable = nullable,
    };
    mylite_field_descriptor_set_nullable(out_descriptor, nullable);
    return MYLITE_OK;
}

static uint64_t conditional_string_argument_length(
    mylite_db *database,
    const struct mylite_field_descriptor *descriptor
) {
    uint64_t max_bytes_per_character =
        mylite_expression_descriptor_connection_character_max_length(database);

    if (descriptor == NULL || descriptor->type == MYLITE_FIELD_TYPE_NULL) {
        return 0U;
    }
    if (mylite_expression_descriptor_has_text_result(descriptor) ||
        descriptor->type == MYLITE_FIELD_TYPE_BLOB) {
        return descriptor->length;
    }
    if (max_bytes_per_character != 0U &&
        descriptor->length > UINT64_MAX / max_bytes_per_character) {
        return mylite_mysql_long_text_length;
    }
    return descriptor->length * max_bytes_per_character;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_conditional_numeric_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *first_result,
    size_t result_count,
    bool nullable,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_function_callbacks *callbacks
) {
    struct mylite_field_descriptor aggregate = mylite_expression_descriptor_defaults();
    size_t processed = 0U;
    bool saw_nonnull = false;

    for (const struct mylite_sql_ast_node *result = first_result; result != NULL;
         result = result->next_sibling) {
        struct mylite_field_descriptor descriptor = mylite_expression_descriptor_defaults();
        int status = MYLITE_OK;

        if (result_count != 0U && processed >= result_count) {
            break;
        }
        ++processed;
        status = callbacks->infer_expression_descriptor(database, plan, result, NULL, &descriptor);
        if (status != MYLITE_OK) {
            return status;
        }
        if (!conditional_descriptor_has_supported_numeric_result(&descriptor)) {
            return MYLITE_UNSUPPORTED;
        }
        aggregate_conditional_numeric_descriptor(&descriptor, &aggregate, &saw_nonnull);
    }

    if (!saw_nonnull) {
        *out_descriptor = mylite_expression_descriptor_null();
        return MYLITE_OK;
    }
    *out_descriptor = aggregate;
    mylite_field_descriptor_set_nullable(out_descriptor, nullable);
    return MYLITE_OK;
}

static void aggregate_conditional_numeric_descriptor(
    const struct mylite_field_descriptor *argument,
    struct mylite_field_descriptor *aggregate,
    bool *out_saw_nonnull
) {
    if (argument == NULL || argument->type == MYLITE_FIELD_TYPE_NULL) {
        return;
    }

    if (!*out_saw_nonnull) {
        *aggregate = *argument;
        aggregate->flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM;
        if ((argument->flags & MYLITE_FIELD_FLAG_UNSIGNED) != 0U) {
            aggregate->flags |= MYLITE_FIELD_FLAG_UNSIGNED;
        }
        aggregate->charset_id = mylite_mysql_binary_charset_id;
        aggregate->max_length = 0U;
        *out_saw_nonnull = true;
        return;
    }

    if (mylite_expression_descriptor_has_double_result(argument) ||
        argument->type == MYLITE_FIELD_TYPE_FLOAT ||
        mylite_expression_descriptor_has_double_result(aggregate) ||
        aggregate->type == MYLITE_FIELD_TYPE_FLOAT) {
        *aggregate = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_DOUBLE,
            .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
            .length = mylite_expression_descriptor_max_u64(
                mylite_expression_descriptor_max_u64(aggregate->length, argument->length),
                mylite_mysql_double_display_length
            ),
            .decimals = mylite_mysql_not_fixed_decimals,
            .charset_id = mylite_mysql_binary_charset_id,
            .nullable = true,
        };
        return;
    }

    if (mylite_expression_descriptor_has_decimal_result(argument) ||
        mylite_expression_descriptor_has_decimal_result(aggregate)) {
        *aggregate = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_NEWDECIMAL,
            .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
            .length = mylite_expression_descriptor_max_u64(aggregate->length, argument->length),
            .decimals =
                aggregate->decimals > argument->decimals ? aggregate->decimals : argument->decimals,
            .charset_id = mylite_mysql_binary_charset_id,
            .nullable = true,
        };
        return;
    }

    if (((aggregate->flags & MYLITE_FIELD_FLAG_UNSIGNED) != 0U) !=
        ((argument->flags & MYLITE_FIELD_FLAG_UNSIGNED) != 0U)) {
        aggregate->type = MYLITE_FIELD_TYPE_LONGLONG;
    } else if (aggregate->type != argument->type) {
        aggregate->type = MYLITE_FIELD_TYPE_LONGLONG;
    }
    aggregate->flags |= MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM;
    if (((aggregate->flags & MYLITE_FIELD_FLAG_UNSIGNED) == 0U) ||
        ((argument->flags & MYLITE_FIELD_FLAG_UNSIGNED) == 0U)) {
        aggregate->flags &= ~(unsigned int)MYLITE_FIELD_FLAG_UNSIGNED;
    }
    aggregate->charset_id = mylite_mysql_binary_charset_id;
    aggregate->length = mylite_expression_descriptor_max_u64(aggregate->length, argument->length);
    aggregate->decimals = 0U;
}

static bool conditional_descriptor_has_supported_numeric_result(
    const struct mylite_field_descriptor *descriptor
) {
    if (descriptor == NULL) {
        return false;
    }
    if (descriptor->type == MYLITE_FIELD_TYPE_NULL) {
        return true;
    }
    if (mylite_expression_descriptor_has_decimal_result(descriptor) ||
        mylite_expression_descriptor_has_double_result(descriptor) ||
        descriptor->type == MYLITE_FIELD_TYPE_FLOAT) {
        return true;
    }
    return conditional_descriptor_has_integer_result(descriptor);
}

static bool conditional_descriptor_has_integer_result(
    const struct mylite_field_descriptor *descriptor
) {
    if (descriptor == NULL) {
        return false;
    }
    switch (descriptor->type) {
    case MYLITE_FIELD_TYPE_TINY:
    case MYLITE_FIELD_TYPE_SHORT:
    case MYLITE_FIELD_TYPE_LONG:
    case MYLITE_FIELD_TYPE_LONGLONG:
    case MYLITE_FIELD_TYPE_INT24:
    case MYLITE_FIELD_TYPE_YEAR:
        return true;
    default:
        return false;
    }
}

static bool infer_common_scalar_function_descriptor(
    mylite_db *database,
    const struct mylite_sql_ast_node *name,
    bool arguments_nullable,
    bool result_nullable,
    struct mylite_field_descriptor *out_descriptor
) {
    if (mylite_expression_descriptor_infer_session_or_inet_function(
            database,
            name,
            out_descriptor
        )) {
        return true;
    }
    if (mylite_expression_descriptor_infer_strcmp_function(name, result_nullable, out_descriptor)) {
        return true;
    }
    if (mylite_function_name_is_regexp_like(name)) {
        *out_descriptor = mylite_expression_descriptor_boolean(result_nullable);
        return true;
    }
    if (mylite_expression_descriptor_infer_regexp_scalar_function(
            database,
            name,
            result_nullable,
            out_descriptor
        )) {
        return true;
    }
    if (mylite_expression_descriptor_infer_uuid_function(database, name, out_descriptor)) {
        return true;
    }
    if (mylite_expression_descriptor_infer_math_function(name, result_nullable, out_descriptor)) {
        return true;
    }
    if (mylite_expression_descriptor_infer_temporal_scalar_function(
            name,
            arguments_nullable,
            out_descriptor
        )) {
        return true;
    }
    return mylite_expression_descriptor_infer_base_conversion_function(
        database,
        name,
        out_descriptor
    );
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_variadic_scalar_function_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *value,
    bool result_nullable,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_function_callbacks *callbacks
) {
    struct mylite_expression_descriptor_numeric_callbacks numeric_callbacks = {
        .infer_expression_descriptor = callbacks->infer_expression_descriptor,
    };
    struct mylite_expression_descriptor_compare_callbacks compare_callbacks = {
        .infer_expression_descriptor = callbacks->infer_expression_descriptor,
    };
    int status = mylite_expression_descriptor_infer_numeric_variadic_function(
        database,
        plan,
        expression,
        value,
        result_nullable,
        out_descriptor,
        &numeric_callbacks
    );

    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }
    status = mylite_expression_descriptor_infer_greatest_least_function(
        database,
        plan,
        expression,
        result_nullable,
        out_descriptor,
        &compare_callbacks
    );
    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }
    return mylite_expression_descriptor_infer_char_function(database, expression, out_descriptor);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_function_arguments_nullable(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *arguments,
    bool *out_nullable,
    const struct mylite_expression_descriptor_function_callbacks *callbacks
) {
    bool nullable = false;

    for (const struct mylite_sql_ast_node *child = arguments == NULL ? NULL
                                                                     : arguments->first_child;
         child != NULL;
         child = child->next_sibling) {
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

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_function_arguments_all_nullable(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *arguments,
    bool *out_nullable,
    const struct mylite_expression_descriptor_function_callbacks *callbacks
) {
    bool nullable = true;

    for (const struct mylite_sql_ast_node *child = arguments == NULL ? NULL
                                                                     : arguments->first_child;
         child != NULL;
         child = child->next_sibling) {
        struct mylite_field_descriptor child_descriptor = mylite_expression_descriptor_defaults();
        int status =
            callbacks->infer_expression_descriptor(database, plan, child, NULL, &child_descriptor);

        if (status != MYLITE_OK) {
            return status;
        }
        if (!mylite_expression_descriptor_is_nullable(&child_descriptor)) {
            nullable = false;
        }
    }
    *out_nullable = nullable;
    return MYLITE_OK;
}

static int validate_function_descriptor_callbacks(
    const struct mylite_expression_descriptor_function_callbacks *callbacks
) {
    if (callbacks == NULL || callbacks->infer_expression_descriptor == NULL) {
        return MYLITE_MISUSE;
    }
    return MYLITE_OK;
}
