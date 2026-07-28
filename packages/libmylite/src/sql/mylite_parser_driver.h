#ifndef MYLITE_SQL_MYLITE_PARSER_DRIVER_H
#define MYLITE_SQL_MYLITE_PARSER_DRIVER_H

#include "mylite_parser_internal.h"

#include <stddef.h>

struct mylite_sql_parser_retry_context;

void *mylite_sql_lemonAlloc(void *(*malloc_proc)(size_t));
void mylite_sql_lemon(
    void *parser,
    int parser_token,
    struct mylite_sql_token token,
    struct mylite_sql_parser_state *state
);
void mylite_sql_lemonFree(void *parser, void (*free_proc)(void *));

enum {
    mylite_sql_parser_version_comment_lexer_stack_limit = 16,
};

enum mylite_sql_parse_status mylite_sql_parser_parse_with_lemon(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *out_result
);
enum mylite_sql_parse_status mylite_sql_parser_parse_with_lemon_options(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *out_result,
    bool inject_parenthesized_row_constructors,
    struct mylite_sql_parser_retry_context *retry_context
);
void mylite_sql_parser_reset_parse_result(struct mylite_sql_parse_result *out_result);
enum mylite_sql_parse_status mylite_sql_parser_push_version_comment_payload_lexer(
    const struct mylite_sql_token *token,
    unsigned int modes,
    struct mylite_sql_lexer *lexer_stack,
    size_t *lexer_count
);

#endif
