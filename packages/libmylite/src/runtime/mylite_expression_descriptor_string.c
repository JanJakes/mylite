#include "mylite_expression_descriptor_string.h"

#include "mylite_expression.h"
#include "mylite_expression_descriptor.h"
#include "mylite_expression_descriptor_string_quote.h"
#include "mylite_function_names.h"
#include "mylite_metadata_constants.h"
#include "mylite_span.h"
#include "sql/mylite_ast.h"

#include <stdint.h>
#include <stdlib.h>

static int infer_elt_function_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_string_callbacks *callbacks
);

static int infer_make_set_function_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_string_callbacks *callbacks
);

static uint64_t insert_function_result_length(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_descriptor_string_callbacks *callbacks
);

static uint64_t make_set_function_result_length(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_descriptor_string_callbacks *callbacks
);

static int make_set_function_members_are_all_null(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    bool *out_all_null,
    const struct mylite_expression_descriptor_string_callbacks *callbacks
);

static uint64_t make_set_all_null_result_length(const struct mylite_sql_ast_node *expression);

static uint64_t elt_function_result_length(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_descriptor_string_callbacks *callbacks
);

static uint64_t concat_function_result_length(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *arguments,
    const struct mylite_expression_descriptor_string_callbacks *callbacks
);

static uint64_t concat_ws_function_result_length(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *arguments,
    const struct mylite_expression_descriptor_string_callbacks *callbacks
);

static uint64_t elt_argument_result_length(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_descriptor_string_callbacks *callbacks
);

static uint64_t expression_text_display_length(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_descriptor_string_callbacks *callbacks
);

static uint64_t slice_string_function_result_length(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *value,
    const struct mylite_expression_descriptor_string_callbacks *callbacks
);

static uint64_t repeat_function_result_length(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *arguments,
    const struct mylite_expression_descriptor_string_callbacks *callbacks
);

static uint64_t padding_function_result_length(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *arguments,
    const struct mylite_expression_descriptor_string_callbacks *callbacks
);

static uint64_t add_text_display_lengths(uint64_t left, uint64_t right);

static uint64_t multiply_text_display_length(uint64_t length, uint64_t factor);

static bool function_name_is_padding(const struct mylite_sql_ast_node *name);

static bool nonnegative_integer_constant(
    const struct mylite_sql_ast_node *expression,
    uint64_t *out_value
);

static int infer_function_arguments_nullable(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *arguments,
    bool *out_nullable,
    const struct mylite_expression_descriptor_string_callbacks *callbacks
);

int mylite_expression_descriptor_infer_slice_string_function(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *value,
    bool nullable,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_string_callbacks *callbacks,
    bool *out_matched
) {
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(expression, 0U);

    if (callbacks == NULL || callbacks->infer_expression_descriptor == NULL ||
        out_matched == NULL) {
        return MYLITE_MISUSE;
    }

    *out_matched = mylite_function_name_has_slice_string_result(name);
    if (!*out_matched) {
        return MYLITE_OK;
    }
    if (mylite_function_name_is_elt(name)) {
        int status =
            infer_elt_function_descriptor(database, plan, expression, out_descriptor, callbacks);

        if (status != MYLITE_UNSUPPORTED) {
            return status;
        }
    }
    if (mylite_function_name_is_make_set(name)) {
        (void)value;
        (void)nullable;
        return infer_make_set_function_descriptor(
            database,
            plan,
            expression,
            out_descriptor,
            callbacks
        );
    }

    *out_descriptor = (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_VAR_STRING,
        .flags = 0U,
        .length = slice_string_function_result_length(database, plan, expression, value, callbacks),
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = mylite_expression_descriptor_connection_charset_id(database),
        .nullable = true,
    };
    mylite_field_descriptor_set_nullable(out_descriptor, true);
    return MYLITE_OK;
}

int mylite_expression_descriptor_infer_concat_function(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    bool nullable,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_string_callbacks *callbacks,
    bool *out_matched
) {
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    uint64_t length = 0U;

    if (callbacks == NULL || callbacks->infer_expression_descriptor == NULL ||
        out_matched == NULL) {
        return MYLITE_MISUSE;
    }

    *out_matched = mylite_function_name_is_concat(name);
    if (!*out_matched) {
        return MYLITE_OK;
    }

    length = concat_function_result_length(database, plan, arguments, callbacks);
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

static int infer_elt_function_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_string_callbacks *callbacks
) {
    bool all_members_null = false;
    int status = make_set_function_members_are_all_null(
        database,
        plan,
        expression,
        &all_members_null,
        callbacks
    );

    if (status != MYLITE_OK) {
        return status;
    }
    if (!all_members_null) {
        return MYLITE_UNSUPPORTED;
    }

    *out_descriptor = (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_VAR_STRING,
        .flags = MYLITE_FIELD_FLAG_BINARY,
        .length = 0U,
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = true,
    };
    mylite_field_descriptor_set_nullable(out_descriptor, true);
    return MYLITE_OK;
}

static int infer_make_set_function_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_string_callbacks *callbacks
) {
    bool all_members_null = false;
    int status = make_set_function_members_are_all_null(
        database,
        plan,
        expression,
        &all_members_null,
        callbacks
    );
    bool nullable = false;
    unsigned int flags = 0U;
    uint64_t length = 0U;
    unsigned int charset_id = mylite_expression_descriptor_connection_charset_id(database);

    if (status != MYLITE_OK) {
        return status;
    }
    status = infer_function_arguments_nullable(
        database,
        plan,
        mylite_ast_child_at(expression, 1U),
        &nullable,
        callbacks
    );
    if (status != MYLITE_OK) {
        return status;
    }

    if (all_members_null) {
        flags = MYLITE_FIELD_FLAG_BINARY;
        length = make_set_all_null_result_length(expression);
        charset_id = mylite_mysql_binary_charset_id;
    } else {
        length = make_set_function_result_length(database, plan, expression, callbacks);
    }

    *out_descriptor = (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_VAR_STRING,
        .flags = flags,
        .length = length,
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = charset_id,
        .nullable = nullable,
    };
    mylite_field_descriptor_set_nullable(out_descriptor, nullable);
    return MYLITE_OK;
}

static uint64_t insert_function_result_length(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_descriptor_string_callbacks *callbacks
) {
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    uint64_t source_length = expression_text_display_length(
        database,
        plan,
        mylite_ast_child_at(arguments, 0U),
        callbacks
    );
    uint64_t replacement_length = expression_text_display_length(
        database,
        plan,
        mylite_ast_child_at(arguments, 3U),
        callbacks
    );

    if (source_length > UINT64_MAX - replacement_length) {
        return mylite_mysql_long_text_length;
    }
    return source_length + replacement_length;
}

static uint64_t make_set_function_result_length(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_descriptor_string_callbacks *callbacks
) {
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    uint64_t separator_length =
        mylite_expression_descriptor_connection_character_max_length(database);
    uint64_t result = 0U;
    size_t member_count = 0U;

    for (const struct mylite_sql_ast_node *argument = arguments == NULL ||
                                                              arguments->first_child == NULL
                                                          ? NULL
                                                          : arguments->first_child->next_sibling;
         argument != NULL;
         argument = argument->next_sibling) {
        uint64_t length = elt_argument_result_length(database, plan, argument, callbacks);

        if (member_count != 0U) {
            if (result > UINT64_MAX - separator_length) {
                return mylite_mysql_long_text_length;
            }
            result += separator_length;
        }
        if (result > UINT64_MAX - length) {
            return mylite_mysql_long_text_length;
        }
        result += length;
        ++member_count;
    }
    return result;
}

static int make_set_function_members_are_all_null(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    bool *out_all_null,
    const struct mylite_expression_descriptor_string_callbacks *callbacks
) {
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    bool saw_member = false;

    for (const struct mylite_sql_ast_node *argument = arguments == NULL ||
                                                              arguments->first_child == NULL
                                                          ? NULL
                                                          : arguments->first_child->next_sibling;
         argument != NULL;
         argument = argument->next_sibling) {
        struct mylite_field_descriptor descriptor = mylite_expression_descriptor_defaults();
        int status =
            callbacks->infer_expression_descriptor(database, plan, argument, NULL, &descriptor);

        if (status != MYLITE_OK) {
            return status;
        }
        saw_member = true;
        if (descriptor.type != MYLITE_FIELD_TYPE_NULL) {
            *out_all_null = false;
            return MYLITE_OK;
        }
    }
    *out_all_null = saw_member;
    return MYLITE_OK;
}

static uint64_t make_set_all_null_result_length(const struct mylite_sql_ast_node *expression) {
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    size_t member_count = 0U;

    for (const struct mylite_sql_ast_node *argument = arguments == NULL ||
                                                              arguments->first_child == NULL
                                                          ? NULL
                                                          : arguments->first_child->next_sibling;
         argument != NULL;
         argument = argument->next_sibling) {
        ++member_count;
    }
    if (member_count <= 1U) {
        return 0U;
    }
    return (uint64_t)(member_count - 1U);
}

static uint64_t elt_function_result_length(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_descriptor_string_callbacks *callbacks
) {
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    uint64_t result = 0U;

    for (const struct mylite_sql_ast_node *argument = arguments == NULL ||
                                                              arguments->first_child == NULL
                                                          ? NULL
                                                          : arguments->first_child->next_sibling;
         argument != NULL;
         argument = argument->next_sibling) {
        uint64_t length = elt_argument_result_length(database, plan, argument, callbacks);

        result = mylite_expression_descriptor_max_u64(result, length);
    }
    return result;
}

static uint64_t concat_function_result_length(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *arguments,
    const struct mylite_expression_descriptor_string_callbacks *callbacks
) {
    uint64_t result = 0U;

    for (const struct mylite_sql_ast_node *argument = arguments == NULL ? NULL
                                                                        : arguments->first_child;
         argument != NULL;
         argument = argument->next_sibling) {
        uint64_t length = elt_argument_result_length(database, plan, argument, callbacks);

        result = add_text_display_lengths(result, length);
    }
    return result;
}

static uint64_t concat_ws_function_result_length(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *arguments,
    const struct mylite_expression_descriptor_string_callbacks *callbacks
) {
    const struct mylite_sql_ast_node *separator = arguments == NULL ? NULL : arguments->first_child;
    uint64_t separator_length = elt_argument_result_length(database, plan, separator, callbacks);
    uint64_t result = 0U;
    size_t value_count = 0U;

    for (const struct mylite_sql_ast_node *argument = separator == NULL ? NULL
                                                                        : separator->next_sibling;
         argument != NULL;
         argument = argument->next_sibling) {
        uint64_t length = elt_argument_result_length(database, plan, argument, callbacks);

        if (value_count != 0U) {
            result = add_text_display_lengths(result, separator_length);
        }
        result = add_text_display_lengths(result, length);
        ++value_count;
    }
    return result;
}

static uint64_t elt_argument_result_length(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_descriptor_string_callbacks *callbacks
) {
    struct mylite_field_descriptor descriptor = mylite_expression_descriptor_defaults();

    if (callbacks->infer_expression_descriptor(database, plan, expression, NULL, &descriptor) ==
        MYLITE_OK) {
        if (descriptor.type == MYLITE_FIELD_TYPE_NULL) {
            return 0U;
        }
        if (descriptor.charset_id == mylite_mysql_binary_charset_id) {
            uint64_t max_bytes_per_character =
                mylite_expression_descriptor_connection_character_max_length(database);

            return descriptor.length > UINT64_MAX / max_bytes_per_character
                       ? mylite_mysql_long_text_length
                       : descriptor.length * max_bytes_per_character;
        }
        return descriptor.length;
    }
    return expression_text_display_length(database, plan, expression, callbacks);
}

static uint64_t expression_text_display_length(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_descriptor_string_callbacks *callbacks
) {
    struct mylite_expression_warnings warnings = {0};
    struct mylite_expression_value value = {0};
    struct mylite_field_descriptor descriptor = mylite_expression_descriptor_defaults();
    uint64_t result = mylite_mysql_text_length;

    if (expression != NULL && mylite_expression_is_cacheable_no_table(expression) &&
        mylite_expression_eval(expression, &warnings, &value) == 0) {
        char *text = mylite_expression_value_to_text(&value);

        if (text == NULL) {
            result = 0U;
        } else {
            result = mylite_expression_descriptor_utf8_display_character_count(text) *
                     mylite_expression_descriptor_connection_character_max_length(database);
        }
        free(text);
        mylite_expression_value_deinit(&value);
        mylite_expression_warnings_deinit(&warnings);
        return result;
    }

    mylite_expression_value_deinit(&value);
    mylite_expression_warnings_deinit(&warnings);
    if (callbacks->infer_expression_descriptor(database, plan, expression, NULL, &descriptor) ==
        MYLITE_OK) {
        return descriptor.length;
    }
    return result;
}

static uint64_t slice_string_function_result_length(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_value *value,
    const struct mylite_expression_descriptor_string_callbacks *callbacks
) {
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    const struct mylite_sql_ast_node *source = mylite_ast_child_at(arguments, 0U);
    struct mylite_field_descriptor source_descriptor = mylite_expression_descriptor_defaults();

    if (mylite_function_name_is_elt(name)) {
        return elt_function_result_length(database, plan, expression, callbacks);
    }
    if (mylite_function_name_is_quote(name)) {
        return mylite_expression_descriptor_quote_function_result_length(
            database,
            plan,
            expression,
            callbacks
        );
    }
    if (mylite_function_name_is_insert(name)) {
        return insert_function_result_length(database, plan, expression, callbacks);
    }
    if (name != NULL && mylite_span_equal_ci(name->span, "REPEAT")) {
        return repeat_function_result_length(database, plan, arguments, callbacks);
    }
    if (function_name_is_padding(name)) {
        return padding_function_result_length(database, plan, arguments, callbacks);
    }
    if (!mylite_function_name_uses_source_length(name) && value != NULL &&
        value->kind == MYLITE_EXPRESSION_VALUE_TEXT) {
        return mylite_expression_descriptor_string_length(database, value, NULL);
    }
    if (mylite_function_name_is_concat_ws(name)) {
        return concat_ws_function_result_length(database, plan, arguments, callbacks);
    }
    if (source != NULL &&
        callbacks->infer_expression_descriptor(database, plan, source, NULL, &source_descriptor) ==
            MYLITE_OK) {
        return source_descriptor.length;
    }
    return mylite_mysql_text_length;
}

static uint64_t repeat_function_result_length(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *arguments,
    const struct mylite_expression_descriptor_string_callbacks *callbacks
) {
    const struct mylite_sql_ast_node *source = mylite_ast_child_at(arguments, 0U);
    const struct mylite_sql_ast_node *count = mylite_ast_child_at(arguments, 1U);
    uint64_t repeat_count = 0U;
    uint64_t source_length = expression_text_display_length(database, plan, source, callbacks);

    if (!nonnegative_integer_constant(count, &repeat_count)) {
        return source_length;
    }
    return multiply_text_display_length(source_length, repeat_count);
}

static uint64_t padding_function_result_length(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *arguments,
    const struct mylite_expression_descriptor_string_callbacks *callbacks
) {
    const struct mylite_sql_ast_node *source = mylite_ast_child_at(arguments, 0U);
    const struct mylite_sql_ast_node *target = mylite_ast_child_at(arguments, 1U);
    uint64_t target_characters = 0U;
    uint64_t max_bytes_per_character =
        mylite_expression_descriptor_connection_character_max_length(database);

    if (!nonnegative_integer_constant(target, &target_characters)) {
        return expression_text_display_length(database, plan, source, callbacks);
    }
    return multiply_text_display_length(target_characters, max_bytes_per_character);
}

static uint64_t add_text_display_lengths(uint64_t left, uint64_t right) {
    if (left > UINT64_MAX - right) {
        return mylite_mysql_long_text_length;
    }
    return left + right;
}

static uint64_t multiply_text_display_length(uint64_t length, uint64_t factor) {
    if (factor != 0U && length > UINT64_MAX / factor) {
        return mylite_mysql_long_text_length;
    }
    return length * factor;
}

static bool function_name_is_padding(const struct mylite_sql_ast_node *name) {
    return name != NULL &&
           (mylite_span_equal_ci(name->span, "LPAD") || mylite_span_equal_ci(name->span, "RPAD"));
}

static bool nonnegative_integer_constant(
    const struct mylite_sql_ast_node *expression,
    uint64_t *out_value
) {
    struct mylite_expression_warnings warnings = {0};
    struct mylite_expression_value value = {0};
    bool matched = false;

    if (expression == NULL || out_value == NULL ||
        !mylite_expression_is_cacheable_no_table(expression)) {
        return false;
    }
    if (mylite_expression_eval(expression, &warnings, &value) != 0) {
        mylite_expression_value_deinit(&value);
        mylite_expression_warnings_deinit(&warnings);
        return false;
    }
    if (value.kind == MYLITE_EXPRESSION_VALUE_INT64 && value.int64_value >= 0) {
        *out_value = (uint64_t)value.int64_value;
        matched = true;
    } else if (value.kind == MYLITE_EXPRESSION_VALUE_UINT64) {
        *out_value = value.uint64_value;
        matched = true;
    }
    mylite_expression_value_deinit(&value);
    mylite_expression_warnings_deinit(&warnings);
    return matched;
}

static int infer_function_arguments_nullable(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *arguments,
    bool *out_nullable,
    const struct mylite_expression_descriptor_string_callbacks *callbacks
) {
    *out_nullable = false;
    for (const struct mylite_sql_ast_node *argument = arguments == NULL ? NULL
                                                                        : arguments->first_child;
         argument != NULL;
         argument = argument->next_sibling) {
        struct mylite_field_descriptor descriptor = mylite_expression_descriptor_defaults();
        int status =
            callbacks->infer_expression_descriptor(database, plan, argument, NULL, &descriptor);

        if (status != MYLITE_OK) {
            return status;
        }
        if (mylite_expression_descriptor_is_nullable(&descriptor)) {
            *out_nullable = true;
        }
    }
    return MYLITE_OK;
}
