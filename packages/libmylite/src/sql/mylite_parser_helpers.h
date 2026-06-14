#ifndef MYLITE_SQL_MYLITE_PARSER_HELPERS_H
#define MYLITE_SQL_MYLITE_PARSER_HELPERS_H

#include "mylite_parser_internal.h"

#include <stdbool.h>
#include <stddef.h>

char mylite_sql_parser_ascii_upper(unsigned char byte);
bool mylite_sql_parser_token_text_equals(const struct mylite_sql_token *token, const char *text);
bool mylite_sql_parser_token_text_is_count_function_name(const struct mylite_sql_token *token);
bool mylite_sql_parser_token_text_is_generic_aggregate_window_function_name(
    const struct mylite_sql_token *token
);
bool mylite_sql_parser_is_parse_ok(const struct mylite_sql_parser_state *state);
bool mylite_sql_parser_sql_mode_has(
    const struct mylite_sql_parser_state *state,
    enum mylite_sql_mode mode
);
void mylite_sql_parser_set_state_status(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_parse_status status
);
struct mylite_sql_ast_node *mylite_sql_parser_make_node(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_ast_node_kind kind,
    struct mylite_sql_source_span span
);
struct mylite_sql_source_span mylite_sql_parser_span_from_token(const struct mylite_sql_token *token
);
struct mylite_sql_source_span mylite_sql_parser_span_join(
    struct mylite_sql_source_span left,
    struct mylite_sql_source_span right
);
struct mylite_sql_ast_node *mylite_sql_parser_child_at(
    struct mylite_sql_ast_node *node,
    size_t index
);
void mylite_sql_parser_apply_alter_table_options(
    struct mylite_sql_ast_node *statement,
    struct mylite_sql_alter_table_options options
);

#endif
