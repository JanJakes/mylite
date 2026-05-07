#include "mylite_span.h"

#include <mylite/mylite.h>

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *copy_current_timestamp_default_text(
    const struct mylite_sql_ast_node *node,
    bool parenthesized
);

static char *copy_now_default_text(const struct mylite_sql_ast_node *node, bool parenthesized);

static bool column_default_function_call_is_now(const struct mylite_sql_ast_node *node);

static bool column_default_function_call_fsp(
    const struct mylite_sql_ast_node *node,
    unsigned int *out_fsp
);

static bool column_default_literal_fsp(
    const struct mylite_sql_ast_node *node,
    unsigned int *out_fsp
);

static char *copy_current_timestamp_default_text_from_fsp(unsigned int fsp, bool parenthesized);

static bool column_default_current_timestamp_text_fsp(const char *text, unsigned int *out_fsp);

static bool column_default_text_function_fsp(
    const char *text,
    const char *name,
    bool requires_parentheses,
    unsigned int *out_fsp
);

static bool parse_default_fsp_text(const char *text, size_t length, unsigned int *out_fsp);

bool mylite_span_equal_ci(struct mylite_sql_source_span span, const char *text) {
    size_t text_length = text == NULL ? 0U : strlen(text);

    if (span.text == NULL || text == NULL || span.length != text_length) {
        return false;
    }
    for (size_t index = 0U; index < span.length; ++index) {
        unsigned char left_byte = (unsigned char)span.text[index];
        unsigned char right_byte = (unsigned char)text[index];

        if (left_byte >= 'A' && left_byte <= 'Z') {
            left_byte = (unsigned char)(left_byte - 'A' + 'a');
        }
        if (right_byte >= 'A' && right_byte <= 'Z') {
            right_byte = (unsigned char)(right_byte - 'A' + 'a');
        }
        if (left_byte != right_byte) {
            return false;
        }
    }
    return true;
}

bool mylite_source_span_equal_ci(
    struct mylite_sql_source_span left,
    struct mylite_sql_source_span right
) {
    if (left.length != right.length || left.text == NULL || right.text == NULL) {
        return false;
    }
    for (size_t index = 0U; index < left.length; ++index) {
        unsigned char left_byte = (unsigned char)left.text[index];
        unsigned char right_byte = (unsigned char)right.text[index];

        if (left_byte >= 'A' && left_byte <= 'Z') {
            left_byte = (unsigned char)(left_byte - 'A' + 'a');
        }
        if (right_byte >= 'A' && right_byte <= 'Z') {
            right_byte = (unsigned char)(right_byte - 'A' + 'a');
        }
        if (left_byte != right_byte) {
            return false;
        }
    }
    return true;
}

bool mylite_ascii_case_equal(const char *left, const char *right) {
    size_t index = 0U;

    if (left == NULL || right == NULL) {
        return false;
    }

    while (left[index] != '\0' && right[index] != '\0') {
        unsigned char left_byte = (unsigned char)left[index];
        unsigned char right_byte = (unsigned char)right[index];

        if (left_byte >= 'A' && left_byte <= 'Z') {
            left_byte = (unsigned char)(left_byte - 'A' + 'a');
        }
        if (right_byte >= 'A' && right_byte <= 'Z') {
            right_byte = (unsigned char)(right_byte - 'A' + 'a');
        }
        if (left_byte != right_byte) {
            return false;
        }
        ++index;
    }
    if (left[index] == '\0' && right[index] == '\0') {
        return true;
    }
    return false;
}

void mylite_uppercase_ascii_text(char *text) {
    for (size_t index = 0U; text != NULL && text[index] != '\0'; ++index) {
        if (text[index] >= 'a' && text[index] <= 'z') {
            text[index] = (char)(text[index] - 'a' + 'A');
        }
    }
}

char *mylite_copy_schema_text_span(const struct mylite_sql_ast_node *node) {
    if (node != NULL && node->kind == MYLITE_SQL_AST_LITERAL) {
        return mylite_copy_string_literal_span(node);
    }
    return mylite_copy_identifier_span(node);
}

char *mylite_copy_identifier_span(const struct mylite_sql_ast_node *node) {
    const char *text = node == NULL ? NULL : node->span.text;
    size_t length = node == NULL ? 0U : node->span.length;
    char *copy = NULL;
    size_t output = 0U;

    if (text == NULL) {
        return NULL;
    }
    if (length < 2U || (text[0] != '`' && text[0] != '"') || text[length - 1U] != text[0]) {
        return mylite_copy_span_text(text, length);
    }

    copy = malloc(length - 1U);
    if (copy == NULL) {
        return NULL;
    }

    for (size_t index = 1U; index + 1U < length; ++index) {
        if (text[index] == text[0] && index + 2U < length && text[index + 1U] == text[0]) {
            copy[output++] = text[0];
            ++index;
        } else {
            copy[output++] = text[index];
        }
    }
    copy[output] = '\0';
    return copy;
}

int mylite_copy_identifier_parts(
    const struct mylite_sql_ast_node *identifier,
    char **parts,
    size_t *part_count
) {
    const struct mylite_sql_ast_node *segments[3] = {0};
    const struct mylite_sql_ast_node *current = identifier;
    size_t segment_count = 0U;

    *part_count = 0U;
    while (current != NULL && current->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        if (segment_count >= 3U) {
            return MYLITE_UNSUPPORTED;
        }
        segments[segment_count++] = mylite_ast_child_at(current, 1U);
        current = mylite_ast_child_at(current, 0U);
    }
    if (current == NULL || current->kind != MYLITE_SQL_AST_IDENTIFIER || segment_count >= 3U) {
        return MYLITE_UNSUPPORTED;
    }
    segments[segment_count++] = current;

    for (size_t index = 0U; index < segment_count; ++index) {
        const struct mylite_sql_ast_node *segment = segments[segment_count - index - 1U];

        if (segment == NULL || segment->kind != MYLITE_SQL_AST_IDENTIFIER) {
            return MYLITE_UNSUPPORTED;
        }
        parts[index] = mylite_copy_identifier_span(segment);
        if (parts[index] == NULL) {
            for (size_t previous = 0U; previous < index; ++previous) {
                free(parts[previous]);
                parts[previous] = NULL;
            }
            *part_count = 0U;
            return MYLITE_NOMEM;
        }
        *part_count += 1U;
    }
    return MYLITE_OK;
}

static bool decode_string_escape(char escaped, char *out_character);

char *mylite_copy_string_literal_span(const struct mylite_sql_ast_node *node) {
    return mylite_copy_string_literal_span_with_length(node, NULL);
}

char *mylite_copy_string_literal_span_with_length(
    const struct mylite_sql_ast_node *node,
    size_t *out_length
) {
    const char *text = node == NULL ? NULL : node->span.text;
    size_t length = node == NULL ? 0U : node->span.length;
    char quote = '\0';
    char *copy = NULL;
    size_t output = 0U;
    size_t start = 1U;

    if (text == NULL) {
        return NULL;
    }
    if (length < 2U || ((text[0] != '\'' && text[0] != '"') &&
                        (length < 3U || (text[0] != 'N' && text[0] != 'n') ||
                         (text[1] != '\'' && text[1] != '"')))) {
        if (out_length != NULL) {
            *out_length = length;
        }
        return mylite_copy_span_text(text, length);
    }

    if (text[0] == 'N' || text[0] == 'n') {
        start = 2U;
    }
    quote = text[start - 1U];
    copy = malloc(length - start);
    if (copy == NULL) {
        return NULL;
    }

    for (size_t index = start; index + 1U < length; ++index) {
        if (text[index] == quote && index + 2U < length && text[index + 1U] == quote) {
            copy[output++] = quote;
            ++index;
        } else if (!node->no_backslash_escapes && text[index] == '\\' && index + 2U < length) {
            char escaped = '\0';

            if (decode_string_escape(text[index + 1U], &escaped)) {
                copy[output++] = escaped;
                ++index;
            } else {
                copy[output++] = text[index];
            }
        } else {
            copy[output++] = text[index];
        }
    }
    copy[output] = '\0';
    if (out_length != NULL) {
        *out_length = output;
    }
    return copy;
}

static bool decode_string_escape(char escaped, char *out_character) {
    switch (escaped) {
    case '\'':
    case '"':
    case '\\':
        *out_character = escaped;
        return true;
    case '0':
        *out_character = '\0';
        return true;
    case 'b':
        *out_character = '\b';
        return true;
    case 'n':
        *out_character = '\n';
        return true;
    case 'r':
        *out_character = '\r';
        return true;
    case 't':
        *out_character = '\t';
        return true;
    case 'Z':
        *out_character = '\x1A';
        return true;
    default:
        return false;
    }
}

char *mylite_copy_unquoted_span_text(struct mylite_sql_source_span span) {
    const char *text = span.text == NULL ? "" : span.text;
    size_t start = 0U;
    size_t end = span.text == NULL ? 0U : span.length;

    if (end >= 2U && (text[0] == '\'' || text[0] == '"') && text[end - 1U] == text[0]) {
        start = 1U;
        --end;
    }
    return mylite_copy_span_text(text + start, end - start);
}

char *mylite_copy_nonempty_cstring(const char *text) {
    size_t length = 0U;
    char *copy = NULL;

    if (text == NULL || text[0] == '\0') {
        return NULL;
    }
    length = strlen(text);
    if (length == 0U || length == SIZE_MAX) {
        return NULL;
    }

    copy = malloc(length + 1U);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, text, length + 1U);
    return copy;
}

char *mylite_copy_span_text(const char *text, size_t length) {
    char *copy = malloc(length + 1U);

    if (copy == NULL) {
        return NULL;
    }

    if (length > 0U) {
        memcpy(copy, text, length);
    }
    copy[length] = '\0';
    return copy;
}

bool mylite_span_contains_newline(const char *text, size_t length) {
    for (size_t index = 0U; index < length; ++index) {
        if (text[index] == '\n' || text[index] == '\r') {
            return true;
        }
    }
    return false;
}

bool mylite_text_contains_word(const char *text, const char *word) {
    if (text == NULL || word == NULL || word[0] == '\0') {
        return false;
    }
    return strstr(text, word) != NULL;
}

char *mylite_copy_column_default_text(const struct mylite_sql_ast_node *node) {
    if (node == NULL) {
        return NULL;
    }
    if (node->kind == MYLITE_SQL_AST_LITERAL &&
        node->literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
        return mylite_copy_string_literal_span(node);
    }
    if (node->kind == MYLITE_SQL_AST_LITERAL && node->literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        return NULL;
    }
    if (node->kind == MYLITE_SQL_AST_CURRENT_TIMESTAMP) {
        return copy_current_timestamp_default_text(node, false);
    }
    if (node->kind == MYLITE_SQL_AST_FUNCTION_CALL && column_default_function_call_is_now(node)) {
        return copy_now_default_text(node, false);
    }
    if (node->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
        const struct mylite_sql_ast_node *inner = mylite_ast_child_at(node, 0U);

        if (inner != NULL && inner->kind == MYLITE_SQL_AST_CURRENT_TIMESTAMP) {
            return copy_current_timestamp_default_text(inner, true);
        }
        if (inner != NULL && inner->kind == MYLITE_SQL_AST_FUNCTION_CALL &&
            column_default_function_call_is_now(inner)) {
            return copy_now_default_text(inner, true);
        }
    }
    return mylite_copy_span_text(node->span.text, node->span.length);
}

bool mylite_column_default_node_is_generated(const struct mylite_sql_ast_node *node) {
    if (node == NULL) {
        return false;
    }
    if (node->kind == MYLITE_SQL_AST_CURRENT_TIMESTAMP ||
        node->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
        return true;
    }
    return node->kind == MYLITE_SQL_AST_FUNCTION_CALL && column_default_function_call_is_now(node);
}

bool mylite_column_default_current_timestamp_fsp(const char *default_text, unsigned int *out_fsp) {
    const char *start = default_text;
    const char *end = default_text == NULL ? NULL : default_text + strlen(default_text);
    char *copy = NULL;
    bool matches = false;

    if (default_text == NULL) {
        return false;
    }
    while (start < end && isspace((unsigned char)*start)) {
        ++start;
    }
    while (end > start && isspace((unsigned char)*(end - 1))) {
        --end;
    }
    if (end > start + 1 && *start == '(' && *(end - 1) == ')') {
        ++start;
        --end;
        while (start < end && isspace((unsigned char)*start)) {
            ++start;
        }
        while (end > start && isspace((unsigned char)*(end - 1))) {
            --end;
        }
    }

    copy = mylite_copy_span_text(start, (size_t)(end - start));
    if (copy == NULL) {
        return false;
    }
    matches = column_default_current_timestamp_text_fsp(copy, out_fsp);
    free(copy);
    return matches;
}

bool mylite_column_default_is_current_timestamp(const char *default_text) {
    return mylite_column_default_current_timestamp_fsp(default_text, NULL);
}

static char *copy_current_timestamp_default_text(
    const struct mylite_sql_ast_node *node,
    bool parenthesized
) {
    unsigned int fsp = 0U;

    if (node != NULL && node->has_column_precision) {
        fsp = (unsigned int)node->column_precision;
    }
    return copy_current_timestamp_default_text_from_fsp(fsp, parenthesized);
}

static char *copy_now_default_text(const struct mylite_sql_ast_node *node, bool parenthesized) {
    unsigned int fsp = 0U;

    (void)column_default_function_call_fsp(node, &fsp);
    return copy_current_timestamp_default_text_from_fsp(fsp, parenthesized);
}

static bool column_default_function_call_is_now(const struct mylite_sql_ast_node *node) {
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(node, 0U);

    return node != NULL && node->kind == MYLITE_SQL_AST_FUNCTION_CALL &&
           mylite_span_equal_ci(
               name == NULL ? (struct mylite_sql_source_span){0} : name->span,
               "NOW"
           );
}

static bool column_default_function_call_fsp(
    const struct mylite_sql_ast_node *node,
    unsigned int *out_fsp
) {
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(node, 1U);
    size_t argument_count = arguments == NULL ? 0U : mylite_sql_ast_node_child_count(arguments);

    if (out_fsp != NULL) {
        *out_fsp = 0U;
    }
    if (node == NULL || node->kind != MYLITE_SQL_AST_FUNCTION_CALL) {
        return false;
    }
    if (argument_count == 0U) {
        return true;
    }
    if (argument_count == 1U) {
        return column_default_literal_fsp(mylite_ast_child_at(arguments, 0U), out_fsp);
    }
    return false;
}

static bool column_default_literal_fsp(
    const struct mylite_sql_ast_node *node,
    unsigned int *out_fsp
) {
    if (node == NULL || node->kind != MYLITE_SQL_AST_LITERAL ||
        node->literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        return false;
    }
    return parse_default_fsp_text(node->span.text, node->span.length, out_fsp);
}

static char *copy_current_timestamp_default_text_from_fsp(unsigned int fsp, bool parenthesized) {
    enum { default_text_capacity = 32U };

    char text[default_text_capacity];
    int length = 0;

    if (parenthesized) {
        length = fsp == 0U ? snprintf(text, sizeof(text), "now()")
                           : snprintf(text, sizeof(text), "now(%u)", fsp);
    } else {
        length = fsp == 0U ? snprintf(text, sizeof(text), "CURRENT_TIMESTAMP")
                           : snprintf(text, sizeof(text), "CURRENT_TIMESTAMP(%u)", fsp);
    }
    if (length < 0 || (size_t)length >= sizeof(text)) {
        return NULL;
    }
    return mylite_copy_span_text(text, (size_t)length);
}

static bool column_default_current_timestamp_text_fsp(const char *text, unsigned int *out_fsp) {
    if (out_fsp != NULL) {
        *out_fsp = 0U;
    }
    if (mylite_ascii_case_equal(text, "CURRENT_TIMESTAMP") ||
        mylite_ascii_case_equal(text, "CURRENT_TIMESTAMP()") ||
        mylite_ascii_case_equal(text, "now()")) {
        return true;
    }
    if (column_default_text_function_fsp(text, "CURRENT_TIMESTAMP", false, out_fsp)) {
        return true;
    }
    return column_default_text_function_fsp(text, "now", true, out_fsp);
}

static bool column_default_text_function_fsp(
    const char *text,
    const char *name,
    bool requires_parentheses,
    unsigned int *out_fsp
) {
    size_t text_length = text == NULL ? 0U : strlen(text);
    size_t name_length = name == NULL ? 0U : strlen(name);

    if (text == NULL || name == NULL || text_length <= name_length + 2U ||
        text[name_length] != '(' || text[text_length - 1U] != ')') {
        return false;
    }
    if (!mylite_span_equal_ci(
            (struct mylite_sql_source_span){.text = text, .length = name_length},
            name
        )) {
        return false;
    }
    if (requires_parentheses && text_length == name_length + 2U) {
        return true;
    }
    return parse_default_fsp_text(text + name_length + 1U, text_length - name_length - 2U, out_fsp);
}

static bool parse_default_fsp_text(const char *text, size_t length, unsigned int *out_fsp) {
    enum { max_fsp = 6U };

    unsigned int fsp = 0U;

    if (text == NULL || length == 0U) {
        return false;
    }
    for (size_t index = 0U; index < length; ++index) {
        unsigned char byte = (unsigned char)text[index];

        if (!isdigit(byte)) {
            return false;
        }
        fsp = (fsp * 10U) + (unsigned int)(byte - '0');
        if (fsp > max_fsp) {
            return false;
        }
    }
    if (out_fsp != NULL) {
        *out_fsp = fsp;
    }
    return true;
}

const struct mylite_sql_ast_node *mylite_ast_child_at(
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

const struct mylite_sql_ast_node *mylite_ast_find_child_kind(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_node_kind kind
) {
    const struct mylite_sql_ast_node *child = NULL;

    if (node == NULL) {
        return NULL;
    }

    for (child = node->first_child; child != NULL; child = child->next_sibling) {
        if (child->kind == kind) {
            return child;
        }
    }
    return NULL;
}

const struct mylite_sql_ast_node *mylite_ast_single_statement(
    const struct mylite_sql_ast_node *root
) {
    if (root == NULL || root->kind != MYLITE_SQL_AST_SCRIPT || root->first_child == NULL ||
        root->first_child->next_sibling != NULL) {
        return NULL;
    }

    return root->first_child;
}
