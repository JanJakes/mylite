#include "mylite_parser.h"

#include "mylite_parse.h"
#include "mylite_parser_internal.h"
#include "types/mylite_column_type.h"

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

static unsigned int parse_display_width(const struct mylite_sql_ast_node *display_width);
static bool parse_column_length(const struct mylite_sql_ast_node *length, uint64_t *out_length);
static struct mylite_sql_ast_node *
make_checked_integer_literal(struct mylite_sql_parser_state *state,
                             struct mylite_sql_token integer_token);
static const char *column_type_descriptor_name(enum mylite_sql_ast_column_type column_type);
static bool column_type_uses_string_binary_descriptor(enum mylite_sql_ast_column_type column_type);
static bool column_type_uses_numeric_descriptor(enum mylite_sql_ast_column_type column_type);
static bool column_type_uses_temporal_descriptor(enum mylite_sql_ast_column_type column_type);
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
static bool transaction_characteristics_conflict(const struct mylite_sql_ast_node *left,
                                                 const struct mylite_sql_ast_node *right);
static bool expression_contains_function_call(const struct mylite_sql_ast_node *expression);
static void set_syntax_error_at_span(struct mylite_sql_parser_state *state,
                                     struct mylite_sql_source_span span);
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
    struct mylite_sql_ast_node *select_list, struct mylite_sql_ast_node *from_clause,
    struct mylite_sql_ast_node *where_clause, struct mylite_sql_ast_node *order_by_clause,
    struct mylite_sql_ast_node *limit_clause)
{
    struct mylite_sql_source_span span = span_from_token(&select_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (select_list != NULL) {
        span = span_join(span, select_list->span);
    }
    if (from_clause != NULL) {
        span = span_join(span, from_clause->span);
    }
    if (where_clause != NULL) {
        span = span_join(span, where_clause->span);
    }
    if (order_by_clause != NULL) {
        span = span_join(span, order_by_clause->span);
    }
    if (limit_clause != NULL) {
        span = span_join(span, limit_clause->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SELECT_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, select_list);
    mylite_sql_ast_node_append_child(statement, from_clause);
    mylite_sql_ast_node_append_child(statement, where_clause);
    mylite_sql_ast_node_append_child(statement, order_by_clause);
    mylite_sql_ast_node_append_child(statement, limit_clause);
    return statement;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_where_clause(struct mylite_sql_parser_state *state,
                                    struct mylite_sql_token where_token,
                                    struct mylite_sql_ast_node *expression)
{
    struct mylite_sql_source_span span = span_from_token(&where_token);
    struct mylite_sql_ast_node *where_clause = NULL;

    if (expression != NULL) {
        span = span_join(span, expression->span);
    }

    where_clause = make_node(state, MYLITE_SQL_AST_WHERE_CLAUSE, span);
    if (where_clause == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(where_clause, expression);
    return where_clause;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_order_by_clause(
    struct mylite_sql_parser_state *state, struct mylite_sql_token order_token,
    struct mylite_sql_token by_token, struct mylite_sql_ast_node *items)
{
    struct mylite_sql_source_span span =
        span_join(span_from_token(&order_token), span_from_token(&by_token));
    struct mylite_sql_ast_node *order_by_clause = NULL;

    if (items != NULL) {
        span = span_join(span, items->span);
    }

    order_by_clause = make_node(state, MYLITE_SQL_AST_ORDER_BY_CLAUSE, span);
    if (order_by_clause == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(order_by_clause, items);
    return order_by_clause;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_order_item_list(struct mylite_sql_parser_state *state,
                                       struct mylite_sql_ast_node *item)
{
    struct mylite_sql_ast_node *list =
        make_node(state, MYLITE_SQL_AST_ORDER_ITEM_LIST,
                  item == NULL ? (struct mylite_sql_source_span){0} : item->span);

    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, item);
    return list;
}

struct mylite_sql_ast_node *
mylite_sql_parser_append_order_item(struct mylite_sql_parser_state *state,
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
mylite_sql_parser_make_order_item(struct mylite_sql_parser_state *state,
                                  struct mylite_sql_ast_node *expression,
                                  struct mylite_sql_token direction_token)
{
    enum mylite_sql_ast_key_part_order direction = MYLITE_SQL_AST_KEY_PART_ORDER_ASC;
    struct mylite_sql_source_span span =
        expression == NULL ? (struct mylite_sql_source_span){0} : expression->span;
    struct mylite_sql_ast_node *item = NULL;

    if (direction_token.text != NULL) {
        span = span_join(span, span_from_token(&direction_token));
        if (token_text_equals(&direction_token, "DESC")) {
            direction = MYLITE_SQL_AST_KEY_PART_ORDER_DESC;
        }
    }

    item = make_node(state, MYLITE_SQL_AST_ORDER_ITEM, span);
    if (item == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_key_part_order(item, direction);
    mylite_sql_ast_node_append_child(item, expression);
    return item;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_limit_clause(
    struct mylite_sql_parser_state *state, struct mylite_sql_token limit_token,
    struct mylite_sql_ast_node *offset_bound, struct mylite_sql_ast_node *row_count_bound)
{
    struct mylite_sql_source_span span = span_from_token(&limit_token);
    struct mylite_sql_ast_node *limit_clause = NULL;

    if (offset_bound == NULL) {
        offset_bound =
            make_node(state, MYLITE_SQL_AST_LIMIT_BOUND, (struct mylite_sql_source_span){0});
        if (offset_bound == NULL) {
            return NULL;
        }
        mylite_sql_ast_node_set_limit_bound_value(offset_bound, 0U);
    }
    if (row_count_bound != NULL) {
        span = span_join(span, row_count_bound->span);
    }
    if (offset_bound->span.text != NULL) {
        span = span_join(span, offset_bound->span);
    }

    limit_clause = make_node(state, MYLITE_SQL_AST_LIMIT_CLAUSE, span);
    if (limit_clause == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(limit_clause, offset_bound);
    mylite_sql_ast_node_append_child(limit_clause, row_count_bound);
    return limit_clause;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_limit_bound(struct mylite_sql_parser_state *state,
                                   struct mylite_sql_token integer_token)
{
    struct mylite_sql_ast_node *bound =
        make_node(state, MYLITE_SQL_AST_LIMIT_BOUND, span_from_token(&integer_token));
    uint64_t value = 0U;

    if (bound == NULL) {
        return NULL;
    }
    if (!parse_column_length(bound, &value)) {
        mylite_sql_parser_state_syntax_error(state, MYLITE_SQL_PARSE_INTEGER, integer_token);
        return NULL;
    }

    mylite_sql_ast_node_set_limit_bound_value(bound, value);
    return bound;
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

struct mylite_sql_ast_node *mylite_sql_parser_make_drop_table_statement(
    struct mylite_sql_parser_state *state, struct mylite_sql_parser_drop_table_tokens tokens,
    struct mylite_sql_ast_node *if_exists, struct mylite_sql_ast_node *table_names)
{
    struct mylite_sql_source_span span = span_from_token(&tokens.drop);
    struct mylite_sql_ast_node *statement = NULL;

    if (table_names != NULL) {
        span = span_join(span, table_names->span);
    }
    if (tokens.mode.text != NULL) {
        span = span_join(span, span_from_token(&tokens.mode));
    }

    statement = make_node(state, MYLITE_SQL_AST_DROP_TABLE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }
    if (tokens.temporary.text != NULL) {
        mylite_sql_ast_node_set_drop_table_temporary(statement);
    }
    if (tokens.mode.text != NULL) {
        if (token_text_equals(&tokens.mode, "RESTRICT")) {
            mylite_sql_ast_node_set_drop_table_restrict(statement);
        } else if (token_text_equals(&tokens.mode, "CASCADE")) {
            mylite_sql_ast_node_set_drop_table_cascade(statement);
        }
    }

    mylite_sql_ast_node_append_child(statement, table_names);
    mylite_sql_ast_node_append_child(statement, if_exists);
    return statement;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_table_name_list(struct mylite_sql_parser_state *state,
                                       struct mylite_sql_ast_node *table_name)
{
    struct mylite_sql_ast_node *list = NULL;

    if (table_name == NULL) {
        return NULL;
    }

    list = make_node(state, MYLITE_SQL_AST_TABLE_NAME_LIST, table_name->span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, table_name);
    return list;
}

struct mylite_sql_ast_node *
mylite_sql_parser_append_table_name(struct mylite_sql_parser_state *state,
                                    struct mylite_sql_ast_node *list,
                                    struct mylite_sql_ast_node *table_name)
{
    (void)state;

    if (list == NULL || table_name == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, table_name);
    mylite_sql_ast_node_set_span(list, span_join(list->span, table_name->span));
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_values_statement(
    struct mylite_sql_parser_state *state, struct mylite_sql_token insert_token,
    struct mylite_sql_ast_node *table_name, struct mylite_sql_ast_node *columns,
    struct mylite_sql_ast_node *rows)
{
    struct mylite_sql_source_span span = span_from_token(&insert_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }
    if (columns != NULL && columns->span.text != NULL) {
        span = span_join(span, columns->span);
    }
    if (rows != NULL) {
        span = span_join(span, rows->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_INSERT_VALUES_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, columns);
    mylite_sql_ast_node_append_child(statement, rows);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_set_statement(
    struct mylite_sql_parser_state *state, struct mylite_sql_token insert_token,
    struct mylite_sql_ast_node *table_name, struct mylite_sql_ast_node *assignments)
{
    struct mylite_sql_source_span span = span_from_token(&insert_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }
    if (assignments != NULL) {
        span = span_join(span, assignments->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_INSERT_SET_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, assignments);
    return statement;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_insert_column_list(struct mylite_sql_parser_state *state,
                                          struct mylite_sql_ast_node *column)
{
    struct mylite_sql_source_span span =
        column == NULL ? (struct mylite_sql_source_span){0} : column->span;
    struct mylite_sql_ast_node *list = make_node(state, MYLITE_SQL_AST_INSERT_COLUMN_LIST, span);

    if (list == NULL) {
        return NULL;
    }
    mylite_sql_ast_node_append_child(list, column);
    return list;
}

struct mylite_sql_ast_node *
mylite_sql_parser_append_insert_column(struct mylite_sql_parser_state *state,
                                       struct mylite_sql_ast_node *list,
                                       struct mylite_sql_ast_node *column)
{
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, column);
    if (column != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, column->span));
    }
    return list;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_insert_row_list(struct mylite_sql_parser_state *state,
                                       struct mylite_sql_ast_node *row)
{
    struct mylite_sql_source_span span =
        row == NULL ? (struct mylite_sql_source_span){0} : row->span;
    struct mylite_sql_ast_node *list = make_node(state, MYLITE_SQL_AST_INSERT_ROW_LIST, span);

    if (list == NULL) {
        return NULL;
    }
    mylite_sql_ast_node_append_child(list, row);
    return list;
}

struct mylite_sql_ast_node *
mylite_sql_parser_append_insert_row(struct mylite_sql_parser_state *state,
                                    struct mylite_sql_ast_node *list,
                                    struct mylite_sql_ast_node *row)
{
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, row);
    if (row != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, row->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_row(struct mylite_sql_parser_state *state,
                                                              struct mylite_sql_token start_token,
                                                              struct mylite_sql_ast_node *values,
                                                              struct mylite_sql_token end_token)
{
    struct mylite_sql_source_span span = span_from_token(&start_token);
    struct mylite_sql_ast_node *row = NULL;

    if (values != NULL) {
        span = span_join(span, values->span);
    }
    if (end_token.text != NULL) {
        span = span_join(span, span_from_token(&end_token));
    }

    row = make_node(state, MYLITE_SQL_AST_INSERT_ROW, span);
    if (row == NULL) {
        return NULL;
    }
    if (values != NULL) {
        for (struct mylite_sql_ast_node *value = values->first_child; value != NULL;) {
            struct mylite_sql_ast_node *next = value->next_sibling;
            mylite_sql_ast_node_append_child(row, value);
            value = next;
        }
    }
    return row;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_insert_value_list(struct mylite_sql_parser_state *state,
                                         struct mylite_sql_ast_node *value)
{
    struct mylite_sql_source_span span =
        value == NULL ? (struct mylite_sql_source_span){0} : value->span;
    struct mylite_sql_ast_node *list = make_node(state, MYLITE_SQL_AST_INSERT_VALUE_LIST, span);

    if (list == NULL) {
        return NULL;
    }
    mylite_sql_ast_node_append_child(list, value);
    return list;
}

struct mylite_sql_ast_node *
mylite_sql_parser_append_insert_value(struct mylite_sql_parser_state *state,
                                      struct mylite_sql_ast_node *list,
                                      struct mylite_sql_ast_node *value)
{
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, value);
    if (value != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, value->span));
    }
    return list;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_insert_set_assignment_list(struct mylite_sql_parser_state *state,
                                                  struct mylite_sql_ast_node *assignment)
{
    struct mylite_sql_source_span span =
        assignment == NULL ? (struct mylite_sql_source_span){0} : assignment->span;
    struct mylite_sql_ast_node *list =
        make_node(state, MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT_LIST, span);

    if (list == NULL) {
        return NULL;
    }
    mylite_sql_ast_node_append_child(list, assignment);
    return list;
}

struct mylite_sql_ast_node *
mylite_sql_parser_append_insert_set_assignment(struct mylite_sql_parser_state *state,
                                               struct mylite_sql_ast_node *list,
                                               struct mylite_sql_ast_node *assignment)
{
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, assignment);
    if (assignment != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, assignment->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_set_assignment(
    struct mylite_sql_parser_state *state, struct mylite_sql_ast_node *target,
    struct mylite_sql_token equal_token, struct mylite_sql_ast_node *value)
{
    struct mylite_sql_source_span span =
        target == NULL ? span_from_token(&equal_token) : target->span;
    struct mylite_sql_ast_node *assignment = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }

    assignment = make_node(state, MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT, span);
    if (assignment == NULL) {
        return NULL;
    }
    mylite_sql_ast_node_append_child(assignment, target);
    mylite_sql_ast_node_append_child(assignment, value);
    return assignment;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_update_statement(
    struct mylite_sql_parser_state *state, struct mylite_sql_token update_token,
    struct mylite_sql_ast_node *target, struct mylite_sql_ast_node *assignments,
    struct mylite_sql_ast_node *where_clause, struct mylite_sql_ast_node *order_by_clause,
    struct mylite_sql_ast_node *limit_clause)
{
    struct mylite_sql_source_span span = span_from_token(&update_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (target != NULL) {
        span = span_join(span, target->span);
    }
    if (assignments != NULL) {
        span = span_join(span, assignments->span);
    }
    if (where_clause != NULL) {
        span = span_join(span, where_clause->span);
    }
    if (order_by_clause != NULL) {
        span = span_join(span, order_by_clause->span);
    }
    if (limit_clause != NULL) {
        span = span_join(span, limit_clause->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_UPDATE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, target);
    mylite_sql_ast_node_append_child(statement, assignments);
    mylite_sql_ast_node_append_child(statement, where_clause);
    mylite_sql_ast_node_append_child(statement, order_by_clause);
    mylite_sql_ast_node_append_child(statement, limit_clause);
    return statement;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_update_target(struct mylite_sql_parser_state *state,
                                     struct mylite_sql_ast_node *table_name,
                                     struct mylite_sql_ast_node *alias)
{
    struct mylite_sql_source_span span =
        table_name == NULL ? (struct mylite_sql_source_span){0} : table_name->span;
    struct mylite_sql_ast_node *target = NULL;

    if (alias != NULL) {
        span = span_join(span, alias->span);
    }

    target = make_node(state, MYLITE_SQL_AST_UPDATE_TARGET, span);
    if (target == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(target, table_name);
    mylite_sql_ast_node_append_child(target, alias);
    return target;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_update_assignment_list(struct mylite_sql_parser_state *state,
                                              struct mylite_sql_ast_node *assignment)
{
    struct mylite_sql_source_span span =
        assignment == NULL ? (struct mylite_sql_source_span){0} : assignment->span;
    struct mylite_sql_ast_node *list =
        make_node(state, MYLITE_SQL_AST_UPDATE_ASSIGNMENT_LIST, span);

    if (list == NULL) {
        return NULL;
    }
    mylite_sql_ast_node_append_child(list, assignment);
    return list;
}

struct mylite_sql_ast_node *
mylite_sql_parser_append_update_assignment(struct mylite_sql_parser_state *state,
                                           struct mylite_sql_ast_node *list,
                                           struct mylite_sql_ast_node *assignment)
{
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, assignment);
    if (assignment != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, assignment->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_update_assignment(
    struct mylite_sql_parser_state *state, struct mylite_sql_ast_node *target,
    struct mylite_sql_token equal_token, struct mylite_sql_ast_node *value)
{
    struct mylite_sql_source_span span =
        target == NULL ? span_from_token(&equal_token) : target->span;
    struct mylite_sql_ast_node *assignment = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }

    assignment = make_node(state, MYLITE_SQL_AST_UPDATE_ASSIGNMENT, span);
    if (assignment == NULL) {
        return NULL;
    }
    mylite_sql_ast_node_append_child(assignment, target);
    mylite_sql_ast_node_append_child(assignment, value);
    return assignment;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_update_limit_clause(struct mylite_sql_parser_state *state,
                                           struct mylite_sql_token limit_token,
                                           struct mylite_sql_ast_node *row_count_bound)
{
    struct mylite_sql_source_span span = span_from_token(&limit_token);
    struct mylite_sql_ast_node *limit_clause = NULL;

    if (row_count_bound != NULL) {
        span = span_join(span, row_count_bound->span);
    }

    limit_clause = make_node(state, MYLITE_SQL_AST_UPDATE_LIMIT_CLAUSE, span);
    if (limit_clause == NULL) {
        return NULL;
    }
    mylite_sql_ast_node_append_child(limit_clause, row_count_bound);
    return limit_clause;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_delete_statement(
    struct mylite_sql_parser_state *state, struct mylite_sql_token delete_token,
    struct mylite_sql_ast_node *target, struct mylite_sql_ast_node *where_clause,
    struct mylite_sql_ast_node *order_by_clause, struct mylite_sql_ast_node *limit_clause)
{
    struct mylite_sql_source_span span = span_from_token(&delete_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (target != NULL) {
        span = span_join(span, target->span);
    }
    if (where_clause != NULL) {
        span = span_join(span, where_clause->span);
    }
    if (order_by_clause != NULL) {
        span = span_join(span, order_by_clause->span);
    }
    if (limit_clause != NULL) {
        span = span_join(span, limit_clause->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_DELETE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, target);
    mylite_sql_ast_node_append_child(statement, where_clause);
    mylite_sql_ast_node_append_child(statement, order_by_clause);
    mylite_sql_ast_node_append_child(statement, limit_clause);
    return statement;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_start_transaction_statement(struct mylite_sql_parser_state *state,
                                                   struct mylite_sql_token start_token,
                                                   struct mylite_sql_ast_node *characteristics)
{
    struct mylite_sql_source_span span = span_from_token(&start_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (characteristics != NULL) {
        span = span_join(span, characteristics->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_START_TRANSACTION_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, characteristics);
    return statement;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_begin_transaction_statement(struct mylite_sql_parser_state *state,
                                                   struct mylite_sql_parser_statement_tokens tokens)
{
    struct mylite_sql_source_span span = span_from_token(&tokens.start);

    if (tokens.end.text != NULL) {
        span = span_join(span, span_from_token(&tokens.end));
    }

    return make_node(state, MYLITE_SQL_AST_BEGIN_TRANSACTION_STATEMENT, span);
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_transaction_characteristic_list(struct mylite_sql_parser_state *state,
                                                       struct mylite_sql_ast_node *characteristic)
{
    struct mylite_sql_ast_node *list = NULL;

    if (characteristic == NULL) {
        return NULL;
    }

    list = make_node(state, MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC_LIST, characteristic->span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, characteristic);
    return list;
}

struct mylite_sql_ast_node *
mylite_sql_parser_append_transaction_characteristic(struct mylite_sql_parser_state *state,
                                                    struct mylite_sql_ast_node *list,
                                                    struct mylite_sql_ast_node *characteristic)
{
    const struct mylite_sql_ast_node *existing = NULL;

    if (!is_parse_ok(state) || list == NULL || characteristic == NULL) {
        return list;
    }

    for (existing = list->first_child; existing != NULL; existing = existing->next_sibling) {
        if (transaction_characteristics_conflict(existing, characteristic)) {
            set_syntax_error_at_span(state, characteristic->span);
            return list;
        }
    }

    mylite_sql_ast_node_append_child(list, characteristic);
    mylite_sql_ast_node_set_span(list, span_join(list->span, characteristic->span));
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_transaction_access_mode(
    struct mylite_sql_parser_state *state, struct mylite_sql_token read_token,
    struct mylite_sql_token end_token, enum mylite_sql_ast_transaction_access_mode access_mode)
{
    struct mylite_sql_ast_node *characteristic =
        make_node(state, MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC,
                  span_join(span_from_token(&read_token), span_from_token(&end_token)));

    if (characteristic == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_transaction_access_mode(characteristic, access_mode);
    return characteristic;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_transaction_consistent_snapshot(struct mylite_sql_parser_state *state,
                                                       struct mylite_sql_token with_token,
                                                       struct mylite_sql_token snapshot_token)
{
    struct mylite_sql_ast_node *characteristic =
        make_node(state, MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC,
                  span_join(span_from_token(&with_token), span_from_token(&snapshot_token)));

    if (characteristic == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_transaction_consistent_snapshot(characteristic);
    return characteristic;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_commit_statement(struct mylite_sql_parser_state *state,
                                        struct mylite_sql_parser_statement_tokens tokens,
                                        struct mylite_sql_ast_node *completion)
{
    struct mylite_sql_source_span span = span_from_token(&tokens.start);
    struct mylite_sql_ast_node *statement = NULL;

    if (tokens.end.text != NULL) {
        span = span_join(span, span_from_token(&tokens.end));
    }
    if (completion != NULL && completion->span.text != NULL) {
        span = span_join(span, completion->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_COMMIT_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, completion);
    return statement;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_rollback_statement(struct mylite_sql_parser_state *state,
                                          struct mylite_sql_parser_statement_tokens tokens,
                                          struct mylite_sql_ast_node *completion)
{
    struct mylite_sql_source_span span = span_from_token(&tokens.start);
    struct mylite_sql_ast_node *statement = NULL;

    if (tokens.end.text != NULL) {
        span = span_join(span, span_from_token(&tokens.end));
    }
    if (completion != NULL && completion->span.text != NULL) {
        span = span_join(span, completion->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ROLLBACK_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, completion);
    return statement;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_savepoint_statement(struct mylite_sql_parser_state *state,
                                           struct mylite_sql_token savepoint_token,
                                           struct mylite_sql_ast_node *name)
{
    struct mylite_sql_source_span span = span_from_token(&savepoint_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (name != NULL) {
        span = span_join(span, name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SAVEPOINT_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, name);
    return statement;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_rollback_to_savepoint_statement(struct mylite_sql_parser_state *state,
                                                       struct mylite_sql_token rollback_token,
                                                       struct mylite_sql_ast_node *name)
{
    struct mylite_sql_source_span span = span_from_token(&rollback_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (name != NULL) {
        span = span_join(span, name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ROLLBACK_TO_SAVEPOINT_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, name);
    return statement;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_release_savepoint_statement(struct mylite_sql_parser_state *state,
                                                   struct mylite_sql_token release_token,
                                                   struct mylite_sql_ast_node *name)
{
    struct mylite_sql_source_span span = span_from_token(&release_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (name != NULL) {
        span = span_join(span, name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_RELEASE_SAVEPOINT_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, name);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_transaction_completion(
    struct mylite_sql_parser_state *state, struct mylite_sql_parser_completion_tokens tokens,
    enum mylite_sql_ast_transaction_chain chain, enum mylite_sql_ast_transaction_release release)
{
    struct mylite_sql_source_span span = {0};
    struct mylite_sql_ast_node *completion = NULL;

    if (tokens.start.text != NULL) {
        span = span_from_token(&tokens.start);
        if (tokens.end.text != NULL) {
            span = span_join(span, span_from_token(&tokens.end));
        }
    }

    completion = make_node(state, MYLITE_SQL_AST_TRANSACTION_COMPLETION, span);
    if (completion == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_transaction_completion(completion, chain, release);
    return completion;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_delete_target(struct mylite_sql_parser_state *state,
                                     struct mylite_sql_ast_node *table_name,
                                     struct mylite_sql_ast_node *alias)
{
    struct mylite_sql_source_span span =
        table_name == NULL ? (struct mylite_sql_source_span){0} : table_name->span;
    struct mylite_sql_ast_node *target = NULL;

    if (alias != NULL) {
        span = span_join(span, alias->span);
    }

    target = make_node(state, MYLITE_SQL_AST_DELETE_TARGET, span);
    if (target == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(target, table_name);
    mylite_sql_ast_node_append_child(target, alias);
    return target;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_delete_limit_clause(struct mylite_sql_parser_state *state,
                                           struct mylite_sql_token limit_token,
                                           struct mylite_sql_ast_node *row_count_bound)
{
    struct mylite_sql_source_span span = span_from_token(&limit_token);
    struct mylite_sql_ast_node *limit_clause = NULL;

    if (row_count_bound != NULL) {
        span = span_join(span, row_count_bound->span);
    }

    limit_clause = make_node(state, MYLITE_SQL_AST_DELETE_LIMIT_CLAUSE, span);
    if (limit_clause == NULL) {
        return NULL;
    }
    mylite_sql_ast_node_append_child(limit_clause, row_count_bound);
    return limit_clause;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_show_schemas_statement(struct mylite_sql_parser_state *state,
                                              struct mylite_sql_token show_token,
                                              struct mylite_sql_token schemas_token)
{
    return make_node(state, MYLITE_SQL_AST_SHOW_SCHEMAS_STATEMENT,
                     span_join(span_from_token(&show_token), span_from_token(&schemas_token)));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_set_names_statement(
    struct mylite_sql_parser_state *state, struct mylite_sql_token set_token,
    struct mylite_sql_ast_node *character_set, struct mylite_sql_ast_node *collation)
{
    struct mylite_sql_source_span span = span_from_token(&set_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (character_set != NULL) {
        span = span_join(span, character_set->span);
    }
    if (collation != NULL) {
        span = span_join(span, collation->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SET_NAMES_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, character_set);
    mylite_sql_ast_node_append_child(statement, collation);
    return statement;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_set_character_set_statement(struct mylite_sql_parser_state *state,
                                                   struct mylite_sql_token set_token,
                                                   struct mylite_sql_ast_node *character_set)
{
    struct mylite_sql_source_span span = span_from_token(&set_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (character_set != NULL) {
        span = span_join(span, character_set->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, character_set);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_create_table_statement(
    struct mylite_sql_parser_state *state, struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *if_not_exists, struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *columns, struct mylite_sql_ast_node *options)
{
    struct mylite_sql_source_span span = span_from_token(&create_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }
    if (columns != NULL) {
        span = span_join(span, columns->span);
    }
    if (options != NULL && options->span.text != NULL) {
        span = span_join(span, options->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, columns);
    mylite_sql_ast_node_append_child(statement, if_not_exists);
    mylite_sql_ast_node_append_child(statement, options);
    return statement;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_table_option_list(struct mylite_sql_parser_state *state)
{
    return make_node(state, MYLITE_SQL_AST_TABLE_OPTION_LIST, (struct mylite_sql_source_span){0});
}

struct mylite_sql_ast_node *
mylite_sql_parser_append_table_option(struct mylite_sql_parser_state *state,
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

struct mylite_sql_ast_node *mylite_sql_parser_make_table_option(
    struct mylite_sql_parser_state *state, struct mylite_sql_token option_token,
    enum mylite_sql_ast_table_option option_kind, struct mylite_sql_ast_node *value)
{
    struct mylite_sql_source_span span = span_from_token(&option_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }

    option = make_node(state, MYLITE_SQL_AST_TABLE_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_table_option(option, option_kind);
    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_comment_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_parser_table_string_option_tokens tokens)
{
    return mylite_sql_parser_make_table_option(
        state, tokens.option, MYLITE_SQL_AST_TABLE_OPTION_COMMENT,
        mylite_sql_parser_make_literal(state, tokens.string, MYLITE_SQL_AST_LITERAL_STRING));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_auto_increment_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_parser_table_integer_option_tokens tokens)
{
    return mylite_sql_parser_make_table_option(state, tokens.option,
                                               MYLITE_SQL_AST_TABLE_OPTION_AUTO_INCREMENT,
                                               make_checked_integer_literal(state, tokens.integer));
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_column_definition_list(struct mylite_sql_parser_state *state,
                                              struct mylite_sql_ast_node *column)
{
    struct mylite_sql_source_span span =
        column == NULL ? (struct mylite_sql_source_span){0} : column->span;
    struct mylite_sql_ast_node *list =
        make_node(state, MYLITE_SQL_AST_COLUMN_DEFINITION_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, column);
    return list;
}

struct mylite_sql_ast_node *
mylite_sql_parser_append_column_definition(struct mylite_sql_parser_state *state,
                                           struct mylite_sql_ast_node *list,
                                           struct mylite_sql_ast_node *column)
{
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, column);
    if (column != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, column->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_primary_key_constraint(
    struct mylite_sql_parser_state *state, struct mylite_sql_token start_token,
    struct mylite_sql_ast_node *constraint_name, struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *index_type, struct mylite_sql_ast_node *key_parts,
    struct mylite_sql_ast_node *options)
{
    struct mylite_sql_source_span span = span_from_token(&start_token);
    struct mylite_sql_ast_node *constraint = NULL;

    if (constraint_name != NULL) {
        span = span_join(span, constraint_name->span);
    }
    if (index_name != NULL) {
        span = span_join(span, index_name->span);
    }
    if (index_type != NULL) {
        span = span_join(span, index_type->span);
    }
    if (key_parts != NULL) {
        span = span_join(span, key_parts->span);
    }
    if (options != NULL && options->span.text != NULL) {
        span = span_join(span, options->span);
    }

    constraint = make_node(state, MYLITE_SQL_AST_PRIMARY_KEY_CONSTRAINT, span);
    if (constraint == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(constraint, constraint_name);
    mylite_sql_ast_node_append_child(constraint, index_name);
    mylite_sql_ast_node_append_child(constraint, index_type);
    mylite_sql_ast_node_append_child(constraint, key_parts);
    mylite_sql_ast_node_append_child(constraint, options);
    return constraint;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_secondary_index(
    struct mylite_sql_parser_state *state, struct mylite_sql_token start_token,
    struct mylite_sql_ast_node *index_name, struct mylite_sql_ast_node *index_type,
    struct mylite_sql_ast_node *key_parts, struct mylite_sql_ast_node *options)
{
    struct mylite_sql_source_span span = span_from_token(&start_token);
    struct mylite_sql_ast_node *index = NULL;

    if (index_name != NULL) {
        span = span_join(span, index_name->span);
    }
    if (index_type != NULL) {
        span = span_join(span, index_type->span);
    }
    if (key_parts != NULL) {
        span = span_join(span, key_parts->span);
    }
    if (options != NULL && options->span.text != NULL) {
        span = span_join(span, options->span);
    }

    index = make_node(state, MYLITE_SQL_AST_SECONDARY_INDEX, span);
    if (index == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(index, index_name);
    mylite_sql_ast_node_append_child(index, index_type);
    mylite_sql_ast_node_append_child(index, key_parts);
    mylite_sql_ast_node_append_child(index, options);
    return index;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_unique_index(
    struct mylite_sql_parser_state *state, struct mylite_sql_token start_token,
    struct mylite_sql_ast_node *constraint_name, struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *index_type, struct mylite_sql_ast_node *key_parts,
    struct mylite_sql_ast_node *options)
{
    struct mylite_sql_source_span span = span_from_token(&start_token);
    struct mylite_sql_ast_node *index = NULL;

    if (constraint_name != NULL) {
        span = span_join(span, constraint_name->span);
    }
    if (index_name != NULL) {
        span = span_join(span, index_name->span);
    }
    if (index_type != NULL) {
        span = span_join(span, index_type->span);
    }
    if (key_parts != NULL) {
        span = span_join(span, key_parts->span);
    }
    if (options != NULL && options->span.text != NULL) {
        span = span_join(span, options->span);
    }

    index = make_node(state, MYLITE_SQL_AST_UNIQUE_INDEX, span);
    if (index == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(index, constraint_name);
    mylite_sql_ast_node_append_child(index, index_name);
    mylite_sql_ast_node_append_child(index, index_type);
    mylite_sql_ast_node_append_child(index, key_parts);
    mylite_sql_ast_node_append_child(index, options);
    return index;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_key_part_list(struct mylite_sql_parser_state *state,
                                     struct mylite_sql_ast_node *key_part)
{
    struct mylite_sql_source_span span =
        key_part == NULL ? (struct mylite_sql_source_span){0} : key_part->span;
    struct mylite_sql_ast_node *list = make_node(state, MYLITE_SQL_AST_KEY_PART_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, key_part);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_key_part(struct mylite_sql_parser_state *state,
                                                              struct mylite_sql_ast_node *list,
                                                              struct mylite_sql_ast_node *key_part)
{
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, key_part);
    if (key_part != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, key_part->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_key_part(
    struct mylite_sql_parser_state *state, struct mylite_sql_ast_node *name,
    struct mylite_sql_ast_node *prefix, enum mylite_sql_ast_key_part_order order,
    struct mylite_sql_token order_token)
{
    struct mylite_sql_source_span span =
        name == NULL ? (struct mylite_sql_source_span){0} : name->span;
    struct mylite_sql_ast_node *key_part = NULL;

    if (prefix != NULL) {
        span = span_join(span, prefix->span);
    }
    if (order != MYLITE_SQL_AST_KEY_PART_ORDER_NONE) {
        span = span_join(span, span_from_token(&order_token));
    }

    key_part = make_node(state, MYLITE_SQL_AST_KEY_PART, span);
    if (key_part == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_key_part_order(key_part, order);
    mylite_sql_ast_node_append_child(key_part, name);
    mylite_sql_ast_node_append_child(key_part, prefix);
    return key_part;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_key_part_prefix(struct mylite_sql_parser_state *state,
                                       struct mylite_sql_parser_key_part_prefix_tokens tokens)
{
    struct mylite_sql_ast_node *prefix = make_checked_integer_literal(state, tokens.integer);
    if (prefix == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_span(prefix, span_join(span_from_token(&tokens.left_paren),
                                                   span_from_token(&tokens.right_paren)));
    return prefix;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_index_type(
    struct mylite_sql_parser_state *state, struct mylite_sql_token using_token,
    struct mylite_sql_token algorithm_token, enum mylite_sql_ast_index_algorithm algorithm)
{
    struct mylite_sql_ast_node *index_type =
        make_node(state, MYLITE_SQL_AST_INDEX_TYPE,
                  span_join(span_from_token(&using_token), span_from_token(&algorithm_token)));
    if (index_type == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_index_algorithm(index_type, algorithm);
    return index_type;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_index_option_list(struct mylite_sql_parser_state *state)
{
    return make_node(state, MYLITE_SQL_AST_INDEX_OPTION_LIST, (struct mylite_sql_source_span){0});
}

struct mylite_sql_ast_node *
mylite_sql_parser_append_index_option(struct mylite_sql_parser_state *state,
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

struct mylite_sql_ast_node *
mylite_sql_parser_make_index_using_option(struct mylite_sql_parser_state *state,
                                          struct mylite_sql_ast_node *index_type)
{
    struct mylite_sql_source_span span =
        index_type == NULL ? (struct mylite_sql_source_span){0} : index_type->span;
    struct mylite_sql_ast_node *option = make_node(state, MYLITE_SQL_AST_INDEX_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_index_option(option, MYLITE_SQL_AST_INDEX_OPTION_USING);
    if (index_type != NULL) {
        mylite_sql_ast_node_set_index_algorithm(option, index_type->index_algorithm);
    }
    mylite_sql_ast_node_append_child(option, index_type);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_index_key_block_size_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_parser_index_key_block_size_tokens tokens)
{
    struct mylite_sql_ast_node *value = make_checked_integer_literal(state, tokens.integer);
    struct mylite_sql_source_span span = span_from_token(&tokens.key_block_size);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }

    option = make_node(state, MYLITE_SQL_AST_INDEX_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_index_option(option, MYLITE_SQL_AST_INDEX_OPTION_KEY_BLOCK_SIZE);
    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_index_comment_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_parser_index_string_option_tokens tokens)
{
    struct mylite_sql_ast_node *value =
        mylite_sql_parser_make_literal(state, tokens.string, MYLITE_SQL_AST_LITERAL_STRING);
    struct mylite_sql_source_span span = span_from_token(&tokens.option);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }

    option = make_node(state, MYLITE_SQL_AST_INDEX_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_index_option(option, MYLITE_SQL_AST_INDEX_OPTION_COMMENT);
    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_index_visibility_option(struct mylite_sql_parser_state *state,
                                               struct mylite_sql_token visibility_token,
                                               enum mylite_sql_ast_index_option visibility)
{
    struct mylite_sql_ast_node *option =
        make_node(state, MYLITE_SQL_AST_INDEX_OPTION, span_from_token(&visibility_token));
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_index_option(option, visibility);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_index_attribute_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_parser_index_string_option_tokens tokens,
    enum mylite_sql_ast_index_option option_kind)
{
    struct mylite_sql_ast_node *value =
        mylite_sql_parser_make_literal(state, tokens.string, MYLITE_SQL_AST_LITERAL_STRING);
    struct mylite_sql_source_span span = span_from_token(&tokens.option);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }

    option = make_node(state, MYLITE_SQL_AST_INDEX_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_index_option(option, option_kind);
    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_definition(
    struct mylite_sql_parser_state *state, struct mylite_sql_ast_node *name,
    struct mylite_sql_ast_node *column_type, struct mylite_sql_ast_node *attributes)
{
    struct mylite_sql_source_span span =
        name == NULL ? (struct mylite_sql_source_span){0} : name->span;
    struct mylite_sql_ast_node *column = NULL;

    if (column_type != NULL) {
        span = span_join(span, column_type->span);
    }
    if (attributes != NULL && attributes->span.text != NULL) {
        span = span_join(span, attributes->span);
    }

    column = make_node(state, MYLITE_SQL_AST_COLUMN_DEFINITION, span);
    if (column == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(column, name);
    mylite_sql_ast_node_append_child(column, column_type);
    mylite_sql_ast_node_append_child(column, attributes);
    return column;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_column_type(struct mylite_sql_parser_state *state,
                                   struct mylite_sql_token type_token,
                                   enum mylite_sql_ast_column_type column_type)
{
    struct mylite_sql_ast_node *node =
        make_node(state, MYLITE_SQL_AST_COLUMN_TYPE, span_from_token(&type_token));
    if (node == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_column_type(node, column_type);
    return node;
}

struct mylite_sql_ast_node *
mylite_sql_parser_set_column_display_width(struct mylite_sql_parser_state *state,
                                           struct mylite_sql_ast_node *column_type,
                                           struct mylite_sql_ast_node *display_width)
{
    if (!is_parse_ok(state) || column_type == NULL || display_width == NULL) {
        return column_type;
    }

    mylite_sql_ast_node_set_column_display_width(column_type, parse_display_width(display_width));
    mylite_sql_ast_node_set_span(column_type, span_join(column_type->span, display_width->span));
    return column_type;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_integer_display_width(struct mylite_sql_parser_state *state,
                                             struct mylite_sql_parser_display_width_tokens tokens)
{
    enum { display_width_max = 255U };
    struct mylite_sql_ast_node *display_width =
        mylite_sql_parser_make_literal(state, tokens.integer, MYLITE_SQL_AST_LITERAL_INTEGER);
    if (display_width == NULL) {
        return NULL;
    }
    if (parse_display_width(display_width) > display_width_max) {
        mylite_sql_parser_state_syntax_error(state, MYLITE_SQL_PARSE_INTEGER, tokens.integer);
        return NULL;
    }

    mylite_sql_ast_node_set_span(display_width, span_join(span_from_token(&tokens.left_paren),
                                                          span_from_token(&tokens.right_paren)));
    return display_width;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_column_length(struct mylite_sql_parser_state *state,
                                     struct mylite_sql_parser_column_length_tokens tokens)
{
    struct mylite_sql_ast_node *length =
        mylite_sql_parser_make_literal(state, tokens.integer, MYLITE_SQL_AST_LITERAL_INTEGER);
    if (length == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_span(length, span_join(span_from_token(&tokens.left_paren),
                                                   span_from_token(&tokens.right_paren)));
    {
        uint64_t column_length = 0ULL;
        if (!parse_column_length(length, &column_length)) {
            mylite_sql_parser_state_syntax_error(state, MYLITE_SQL_PARSE_INTEGER, tokens.integer);
            return NULL;
        }
        mylite_sql_ast_node_set_column_length(length, column_length);
    }
    return length;
}

struct mylite_sql_ast_node *
mylite_sql_parser_set_column_length(struct mylite_sql_parser_state *state,
                                    struct mylite_sql_ast_node *column_type,
                                    struct mylite_sql_ast_node *length)
{
    if (!is_parse_ok(state) || column_type == NULL || length == NULL) {
        return column_type;
    }

    mylite_sql_ast_node_set_column_length(column_type, length->column_length);
    mylite_sql_ast_node_set_span(column_type, span_join(column_type->span, length->span));
    return column_type;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_column_precision(struct mylite_sql_parser_state *state,
                                        struct mylite_sql_parser_precision_tokens tokens)
{
    struct mylite_sql_ast_node *precision =
        mylite_sql_parser_make_literal(state, tokens.precision, MYLITE_SQL_AST_LITERAL_INTEGER);
    if (precision == NULL) {
        return NULL;
    }

    {
        uint64_t value = 0ULL;
        if (!parse_column_length(precision, &value)) {
            mylite_sql_parser_state_syntax_error(state, MYLITE_SQL_PARSE_INTEGER, tokens.precision);
            return NULL;
        }
        mylite_sql_ast_node_set_column_precision(precision, value);
    }
    mylite_sql_ast_node_set_span(precision, span_join(span_from_token(&tokens.left_paren),
                                                      span_from_token(&tokens.right_paren)));
    return precision;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_precision_scale(
    struct mylite_sql_parser_state *state, struct mylite_sql_parser_precision_scale_tokens tokens)
{
    struct mylite_sql_ast_node *precision_scale =
        mylite_sql_parser_make_column_precision(state, (struct mylite_sql_parser_precision_tokens){
                                                           .left_paren = tokens.left_paren,
                                                           .precision = tokens.precision,
                                                           .right_paren = tokens.right_paren,
                                                       });
    if (precision_scale == NULL) {
        return NULL;
    }

    {
        struct mylite_sql_ast_node *scale =
            mylite_sql_parser_make_literal(state, tokens.scale, MYLITE_SQL_AST_LITERAL_INTEGER);
        uint64_t value = 0ULL;
        if (scale == NULL || !parse_column_length(scale, &value)) {
            mylite_sql_parser_state_syntax_error(state, MYLITE_SQL_PARSE_INTEGER, tokens.scale);
            return NULL;
        }
        mylite_sql_ast_node_set_column_scale(precision_scale, value);
    }
    return precision_scale;
}

struct mylite_sql_ast_node *
mylite_sql_parser_set_column_precision_scale(struct mylite_sql_parser_state *state,
                                             struct mylite_sql_ast_node *column_type,
                                             struct mylite_sql_ast_node *precision_scale)
{
    if (!is_parse_ok(state) || column_type == NULL || precision_scale == NULL) {
        return column_type;
    }

    mylite_sql_ast_node_set_column_precision(column_type, precision_scale->column_precision);
    if (precision_scale->has_column_scale) {
        mylite_sql_ast_node_set_column_scale(column_type, precision_scale->column_scale);
    }
    mylite_sql_ast_node_set_span(column_type, span_join(column_type->span, precision_scale->span));
    return column_type;
}

struct mylite_sql_ast_node *
mylite_sql_parser_set_column_type_signed(struct mylite_sql_parser_state *state,
                                         struct mylite_sql_ast_node *column_type,
                                         struct mylite_sql_token signed_token)
{
    if (!is_parse_ok(state) || column_type == NULL) {
        return column_type;
    }

    mylite_sql_ast_node_set_column_type_signed(column_type);
    mylite_sql_ast_node_set_span(column_type,
                                 span_join(column_type->span, span_from_token(&signed_token)));
    return column_type;
}

struct mylite_sql_ast_node *
mylite_sql_parser_set_column_type_unsigned(struct mylite_sql_parser_state *state,
                                           struct mylite_sql_ast_node *column_type,
                                           struct mylite_sql_token unsigned_token)
{
    if (!is_parse_ok(state) || column_type == NULL) {
        return column_type;
    }

    mylite_sql_ast_node_set_column_type_unsigned(column_type);
    mylite_sql_ast_node_set_span(column_type,
                                 span_join(column_type->span, span_from_token(&unsigned_token)));
    return column_type;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_column_type_attribute_list(struct mylite_sql_parser_state *state)
{
    return make_node(state, MYLITE_SQL_AST_COLUMN_TYPE_ATTRIBUTE_LIST,
                     (struct mylite_sql_source_span){0});
}

struct mylite_sql_ast_node *
mylite_sql_parser_set_column_type_character_set(struct mylite_sql_parser_state *state,
                                                struct mylite_sql_ast_node *attributes,
                                                struct mylite_sql_ast_node *character_set)
{
    if (!is_parse_ok(state) || attributes == NULL || character_set == NULL) {
        return attributes;
    }

    mylite_sql_ast_node_set_column_character_set(attributes, character_set->span);
    mylite_sql_ast_node_set_span(attributes, span_join(attributes->span, character_set->span));
    return attributes;
}

struct mylite_sql_ast_node *
mylite_sql_parser_set_column_type_collation(struct mylite_sql_parser_state *state,
                                            struct mylite_sql_ast_node *attributes,
                                            struct mylite_sql_ast_node *collation)
{
    if (!is_parse_ok(state) || attributes == NULL || collation == NULL) {
        return attributes;
    }

    mylite_sql_ast_node_set_column_collation(attributes, collation->span);
    mylite_sql_ast_node_set_span(attributes, span_join(attributes->span, collation->span));
    return attributes;
}

struct mylite_sql_ast_node *
mylite_sql_parser_set_column_type_binary_attribute(struct mylite_sql_parser_state *state,
                                                   struct mylite_sql_ast_node *attributes,
                                                   struct mylite_sql_token binary_token)
{
    if (!is_parse_ok(state) || attributes == NULL) {
        return attributes;
    }

    mylite_sql_ast_node_set_column_binary_attribute(attributes);
    mylite_sql_ast_node_set_span(attributes,
                                 span_join(attributes->span, span_from_token(&binary_token)));
    return attributes;
}

struct mylite_sql_ast_node *
mylite_sql_parser_set_column_type_byte_attribute(struct mylite_sql_parser_state *state,
                                                 struct mylite_sql_ast_node *attributes,
                                                 struct mylite_sql_token byte_token)
{
    if (!is_parse_ok(state) || attributes == NULL) {
        return attributes;
    }

    mylite_sql_ast_node_set_column_byte_attribute(attributes);
    mylite_sql_ast_node_set_span(attributes,
                                 span_join(attributes->span, span_from_token(&byte_token)));
    return attributes;
}

struct mylite_sql_ast_node *
mylite_sql_parser_set_column_type_zerofill_attribute(struct mylite_sql_parser_state *state,
                                                     struct mylite_sql_ast_node *attributes,
                                                     struct mylite_sql_token zerofill_token)
{
    if (!is_parse_ok(state) || attributes == NULL) {
        return attributes;
    }

    mylite_sql_ast_node_set_column_zerofill_attribute(attributes);
    mylite_sql_ast_node_set_span(attributes,
                                 span_join(attributes->span, span_from_token(&zerofill_token)));
    return attributes;
}

struct mylite_sql_ast_node *
mylite_sql_parser_apply_column_type_attributes(struct mylite_sql_parser_state *state,
                                               struct mylite_sql_ast_node *column_type,
                                               struct mylite_sql_ast_node *attributes)
{
    if (!is_parse_ok(state) || column_type == NULL || attributes == NULL) {
        return column_type;
    }

    if (attributes->column_type_signed) {
        mylite_sql_ast_node_set_column_type_signed(column_type);
    }
    if (attributes->column_type_unsigned) {
        mylite_sql_ast_node_set_column_type_unsigned(column_type);
    }
    if (attributes->has_column_character_set) {
        mylite_sql_ast_node_set_column_character_set(column_type, attributes->column_character_set);
    }
    if (attributes->has_column_collation) {
        mylite_sql_ast_node_set_column_collation(column_type, attributes->column_collation);
    }
    if (attributes->column_binary_attribute) {
        mylite_sql_ast_node_set_column_binary_attribute(column_type);
    }
    if (attributes->column_byte_attribute) {
        mylite_sql_ast_node_set_column_byte_attribute(column_type);
    }
    if (attributes->column_zerofill_attribute) {
        mylite_sql_ast_node_set_column_zerofill_attribute(column_type);
    }
    if (attributes->span.text != NULL) {
        mylite_sql_ast_node_set_span(column_type, span_join(column_type->span, attributes->span));
    }
    return mylite_sql_parser_validate_column_type(state, column_type);
}

struct mylite_sql_ast_node *
mylite_sql_parser_set_column_type_national(struct mylite_sql_parser_state *state,
                                           struct mylite_sql_ast_node *column_type,
                                           struct mylite_sql_token national_token)
{
    if (!is_parse_ok(state) || column_type == NULL) {
        return column_type;
    }

    mylite_sql_ast_node_set_column_national_attribute(column_type);
    mylite_sql_ast_node_set_span(column_type,
                                 span_join(span_from_token(&national_token), column_type->span));
    return mylite_sql_parser_validate_column_type(state, column_type);
}

struct mylite_sql_ast_node *
mylite_sql_parser_validate_column_type(struct mylite_sql_parser_state *state,
                                       struct mylite_sql_ast_node *column_type)
{
    const char *type_name = NULL;
    struct mylite_column_type_descriptor descriptor;
    struct mylite_column_type_attributes attributes = {false};
    enum mylite_column_type_status status = MYLITE_COLUMN_TYPE_OK;

    if (!is_parse_ok(state) || column_type == NULL ||
        (!column_type_uses_string_binary_descriptor(column_type->column_type) &&
         !column_type_uses_numeric_descriptor(column_type->column_type) &&
         !column_type_uses_temporal_descriptor(column_type->column_type))) {
        return column_type;
    }

    type_name = column_type_descriptor_name(column_type->column_type);
    attributes = (struct mylite_column_type_attributes){
        .has_signed = column_type->column_type_signed,
        .has_unsigned = column_type->column_type_unsigned,
        .has_length = column_type->has_column_length,
        .length = column_type->column_length,
        .has_precision = column_type->has_column_precision,
        .precision = column_type->column_precision,
        .has_scale = column_type->has_column_scale,
        .scale = column_type->column_scale,
        .has_character_set = column_type->has_column_character_set,
        .character_set = column_type->column_character_set.text,
        .character_set_length = column_type->column_character_set.length,
        .has_collation = column_type->has_column_collation,
        .collation = column_type->column_collation.text,
        .collation_length = column_type->column_collation.length,
        .has_binary_attribute = column_type->column_binary_attribute,
        .has_byte_attribute = column_type->column_byte_attribute,
        .has_zerofill_attribute = column_type->column_zerofill_attribute,
        .is_national = column_type->column_national_attribute,
    };

    if (column_type_uses_temporal_descriptor(column_type->column_type)) {
        status = mylite_column_type_describe_temporal(type_name, strlen(type_name), attributes,
                                                      &descriptor);
    } else if (column_type_uses_numeric_descriptor(column_type->column_type)) {
        status = mylite_column_type_describe_numeric(type_name, strlen(type_name), attributes,
                                                     &descriptor);
    } else {
        status = mylite_column_type_describe_string_binary(type_name, strlen(type_name), attributes,
                                                           &descriptor);
    }
    if (status != MYLITE_COLUMN_TYPE_OK) {
        mylite_sql_parser_state_parse_failed(state);
    }
    return column_type;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_column_attribute_list(struct mylite_sql_parser_state *state)
{
    return make_node(state, MYLITE_SQL_AST_COLUMN_ATTRIBUTE_LIST,
                     (struct mylite_sql_source_span){0});
}

struct mylite_sql_ast_node *
mylite_sql_parser_append_column_attribute(struct mylite_sql_parser_state *state,
                                          struct mylite_sql_ast_node *list,
                                          struct mylite_sql_ast_node *attribute)
{
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, attribute);
    if (attribute != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, attribute->span));
    }
    return list;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_column_null_attribute(struct mylite_sql_parser_state *state,
                                             struct mylite_sql_token null_token)
{
    struct mylite_sql_ast_node *attribute =
        make_node(state, MYLITE_SQL_AST_COLUMN_ATTRIBUTE, span_from_token(&null_token));
    if (attribute == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_column_attribute(attribute, MYLITE_SQL_AST_COLUMN_ATTRIBUTE_NULL);
    return attribute;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_column_not_null_attribute(struct mylite_sql_parser_state *state,
                                                 struct mylite_sql_token not_token,
                                                 struct mylite_sql_token null_token)
{
    struct mylite_sql_ast_node *attribute =
        make_node(state, MYLITE_SQL_AST_COLUMN_ATTRIBUTE,
                  span_join(span_from_token(&not_token), span_from_token(&null_token)));
    if (attribute == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_column_attribute(attribute, MYLITE_SQL_AST_COLUMN_ATTRIBUTE_NOT_NULL);
    return attribute;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_column_default_attribute(struct mylite_sql_parser_state *state,
                                                struct mylite_sql_token default_token,
                                                struct mylite_sql_ast_node *value)
{
    struct mylite_sql_source_span span = span_from_token(&default_token);
    struct mylite_sql_ast_node *attribute = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }

    attribute = make_node(state, MYLITE_SQL_AST_COLUMN_ATTRIBUTE, span);
    if (attribute == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_column_attribute(attribute, MYLITE_SQL_AST_COLUMN_ATTRIBUTE_DEFAULT);
    mylite_sql_ast_node_append_child(attribute, value);
    return attribute;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_on_update_attribute(
    struct mylite_sql_parser_state *state, struct mylite_sql_token on_token,
    struct mylite_sql_token update_token, struct mylite_sql_ast_node *value)
{
    struct mylite_sql_source_span span =
        span_join(span_from_token(&on_token), span_from_token(&update_token));
    struct mylite_sql_ast_node *attribute = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }

    attribute = make_node(state, MYLITE_SQL_AST_COLUMN_ATTRIBUTE, span);
    if (attribute == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_column_attribute(attribute, MYLITE_SQL_AST_COLUMN_ATTRIBUTE_ON_UPDATE);
    mylite_sql_ast_node_append_child(attribute, value);
    return attribute;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_column_comment_attribute(struct mylite_sql_parser_state *state,
                                                struct mylite_sql_token comment_token,
                                                struct mylite_sql_ast_node *value)
{
    struct mylite_sql_source_span span = span_from_token(&comment_token);
    struct mylite_sql_ast_node *attribute = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }

    attribute = make_node(state, MYLITE_SQL_AST_COLUMN_ATTRIBUTE, span);
    if (attribute == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_column_attribute(attribute, MYLITE_SQL_AST_COLUMN_ATTRIBUTE_COMMENT);
    mylite_sql_ast_node_append_child(attribute, value);
    return attribute;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_column_visibility_attribute(struct mylite_sql_parser_state *state,
                                                   struct mylite_sql_token visibility_token,
                                                   enum mylite_sql_ast_column_attribute visibility)
{
    struct mylite_sql_ast_node *attribute =
        make_node(state, MYLITE_SQL_AST_COLUMN_ATTRIBUTE, span_from_token(&visibility_token));
    if (attribute == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_column_attribute(attribute, visibility);
    return attribute;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_format_attribute(
    struct mylite_sql_parser_state *state, struct mylite_sql_token column_format_token,
    struct mylite_sql_token value_token, enum mylite_sql_ast_column_format format)
{
    struct mylite_sql_ast_node *attribute =
        make_node(state, MYLITE_SQL_AST_COLUMN_ATTRIBUTE,
                  span_join(span_from_token(&column_format_token), span_from_token(&value_token)));
    if (attribute == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_column_attribute(attribute,
                                             MYLITE_SQL_AST_COLUMN_ATTRIBUTE_COLUMN_FORMAT);
    mylite_sql_ast_node_set_column_format(attribute, format);
    return attribute;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_storage_attribute(
    struct mylite_sql_parser_state *state, struct mylite_sql_token storage_token,
    struct mylite_sql_token value_token, enum mylite_sql_ast_column_storage storage)
{
    struct mylite_sql_ast_node *attribute =
        make_node(state, MYLITE_SQL_AST_COLUMN_ATTRIBUTE,
                  span_join(span_from_token(&storage_token), span_from_token(&value_token)));
    if (attribute == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_column_attribute(attribute, MYLITE_SQL_AST_COLUMN_ATTRIBUTE_STORAGE);
    mylite_sql_ast_node_set_column_storage(attribute, storage);
    return attribute;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_column_auto_increment_attribute(struct mylite_sql_parser_state *state,
                                                       struct mylite_sql_token auto_increment_token)
{
    struct mylite_sql_ast_node *attribute =
        make_node(state, MYLITE_SQL_AST_COLUMN_ATTRIBUTE, span_from_token(&auto_increment_token));
    if (attribute == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_column_attribute(attribute,
                                             MYLITE_SQL_AST_COLUMN_ATTRIBUTE_AUTO_INCREMENT);
    return attribute;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_column_primary_key_attribute(struct mylite_sql_parser_state *state,
                                                    struct mylite_sql_token start_token,
                                                    struct mylite_sql_token key_token)
{
    struct mylite_sql_source_span span = span_from_token(&start_token);
    struct mylite_sql_ast_node *attribute = NULL;

    if (key_token.text != start_token.text || key_token.length != start_token.length ||
        key_token.offset != start_token.offset) {
        span = span_join(span, span_from_token(&key_token));
    }

    attribute = make_node(state, MYLITE_SQL_AST_COLUMN_ATTRIBUTE, span);
    if (attribute == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_column_attribute(attribute,
                                             MYLITE_SQL_AST_COLUMN_ATTRIBUTE_PRIMARY_KEY);
    return attribute;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_unique_key_attribute(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_parser_column_unique_key_attribute_tokens tokens)
{
    struct mylite_sql_source_span span = span_from_token(&tokens.unique_token);
    struct mylite_sql_ast_node *attribute = NULL;

    if (tokens.key_token.text != NULL) {
        span = span_join(span, span_from_token(&tokens.key_token));
    }

    attribute = make_node(state, MYLITE_SQL_AST_COLUMN_ATTRIBUTE, span);
    if (attribute == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_column_attribute(attribute, MYLITE_SQL_AST_COLUMN_ATTRIBUTE_UNIQUE_KEY);
    return attribute;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_current_timestamp(struct mylite_sql_parser_state *state,
                                         struct mylite_sql_token current_timestamp_token,
                                         struct mylite_sql_ast_node *precision)
{
    enum { current_timestamp_fsp_max = 6U };
    struct mylite_sql_source_span span = span_from_token(&current_timestamp_token);
    struct mylite_sql_ast_node *current_timestamp = NULL;

    if (precision != NULL) {
        if (precision->column_precision > current_timestamp_fsp_max) {
            mylite_sql_parser_state_parse_failed(state);
            return NULL;
        }
        span = span_join(span, precision->span);
    }

    current_timestamp = make_node(state, MYLITE_SQL_AST_CURRENT_TIMESTAMP, span);
    if (current_timestamp == NULL) {
        return NULL;
    }

    if (precision != NULL) {
        mylite_sql_ast_node_set_column_precision(current_timestamp, precision->column_precision);
    }
    return current_timestamp;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_current_timestamp_empty_parens(
    struct mylite_sql_parser_state *state, struct mylite_sql_token current_timestamp_token,
    struct mylite_sql_token left_paren, struct mylite_sql_token right_paren)
{
    struct mylite_sql_ast_node *current_timestamp =
        mylite_sql_parser_make_current_timestamp(state, current_timestamp_token, NULL);
    if (current_timestamp == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_span(
        current_timestamp,
        span_join(span_from_token(&current_timestamp_token),
                  span_join(span_from_token(&left_paren), span_from_token(&right_paren))));
    return current_timestamp;
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

struct mylite_sql_ast_node *
mylite_sql_parser_make_aliased_select_item(struct mylite_sql_parser_state *state,
                                           struct mylite_sql_ast_node *expression,
                                           struct mylite_sql_ast_node *alias)
{
    struct mylite_sql_source_span span =
        expression == NULL ? (struct mylite_sql_source_span){0} : expression->span;
    struct mylite_sql_ast_node *item = NULL;

    if (alias != NULL) {
        span = span_join(span, alias->span);
    }

    item = make_node(state, MYLITE_SQL_AST_SELECT_ITEM, span);
    if (item == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(item, expression);
    mylite_sql_ast_node_append_child(item, alias);
    return item;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_from_dual(struct mylite_sql_parser_state *state,
                                                             struct mylite_sql_token from_token,
                                                             struct mylite_sql_token dual_token)
{
    return make_node(state, MYLITE_SQL_AST_FROM_DUAL,
                     span_join(span_from_token(&from_token), span_from_token(&dual_token)));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_from_table(
    struct mylite_sql_parser_state *state, struct mylite_sql_token from_token,
    struct mylite_sql_ast_node *table_name, struct mylite_sql_ast_node *alias)
{
    struct mylite_sql_source_span span = span_from_token(&from_token);
    struct mylite_sql_ast_node *from_table = NULL;

    if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }
    if (alias != NULL) {
        span = span_join(span, alias->span);
    }

    from_table = make_node(state, MYLITE_SQL_AST_FROM_TABLE, span);
    if (from_table == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(from_table, table_name);
    mylite_sql_ast_node_append_child(from_table, alias);
    return from_table;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_identifier(struct mylite_sql_parser_state *state,
                                                              struct mylite_sql_token token)
{
    return make_node(state, MYLITE_SQL_AST_IDENTIFIER, span_from_token(&token));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_default(struct mylite_sql_parser_state *state,
                                                           struct mylite_sql_token token)
{
    return make_node(state, MYLITE_SQL_AST_DEFAULT, span_from_token(&token));
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

struct mylite_sql_ast_node *mylite_sql_parser_make_qualified_wildcard(
    struct mylite_sql_parser_state *state, struct mylite_sql_ast_node *first,
    struct mylite_sql_ast_node *second, struct mylite_sql_token wildcard_token)
{
    struct mylite_sql_source_span span =
        first == NULL ? span_from_token(&wildcard_token) : first->span;
    struct mylite_sql_ast_node *wildcard = NULL;

    if (second != NULL) {
        span = span_join(span, second->span);
    }
    span = span_join(span, span_from_token(&wildcard_token));

    wildcard = make_node(state, MYLITE_SQL_AST_WILDCARD, span);
    if (wildcard == NULL) {
        return NULL;
    }

    if (second == NULL && first != NULL && first->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER &&
        first->first_child != NULL && first->first_child->next_sibling != NULL &&
        first->first_child->kind == MYLITE_SQL_AST_IDENTIFIER &&
        first->first_child->next_sibling->kind == MYLITE_SQL_AST_IDENTIFIER) {
        struct mylite_sql_ast_node *left = first->first_child;
        struct mylite_sql_ast_node *right = first->first_child->next_sibling;

        mylite_sql_ast_node_append_child(wildcard, left);
        mylite_sql_ast_node_append_child(wildcard, right);
    } else {
        mylite_sql_ast_node_append_child(wildcard, first);
        mylite_sql_ast_node_append_child(wildcard, second);
    }
    return wildcard;
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

struct mylite_sql_ast_node *
mylite_sql_parser_make_bare_function_call(struct mylite_sql_parser_state *state,
                                          struct mylite_sql_token name_token)
{
    struct mylite_sql_source_span span = span_from_token(&name_token);
    struct mylite_sql_ast_node *name = mylite_sql_parser_make_identifier(state, name_token);
    struct mylite_sql_ast_node *arguments =
        make_node(state, MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST, span);
    struct mylite_sql_ast_node *call = NULL;

    if (name == NULL || arguments == NULL) {
        return NULL;
    }

    call = make_node(state, MYLITE_SQL_AST_FUNCTION_CALL, span);
    if (call == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(call, name);
    mylite_sql_ast_node_append_child(call, arguments);
    return call;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_function_call(
    struct mylite_sql_parser_state *state, struct mylite_sql_ast_node *name,
    struct mylite_sql_token left_paren, struct mylite_sql_ast_node *arguments,
    struct mylite_sql_token right_paren)
{
    struct mylite_sql_source_span span = name == NULL ? span_from_token(&left_paren) : name->span;
    struct mylite_sql_ast_node *call = NULL;

    span = span_join(span, span_from_token(&right_paren));

    call = make_node(state, MYLITE_SQL_AST_FUNCTION_CALL, span);
    if (call == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(call, name);
    mylite_sql_ast_node_append_child(call, arguments);
    return call;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_simple_case_expression(
    struct mylite_sql_parser_state *state, struct mylite_sql_token case_token,
    struct mylite_sql_ast_node *base, struct mylite_sql_ast_node *when_list,
    struct mylite_sql_ast_node *else_expression, struct mylite_sql_token end_token)
{
    struct mylite_sql_ast_node *expression =
        make_node(state, MYLITE_SQL_AST_CASE_EXPRESSION,
                  span_join(span_from_token(&case_token), span_from_token(&end_token)));
    if (expression == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_case_expression_simple(expression);
    mylite_sql_ast_node_append_child(expression, base);
    mylite_sql_ast_node_append_child(expression, when_list);
    mylite_sql_ast_node_append_child(expression, else_expression);
    return expression;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_searched_case_expression(
    struct mylite_sql_parser_state *state, struct mylite_sql_token case_token,
    struct mylite_sql_ast_node *when_list, struct mylite_sql_ast_node *else_expression,
    struct mylite_sql_token end_token)
{
    struct mylite_sql_ast_node *expression =
        make_node(state, MYLITE_SQL_AST_CASE_EXPRESSION,
                  span_join(span_from_token(&case_token), span_from_token(&end_token)));
    if (expression == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(expression, when_list);
    mylite_sql_ast_node_append_child(expression, else_expression);
    return expression;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_case_when_list(struct mylite_sql_parser_state *state,
                                      struct mylite_sql_ast_node *case_when)
{
    struct mylite_sql_ast_node *list =
        make_node(state, MYLITE_SQL_AST_CASE_WHEN_LIST,
                  case_when == NULL ? (struct mylite_sql_source_span){0} : case_when->span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, case_when);
    return list;
}

struct mylite_sql_ast_node *
mylite_sql_parser_append_case_when(struct mylite_sql_parser_state *state,
                                   struct mylite_sql_ast_node *list,
                                   struct mylite_sql_ast_node *case_when)
{
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, case_when);
    if (case_when != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, case_when->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_case_when(struct mylite_sql_parser_state *state,
                                                             struct mylite_sql_token when_token,
                                                             struct mylite_sql_ast_node *condition,
                                                             struct mylite_sql_ast_node *result)
{
    struct mylite_sql_source_span span = span_from_token(&when_token);
    struct mylite_sql_ast_node *case_when = NULL;

    if (result != NULL) {
        span = span_join(span, result->span);
    } else if (condition != NULL) {
        span = span_join(span, condition->span);
    }

    case_when = make_node(state, MYLITE_SQL_AST_CASE_WHEN, span);
    if (case_when == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(case_when, condition);
    mylite_sql_ast_node_append_child(case_when, result);
    return case_when;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_empty_function_argument_list(struct mylite_sql_parser_state *state,
                                                    struct mylite_sql_token left_paren,
                                                    struct mylite_sql_token right_paren)
{
    return make_node(state, MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST,
                     span_join(span_from_token(&left_paren), span_from_token(&right_paren)));
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_function_argument_list(struct mylite_sql_parser_state *state,
                                              struct mylite_sql_ast_node *argument)
{
    struct mylite_sql_source_span span =
        argument == NULL ? (struct mylite_sql_source_span){0} : argument->span;
    struct mylite_sql_ast_node *list =
        make_node(state, MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, argument);
    return list;
}

struct mylite_sql_ast_node *
mylite_sql_parser_append_function_argument(struct mylite_sql_parser_state *state,
                                           struct mylite_sql_ast_node *list,
                                           struct mylite_sql_ast_node *argument)
{
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, argument);
    if (argument != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, argument->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_parenthesized_column_default_expression(
    struct mylite_sql_parser_state *state, struct mylite_sql_token left_paren,
    struct mylite_sql_ast_node *expression, struct mylite_sql_token right_paren)
{
    if (expression_contains_function_call(expression)) {
        mylite_sql_parser_state_parse_failed(state);
        return NULL;
    }
    return mylite_sql_parser_make_parenthesized_expression(state, left_paren, expression,
                                                           right_paren);
}

// NOLINTNEXTLINE(misc-no-recursion)
static bool expression_contains_function_call(const struct mylite_sql_ast_node *expression)
{
    if (expression == NULL) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_FUNCTION_CALL) {
        return true;
    }
    for (const struct mylite_sql_ast_node *child = expression->first_child; child != NULL;
         child = child->next_sibling) {
        if (expression_contains_function_call(child)) {
            return true;
        }
    }
    return false;
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

struct mylite_sql_ast_node *mylite_sql_parser_make_ternary_expression(
    struct mylite_sql_parser_state *state, struct mylite_sql_ast_node *first,
    struct mylite_sql_token operator_token, enum mylite_sql_ast_operator operator_kind,
    struct mylite_sql_ast_node *second, struct mylite_sql_ast_node *third)
{
    struct mylite_sql_source_span span =
        first == NULL ? span_from_token(&operator_token) : first->span;
    struct mylite_sql_ast_node *expression = NULL;

    if (third != NULL) {
        span = span_join(span, third->span);
    } else if (second != NULL) {
        span = span_join(span, second->span);
    }

    expression = make_node(state, MYLITE_SQL_AST_TERNARY_EXPRESSION, span);
    if (expression == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_operator(expression, operator_kind);
    mylite_sql_ast_node_append_child(expression, first);
    mylite_sql_ast_node_append_child(expression, second);
    mylite_sql_ast_node_append_child(expression, third);
    return expression;
}

struct mylite_sql_ast_node *
mylite_sql_parser_make_expression_list(struct mylite_sql_parser_state *state,
                                       struct mylite_sql_ast_node *expression)
{
    struct mylite_sql_ast_node *list =
        make_node(state, MYLITE_SQL_AST_EXPRESSION_LIST,
                  expression == NULL ? (struct mylite_sql_source_span){0} : expression->span);

    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, expression);
    return list;
}

struct mylite_sql_ast_node *
mylite_sql_parser_append_expression(struct mylite_sql_parser_state *state,
                                    struct mylite_sql_ast_node *list,
                                    struct mylite_sql_ast_node *expression)
{
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, expression);
    if (expression != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, expression->span));
    }
    return list;
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

static unsigned int parse_display_width(const struct mylite_sql_ast_node *display_width)
{
    enum { decimal_radix = 10U };
    enum { out_of_range_display_width = 256U };
    unsigned int value = 0U;

    if (display_width == NULL || display_width->span.text == NULL) {
        return 0U;
    }

    for (size_t index = 0U; index < display_width->span.length; ++index) {
        unsigned char byte = (unsigned char)display_width->span.text[index];
        if (byte >= '0' && byte <= '9') {
            value = (value * decimal_radix) + (unsigned int)(byte - '0');
            if (value >= out_of_range_display_width) {
                return out_of_range_display_width;
            }
        }
    }
    return value;
}

static bool parse_column_length(const struct mylite_sql_ast_node *length, uint64_t *out_length)
{
    enum { decimal_radix = 10U };
    uint64_t value = 0ULL;

    if (out_length == NULL) {
        return false;
    }
    *out_length = 0ULL;

    if (length == NULL || length->span.text == NULL) {
        return true;
    }

    for (size_t index = 0U; index < length->span.length; ++index) {
        unsigned char byte = (unsigned char)length->span.text[index];
        if (byte >= '0' && byte <= '9') {
            uint64_t digit = (uint64_t)(byte - '0');
            if (value > (UINT64_MAX - digit) / decimal_radix) {
                return false;
            }
            value = (value * decimal_radix) + digit;
        }
    }
    *out_length = value;
    return true;
}

static struct mylite_sql_ast_node *
make_checked_integer_literal(struct mylite_sql_parser_state *state,
                             struct mylite_sql_token integer_token)
{
    struct mylite_sql_ast_node *literal =
        mylite_sql_parser_make_literal(state, integer_token, MYLITE_SQL_AST_LITERAL_INTEGER);
    uint64_t value = 0ULL;

    if (literal == NULL) {
        return NULL;
    }
    if (!parse_column_length(literal, &value)) {
        mylite_sql_parser_state_syntax_error(state, MYLITE_SQL_PARSE_INTEGER, integer_token);
        return NULL;
    }

    mylite_sql_ast_node_set_column_length(literal, value);
    return literal;
}

static const char *column_type_descriptor_name(enum mylite_sql_ast_column_type column_type)
{
    switch (column_type) {
    case MYLITE_SQL_AST_COLUMN_TYPE_CHAR:
        return "CHAR";
    case MYLITE_SQL_AST_COLUMN_TYPE_VARCHAR:
        return "VARCHAR";
    case MYLITE_SQL_AST_COLUMN_TYPE_TINYTEXT:
        return "TINYTEXT";
    case MYLITE_SQL_AST_COLUMN_TYPE_TEXT:
        return "TEXT";
    case MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMTEXT:
        return "MEDIUMTEXT";
    case MYLITE_SQL_AST_COLUMN_TYPE_LONGTEXT:
        return "LONGTEXT";
    case MYLITE_SQL_AST_COLUMN_TYPE_BINARY:
        return "BINARY";
    case MYLITE_SQL_AST_COLUMN_TYPE_VARBINARY:
        return "VARBINARY";
    case MYLITE_SQL_AST_COLUMN_TYPE_TINYBLOB:
        return "TINYBLOB";
    case MYLITE_SQL_AST_COLUMN_TYPE_BLOB:
        return "BLOB";
    case MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMBLOB:
        return "MEDIUMBLOB";
    case MYLITE_SQL_AST_COLUMN_TYPE_LONGBLOB:
        return "LONGBLOB";
    case MYLITE_SQL_AST_COLUMN_TYPE_DECIMAL:
        return "DECIMAL";
    case MYLITE_SQL_AST_COLUMN_TYPE_FLOAT:
        return "FLOAT";
    case MYLITE_SQL_AST_COLUMN_TYPE_DOUBLE:
        return "DOUBLE";
    case MYLITE_SQL_AST_COLUMN_TYPE_DATE:
        return "DATE";
    case MYLITE_SQL_AST_COLUMN_TYPE_TIME:
        return "TIME";
    case MYLITE_SQL_AST_COLUMN_TYPE_DATETIME:
        return "DATETIME";
    case MYLITE_SQL_AST_COLUMN_TYPE_TIMESTAMP:
        return "TIMESTAMP";
    case MYLITE_SQL_AST_COLUMN_TYPE_YEAR:
        return "YEAR";
    case MYLITE_SQL_AST_COLUMN_TYPE_NONE:
    case MYLITE_SQL_AST_COLUMN_TYPE_TINYINT:
    case MYLITE_SQL_AST_COLUMN_TYPE_SMALLINT:
    case MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMINT:
    case MYLITE_SQL_AST_COLUMN_TYPE_INT:
    case MYLITE_SQL_AST_COLUMN_TYPE_BIGINT:
    case MYLITE_SQL_AST_COLUMN_TYPE_BOOL:
    case MYLITE_SQL_AST_COLUMN_TYPE_BOOLEAN:
        break;
    }
    return "";
}

static bool column_type_uses_string_binary_descriptor(enum mylite_sql_ast_column_type column_type)
{
    return (column_type >= MYLITE_SQL_AST_COLUMN_TYPE_CHAR &&
            column_type <= MYLITE_SQL_AST_COLUMN_TYPE_LONGBLOB) != 0;
}

static bool column_type_uses_numeric_descriptor(enum mylite_sql_ast_column_type column_type)
{
    return (column_type >= MYLITE_SQL_AST_COLUMN_TYPE_DECIMAL &&
            column_type <= MYLITE_SQL_AST_COLUMN_TYPE_DOUBLE) != 0;
}

static bool column_type_uses_temporal_descriptor(enum mylite_sql_ast_column_type column_type)
{
    return (column_type >= MYLITE_SQL_AST_COLUMN_TYPE_DATE &&
            column_type <= MYLITE_SQL_AST_COLUMN_TYPE_YEAR) != 0;
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
        {"AND", MYLITE_SQL_PARSE_AND},
        {"AS", MYLITE_SQL_PARSE_AS},
        {"ASC", MYLITE_SQL_PARSE_ASC},
        {"AUTO_INCREMENT", MYLITE_SQL_PARSE_AUTO_INCREMENT},
        {"BIGINT", MYLITE_SQL_PARSE_BIGINT},
        {"BINARY", MYLITE_SQL_PARSE_BINARY},
        {"BETWEEN", MYLITE_SQL_PARSE_BETWEEN},
        {"BEGIN", MYLITE_SQL_PARSE_BEGIN},
        {"BOOL", MYLITE_SQL_PARSE_BOOL},
        {"BOOLEAN", MYLITE_SQL_PARSE_BOOLEAN},
        {"BLOB", MYLITE_SQL_PARSE_BLOB},
        {"BTREE", MYLITE_SQL_PARSE_BTREE},
        {"BY", MYLITE_SQL_PARSE_BY},
        {"BYTE", MYLITE_SQL_PARSE_BYTE},
        {"CASCADE", MYLITE_SQL_PARSE_CASCADE},
        {"CASE", MYLITE_SQL_PARSE_CASE},
        {"CHAR", MYLITE_SQL_PARSE_CHAR},
        {"CHARACTER", MYLITE_SQL_PARSE_CHARACTER},
        {"CHARSET", MYLITE_SQL_PARSE_CHARSET},
        {"CHAIN", MYLITE_SQL_PARSE_CHAIN},
        {"COLLATE", MYLITE_SQL_PARSE_COLLATE},
        {"COLUMN_FORMAT", MYLITE_SQL_PARSE_COLUMN_FORMAT},
        {"COMMENT", MYLITE_SQL_PARSE_COMMENT},
        {"COMMIT", MYLITE_SQL_PARSE_COMMIT},
        {"CONSISTENT", MYLITE_SQL_PARSE_CONSISTENT},
        {"CONSTRAINT", MYLITE_SQL_PARSE_CONSTRAINT},
        {"CREATE", MYLITE_SQL_PARSE_CREATE},
        {"CURRENT_DATE", MYLITE_SQL_PARSE_CURRENT_DATE},
        {"CURRENT_TIME", MYLITE_SQL_PARSE_CURRENT_TIME},
        {"CURRENT_TIMESTAMP", MYLITE_SQL_PARSE_CURRENT_TIMESTAMP},
        {"CURRENT_USER", MYLITE_SQL_PARSE_CURRENT_USER},
        {"DATABASE", MYLITE_SQL_PARSE_DATABASE},
        {"DATABASES", MYLITE_SQL_PARSE_DATABASES},
        {"DATE", MYLITE_SQL_PARSE_DATE},
        {"DATETIME", MYLITE_SQL_PARSE_DATETIME},
        {"DEC", MYLITE_SQL_PARSE_DEC},
        {"DECIMAL", MYLITE_SQL_PARSE_DECIMALKW},
        {"DEFAULT", MYLITE_SQL_PARSE_DEFAULT},
        {"DELETE", MYLITE_SQL_PARSE_DELETE},
        {"DESC", MYLITE_SQL_PARSE_DESC},
        {"DIV", MYLITE_SQL_PARSE_DIV},
        {"DISK", MYLITE_SQL_PARSE_DISK},
        {"DOUBLE", MYLITE_SQL_PARSE_DOUBLE},
        {"DROP", MYLITE_SQL_PARSE_DROP},
        {"DUAL", MYLITE_SQL_PARSE_DUAL},
        {"DYNAMIC", MYLITE_SQL_PARSE_DYNAMIC},
        {"ELSE", MYLITE_SQL_PARSE_ELSE},
        {"END", MYLITE_SQL_PARSE_END},
        {"ENGINE", MYLITE_SQL_PARSE_ENGINE},
        {"ENGINE_ATTRIBUTE", MYLITE_SQL_PARSE_ENGINE_ATTRIBUTE},
        {"ENCRYPTION", MYLITE_SQL_PARSE_ENCRYPTION},
        {"ESCAPE", MYLITE_SQL_PARSE_ESCAPE},
        {"EXISTS", MYLITE_SQL_PARSE_EXISTS},
        {"FALSE", MYLITE_SQL_PARSE_FALSE},
        {"FIXED", MYLITE_SQL_PARSE_FIXED},
        {"FLOAT", MYLITE_SQL_PARSE_FLOATKW},
        {"FLOAT4", MYLITE_SQL_PARSE_FLOAT4},
        {"FLOAT8", MYLITE_SQL_PARSE_FLOAT8},
        {"FROM", MYLITE_SQL_PARSE_FROM},
        {"HASH", MYLITE_SQL_PARSE_HASH},
        {"IF", MYLITE_SQL_PARSE_IF},
        {"IN", MYLITE_SQL_PARSE_IN},
        {"INT", MYLITE_SQL_PARSE_INT},
        {"INT1", MYLITE_SQL_PARSE_INT1},
        {"INT2", MYLITE_SQL_PARSE_INT2},
        {"INT3", MYLITE_SQL_PARSE_INT3},
        {"INT4", MYLITE_SQL_PARSE_INT4},
        {"INT8", MYLITE_SQL_PARSE_INT8},
        {"INTEGER", MYLITE_SQL_PARSE_INTEGERKW},
        {"INDEX", MYLITE_SQL_PARSE_INDEX},
        {"INSERT", MYLITE_SQL_PARSE_INSERT},
        {"INVISIBLE", MYLITE_SQL_PARSE_INVISIBLE},
        {"INTO", MYLITE_SQL_PARSE_INTO},
        {"IS", MYLITE_SQL_PARSE_IS},
        {"KEY", MYLITE_SQL_PARSE_KEY},
        {"KEY_BLOCK_SIZE", MYLITE_SQL_PARSE_KEY_BLOCK_SIZE},
        {"LEFT", MYLITE_SQL_PARSE_LEFT},
        {"LONGBLOB", MYLITE_SQL_PARSE_LONGBLOB},
        {"LONG", MYLITE_SQL_PARSE_LONG},
        {"LONGTEXT", MYLITE_SQL_PARSE_LONGTEXT},
        {"LOCALTIME", MYLITE_SQL_PARSE_LOCALTIME},
        {"LOCALTIMESTAMP", MYLITE_SQL_PARSE_LOCALTIMESTAMP},
        {"LIKE", MYLITE_SQL_PARSE_LIKE},
        {"LIMIT", MYLITE_SQL_PARSE_LIMIT},
        {"MEDIUMINT", MYLITE_SQL_PARSE_MEDIUMINT},
        {"MEDIUMBLOB", MYLITE_SQL_PARSE_MEDIUMBLOB},
        {"MEDIUMTEXT", MYLITE_SQL_PARSE_MEDIUMTEXT},
        {"MEMORY", MYLITE_SQL_PARSE_MEMORY},
        {"MIDDLEINT", MYLITE_SQL_PARSE_MIDDLEINT},
        {"MOD", MYLITE_SQL_PARSE_MOD},
        {"NAMES", MYLITE_SQL_PARSE_NAMES},
        {"NATIONAL", MYLITE_SQL_PARSE_NATIONAL},
        {"NCHAR", MYLITE_SQL_PARSE_NCHAR},
        {"NO", MYLITE_SQL_PARSE_NO},
        {"NOT", MYLITE_SQL_PARSE_NOT},
        {"NULL", MYLITE_SQL_PARSE_NULL},
        {"NVARCHAR", MYLITE_SQL_PARSE_NVARCHAR},
        {"OFFSET", MYLITE_SQL_PARSE_OFFSET},
        {"NUMERIC", MYLITE_SQL_PARSE_NUMERIC},
        {"ON", MYLITE_SQL_PARSE_ON},
        {"ONLY", MYLITE_SQL_PARSE_ONLY},
        {"OR", MYLITE_SQL_PARSE_OR},
        {"ORDER", MYLITE_SQL_PARSE_ORDER},
        {"PRECISION", MYLITE_SQL_PARSE_PRECISION},
        {"PRIMARY", MYLITE_SQL_PARSE_PRIMARY},
        {"READ", MYLITE_SQL_PARSE_READ},
        {"REAL", MYLITE_SQL_PARSE_REAL},
        {"RELEASE", MYLITE_SQL_PARSE_RELEASE},
        {"REPLACE", MYLITE_SQL_PARSE_REPLACE},
        {"RESTRICT", MYLITE_SQL_PARSE_RESTRICT},
        {"RIGHT", MYLITE_SQL_PARSE_RIGHT},
        {"ROLLBACK", MYLITE_SQL_PARSE_ROLLBACK},
        {"ROW", MYLITE_SQL_PARSE_ROW},
        {"SCHEMA", MYLITE_SQL_PARSE_SCHEMA},
        {"SCHEMAS", MYLITE_SQL_PARSE_SCHEMAS},
        {"SAVEPOINT", MYLITE_SQL_PARSE_SAVEPOINT},
        {"SECONDARY_ENGINE_ATTRIBUTE", MYLITE_SQL_PARSE_SECONDARY_ENGINE_ATTRIBUTE},
        {"SELECT", MYLITE_SQL_PARSE_SELECT},
        {"SET", MYLITE_SQL_PARSE_SET},
        {"SHOW", MYLITE_SQL_PARSE_SHOW},
        {"SIGNED", MYLITE_SQL_PARSE_SIGNED},
        {"SMALLINT", MYLITE_SQL_PARSE_SMALLINT},
        {"SNAPSHOT", MYLITE_SQL_PARSE_SNAPSHOT},
        {"START", MYLITE_SQL_PARSE_START},
        {"STORAGE", MYLITE_SQL_PARSE_STORAGE},
        {"TABLE", MYLITE_SQL_PARSE_TABLE},
        {"TEMPORARY", MYLITE_SQL_PARSE_TEMPORARY},
        {"TEXT", MYLITE_SQL_PARSE_TEXT},
        {"THEN", MYLITE_SQL_PARSE_THEN},
        {"TIME", MYLITE_SQL_PARSE_TIME},
        {"TIMESTAMP", MYLITE_SQL_PARSE_TIMESTAMP},
        {"TINYBLOB", MYLITE_SQL_PARSE_TINYBLOB},
        {"TINYINT", MYLITE_SQL_PARSE_TINYINT},
        {"TINYTEXT", MYLITE_SQL_PARSE_TINYTEXT},
        {"TO", MYLITE_SQL_PARSE_TO},
        {"TRANSACTION", MYLITE_SQL_PARSE_TRANSACTION},
        {"TRUE", MYLITE_SQL_PARSE_TRUE},
        {"UNIQUE", MYLITE_SQL_PARSE_UNIQUE},
        {"UNKNOWN", MYLITE_SQL_PARSE_UNKNOWN},
        {"UNSIGNED", MYLITE_SQL_PARSE_UNSIGNED},
        {"UPDATE", MYLITE_SQL_PARSE_UPDATE},
        {"USE", MYLITE_SQL_PARSE_USE},
        {"USING", MYLITE_SQL_PARSE_USING},
        {"UTC_DATE", MYLITE_SQL_PARSE_UTC_DATE},
        {"UTC_TIME", MYLITE_SQL_PARSE_UTC_TIME},
        {"UTC_TIMESTAMP", MYLITE_SQL_PARSE_UTC_TIMESTAMP},
        {"VARBINARY", MYLITE_SQL_PARSE_VARBINARY},
        {"VALUE", MYLITE_SQL_PARSE_VALUE},
        {"VALUES", MYLITE_SQL_PARSE_VALUES},
        {"VARCHAR", MYLITE_SQL_PARSE_VARCHAR},
        {"VARYING", MYLITE_SQL_PARSE_VARYING},
        {"WHEN", MYLITE_SQL_PARSE_WHEN},
        {"WHERE", MYLITE_SQL_PARSE_WHERE},
        {"VISIBLE", MYLITE_SQL_PARSE_VISIBLE},
        {"WITH", MYLITE_SQL_PARSE_WITH},
        {"WORK", MYLITE_SQL_PARSE_WORK},
        {"WRITE", MYLITE_SQL_PARSE_WRITE},
        {"XOR", MYLITE_SQL_PARSE_XOR},
        {"YEAR", MYLITE_SQL_PARSE_YEAR},
        {"ZEROFILL", MYLITE_SQL_PARSE_ZEROFILL},
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
    case MYLITE_SQL_OPERATOR_NULL_SAFE_EQUAL:
        *out_parser_token = MYLITE_SQL_PARSE_NULL_SAFE_EQ;
        return true;
    case MYLITE_SQL_OPERATOR_LEFT_SHIFT:
        *out_parser_token = MYLITE_SQL_PARSE_SHIFT_LEFT;
        return true;
    case MYLITE_SQL_OPERATOR_RIGHT_SHIFT:
        *out_parser_token = MYLITE_SQL_PARSE_SHIFT_RIGHT;
        return true;
    case MYLITE_SQL_OPERATOR_LESS_EQUAL:
        *out_parser_token = MYLITE_SQL_PARSE_LE;
        return true;
    case MYLITE_SQL_OPERATOR_GREATER_EQUAL:
        *out_parser_token = MYLITE_SQL_PARSE_GE;
        return true;
    case MYLITE_SQL_OPERATOR_NOT_EQUAL:
        *out_parser_token = MYLITE_SQL_PARSE_NE;
        return true;
    case MYLITE_SQL_OPERATOR_LOGICAL_AND:
        *out_parser_token = MYLITE_SQL_PARSE_LOGICAL_AND;
        return true;
    case MYLITE_SQL_OPERATOR_LOGICAL_OR:
        *out_parser_token = MYLITE_SQL_PARSE_LOGICAL_OR;
        return true;
    case MYLITE_SQL_OPERATOR_NONE:
    case MYLITE_SQL_OPERATOR_JSON_UNQUOTE_EXTRACT:
    case MYLITE_SQL_OPERATOR_JSON_EXTRACT:
    case MYLITE_SQL_OPERATOR_ASSIGN:
        return false;
    case MYLITE_SQL_OPERATOR_EQUAL:
        *out_parser_token = MYLITE_SQL_PARSE_EQ;
        return true;
    case MYLITE_SQL_OPERATOR_LESS:
        *out_parser_token = MYLITE_SQL_PARSE_LT;
        return true;
    case MYLITE_SQL_OPERATOR_GREATER:
        *out_parser_token = MYLITE_SQL_PARSE_GT;
        return true;
    case MYLITE_SQL_OPERATOR_PERCENT:
        *out_parser_token = MYLITE_SQL_PARSE_PERCENT;
        return true;
    case MYLITE_SQL_OPERATOR_NOT:
        *out_parser_token = MYLITE_SQL_PARSE_LOGICAL_NOT;
        return true;
    case MYLITE_SQL_OPERATOR_BITWISE_NOT:
        *out_parser_token = MYLITE_SQL_PARSE_BIT_NOT;
        return true;
    case MYLITE_SQL_OPERATOR_BITWISE_XOR:
        *out_parser_token = MYLITE_SQL_PARSE_BIT_XOR;
        return true;
    case MYLITE_SQL_OPERATOR_BITWISE_AND:
        *out_parser_token = MYLITE_SQL_PARSE_BIT_AND;
        return true;
    case MYLITE_SQL_OPERATOR_BITWISE_OR:
        *out_parser_token = MYLITE_SQL_PARSE_BIT_OR;
        return true;
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

static bool transaction_characteristics_conflict(const struct mylite_sql_ast_node *left,
                                                 const struct mylite_sql_ast_node *right)
{
    if (left == NULL || right == NULL) {
        return false;
    }

    if (left->transaction_access_mode != MYLITE_SQL_AST_TRANSACTION_ACCESS_NONE &&
        right->transaction_access_mode != MYLITE_SQL_AST_TRANSACTION_ACCESS_NONE &&
        left->transaction_access_mode != right->transaction_access_mode) {
        return true;
    }
    return false;
}

static void set_syntax_error_at_span(struct mylite_sql_parser_state *state,
                                     struct mylite_sql_source_span span)
{
    if (!is_parse_ok(state)) {
        return;
    }

    record_parse_error(state->result, (struct mylite_sql_parse_error){
                                          .status = MYLITE_SQL_PARSE_SYNTAX_ERROR,
                                          .parser_token = -1,
                                          .token =
                                              {
                                                  .kind = MYLITE_SQL_TOKEN_KEYWORD,
                                                  .text = span.text,
                                                  .length = span.length,
                                                  .offset = span.offset,
                                                  .line = span.line,
                                                  .column = span.column,
                                              },
                                      });
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
