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
    const struct mylite_expression_descriptor_string_callbacks *callbacks
);

static uint64_t quote_function_descriptor_display_length(
    const struct mylite_field_descriptor *descriptor,
    uint64_t max_bytes_per_character
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
    uint64_t source_length = quote_function_source_display_length(
        database,
        plan,
        source,
        max_bytes_per_character,
        &source_is_null,
        callbacks
    );

    if (source_is_null) {
        return max_bytes_per_character > UINT64_MAX / 4U ? mylite_mysql_long_text_length
                                                         : 4U * max_bytes_per_character;
    }

    if (max_bytes_per_character > UINT64_MAX / 2U) {
        return mylite_mysql_long_text_length;
    }
    quote_length = 2U * max_bytes_per_character;
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
    const struct mylite_expression_descriptor_string_callbacks *callbacks
) {
    struct mylite_expression_warnings warnings = {0};
    struct mylite_expression_value value = {0};
    struct mylite_field_descriptor descriptor = mylite_expression_descriptor_defaults();

    *out_source_is_null = false;
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

    if (callbacks->infer_expression_descriptor(database, plan, source, NULL, &descriptor) ==
        MYLITE_OK) {
        return quote_function_descriptor_display_length(&descriptor, max_bytes_per_character);
    }
    return mylite_mysql_text_length;
}

static uint64_t quote_function_descriptor_display_length(
    const struct mylite_field_descriptor *descriptor,
    uint64_t max_bytes_per_character
) {
    if (mylite_expression_descriptor_has_text_result(descriptor)) {
        return descriptor->length;
    }
    if (descriptor == NULL || max_bytes_per_character == 0U ||
        descriptor->length > UINT64_MAX / max_bytes_per_character) {
        return mylite_mysql_long_text_length;
    }
    return descriptor->length * max_bytes_per_character;
}
