#ifndef MYLITE_SQL_MYLITE_PARSER_TOKEN_MAP_H
#define MYLITE_SQL_MYLITE_PARSER_TOKEN_MAP_H

#include "mylite_parser_internal.h"

#include <stdbool.h>

struct mylite_sql_parser_token_map {
    int parser_token;
    bool previous_token_was_dot;
};

struct mylite_sql_parser_token_history {
    int previous_parser_token;
    int token_before_previous_parser_token;
};

bool mylite_sql_parser_map_lexer_token(
    const struct mylite_sql_parser_state *state,
    const struct mylite_sql_lexer *lexer,
    const struct mylite_sql_token *token,
    bool previous_token_was_dot,
    const struct mylite_sql_parser_token_history *history,
    struct mylite_sql_parser_token_map *out_map
);
bool mylite_sql_parser_should_skip_select_lock_target_list(
    const struct mylite_sql_token *token,
    const struct mylite_sql_parser_token_history *history
);
enum mylite_sql_parse_status mylite_sql_parser_skip_select_lock_target_list(
    struct mylite_sql_lexer *lexer,
    struct mylite_sql_token *out_next_token
);
bool mylite_sql_parser_token_can_be_select_lock_target_identifier(
    const struct mylite_sql_token *token
);
bool mylite_sql_parser_token_is_comment(enum mylite_sql_token_kind kind);
bool mylite_sql_parser_token_is_left_paren(const struct mylite_sql_token *token);
bool mylite_sql_parser_token_is_right_paren(const struct mylite_sql_token *token);
bool mylite_sql_parser_token_is_comma(const struct mylite_sql_token *token);
bool mylite_sql_parser_token_is_equal_sign(const struct mylite_sql_token *token);
bool mylite_sql_parser_token_is_string_literal(const struct mylite_sql_token *token);
void mylite_sql_parser_update_token_history(
    struct mylite_sql_parser_token_history *history,
    int parser_token
);

#endif
