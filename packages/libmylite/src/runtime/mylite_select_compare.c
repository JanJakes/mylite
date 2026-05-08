#include "mylite_select_compare.h"

#include "mylite_expression.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int compare_select_text_values(
    const char *left,
    size_t left_length,
    const char *right,
    size_t right_length,
    bool case_sensitive,
    bool pad_space_compare
);

static bool expression_value_uses_binary_text_compare(const struct mylite_expression_value *value);

static bool expression_value_uses_binary_text_charset(const struct mylite_expression_value *value);

static bool expression_value_uses_pad_space_text_compare(
    const struct mylite_expression_value *value
);

static size_t expression_value_text_length(const struct mylite_expression_value *value);

static size_t trimmed_text_length(const char *text, size_t length);

static size_t nullable_text_length(const char *text);

int mylite_select_compare_values(
    const struct mylite_expression_value *left,
    const struct mylite_expression_value *right
) {
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
    if (left->kind == MYLITE_EXPRESSION_VALUE_TEXT && right->kind == MYLITE_EXPRESSION_VALUE_TEXT) {
        bool binary_compare = expression_value_uses_binary_text_compare(left) ||
                              expression_value_uses_binary_text_compare(right);
        bool binary_charset_compare = expression_value_uses_binary_text_charset(left) ||
                                      expression_value_uses_binary_text_charset(right);
        bool pad_space_compare =
            !binary_charset_compare && (expression_value_uses_pad_space_text_compare(left) ||
                                        expression_value_uses_pad_space_text_compare(right));

        return compare_select_text_values(
            left->text_value,
            expression_value_text_length(left),
            right->text_value,
            expression_value_text_length(right),
            binary_compare,
            pad_space_compare
        );
    }
    if (left->kind == MYLITE_EXPRESSION_VALUE_REAL || right->kind == MYLITE_EXPRESSION_VALUE_REAL) {
        double left_value = left->kind == MYLITE_EXPRESSION_VALUE_REAL
                                ? left->real_value
                                : (double)mylite_expression_value_to_int64(left);
        double right_value = right->kind == MYLITE_EXPRESSION_VALUE_REAL
                                 ? right->real_value
                                 : (double)mylite_expression_value_to_int64(right);

        return (left_value > right_value) - (left_value < right_value);
    }
    if (left->kind == MYLITE_EXPRESSION_VALUE_UINT64 ||
        right->kind == MYLITE_EXPRESSION_VALUE_UINT64) {
        uint64_t left_value = left->kind == MYLITE_EXPRESSION_VALUE_UINT64
                                  ? left->uint64_value
                                  : (uint64_t)mylite_expression_value_to_int64(left);
        uint64_t right_value = right->kind == MYLITE_EXPRESSION_VALUE_UINT64
                                   ? right->uint64_value
                                   : (uint64_t)mylite_expression_value_to_int64(right);

        return (left_value > right_value) - (left_value < right_value);
    }
    if (left->kind == MYLITE_EXPRESSION_VALUE_TEXT || right->kind == MYLITE_EXPRESSION_VALUE_TEXT) {
        char *left_text = mylite_expression_value_to_text(left);
        char *right_text = mylite_expression_value_to_text(right);
        int comparison = compare_select_text_values(
            left_text,
            nullable_text_length(left_text),
            right_text,
            nullable_text_length(right_text),
            false,
            false
        );

        free(left_text);
        free(right_text);
        return comparison;
    }
    return (left->int64_value > right->int64_value) - (left->int64_value < right->int64_value);
}

int mylite_select_compare_binary_text_values(
    const char *left,
    size_t left_length,
    const char *right,
    size_t right_length
) {
    size_t compare_length = 0U;
    int comparison = 0;

    if (left == NULL) {
        left = "";
        left_length = 0U;
    }
    if (right == NULL) {
        right = "";
        right_length = 0U;
    }
    compare_length = left_length < right_length ? left_length : right_length;
    comparison = compare_length == 0U ? 0 : memcmp(left, right, compare_length);
    if (comparison == 0) {
        return (left_length > right_length) - (left_length < right_length);
    }
    return (comparison > 0) - (comparison < 0);
}

static int compare_select_text_values(
    const char *left,
    size_t left_length,
    const char *right,
    size_t right_length,
    bool case_sensitive,
    bool pad_space_compare
) {
    size_t index = 0U;
    size_t compare_length = 0U;

    if (left == NULL) {
        left = "";
        left_length = 0U;
    }
    if (right == NULL) {
        right = "";
        right_length = 0U;
    }
    if (pad_space_compare) {
        left_length = trimmed_text_length(left, left_length);
        right_length = trimmed_text_length(right, right_length);
    }
    compare_length = left_length < right_length ? left_length : right_length;
    while (index < compare_length) {
        unsigned char left_byte = (unsigned char)left[index];
        unsigned char right_byte = (unsigned char)right[index];

        if (!case_sensitive && left_byte >= 'A' && left_byte <= 'Z') {
            left_byte = (unsigned char)(left_byte - 'A' + 'a');
        }
        if (!case_sensitive && right_byte >= 'A' && right_byte <= 'Z') {
            right_byte = (unsigned char)(right_byte - 'A' + 'a');
        }
        if (left_byte != right_byte) {
            return (left_byte > right_byte) - (left_byte < right_byte);
        }
        ++index;
    }
    return (left_length > right_length) - (left_length < right_length);
}

static bool expression_value_uses_binary_text_compare(const struct mylite_expression_value *value) {
    return value != NULL && value->kind == MYLITE_EXPRESSION_VALUE_TEXT &&
           (value->binary_text_compare ||
            value->text_charset == MYLITE_EXPRESSION_TEXT_CHARSET_BINARY);
}

static bool expression_value_uses_binary_text_charset(const struct mylite_expression_value *value) {
    return value != NULL && value->kind == MYLITE_EXPRESSION_VALUE_TEXT &&
           value->text_charset == MYLITE_EXPRESSION_TEXT_CHARSET_BINARY;
}

static bool expression_value_uses_pad_space_text_compare(
    const struct mylite_expression_value *value
) {
    return value != NULL && value->kind == MYLITE_EXPRESSION_VALUE_TEXT &&
           value->pad_space_text_compare;
}

static size_t expression_value_text_length(const struct mylite_expression_value *value) {
    if (value == NULL || value->text_value == NULL) {
        return 0U;
    }
    return value->text_length;
}

static size_t trimmed_text_length(const char *text, size_t length) {
    if (text == NULL) {
        return 0U;
    }
    while (length > 0U && text[length - 1U] == ' ') {
        --length;
    }
    return length;
}

static size_t nullable_text_length(const char *text) {
    return text == NULL ? 0U : strlen(text);
}
