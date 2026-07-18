#include "mylite_parser_helpers.h"

#include <stdint.h>
#include <string.h>

char mylite_sql_parser_ascii_upper(unsigned char byte) {
    if (byte >= 'a' && byte <= 'z') {
        return (char)(byte - ('a' - 'A'));
    }
    return (char)byte;
}

bool mylite_sql_parser_is_parse_ok(const struct mylite_sql_parser_state *state) {
    if (state != NULL && state->result != NULL && state->result->status == MYLITE_SQL_PARSE_OK) {
        return true;
    }
    return false;
}

bool mylite_sql_parser_sql_mode_has(
    const struct mylite_sql_parser_state *state,
    enum mylite_sql_mode mode
) {
    if (state == NULL) {
        return false;
    }
    return (state->modes & (unsigned int)mode) != 0U;
}

void mylite_sql_parser_set_state_status(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_parse_status status
) {
    if (!mylite_sql_parser_is_parse_ok(state)) {
        return;
    }

    state->result->status = status;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_node(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_ast_node_kind kind,
    struct mylite_sql_source_span span
) {
    struct mylite_sql_ast_node *node = NULL;

    if (!mylite_sql_parser_is_parse_ok(state)) {
        return NULL;
    }
    if (!mylite_sql_source_span_is_valid(span)) {
        mylite_sql_parser_set_state_status(state, MYLITE_SQL_PARSE_SYNTAX_ERROR);
        return NULL;
    }

    node = mylite_sql_ast_new_node(&state->result->ast, kind, span);
    if (node == NULL) {
        mylite_sql_parser_set_state_status(state, MYLITE_SQL_PARSE_NOMEM);
    }
    return node;
}

struct mylite_sql_source_span mylite_sql_parser_span_from_token(const struct mylite_sql_token *token
) {
    if (token == NULL) {
        return (struct mylite_sql_source_span){0};
    }

    return (struct mylite_sql_source_span){
        .text = token->text,
        .length = token->length,
        .offset = token->offset,
        .source_length = token->source_length,
    };
}

struct mylite_sql_source_span mylite_sql_parser_span_join(
    struct mylite_sql_source_span left,
    struct mylite_sql_source_span right
) {
    struct mylite_sql_source_span start = left;
    uintptr_t left_source = 0U;
    uintptr_t right_source = 0U;
    size_t left_end = 0U;
    size_t right_end = 0U;
    size_t end = 0U;

    if (left.text == NULL || left.length == 0U) {
        return right;
    }
    if (right.text == NULL || right.length == 0U) {
        return left;
    }
    if (!mylite_sql_source_span_is_valid(left) || !mylite_sql_source_span_is_valid(right) ||
        left.source_length != right.source_length) {
        return (struct mylite_sql_source_span){.text = left.text, .length = SIZE_MAX};
    }

    left_source = (uintptr_t)left.text;
    right_source = (uintptr_t)right.text;
    if (left_source < left.offset || right_source < right.offset ||
        left_source - left.offset != right_source - right.offset) {
        return (struct mylite_sql_source_span){.text = left.text, .length = SIZE_MAX};
    }

    left_end = left.offset + left.length;
    right_end = right.offset + right.length;
    end = left_end > right_end ? left_end : right_end;

    if (right.offset < left.offset) {
        start = right;
    }

    start.length = end - start.offset;
    return start;
}

struct mylite_sql_ast_node *mylite_sql_parser_child_at(
    struct mylite_sql_ast_node *node,
    size_t index
) {
    struct mylite_sql_ast_node *child = NULL;

    if (node == NULL) {
        return NULL;
    }
    child = node->first_child;
    for (size_t child_index = 0U; child != NULL && child_index < index; ++child_index) {
        child = child->next_sibling;
    }
    return child;
}

void mylite_sql_parser_apply_alter_table_options(
    struct mylite_sql_ast_node *statement,
    struct mylite_sql_alter_table_options options
) {
    if (statement == NULL) {
        return;
    }

    mylite_sql_ast_node_set_alter_table_options(statement, options.algorithm, options.lock);
    if (options.has_span) {
        mylite_sql_ast_node_set_span(
            statement,
            mylite_sql_parser_span_join(statement->span, options.span)
        );
    }
}

bool mylite_sql_parser_token_text_equals(const struct mylite_sql_token *token, const char *text) {
    size_t length = 0U;

    if (token == NULL || text == NULL) {
        return false;
    }

    length = strlen(text);
    if (token->length != length) {
        return false;
    }

    if (memcmp(token->text, text, length) == 0) {
        return true;
    }

    for (size_t index = 0U; index < length; ++index) {
        if (mylite_sql_parser_ascii_upper((unsigned char)token->text[index]) != text[index]) {
            return false;
        }
    }

    return true;
}

bool mylite_sql_parser_token_text_is_count_function_name(const struct mylite_sql_token *token) {
    return mylite_sql_parser_token_text_equals(token, "COUNT");
}

bool mylite_sql_parser_token_text_is_generic_aggregate_window_function_name(
    const struct mylite_sql_token *token
) {
    static const char *const names[] = {
        "JSON_ARRAYAGG",
        "JSON_OBJECTAGG",
        "STD",
        "STDDEV",
        "STDDEV_POP",
        "STDDEV_SAMP",
        "VAR_POP",
        "VAR_SAMP",
        "VARIANCE",
    };

    for (size_t index = 0U; index < sizeof(names) / sizeof(names[0]); ++index) {
        if (mylite_sql_parser_token_text_equals(token, names[index])) {
            return true;
        }
    }
    return false;
}
