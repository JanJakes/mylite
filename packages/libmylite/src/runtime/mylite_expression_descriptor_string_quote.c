#include "mylite_expression_descriptor_string_quote.h"

#include "mylite_expression.h"
#include "mylite_expression_descriptor.h"
#include "mylite_metadata_constants.h"
#include "mylite_span.h"
#include "sql/mylite_ast.h"

#include <stdbool.h>
#include <stdint.h>

static uint64_t quote_function_source_display_length(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *source,
    uint64_t max_bytes_per_character,
    bool *out_source_is_null,
    bool *out_source_is_binary_string,
    bool *out_source_depends_on_row,
    const struct mylite_expression_descriptor_string_callbacks *callbacks
);

static uint64_t quote_function_descriptor_display_length(
    const struct mylite_field_descriptor *descriptor,
    uint64_t max_bytes_per_character,
    bool source_is_binary_string,
    bool source_depends_on_row
);

static bool quote_function_descriptor_is_binary_string(
    const struct mylite_field_descriptor *descriptor
);

uint64_t mylite_expression_descriptor_quote_function_result_length(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_descriptor_string_callbacks *callbacks
) {
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    const struct mylite_sql_ast_node *source = mylite_ast_child_at(arguments, 0U);
    uint64_t max_bytes_per_character =
        mylite_expression_descriptor_connection_character_max_length(database);
    uint64_t quote_length = 0U;
    bool source_is_null = false;
    bool source_is_binary_string = false;
    bool source_depends_on_row = false;
    uint64_t source_length = quote_function_source_display_length(
        database,
        plan,
        source,
        max_bytes_per_character,
        &source_is_null,
        &source_is_binary_string,
        &source_depends_on_row,
        callbacks
    );

    if (source_is_null) {
        return max_bytes_per_character > UINT64_MAX / 4U ? mylite_mysql_long_text_length
                                                         : 4U * max_bytes_per_character;
    }

    if (max_bytes_per_character > UINT64_MAX / 2U) {
        return mylite_mysql_long_text_length;
    }
    quote_length =
        source_is_binary_string && source_depends_on_row ? 2U : 2U * max_bytes_per_character;
    if (source_length > (UINT64_MAX - quote_length) / 2U) {
        return mylite_mysql_long_text_length;
    }
    return (source_length * 2U) + quote_length;
}

static uint64_t quote_function_source_display_length(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *source,
    uint64_t max_bytes_per_character,
    bool *out_source_is_null,
    bool *out_source_is_binary_string,
    bool *out_source_depends_on_row,
    const struct mylite_expression_descriptor_string_callbacks *callbacks
) {
    struct mylite_expression_warnings warnings = {0};
    struct mylite_expression_value value = {0};
    struct mylite_field_descriptor descriptor = mylite_expression_descriptor_defaults();
    bool descriptor_available = false;
    bool source_depends_on_row =
        plan != NULL && source != NULL && !mylite_expression_is_supported_no_table(source);

    *out_source_is_null = false;
    *out_source_is_binary_string = false;
    *out_source_depends_on_row = source_depends_on_row;
    descriptor_available =
        callbacks->infer_expression_descriptor(database, plan, source, NULL, &descriptor) ==
        MYLITE_OK;
    if (descriptor_available && quote_function_descriptor_is_binary_string(&descriptor)) {
        *out_source_is_binary_string = true;
        return quote_function_descriptor_display_length(
            &descriptor,
            max_bytes_per_character,
            true,
            source_depends_on_row
        );
    }
    if (source != NULL && mylite_expression_is_cacheable_no_table(source) &&
        mylite_expression_eval(source, &warnings, &value) == 0) {
        if (value.kind == MYLITE_EXPRESSION_VALUE_NULL) {
            *out_source_is_null = true;
            mylite_expression_value_deinit(&value);
            mylite_expression_warnings_deinit(&warnings);
            return 0U;
        }
        if (value.kind == MYLITE_EXPRESSION_VALUE_TEXT) {
            uint64_t result =
                mylite_expression_descriptor_utf8_display_character_count(value.text_value) *
                max_bytes_per_character;

            mylite_expression_value_deinit(&value);
            mylite_expression_warnings_deinit(&warnings);
            return result;
        }
        if (source->kind == MYLITE_SQL_AST_LITERAL &&
            (source->literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ||
             source->literal_kind == MYLITE_SQL_AST_LITERAL_FALSE)) {
            mylite_expression_value_deinit(&value);
            mylite_expression_warnings_deinit(&warnings);
            return max_bytes_per_character;
        }
    }
    mylite_expression_value_deinit(&value);
    mylite_expression_warnings_deinit(&warnings);

    if (descriptor_available) {
        return quote_function_descriptor_display_length(
            &descriptor,
            max_bytes_per_character,
            false,
            source_depends_on_row
        );
    }
    return mylite_mysql_text_length;
}

static uint64_t quote_function_descriptor_display_length(
    const struct mylite_field_descriptor *descriptor,
    uint64_t max_bytes_per_character,
    bool source_is_binary_string,
    bool source_depends_on_row
) {
    if (source_is_binary_string) {
        if (source_depends_on_row) {
            return descriptor == NULL ? 0U : descriptor->length;
        }
        if (descriptor == NULL || max_bytes_per_character == 0U ||
            descriptor->length > UINT64_MAX / max_bytes_per_character) {
            return mylite_mysql_long_text_length;
        }
        return descriptor->length * max_bytes_per_character;
    }
    if (mylite_expression_descriptor_has_text_result(descriptor)) {
        return descriptor->length;
    }
    if (descriptor == NULL || max_bytes_per_character == 0U ||
        descriptor->length > UINT64_MAX / max_bytes_per_character) {
        return mylite_mysql_long_text_length;
    }
    return descriptor->length * max_bytes_per_character;
}

static bool quote_function_descriptor_is_binary_string(
    const struct mylite_field_descriptor *descriptor
) {
    return descriptor != NULL &&
           ((descriptor->flags & MYLITE_FIELD_FLAG_BINARY) != 0U ||
            descriptor->charset_id == mylite_mysql_binary_charset_id) &&
           (mylite_expression_descriptor_has_text_result(descriptor) ||
            descriptor->type == MYLITE_FIELD_TYPE_BLOB);
}
