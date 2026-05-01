#include "mylite_parser.h"

#include "mylite_parse.h"
#include "mylite_parser_internal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

void *mylite_sql_lemonAlloc(void *(*malloc_proc)(size_t));
void mylite_sql_lemon(void *parser, int parser_token, struct mylite_sql_token token,
                      struct mylite_sql_parser_state *state);
void mylite_sql_lemonFree(void *parser, void (*free_proc)(void *));

struct mylite_sql_parser_token_map {
    int parser_token;
    bool previous_token_was_dot;
};

struct mylite_sql_parse_error {
    enum mylite_sql_parse_status status;
    int parser_token;
    struct mylite_sql_token token;
};

struct mylite_sql_parser_keyword_token {
    const char *word;
    int parser_token;
};

static bool map_lexer_token(const struct mylite_sql_token *token, bool previous_token_was_dot,
                            struct mylite_sql_parser_token_map *out_map);
static void record_parse_error(struct mylite_sql_parse_result *result,
                               struct mylite_sql_parse_error error);
static bool is_comment_token(enum mylite_sql_token_kind kind);
static bool map_keyword_token(const struct mylite_sql_token *token, bool previous_token_was_dot,
                              int *out_parser_token);
static bool lookup_keyword_parser_token(const struct mylite_sql_token *token,
                                        int *out_parser_token);
static bool map_punctuation_token(const struct mylite_sql_token *token, int *out_parser_token);
static bool map_operator_token(const struct mylite_sql_token *token, int *out_parser_token);
static bool token_text_equals(const struct mylite_sql_token *token, const char *text);
static char ascii_upper(unsigned char byte);
static bool is_parse_ok(const struct mylite_sql_parser_state *state);
static void set_state_status(struct mylite_sql_parser_state *state,
                             enum mylite_sql_parse_status status);
static struct mylite_sql_ast_node *make_node(struct mylite_sql_parser_state *state,
                                             enum mylite_sql_ast_node_kind kind,
                                             struct mylite_sql_source_span span);
static struct mylite_sql_source_span span_from_token(const struct mylite_sql_token *token);
static struct mylite_sql_source_span span_join(struct mylite_sql_source_span left,
                                               struct mylite_sql_source_span right);

enum mylite_sql_parse_status mylite_sql_parse(struct mylite_sql_parse_config config,
                                              struct mylite_sql_parse_result *out_result)
{
    struct mylite_sql_parser_state state;
    struct mylite_sql_lexer lexer;
    void *parser = NULL;
    bool previous_token_was_dot = false;

    if (out_result == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }

    memset(out_result, 0, sizeof(*out_result));
    out_result->status = MYLITE_SQL_PARSE_OK;
    mylite_sql_ast_init(&out_result->ast);

    if (config.input == NULL && config.length != 0U) {
        out_result->status = MYLITE_SQL_PARSE_MISUSE;
        return out_result->status;
    }

    state = (struct mylite_sql_parser_state){
        .result = out_result,
        .accepted = false,
    };

    parser = mylite_sql_lemonAlloc(malloc);
    if (parser == NULL) {
        out_result->status = MYLITE_SQL_PARSE_NOMEM;
        return out_result->status;
    }

    mylite_sql_lexer_init(&lexer, (struct mylite_sql_lexer_config){
                                      .input = config.input,
                                      .length = config.length,
                                      .modes = config.modes,
                                  });
    for (;;) {
        struct mylite_sql_token token;
        struct mylite_sql_parser_token_map token_map;

        if (mylite_sql_lexer_next(&lexer, &token) != 0) {
            out_result->status = MYLITE_SQL_PARSE_MISUSE;
            break;
        }

        if (is_comment_token(token.kind)) {
            continue;
        }

        if (token.kind == MYLITE_SQL_TOKEN_ERROR) {
            record_parse_error(out_result, (struct mylite_sql_parse_error){
                                               .status = MYLITE_SQL_PARSE_LEXER_ERROR,
                                               .parser_token = 0,
                                               .token = token,
                                           });
            break;
        }

        if (!map_lexer_token(&token, previous_token_was_dot, &token_map)) {
            record_parse_error(out_result, (struct mylite_sql_parse_error){
                                               .status = MYLITE_SQL_PARSE_SYNTAX_ERROR,
                                               .parser_token = -1,
                                               .token = token,
                                           });
            break;
        }

        mylite_sql_lemon(parser, token_map.parser_token, token, &state);
        previous_token_was_dot = token_map.previous_token_was_dot;

        if (out_result->status != MYLITE_SQL_PARSE_OK || token.kind == MYLITE_SQL_TOKEN_EOF) {
            break;
        }
    }

    mylite_sql_lemonFree(parser, free);

    if (out_result->status == MYLITE_SQL_PARSE_OK && !state.accepted) {
        out_result->status = MYLITE_SQL_PARSE_SYNTAX_ERROR;
    }

    return out_result->status;
}

void mylite_sql_parse_result_deinit(struct mylite_sql_parse_result *result)
{
    if (result == NULL) {
        return;
    }

    mylite_sql_ast_deinit(&result->ast);
    memset(result, 0, sizeof(*result));
}

const char *mylite_sql_parse_status_name(enum mylite_sql_parse_status status)
{
    switch (status) {
    case MYLITE_SQL_PARSE_OK:
        return "ok";
    case MYLITE_SQL_PARSE_MISUSE:
        return "misuse";
    case MYLITE_SQL_PARSE_NOMEM:
        return "nomem";
    case MYLITE_SQL_PARSE_LEXER_ERROR:
        return "lexer_error";
    case MYLITE_SQL_PARSE_SYNTAX_ERROR:
        return "syntax_error";
    case MYLITE_SQL_PARSE_STACK_OVERFLOW:
        return "stack_overflow";
    }

    return "unknown";
}

void mylite_sql_parser_state_set_root(struct mylite_sql_parser_state *state,
                                      struct mylite_sql_ast_node *root)
{
    if (!is_parse_ok(state)) {
        return;
    }

    state->result->root = root;
}

void mylite_sql_parser_state_syntax_error(struct mylite_sql_parser_state *state, int parser_token,
                                          struct mylite_sql_token token)
{
    if (!is_parse_ok(state)) {
        return;
    }

    record_parse_error(state->result, (struct mylite_sql_parse_error){
                                          .status = MYLITE_SQL_PARSE_SYNTAX_ERROR,
                                          .parser_token = parser_token,
                                          .token = token,
                                      });
}

void mylite_sql_parser_state_parse_failed(struct mylite_sql_parser_state *state)
{
    if (!is_parse_ok(state)) {
        return;
    }

    set_state_status(state, MYLITE_SQL_PARSE_SYNTAX_ERROR);
}

void mylite_sql_parser_state_accept(struct mylite_sql_parser_state *state)
{
    if (!is_parse_ok(state)) {
        return;
    }

    state->accepted = true;
}

void mylite_sql_parser_state_stack_overflow(struct mylite_sql_parser_state *state)
{
    if (!is_parse_ok(state)) {
        return;
    }

    set_state_status(state, MYLITE_SQL_PARSE_STACK_OVERFLOW);
}

struct mylite_sql_ast_node *mylite_sql_parser_make_script(struct mylite_sql_parser_state *state)
{
    return make_node(state, MYLITE_SQL_AST_SCRIPT, (struct mylite_sql_source_span){0});
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_script_with_statement(struct mylite_sql_parser_state *state,
                                             struct mylite_sql_ast_node *statement)
{
    struct mylite_sql_ast_node *script = mylite_sql_parser_make_script(state);
    if (script == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(script, statement);
    if (statement != NULL) {
        mylite_sql_ast_node_set_span(script, statement->span);
    }
    return script;
}

struct mylite_sql_ast_node *
mylite_sql_parser_append_statement(struct mylite_sql_parser_state *state,
                                   struct mylite_sql_ast_node *script,
                                   struct mylite_sql_ast_node *statement)
{
    if (!is_parse_ok(state) || script == NULL) {
        return script;
    }

    mylite_sql_ast_node_append_child(script, statement);
    if (statement != NULL) {
        mylite_sql_ast_node_set_span(script, span_join(script->span, statement->span));
    }
    return script;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_select_statement(
    struct mylite_sql_parser_state *state, struct mylite_sql_token select_token,
    struct mylite_sql_ast_node *select_list, struct mylite_sql_ast_node *from_clause)
{
    struct mylite_sql_source_span span = span_from_token(&select_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (select_list != NULL) {
        span = span_join(span, select_list->span);
    }
    if (from_clause != NULL) {
        span = span_join(span, from_clause->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SELECT_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, select_list);
    mylite_sql_ast_node_append_child(statement, from_clause);
    return statement;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_use_statement(struct mylite_sql_parser_state *state,
                                     struct mylite_sql_token use_token,
                                     struct mylite_sql_ast_node *schema_name)
{
    struct mylite_sql_source_span span = span_from_token(&use_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (schema_name != NULL) {
        span = span_join(span, schema_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_USE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, schema_name);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_create_schema_statement(
    struct mylite_sql_parser_state *state, struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *if_not_exists, struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *options)
{
    struct mylite_sql_source_span span = span_from_token(&create_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (schema_name != NULL) {
        span = span_join(span, schema_name->span);
    }
    if (options != NULL) {
        span = span_join(span, options->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, if_not_exists);
    mylite_sql_ast_node_append_child(statement, schema_name);
    mylite_sql_ast_node_append_child(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_schema_statement(
    struct mylite_sql_parser_state *state, struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *schema_name, struct mylite_sql_ast_node *options)
{
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (schema_name != NULL) {
        span = span_join(span, schema_name->span);
    }
    if (options != NULL) {
        span = span_join(span, options->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_SCHEMA_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, schema_name);
    mylite_sql_ast_node_append_child(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_drop_schema_statement(
    struct mylite_sql_parser_state *state, struct mylite_sql_token drop_token,
    struct mylite_sql_ast_node *if_exists, struct mylite_sql_ast_node *schema_name)
{
    struct mylite_sql_source_span span = span_from_token(&drop_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (schema_name != NULL) {
        span = span_join(span, schema_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, if_exists);
    mylite_sql_ast_node_append_child(statement, schema_name);
    return statement;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_show_schemas_statement(struct mylite_sql_parser_state *state,
                                              struct mylite_sql_token show_token,
                                              struct mylite_sql_token schemas_token)
{
    return make_node(state, MYLITE_SQL_AST_SHOW_SCHEMAS_STATEMENT,
                     span_join(span_from_token(&show_token), span_from_token(&schemas_token)));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_if_exists(struct mylite_sql_parser_state *state,
                                                             struct mylite_sql_token if_token,
                                                             struct mylite_sql_token exists_token)
{
    return make_node(state, MYLITE_SQL_AST_IF_EXISTS,
                     span_join(span_from_token(&if_token), span_from_token(&exists_token)));
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_if_not_exists(struct mylite_sql_parser_state *state,
                                     struct mylite_sql_token if_token,
                                     struct mylite_sql_token exists_token)
{
    return make_node(state, MYLITE_SQL_AST_IF_NOT_EXISTS,
                     span_join(span_from_token(&if_token), span_from_token(&exists_token)));
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_schema_option_list(struct mylite_sql_parser_state *state)
{
    return make_node(state, MYLITE_SQL_AST_SCHEMA_OPTION_LIST, (struct mylite_sql_source_span){0});
}

struct mylite_sql_ast_node *
mylite_sql_parser_append_schema_option(struct mylite_sql_parser_state *state,
                                       struct mylite_sql_ast_node *list,
                                       struct mylite_sql_ast_node *option)
{
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, option);
    if (option != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, option->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_schema_option(
    struct mylite_sql_parser_state *state, struct mylite_sql_token start_token,
    enum mylite_sql_ast_schema_option schema_option, struct mylite_sql_ast_node *value)
{
    struct mylite_sql_source_span span = span_from_token(&start_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }

    option = make_node(state, MYLITE_SQL_AST_SCHEMA_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_schema_option(option, schema_option);
    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_wildcard_select_list(struct mylite_sql_parser_state *state,
                                            struct mylite_sql_token wildcard_token)
{
    return mylite_sql_parser_make_select_list(
        state, mylite_sql_parser_make_select_item(
                   state, mylite_sql_parser_make_wildcard(state, wildcard_token)));
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_select_list(struct mylite_sql_parser_state *state,
                                   struct mylite_sql_ast_node *item)
{
    struct mylite_sql_source_span span =
        item == NULL ? (struct mylite_sql_source_span){0} : item->span;
    struct mylite_sql_ast_node *list = make_node(state, MYLITE_SQL_AST_SELECT_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, item);
    return list;
}

struct mylite_sql_ast_node *
mylite_sql_parser_append_select_item(struct mylite_sql_parser_state *state,
                                     struct mylite_sql_ast_node *list,
                                     struct mylite_sql_ast_node *item)
{
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, item);
    if (item != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, item->span));
    }
    return list;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_select_item(struct mylite_sql_parser_state *state,
                                   struct mylite_sql_ast_node *expression)
{
    struct mylite_sql_source_span span =
        expression == NULL ? (struct mylite_sql_source_span){0} : expression->span;
    struct mylite_sql_ast_node *item = make_node(state, MYLITE_SQL_AST_SELECT_ITEM, span);
    if (item == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(item, expression);
    return item;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_from_dual(struct mylite_sql_parser_state *state,
                                                             struct mylite_sql_token from_token,
                                                             struct mylite_sql_token dual_token)
{
    return make_node(state, MYLITE_SQL_AST_FROM_DUAL,
                     span_join(span_from_token(&from_token), span_from_token(&dual_token)));
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_from_table(struct mylite_sql_parser_state *state,
                                  struct mylite_sql_token from_token,
                                  struct mylite_sql_ast_node *table_name)
{
    struct mylite_sql_source_span span = span_from_token(&from_token);
    struct mylite_sql_ast_node *from_table = NULL;

    if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    from_table = make_node(state, MYLITE_SQL_AST_FROM_TABLE, span);
    if (from_table == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(from_table, table_name);
    return from_table;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_identifier(struct mylite_sql_parser_state *state,
                                                              struct mylite_sql_token token)
{
    return make_node(state, MYLITE_SQL_AST_IDENTIFIER, span_from_token(&token));
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_qualified_identifier(struct mylite_sql_parser_state *state,
                                            struct mylite_sql_ast_node *left,
                                            struct mylite_sql_ast_node *right)
{
    struct mylite_sql_source_span span =
        left == NULL ? (struct mylite_sql_source_span){0} : left->span;
    struct mylite_sql_ast_node *identifier = NULL;

    if (right != NULL) {
        span = span_join(span, right->span);
    }

    identifier = make_node(state, MYLITE_SQL_AST_QUALIFIED_IDENTIFIER, span);
    if (identifier == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(identifier, left);
    mylite_sql_ast_node_append_child(identifier, right);
    return identifier;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_wildcard(struct mylite_sql_parser_state *state,
                                                            struct mylite_sql_token token)
{
    return make_node(state, MYLITE_SQL_AST_WILDCARD, span_from_token(&token));
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_literal(struct mylite_sql_parser_state *state, struct mylite_sql_token token,
                               enum mylite_sql_ast_literal_kind literal_kind)
{
    struct mylite_sql_ast_node *literal =
        make_node(state, MYLITE_SQL_AST_LITERAL, span_from_token(&token));
    if (literal == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_literal_kind(literal, literal_kind);
    return literal;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_unary_expression(
    struct mylite_sql_parser_state *state, struct mylite_sql_token operator_token,
    enum mylite_sql_ast_operator operator_kind, struct mylite_sql_ast_node *operand)
{
    struct mylite_sql_source_span span = span_from_token(&operator_token);
    struct mylite_sql_ast_node *expression = NULL;

    if (operand != NULL) {
        span = span_join(span, operand->span);
    }

    expression = make_node(state, MYLITE_SQL_AST_UNARY_EXPRESSION, span);
    if (expression == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_operator(expression, operator_kind);
    mylite_sql_ast_node_append_child(expression, operand);
    return expression;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_binary_expression(
    struct mylite_sql_parser_state *state, struct mylite_sql_ast_node *left,
    struct mylite_sql_token operator_token, enum mylite_sql_ast_operator operator_kind,
    struct mylite_sql_ast_node *right)
{
    struct mylite_sql_source_span span =
        left == NULL ? span_from_token(&operator_token) : left->span;
    struct mylite_sql_ast_node *expression = NULL;

    if (right != NULL) {
        span = span_join(span, right->span);
    }

    expression = make_node(state, MYLITE_SQL_AST_BINARY_EXPRESSION, span);
    if (expression == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_operator(expression, operator_kind);
    mylite_sql_ast_node_append_child(expression, left);
    mylite_sql_ast_node_append_child(expression, right);
    return expression;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_parenthesized_expression(
    struct mylite_sql_parser_state *state, struct mylite_sql_token left_paren,
    struct mylite_sql_ast_node *expression, struct mylite_sql_token right_paren)
{
    struct mylite_sql_ast_node *parenthesized =
        make_node(state, MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
                  span_join(span_from_token(&left_paren), span_from_token(&right_paren)));
    if (parenthesized == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(parenthesized, expression);
    return parenthesized;
}

static bool map_lexer_token(const struct mylite_sql_token *token, bool previous_token_was_dot,
                            struct mylite_sql_parser_token_map *out_map)
{
    int parser_token = 0;

    if (token == NULL || out_map == NULL) {
        return false;
    }

    if (token->kind == MYLITE_SQL_TOKEN_EOF) {
        *out_map = (struct mylite_sql_parser_token_map){
            .parser_token = 0,
            .previous_token_was_dot = false,
        };
        return true;
    }

    switch (token->kind) {
    case MYLITE_SQL_TOKEN_IDENTIFIER:
        parser_token = MYLITE_SQL_PARSE_IDENTIFIER;
        break;
    case MYLITE_SQL_TOKEN_QUOTED_IDENTIFIER:
        parser_token = MYLITE_SQL_PARSE_QUOTED_IDENTIFIER;
        break;
    case MYLITE_SQL_TOKEN_KEYWORD:
        if (!map_keyword_token(token, previous_token_was_dot, &parser_token)) {
            return false;
        }
        break;
    case MYLITE_SQL_TOKEN_STRING:
        parser_token = MYLITE_SQL_PARSE_STRING;
        break;
    case MYLITE_SQL_TOKEN_NATIONAL_STRING:
        parser_token = MYLITE_SQL_PARSE_NATIONAL_STRING;
        break;
    case MYLITE_SQL_TOKEN_HEX_LITERAL:
        parser_token = MYLITE_SQL_PARSE_HEX_LITERAL;
        break;
    case MYLITE_SQL_TOKEN_BIT_LITERAL:
        parser_token = MYLITE_SQL_PARSE_BIT_LITERAL;
        break;
    case MYLITE_SQL_TOKEN_INTEGER:
        parser_token = MYLITE_SQL_PARSE_INTEGER;
        break;
    case MYLITE_SQL_TOKEN_DECIMAL:
        parser_token = MYLITE_SQL_PARSE_DECIMAL;
        break;
    case MYLITE_SQL_TOKEN_FLOAT:
        parser_token = MYLITE_SQL_PARSE_FLOAT;
        break;
    case MYLITE_SQL_TOKEN_OPERATOR:
        if (!map_operator_token(token, &parser_token)) {
            return false;
        }
        break;
    case MYLITE_SQL_TOKEN_PUNCTUATION:
        if (!map_punctuation_token(token, &parser_token)) {
            return false;
        }
        break;
    case MYLITE_SQL_TOKEN_EOF:
    case MYLITE_SQL_TOKEN_ERROR:
    case MYLITE_SQL_TOKEN_COMMENT:
    case MYLITE_SQL_TOKEN_VERSION_COMMENT:
    case MYLITE_SQL_TOKEN_HINT_COMMENT:
    case MYLITE_SQL_TOKEN_USER_VARIABLE:
    case MYLITE_SQL_TOKEN_SYSTEM_VARIABLE:
    case MYLITE_SQL_TOKEN_PARAMETER:
        return false;
    }

    *out_map = (struct mylite_sql_parser_token_map){
        .parser_token = parser_token,
        .previous_token_was_dot = parser_token == MYLITE_SQL_PARSE_DOT,
    };
    return true;
}

static void record_parse_error(struct mylite_sql_parse_result *result,
                               struct mylite_sql_parse_error error)
{
    if (result == NULL || result->status != MYLITE_SQL_PARSE_OK) {
        return;
    }

    result->status = error.status;
    result->parser_token = error.parser_token;
    result->error_token = error.token;
}

static bool is_comment_token(enum mylite_sql_token_kind kind)
{
    if (kind == MYLITE_SQL_TOKEN_COMMENT || kind == MYLITE_SQL_TOKEN_VERSION_COMMENT ||
        kind == MYLITE_SQL_TOKEN_HINT_COMMENT) {
        return true;
    }
    return false;
}

static bool map_keyword_token(const struct mylite_sql_token *token, bool previous_token_was_dot,
                              int *out_parser_token)
{
    if (previous_token_was_dot) {
        *out_parser_token = MYLITE_SQL_PARSE_IDENTIFIER;
        return true;
    }

    if (lookup_keyword_parser_token(token, out_parser_token)) {
        return true;
    }

    if ((token->keyword_flags & MYLITE_SQL_KEYWORD_RESERVED) != 0U) {
        return false;
    }

    *out_parser_token = MYLITE_SQL_PARSE_IDENTIFIER;
    return true;
}

static bool lookup_keyword_parser_token(const struct mylite_sql_token *token, int *out_parser_token)
{
    static const struct mylite_sql_parser_keyword_token keywords[] = {
        {"ALTER", MYLITE_SQL_PARSE_ALTER},
        {"CHARACTER", MYLITE_SQL_PARSE_CHARACTER},
        {"CHARSET", MYLITE_SQL_PARSE_CHARSET},
        {"COLLATE", MYLITE_SQL_PARSE_COLLATE},
        {"CREATE", MYLITE_SQL_PARSE_CREATE},
        {"DATABASE", MYLITE_SQL_PARSE_DATABASE},
        {"DATABASES", MYLITE_SQL_PARSE_DATABASES},
        {"DEFAULT", MYLITE_SQL_PARSE_DEFAULT},
        {"DROP", MYLITE_SQL_PARSE_DROP},
        {"DUAL", MYLITE_SQL_PARSE_DUAL},
        {"ENCRYPTION", MYLITE_SQL_PARSE_ENCRYPTION},
        {"EXISTS", MYLITE_SQL_PARSE_EXISTS},
        {"FALSE", MYLITE_SQL_PARSE_FALSE},
        {"FROM", MYLITE_SQL_PARSE_FROM},
        {"IF", MYLITE_SQL_PARSE_IF},
        {"NOT", MYLITE_SQL_PARSE_NOT},
        {"NULL", MYLITE_SQL_PARSE_NULL},
        {"ONLY", MYLITE_SQL_PARSE_ONLY},
        {"READ", MYLITE_SQL_PARSE_READ},
        {"SCHEMA", MYLITE_SQL_PARSE_SCHEMA},
        {"SCHEMAS", MYLITE_SQL_PARSE_SCHEMAS},
        {"SELECT", MYLITE_SQL_PARSE_SELECT},
        {"SET", MYLITE_SQL_PARSE_SET},
        {"SHOW", MYLITE_SQL_PARSE_SHOW},
        {"TRUE", MYLITE_SQL_PARSE_TRUE},
        {"USE", MYLITE_SQL_PARSE_USE},
    };

    for (size_t index = 0U; index < sizeof(keywords) / sizeof(keywords[0]); ++index) {
        if (token_text_equals(token, keywords[index].word)) {
            *out_parser_token = keywords[index].parser_token;
            return true;
        }
    }
    return false;
}

static bool map_punctuation_token(const struct mylite_sql_token *token, int *out_parser_token)
{
    if (token->length != 1U) {
        return false;
    }

    switch (token->text[0]) {
    case ';':
        *out_parser_token = MYLITE_SQL_PARSE_SEMICOLON;
        return true;
    case ',':
        *out_parser_token = MYLITE_SQL_PARSE_COMMA;
        return true;
    case '.':
        *out_parser_token = MYLITE_SQL_PARSE_DOT;
        return true;
    case '(':
        *out_parser_token = MYLITE_SQL_PARSE_LPAREN;
        return true;
    case ')':
        *out_parser_token = MYLITE_SQL_PARSE_RPAREN;
        return true;
    default:
        return false;
    }
}

static bool map_operator_token(const struct mylite_sql_token *token, int *out_parser_token)
{
    switch (token->operator_kind) {
    case MYLITE_SQL_OPERATOR_PLUS:
        *out_parser_token = MYLITE_SQL_PARSE_PLUS;
        return true;
    case MYLITE_SQL_OPERATOR_MINUS:
        *out_parser_token = MYLITE_SQL_PARSE_MINUS;
        return true;
    case MYLITE_SQL_OPERATOR_STAR:
        *out_parser_token = MYLITE_SQL_PARSE_STAR;
        return true;
    case MYLITE_SQL_OPERATOR_SLASH:
        *out_parser_token = MYLITE_SQL_PARSE_SLASH;
        return true;
    case MYLITE_SQL_OPERATOR_NONE:
    case MYLITE_SQL_OPERATOR_JSON_UNQUOTE_EXTRACT:
    case MYLITE_SQL_OPERATOR_JSON_EXTRACT:
    case MYLITE_SQL_OPERATOR_NULL_SAFE_EQUAL:
    case MYLITE_SQL_OPERATOR_LEFT_SHIFT:
    case MYLITE_SQL_OPERATOR_RIGHT_SHIFT:
    case MYLITE_SQL_OPERATOR_LESS_EQUAL:
    case MYLITE_SQL_OPERATOR_GREATER_EQUAL:
    case MYLITE_SQL_OPERATOR_NOT_EQUAL:
    case MYLITE_SQL_OPERATOR_LOGICAL_AND:
    case MYLITE_SQL_OPERATOR_LOGICAL_OR:
    case MYLITE_SQL_OPERATOR_ASSIGN:
        return false;
    case MYLITE_SQL_OPERATOR_EQUAL:
        *out_parser_token = MYLITE_SQL_PARSE_EQ;
        return true;
    case MYLITE_SQL_OPERATOR_LESS:
    case MYLITE_SQL_OPERATOR_GREATER:
    case MYLITE_SQL_OPERATOR_PERCENT:
    case MYLITE_SQL_OPERATOR_NOT:
    case MYLITE_SQL_OPERATOR_BITWISE_NOT:
    case MYLITE_SQL_OPERATOR_BITWISE_XOR:
    case MYLITE_SQL_OPERATOR_BITWISE_AND:
    case MYLITE_SQL_OPERATOR_BITWISE_OR:
        return false;
    }

    return false;
}

static bool token_text_equals(const struct mylite_sql_token *token, const char *text)
{
    size_t length = strlen(text);

    if (token->length != length) {
        return false;
    }

    for (size_t index = 0U; index < length; ++index) {
        if (ascii_upper((unsigned char)token->text[index]) != text[index]) {
            return false;
        }
    }

    return true;
}

static char ascii_upper(unsigned char byte)
{
    if (byte >= 'a' && byte <= 'z') {
        return (char)(byte - ('a' - 'A'));
    }
    return (char)byte;
}

static bool is_parse_ok(const struct mylite_sql_parser_state *state)
{
    if (state != NULL && state->result != NULL && state->result->status == MYLITE_SQL_PARSE_OK) {
        return true;
    }
    return false;
}

static void set_state_status(struct mylite_sql_parser_state *state,
                             enum mylite_sql_parse_status status)
{
    if (!is_parse_ok(state)) {
        return;
    }

    state->result->status = status;
}

static struct mylite_sql_ast_node *make_node(struct mylite_sql_parser_state *state,
                                             enum mylite_sql_ast_node_kind kind,
                                             struct mylite_sql_source_span span)
{
    struct mylite_sql_ast_node *node = NULL;

    if (!is_parse_ok(state)) {
        return NULL;
    }

    node = mylite_sql_ast_new_node(&state->result->ast, kind, span);
    if (node == NULL) {
        set_state_status(state, MYLITE_SQL_PARSE_NOMEM);
    }
    return node;
}

static struct mylite_sql_source_span span_from_token(const struct mylite_sql_token *token)
{
    if (token == NULL) {
        return (struct mylite_sql_source_span){0};
    }

    return (struct mylite_sql_source_span){
        .text = token->text,
        .length = token->length,
        .offset = token->offset,
        .line = token->line,
        .column = token->column,
    };
}

static struct mylite_sql_source_span span_join(struct mylite_sql_source_span left,
                                               struct mylite_sql_source_span right)
{
    struct mylite_sql_source_span start = left;
    size_t left_end = left.offset + left.length;
    size_t right_end = right.offset + right.length;
    size_t end = left_end > right_end ? left_end : right_end;

    if (left.text == NULL || left.length == 0U) {
        return right;
    }
    if (right.text == NULL || right.length == 0U) {
        return left;
    }

    if (right.offset < left.offset) {
        start = right;
    }

    start.length = end - start.offset;
    return start;
}
