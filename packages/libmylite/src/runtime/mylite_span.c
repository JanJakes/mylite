#include "mylite_span.h"

#include <mylite/mylite.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

bool mylite_span_equal_ci(struct mylite_sql_source_span span, const char *text)
{
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

bool mylite_ascii_case_equal(const char *left, const char *right)
{
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

void mylite_uppercase_ascii_text(char *text)
{
    for (size_t index = 0U; text != NULL && text[index] != '\0'; ++index) {
        if (text[index] >= 'a' && text[index] <= 'z') {
            text[index] = (char)(text[index] - 'a' + 'A');
        }
    }
}

char *mylite_copy_schema_text_span(const struct mylite_sql_ast_node *node)
{
    if (node != NULL && node->kind == MYLITE_SQL_AST_LITERAL) {
        return mylite_copy_string_literal_span(node);
    }
    return mylite_copy_identifier_span(node);
}

char *mylite_copy_identifier_span(const struct mylite_sql_ast_node *node)
{
    const char *text = node == NULL ? NULL : node->span.text;
    size_t length = node == NULL ? 0U : node->span.length;
    char *copy = NULL;
    size_t output = 0U;

    if (text == NULL) {
        return NULL;
    }
    if (length < 2U || text[0] != '`' || text[length - 1U] != '`') {
        return mylite_copy_span_text(text, length);
    }

    copy = malloc(length - 1U);
    if (copy == NULL) {
        return NULL;
    }

    for (size_t index = 1U; index + 1U < length; ++index) {
        if (text[index] == '`' && index + 2U < length && text[index + 1U] == '`') {
            copy[output++] = '`';
            ++index;
        } else {
            copy[output++] = text[index];
        }
    }
    copy[output] = '\0';
    return copy;
}

int mylite_copy_identifier_parts(const struct mylite_sql_ast_node *identifier, char **parts,
                                 size_t *part_count)
{
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

char *mylite_copy_string_literal_span(const struct mylite_sql_ast_node *node)
{
    const char *text = node == NULL ? NULL : node->span.text;
    size_t length = node == NULL ? 0U : node->span.length;
    char quote = '\0';
    char *copy = NULL;
    size_t output = 0U;

    if (text == NULL) {
        return NULL;
    }
    if (length < 2U || (text[0] != '\'' && text[0] != '"')) {
        return mylite_copy_span_text(text, length);
    }

    quote = text[0];
    copy = malloc(length - 1U);
    if (copy == NULL) {
        return NULL;
    }

    for (size_t index = 1U; index + 1U < length; ++index) {
        if (text[index] == quote && index + 2U < length && text[index + 1U] == quote) {
            copy[output++] = quote;
            ++index;
        } else if (text[index] == '\\' && index + 2U < length) {
            copy[output++] = text[++index];
        } else {
            copy[output++] = text[index];
        }
    }
    copy[output] = '\0';
    return copy;
}

char *mylite_copy_unquoted_span_text(struct mylite_sql_source_span span)
{
    const char *text = span.text == NULL ? "" : span.text;
    size_t start = 0U;
    size_t end = span.text == NULL ? 0U : span.length;

    if (end >= 2U && (text[0] == '\'' || text[0] == '"') && text[end - 1U] == text[0]) {
        start = 1U;
        --end;
    }
    return mylite_copy_span_text(text + start, end - start);
}

char *mylite_copy_nonempty_cstring(const char *text)
{
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

char *mylite_copy_span_text(const char *text, size_t length)
{
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

bool mylite_span_contains_newline(const char *text, size_t length)
{
    for (size_t index = 0U; index < length; ++index) {
        if (text[index] == '\n' || text[index] == '\r') {
            return true;
        }
    }
    return false;
}

bool mylite_text_contains_word(const char *text, const char *word)
{
    if (text == NULL || word == NULL || word[0] == '\0') {
        return false;
    }
    return strstr(text, word) != NULL;
}

const struct mylite_sql_ast_node *mylite_ast_child_at(const struct mylite_sql_ast_node *node,
                                                      size_t index)
{
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

const struct mylite_sql_ast_node *mylite_ast_find_child_kind(const struct mylite_sql_ast_node *node,
                                                             enum mylite_sql_ast_node_kind kind)
{
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

const struct mylite_sql_ast_node *
mylite_ast_single_statement(const struct mylite_sql_ast_node *root)
{
    if (root == NULL || root->kind != MYLITE_SQL_AST_SCRIPT || root->first_child == NULL ||
        root->first_child->next_sibling != NULL) {
        return NULL;
    }

    return root->first_child;
}
