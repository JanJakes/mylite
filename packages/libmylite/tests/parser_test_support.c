#include "parser_test_support.h"

#include <stdio.h>
#include <string.h>

int parser_test_parse_sql(
    const char *sql,
    enum mylite_sql_parse_status expected_status,
    struct mylite_sql_parse_result *out_result
) {
    return parser_test_parse_sql_with_modes(
        sql,
        expected_status,
        (struct parser_test_parse_modes){0},
        out_result
    );
}

int parser_test_parse_sql_with_ignore_space(
    const char *sql,
    enum mylite_sql_parse_status expected_status,
    struct mylite_sql_parse_result *out_result
) {
    return parser_test_parse_sql_with_modes(
        sql,
        expected_status,
        (struct parser_test_parse_modes){.value = MYLITE_SQL_MODE_IGNORE_SPACE},
        out_result
    );
}

int parser_test_parse_sql_with_modes(
    const char *sql,
    enum mylite_sql_parse_status expected_status,
    struct parser_test_parse_modes modes,
    struct mylite_sql_parse_result *out_result
) {
    enum mylite_sql_parse_status actual = mylite_sql_parse(
        (struct mylite_sql_parse_config){
            .input = sql,
            .length = strlen(sql),
            .modes = modes.value,
        },
        out_result
    );

    if (actual != expected_status) {
        fprintf(
            stderr,
            "parse '%s': expected %s, got %s\n",
            sql,
            mylite_sql_parse_status_name(expected_status),
            mylite_sql_parse_status_name(actual)
        );
        return 1;
    }

    return 0;
}

const struct mylite_sql_ast_node *parser_test_child_at(
    const struct mylite_sql_ast_node *node,
    size_t index
) {
    const struct mylite_sql_ast_node *child = NULL;

    if (node == NULL) {
        return NULL;
    }

    child = node->first_child;
    for (size_t current = 0U; current < index && child != NULL; ++current) {
        child = child->next_sibling;
    }
    return child;
}

const struct mylite_sql_ast_node *parser_test_first_child_kind(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_node_kind kind
) {
    const struct mylite_sql_ast_node *child = NULL;

    if (node == NULL) {
        return NULL;
    }

    child = node->first_child;
    while (child != NULL) {
        if (child->kind == kind) {
            return child;
        }
        child = child->next_sibling;
    }

    return NULL;
}

int parser_test_expect_node(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_node_kind expected_kind,
    const char *context
) {
    if (node == NULL) {
        fprintf(
            stderr,
            "%s: expected %s, got null\n",
            context,
            mylite_sql_ast_node_kind_name(expected_kind)
        );
        return 1;
    }

    if (node->kind != expected_kind) {
        fprintf(
            stderr,
            "%s: expected %s, got %s\n",
            context,
            mylite_sql_ast_node_kind_name(expected_kind),
            mylite_sql_ast_node_kind_name(node->kind)
        );
        return 1;
    }

    return 0;
}

int parser_test_expect_child_count(
    const struct mylite_sql_ast_node *node,
    size_t expected,
    const char *context
) {
    size_t actual = mylite_sql_ast_node_child_count(node);

    if (actual != expected) {
        fprintf(stderr, "%s: expected %zu children, got %zu\n", context, expected, actual);
        return 1;
    }

    return 0;
}

int parser_test_expect_span_text(
    const struct mylite_sql_ast_node *node,
    const char *expected,
    const char *context
) {
    size_t expected_length = strlen(expected);

    if (node == NULL) {
        fprintf(stderr, "%s: expected span '%s', got null node\n", context, expected);
        return 1;
    }

    if (node->span.length != expected_length ||
        (expected_length > 0U && memcmp(node->span.text, expected, expected_length) != 0)) {
        fprintf(
            stderr,
            "%s: expected span '%s', got '%.*s'\n",
            context,
            expected,
            (int)node->span.length,
            node->span.text == NULL ? "" : node->span.text
        );
        return 1;
    }

    return 0;
}

int parser_test_expect_literal(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_literal_kind expected,
    const char *context
) {
    int failures = parser_test_expect_node(node, MYLITE_SQL_AST_LITERAL, context);

    if (node != NULL && mylite_sql_ast_node_literal_kind(node) != expected) {
        fprintf(
            stderr,
            "%s: expected literal %s, got %s\n",
            context,
            mylite_sql_ast_literal_kind_name(expected),
            mylite_sql_ast_literal_kind_name(mylite_sql_ast_node_literal_kind(node))
        );
        failures = 1;
    }

    return failures;
}

int parser_test_expect_operator(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_operator expected,
    const char *context
) {
    if (node == NULL) {
        fprintf(
            stderr,
            "%s: expected operator %s, got null node\n",
            context,
            mylite_sql_ast_operator_name(expected)
        );
        return 1;
    }

    if (mylite_sql_ast_node_operator(node) != expected) {
        fprintf(
            stderr,
            "%s: expected operator %s, got %s\n",
            context,
            mylite_sql_ast_operator_name(expected),
            mylite_sql_ast_operator_name(mylite_sql_ast_node_operator(node))
        );
        return 1;
    }

    return 0;
}

int parser_test_expect_true(int condition, const char *context) {
    if (!condition) {
        fprintf(stderr, "%s: expected true\n", context);
        return 1;
    }

    return 0;
}

int parser_test_expect_integer_type(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_integer_type expected_type,
    int expected_unsigned,
    const char *context
) {
    enum mylite_sql_ast_integer_type actual_type = mylite_sql_ast_node_integer_type(node);
    int actual_unsigned = mylite_sql_ast_node_integer_type_is_unsigned(node);

    if (actual_type != expected_type || actual_unsigned != expected_unsigned) {
        fprintf(
            stderr,
            "%s: expected %s unsigned=%d, got %s unsigned=%d\n",
            context,
            mylite_sql_ast_integer_type_name(expected_type),
            expected_unsigned,
            mylite_sql_ast_integer_type_name(actual_type),
            actual_unsigned
        );
        return 1;
    }

    return 0;
}

int parser_test_expect_varchar_type(
    const struct mylite_sql_ast_node *node,
    const char *expected_length,
    const char *context
) {
    struct mylite_sql_source_span span = {0};

    if (parser_test_expect_node(node, MYLITE_SQL_AST_VARCHAR_TYPE, context) != 0) {
        return 1;
    }

    span = mylite_sql_ast_node_varchar_type_length_span(node);
    if (span.text == NULL || span.length != strlen(expected_length) ||
        strncmp(span.text, expected_length, span.length) != 0) {
        fprintf(
            stderr,
            "%s: expected VARCHAR length %s, got %.*s\n",
            context,
            expected_length,
            (int)span.length,
            span.text == NULL ? "" : span.text
        );
        return 1;
    }
    if (mylite_sql_ast_node_varchar_type_is_national(node) != 0) {
        fprintf(stderr, "%s: expected ordinary VARCHAR type, got national type\n", context);
        return 1;
    }

    return 0;
}

int parser_test_expect_national_varchar_type(
    const struct mylite_sql_ast_node *node,
    const char *expected_length,
    const char *context
) {
    struct mylite_sql_source_span span = {0};

    if (parser_test_expect_node(node, MYLITE_SQL_AST_VARCHAR_TYPE, context) != 0) {
        return 1;
    }

    span = mylite_sql_ast_node_varchar_type_length_span(node);
    if (span.text == NULL || span.length != strlen(expected_length) ||
        strncmp(span.text, expected_length, span.length) != 0) {
        fprintf(
            stderr,
            "%s: expected national VARCHAR length %s, got %.*s\n",
            context,
            expected_length,
            (int)span.length,
            span.text == NULL ? "" : span.text
        );
        return 1;
    }
    if (mylite_sql_ast_node_varchar_type_is_national(node) == 0) {
        fprintf(stderr, "%s: expected national VARCHAR type\n", context);
        return 1;
    }

    return 0;
}

int parser_test_expect_char_type(
    const struct mylite_sql_ast_node *node,
    const char *expected_length,
    int expected_explicit_length,
    const char *context
) {
    struct mylite_sql_source_span span = {0};
    int has_explicit_length = 0;

    if (parser_test_expect_node(node, MYLITE_SQL_AST_CHAR_TYPE, context) != 0) {
        return 1;
    }

    has_explicit_length = mylite_sql_ast_node_char_type_has_explicit_length(node);
    if (has_explicit_length != expected_explicit_length) {
        fprintf(
            stderr,
            "%s: expected explicit CHAR length %d, got %d\n",
            context,
            expected_explicit_length,
            has_explicit_length
        );
        return 1;
    }
    if (mylite_sql_ast_node_char_type_is_national(node) != 0) {
        fprintf(stderr, "%s: expected ordinary CHAR type, got national type\n", context);
        return 1;
    }
    if (expected_length == NULL) {
        return 0;
    }

    span = mylite_sql_ast_node_char_type_length_span(node);
    if (span.text == NULL || span.length != strlen(expected_length) ||
        strncmp(span.text, expected_length, span.length) != 0) {
        fprintf(
            stderr,
            "%s: expected CHAR length %s, got %.*s\n",
            context,
            expected_length,
            (int)span.length,
            span.text == NULL ? "" : span.text
        );
        return 1;
    }

    return 0;
}

int parser_test_expect_national_char_type(
    const struct mylite_sql_ast_node *node,
    const char *expected_length,
    int expected_explicit_length,
    const char *context
) {
    struct mylite_sql_source_span span = {0};
    int has_explicit_length = 0;

    if (parser_test_expect_node(node, MYLITE_SQL_AST_CHAR_TYPE, context) != 0) {
        return 1;
    }

    has_explicit_length = mylite_sql_ast_node_char_type_has_explicit_length(node);
    if (has_explicit_length != expected_explicit_length) {
        fprintf(
            stderr,
            "%s: expected explicit national CHAR length %d, got %d\n",
            context,
            expected_explicit_length,
            has_explicit_length
        );
        return 1;
    }
    if (mylite_sql_ast_node_char_type_is_national(node) == 0) {
        fprintf(stderr, "%s: expected national CHAR type\n", context);
        return 1;
    }
    if (expected_length == NULL) {
        return 0;
    }

    span = mylite_sql_ast_node_char_type_length_span(node);
    if (span.text == NULL || span.length != strlen(expected_length) ||
        strncmp(span.text, expected_length, span.length) != 0) {
        fprintf(
            stderr,
            "%s: expected national CHAR length %s, got %.*s\n",
            context,
            expected_length,
            (int)span.length,
            span.text == NULL ? "" : span.text
        );
        return 1;
    }

    return 0;
}

int parser_test_expect_text_type(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_text_type expected,
    const char *context
) {
    enum mylite_sql_ast_text_type actual = MYLITE_SQL_AST_TEXT_TYPE_NONE;

    if (parser_test_expect_node(node, MYLITE_SQL_AST_TEXT_TYPE, context) != 0) {
        return 1;
    }

    actual = mylite_sql_ast_node_text_type(node);
    if (actual != expected) {
        fprintf(
            stderr,
            "%s: expected text type %s, got %s\n",
            context,
            mylite_sql_ast_text_type_name(expected),
            mylite_sql_ast_text_type_name(actual)
        );
        return 1;
    }

    return 0;
}

int parser_test_expect_text_type_length(
    const struct mylite_sql_ast_node *node,
    const char *expected_length,
    const char *context
) {
    struct mylite_sql_source_span span;

    if (parser_test_expect_node(node, MYLITE_SQL_AST_TEXT_TYPE, context) != 0) {
        return 1;
    }
    if (expected_length == NULL) {
        if (mylite_sql_ast_node_text_type_has_length(node) != 0) {
            fprintf(stderr, "%s: expected no text length\n", context);
            return 1;
        }
        return 0;
    }
    if (mylite_sql_ast_node_text_type_has_length(node) == 0) {
        fprintf(stderr, "%s: expected text length %s\n", context, expected_length);
        return 1;
    }

    span = mylite_sql_ast_node_text_type_length_span(node);
    if (span.text == NULL || strlen(expected_length) != span.length ||
        strncmp(span.text, expected_length, span.length) != 0) {
        fprintf(
            stderr,
            "%s: expected text length %s, got %.*s\n",
            context,
            expected_length,
            (int)span.length,
            span.text == NULL ? "" : span.text
        );
        return 1;
    }

    return 0;
}

int parser_test_expect_binary_string_type(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_binary_string_type expected,
    const char *context
) {
    enum mylite_sql_ast_binary_string_type actual = MYLITE_SQL_AST_BINARY_STRING_TYPE_NONE;

    if (parser_test_expect_node(node, MYLITE_SQL_AST_BINARY_STRING_TYPE, context) != 0) {
        return 1;
    }

    actual = mylite_sql_ast_node_binary_string_type(node);
    if (actual != expected) {
        fprintf(
            stderr,
            "%s: expected binary string type %s, got %s\n",
            context,
            mylite_sql_ast_binary_string_type_name(expected),
            mylite_sql_ast_binary_string_type_name(actual)
        );
        return 1;
    }

    return 0;
}

int parser_test_expect_bit_type(
    const struct mylite_sql_ast_node *node,
    const char *expected_length,
    int expected_explicit_length,
    const char *context
) {
    struct mylite_sql_source_span span = {0};
    int has_explicit_length = 0;

    if (parser_test_expect_node(node, MYLITE_SQL_AST_BIT_TYPE, context) != 0) {
        return 1;
    }

    has_explicit_length = mylite_sql_ast_node_bit_type_has_length(node);
    if (has_explicit_length != expected_explicit_length) {
        fprintf(
            stderr,
            "%s: expected explicit BIT length %d, got %d\n",
            context,
            expected_explicit_length,
            has_explicit_length
        );
        return 1;
    }
    if (expected_length == NULL) {
        return 0;
    }

    span = mylite_sql_ast_node_bit_type_length_span(node);
    if (span.text == NULL || span.length != strlen(expected_length) ||
        strncmp(span.text, expected_length, span.length) != 0) {
        fprintf(
            stderr,
            "%s: expected BIT length %s, got %.*s\n",
            context,
            expected_length,
            (int)span.length,
            span.text == NULL ? "" : span.text
        );
        return 1;
    }

    return 0;
}

int parser_test_expect_year_type(
    const struct mylite_sql_ast_node *node,
    const char *expected_width,
    int expected_explicit_width,
    const char *context
) {
    struct mylite_sql_source_span span = {0};
    int has_explicit_width = 0;

    if (parser_test_expect_node(node, MYLITE_SQL_AST_YEAR_TYPE, context) != 0) {
        return 1;
    }

    has_explicit_width = mylite_sql_ast_node_year_type_has_width(node);
    if (has_explicit_width != expected_explicit_width) {
        fprintf(
            stderr,
            "%s: expected explicit YEAR width %d, got %d\n",
            context,
            expected_explicit_width,
            has_explicit_width
        );
        return 1;
    }
    if (expected_width == NULL) {
        return 0;
    }

    span = mylite_sql_ast_node_year_type_width_span(node);
    if (span.text == NULL || span.length != strlen(expected_width) ||
        strncmp(span.text, expected_width, span.length) != 0) {
        fprintf(
            stderr,
            "%s: expected YEAR width %s, got %.*s\n",
            context,
            expected_width,
            (int)span.length,
            span.text == NULL ? "" : span.text
        );
        return 1;
    }

    return 0;
}

int parser_test_expect_decimal_type(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_decimal_type expected,
    const char *expected_precision,
    const char *expected_scale,
    int expected_unsigned,
    const char *context
) {
    struct mylite_sql_source_span precision_span = {0};
    struct mylite_sql_source_span scale_span = {0};
    enum mylite_sql_ast_decimal_type actual = MYLITE_SQL_AST_DECIMAL_TYPE_DECIMAL;
    int actual_unsigned = 0;

    if (parser_test_expect_node(node, MYLITE_SQL_AST_DECIMAL_TYPE, context) != 0) {
        return 1;
    }

    actual = mylite_sql_ast_node_decimal_type(node);
    actual_unsigned = mylite_sql_ast_node_decimal_type_is_unsigned(node);
    if (actual != expected || actual_unsigned != expected_unsigned) {
        fprintf(
            stderr,
            "%s: expected decimal type %s unsigned=%d, got %s unsigned=%d\n",
            context,
            mylite_sql_ast_decimal_type_name(expected),
            expected_unsigned,
            mylite_sql_ast_decimal_type_name(actual),
            actual_unsigned
        );
        return 1;
    }

    if (expected_precision == NULL) {
        if (mylite_sql_ast_node_decimal_type_has_precision(node) != 0) {
            fprintf(stderr, "%s: expected no decimal precision\n", context);
            return 1;
        }
        return 0;
    }
    if (mylite_sql_ast_node_decimal_type_has_precision(node) == 0) {
        fprintf(stderr, "%s: expected decimal precision %s\n", context, expected_precision);
        return 1;
    }
    precision_span = mylite_sql_ast_node_decimal_type_precision_span(node);
    if (precision_span.text == NULL || precision_span.length != strlen(expected_precision) ||
        strncmp(precision_span.text, expected_precision, precision_span.length) != 0) {
        fprintf(
            stderr,
            "%s: expected decimal precision %s, got %.*s\n",
            context,
            expected_precision,
            (int)precision_span.length,
            precision_span.text == NULL ? "" : precision_span.text
        );
        return 1;
    }

    if (expected_scale == NULL) {
        if (mylite_sql_ast_node_decimal_type_has_scale(node) != 0) {
            fprintf(stderr, "%s: expected no decimal scale\n", context);
            return 1;
        }
        return 0;
    }
    if (mylite_sql_ast_node_decimal_type_has_scale(node) == 0) {
        fprintf(stderr, "%s: expected decimal scale %s\n", context, expected_scale);
        return 1;
    }
    scale_span = mylite_sql_ast_node_decimal_type_scale_span(node);
    if (scale_span.text == NULL || scale_span.length != strlen(expected_scale) ||
        strncmp(scale_span.text, expected_scale, scale_span.length) != 0) {
        fprintf(
            stderr,
            "%s: expected decimal scale %s, got %.*s\n",
            context,
            expected_scale,
            (int)scale_span.length,
            scale_span.text == NULL ? "" : scale_span.text
        );
        return 1;
    }

    return 0;
}

int parser_test_expect_approximate_type(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_approximate_type expected,
    const char *expected_precision,
    int expected_unsigned,
    const char *context
) {
    struct mylite_sql_source_span precision_span = {0};
    enum mylite_sql_ast_approximate_type actual = MYLITE_SQL_AST_APPROXIMATE_TYPE_FLOAT;
    int actual_unsigned = 0;

    if (parser_test_expect_node(node, MYLITE_SQL_AST_APPROXIMATE_TYPE, context) != 0) {
        return 1;
    }

    actual = mylite_sql_ast_node_approximate_type(node);
    actual_unsigned = mylite_sql_ast_node_approximate_type_is_unsigned(node);
    if (actual != expected || actual_unsigned != expected_unsigned) {
        fprintf(
            stderr,
            "%s: expected approximate type %s unsigned=%d, got %s unsigned=%d\n",
            context,
            mylite_sql_ast_approximate_type_name(expected),
            expected_unsigned,
            mylite_sql_ast_approximate_type_name(actual),
            actual_unsigned
        );
        return 1;
    }

    if (expected_precision == NULL) {
        if (mylite_sql_ast_node_approximate_type_has_precision(node) != 0) {
            fprintf(stderr, "%s: expected no approximate precision\n", context);
            return 1;
        }
        return 0;
    }
    if (mylite_sql_ast_node_approximate_type_has_precision(node) == 0) {
        fprintf(stderr, "%s: expected approximate precision %s\n", context, expected_precision);
        return 1;
    }
    precision_span = mylite_sql_ast_node_approximate_type_precision_span(node);
    if (precision_span.text == NULL || precision_span.length != strlen(expected_precision) ||
        strncmp(precision_span.text, expected_precision, precision_span.length) != 0) {
        fprintf(
            stderr,
            "%s: expected approximate precision %s, got %.*s\n",
            context,
            expected_precision,
            (int)precision_span.length,
            precision_span.text == NULL ? "" : precision_span.text
        );
        return 1;
    }

    return 0;
}

int parser_test_expect_integer_display_width(
    const struct mylite_sql_ast_node *node,
    const char *expected_width,
    const char *context
) {
    int has_width = mylite_sql_ast_node_integer_type_has_display_width(node);
    struct mylite_sql_source_span span = mylite_sql_ast_node_integer_type_display_width_span(node);

    if (expected_width == NULL) {
        if (has_width) {
            fprintf(stderr, "%s: expected no display width\n", context);
            return 1;
        }
        return 0;
    }

    if (!has_width) {
        fprintf(
            stderr,
            "%s: expected display width %s, got no display width\n",
            context,
            expected_width
        );
        return 1;
    }
    if (span.text == NULL || span.length != strlen(expected_width) ||
        strncmp(span.text, expected_width, span.length) != 0) {
        fprintf(
            stderr,
            "%s: expected display width %s, got %.*s\n",
            context,
            expected_width,
            (int)span.length,
            span.text == NULL ? "" : span.text
        );
        return 1;
    }

    return 0;
}

int parser_test_expect_integer_bool_alias(
    const struct mylite_sql_ast_node *node,
    const char *context
) {
    if (mylite_sql_ast_node_integer_type_is_bool_alias(node) == 0) {
        fprintf(stderr, "%s: expected bool alias marker\n", context);
        return 1;
    }

    return 0;
}

int parser_test_expect_integer_serial_alias(
    const struct mylite_sql_ast_node *node,
    const char *context
) {
    if (mylite_sql_ast_node_integer_type_is_serial_alias(node) == 0) {
        fprintf(stderr, "%s: expected serial alias marker\n", context);
        return 1;
    }

    return 0;
}

int parser_test_expect_nullability(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_nullability expected,
    const char *context
) {
    enum mylite_sql_ast_nullability actual = mylite_sql_ast_node_nullability(node);

    if (actual != expected) {
        fprintf(
            stderr,
            "%s: expected %s, got %s\n",
            context,
            mylite_sql_ast_nullability_name(expected),
            mylite_sql_ast_nullability_name(actual)
        );
        return 1;
    }

    return 0;
}

int parser_test_expect_order_direction(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_order_direction expected,
    const char *context
) {
    enum mylite_sql_ast_order_direction actual = mylite_sql_ast_node_order_direction(node);

    if (actual != expected) {
        fprintf(
            stderr,
            "%s: expected order direction %s, got %s\n",
            context,
            mylite_sql_ast_order_direction_name(expected),
            mylite_sql_ast_order_direction_name(actual)
        );
        return 1;
    }

    return 0;
}

int parser_test_expect_column_visibility(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_column_visibility expected,
    const char *context
) {
    enum mylite_sql_ast_column_visibility actual = mylite_sql_ast_node_column_visibility(node);

    if (actual != expected) {
        fprintf(
            stderr,
            "%s: expected column visibility %s, got %s\n",
            context,
            mylite_sql_ast_column_visibility_name(expected),
            mylite_sql_ast_column_visibility_name(actual)
        );
        return 1;
    }

    return 0;
}
