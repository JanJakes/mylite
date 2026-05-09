#include "mylite_parser.h"

#include "mylite_parse.h"
#include "mylite_parser_internal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

void *mylite_sql_lemonAlloc(void *(*malloc_proc)(size_t));
void mylite_sql_lemon(
    void *parser,
    int parser_token,
    struct mylite_sql_token token,
    struct mylite_sql_parser_state *state
);
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

static bool map_lexer_token(
    const struct mylite_sql_token *token,
    bool previous_token_was_dot,
    struct mylite_sql_parser_token_map *out_map
);
static void record_parse_error(
    struct mylite_sql_parse_result *result,
    struct mylite_sql_parse_error error
);
static bool is_comment_token(enum mylite_sql_token_kind kind);
static bool map_keyword_token(
    const struct mylite_sql_token *token,
    bool previous_token_was_dot,
    int *out_parser_token
);
static bool map_punctuation_token(const struct mylite_sql_token *token, int *out_parser_token);
static bool map_operator_token(const struct mylite_sql_token *token, int *out_parser_token);
static bool token_text_equals(const struct mylite_sql_token *token, const char *text);
static char ascii_upper(unsigned char byte);
static bool is_parse_ok(const struct mylite_sql_parser_state *state);
static void set_state_status(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_parse_status status
);
static struct mylite_sql_ast_node *make_node(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_ast_node_kind kind,
    struct mylite_sql_source_span span
);
static struct mylite_sql_source_span span_from_token(const struct mylite_sql_token *token);
static struct mylite_sql_source_span span_join(
    struct mylite_sql_source_span left,
    struct mylite_sql_source_span right
);

enum mylite_sql_parse_status mylite_sql_parse(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *out_result
) {
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

    mylite_sql_lexer_init(
        &lexer,
        (struct mylite_sql_lexer_config){
            .input = config.input,
            .length = config.length,
            .modes = config.modes,
        }
    );
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
            record_parse_error(
                out_result,
                (struct mylite_sql_parse_error){
                    .status = MYLITE_SQL_PARSE_LEXER_ERROR,
                    .parser_token = 0,
                    .token = token,
                }
            );
            break;
        }

        if (!map_lexer_token(&token, previous_token_was_dot, &token_map)) {
            record_parse_error(
                out_result,
                (struct mylite_sql_parse_error){
                    .status = MYLITE_SQL_PARSE_SYNTAX_ERROR,
                    .parser_token = -1,
                    .token = token,
                }
            );
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

void mylite_sql_parse_result_deinit(struct mylite_sql_parse_result *result) {
    if (result == NULL) {
        return;
    }

    mylite_sql_ast_deinit(&result->ast);
    memset(result, 0, sizeof(*result));
}

const char *mylite_sql_parse_status_name(enum mylite_sql_parse_status status) {
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

void mylite_sql_parser_state_set_root(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *root
) {
    if (!is_parse_ok(state)) {
        return;
    }

    state->result->root = root;
}

void mylite_sql_parser_state_syntax_error(
    struct mylite_sql_parser_state *state,
    int parser_token,
    struct mylite_sql_token token
) {
    if (!is_parse_ok(state)) {
        return;
    }

    record_parse_error(
        state->result,
        (struct mylite_sql_parse_error){
            .status = MYLITE_SQL_PARSE_SYNTAX_ERROR,
            .parser_token = parser_token,
            .token = token,
        }
    );
}

void mylite_sql_parser_state_parse_failed(struct mylite_sql_parser_state *state) {
    if (!is_parse_ok(state)) {
        return;
    }

    set_state_status(state, MYLITE_SQL_PARSE_SYNTAX_ERROR);
}

void mylite_sql_parser_state_accept(struct mylite_sql_parser_state *state) {
    if (!is_parse_ok(state)) {
        return;
    }

    state->accepted = true;
}

void mylite_sql_parser_state_stack_overflow(struct mylite_sql_parser_state *state) {
    if (!is_parse_ok(state)) {
        return;
    }

    set_state_status(state, MYLITE_SQL_PARSE_STACK_OVERFLOW);
}

struct mylite_sql_ast_node *mylite_sql_parser_make_script(struct mylite_sql_parser_state *state) {
    return make_node(state, MYLITE_SQL_AST_SCRIPT, (struct mylite_sql_source_span){0});
}

struct mylite_sql_ast_node *mylite_sql_parser_make_script_with_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *statement
) {
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

struct mylite_sql_ast_node *mylite_sql_parser_append_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *script,
    struct mylite_sql_ast_node *statement
) {
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
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token select_token,
    struct mylite_sql_ast_node *select_list,
    struct mylite_sql_ast_node *from_clause,
    struct mylite_sql_ast_node *where_clause,
    struct mylite_sql_ast_node *order_clause,
    struct mylite_sql_ast_node *limit_clause
) {
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
    if (order_clause != NULL) {
        span = span_join(span, order_clause->span);
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
    mylite_sql_ast_node_append_child(statement, order_clause);
    mylite_sql_ast_node_append_child(statement, limit_clause);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_use_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token use_token,
    struct mylite_sql_ast_node *schema_name
) {
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

struct mylite_sql_ast_node *mylite_sql_parser_make_create_table_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *if_not_exists_clause,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *columns,
    struct mylite_sql_token right_paren,
    struct mylite_sql_ast_node *table_options
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&create_token), span_from_token(&right_paren));
    struct mylite_sql_ast_node *statement =
        make_node(state, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, columns);
    if (if_not_exists_clause != NULL) {
        mylite_sql_ast_node_append_child(statement, if_not_exists_clause);
    }
    if (table_options != NULL) {
        mylite_sql_ast_node_append_child(statement, table_options);
        mylite_sql_ast_node_set_span(statement, span_join(statement->span, table_options->span));
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_create_if_not_exists_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token if_token,
    struct mylite_sql_token exists_token
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&if_token), span_from_token(&exists_token));

    return make_node(state, MYLITE_SQL_AST_CREATE_IF_NOT_EXISTS_CLAUSE, span);
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_option_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *option
) {
    struct mylite_sql_source_span span =
        option == NULL ? (struct mylite_sql_source_span){0} : option->span;
    struct mylite_sql_ast_node *list = make_node(state, MYLITE_SQL_AST_TABLE_OPTION_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, option);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_table_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *option
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, option);
    if (option != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, option->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_engine_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token engine_token,
    struct mylite_sql_ast_node *engine_name
) {
    struct mylite_sql_source_span span = span_from_token(&engine_token);
    struct mylite_sql_ast_node *option = NULL;

    if (engine_name != NULL) {
        span = span_join(span, engine_name->span);
    }

    option = make_node(state, MYLITE_SQL_AST_TABLE_ENGINE_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, engine_name);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_charset_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token charset_token,
    struct mylite_sql_ast_node *charset_name
) {
    struct mylite_sql_source_span span = span_from_token(&charset_token);
    struct mylite_sql_ast_node *option = NULL;

    if (charset_name != NULL) {
        span = span_join(span, charset_name->span);
    }

    option = make_node(state, MYLITE_SQL_AST_TABLE_CHARSET_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, charset_name);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_collation_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token collate_token,
    struct mylite_sql_ast_node *collation_name
) {
    struct mylite_sql_source_span span = span_from_token(&collate_token);
    struct mylite_sql_ast_node *option = NULL;

    if (collation_name != NULL) {
        span = span_join(span, collation_name->span);
    }

    option = make_node(state, MYLITE_SQL_AST_TABLE_COLLATION_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, collation_name);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_create_schema_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *if_not_exists_clause,
    struct mylite_sql_ast_node *schema_name
) {
    struct mylite_sql_source_span span = span_from_token(&create_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (if_not_exists_clause != NULL) {
        span = span_join(span, if_not_exists_clause->span);
    }
    if (schema_name != NULL) {
        span = span_join(span, schema_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, schema_name);
    if (if_not_exists_clause != NULL) {
        mylite_sql_ast_node_append_child(statement, if_not_exists_clause);
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_create_schema_if_not_exists_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token if_token,
    struct mylite_sql_token exists_token
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&if_token), span_from_token(&exists_token));

    return make_node(state, MYLITE_SQL_AST_CREATE_SCHEMA_IF_NOT_EXISTS_CLAUSE, span);
}

struct mylite_sql_ast_node *mylite_sql_parser_make_drop_table_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token drop_token,
    struct mylite_sql_ast_node *if_exists_clause,
    struct mylite_sql_ast_node *table_names
) {
    struct mylite_sql_source_span span = span_from_token(&drop_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (table_names != NULL) {
        span = span_join(span, table_names->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_DROP_TABLE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_names);
    if (if_exists_clause != NULL) {
        mylite_sql_ast_node_append_child(statement, if_exists_clause);
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_name_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *table_name
) {
    struct mylite_sql_source_span span =
        table_name == NULL ? (struct mylite_sql_source_span){0} : table_name->span;
    struct mylite_sql_ast_node *list = make_node(state, MYLITE_SQL_AST_TABLE_NAME_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, table_name);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_table_name(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *table_name
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, table_name);
    if (table_name != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, table_name->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_drop_if_exists_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token if_token,
    struct mylite_sql_token exists_token
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&if_token), span_from_token(&exists_token));

    return make_node(state, MYLITE_SQL_AST_DROP_IF_EXISTS_CLAUSE, span);
}

struct mylite_sql_ast_node *mylite_sql_parser_make_drop_schema_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token drop_token,
    struct mylite_sql_ast_node *if_exists_clause,
    struct mylite_sql_ast_node *schema_name
) {
    struct mylite_sql_source_span span = span_from_token(&drop_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (if_exists_clause != NULL) {
        span = span_join(span, if_exists_clause->span);
    }
    if (schema_name != NULL) {
        span = span_join(span, schema_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, schema_name);
    if (if_exists_clause != NULL) {
        mylite_sql_ast_node_append_child(statement, if_exists_clause);
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_drop_schema_if_exists_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token if_token,
    struct mylite_sql_token exists_token
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&if_token), span_from_token(&exists_token));

    return make_node(state, MYLITE_SQL_AST_DROP_SCHEMA_IF_EXISTS_CLAUSE, span);
}

struct mylite_sql_ast_node *mylite_sql_parser_make_truncate_table_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token truncate_token,
    struct mylite_sql_ast_node *table_name
) {
    struct mylite_sql_source_span span = span_from_token(&truncate_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_TRUNCATE_TABLE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_tables_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token tables_token,
    struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *like_pattern
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&show_token), span_from_token(&tables_token));
    struct mylite_sql_ast_node *statement = NULL;

    if (like_pattern != NULL) {
        span = span_join(span, like_pattern->span);
    } else if (schema_name != NULL) {
        span = span_join(span, schema_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SHOW_TABLES_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, schema_name);
    mylite_sql_ast_node_append_child(statement, like_pattern);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_table_status_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token status_token,
    struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *like_pattern
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&show_token), span_from_token(&status_token));
    struct mylite_sql_ast_node *statement = NULL;

    if (like_pattern != NULL) {
        span = span_join(span, like_pattern->span);
    } else if (schema_name != NULL) {
        span = span_join(span, schema_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SHOW_TABLE_STATUS_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, schema_name);
    mylite_sql_ast_node_append_child(statement, like_pattern);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_character_set_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token end_token,
    struct mylite_sql_ast_node *like_pattern
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&show_token), span_from_token(&end_token));
    struct mylite_sql_ast_node *statement = NULL;

    if (like_pattern != NULL) {
        span = span_join(span, like_pattern->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SHOW_CHARACTER_SET_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, like_pattern);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_collation_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token collation_token,
    struct mylite_sql_ast_node *like_pattern
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&show_token), span_from_token(&collation_token));
    struct mylite_sql_ast_node *statement = NULL;

    if (like_pattern != NULL) {
        span = span_join(span, like_pattern->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SHOW_COLLATION_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, like_pattern);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_triggers_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token triggers_token,
    struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *like_pattern
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&show_token), span_from_token(&triggers_token));
    struct mylite_sql_ast_node *statement = NULL;

    if (like_pattern != NULL) {
        span = span_join(span, like_pattern->span);
    } else if (schema_name != NULL) {
        span = span_join(span, schema_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SHOW_TRIGGERS_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, schema_name);
    mylite_sql_ast_node_append_child(statement, like_pattern);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_events_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token events_token,
    struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *like_pattern
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&show_token), span_from_token(&events_token));
    struct mylite_sql_ast_node *statement = NULL;

    if (like_pattern != NULL) {
        span = span_join(span, like_pattern->span);
    } else if (schema_name != NULL) {
        span = span_join(span, schema_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SHOW_EVENTS_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, schema_name);
    mylite_sql_ast_node_append_child(statement, like_pattern);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_open_tables_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token tables_token,
    struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *like_pattern
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&show_token), span_from_token(&tables_token));
    struct mylite_sql_ast_node *statement = NULL;

    if (like_pattern != NULL) {
        span = span_join(span, like_pattern->span);
    } else if (schema_name != NULL) {
        span = span_join(span, schema_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SHOW_OPEN_TABLES_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, schema_name);
    mylite_sql_ast_node_append_child(statement, like_pattern);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_routine_status_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token status_token,
    enum mylite_sql_ast_node_kind statement_kind,
    struct mylite_sql_ast_node *like_pattern
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&show_token), span_from_token(&status_token));
    struct mylite_sql_ast_node *statement = NULL;

    if (like_pattern != NULL) {
        span = span_join(span, like_pattern->span);
    }

    statement = make_node(state, statement_kind, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, like_pattern);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_processlist_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token processlist_token,
    enum mylite_sql_ast_node_kind statement_kind
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&show_token), span_from_token(&processlist_token));

    return make_node(state, statement_kind, span);
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_warnings_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token warnings_token,
    struct mylite_sql_ast_node *limit_clause
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&show_token), span_from_token(&warnings_token));
    struct mylite_sql_ast_node *statement = NULL;

    if (limit_clause != NULL) {
        span = span_join(span, limit_clause->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SHOW_WARNINGS_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, limit_clause);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_count_warnings_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_show_count_warnings_tokens tokens
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&tokens.show), span_from_token(&tokens.warnings));

    if (tokens.left_paren.offset != tokens.count.offset + tokens.count.length) {
        mylite_sql_parser_state_syntax_error(state, MYLITE_SQL_PARSE_LPAREN, tokens.left_paren);
        return NULL;
    }

    return make_node(state, MYLITE_SQL_AST_SHOW_COUNT_WARNINGS_STATEMENT, span);
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_errors_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token errors_token,
    struct mylite_sql_ast_node *limit_clause
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&show_token), span_from_token(&errors_token));
    struct mylite_sql_ast_node *statement = NULL;

    if (limit_clause != NULL) {
        span = span_join(span, limit_clause->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SHOW_ERRORS_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, limit_clause);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_count_errors_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_show_count_errors_tokens tokens
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&tokens.show), span_from_token(&tokens.errors));

    if (tokens.left_paren.offset != tokens.count.offset + tokens.count.length) {
        mylite_sql_parser_state_syntax_error(state, MYLITE_SQL_PARSE_LPAREN, tokens.left_paren);
        return NULL;
    }

    return make_node(state, MYLITE_SQL_AST_SHOW_COUNT_ERRORS_STATEMENT, span);
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_columns_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token start_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *like_pattern
) {
    struct mylite_sql_source_span span = span_from_token(&start_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (like_pattern != NULL) {
        span = span_join(span, like_pattern->span);
    } else if (schema_name != NULL) {
        span = span_join(span, schema_name->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, schema_name);
    mylite_sql_ast_node_append_child(statement, like_pattern);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_index_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *schema_name
) {
    struct mylite_sql_source_span span = span_from_token(&show_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (schema_name != NULL) {
        span = span_join(span, schema_name->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SHOW_INDEX_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, schema_name);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_databases_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token databases_token,
    struct mylite_sql_ast_node *like_pattern
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&show_token), span_from_token(&databases_token));
    struct mylite_sql_ast_node *statement = NULL;

    if (like_pattern != NULL) {
        span = span_join(span, like_pattern->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SHOW_DATABASES_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, like_pattern);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_create_table_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_ast_node *table_name
) {
    struct mylite_sql_source_span span = span_from_token(&show_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SHOW_CREATE_TABLE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_create_database_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_ast_node *schema_name
) {
    struct mylite_sql_source_span span = span_from_token(&show_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (schema_name != NULL) {
        span = span_join(span, schema_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SHOW_CREATE_DATABASE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, schema_name);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_engines_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token engines_token
) {
    return make_node(
        state,
        MYLITE_SQL_AST_SHOW_ENGINES_STATEMENT,
        span_join(span_from_token(&show_token), span_from_token(&engines_token))
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_rename_table_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token rename_token,
    struct mylite_sql_ast_node *pairs
) {
    struct mylite_sql_source_span span = span_from_token(&rename_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (pairs != NULL) {
        span = span_join(span, pairs->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_RENAME_TABLE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, pairs);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_rename_table_pair_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *pair
) {
    struct mylite_sql_source_span span =
        pair == NULL ? (struct mylite_sql_source_span){0} : pair->span;
    struct mylite_sql_ast_node *list =
        make_node(state, MYLITE_SQL_AST_RENAME_TABLE_PAIR_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, pair);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_rename_table_pair(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *pair
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, pair);
    if (pair != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, pair->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_rename_table_pair(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *source_name,
    struct mylite_sql_token to_token,
    struct mylite_sql_ast_node *target_name
) {
    struct mylite_sql_source_span span =
        source_name == NULL ? span_from_token(&to_token) : source_name->span;
    struct mylite_sql_ast_node *pair = NULL;

    if (target_name != NULL) {
        span = span_join(span, target_name->span);
    }

    pair = make_node(state, MYLITE_SQL_AST_RENAME_TABLE_PAIR, span);
    if (pair == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(pair, source_name);
    mylite_sql_ast_node_append_child(pair, target_name);
    return pair;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_rename_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *source_name,
    struct mylite_sql_ast_node *target_name
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (target_name != NULL) {
        span = span_join(span, target_name->span);
    } else if (source_name != NULL) {
        span = span_join(span, source_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_RENAME_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, source_name);
    mylite_sql_ast_node_append_child(statement, target_name);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_add_column_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *column
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (column != NULL) {
        span = span_join(span, column->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_ADD_COLUMN_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, column);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_drop_column_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *column_name
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (column_name != NULL) {
        span = span_join(span, column_name->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_DROP_COLUMN_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, column_name);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_rename_column_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *old_column_name,
    struct mylite_sql_ast_node *new_column_name
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (new_column_name != NULL) {
        span = span_join(span, new_column_name->span);
    } else if (old_column_name != NULL) {
        span = span_join(span, old_column_name->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_RENAME_COLUMN_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, old_column_name);
    mylite_sql_ast_node_append_child(statement, new_column_name);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_modify_column_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *column
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (column != NULL) {
        span = span_join(span, column->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_MODIFY_COLUMN_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, column);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_change_column_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *old_column_name,
    struct mylite_sql_ast_node *column
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (column != NULL) {
        span = span_join(span, column->span);
    } else if (old_column_name != NULL) {
        span = span_join(span, old_column_name->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_CHANGE_COLUMN_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, old_column_name);
    mylite_sql_ast_node_append_child(statement, column);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_set_default_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *column_name,
    struct mylite_sql_ast_node *default_node
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (default_node != NULL) {
        span = span_join(span, default_node->span);
    } else if (column_name != NULL) {
        span = span_join(span, column_name->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_SET_DEFAULT_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, column_name);
    mylite_sql_ast_node_append_child(statement, default_node);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token insert_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *columns,
    struct mylite_sql_ast_node *rows
) {
    struct mylite_sql_source_span span = span_from_token(&insert_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (rows != NULL) {
        span = span_join(span, rows->span);
    } else if (columns != NULL) {
        span = span_join(span, columns->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_INSERT_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, columns);
    mylite_sql_ast_node_append_child(statement, rows);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_set_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token insert_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *assignments
) {
    struct mylite_sql_source_span span = span_from_token(&insert_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (assignments != NULL) {
        span = span_join(span, assignments->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_INSERT_SET_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, assignments);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_assignment_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *assignment
) {
    struct mylite_sql_source_span span =
        assignment == NULL ? (struct mylite_sql_source_span){0} : assignment->span;
    struct mylite_sql_ast_node *list =
        make_node(state, MYLITE_SQL_AST_INSERT_ASSIGNMENT_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, assignment);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_insert_assignment(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *assignment
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, assignment);
    if (assignment != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, assignment->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_assignment(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *target,
    struct mylite_sql_token equals_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span =
        target == NULL ? span_from_token(&equals_token) : target->span;
    struct mylite_sql_ast_node *assignment = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }

    assignment = make_node(state, MYLITE_SQL_AST_INSERT_ASSIGNMENT, span);
    if (assignment == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(assignment, target);
    mylite_sql_ast_node_append_child(assignment, value);
    return assignment;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_delete_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token delete_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *where_clause,
    struct mylite_sql_ast_node *order_clause,
    struct mylite_sql_ast_node *limit_clause
) {
    struct mylite_sql_source_span span = span_from_token(&delete_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }
    if (where_clause != NULL) {
        span = span_join(span, where_clause->span);
    }
    if (order_clause != NULL) {
        span = span_join(span, order_clause->span);
    }
    if (limit_clause != NULL) {
        span = span_join(span, limit_clause->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_DELETE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, where_clause);
    mylite_sql_ast_node_append_child(statement, order_clause);
    mylite_sql_ast_node_append_child(statement, limit_clause);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_update_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token update_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *assignments,
    struct mylite_sql_ast_node *where_clause,
    struct mylite_sql_ast_node *order_clause,
    struct mylite_sql_ast_node *limit_clause
) {
    struct mylite_sql_source_span span = span_from_token(&update_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }
    if (assignments != NULL) {
        span = span_join(span, assignments->span);
    }
    if (where_clause != NULL) {
        span = span_join(span, where_clause->span);
    }
    if (order_clause != NULL) {
        span = span_join(span, order_clause->span);
    }
    if (limit_clause != NULL) {
        span = span_join(span, limit_clause->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_UPDATE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, assignments);
    mylite_sql_ast_node_append_child(statement, where_clause);
    mylite_sql_ast_node_append_child(statement, order_clause);
    mylite_sql_ast_node_append_child(statement, limit_clause);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_update_assignment_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *assignment
) {
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

struct mylite_sql_ast_node *mylite_sql_parser_append_update_assignment(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *assignment
) {
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
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *target,
    struct mylite_sql_token equals_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span =
        target == NULL ? span_from_token(&equals_token) : target->span;
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

struct mylite_sql_ast_node *mylite_sql_parser_make_wildcard_select_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token wildcard_token
) {
    return mylite_sql_parser_make_select_list(
        state,
        mylite_sql_parser_make_select_item(
            state,
            mylite_sql_parser_make_wildcard(state, wildcard_token)
        )
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_select_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *item
) {
    struct mylite_sql_source_span span =
        item == NULL ? (struct mylite_sql_source_span){0} : item->span;
    struct mylite_sql_ast_node *list = make_node(state, MYLITE_SQL_AST_SELECT_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, item);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_select_item(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *item
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, item);
    if (item != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, item->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_select_item(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *expression
) {
    struct mylite_sql_source_span span =
        expression == NULL ? (struct mylite_sql_source_span){0} : expression->span;
    struct mylite_sql_ast_node *item = make_node(state, MYLITE_SQL_AST_SELECT_ITEM, span);
    if (item == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(item, expression);
    return item;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_from_dual(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token from_token,
    struct mylite_sql_token dual_token
) {
    return make_node(
        state,
        MYLITE_SQL_AST_FROM_DUAL,
        span_join(span_from_token(&from_token), span_from_token(&dual_token))
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_from_table(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token from_token,
    struct mylite_sql_ast_node *table_name
) {
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

struct mylite_sql_ast_node *mylite_sql_parser_make_where_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token where_token,
    struct mylite_sql_ast_node *predicate
) {
    struct mylite_sql_source_span span = span_from_token(&where_token);
    struct mylite_sql_ast_node *where_clause = NULL;

    if (predicate != NULL) {
        span = span_join(span, predicate->span);
    }

    where_clause = make_node(state, MYLITE_SQL_AST_WHERE_CLAUSE, span);
    if (where_clause == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(where_clause, predicate);
    return where_clause;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_comparison_predicate(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *left,
    struct mylite_sql_token operator_token,
    enum mylite_sql_ast_operator operator_kind,
    struct mylite_sql_ast_node *right
) {
    struct mylite_sql_source_span span =
        left == NULL ? span_from_token(&operator_token) : left->span;
    struct mylite_sql_ast_node *predicate = NULL;

    if (right != NULL) {
        span = span_join(span, right->span);
    }

    predicate = make_node(state, MYLITE_SQL_AST_COMPARISON_PREDICATE, span);
    if (predicate == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_operator(predicate, operator_kind);
    mylite_sql_ast_node_append_child(predicate, left);
    mylite_sql_ast_node_append_child(predicate, right);
    return predicate;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_is_null_predicate(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *left,
    struct mylite_sql_token is_token,
    enum mylite_sql_ast_operator operator_kind,
    struct mylite_sql_token null_token
) {
    struct mylite_sql_source_span span = left == NULL ? span_from_token(&is_token) : left->span;
    struct mylite_sql_ast_node *predicate = NULL;

    span = span_join(span, span_from_token(&null_token));
    predicate = make_node(state, MYLITE_SQL_AST_IS_NULL_PREDICATE, span);
    if (predicate == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_operator(predicate, operator_kind);
    mylite_sql_ast_node_append_child(predicate, left);
    return predicate;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_order_by_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token order_token,
    struct mylite_sql_ast_node *order_key,
    struct mylite_sql_ast_node *direction
) {
    struct mylite_sql_source_span span = span_from_token(&order_token);
    struct mylite_sql_ast_node *order_clause = NULL;

    if (direction != NULL) {
        span = span_join(span, direction->span);
    } else if (order_key != NULL) {
        span = span_join(span, order_key->span);
    }

    order_clause = make_node(state, MYLITE_SQL_AST_ORDER_BY_CLAUSE, span);
    if (order_clause == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(order_clause, order_key);
    mylite_sql_ast_node_append_child(order_clause, direction);
    return order_clause;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_order_direction(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token direction_token,
    enum mylite_sql_ast_order_direction direction
) {
    struct mylite_sql_ast_node *direction_node =
        make_node(state, MYLITE_SQL_AST_ORDER_DIRECTION, span_from_token(&direction_token));
    if (direction_node == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_order_direction(direction_node, direction);
    return direction_node;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_limit_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token limit_token,
    struct mylite_sql_ast_node *row_count,
    struct mylite_sql_ast_node *offset
) {
    struct mylite_sql_source_span span = span_from_token(&limit_token);
    struct mylite_sql_ast_node *limit_clause = NULL;

    if (offset != NULL) {
        span = span_join(span, offset->span);
    }
    if (row_count != NULL) {
        span = span_join(span, row_count->span);
    }

    limit_clause = make_node(state, MYLITE_SQL_AST_LIMIT_CLAUSE, span);
    if (limit_clause == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(limit_clause, row_count);
    mylite_sql_ast_node_append_child(limit_clause, offset);
    return limit_clause;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_identifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
) {
    return make_node(state, MYLITE_SQL_AST_IDENTIFIER, span_from_token(&token));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_qualified_identifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *left,
    struct mylite_sql_ast_node *right
) {
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

struct mylite_sql_ast_node *mylite_sql_parser_make_wildcard(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
) {
    return make_node(state, MYLITE_SQL_AST_WILDCARD, span_from_token(&token));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_literal(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token,
    enum mylite_sql_ast_literal_kind literal_kind
) {
    struct mylite_sql_ast_node *literal =
        make_node(state, MYLITE_SQL_AST_LITERAL, span_from_token(&token));
    if (literal == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_literal_kind(literal, literal_kind);
    return literal;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_system_variable(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
) {
    return make_node(state, MYLITE_SQL_AST_SYSTEM_VARIABLE, span_from_token(&token));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_unary_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token operator_token,
    enum mylite_sql_ast_operator operator_kind,
    struct mylite_sql_ast_node *operand
) {
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
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *left,
    struct mylite_sql_token operator_token,
    enum mylite_sql_ast_operator operator_kind,
    struct mylite_sql_ast_node *right
) {
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
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token left_paren,
    struct mylite_sql_ast_node *expression,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_ast_node *parenthesized = make_node(
        state,
        MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION,
        span_join(span_from_token(&left_paren), span_from_token(&right_paren))
    );
    if (parenthesized == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(parenthesized, expression);
    return parenthesized;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_zero_argument_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_token right_paren
) {
    return make_node(
        state,
        function_kind,
        span_join(span_from_token(&function_token), span_from_token(&right_paren))
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_no_space_zero_argument_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    struct mylite_sql_token left_paren,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_token right_paren
) {
    if (left_paren.offset != function_token.offset + function_token.length) {
        mylite_sql_parser_state_syntax_error(state, MYLITE_SQL_PARSE_LPAREN, left_paren);
        return NULL;
    }

    return mylite_sql_parser_make_zero_argument_function(
        state,
        function_token,
        function_kind,
        right_paren
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_function_argument_count_error(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    enum mylite_sql_ast_node_kind error_kind,
    struct mylite_sql_ast_node *arguments,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_ast_node *error = NULL;

    error = make_node(
        state,
        error_kind,
        span_join(span_from_token(&function_token), span_from_token(&right_paren))
    );
    if (error == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(error, arguments);
    return error;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_function_argument_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *argument
) {
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

struct mylite_sql_ast_node *mylite_sql_parser_append_function_argument(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *argument
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, argument);
    if (argument != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, argument->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_current_user_keyword(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token current_user_token
) {
    return make_node(
        state,
        MYLITE_SQL_AST_CURRENT_USER_FUNCTION,
        span_from_token(&current_user_token)
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_definition_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *column
) {
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

struct mylite_sql_ast_node *mylite_sql_parser_append_column_definition(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *column
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, column);
    if (column != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, column->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_definition(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *name,
    struct mylite_sql_ast_node *integer_type,
    struct mylite_sql_ast_node *nullability,
    struct mylite_sql_ast_node *default_null
) {
    struct mylite_sql_source_span span =
        name == NULL ? (struct mylite_sql_source_span){0} : name->span;
    struct mylite_sql_ast_node *column = NULL;

    if (integer_type != NULL) {
        span = span_join(span, integer_type->span);
    }
    if (nullability != NULL) {
        span = span_join(span, nullability->span);
    }
    if (default_null != NULL) {
        span = span_join(span, default_null->span);
    }

    column = make_node(state, MYLITE_SQL_AST_COLUMN_DEFINITION, span);
    if (column == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(column, name);
    mylite_sql_ast_node_append_child(column, integer_type);
    mylite_sql_ast_node_append_child(column, nullability);
    mylite_sql_ast_node_append_child(column, default_null);
    return column;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_default_null(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token default_token,
    struct mylite_sql_token null_token
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&default_token), span_from_token(&null_token));

    return make_node(state, MYLITE_SQL_AST_COLUMN_DEFAULT_NULL, span);
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_default_value(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token default_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = span_from_token(&default_token);
    struct mylite_sql_ast_node *default_value = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }

    default_value = make_node(state, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE, span);
    if (default_value == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(default_value, value);
    return default_value;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_integer_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token type_token,
    enum mylite_sql_ast_integer_type integer_type,
    struct mylite_sql_token display_width_token,
    struct mylite_sql_token display_width_end_token,
    struct mylite_sql_token attribute_token,
    int is_unsigned,
    int is_bool_alias
) {
    struct mylite_sql_source_span span = span_from_token(&type_token);
    struct mylite_sql_ast_node *type = NULL;

    if (display_width_end_token.text != NULL) {
        span = span_join(span, span_from_token(&display_width_end_token));
    }
    if (attribute_token.text != NULL) {
        span = span_join(span, span_from_token(&attribute_token));
    }

    type = make_node(state, MYLITE_SQL_AST_INTEGER_TYPE, span);
    if (type == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_integer_type(
        type,
        (struct mylite_sql_ast_integer_type_payload){
            .kind = integer_type,
            .is_unsigned = is_unsigned,
            .has_display_width = display_width_token.text != NULL,
            .is_bool_alias = is_bool_alias,
            .display_width_span = span_from_token(&display_width_token),
        }
    );
    return type;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_nullability(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_ast_nullability nullability,
    struct mylite_sql_token first_token,
    struct mylite_sql_token last_token
) {
    struct mylite_sql_ast_node *node = make_node(
        state,
        MYLITE_SQL_AST_NULLABILITY,
        span_join(span_from_token(&first_token), span_from_token(&last_token))
    );
    if (node == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_nullability(node, nullability);
    return node;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_identifier_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *identifier
) {
    struct mylite_sql_source_span span =
        identifier == NULL ? (struct mylite_sql_source_span){0} : identifier->span;
    struct mylite_sql_ast_node *list = make_node(state, MYLITE_SQL_AST_IDENTIFIER_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, identifier);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_identifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *identifier
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, identifier);
    if (identifier != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, identifier->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_row_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *row
) {
    struct mylite_sql_source_span span =
        row == NULL ? (struct mylite_sql_source_span){0} : row->span;
    struct mylite_sql_ast_node *list = make_node(state, MYLITE_SQL_AST_INSERT_ROW_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, row);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_insert_row(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *row
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, row);
    if (row != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, row->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_row_values(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span =
        value == NULL ? (struct mylite_sql_source_span){0} : value->span;
    struct mylite_sql_ast_node *row = make_node(state, MYLITE_SQL_AST_INSERT_ROW, span);
    if (row == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(row, value);
    return row;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_insert_value(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *row,
    struct mylite_sql_ast_node *value
) {
    if (!is_parse_ok(state) || row == NULL) {
        return row;
    }

    mylite_sql_ast_node_append_child(row, value);
    if (value != NULL) {
        mylite_sql_ast_node_set_span(row, span_join(row->span, value->span));
    }
    return row;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_row(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token left_paren,
    struct mylite_sql_ast_node *values,
    struct mylite_sql_token right_paren
) {
    if (!is_parse_ok(state) || values == NULL) {
        return values;
    }

    mylite_sql_ast_node_set_span(
        values,
        span_join(span_from_token(&left_paren), span_from_token(&right_paren))
    );
    return values;
}

static bool map_lexer_token(
    const struct mylite_sql_token *token,
    bool previous_token_was_dot,
    struct mylite_sql_parser_token_map *out_map
) {
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
    case MYLITE_SQL_TOKEN_PARAMETER:
        return false;
    case MYLITE_SQL_TOKEN_SYSTEM_VARIABLE:
        parser_token = MYLITE_SQL_PARSE_SYSTEM_VARIABLE;
        break;
    }

    *out_map = (struct mylite_sql_parser_token_map){
        .parser_token = parser_token,
        .previous_token_was_dot = parser_token == MYLITE_SQL_PARSE_DOT,
    };
    return true;
}

static void record_parse_error(
    struct mylite_sql_parse_result *result,
    struct mylite_sql_parse_error error
) {
    if (result == NULL || result->status != MYLITE_SQL_PARSE_OK) {
        return;
    }

    result->status = error.status;
    result->parser_token = error.parser_token;
    result->error_token = error.token;
}

static bool is_comment_token(enum mylite_sql_token_kind kind) {
    if (kind == MYLITE_SQL_TOKEN_COMMENT || kind == MYLITE_SQL_TOKEN_VERSION_COMMENT ||
        kind == MYLITE_SQL_TOKEN_HINT_COMMENT) {
        return true;
    }
    return false;
}

static bool map_keyword_token(
    const struct mylite_sql_token *token,
    bool previous_token_was_dot,
    int *out_parser_token
) {
    static const struct {
        const char *keyword;
        int parser_token;
    } keyword_mappings[] = {
        {"SELECT", MYLITE_SQL_PARSE_SELECT},
        {"ALTER", MYLITE_SQL_PARSE_ALTER},
        {"AS", MYLITE_SQL_PARSE_AS},
        {"FROM", MYLITE_SQL_PARSE_FROM},
        {"WHERE", MYLITE_SQL_PARSE_WHERE},
        {"ORDER", MYLITE_SQL_PARSE_ORDER},
        {"BY", MYLITE_SQL_PARSE_BY},
        {"CONNECTION_ID", MYLITE_SQL_PARSE_CONNECTION_ID},
        {"COUNT", MYLITE_SQL_PARSE_COUNT},
        {"CURRENT_ROLE", MYLITE_SQL_PARSE_CURRENT_ROLE},
        {"CURRENT_USER", MYLITE_SQL_PARSE_CURRENT_USER},
        {"ASC", MYLITE_SQL_PARSE_ASC},
        {"DESC", MYLITE_SQL_PARSE_DESC},
        {"LIMIT", MYLITE_SQL_PARSE_LIMIT},
        {"OFFSET", MYLITE_SQL_PARSE_OFFSET},
        {"USE", MYLITE_SQL_PARSE_USE},
        {"CREATE", MYLITE_SQL_PARSE_CREATE},
        {"TABLE", MYLITE_SQL_PARSE_TABLE},
        {"IF", MYLITE_SQL_PARSE_IF},
        {"EXISTS", MYLITE_SQL_PARSE_EXISTS},
        {"DATABASE", MYLITE_SQL_PARSE_DATABASE},
        {"DATABASES", MYLITE_SQL_PARSE_DATABASES},
        {"DROP", MYLITE_SQL_PARSE_DROP},
        {"TRUNCATE", MYLITE_SQL_PARSE_TRUNCATE},
        {"SHOW", MYLITE_SQL_PARSE_SHOW},
        {"TABLES", MYLITE_SQL_PARSE_TABLES},
        {"COLUMNS", MYLITE_SQL_PARSE_COLUMNS},
        {"FIELDS", MYLITE_SQL_PARSE_FIELDS},
        {"INDEX", MYLITE_SQL_PARSE_INDEX},
        {"INDEXES", MYLITE_SQL_PARSE_INDEXES},
        {"KEYS", MYLITE_SQL_PARSE_KEYS},
        {"FULL", MYLITE_SQL_PARSE_FULL},
        {"TRIGGERS", MYLITE_SQL_PARSE_TRIGGERS},
        {"EVENTS", MYLITE_SQL_PARSE_EVENTS},
        {"OPEN", MYLITE_SQL_PARSE_OPEN},
        {"PROCESSLIST", MYLITE_SQL_PARSE_PROCESSLIST},
        {"WARNINGS", MYLITE_SQL_PARSE_WARNINGS},
        {"ERRORS", MYLITE_SQL_PARSE_ERRORS},
        {"PROCEDURE", MYLITE_SQL_PARSE_PROCEDURE},
        {"FUNCTION", MYLITE_SQL_PARSE_FUNCTION},
        {"ENGINE", MYLITE_SQL_PARSE_ENGINE},
        {"ENGINES", MYLITE_SQL_PARSE_ENGINES},
        {"STATUS", MYLITE_SQL_PARSE_STATUS},
        {"STORAGE", MYLITE_SQL_PARSE_STORAGE},
        {"DEFAULT", MYLITE_SQL_PARSE_DEFAULT},
        {"CHARACTER", MYLITE_SQL_PARSE_CHARACTER},
        {"CHARSET", MYLITE_SQL_PARSE_CHARSET},
        {"COLLATE", MYLITE_SQL_PARSE_COLLATE},
        {"COLLATION", MYLITE_SQL_PARSE_COLLATION},
        {"LIKE", MYLITE_SQL_PARSE_LIKE},
        {"SCHEMA", MYLITE_SQL_PARSE_SCHEMA},
        {"SCHEMAS", MYLITE_SQL_PARSE_SCHEMAS},
        {"DESCRIBE", MYLITE_SQL_PARSE_DESCRIBE},
        {"EXPLAIN", MYLITE_SQL_PARSE_EXPLAIN},
        {"SESSION_USER", MYLITE_SQL_PARSE_SESSION_USER},
        {"RENAME", MYLITE_SQL_PARSE_RENAME},
        {"ADD", MYLITE_SQL_PARSE_ADD},
        {"MODIFY", MYLITE_SQL_PARSE_MODIFY},
        {"CHANGE", MYLITE_SQL_PARSE_CHANGE},
        {"COLUMN", MYLITE_SQL_PARSE_COLUMN},
        {"INSERT", MYLITE_SQL_PARSE_INSERT},
        {"INTO", MYLITE_SQL_PARSE_INTO},
        {"VALUES", MYLITE_SQL_PARSE_VALUES},
        {"TO", MYLITE_SQL_PARSE_TO},
        {"DELETE", MYLITE_SQL_PARSE_DELETE},
        {"UPDATE", MYLITE_SQL_PARSE_UPDATE},
        {"SET", MYLITE_SQL_PARSE_SET},
        {"INT", MYLITE_SQL_PARSE_INT},
        {"TINYINT", MYLITE_SQL_PARSE_TINYINT},
        {"SMALLINT", MYLITE_SQL_PARSE_SMALLINT},
        {"MEDIUMINT", MYLITE_SQL_PARSE_MEDIUMINT},
        {"INTEGER", MYLITE_SQL_PARSE_INTEGER_TYPE},
        {"BIGINT", MYLITE_SQL_PARSE_BIGINT},
        {"BOOL", MYLITE_SQL_PARSE_BOOL},
        {"BOOLEAN", MYLITE_SQL_PARSE_BOOLEAN},
        {"INT1", MYLITE_SQL_PARSE_INT1},
        {"INT2", MYLITE_SQL_PARSE_INT2},
        {"INT3", MYLITE_SQL_PARSE_INT3},
        {"INT4", MYLITE_SQL_PARSE_INT4},
        {"INT8", MYLITE_SQL_PARSE_INT8},
        {"SIGNED", MYLITE_SQL_PARSE_SIGNED},
        {"UNSIGNED", MYLITE_SQL_PARSE_UNSIGNED},
        {"NOT", MYLITE_SQL_PARSE_NOT},
        {"IS", MYLITE_SQL_PARSE_IS},
        {"IN", MYLITE_SQL_PARSE_IN},
        {"TRUE", MYLITE_SQL_PARSE_TRUE},
        {"FALSE", MYLITE_SQL_PARSE_FALSE},
        {"NULL", MYLITE_SQL_PARSE_NULL},
        {"DUAL", MYLITE_SQL_PARSE_DUAL},
        {"USER", MYLITE_SQL_PARSE_USER},
        {"VERSION", MYLITE_SQL_PARSE_VERSION},
        {"ROW_COUNT", MYLITE_SQL_PARSE_ROW_COUNT},
        {"SYSTEM_USER", MYLITE_SQL_PARSE_SYSTEM_USER},
    };

    if (previous_token_was_dot) {
        *out_parser_token = MYLITE_SQL_PARSE_IDENTIFIER;
        return true;
    }

    for (size_t index = 0U; index < sizeof(keyword_mappings) / sizeof(keyword_mappings[0]);
         ++index) {
        if (token_text_equals(token, keyword_mappings[index].keyword)) {
            *out_parser_token = keyword_mappings[index].parser_token;
            return true;
        }
    }

    if ((token->keyword_flags & MYLITE_SQL_KEYWORD_RESERVED) != 0U) {
        return false;
    }

    *out_parser_token = MYLITE_SQL_PARSE_IDENTIFIER;
    return true;
}

static bool map_punctuation_token(const struct mylite_sql_token *token, int *out_parser_token) {
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

static bool map_operator_token(const struct mylite_sql_token *token, int *out_parser_token) {
    switch (token->operator_kind) {
    case MYLITE_SQL_OPERATOR_NULL_SAFE_EQUAL:
        *out_parser_token = MYLITE_SQL_PARSE_NULL_SAFE_EQUAL;
        return true;
    case MYLITE_SQL_OPERATOR_LESS_EQUAL:
        *out_parser_token = MYLITE_SQL_PARSE_LESS_EQUAL;
        return true;
    case MYLITE_SQL_OPERATOR_GREATER_EQUAL:
        *out_parser_token = MYLITE_SQL_PARSE_GREATER_EQUAL;
        return true;
    case MYLITE_SQL_OPERATOR_NOT_EQUAL:
        *out_parser_token = MYLITE_SQL_PARSE_NOT_EQUAL;
        return true;
    case MYLITE_SQL_OPERATOR_EQUAL:
        *out_parser_token = MYLITE_SQL_PARSE_EQUAL;
        return true;
    case MYLITE_SQL_OPERATOR_LESS:
        *out_parser_token = MYLITE_SQL_PARSE_LESS;
        return true;
    case MYLITE_SQL_OPERATOR_GREATER:
        *out_parser_token = MYLITE_SQL_PARSE_GREATER;
        return true;
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
    case MYLITE_SQL_OPERATOR_LEFT_SHIFT:
    case MYLITE_SQL_OPERATOR_RIGHT_SHIFT:
    case MYLITE_SQL_OPERATOR_LOGICAL_AND:
    case MYLITE_SQL_OPERATOR_LOGICAL_OR:
    case MYLITE_SQL_OPERATOR_ASSIGN:
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

static bool token_text_equals(const struct mylite_sql_token *token, const char *text) {
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

static char ascii_upper(unsigned char byte) {
    if (byte >= 'a' && byte <= 'z') {
        return (char)(byte - ('a' - 'A'));
    }
    return (char)byte;
}

static bool is_parse_ok(const struct mylite_sql_parser_state *state) {
    if (state != NULL && state->result != NULL && state->result->status == MYLITE_SQL_PARSE_OK) {
        return true;
    }
    return false;
}

static void set_state_status(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_parse_status status
) {
    if (!is_parse_ok(state)) {
        return;
    }

    state->result->status = status;
}

static struct mylite_sql_ast_node *make_node(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_ast_node_kind kind,
    struct mylite_sql_source_span span
) {
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

static struct mylite_sql_source_span span_from_token(const struct mylite_sql_token *token) {
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

static struct mylite_sql_source_span span_join(
    struct mylite_sql_source_span left,
    struct mylite_sql_source_span right
) {
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
