#include "mylite_expression_descriptor_string.h"

#include "mylite_expression_descriptor.h"
#include "mylite_function_names.h"
#include "mylite_metadata_constants.h"
#include "mylite_span.h"
#include "sql/mylite_ast.h"
#include "sql/mylite_expression.h"

#include <stdint.h>

static int infer_hex_function_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_string_callbacks *callbacks
);

static int infer_unhex_function_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_string_callbacks *callbacks
);

static int infer_to_base64_function_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_string_callbacks *callbacks
);

static int infer_from_base64_function_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_string_callbacks *callbacks
);

static uint64_t base64_encoded_descriptor_length(uint64_t source_length);

static uint64_t base64_decoded_descriptor_length(uint64_t source_length);

int mylite_expression_descriptor_infer_string_encoding_function(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_string_callbacks *callbacks,
    bool *out_matched
) {
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(expression, 0U);

    if (callbacks == NULL || callbacks->infer_expression_descriptor == NULL ||
        out_matched == NULL) {
        return MYLITE_MISUSE;
    }
    *out_matched = true;
    if (mylite_function_name_is_hex(name)) {
        return infer_hex_function_descriptor(database, plan, expression, out_descriptor, callbacks);
    }
    if (mylite_function_name_is_unhex(name)) {
        return infer_unhex_function_descriptor(
            database,
            plan,
            expression,
            out_descriptor,
            callbacks
        );
    }
    if (mylite_function_name_is_to_base64(name)) {
        return infer_to_base64_function_descriptor(
            database,
            plan,
            expression,
            out_descriptor,
            callbacks
        );
    }
    if (mylite_function_name_is_from_base64(name)) {
        return infer_from_base64_function_descriptor(
            database,
            plan,
            expression,
            out_descriptor,
            callbacks
        );
    }
    *out_matched = false;
    return MYLITE_OK;
}

static int infer_hex_function_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_string_callbacks *callbacks
) {
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    const struct mylite_sql_ast_node *source = mylite_ast_child_at(arguments, 0U);
    struct mylite_field_descriptor source_descriptor = mylite_expression_descriptor_defaults();
    const bool source_depends_on_row =
        plan != NULL && !mylite_expression_is_supported_no_table(source);
    uint64_t max_bytes_per_character =
        mylite_expression_descriptor_connection_character_max_length(database);
    uint64_t length = 0U;
    int status =
        callbacks->infer_expression_descriptor(database, plan, source, NULL, &source_descriptor);

    if (status != MYLITE_OK) {
        return status;
    }
    if (source_descriptor.type == MYLITE_FIELD_TYPE_NULL) {
        length = 0U;
    } else if (mylite_expression_descriptor_has_numeric_result(&source_descriptor)) {
        if (source_depends_on_row) {
            length = mylite_mysql_hex_numeric_result_chars;
        } else if (max_bytes_per_character > UINT64_MAX / mylite_mysql_hex_numeric_result_chars) {
            length = mylite_mysql_long_text_length;
        } else {
            length = mylite_mysql_hex_numeric_result_chars * max_bytes_per_character;
        }
    } else if (source_depends_on_row && source_descriptor.length > UINT64_MAX / 2U) {
        length = mylite_mysql_long_text_length;
    } else if (source_depends_on_row) {
        length = source_descriptor.length * 2U;
    } else if (
        source_descriptor.length > UINT64_MAX / 2U ||
        (source_descriptor.length * 2U) > UINT64_MAX / max_bytes_per_character
    ) {
        length = mylite_mysql_long_text_length;
    } else {
        length = source_descriptor.length * 2U * max_bytes_per_character;
    }

    *out_descriptor = (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_VAR_STRING,
        .flags = 0U,
        .length = length,
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = source_depends_on_row
                          ? mylite_mysql_latin1_swedish_ci_charset_id
                          : mylite_expression_descriptor_connection_charset_id(database),
        .nullable = true,
    };
    mylite_field_descriptor_set_nullable(out_descriptor, true);
    return MYLITE_OK;
}

static int infer_unhex_function_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_string_callbacks *callbacks
) {
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    const struct mylite_sql_ast_node *source = mylite_ast_child_at(arguments, 0U);
    struct mylite_field_descriptor source_descriptor = mylite_expression_descriptor_defaults();
    uint64_t length = 0U;
    int status =
        callbacks->infer_expression_descriptor(database, plan, source, NULL, &source_descriptor);

    if (status != MYLITE_OK) {
        return status;
    }
    if (source_descriptor.type == MYLITE_FIELD_TYPE_NULL) {
        length = 0U;
    } else if (source_descriptor.length == UINT64_MAX) {
        length = mylite_mysql_long_text_length;
    } else {
        length = (source_descriptor.length + 1U) / 2U;
    }

    *out_descriptor = (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_VAR_STRING,
        .flags = MYLITE_FIELD_FLAG_BINARY,
        .length = length,
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = true,
    };
    mylite_field_descriptor_set_nullable(out_descriptor, true);
    return MYLITE_OK;
}

static int infer_to_base64_function_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_string_callbacks *callbacks
) {
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    const struct mylite_sql_ast_node *source = mylite_ast_child_at(arguments, 0U);
    struct mylite_field_descriptor source_descriptor = mylite_expression_descriptor_defaults();
    uint64_t max_bytes_per_character =
        mylite_expression_descriptor_connection_character_max_length(database);
    uint64_t encoded_chars = 0U;
    uint64_t length = 0U;
    int status =
        callbacks->infer_expression_descriptor(database, plan, source, NULL, &source_descriptor);

    if (status != MYLITE_OK) {
        return status;
    }
    encoded_chars = source_descriptor.type == MYLITE_FIELD_TYPE_NULL
                        ? 0U
                        : base64_encoded_descriptor_length(source_descriptor.length);
    length = max_bytes_per_character != 0U && encoded_chars > UINT64_MAX / max_bytes_per_character
                 ? mylite_mysql_long_text_length
                 : encoded_chars * max_bytes_per_character;

    *out_descriptor = (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_VAR_STRING,
        .flags = 0U,
        .length = length,
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = mylite_expression_descriptor_connection_charset_id(database),
        .nullable = true,
    };
    mylite_field_descriptor_set_nullable(out_descriptor, true);
    return MYLITE_OK;
}

static int infer_from_base64_function_descriptor(
    mylite_db *database,
    const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor,
    const struct mylite_expression_descriptor_string_callbacks *callbacks
) {
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    const struct mylite_sql_ast_node *source = mylite_ast_child_at(arguments, 0U);
    struct mylite_field_descriptor source_descriptor = mylite_expression_descriptor_defaults();
    uint64_t length = 0U;
    int status =
        callbacks->infer_expression_descriptor(database, plan, source, NULL, &source_descriptor);

    if (status != MYLITE_OK) {
        return status;
    }
    length = source_descriptor.type == MYLITE_FIELD_TYPE_NULL
                 ? 0U
                 : base64_decoded_descriptor_length(source_descriptor.length);

    *out_descriptor = (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_VAR_STRING,
        .flags = MYLITE_FIELD_FLAG_BINARY,
        .length = length,
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = true,
    };
    mylite_field_descriptor_set_nullable(out_descriptor, true);
    return MYLITE_OK;
}

static uint64_t base64_encoded_descriptor_length(uint64_t source_length) {
    uint64_t groups = 0U;
    uint64_t encoded = 0U;
    uint64_t newlines = 0U;

    if (source_length == 0U) {
        return 0U;
    }
    if (source_length > UINT64_MAX - (mylite_mysql_base64_input_group - 1U)) {
        return mylite_mysql_long_text_length;
    }
    groups =
        (source_length + (mylite_mysql_base64_input_group - 1U)) / mylite_mysql_base64_input_group;
    if (groups > UINT64_MAX / mylite_mysql_base64_output_group) {
        return mylite_mysql_long_text_length;
    }
    encoded = groups * mylite_mysql_base64_output_group;
    newlines = (encoded - 1U) / mylite_mysql_base64_line_length;
    if (encoded > UINT64_MAX - newlines) {
        return mylite_mysql_long_text_length;
    }
    return encoded + newlines;
}

static uint64_t base64_decoded_descriptor_length(uint64_t source_length) {
    return ((source_length / mylite_mysql_base64_output_group) * mylite_mysql_base64_input_group) +
           (((source_length % mylite_mysql_base64_output_group) * mylite_mysql_base64_input_group) /
            mylite_mysql_base64_output_group);
}
