#ifndef MYLITE_SQL_MYLITE_PARSER_H
#define MYLITE_SQL_MYLITE_PARSER_H

#include "mylite_ast.h"
#include "mylite_lexer.h"

#include <stdbool.h>
#include <stddef.h>

enum mylite_sql_parse_status {
    MYLITE_SQL_PARSE_OK = 0,
    MYLITE_SQL_PARSE_MISUSE = 1,
    MYLITE_SQL_PARSE_NOMEM = 2,
    MYLITE_SQL_PARSE_LEXER_ERROR = 3,
    MYLITE_SQL_PARSE_SYNTAX_ERROR = 4,
    MYLITE_SQL_PARSE_STACK_OVERFLOW = 5,
};

struct mylite_sql_parse_config {
    const char *input;
    size_t length;
    unsigned int modes;
    bool allow_parameters;
};

struct mylite_sql_parse_result {
    struct mylite_sql_ast ast;
    struct mylite_sql_ast_node *root;
    size_t parameter_count;
    size_t retry_tokenization_count;
    size_t retry_callback_count;
    size_t retry_handled_count;
    enum mylite_sql_parse_status status;
    struct mylite_sql_token error_token;
    int parser_token;
};

enum mylite_sql_parse_status mylite_sql_parse(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *out_result
);

void mylite_sql_parse_result_deinit(struct mylite_sql_parse_result *result);

const char *mylite_sql_parse_status_name(enum mylite_sql_parse_status status);

#endif
