#include "mylite_select_rowset_distinct.h"

#include "mylite_field_descriptor.h"
#include "mylite_metadata_constants.h"
#include "mylite_metadata_types.h"
#include "mylite_select_compare.h"

#include <stddef.h>

static size_t expression_value_text_length(const struct mylite_expression_value *value);
static bool
table_select_text_descriptor_is_binary(const struct mylite_field_descriptor *descriptor);

bool mylite_select_result_distinct_row_exists(const struct mylite_table_select_result *result,
                                              const struct mylite_select_plan *plan,
                                              const struct mylite_result_metadata *metadata,
                                              const struct mylite_table_select_row *row)
{
    for (size_t index = 0U; index < result->row_count; ++index) {
        if (mylite_select_output_values_equal(plan, metadata, &result->rows[index], row)) {
            return true;
        }
    }
    return false;
}

bool mylite_select_output_values_equal(const struct mylite_select_plan *plan,
                                       const struct mylite_result_metadata *metadata,
                                       const struct mylite_table_select_row *left,
                                       const struct mylite_table_select_row *right)
{
    if (left->output_value_count != plan->output_count ||
        right->output_value_count != plan->output_count) {
        return false;
    }

    for (size_t index = 0U; index < plan->output_count; ++index) {
        const struct mylite_field_descriptor *descriptor =
            metadata != NULL && index < metadata->column_count
                ? &metadata->columns[index].descriptor
                : NULL;

        if (mylite_select_compare_distinct_values(&left->output_values[index],
                                                  &right->output_values[index], descriptor) != 0) {
            return false;
        }
    }
    return true;
}

int mylite_select_compare_distinct_values(const struct mylite_expression_value *left,
                                          const struct mylite_expression_value *right,
                                          const struct mylite_field_descriptor *descriptor)
{
    bool left_null = left->kind == MYLITE_EXPRESSION_VALUE_NULL;
    bool right_null = right->kind == MYLITE_EXPRESSION_VALUE_NULL;

    if (left_null || right_null) {
        if (left_null == right_null) {
            return 0;
        }
        if (left_null) {
            return -1;
        }
        return 1;
    }
    if (left->kind == MYLITE_EXPRESSION_VALUE_TEXT && right->kind == MYLITE_EXPRESSION_VALUE_TEXT &&
        table_select_text_descriptor_is_binary(descriptor)) {
        return mylite_select_compare_binary_text_values(
            left->text_value, expression_value_text_length(left), right->text_value,
            expression_value_text_length(right));
    }
    return mylite_select_compare_values(left, right);
}

static size_t expression_value_text_length(const struct mylite_expression_value *value)
{
    if (value == NULL || value->text_value == NULL) {
        return 0U;
    }
    return value->text_length;
}

static bool table_select_text_descriptor_is_binary(const struct mylite_field_descriptor *descriptor)
{
    if (descriptor == NULL) {
        return false;
    }
    switch (descriptor->type) {
    case MYLITE_FIELD_TYPE_STRING:
    case MYLITE_FIELD_TYPE_VAR_STRING:
    case MYLITE_FIELD_TYPE_BLOB:
        if ((descriptor->flags & MYLITE_FIELD_FLAG_BINARY) != 0U) {
            return true;
        }
        if (descriptor->charset_id == mylite_mysql_binary_charset_id ||
            descriptor->charset_id == mylite_mysql_utf8mb4_bin_charset_id) {
            return true;
        }
        return false;
    default:
        return false;
    }
}
