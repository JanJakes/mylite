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

struct column_attribute_positions {
    size_t charset;
    size_t collation;
    size_t comment;
    size_t nullability;
    size_t default_value;
    size_t primary_key;
    size_t unique_key;
    size_t auto_increment;
    size_t generated;
};

static bool map_lexer_token(
    const struct mylite_sql_parser_state *state,
    const struct mylite_sql_token *token,
    bool previous_token_was_dot,
    int previous_parser_token,
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
    int previous_parser_token,
    int *out_parser_token
);
static bool map_punctuation_token(const struct mylite_sql_token *token, int *out_parser_token);
static bool map_operator_token(
    const struct mylite_sql_parser_state *state,
    const struct mylite_sql_token *token,
    int *out_parser_token
);
static bool previous_token_allows_select_noop_modifier(int previous_parser_token);
static bool token_text_equals(const struct mylite_sql_token *token, const char *text);
static char ascii_upper(unsigned char byte);
static bool is_parse_ok(const struct mylite_sql_parser_state *state);
static bool parser_sql_mode_has(
    const struct mylite_sql_parser_state *state,
    enum mylite_sql_mode mode
);
static bool create_table_name_is_no_space_function_identifier(
    const struct mylite_sql_parser_state *state,
    const struct mylite_sql_ast_node *table_name,
    const struct mylite_sql_token *left_paren
);
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
static void apply_alter_table_options(
    struct mylite_sql_ast_node *statement,
    struct mylite_sql_alter_table_options options
);
static const struct mylite_sql_ast_node *last_identifier_component(
    const struct mylite_sql_ast_node *identifier
);
static bool span_text_equals(const struct mylite_sql_source_span *span, const char *text);
static bool span_text_matches_ignore_space_function_name(const struct mylite_sql_source_span *span);
static int scan_column_attribute_positions(
    struct mylite_sql_parser_state *state,
    const struct mylite_sql_ast_node *attributes,
    struct column_attribute_positions *out_positions
);
static int record_column_attribute_position(
    struct mylite_sql_parser_state *state,
    size_t *slot,
    size_t position
);
static int validate_legacy_column_attribute_order(
    struct mylite_sql_parser_state *state,
    const struct column_attribute_positions *positions
);
static size_t column_charset_collation_position_limit(
    const struct column_attribute_positions *positions
);
static bool legacy_column_attribute_precedes_charset_collation(
    const struct column_attribute_positions *positions,
    size_t charset_collation_limit
);
static bool column_attribute_position_is_set(size_t position);

enum mylite_sql_parse_status mylite_sql_parse(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *out_result
) {
    struct mylite_sql_parser_state state;
    struct mylite_sql_lexer lexer;
    void *parser = NULL;
    bool previous_token_was_dot = false;
    int previous_parser_token = 0;

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
        .modes = config.modes,
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

        if (!map_lexer_token(
                &state,
                &token,
                previous_token_was_dot,
                previous_parser_token,
                &token_map
            )) {
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
        previous_parser_token = token_map.parser_token;

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

struct mylite_sql_ast_node *mylite_sql_parser_make_table_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token table_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *order_clause,
    struct mylite_sql_ast_node *limit_clause
) {
    return mylite_sql_parser_make_select_statement(
        state,
        table_token,
        mylite_sql_parser_make_wildcard_select_list(state, table_token),
        mylite_sql_parser_make_from_table(state, table_token, table_name, NULL, NULL),
        NULL,
        NULL,
        NULL,
        order_clause,
        limit_clause
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_select_statement_with_modifiers(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token select_token,
    struct mylite_sql_select_modifiers modifiers,
    struct mylite_sql_ast_node *select_list,
    struct mylite_sql_ast_node *from_clause,
    struct mylite_sql_ast_node *where_clause,
    struct mylite_sql_ast_node *group_clause,
    struct mylite_sql_ast_node *having_clause,
    struct mylite_sql_ast_node *order_clause,
    struct mylite_sql_ast_node *limit_clause,
    struct mylite_sql_select_locking_clause locking_clause
) {
    struct mylite_sql_ast_node *statement = mylite_sql_parser_make_select_statement(
        state,
        select_token,
        select_list,
        from_clause,
        where_clause,
        group_clause,
        having_clause,
        order_clause,
        limit_clause
    );

    mylite_sql_ast_node_set_select_modifier(statement, modifiers.duplicate_modifier);
    mylite_sql_ast_node_set_select_options(statement, modifiers.options);
    mylite_sql_ast_node_set_select_calc_found_rows(statement, modifiers.calc_found_rows);
    mylite_sql_ast_node_set_select_locking_clause(statement, locking_clause.kind);
    if (statement != NULL && locking_clause.kind != MYLITE_SQL_AST_SELECT_LOCKING_CLAUSE_NONE) {
        mylite_sql_ast_node_set_span(statement, span_join(statement->span, locking_clause.span));
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_select_distinct_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token select_token,
    struct mylite_sql_ast_node *select_list,
    struct mylite_sql_ast_node *from_clause,
    struct mylite_sql_ast_node *where_clause,
    struct mylite_sql_ast_node *group_clause,
    struct mylite_sql_ast_node *having_clause,
    struct mylite_sql_ast_node *order_clause,
    struct mylite_sql_ast_node *limit_clause
) {
    struct mylite_sql_ast_node *statement = mylite_sql_parser_make_select_statement(
        state,
        select_token,
        select_list,
        from_clause,
        where_clause,
        group_clause,
        having_clause,
        order_clause,
        limit_clause
    );

    mylite_sql_ast_node_set_select_modifier(statement, MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_select_calc_found_rows_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token select_token,
    struct mylite_sql_ast_node *select_list,
    struct mylite_sql_ast_node *from_clause,
    struct mylite_sql_ast_node *where_clause,
    struct mylite_sql_ast_node *order_clause,
    struct mylite_sql_ast_node *limit_clause
) {
    struct mylite_sql_ast_node *statement = mylite_sql_parser_make_select_statement(
        state,
        select_token,
        select_list,
        from_clause,
        where_clause,
        NULL,
        NULL,
        order_clause,
        limit_clause
    );

    mylite_sql_ast_node_set_select_calc_found_rows(statement, 1);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_select_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token select_token,
    struct mylite_sql_ast_node *select_list,
    struct mylite_sql_ast_node *from_clause,
    struct mylite_sql_ast_node *where_clause,
    struct mylite_sql_ast_node *group_clause,
    struct mylite_sql_ast_node *having_clause,
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
    if (group_clause != NULL) {
        span = span_join(span, group_clause->span);
    }
    if (having_clause != NULL) {
        span = span_join(span, having_clause->span);
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
    mylite_sql_ast_node_append_child(statement, group_clause);
    mylite_sql_ast_node_append_child(statement, having_clause);
    mylite_sql_ast_node_append_child(statement, order_clause);
    mylite_sql_ast_node_append_child(statement, limit_clause);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_compound_select_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *first_select,
    struct mylite_sql_ast_node *terms
) {
    struct mylite_sql_source_span span = {0};
    struct mylite_sql_ast_node *statement = NULL;

    if (first_select != NULL) {
        span = first_select->span;
    }
    if (terms != NULL) {
        span = first_select == NULL ? terms->span : span_join(span, terms->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_COMPOUND_SELECT_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, first_select);
    mylite_sql_ast_node_append_child(statement, terms);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_values_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token values_token,
    struct mylite_sql_ast_node *rows,
    struct mylite_sql_ast_node *order_clause,
    struct mylite_sql_ast_node *limit_clause
) {
    struct mylite_sql_source_span span = span_from_token(&values_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (rows != NULL) {
        span = span_join(span, rows->span);
    }
    if (order_clause != NULL) {
        span = span_join(span, order_clause->span);
    }
    if (limit_clause != NULL) {
        span = span_join(span, limit_clause->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_VALUES_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, rows);
    mylite_sql_ast_node_append_child(statement, order_clause);
    mylite_sql_ast_node_append_child(statement, limit_clause);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_union_term_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *term
) {
    struct mylite_sql_source_span span = {0};
    struct mylite_sql_ast_node *list = NULL;

    if (term != NULL) {
        span = term->span;
    }
    list = make_node(state, MYLITE_SQL_AST_UNION_TERM_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, term);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_union_term(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *terms,
    struct mylite_sql_ast_node *term
) {
    (void)state;

    if (terms == NULL || term == NULL) {
        return terms;
    }

    mylite_sql_ast_node_append_child(terms, term);
    mylite_sql_ast_node_set_span(terms, span_join(terms->span, term->span));
    return terms;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_union_term(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token union_token,
    enum mylite_sql_ast_union_modifier modifier,
    struct mylite_sql_ast_node *select_statement
) {
    struct mylite_sql_source_span span = span_from_token(&union_token);
    struct mylite_sql_ast_node *term = NULL;

    if (select_statement != NULL) {
        span = span_join(span, select_statement->span);
    }

    term = make_node(state, MYLITE_SQL_AST_UNION_TERM, span);
    if (term == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_union_modifier(term, modifier);
    mylite_sql_ast_node_append_child(term, select_statement);
    return term;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_do_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token do_token,
    struct mylite_sql_ast_node *expression_list
) {
    struct mylite_sql_source_span span = span_from_token(&do_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (expression_list != NULL) {
        span = span_join(span, expression_list->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_DO_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, expression_list);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_do_expression_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *expression
) {
    struct mylite_sql_source_span span =
        expression == NULL ? (struct mylite_sql_source_span){0} : expression->span;
    struct mylite_sql_ast_node *list = make_node(state, MYLITE_SQL_AST_DO_EXPRESSION_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, expression);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_do_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *expression
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, expression);
    if (expression != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, expression->span));
    }
    return list;
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

struct mylite_sql_ast_node *mylite_sql_parser_make_begin_immediate_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token begin_token,
    struct mylite_sql_token immediate_token
) {
    if (!token_text_equals(&immediate_token, "IMMEDIATE")) {
        mylite_sql_parser_state_syntax_error(state, MYLITE_SQL_PARSE_IDENTIFIER, immediate_token);
        return NULL;
    }

    return mylite_sql_parser_make_transaction_control_statement(
        state,
        MYLITE_SQL_AST_START_TRANSACTION_STATEMENT,
        begin_token,
        immediate_token,
        NULL
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_transaction_control_statement(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_ast_node_kind statement_kind,
    struct mylite_sql_token first_token,
    struct mylite_sql_token last_token,
    struct mylite_sql_ast_node *characteristics
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&first_token), span_from_token(&last_token));
    struct mylite_sql_ast_node *statement = NULL;

    if (characteristics != NULL) {
        span = span_join(span, characteristics->span);
    }

    statement = make_node(state, statement_kind, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, characteristics);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_set_transaction_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token set_token,
    struct mylite_sql_ast_node *scope,
    struct mylite_sql_ast_node *characteristics
) {
    struct mylite_sql_source_span span = span_from_token(&set_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (characteristics != NULL) {
        span = span_join(span, characteristics->span);
    } else if (scope != NULL) {
        span = span_join(span, scope->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SET_TRANSACTION_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, scope);
    mylite_sql_ast_node_append_child(statement, characteristics);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_transaction_characteristic_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *characteristic
) {
    struct mylite_sql_source_span span =
        characteristic == NULL ? (struct mylite_sql_source_span){0} : characteristic->span;
    struct mylite_sql_ast_node *list =
        make_node(state, MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC_LIST, span);

    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, characteristic);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_transaction_characteristic(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *characteristic
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, characteristic);
    if (characteristic != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, characteristic->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_transaction_characteristic(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_ast_node_kind kind,
    struct mylite_sql_token first_token,
    struct mylite_sql_token last_token
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&first_token), span_from_token(&last_token));

    return make_node(state, kind, span);
}

struct mylite_sql_ast_node *mylite_sql_parser_make_savepoint_control_statement(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_ast_node_kind statement_kind,
    struct mylite_sql_token first_token,
    struct mylite_sql_ast_node *savepoint_name
) {
    struct mylite_sql_source_span span = span_from_token(&first_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (savepoint_name != NULL) {
        span = span_join(span, savepoint_name->span);
    }

    statement = make_node(state, statement_kind, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, savepoint_name);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_maintenance_statement(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_ast_node_kind statement_kind,
    struct mylite_sql_token first_token,
    struct mylite_sql_ast_node *table_names
) {
    struct mylite_sql_source_span span = span_from_token(&first_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (table_names != NULL) {
        span = span_join(span, table_names->span);
    }

    statement = make_node(state, statement_kind, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_names);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_lock_tables_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token lock_token,
    struct mylite_sql_ast_node *targets
) {
    struct mylite_sql_source_span span = span_from_token(&lock_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (targets != NULL) {
        span = span_join(span, targets->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_LOCK_TABLES_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, targets);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_unlock_tables_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token unlock_token,
    struct mylite_sql_token table_token
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&unlock_token), span_from_token(&table_token));

    return make_node(state, MYLITE_SQL_AST_UNLOCK_TABLES_STATEMENT, span);
}

struct mylite_sql_ast_node *mylite_sql_parser_make_lock_table_target_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *target
) {
    struct mylite_sql_source_span span =
        target == NULL ? (struct mylite_sql_source_span){0} : target->span;
    struct mylite_sql_ast_node *list =
        make_node(state, MYLITE_SQL_AST_LOCK_TABLE_TARGET_LIST, span);

    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, target);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_lock_table_target(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *target
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, target);
    if (target != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, target->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_lock_table_target(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *alias,
    struct mylite_sql_ast_node *lock_type
) {
    struct mylite_sql_source_span span =
        table_name == NULL ? (struct mylite_sql_source_span){0} : table_name->span;
    struct mylite_sql_ast_node *target = NULL;

    if (lock_type != NULL) {
        span = span_join(span, lock_type->span);
    } else if (alias != NULL) {
        span = span_join(span, alias->span);
    }

    target = make_node(state, MYLITE_SQL_AST_LOCK_TABLE_TARGET, span);
    if (target == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(target, table_name);
    mylite_sql_ast_node_append_child(target, alias);
    mylite_sql_ast_node_append_child(target, lock_type);
    return target;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_lock_table_type(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_ast_node_kind kind,
    struct mylite_sql_token first_token,
    struct mylite_sql_token last_token
) {
    const struct mylite_sql_source_span span =
        span_join(span_from_token(&first_token), span_from_token(&last_token));

    return make_node(state, kind, span);
}

struct mylite_sql_ast_node *mylite_sql_parser_make_set_names_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token set_token,
    struct mylite_sql_ast_node *charset_name,
    struct mylite_sql_ast_node *collation_name
) {
    struct mylite_sql_source_span span = span_from_token(&set_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (collation_name != NULL) {
        span = span_join(span, collation_name->span);
    } else if (charset_name != NULL) {
        span = span_join(span, charset_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SET_NAMES_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, charset_name);
    mylite_sql_ast_node_append_child(statement, collation_name);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_set_character_set_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token set_token,
    struct mylite_sql_ast_node *charset_name
) {
    struct mylite_sql_source_span span = span_from_token(&set_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (charset_name != NULL) {
        span = span_join(span, charset_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, charset_name);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_set_character_set_default_target(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token default_token
) {
    return make_node(
        state,
        MYLITE_SQL_AST_SET_CHARACTER_SET_DEFAULT_TARGET,
        span_from_token(&default_token)
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_set_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token set_token,
    struct mylite_sql_ast_node *assignments
) {
    struct mylite_sql_source_span span = span_from_token(&set_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (assignments != NULL) {
        span = span_join(span, assignments->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SET_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, assignments);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_set_assignment_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *assignment
) {
    struct mylite_sql_source_span span =
        assignment == NULL ? (struct mylite_sql_source_span){0} : assignment->span;
    struct mylite_sql_ast_node *list = make_node(state, MYLITE_SQL_AST_SET_ASSIGNMENT_LIST, span);

    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, assignment);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_set_assignment(
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

struct mylite_sql_ast_node *mylite_sql_parser_make_set_assignment(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *target,
    struct mylite_sql_token operator_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span =
        target == NULL ? span_from_token(&operator_token) : target->span;
    struct mylite_sql_ast_node *assignment = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }

    assignment = make_node(state, MYLITE_SQL_AST_SET_ASSIGNMENT, span);
    if (assignment == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(assignment, target);
    mylite_sql_ast_node_append_child(assignment, value);
    return assignment;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_set_system_variable_target(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *scope,
    struct mylite_sql_ast_node *name
) {
    struct mylite_sql_source_span span =
        scope == NULL ? (struct mylite_sql_source_span){0} : scope->span;
    struct mylite_sql_ast_node *target = NULL;

    if (name != NULL) {
        span = scope == NULL ? name->span : span_join(span, name->span);
    }

    target = make_node(state, MYLITE_SQL_AST_SET_SYSTEM_VARIABLE_TARGET, span);
    if (target == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(target, scope);
    mylite_sql_ast_node_append_child(target, name);
    return target;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_set_default_value(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token default_token
) {
    return make_node(state, MYLITE_SQL_AST_SET_DEFAULT_VALUE, span_from_token(&default_token));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_user_variable(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
) {
    return make_node(state, MYLITE_SQL_AST_USER_VARIABLE, span_from_token(&token));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_prepare_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token prepare_token,
    struct mylite_sql_ast_node *name,
    struct mylite_sql_ast_node *source
) {
    struct mylite_sql_source_span span = span_from_token(&prepare_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (source != NULL) {
        span = span_join(span, source->span);
    } else if (name != NULL) {
        span = span_join(span, name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_PREPARE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, name);
    mylite_sql_ast_node_append_child(statement, source);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_execute_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token execute_token,
    struct mylite_sql_ast_node *name,
    struct mylite_sql_ast_node *using_list
) {
    struct mylite_sql_source_span span = span_from_token(&execute_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (using_list != NULL) {
        span = span_join(span, using_list->span);
    } else if (name != NULL) {
        span = span_join(span, name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_EXECUTE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, name);
    mylite_sql_ast_node_append_child(statement, using_list);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_execute_using_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *variable
) {
    struct mylite_sql_source_span span =
        variable == NULL ? (struct mylite_sql_source_span){0} : variable->span;
    struct mylite_sql_ast_node *list = make_node(state, MYLITE_SQL_AST_EXECUTE_USING_LIST, span);

    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, variable);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_execute_using_variable(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *variable
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, variable);
    if (variable != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, variable->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_deallocate_prepare_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token first_token,
    struct mylite_sql_ast_node *name
) {
    struct mylite_sql_source_span span = span_from_token(&first_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (name != NULL) {
        span = span_join(span, name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_DEALLOCATE_PREPARE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, name);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_create_table_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *if_not_exists_clause,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_token left_paren,
    struct mylite_sql_ast_node *columns,
    struct mylite_sql_token right_paren,
    struct mylite_sql_ast_node *table_options
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&create_token), span_from_token(&right_paren));
    struct mylite_sql_ast_node *statement = NULL;

    if (create_table_name_is_no_space_function_identifier(state, table_name, &left_paren)) {
        mylite_sql_parser_state_syntax_error(state, MYLITE_SQL_PARSE_LPAREN, left_paren);
        return NULL;
    }

    statement = make_node(state, MYLITE_SQL_AST_CREATE_TABLE_STATEMENT, span);
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

struct mylite_sql_ast_node *mylite_sql_parser_make_create_temporary_table_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *if_not_exists_clause,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_token left_paren,
    struct mylite_sql_ast_node *columns,
    struct mylite_sql_token right_paren,
    struct mylite_sql_ast_node *table_options
) {
    struct mylite_sql_ast_node *statement = mylite_sql_parser_make_create_table_statement(
        state,
        create_token,
        if_not_exists_clause,
        table_name,
        left_paren,
        columns,
        right_paren,
        table_options
    );

    if (statement != NULL) {
        statement->kind = MYLITE_SQL_AST_CREATE_TEMPORARY_TABLE_STATEMENT;
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_create_table_like_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *if_not_exists_clause,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *source_table
) {
    struct mylite_sql_source_span span = span_from_token(&create_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (source_table != NULL) {
        span = span_join(span, source_table->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_CREATE_TABLE_LIKE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, source_table);
    if (if_not_exists_clause != NULL) {
        mylite_sql_ast_node_append_child(statement, if_not_exists_clause);
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_create_temporary_table_like_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *if_not_exists_clause,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *source_table
) {
    struct mylite_sql_ast_node *statement = mylite_sql_parser_make_create_table_like_statement(
        state,
        create_token,
        if_not_exists_clause,
        table_name,
        source_table
    );

    if (statement != NULL) {
        statement->kind = MYLITE_SQL_AST_CREATE_TEMPORARY_TABLE_LIKE_STATEMENT;
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_create_table_select_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *if_not_exists_clause,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *select_statement
) {
    struct mylite_sql_source_span span = span_from_token(&create_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (select_statement != NULL) {
        span = span_join(span, select_statement->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_CREATE_TABLE_SELECT_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, select_statement);
    if (if_not_exists_clause != NULL) {
        mylite_sql_ast_node_append_child(statement, if_not_exists_clause);
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_create_temporary_table_select_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *if_not_exists_clause,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *select_statement
) {
    struct mylite_sql_ast_node *statement = mylite_sql_parser_make_create_table_select_statement(
        state,
        create_token,
        if_not_exists_clause,
        table_name,
        select_statement
    );

    if (statement != NULL) {
        statement->kind = MYLITE_SQL_AST_CREATE_TEMPORARY_TABLE_SELECT_STATEMENT;
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_create_view_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *view_name,
    struct mylite_sql_ast_node *select_statement
) {
    struct mylite_sql_source_span span = span_from_token(&create_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (select_statement != NULL) {
        span = span_join(span, select_statement->span);
    } else if (view_name != NULL) {
        span = span_join(span, view_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_CREATE_VIEW_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, view_name);
    mylite_sql_ast_node_append_child(statement, select_statement);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_create_index_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    bool is_unique,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *index_type,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *part_list,
    struct mylite_sql_ast_node *index_options
) {
    struct mylite_sql_source_span span = span_from_token(&create_token);
    struct mylite_sql_ast_node *statement = NULL;
    enum mylite_sql_ast_node_kind statement_kind = MYLITE_SQL_AST_CREATE_INDEX_STATEMENT;

    if (index_options != NULL) {
        span = span_join(span, index_options->span);
    } else if (part_list != NULL) {
        span = span_join(span, part_list->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    } else if (index_type != NULL) {
        span = span_join(span, index_type->span);
    } else if (index_name != NULL) {
        span = span_join(span, index_name->span);
    }

    if (is_unique) {
        statement_kind = MYLITE_SQL_AST_CREATE_UNIQUE_INDEX_STATEMENT;
    }
    statement = make_node(state, statement_kind, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, index_name);
    if (index_type != NULL) {
        mylite_sql_ast_node_append_child(statement, index_type);
    }
    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, part_list);
    if (index_options != NULL) {
        mylite_sql_ast_node_append_child(statement, index_options);
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_create_fulltext_index_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *part_list,
    struct mylite_sql_ast_node *index_options
) {
    struct mylite_sql_source_span span = span_from_token(&create_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (index_options != NULL) {
        span = span_join(span, index_options->span);
    } else if (part_list != NULL) {
        span = span_join(span, part_list->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    } else if (index_name != NULL) {
        span = span_join(span, index_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_CREATE_FULLTEXT_INDEX_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, index_name);
    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, part_list);
    if (index_options != NULL) {
        mylite_sql_ast_node_append_child(statement, index_options);
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_create_spatial_index_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *part_list,
    struct mylite_sql_ast_node *index_options
) {
    struct mylite_sql_source_span span = span_from_token(&create_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (index_options != NULL) {
        span = span_join(span, index_options->span);
    } else if (part_list != NULL) {
        span = span_join(span, part_list->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    } else if (index_name != NULL) {
        span = span_join(span, index_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_CREATE_SPATIAL_INDEX_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, index_name);
    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, part_list);
    if (index_options != NULL) {
        mylite_sql_ast_node_append_child(statement, index_options);
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_drop_index_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token drop_token,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *table_name
) {
    struct mylite_sql_source_span span = span_from_token(&drop_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (table_name != NULL) {
        span = span_join(span, table_name->span);
    } else if (index_name != NULL) {
        span = span_join(span, index_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_DROP_INDEX_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, index_name);
    mylite_sql_ast_node_append_child(statement, table_name);
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

struct mylite_sql_ast_node *mylite_sql_parser_make_table_auto_increment_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token auto_increment_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = span_from_token(&auto_increment_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }

    option = make_node(state, MYLITE_SQL_AST_TABLE_AUTO_INCREMENT_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_comment_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token comment_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = span_from_token(&comment_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }

    option = make_node(state, MYLITE_SQL_AST_TABLE_COMMENT_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_row_format_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token row_format_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = span_from_token(&row_format_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }

    option = make_node(state, MYLITE_SQL_AST_TABLE_ROW_FORMAT_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_key_block_size_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token key_block_size_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = span_from_token(&key_block_size_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }

    option = make_node(state, MYLITE_SQL_AST_TABLE_KEY_BLOCK_SIZE_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_pack_keys_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token pack_keys_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = span_from_token(&pack_keys_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }

    option = make_node(state, MYLITE_SQL_AST_TABLE_PACK_KEYS_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_checksum_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token checksum_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = span_from_token(&checksum_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }

    option = make_node(state, MYLITE_SQL_AST_TABLE_CHECKSUM_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_stats_persistent_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token stats_persistent_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = span_from_token(&stats_persistent_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }

    option = make_node(state, MYLITE_SQL_AST_TABLE_STATS_PERSISTENT_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_stats_auto_recalc_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token stats_auto_recalc_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = span_from_token(&stats_auto_recalc_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }

    option = make_node(state, MYLITE_SQL_AST_TABLE_STATS_AUTO_RECALC_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_stats_sample_pages_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token stats_sample_pages_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = span_from_token(&stats_sample_pages_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }

    option = make_node(state, MYLITE_SQL_AST_TABLE_STATS_SAMPLE_PAGES_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_index_option_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *option
) {
    struct mylite_sql_source_span span =
        option == NULL ? (struct mylite_sql_source_span){0} : option->span;
    struct mylite_sql_ast_node *list = make_node(state, MYLITE_SQL_AST_INDEX_OPTION_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, option);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_index_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *option
) {
    (void)state;
    mylite_sql_ast_node_append_child(list, option);
    if (option != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, option->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_index_type_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token using_token,
    struct mylite_sql_ast_node *type_name
) {
    struct mylite_sql_source_span span = span_from_token(&using_token);
    struct mylite_sql_ast_node *option = NULL;

    if (type_name != NULL) {
        span = span_join(span, type_name->span);
    }
    option = make_node(state, MYLITE_SQL_AST_INDEX_TYPE_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, type_name);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_index_comment_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token comment_token,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span = span_from_token(&comment_token);
    struct mylite_sql_ast_node *option = NULL;

    if (value != NULL) {
        span = span_join(span, value->span);
    }
    option = make_node(state, MYLITE_SQL_AST_INDEX_COMMENT_OPTION, span);
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(option, value);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_index_visibility_option(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token visibility_token,
    enum mylite_sql_ast_column_visibility visibility
) {
    struct mylite_sql_ast_node *option = make_node(
        state,
        MYLITE_SQL_AST_INDEX_VISIBILITY_OPTION,
        span_from_token(&visibility_token)
    );
    if (option == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_column_visibility(option, visibility);
    return option;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_create_schema_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token create_token,
    struct mylite_sql_ast_node *if_not_exists_clause,
    struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *schema_options
) {
    struct mylite_sql_source_span span = span_from_token(&create_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (if_not_exists_clause != NULL) {
        span = span_join(span, if_not_exists_clause->span);
    }
    if (schema_name != NULL) {
        span = span_join(span, schema_name->span);
    }
    if (schema_options != NULL) {
        span = span_join(span, schema_options->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, schema_name);
    if (if_not_exists_clause != NULL) {
        mylite_sql_ast_node_append_child(statement, if_not_exists_clause);
    }
    if (schema_options != NULL) {
        mylite_sql_ast_node_append_child(statement, schema_options);
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_schema_default_charset_collation_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *schema_options
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (schema_options != NULL) {
        span = span_join(span, schema_options->span);
    } else if (schema_name != NULL) {
        span = span_join(span, schema_name->span);
    }

    statement =
        make_node(state, MYLITE_SQL_AST_ALTER_SCHEMA_DEFAULT_CHARSET_COLLATION_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    if (schema_name != NULL) {
        mylite_sql_ast_node_append_child(statement, schema_name);
    }
    mylite_sql_ast_node_append_child(statement, schema_options);
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

struct mylite_sql_ast_node *mylite_sql_parser_make_drop_temporary_table_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token drop_token,
    struct mylite_sql_ast_node *if_exists_clause,
    struct mylite_sql_ast_node *table_names
) {
    struct mylite_sql_ast_node *statement = mylite_sql_parser_make_drop_table_statement(
        state,
        drop_token,
        if_exists_clause,
        table_names
    );

    if (statement != NULL) {
        statement->kind = MYLITE_SQL_AST_DROP_TEMPORARY_TABLE_STATEMENT;
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_drop_view_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token drop_token,
    struct mylite_sql_ast_node *if_exists_clause,
    struct mylite_sql_ast_node *view_names
) {
    struct mylite_sql_ast_node *statement = mylite_sql_parser_make_drop_table_statement(
        state,
        drop_token,
        if_exists_clause,
        view_names
    );

    if (statement != NULL) {
        statement->kind = MYLITE_SQL_AST_DROP_VIEW_STATEMENT;
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
    int is_full,
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

    mylite_sql_ast_node_set_show_tables_full(statement, is_full);
    mylite_sql_ast_node_append_child(statement, schema_name);
    mylite_sql_ast_node_append_child(statement, like_pattern);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_variables_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_ast_node *scope,
    struct mylite_sql_token variables_token,
    struct mylite_sql_ast_node *filter
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&show_token), span_from_token(&variables_token));
    struct mylite_sql_ast_node *statement = NULL;

    if (filter != NULL) {
        span = span_join(span, filter->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SHOW_VARIABLES_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, scope);
    mylite_sql_ast_node_append_child(statement, filter);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_status_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_ast_node *scope,
    struct mylite_sql_token status_token,
    struct mylite_sql_ast_node *filter
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&show_token), span_from_token(&status_token));
    struct mylite_sql_ast_node *statement = NULL;

    if (filter != NULL) {
        span = span_join(span, filter->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SHOW_STATUS_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, scope);
    mylite_sql_ast_node_append_child(statement, filter);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_table_status_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token status_token,
    struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *filter
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&show_token), span_from_token(&status_token));
    struct mylite_sql_ast_node *statement = NULL;

    if (filter != NULL) {
        span = span_join(span, filter->span);
    } else if (schema_name != NULL) {
        span = span_join(span, schema_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_SHOW_TABLE_STATUS_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, schema_name);
    mylite_sql_ast_node_append_child(statement, filter);
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

struct mylite_sql_ast_node *mylite_sql_parser_make_show_grants_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token end_token
) {
    return make_node(
        state,
        MYLITE_SQL_AST_SHOW_GRANTS_STATEMENT,
        span_join(span_from_token(&show_token), span_from_token(&end_token))
    );
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

struct mylite_sql_ast_node *mylite_sql_parser_make_show_full_columns_statement(
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

    statement = make_node(state, MYLITE_SQL_AST_SHOW_FULL_COLUMNS_STATEMENT, span);
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
    struct mylite_sql_ast_node *schema_name,
    struct mylite_sql_ast_node *where_clause
) {
    struct mylite_sql_source_span span = span_from_token(&show_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (where_clause != NULL) {
        span = span_join(span, where_clause->span);
    } else if (schema_name != NULL) {
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
    mylite_sql_ast_node_append_child(statement, where_clause);
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

struct mylite_sql_ast_node *mylite_sql_parser_make_show_create_view_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_ast_node *view_name
) {
    struct mylite_sql_ast_node *statement =
        mylite_sql_parser_make_show_create_table_statement(state, show_token, view_name);

    if (statement != NULL) {
        statement->kind = MYLITE_SQL_AST_SHOW_CREATE_VIEW_STATEMENT;
    }
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

struct mylite_sql_ast_node *mylite_sql_parser_make_show_engine_status_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_ast_node *engine_name,
    struct mylite_sql_token status_token
) {
    struct mylite_sql_ast_node *statement = make_node(
        state,
        MYLITE_SQL_AST_SHOW_ENGINE_STATUS_STATEMENT,
        span_join(span_from_token(&show_token), span_from_token(&status_token))
    );

    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, engine_name);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_plugins_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token plugins_token
) {
    return make_node(
        state,
        MYLITE_SQL_AST_SHOW_PLUGINS_STATEMENT,
        span_join(span_from_token(&show_token), span_from_token(&plugins_token))
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_privileges_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token privileges_token
) {
    return make_node(
        state,
        MYLITE_SQL_AST_SHOW_PRIVILEGES_STATEMENT,
        span_join(span_from_token(&show_token), span_from_token(&privileges_token))
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_binary_log_status_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token status_token
) {
    return make_node(
        state,
        MYLITE_SQL_AST_SHOW_BINARY_LOG_STATUS_STATEMENT,
        span_join(span_from_token(&show_token), span_from_token(&status_token))
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_binary_logs_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token logs_token
) {
    return make_node(
        state,
        MYLITE_SQL_AST_SHOW_BINARY_LOGS_STATEMENT,
        span_join(span_from_token(&show_token), span_from_token(&logs_token))
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_replica_status_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token status_token
) {
    return make_node(
        state,
        MYLITE_SQL_AST_SHOW_REPLICA_STATUS_STATEMENT,
        span_join(span_from_token(&show_token), span_from_token(&status_token))
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_show_replicas_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token show_token,
    struct mylite_sql_token replicas_token
) {
    return make_node(
        state,
        MYLITE_SQL_AST_SHOW_REPLICAS_STATEMENT,
        span_join(span_from_token(&show_token), span_from_token(&replicas_token))
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
    struct mylite_sql_ast_node *column,
    struct mylite_sql_ast_node *position,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (position != NULL) {
        span = span_join(span, position->span);
    } else if (column != NULL) {
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
    if (position != NULL) {
        mylite_sql_ast_node_append_child(statement, position);
    }
    apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_action_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *action
) {
    struct mylite_sql_source_span span =
        action == NULL ? (struct mylite_sql_source_span){0} : action->span;
    struct mylite_sql_ast_node *list =
        make_node(state, MYLITE_SQL_AST_ALTER_TABLE_ACTION_LIST, span);

    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, action);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_alter_table_action(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *action
) {
    if (list == NULL) {
        return mylite_sql_parser_make_alter_table_action_list(state, action);
    }
    if (action != NULL) {
        mylite_sql_ast_node_append_child(list, action);
        mylite_sql_ast_node_set_span(list, span_join(list->span, action->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_multi_action_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *actions
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (actions != NULL) {
        span = span_join(span, actions->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_MULTI_ACTION_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, actions);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_add_primary_key_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *primary_key,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (primary_key != NULL) {
        span = span_join(span, primary_key->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_ADD_PRIMARY_KEY_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, primary_key);
    apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_add_index_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *secondary_index,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (secondary_index != NULL) {
        span = span_join(span, secondary_index->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_ADD_INDEX_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, secondary_index);
    apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_add_foreign_key_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *foreign_key,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (foreign_key != NULL) {
        span = span_join(span, foreign_key->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_ADD_FOREIGN_KEY_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, foreign_key);
    apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_drop_foreign_key_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *foreign_key_name,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (foreign_key_name != NULL) {
        span = span_join(span, foreign_key_name->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_DROP_FOREIGN_KEY_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, foreign_key_name);
    apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_drop_constraint_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *constraint_name,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (constraint_name != NULL) {
        span = span_join(span, constraint_name->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_DROP_CONSTRAINT_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, constraint_name);
    apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_drop_index_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (index_name != NULL) {
        span = span_join(span, index_name->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_DROP_INDEX_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, index_name);
    apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_rename_index_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *old_index_name,
    struct mylite_sql_ast_node *new_index_name,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (new_index_name != NULL) {
        span = span_join(span, new_index_name->span);
    } else if (old_index_name != NULL) {
        span = span_join(span, old_index_name->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_RENAME_INDEX_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, old_index_name);
    mylite_sql_ast_node_append_child(statement, new_index_name);
    apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_index_visibility_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_token visibility_token,
    enum mylite_sql_ast_column_visibility visibility,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    span = span_join(span, span_from_token(&visibility_token));

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_INDEX_VISIBILITY_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, index_name);
    apply_alter_table_options(statement, options);
    mylite_sql_ast_node_set_column_visibility(statement, visibility);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_add_check_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *check_constraint
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (check_constraint != NULL) {
        span = span_join(span, check_constraint->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_ADD_CHECK_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, check_constraint);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_drop_check_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *check_name
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (check_name != NULL) {
        span = span_join(span, check_name->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_DROP_CHECK_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, check_name);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_alter_check_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *check_name,
    struct mylite_sql_ast_node *enforcement
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (enforcement != NULL) {
        span = span_join(span, enforcement->span);
    } else if (check_name != NULL) {
        span = span_join(span, check_name->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_ALTER_CHECK_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, check_name);
    mylite_sql_ast_node_append_child(statement, enforcement);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_drop_primary_key_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_token key_token,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (key_token.text != NULL) {
        span = span_join(span, span_from_token(&key_token));
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_DROP_PRIMARY_KEY_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_auto_increment_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *auto_increment_option
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (auto_increment_option != NULL) {
        span = span_join(span, auto_increment_option->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_AUTO_INCREMENT_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, auto_increment_option);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_drop_column_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *column_name,
    struct mylite_sql_alter_table_options options
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
    apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_rename_column_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *old_column_name,
    struct mylite_sql_ast_node *new_column_name,
    struct mylite_sql_alter_table_options options
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
    apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_modify_column_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *column,
    struct mylite_sql_ast_node *position
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (position != NULL) {
        span = span_join(span, position->span);
    } else if (column != NULL) {
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
    if (position != NULL) {
        mylite_sql_ast_node_append_child(statement, position);
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_change_column_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *old_column_name,
    struct mylite_sql_ast_node *column,
    struct mylite_sql_ast_node *position
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (position != NULL) {
        span = span_join(span, position->span);
    } else if (column != NULL) {
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
    if (position != NULL) {
        mylite_sql_ast_node_append_child(statement, position);
    }
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_position_first(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token first_token
) {
    return make_node(state, MYLITE_SQL_AST_COLUMN_POSITION_FIRST, span_from_token(&first_token));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_position_after(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token after_token,
    struct mylite_sql_ast_node *column_name
) {
    struct mylite_sql_source_span span = span_from_token(&after_token);
    struct mylite_sql_ast_node *position = NULL;

    if (column_name != NULL) {
        span = span_join(span, column_name->span);
    }

    position = make_node(state, MYLITE_SQL_AST_COLUMN_POSITION_AFTER, span);
    if (position == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(position, column_name);
    return position;
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

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_drop_default_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *column_name,
    struct mylite_sql_token default_token
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    span = span_join(span, span_from_token(&default_token));

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_DROP_DEFAULT_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, column_name);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_column_visibility_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *column_name,
    struct mylite_sql_token visibility_token,
    enum mylite_sql_ast_column_visibility visibility
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    span = span_join(span, span_from_token(&visibility_token));

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_COLUMN_VISIBILITY_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_column_visibility(statement, visibility);
    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, column_name);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_default_charset_collation_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *table_options
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (table_options != NULL) {
        span = span_join(span, table_options->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement =
        make_node(state, MYLITE_SQL_AST_ALTER_TABLE_DEFAULT_CHARSET_COLLATION_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, table_options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_convert_character_set_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *table_options
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (table_options != NULL) {
        span = span_join(span, table_options->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_CONVERT_CHARACTER_SET_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, table_options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_comment_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *comment_option,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (comment_option != NULL) {
        span = span_join(span, comment_option->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_COMMENT_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, comment_option);
    apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_order_by_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *order_items
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (order_items != NULL) {
        span = span_join(span, order_items->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_ORDER_BY_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, order_items);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_force_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_FORCE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_disable_keys_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_DISABLE_KEYS_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_alter_table_enable_keys_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token alter_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_alter_table_options options
) {
    struct mylite_sql_source_span span = span_from_token(&alter_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_ALTER_TABLE_ENABLE_KEYS_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    apply_alter_table_options(statement, options);
    return statement;
}

struct mylite_sql_alter_table_options mylite_sql_parser_empty_alter_table_options(void) {
    return (struct mylite_sql_alter_table_options){
        .algorithm = MYLITE_SQL_AST_ALTER_ALGORITHM_UNSPECIFIED,
        .lock = MYLITE_SQL_AST_ALTER_LOCK_UNSPECIFIED,
        .span = {0},
        .has_span = 0,
    };
}

struct mylite_sql_alter_algorithm_value mylite_sql_parser_make_alter_algorithm_value(
    struct mylite_sql_token token
) {
    enum mylite_sql_ast_alter_algorithm kind = MYLITE_SQL_AST_ALTER_ALGORITHM_UNKNOWN;

    if (token_text_equals(&token, "DEFAULT")) {
        kind = MYLITE_SQL_AST_ALTER_ALGORITHM_DEFAULT;
    } else if (token_text_equals(&token, "INSTANT")) {
        kind = MYLITE_SQL_AST_ALTER_ALGORITHM_INSTANT;
    } else if (token_text_equals(&token, "INPLACE")) {
        kind = MYLITE_SQL_AST_ALTER_ALGORITHM_INPLACE;
    } else if (token_text_equals(&token, "COPY")) {
        kind = MYLITE_SQL_AST_ALTER_ALGORITHM_COPY;
    }

    return (struct mylite_sql_alter_algorithm_value){
        .kind = kind,
        .span = span_from_token(&token),
    };
}

struct mylite_sql_alter_lock_value mylite_sql_parser_make_alter_lock_value(
    struct mylite_sql_token token
) {
    enum mylite_sql_ast_alter_lock kind = MYLITE_SQL_AST_ALTER_LOCK_UNKNOWN;

    if (token_text_equals(&token, "DEFAULT")) {
        kind = MYLITE_SQL_AST_ALTER_LOCK_DEFAULT;
    } else if (token_text_equals(&token, "NONE")) {
        kind = MYLITE_SQL_AST_ALTER_LOCK_NONE;
    } else if (token_text_equals(&token, "SHARED")) {
        kind = MYLITE_SQL_AST_ALTER_LOCK_SHARED;
    } else if (token_text_equals(&token, "EXCLUSIVE")) {
        kind = MYLITE_SQL_AST_ALTER_LOCK_EXCLUSIVE;
    }

    return (struct mylite_sql_alter_lock_value){
        .kind = kind,
        .span = span_from_token(&token),
    };
}

struct mylite_sql_alter_table_options mylite_sql_parser_make_alter_table_algorithm_option(
    struct mylite_sql_token option_token,
    struct mylite_sql_alter_algorithm_value value
) {
    struct mylite_sql_alter_table_options options = mylite_sql_parser_empty_alter_table_options();

    options.algorithm = value.kind;
    options.span = span_join(span_from_token(&option_token), value.span);
    options.has_span = 1;
    return options;
}

struct mylite_sql_alter_table_options mylite_sql_parser_make_alter_table_lock_option(
    struct mylite_sql_token option_token,
    struct mylite_sql_alter_lock_value value
) {
    struct mylite_sql_alter_table_options options = mylite_sql_parser_empty_alter_table_options();

    options.lock = value.kind;
    options.span = span_join(span_from_token(&option_token), value.span);
    options.has_span = 1;
    return options;
}

struct mylite_sql_alter_table_options mylite_sql_parser_append_alter_table_option(
    struct mylite_sql_alter_table_options list,
    struct mylite_sql_alter_table_options option
) {
    if (option.algorithm != MYLITE_SQL_AST_ALTER_ALGORITHM_UNSPECIFIED) {
        if (list.algorithm == MYLITE_SQL_AST_ALTER_ALGORITHM_UNKNOWN ||
            option.algorithm == MYLITE_SQL_AST_ALTER_ALGORITHM_UNKNOWN) {
            list.algorithm = MYLITE_SQL_AST_ALTER_ALGORITHM_UNKNOWN;
        } else {
            list.algorithm = option.algorithm;
        }
    }
    if (option.lock != MYLITE_SQL_AST_ALTER_LOCK_UNSPECIFIED) {
        if (list.lock == MYLITE_SQL_AST_ALTER_LOCK_UNKNOWN ||
            option.lock == MYLITE_SQL_AST_ALTER_LOCK_UNKNOWN) {
            list.lock = MYLITE_SQL_AST_ALTER_LOCK_UNKNOWN;
        } else {
            list.lock = option.lock;
        }
    }
    if (!list.has_span) {
        list.span = option.span;
        list.has_span = option.has_span;
    } else if (option.has_span) {
        list.span = span_join(list.span, option.span);
    }
    return list;
}

static void apply_alter_table_options(
    struct mylite_sql_ast_node *statement,
    struct mylite_sql_alter_table_options options
) {
    if (statement == NULL) {
        return;
    }

    mylite_sql_ast_node_set_alter_table_options(statement, options.algorithm, options.lock);
    if (options.has_span) {
        mylite_sql_ast_node_set_span(statement, span_join(statement->span, options.span));
    }
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token insert_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *columns,
    struct mylite_sql_ast_node *rows,
    struct mylite_sql_ast_node *modifier,
    struct mylite_sql_ast_node *ignore,
    struct mylite_sql_ast_node *duplicate_update
) {
    struct mylite_sql_source_span span = span_from_token(&insert_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (duplicate_update != NULL) {
        span = span_join(span, duplicate_update->span);
    } else if (rows != NULL) {
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
    mylite_sql_ast_node_append_child(statement, modifier);
    mylite_sql_ast_node_append_child(statement, ignore);
    mylite_sql_ast_node_append_child(statement, duplicate_update);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_select_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token insert_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *columns,
    struct mylite_sql_ast_node *select,
    struct mylite_sql_ast_node *modifier,
    struct mylite_sql_ast_node *ignore,
    struct mylite_sql_ast_node *duplicate_update
) {
    struct mylite_sql_source_span span = span_from_token(&insert_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (duplicate_update != NULL) {
        span = span_join(span, duplicate_update->span);
    } else if (select != NULL) {
        span = span_join(span, select->span);
    } else if (columns != NULL) {
        span = span_join(span, columns->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_INSERT_SELECT_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, columns);
    mylite_sql_ast_node_append_child(statement, select);
    mylite_sql_ast_node_append_child(statement, modifier);
    mylite_sql_ast_node_append_child(statement, ignore);
    mylite_sql_ast_node_append_child(statement, duplicate_update);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_load_data_infile_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token load_token,
    struct mylite_sql_ast_node *file_name,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *ignore_lines,
    struct mylite_sql_ast_node *columns,
    struct mylite_sql_ast_node *local_modifier
) {
    struct mylite_sql_source_span span = span_from_token(&load_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (columns != NULL) {
        span = span_join(span, columns->span);
    } else if (ignore_lines != NULL) {
        span = span_join(span, ignore_lines->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    } else if (file_name != NULL) {
        span = span_join(span, file_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_LOAD_DATA_INFILE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, file_name);
    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, ignore_lines);
    mylite_sql_ast_node_append_child(statement, columns);
    mylite_sql_ast_node_append_child(statement, local_modifier);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_load_data_local_modifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
) {
    return make_node(state, MYLITE_SQL_AST_LOAD_DATA_LOCAL_MODIFIER, span_from_token(&token));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_low_priority_modifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
) {
    return make_node(state, MYLITE_SQL_AST_INSERT_LOW_PRIORITY_MODIFIER, span_from_token(&token));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_high_priority_modifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
) {
    return make_node(state, MYLITE_SQL_AST_INSERT_HIGH_PRIORITY_MODIFIER, span_from_token(&token));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_delayed_modifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
) {
    return make_node(state, MYLITE_SQL_AST_INSERT_DELAYED_MODIFIER, span_from_token(&token));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_ignore_modifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
) {
    return make_node(state, MYLITE_SQL_AST_INSERT_IGNORE_MODIFIER, span_from_token(&token));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_replace_select_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token replace_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *columns,
    struct mylite_sql_ast_node *select,
    struct mylite_sql_ast_node *modifier
) {
    struct mylite_sql_source_span span = span_from_token(&replace_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (select != NULL) {
        span = span_join(span, select->span);
    } else if (columns != NULL) {
        span = span_join(span, columns->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_REPLACE_SELECT_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, columns);
    mylite_sql_ast_node_append_child(statement, select);
    mylite_sql_ast_node_append_child(statement, modifier);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_replace_values_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token replace_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *columns,
    struct mylite_sql_ast_node *rows,
    struct mylite_sql_ast_node *modifier
) {
    struct mylite_sql_source_span span = span_from_token(&replace_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (rows != NULL) {
        span = span_join(span, rows->span);
    } else if (columns != NULL) {
        span = span_join(span, columns->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_REPLACE_VALUES_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, columns);
    mylite_sql_ast_node_append_child(statement, rows);
    mylite_sql_ast_node_append_child(statement, modifier);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_replace_low_priority_modifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
) {
    return make_node(state, MYLITE_SQL_AST_REPLACE_LOW_PRIORITY_MODIFIER, span_from_token(&token));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_replace_delayed_modifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
) {
    return make_node(state, MYLITE_SQL_AST_REPLACE_DELAYED_MODIFIER, span_from_token(&token));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_set_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token insert_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *assignments,
    struct mylite_sql_ast_node *modifier,
    struct mylite_sql_ast_node *ignore,
    struct mylite_sql_ast_node *duplicate_update
) {
    struct mylite_sql_source_span span = span_from_token(&insert_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (duplicate_update != NULL) {
        span = span_join(span, duplicate_update->span);
    } else if (assignments != NULL) {
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
    mylite_sql_ast_node_append_child(statement, modifier);
    mylite_sql_ast_node_append_child(statement, ignore);
    mylite_sql_ast_node_append_child(statement, duplicate_update);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_replace_set_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token replace_token,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *assignments,
    struct mylite_sql_ast_node *modifier
) {
    struct mylite_sql_source_span span = span_from_token(&replace_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (assignments != NULL) {
        span = span_join(span, assignments->span);
    } else if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_REPLACE_SET_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, table_name);
    mylite_sql_ast_node_append_child(statement, assignments);
    mylite_sql_ast_node_append_child(statement, modifier);
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

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_duplicate_update_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token on_token,
    struct mylite_sql_ast_node *assignments
) {
    struct mylite_sql_source_span span = span_from_token(&on_token);
    struct mylite_sql_ast_node *clause = NULL;

    if (assignments != NULL) {
        span = span_join(span, assignments->span);
    }

    clause = make_node(state, MYLITE_SQL_AST_INSERT_DUPLICATE_UPDATE_CLAUSE, span);
    if (clause == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(clause, assignments);
    return clause;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_duplicate_assignment_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *assignment
) {
    struct mylite_sql_source_span span =
        assignment == NULL ? (struct mylite_sql_source_span){0} : assignment->span;
    struct mylite_sql_ast_node *list =
        make_node(state, MYLITE_SQL_AST_INSERT_DUPLICATE_ASSIGNMENT_LIST, span);

    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, assignment);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_insert_duplicate_assignment(
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

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_duplicate_assignment(
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

    assignment = make_node(state, MYLITE_SQL_AST_INSERT_DUPLICATE_ASSIGNMENT, span);
    if (assignment == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(assignment, target);
    mylite_sql_ast_node_append_child(assignment, value);
    return assignment;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_insert_values_reference(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token values_token,
    struct mylite_sql_ast_node *column,
    struct mylite_sql_token close_token
) {
    struct mylite_sql_source_span span = span_from_token(&values_token);
    struct mylite_sql_ast_node *reference = NULL;

    if (column != NULL) {
        span = span_join(span, column->span);
    }
    span = span_join(span, span_from_token(&close_token));

    reference = make_node(state, MYLITE_SQL_AST_INSERT_VALUES_REFERENCE, span);
    if (reference == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(reference, column);
    return reference;
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

struct mylite_sql_ast_node *mylite_sql_parser_make_joined_delete_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token delete_token,
    struct mylite_sql_ast_node *target,
    struct mylite_sql_ast_node *from_join,
    struct mylite_sql_ast_node *where_clause
) {
    struct mylite_sql_source_span span = span_from_token(&delete_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (target != NULL) {
        span = span_join(span, target->span);
    }
    if (from_join != NULL) {
        span = span_join(span, from_join->span);
    }
    if (where_clause != NULL) {
        span = span_join(span, where_clause->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_JOINED_DELETE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, target);
    mylite_sql_ast_node_append_child(statement, from_join);
    mylite_sql_ast_node_append_child(statement, where_clause);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_update_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token update_token,
    struct mylite_sql_update_statement_parts parts
) {
    struct mylite_sql_source_span span = span_from_token(&update_token);
    struct mylite_sql_ast_node *target = parts.target_table;
    struct mylite_sql_ast_node *statement = NULL;

    if (target != NULL && target->kind == MYLITE_SQL_AST_FROM_TABLE &&
        target->first_child != NULL && target->first_child->next_sibling == NULL) {
        target = target->first_child;
    }

    if (target != NULL) {
        span = span_join(span, target->span);
    }
    if (parts.assignment_list != NULL) {
        span = span_join(span, parts.assignment_list->span);
    }
    if (parts.where_clause != NULL) {
        span = span_join(span, parts.where_clause->span);
    }
    if (parts.order_clause != NULL) {
        span = span_join(span, parts.order_clause->span);
    }
    if (parts.limit_clause != NULL) {
        span = span_join(span, parts.limit_clause->span);
    }

    statement = make_node(state, MYLITE_SQL_AST_UPDATE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, target);
    mylite_sql_ast_node_append_child(statement, parts.assignment_list);
    mylite_sql_ast_node_append_child(statement, parts.where_clause);
    mylite_sql_ast_node_append_child(statement, parts.order_clause);
    mylite_sql_ast_node_append_child(statement, parts.limit_clause);
    return statement;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_joined_update_statement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token update_token,
    struct mylite_sql_ast_node *from_join,
    struct mylite_sql_ast_node *assignments,
    struct mylite_sql_ast_node *where_clause,
    struct mylite_sql_ast_node *order_clause,
    struct mylite_sql_ast_node *limit_clause
) {
    struct mylite_sql_source_span span = span_from_token(&update_token);
    struct mylite_sql_ast_node *statement = NULL;

    if (from_join != NULL) {
        span = span_join(span, from_join->span);
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

    statement = make_node(state, MYLITE_SQL_AST_JOINED_UPDATE_STATEMENT, span);
    if (statement == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(statement, from_join);
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
            mylite_sql_parser_make_wildcard(state, wildcard_token),
            NULL
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
    struct mylite_sql_ast_node *expression,
    struct mylite_sql_ast_node *alias
) {
    struct mylite_sql_source_span span =
        expression == NULL ? (struct mylite_sql_source_span){0} : expression->span;
    struct mylite_sql_ast_node *item = make_node(state, MYLITE_SQL_AST_SELECT_ITEM, span);
    if (item == NULL) {
        return NULL;
    }

    if (alias != NULL) {
        mylite_sql_ast_node_set_span(item, span_join(span, alias->span));
    }
    mylite_sql_ast_node_append_child(item, expression);
    mylite_sql_ast_node_append_child(item, alias);
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
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *alias,
    struct mylite_sql_ast_node *index_hints
) {
    struct mylite_sql_source_span span = span_from_token(&from_token);
    struct mylite_sql_ast_node *from_table = NULL;

    if (table_name != NULL) {
        span = span_join(span, table_name->span);
    }
    if (alias != NULL) {
        span = span_join(span, alias->span);
    }
    if (index_hints != NULL) {
        span = span_join(span, index_hints->span);
    }

    from_table = make_node(state, MYLITE_SQL_AST_FROM_TABLE, span);
    if (from_table == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(from_table, table_name);
    if (alias != NULL) {
        mylite_sql_ast_node_append_child(from_table, alias);
    }
    if (index_hints != NULL) {
        mylite_sql_ast_node_append_child(from_table, index_hints);
    }
    return from_table;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_table_source(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *table_name,
    struct mylite_sql_ast_node *alias,
    struct mylite_sql_ast_node *index_hints
) {
    struct mylite_sql_source_span span =
        table_name != NULL ? table_name->span : (struct mylite_sql_source_span){0};
    struct mylite_sql_ast_node *from_table = NULL;

    if (alias != NULL) {
        span = span_join(span, alias->span);
    }
    if (index_hints != NULL) {
        span = span_join(span, index_hints->span);
    }

    from_table = make_node(state, MYLITE_SQL_AST_FROM_TABLE, span);
    if (from_table == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(from_table, table_name);
    if (alias != NULL) {
        mylite_sql_ast_node_append_child(from_table, alias);
    }
    if (index_hints != NULL) {
        mylite_sql_ast_node_append_child(from_table, index_hints);
    }
    return from_table;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_from_join(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token from_token,
    struct mylite_sql_ast_node *left,
    enum mylite_sql_ast_join_kind join_kind,
    struct mylite_sql_ast_node *right,
    struct mylite_sql_ast_node *condition
) {
    struct mylite_sql_source_span span = span_from_token(&from_token);
    struct mylite_sql_ast_node *join = NULL;

    if (left != NULL) {
        span = span_join(span, left->span);
    }
    if (right != NULL) {
        span = span_join(span, right->span);
    }
    if (condition != NULL) {
        span = span_join(span, condition->span);
    }

    join = make_node(state, MYLITE_SQL_AST_FROM_JOIN, span);
    if (join == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_join_kind(join, join_kind);
    mylite_sql_ast_node_append_child(join, left);
    mylite_sql_ast_node_append_child(join, right);
    mylite_sql_ast_node_append_child(join, condition);
    return join;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_join_source(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *left,
    enum mylite_sql_ast_join_kind join_kind,
    struct mylite_sql_ast_node *right,
    struct mylite_sql_ast_node *condition
) {
    struct mylite_sql_source_span span =
        left != NULL ? left->span : (struct mylite_sql_source_span){0};
    struct mylite_sql_ast_node *join = NULL;

    if (right != NULL) {
        span = span_join(span, right->span);
    }
    if (condition != NULL) {
        span = span_join(span, condition->span);
    }

    join = make_node(state, MYLITE_SQL_AST_FROM_JOIN, span);
    if (join == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_join_kind(join, join_kind);
    mylite_sql_ast_node_append_child(join, left);
    mylite_sql_ast_node_append_child(join, right);
    mylite_sql_ast_node_append_child(join, condition);
    return join;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_index_hint_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *hint
) {
    struct mylite_sql_source_span span =
        hint != NULL ? hint->span : (struct mylite_sql_source_span){0};
    struct mylite_sql_ast_node *list = make_node(state, MYLITE_SQL_AST_INDEX_HINT_LIST, span);

    if (list == NULL) {
        return NULL;
    }
    mylite_sql_ast_node_append_child(list, hint);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_index_hint(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *hint
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, hint);
    if (hint != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, hint->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_index_hint(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_ast_node_kind kind,
    struct mylite_sql_token start_token,
    struct mylite_sql_ast_node *scope,
    struct mylite_sql_ast_node *names,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_ast_node *hint = make_node(
        state,
        kind,
        span_join(span_from_token(&start_token), span_from_token(&right_paren))
    );

    if (hint == NULL) {
        return NULL;
    }
    mylite_sql_ast_node_append_child(hint, scope);
    mylite_sql_ast_node_append_child(hint, names);
    return hint;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_index_hint_scope(
    struct mylite_sql_parser_state *state,
    enum mylite_sql_ast_node_kind kind,
    struct mylite_sql_token for_token,
    struct mylite_sql_token last_token
) {
    return make_node(
        state,
        kind,
        span_join(span_from_token(&for_token), span_from_token(&last_token))
    );
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

struct mylite_sql_ast_node *mylite_sql_parser_make_group_by_key_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *group_key
) {
    struct mylite_sql_source_span span =
        group_key == NULL ? (struct mylite_sql_source_span){0} : group_key->span;
    struct mylite_sql_ast_node *list = make_node(state, MYLITE_SQL_AST_GROUP_BY_ITEM_LIST, span);

    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, group_key);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_group_by_key(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *group_key
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, group_key);
    if (group_key != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, group_key->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_group_by_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token group_token,
    struct mylite_sql_ast_node *group_keys
) {
    struct mylite_sql_source_span span = span_from_token(&group_token);
    struct mylite_sql_ast_node *group_clause = NULL;

    if (group_keys != NULL) {
        span = span_join(span, group_keys->span);
    }

    group_clause = make_node(state, MYLITE_SQL_AST_GROUP_BY_CLAUSE, span);
    if (group_clause == NULL) {
        return NULL;
    }

    if (group_keys != NULL && group_keys->kind == MYLITE_SQL_AST_GROUP_BY_ITEM_LIST) {
        struct mylite_sql_ast_node *group_key = group_keys->first_child;

        while (group_key != NULL) {
            struct mylite_sql_ast_node *next = group_key->next_sibling;
            mylite_sql_ast_node_append_child(group_clause, group_key);
            group_key = next;
        }
    } else {
        mylite_sql_ast_node_append_child(group_clause, group_keys);
    }

    return group_clause;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_having_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token having_token,
    struct mylite_sql_ast_node *predicate
) {
    struct mylite_sql_source_span span = span_from_token(&having_token);
    struct mylite_sql_ast_node *having_clause = NULL;

    if (predicate != NULL) {
        span = span_join(span, predicate->span);
    }

    having_clause = make_node(state, MYLITE_SQL_AST_HAVING_CLAUSE, span);
    if (having_clause == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(having_clause, predicate);
    return having_clause;
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

struct mylite_sql_ast_node *mylite_sql_parser_make_is_boolean_predicate(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *left,
    struct mylite_sql_token is_token,
    enum mylite_sql_ast_operator operator_kind,
    struct mylite_sql_token truth_token
) {
    struct mylite_sql_source_span span = left == NULL ? span_from_token(&is_token) : left->span;
    struct mylite_sql_ast_node *predicate = NULL;

    span = span_join(span, span_from_token(&truth_token));
    predicate = make_node(state, MYLITE_SQL_AST_IS_BOOLEAN_PREDICATE, span);
    if (predicate == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_operator(predicate, operator_kind);
    mylite_sql_ast_node_append_child(predicate, left);
    return predicate;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_between_predicate(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *left,
    struct mylite_sql_token between_token,
    struct mylite_sql_ast_node *lower,
    struct mylite_sql_ast_node *upper
) {
    struct mylite_sql_source_span span =
        left == NULL ? span_from_token(&between_token) : left->span;
    struct mylite_sql_ast_node *predicate = NULL;

    if (upper != NULL) {
        span = span_join(span, upper->span);
    }

    predicate = make_node(state, MYLITE_SQL_AST_BETWEEN_PREDICATE, span);
    if (predicate == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(predicate, left);
    mylite_sql_ast_node_append_child(predicate, lower);
    mylite_sql_ast_node_append_child(predicate, upper);
    return predicate;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_in_predicate(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *left,
    struct mylite_sql_token in_token,
    struct mylite_sql_ast_node *values,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_source_span span = left == NULL ? span_from_token(&in_token) : left->span;
    struct mylite_sql_ast_node *predicate = NULL;

    span = span_join(span, span_from_token(&right_paren));
    predicate = make_node(state, MYLITE_SQL_AST_IN_PREDICATE, span);
    if (predicate == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(predicate, left);
    mylite_sql_ast_node_append_child(predicate, values);
    return predicate;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_exists_predicate(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token exists_token,
    struct mylite_sql_ast_node *select_statement,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_source_span span = span_from_token(&exists_token);
    struct mylite_sql_ast_node *predicate = NULL;

    span = span_join(span, span_from_token(&right_paren));
    predicate = make_node(state, MYLITE_SQL_AST_EXISTS_PREDICATE, span);
    if (predicate == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(predicate, select_statement);
    return predicate;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_predicate_value_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span =
        value == NULL ? (struct mylite_sql_source_span){0} : value->span;
    struct mylite_sql_ast_node *list = make_node(state, MYLITE_SQL_AST_PREDICATE_VALUE_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, value);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_predicate_value(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *value
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, value);
    if (value != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, value->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_and_predicate(
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

    predicate = make_node(state, MYLITE_SQL_AST_AND_PREDICATE, span);
    if (predicate == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_operator(predicate, operator_kind);
    mylite_sql_ast_node_append_child(predicate, left);
    mylite_sql_ast_node_append_child(predicate, right);
    return predicate;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_or_predicate(
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

    predicate = make_node(state, MYLITE_SQL_AST_OR_PREDICATE, span);
    if (predicate == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_operator(predicate, operator_kind);
    mylite_sql_ast_node_append_child(predicate, left);
    mylite_sql_ast_node_append_child(predicate, right);
    return predicate;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_xor_predicate(
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

    predicate = make_node(state, MYLITE_SQL_AST_XOR_PREDICATE, span);
    if (predicate == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_operator(predicate, operator_kind);
    mylite_sql_ast_node_append_child(predicate, left);
    mylite_sql_ast_node_append_child(predicate, right);
    return predicate;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_not_predicate(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token operator_token,
    struct mylite_sql_ast_node *child
) {
    struct mylite_sql_source_span span = span_from_token(&operator_token);
    struct mylite_sql_ast_node *predicate = NULL;

    if (child != NULL) {
        span = span_join(span, child->span);
    }

    predicate = make_node(state, MYLITE_SQL_AST_NOT_PREDICATE, span);
    if (predicate == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_operator(predicate, MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT);
    mylite_sql_ast_node_append_child(predicate, child);
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

struct mylite_sql_ast_node *mylite_sql_parser_make_select_order_by_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token order_token,
    struct mylite_sql_parser_select_order_by_parts parts
) {
    struct mylite_sql_source_span span = span_from_token(&order_token);
    struct mylite_sql_ast_node *first_item = NULL;
    struct mylite_sql_ast_node *list = NULL;
    struct mylite_sql_ast_node *order_clause = NULL;
    struct mylite_sql_ast_node *item = NULL;

    if (parts.tail_items == NULL) {
        return mylite_sql_parser_make_order_by_clause(
            state,
            order_token,
            parts.first_order_key,
            parts.first_direction
        );
    }

    first_item =
        mylite_sql_parser_make_order_by_item(state, parts.first_order_key, parts.first_direction);
    list = mylite_sql_parser_make_order_by_item_list(state, first_item);
    if (list == NULL) {
        return NULL;
    }

    item = parts.tail_items->first_child;
    while (item != NULL) {
        struct mylite_sql_ast_node *next = item->next_sibling;
        mylite_sql_parser_append_order_by_item(state, list, item);
        item = next;
    }

    span = span_join(span, list->span);
    order_clause = make_node(state, MYLITE_SQL_AST_ORDER_BY_CLAUSE, span);
    if (order_clause == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(order_clause, list);
    return order_clause;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_order_by_item_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *item
) {
    struct mylite_sql_source_span span =
        item == NULL ? (struct mylite_sql_source_span){0} : item->span;
    struct mylite_sql_ast_node *list = make_node(state, MYLITE_SQL_AST_ORDER_BY_ITEM_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, item);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_order_by_item(
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

struct mylite_sql_ast_node *mylite_sql_parser_make_order_by_item(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *order_key,
    struct mylite_sql_ast_node *direction
) {
    struct mylite_sql_source_span span =
        order_key == NULL ? (struct mylite_sql_source_span){0} : order_key->span;
    struct mylite_sql_ast_node *item = NULL;

    if (direction != NULL) {
        span = span_join(span, direction->span);
    }

    item = make_node(state, MYLITE_SQL_AST_ORDER_BY_ITEM, span);
    if (item == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(item, order_key);
    mylite_sql_ast_node_append_child(item, direction);
    return item;
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

struct mylite_sql_ast_node *mylite_sql_parser_make_ignore_space_sensitive_identifier(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
) {
    if (parser_sql_mode_has(state, MYLITE_SQL_MODE_IGNORE_SPACE)) {
        mylite_sql_parser_state_syntax_error(state, MYLITE_SQL_PARSE_IDENTIFIER, token);
        return NULL;
    }
    return mylite_sql_parser_make_identifier(state, token);
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

struct mylite_sql_ast_node *mylite_sql_parser_make_qualified_wildcard(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *qualifier,
    struct mylite_sql_token token
) {
    struct mylite_sql_ast_node *wildcard = NULL;
    struct mylite_sql_source_span span =
        qualifier == NULL ? span_from_token(&token) : qualifier->span;
    struct mylite_sql_ast_node *qualified = NULL;

    wildcard = mylite_sql_parser_make_wildcard(state, token);
    if (wildcard != NULL) {
        span = span_join(span, wildcard->span);
    }

    qualified = make_node(state, MYLITE_SQL_AST_QUALIFIED_WILDCARD, span);
    if (qualified == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(qualified, qualifier);
    mylite_sql_ast_node_append_child(qualified, wildcard);
    return qualified;
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

struct mylite_sql_ast_node *mylite_sql_parser_make_dml_default_value(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token
) {
    return make_node(state, MYLITE_SQL_AST_DML_DEFAULT_VALUE, span_from_token(&token));
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

struct mylite_sql_ast_node *mylite_sql_parser_make_cast_binary_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token cast_token,
    struct mylite_sql_ast_node *value,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&cast_token), span_from_token(&right_paren));
    struct mylite_sql_ast_node *expression =
        make_node(state, MYLITE_SQL_AST_CAST_BINARY_EXPRESSION, span);

    if (expression == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(expression, value);
    return expression;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_convert_using_binary_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token convert_token,
    struct mylite_sql_ast_node *value,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&convert_token), span_from_token(&right_paren));
    struct mylite_sql_ast_node *expression =
        make_node(state, MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION, span);

    if (expression == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(expression, value);
    return expression;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_convert_binary_type_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token convert_token,
    struct mylite_sql_ast_node *value,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&convert_token), span_from_token(&right_paren));
    struct mylite_sql_ast_node *expression =
        make_node(state, MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION, span);

    if (expression == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(expression, value);
    return expression;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_convert_using_charset_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token convert_token,
    struct mylite_sql_ast_node *value,
    struct mylite_sql_ast_node *charset,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&convert_token), span_from_token(&right_paren));
    struct mylite_sql_ast_node *expression =
        make_node(state, MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION, span);

    if (expression == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(expression, value);
    mylite_sql_ast_node_append_child(expression, charset);
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

struct mylite_sql_ast_node *mylite_sql_parser_make_scalar_subquery_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token left_paren,
    struct mylite_sql_ast_node *select_statement,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_ast_node *subquery = make_node(
        state,
        MYLITE_SQL_AST_SCALAR_SUBQUERY,
        span_join(span_from_token(&left_paren), span_from_token(&right_paren))
    );
    if (subquery == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(subquery, select_statement);
    return subquery;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_searched_case_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token case_token,
    struct mylite_sql_ast_node *when_list,
    struct mylite_sql_ast_node *else_clause,
    struct mylite_sql_token end_token
) {
    struct mylite_sql_ast_node *case_expression = make_node(
        state,
        MYLITE_SQL_AST_SEARCHED_CASE_EXPRESSION,
        span_join(span_from_token(&case_token), span_from_token(&end_token))
    );
    if (case_expression == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(case_expression, when_list);
    mylite_sql_ast_node_append_child(case_expression, else_clause);
    return case_expression;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_simple_case_expression(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token case_token,
    struct mylite_sql_ast_node *case_value,
    struct mylite_sql_ast_node *when_list,
    struct mylite_sql_ast_node *else_clause,
    struct mylite_sql_token end_token
) {
    struct mylite_sql_ast_node *case_expression = make_node(
        state,
        MYLITE_SQL_AST_SIMPLE_CASE_EXPRESSION,
        span_join(span_from_token(&case_token), span_from_token(&end_token))
    );
    if (case_expression == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(case_expression, case_value);
    mylite_sql_ast_node_append_child(case_expression, when_list);
    mylite_sql_ast_node_append_child(case_expression, else_clause);
    return case_expression;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_case_when_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *when_clause
) {
    struct mylite_sql_source_span span =
        when_clause == NULL ? (struct mylite_sql_source_span){0} : when_clause->span;
    struct mylite_sql_ast_node *list = make_node(state, MYLITE_SQL_AST_CASE_WHEN_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, when_clause);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_case_when(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *when_clause
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, when_clause);
    if (when_clause != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, when_clause->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_case_when_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token when_token,
    struct mylite_sql_ast_node *condition,
    struct mylite_sql_ast_node *result
) {
    struct mylite_sql_source_span span = span_from_token(&when_token);
    struct mylite_sql_ast_node *when_clause = NULL;

    if (result != NULL) {
        span = span_join(span, result->span);
    } else if (condition != NULL) {
        span = span_join(span, condition->span);
    }

    when_clause = make_node(state, MYLITE_SQL_AST_CASE_WHEN_CLAUSE, span);
    if (when_clause == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(when_clause, condition);
    mylite_sql_ast_node_append_child(when_clause, result);
    return when_clause;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_case_else_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token else_token,
    struct mylite_sql_ast_node *result
) {
    struct mylite_sql_source_span span = span_from_token(&else_token);
    struct mylite_sql_ast_node *else_clause = NULL;

    if (result != NULL) {
        span = span_join(span, result->span);
    }

    else_clause = make_node(state, MYLITE_SQL_AST_CASE_ELSE_CLAUSE, span);
    if (else_clause == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(else_clause, result);
    return else_clause;
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
    if (!parser_sql_mode_has(state, MYLITE_SQL_MODE_IGNORE_SPACE) &&
        left_paren.offset != function_token.offset + function_token.length) {
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

struct mylite_sql_ast_node *mylite_sql_parser_make_no_space_one_argument_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    struct mylite_sql_token left_paren,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_ast_node *argument,
    struct mylite_sql_token right_paren
) {
    if (!parser_sql_mode_has(state, MYLITE_SQL_MODE_IGNORE_SPACE) &&
        left_paren.offset != function_token.offset + function_token.length) {
        mylite_sql_parser_state_syntax_error(state, MYLITE_SQL_PARSE_LPAREN, left_paren);
        return NULL;
    }

    return mylite_sql_parser_make_one_argument_function(
        state,
        function_token,
        function_kind,
        argument,
        right_paren
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_group_concat_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    struct mylite_sql_token left_paren,
    struct mylite_sql_ast_node *value,
    struct mylite_sql_ast_node *order_clause,
    struct mylite_sql_ast_node *separator,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_ast_node *function = NULL;

    if (!parser_sql_mode_has(state, MYLITE_SQL_MODE_IGNORE_SPACE) &&
        left_paren.offset != function_token.offset + function_token.length) {
        mylite_sql_parser_state_syntax_error(state, MYLITE_SQL_PARSE_LPAREN, left_paren);
        return NULL;
    }

    function = make_node(
        state,
        MYLITE_SQL_AST_GROUP_CONCAT_AGGREGATE_FUNCTION,
        span_join(span_from_token(&function_token), span_from_token(&right_paren))
    );
    if (function == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(function, value);
    mylite_sql_ast_node_append_child(function, order_clause);
    mylite_sql_ast_node_append_child(function, separator);
    return function;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_no_space_two_argument_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    struct mylite_sql_token left_paren,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_ast_node *first_argument,
    struct mylite_sql_ast_node *second_argument,
    struct mylite_sql_token right_paren
) {
    if (!parser_sql_mode_has(state, MYLITE_SQL_MODE_IGNORE_SPACE) &&
        left_paren.offset != function_token.offset + function_token.length) {
        mylite_sql_parser_state_syntax_error(state, MYLITE_SQL_PARSE_LPAREN, left_paren);
        return NULL;
    }

    return mylite_sql_parser_make_two_argument_function(
        state,
        function_token,
        function_kind,
        first_argument,
        second_argument,
        right_paren
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_no_space_three_argument_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    struct mylite_sql_token left_paren,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_ast_node *first_argument,
    struct mylite_sql_ast_node *second_argument,
    struct mylite_sql_ast_node *third_argument,
    struct mylite_sql_token right_paren
) {
    if (!parser_sql_mode_has(state, MYLITE_SQL_MODE_IGNORE_SPACE) &&
        left_paren.offset != function_token.offset + function_token.length) {
        mylite_sql_parser_state_syntax_error(state, MYLITE_SQL_PARSE_LPAREN, left_paren);
        return NULL;
    }

    return mylite_sql_parser_make_three_argument_function(
        state,
        function_token,
        function_kind,
        first_argument,
        second_argument,
        third_argument,
        right_paren
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_one_argument_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_ast_node *argument,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_ast_node *function = NULL;

    function = make_node(
        state,
        function_kind,
        span_join(span_from_token(&function_token), span_from_token(&right_paren))
    );
    if (function == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(function, argument);
    return function;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_trim_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_ast_node *remove_string,
    struct mylite_sql_ast_node *value,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_ast_node *function = NULL;

    function = make_node(
        state,
        function_kind,
        span_join(span_from_token(&function_token), span_from_token(&right_paren))
    );
    if (function == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(function, value);
    mylite_sql_ast_node_append_child(function, remove_string);
    return function;
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

struct mylite_sql_ast_node *mylite_sql_parser_make_two_argument_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_ast_node *first_argument,
    struct mylite_sql_ast_node *second_argument,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_ast_node *function = NULL;

    function = make_node(
        state,
        function_kind,
        span_join(span_from_token(&function_token), span_from_token(&right_paren))
    );
    if (function == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(function, first_argument);
    mylite_sql_ast_node_append_child(function, second_argument);
    return function;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_three_argument_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_ast_node *first_argument,
    struct mylite_sql_ast_node *second_argument,
    struct mylite_sql_ast_node *third_argument,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_ast_node *function = NULL;

    function = make_node(
        state,
        function_kind,
        span_join(span_from_token(&function_token), span_from_token(&right_paren))
    );
    if (function == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(function, first_argument);
    mylite_sql_ast_node_append_child(function, second_argument);
    mylite_sql_ast_node_append_child(function, third_argument);
    return function;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_four_argument_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_ast_node *first_argument,
    struct mylite_sql_ast_node *second_argument,
    struct mylite_sql_ast_node *third_argument,
    struct mylite_sql_ast_node *fourth_argument,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_ast_node *function = NULL;

    function = make_node(
        state,
        function_kind,
        span_join(span_from_token(&function_token), span_from_token(&right_paren))
    );
    if (function == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(function, first_argument);
    mylite_sql_ast_node_append_child(function, second_argument);
    mylite_sql_ast_node_append_child(function, third_argument);
    mylite_sql_ast_node_append_child(function, fourth_argument);
    return function;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_list_argument_function(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token function_token,
    enum mylite_sql_ast_node_kind function_kind,
    struct mylite_sql_ast_node *arguments,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_ast_node *function = NULL;

    function = make_node(
        state,
        function_kind,
        span_join(span_from_token(&function_token), span_from_token(&right_paren))
    );
    if (function == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(function, arguments);
    return function;
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

struct mylite_sql_ast_node *mylite_sql_parser_make_current_timestamp_keyword(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token current_timestamp_token
) {
    return make_node(
        state,
        MYLITE_SQL_AST_CURRENT_TIMESTAMP_VALUE,
        span_from_token(&current_timestamp_token)
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_current_date_keyword(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token current_date_token
) {
    return make_node(
        state,
        MYLITE_SQL_AST_CURRENT_DATE_VALUE,
        span_from_token(&current_date_token)
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_current_time_keyword(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token current_time_token
) {
    return make_node(
        state,
        MYLITE_SQL_AST_CURRENT_TIME_VALUE,
        span_from_token(&current_time_token)
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_utc_date_keyword(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token utc_date_token
) {
    return make_node(state, MYLITE_SQL_AST_UTC_DATE_VALUE, span_from_token(&utc_date_token));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_utc_time_keyword(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token utc_time_token
) {
    return make_node(state, MYLITE_SQL_AST_UTC_TIME_VALUE, span_from_token(&utc_time_token));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_utc_timestamp_keyword(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token utc_timestamp_token
) {
    return make_node(
        state,
        MYLITE_SQL_AST_UTC_TIMESTAMP_VALUE,
        span_from_token(&utc_timestamp_token)
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

struct mylite_sql_ast_node *mylite_sql_parser_make_primary_key_definition(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token primary_token,
    struct mylite_sql_ast_node *index_type,
    struct mylite_sql_ast_node *key_parts,
    struct mylite_sql_token right_paren,
    struct mylite_sql_ast_node *index_options
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&primary_token), span_from_token(&right_paren));
    struct mylite_sql_ast_node *primary_key =
        make_node(state, MYLITE_SQL_AST_PRIMARY_KEY_DEFINITION, span);
    if (primary_key == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(primary_key, key_parts);
    if (index_type != NULL) {
        mylite_sql_ast_node_append_child(primary_key, index_type);
    }
    if (index_options != NULL) {
        mylite_sql_ast_node_append_child(primary_key, index_options);
        mylite_sql_ast_node_set_span(
            primary_key,
            span_join(primary_key->span, index_options->span)
        );
    }
    return primary_key;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_primary_key_part_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *key_part
) {
    struct mylite_sql_source_span span =
        key_part == NULL ? (struct mylite_sql_source_span){0} : key_part->span;
    struct mylite_sql_ast_node *list = make_node(state, MYLITE_SQL_AST_PRIMARY_KEY_PART_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, key_part);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_primary_key_part(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *key_part
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, key_part);
    if (key_part != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, key_part->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_secondary_index_definition(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token index_token,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *index_type,
    struct mylite_sql_ast_node *key_parts,
    struct mylite_sql_token right_paren,
    struct mylite_sql_ast_node *index_options
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&index_token), span_from_token(&right_paren));
    struct mylite_sql_ast_node *secondary_index =
        make_node(state, MYLITE_SQL_AST_SECONDARY_INDEX_DEFINITION, span);
    if (secondary_index == NULL) {
        return NULL;
    }

    if (index_name != NULL) {
        mylite_sql_ast_node_append_child(secondary_index, index_name);
    }
    if (index_type != NULL) {
        mylite_sql_ast_node_append_child(secondary_index, index_type);
    }
    mylite_sql_ast_node_append_child(secondary_index, key_parts);
    if (index_options != NULL) {
        mylite_sql_ast_node_append_child(secondary_index, index_options);
        mylite_sql_ast_node_set_span(
            secondary_index,
            span_join(secondary_index->span, index_options->span)
        );
    }
    return secondary_index;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_unique_index_definition(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token unique_token,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *index_type,
    struct mylite_sql_ast_node *key_parts,
    struct mylite_sql_token right_paren,
    struct mylite_sql_ast_node *index_options
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&unique_token), span_from_token(&right_paren));
    struct mylite_sql_ast_node *unique_index =
        make_node(state, MYLITE_SQL_AST_UNIQUE_INDEX_DEFINITION, span);
    if (unique_index == NULL) {
        return NULL;
    }

    if (index_name != NULL) {
        mylite_sql_ast_node_append_child(unique_index, index_name);
    }
    if (index_type != NULL) {
        mylite_sql_ast_node_append_child(unique_index, index_type);
    }
    mylite_sql_ast_node_append_child(unique_index, key_parts);
    if (index_options != NULL) {
        mylite_sql_ast_node_append_child(unique_index, index_options);
        mylite_sql_ast_node_set_span(
            unique_index,
            span_join(unique_index->span, index_options->span)
        );
    }
    return unique_index;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_fulltext_index_definition(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token fulltext_token,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *key_parts,
    struct mylite_sql_token right_paren,
    struct mylite_sql_ast_node *index_options
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&fulltext_token), span_from_token(&right_paren));
    struct mylite_sql_ast_node *fulltext_index =
        make_node(state, MYLITE_SQL_AST_FULLTEXT_INDEX_DEFINITION, span);
    if (fulltext_index == NULL) {
        return NULL;
    }

    if (index_name != NULL) {
        mylite_sql_ast_node_append_child(fulltext_index, index_name);
    }
    mylite_sql_ast_node_append_child(fulltext_index, key_parts);
    if (index_options != NULL) {
        mylite_sql_ast_node_append_child(fulltext_index, index_options);
        mylite_sql_ast_node_set_span(
            fulltext_index,
            span_join(fulltext_index->span, index_options->span)
        );
    }
    return fulltext_index;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_spatial_index_definition(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token spatial_token,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *key_parts,
    struct mylite_sql_token right_paren,
    struct mylite_sql_ast_node *index_options
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&spatial_token), span_from_token(&right_paren));
    struct mylite_sql_ast_node *spatial_index =
        make_node(state, MYLITE_SQL_AST_SPATIAL_INDEX_DEFINITION, span);
    if (spatial_index == NULL) {
        return NULL;
    }

    if (index_name != NULL) {
        mylite_sql_ast_node_append_child(spatial_index, index_name);
    }
    mylite_sql_ast_node_append_child(spatial_index, key_parts);
    if (index_options != NULL) {
        mylite_sql_ast_node_append_child(spatial_index, index_options);
        mylite_sql_ast_node_set_span(
            spatial_index,
            span_join(spatial_index->span, index_options->span)
        );
    }
    return spatial_index;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_foreign_key_definition(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *constraint_name,
    struct mylite_sql_token foreign_token,
    struct mylite_sql_ast_node *index_name,
    struct mylite_sql_ast_node *child_parts,
    struct mylite_sql_ast_node *referenced_table,
    struct mylite_sql_ast_node *referenced_parts,
    struct mylite_sql_token right_paren,
    struct mylite_sql_ast_node *actions
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&foreign_token), span_from_token(&right_paren));
    struct mylite_sql_ast_node *definition = NULL;

    if (constraint_name != NULL) {
        span = span_join(constraint_name->span, span);
    }
    if (actions != NULL) {
        span = span_join(span, actions->span);
    }

    definition = make_node(state, MYLITE_SQL_AST_FOREIGN_KEY_DEFINITION, span);
    if (definition == NULL) {
        return NULL;
    }

    if (constraint_name != NULL) {
        mylite_sql_ast_node_append_child(definition, constraint_name);
    }
    if (index_name != NULL) {
        mylite_sql_ast_node_append_child(definition, index_name);
    }
    mylite_sql_ast_node_append_child(definition, child_parts);
    mylite_sql_ast_node_append_child(definition, referenced_table);
    mylite_sql_ast_node_append_child(definition, referenced_parts);
    if (actions != NULL) {
        mylite_sql_ast_node_append_child(definition, actions);
    }
    return definition;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_check_constraint_definition(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *constraint_name,
    struct mylite_sql_token check_token,
    struct mylite_sql_ast_node *expression,
    struct mylite_sql_token right_paren,
    struct mylite_sql_ast_node *enforcement
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&check_token), span_from_token(&right_paren));
    struct mylite_sql_ast_node *definition = NULL;

    if (constraint_name != NULL) {
        span = span_join(constraint_name->span, span);
    }
    if (enforcement != NULL) {
        span = span_join(span, enforcement->span);
    }

    definition = make_node(state, MYLITE_SQL_AST_CHECK_CONSTRAINT_DEFINITION, span);
    if (definition == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(definition, expression);
    if (constraint_name != NULL) {
        mylite_sql_ast_node_append_child(definition, constraint_name);
    }
    if (enforcement != NULL) {
        mylite_sql_ast_node_append_child(definition, enforcement);
    }
    return definition;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_check_enforcement(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token,
    enum mylite_sql_ast_node_kind kind
) {
    return make_node(state, kind, span_from_token(&token));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_foreign_key_part_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *key_part
) {
    struct mylite_sql_source_span span =
        key_part == NULL ? (struct mylite_sql_source_span){0} : key_part->span;
    struct mylite_sql_ast_node *list = make_node(state, MYLITE_SQL_AST_FOREIGN_KEY_PART_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, key_part);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_foreign_key_part(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *key_part
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, key_part);
    if (key_part != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, key_part->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_foreign_key_index_name(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *identifier
) {
    struct mylite_sql_source_span span =
        identifier == NULL ? (struct mylite_sql_source_span){0} : identifier->span;
    struct mylite_sql_ast_node *index_name =
        make_node(state, MYLITE_SQL_AST_FOREIGN_KEY_INDEX_NAME, span);
    if (index_name == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(index_name, identifier);
    return index_name;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_foreign_key_action_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *action
) {
    struct mylite_sql_source_span span =
        action == NULL ? (struct mylite_sql_source_span){0} : action->span;
    struct mylite_sql_ast_node *list =
        make_node(state, MYLITE_SQL_AST_FOREIGN_KEY_ACTION_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, action);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_foreign_key_action(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *action
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, action);
    if (action != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, action->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_foreign_key_action(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token first_token,
    struct mylite_sql_token last_token,
    enum mylite_sql_ast_node_kind kind
) {
    return make_node(
        state,
        kind,
        span_join(span_from_token(&first_token), span_from_token(&last_token))
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_secondary_index_part_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *key_part
) {
    struct mylite_sql_source_span span =
        key_part == NULL ? (struct mylite_sql_source_span){0} : key_part->span;
    struct mylite_sql_ast_node *list =
        make_node(state, MYLITE_SQL_AST_SECONDARY_INDEX_PART_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, key_part);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_secondary_index_part(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *key_part
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, key_part);
    if (key_part != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, key_part->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_secondary_index_part(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *column,
    struct mylite_sql_ast_node *prefix_length,
    struct mylite_sql_ast_node *direction
) {
    struct mylite_sql_source_span span =
        column == NULL ? (struct mylite_sql_source_span){0} : column->span;
    struct mylite_sql_ast_node *part = NULL;

    if (prefix_length != NULL) {
        span = span_join(span, prefix_length->span);
    }
    if (direction != NULL) {
        span = span_join(span, direction->span);
    }

    part = make_node(state, MYLITE_SQL_AST_SECONDARY_INDEX_PART, span);
    if (part == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(part, column);
    if (prefix_length != NULL) {
        mylite_sql_ast_node_append_child(part, prefix_length);
    }
    if (direction != NULL) {
        mylite_sql_ast_node_append_child(part, direction);
    }
    return part;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_inline_primary_key(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token primary_token,
    struct mylite_sql_token key_token
) {
    return make_node(
        state,
        MYLITE_SQL_AST_INLINE_PRIMARY_KEY,
        span_join(span_from_token(&primary_token), span_from_token(&key_token))
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_inline_unique_key(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token unique_token,
    struct mylite_sql_token end_token
) {
    return make_node(
        state,
        MYLITE_SQL_AST_INLINE_UNIQUE_KEY,
        span_join(span_from_token(&unique_token), span_from_token(&end_token))
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_attribute_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *attribute
) {
    struct mylite_sql_source_span span =
        attribute == NULL ? (struct mylite_sql_source_span){0} : attribute->span;
    struct mylite_sql_ast_node *list = make_node(state, MYLITE_SQL_AST_COLUMN_ATTRIBUTE_LIST, span);
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, attribute);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_column_attribute(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *list,
    struct mylite_sql_ast_node *attribute
) {
    if (!is_parse_ok(state) || list == NULL) {
        return list;
    }

    mylite_sql_ast_node_append_child(list, attribute);
    if (attribute != NULL) {
        mylite_sql_ast_node_set_span(list, span_join(list->span, attribute->span));
    }
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_auto_increment(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token auto_increment_token
) {
    return make_node(
        state,
        MYLITE_SQL_AST_COLUMN_AUTO_INCREMENT,
        span_from_token(&auto_increment_token)
    );
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_on_update_current_timestamp(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token on_token,
    struct mylite_sql_ast_node *current_timestamp_value
) {
    struct mylite_sql_source_span span = span_from_token(&on_token);
    struct mylite_sql_ast_node *on_update = NULL;

    if (current_timestamp_value != NULL) {
        span = span_join(span, current_timestamp_value->span);
    }
    on_update = make_node(state, MYLITE_SQL_AST_COLUMN_ON_UPDATE_CURRENT_TIMESTAMP, span);
    if (on_update == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(on_update, current_timestamp_value);
    return on_update;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_charset_attribute(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token charset_token,
    struct mylite_sql_ast_node *charset_name
) {
    struct mylite_sql_source_span span = span_from_token(&charset_token);
    struct mylite_sql_ast_node *attribute = NULL;

    if (charset_name != NULL) {
        span = span_join(span, charset_name->span);
    }

    attribute = make_node(state, MYLITE_SQL_AST_COLUMN_CHARSET_ATTRIBUTE, span);
    if (attribute == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(attribute, charset_name);
    return attribute;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_collation_attribute(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token collate_token,
    struct mylite_sql_ast_node *collation_name
) {
    struct mylite_sql_source_span span = span_from_token(&collate_token);
    struct mylite_sql_ast_node *attribute = NULL;

    if (collation_name != NULL) {
        span = span_join(span, collation_name->span);
    }

    attribute = make_node(state, MYLITE_SQL_AST_COLUMN_COLLATION_ATTRIBUTE, span);
    if (attribute == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(attribute, collation_name);
    return attribute;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_comment_attribute(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token comment_token,
    struct mylite_sql_ast_node *comment
) {
    struct mylite_sql_source_span span = span_from_token(&comment_token);
    struct mylite_sql_ast_node *attribute = NULL;

    if (comment != NULL) {
        span = span_join(span, comment->span);
    }

    attribute = make_node(state, MYLITE_SQL_AST_COLUMN_COMMENT_ATTRIBUTE, span);
    if (attribute == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(attribute, comment);
    return attribute;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_generated_column_clause(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token as_token,
    struct mylite_sql_ast_node *expression,
    struct mylite_sql_token right_paren_token,
    struct mylite_sql_ast_node *storage
) {
    struct mylite_sql_source_span span = span_from_token(&as_token);
    struct mylite_sql_ast_node *clause = NULL;

    span = span_join(span, span_from_token(&right_paren_token));
    if (storage != NULL) {
        span = span_join(span, storage->span);
    }

    clause = make_node(state, MYLITE_SQL_AST_GENERATED_COLUMN_CLAUSE, span);
    if (clause == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(clause, expression);
    mylite_sql_ast_node_append_child(clause, storage);
    return clause;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_generated_column_storage(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token token,
    enum mylite_sql_ast_node_kind kind
) {
    return make_node(state, kind, span_from_token(&token));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_definition(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *name,
    struct mylite_sql_ast_node *column_type,
    struct mylite_sql_ast_node *nullability,
    struct mylite_sql_ast_node *default_null,
    struct mylite_sql_ast_node *primary_key
) {
    struct mylite_sql_source_span span =
        name == NULL ? (struct mylite_sql_source_span){0} : name->span;
    struct mylite_sql_ast_node *column = NULL;

    if (column_type != NULL) {
        span = span_join(span, column_type->span);
    }
    if (nullability != NULL) {
        span = span_join(span, nullability->span);
    }
    if (default_null != NULL) {
        span = span_join(span, default_null->span);
    }
    if (primary_key != NULL) {
        span = span_join(span, primary_key->span);
    }

    column = make_node(state, MYLITE_SQL_AST_COLUMN_DEFINITION, span);
    if (column == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(column, name);
    mylite_sql_ast_node_append_child(column, column_type);
    mylite_sql_ast_node_append_child(column, nullability);
    mylite_sql_ast_node_append_child(column, default_null);
    mylite_sql_ast_node_append_child(column, primary_key);
    return column;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_column_definition_with_attributes(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *name,
    struct mylite_sql_ast_node *column_type,
    struct mylite_sql_ast_node *attributes
) {
    struct mylite_sql_source_span span =
        name == NULL ? (struct mylite_sql_source_span){0} : name->span;
    struct column_attribute_positions positions = {0};
    struct mylite_sql_ast_node *column = NULL;
    struct mylite_sql_ast_node *attribute = NULL;
    int rc = MYLITE_SQL_PARSE_OK;

    if (column_type != NULL) {
        span = span_join(span, column_type->span);
    }
    if (attributes != NULL) {
        span = span_join(span, attributes->span);
    }

    rc = scan_column_attribute_positions(state, attributes, &positions);
    if (rc == MYLITE_SQL_PARSE_OK) {
        rc = validate_legacy_column_attribute_order(state, &positions);
    }
    if (rc != MYLITE_SQL_PARSE_OK) {
        return NULL;
    }

    column = make_node(state, MYLITE_SQL_AST_COLUMN_DEFINITION, span);
    if (column == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(column, name);
    mylite_sql_ast_node_append_child(column, column_type);
    attribute = attributes == NULL ? NULL : attributes->first_child;
    while (attribute != NULL) {
        struct mylite_sql_ast_node *next = attribute->next_sibling;

        mylite_sql_ast_node_append_child(column, attribute);
        attribute = next;
    }

    return column;
}

static int scan_column_attribute_positions(
    struct mylite_sql_parser_state *state,
    const struct mylite_sql_ast_node *attributes,
    struct column_attribute_positions *out_positions
) {
    const struct mylite_sql_ast_node *attribute = NULL;
    size_t position = 0U;
    int rc = MYLITE_SQL_PARSE_OK;

    *out_positions = (struct column_attribute_positions){
        .charset = (size_t)-1,
        .collation = (size_t)-1,
        .comment = (size_t)-1,
        .nullability = (size_t)-1,
        .default_value = (size_t)-1,
        .primary_key = (size_t)-1,
        .unique_key = (size_t)-1,
        .auto_increment = (size_t)-1,
        .generated = (size_t)-1,
    };

    attribute = attributes == NULL ? NULL : attributes->first_child;
    while (rc == MYLITE_SQL_PARSE_OK && attribute != NULL) {
        switch (attribute->kind) {
        case MYLITE_SQL_AST_COLUMN_CHARSET_ATTRIBUTE:
            rc = record_column_attribute_position(state, &out_positions->charset, position);
            break;
        case MYLITE_SQL_AST_COLUMN_COLLATION_ATTRIBUTE:
            rc = record_column_attribute_position(state, &out_positions->collation, position);
            break;
        case MYLITE_SQL_AST_COLUMN_COMMENT_ATTRIBUTE:
            if (!column_attribute_position_is_set(out_positions->comment)) {
                out_positions->comment = position;
            }
            break;
        case MYLITE_SQL_AST_NULLABILITY:
            rc = record_column_attribute_position(state, &out_positions->nullability, position);
            break;
        case MYLITE_SQL_AST_COLUMN_DEFAULT_NULL:
        case MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE:
            rc = record_column_attribute_position(state, &out_positions->default_value, position);
            break;
        case MYLITE_SQL_AST_INLINE_PRIMARY_KEY:
            rc = record_column_attribute_position(state, &out_positions->primary_key, position);
            break;
        case MYLITE_SQL_AST_INLINE_UNIQUE_KEY:
            rc = record_column_attribute_position(state, &out_positions->unique_key, position);
            break;
        case MYLITE_SQL_AST_COLUMN_AUTO_INCREMENT:
            rc = record_column_attribute_position(state, &out_positions->auto_increment, position);
            break;
        case MYLITE_SQL_AST_GENERATED_COLUMN_CLAUSE:
            rc = record_column_attribute_position(state, &out_positions->generated, position);
            break;
        default:
            break;
        }

        ++position;
        attribute = attribute->next_sibling;
    }

    return rc;
}

static int record_column_attribute_position(
    struct mylite_sql_parser_state *state,
    size_t *slot,
    size_t position
) {
    if (column_attribute_position_is_set(*slot)) {
        set_state_status(state, MYLITE_SQL_PARSE_SYNTAX_ERROR);
        return MYLITE_SQL_PARSE_SYNTAX_ERROR;
    }

    *slot = position;
    return MYLITE_SQL_PARSE_OK;
}

static int validate_legacy_column_attribute_order(
    struct mylite_sql_parser_state *state,
    const struct column_attribute_positions *positions
) {
    bool invalid_order = false;
    size_t charset_collation_limit = column_charset_collation_position_limit(positions);

    if (column_attribute_position_is_set(positions->auto_increment)) {
        if (!column_attribute_position_is_set(positions->charset) &&
            !column_attribute_position_is_set(positions->collation)) {
            return MYLITE_SQL_PARSE_OK;
        }
    }
    if (column_attribute_position_is_set(positions->charset) &&
        column_attribute_position_is_set(positions->collation) &&
        positions->charset > positions->collation) {
        invalid_order = true;
    }
    if (column_attribute_position_is_set(positions->generated) &&
        ((column_attribute_position_is_set(positions->nullability) &&
          positions->nullability < positions->generated) ||
         (column_attribute_position_is_set(positions->default_value) &&
          positions->default_value < positions->generated) ||
         (column_attribute_position_is_set(positions->primary_key) &&
          positions->primary_key < positions->generated) ||
         (column_attribute_position_is_set(positions->unique_key) &&
          positions->unique_key < positions->generated) ||
         (column_attribute_position_is_set(positions->auto_increment) &&
          positions->auto_increment < positions->generated) ||
         (column_attribute_position_is_set(positions->comment) &&
          positions->comment < positions->generated))) {
        invalid_order = true;
    }
    if (legacy_column_attribute_precedes_charset_collation(positions, charset_collation_limit)) {
        invalid_order = true;
    }
    if (invalid_order) {
        set_state_status(state, MYLITE_SQL_PARSE_SYNTAX_ERROR);
        return MYLITE_SQL_PARSE_SYNTAX_ERROR;
    }
    if (column_attribute_position_is_set(positions->auto_increment)) {
        return MYLITE_SQL_PARSE_OK;
    }

    invalid_order = ((column_attribute_position_is_set(positions->nullability) &&
                      column_attribute_position_is_set(positions->default_value) &&
                      positions->nullability > positions->default_value) ||
                     (column_attribute_position_is_set(positions->nullability) &&
                      column_attribute_position_is_set(positions->primary_key) &&
                      positions->nullability > positions->primary_key) ||
                     (column_attribute_position_is_set(positions->default_value) &&
                      column_attribute_position_is_set(positions->primary_key) &&
                      positions->default_value > positions->primary_key)) != 0;
    if (invalid_order) {
        set_state_status(state, MYLITE_SQL_PARSE_SYNTAX_ERROR);
        return MYLITE_SQL_PARSE_SYNTAX_ERROR;
    }

    return MYLITE_SQL_PARSE_OK;
}

static size_t column_charset_collation_position_limit(
    const struct column_attribute_positions *positions
) {
    size_t limit = (size_t)-1;

    if (column_attribute_position_is_set(positions->charset)) {
        limit = positions->charset;
    }
    if (column_attribute_position_is_set(positions->collation) &&
        (!column_attribute_position_is_set(limit) || positions->collation > limit)) {
        limit = positions->collation;
    }

    return limit;
}

static bool legacy_column_attribute_precedes_charset_collation(
    const struct column_attribute_positions *positions,
    size_t charset_collation_limit
) {
    if (!column_attribute_position_is_set(charset_collation_limit)) {
        return false;
    }

    return ((column_attribute_position_is_set(positions->nullability) &&
             positions->nullability < charset_collation_limit) ||
            (column_attribute_position_is_set(positions->default_value) &&
             positions->default_value < charset_collation_limit) ||
            (column_attribute_position_is_set(positions->primary_key) &&
             positions->primary_key < charset_collation_limit) ||
            (column_attribute_position_is_set(positions->unique_key) &&
             positions->unique_key < charset_collation_limit) ||
            (column_attribute_position_is_set(positions->comment) &&
             positions->comment < charset_collation_limit) ||
            (column_attribute_position_is_set(positions->auto_increment) &&
             positions->auto_increment < charset_collation_limit)) != 0;
}

static bool column_attribute_position_is_set(size_t position) {
    return position != (size_t)-1;
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
    int is_bool_alias,
    int is_serial_alias
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
            .is_serial_alias = is_serial_alias,
            .display_width_span = span_from_token(&display_width_token),
        }
    );
    return type;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_varchar_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_varchar_type_tokens tokens
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&tokens.type_token), span_from_token(&tokens.end_token));
    struct mylite_sql_ast_node *type = make_node(state, MYLITE_SQL_AST_VARCHAR_TYPE, span);
    if (type == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_varchar_type(
        type,
        (struct mylite_sql_ast_varchar_type_payload){
            .is_national = tokens.is_national,
            .length_span = span_from_token(&tokens.length_token),
        }
    );
    return type;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_char_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_char_type_tokens tokens
) {
    struct mylite_sql_source_span span = span_from_token(&tokens.type_token);
    struct mylite_sql_ast_node *type = NULL;

    if (tokens.has_explicit_length ||
        (tokens.end_token.text != NULL && tokens.end_token.text != tokens.type_token.text)) {
        span = span_join(span, span_from_token(&tokens.end_token));
    }

    type = make_node(state, MYLITE_SQL_AST_CHAR_TYPE, span);
    if (type == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_char_type(
        type,
        (struct mylite_sql_ast_char_type_payload){
            .has_explicit_length = tokens.has_explicit_length,
            .is_national = tokens.is_national,
            .length_span = span_from_token(&tokens.length_token),
        }
    );
    return type;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_text_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_text_type_tokens tokens
) {
    struct mylite_sql_source_span span = span_from_token(&tokens.type_token);
    struct mylite_sql_ast_node *type = NULL;

    if (tokens.end_token.text != NULL) {
        span = span_join(span, span_from_token(&tokens.end_token));
    }

    type = make_node(state, MYLITE_SQL_AST_TEXT_TYPE, span);
    if (type == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_text_type(
        type,
        (struct mylite_sql_ast_text_type_payload){
            .kind = tokens.text_type,
            .has_length = tokens.has_length,
            .length_span = span_from_token(&tokens.length_token),
        }
    );
    return type;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_json_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token type_token
) {
    return make_node(state, MYLITE_SQL_AST_JSON_TYPE, span_from_token(&type_token));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_spatial_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_spatial_type_tokens tokens
) {
    struct mylite_sql_ast_node *type =
        make_node(state, MYLITE_SQL_AST_SPATIAL_TYPE, span_from_token(&tokens.type_token));
    if (type == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_spatial_type(type, tokens.spatial_type);
    return type;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_enum_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token type_token,
    struct mylite_sql_ast_node *label_list,
    struct mylite_sql_token end_token
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&type_token), span_from_token(&end_token));
    struct mylite_sql_ast_node *type = make_node(state, MYLITE_SQL_AST_ENUM_TYPE, span);
    if (type == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(type, label_list);
    return type;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_enum_label_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token label_token
) {
    struct mylite_sql_ast_node *label =
        mylite_sql_parser_make_literal(state, label_token, MYLITE_SQL_AST_LITERAL_STRING);
    struct mylite_sql_ast_node *list =
        make_node(state, MYLITE_SQL_AST_ENUM_LABEL_LIST, span_from_token(&label_token));
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, label);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_enum_label(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *label_list,
    struct mylite_sql_token label_token
) {
    struct mylite_sql_ast_node *label = NULL;

    if (!is_parse_ok(state) || label_list == NULL) {
        return label_list;
    }

    label = mylite_sql_parser_make_literal(state, label_token, MYLITE_SQL_AST_LITERAL_STRING);
    mylite_sql_ast_node_append_child(label_list, label);
    mylite_sql_ast_node_set_span(
        label_list,
        span_join(label_list->span, span_from_token(&label_token))
    );
    return label_list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_set_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token type_token,
    struct mylite_sql_ast_node *member_list,
    struct mylite_sql_token end_token
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&type_token), span_from_token(&end_token));
    struct mylite_sql_ast_node *type = make_node(state, MYLITE_SQL_AST_SET_TYPE, span);
    if (type == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(type, member_list);
    return type;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_set_member_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token member_token
) {
    struct mylite_sql_ast_node *member =
        mylite_sql_parser_make_literal(state, member_token, MYLITE_SQL_AST_LITERAL_STRING);
    struct mylite_sql_ast_node *list =
        make_node(state, MYLITE_SQL_AST_SET_MEMBER_LIST, span_from_token(&member_token));
    if (list == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_append_child(list, member);
    return list;
}

struct mylite_sql_ast_node *mylite_sql_parser_append_set_member(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *member_list,
    struct mylite_sql_token member_token
) {
    struct mylite_sql_ast_node *member = NULL;

    if (!is_parse_ok(state) || member_list == NULL) {
        return member_list;
    }

    member = mylite_sql_parser_make_literal(state, member_token, MYLITE_SQL_AST_LITERAL_STRING);
    mylite_sql_ast_node_append_child(member_list, member);
    mylite_sql_ast_node_set_span(
        member_list,
        span_join(member_list->span, span_from_token(&member_token))
    );
    return member_list;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_binary_string_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_binary_string_type_tokens tokens
) {
    struct mylite_sql_source_span span = span_from_token(&tokens.type_token);
    struct mylite_sql_ast_node *type = NULL;

    if (tokens.end_token.text != NULL) {
        span = span_join(span, span_from_token(&tokens.end_token));
    }

    type = make_node(state, MYLITE_SQL_AST_BINARY_STRING_TYPE, span);
    if (type == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_binary_string_type(
        type,
        (struct mylite_sql_ast_binary_string_type_payload){
            .kind = tokens.binary_string_type,
            .has_length = tokens.has_length,
            .length_span = span_from_token(&tokens.length_token),
        }
    );
    return type;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_bit_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_bit_type_tokens tokens
) {
    struct mylite_sql_source_span span = span_from_token(&tokens.type_token);
    struct mylite_sql_ast_node *type = NULL;

    if (tokens.end_token.text != NULL) {
        span = span_join(span, span_from_token(&tokens.end_token));
    }

    type = make_node(state, MYLITE_SQL_AST_BIT_TYPE, span);
    if (type == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_bit_type(
        type,
        (struct mylite_sql_ast_bit_type_payload){
            .has_length = tokens.has_length,
            .length_span = span_from_token(&tokens.length_token),
        }
    );
    return type;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_year_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_year_type_tokens tokens
) {
    struct mylite_sql_source_span span = span_from_token(&tokens.type_token);
    struct mylite_sql_ast_node *type = NULL;

    if (tokens.end_token.text != NULL) {
        span = span_join(span, span_from_token(&tokens.end_token));
    }

    type = make_node(state, MYLITE_SQL_AST_YEAR_TYPE, span);
    if (type == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_year_type(
        type,
        (struct mylite_sql_ast_year_type_payload){
            .has_width = tokens.has_width,
            .width_span = span_from_token(&tokens.width_token),
        }
    );
    return type;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_decimal_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_decimal_type_tokens tokens
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&tokens.type_token), span_from_token(&tokens.end_token));
    struct mylite_sql_ast_node *type = make_node(state, MYLITE_SQL_AST_DECIMAL_TYPE, span);
    if (type == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_decimal_type(
        type,
        (struct mylite_sql_ast_decimal_type_payload){
            .kind = tokens.decimal_type,
            .has_precision = tokens.has_precision,
            .has_scale = tokens.has_scale,
            .is_unsigned = tokens.is_unsigned,
            .precision_span = span_from_token(&tokens.precision_token),
            .scale_span = span_from_token(&tokens.scale_token),
        }
    );
    return type;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_approximate_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_approximate_type_tokens tokens
) {
    struct mylite_sql_source_span span =
        span_join(span_from_token(&tokens.type_token), span_from_token(&tokens.end_token));
    struct mylite_sql_ast_node *type = make_node(state, MYLITE_SQL_AST_APPROXIMATE_TYPE, span);
    if (type == NULL) {
        return NULL;
    }

    mylite_sql_ast_node_set_approximate_type(
        type,
        (struct mylite_sql_ast_approximate_type_payload){
            .kind = tokens.approximate_type,
            .has_precision = tokens.has_precision,
            .is_unsigned = tokens.is_unsigned,
            .precision_span = span_from_token(&tokens.precision_token),
        }
    );
    return type;
}

struct mylite_sql_ast_node *mylite_sql_parser_make_date_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token date_token
) {
    return make_node(state, MYLITE_SQL_AST_DATE_TYPE, span_from_token(&date_token));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_datetime_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token datetime_token
) {
    return make_node(state, MYLITE_SQL_AST_DATETIME_TYPE, span_from_token(&datetime_token));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_timestamp_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token timestamp_token
) {
    return make_node(state, MYLITE_SQL_AST_TIMESTAMP_TYPE, span_from_token(&timestamp_token));
}

struct mylite_sql_ast_node *mylite_sql_parser_make_time_type(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token time_token
) {
    return make_node(state, MYLITE_SQL_AST_TIME_TYPE, span_from_token(&time_token));
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

struct mylite_sql_ast_node *mylite_sql_parser_make_empty_identifier_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token left_paren,
    struct mylite_sql_token right_paren
) {
    struct mylite_sql_ast_node *list = mylite_sql_parser_make_identifier_list(state, NULL);

    if (list == NULL) {
        return NULL;
    }
    mylite_sql_ast_node_set_span(
        list,
        span_join(span_from_token(&left_paren), span_from_token(&right_paren))
    );
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

struct mylite_sql_ast_node *mylite_sql_parser_make_values_row_list(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *row
) {
    struct mylite_sql_source_span span =
        row == NULL ? (struct mylite_sql_source_span){0} : row->span;
    struct mylite_sql_ast_node *list = make_node(state, MYLITE_SQL_AST_VALUES_ROW_LIST, span);
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

struct mylite_sql_ast_node *mylite_sql_parser_append_values_row(
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

struct mylite_sql_ast_node *mylite_sql_parser_make_values_row_values(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_ast_node *value
) {
    struct mylite_sql_source_span span =
        value == NULL ? (struct mylite_sql_source_span){0} : value->span;
    struct mylite_sql_ast_node *row = make_node(state, MYLITE_SQL_AST_VALUES_ROW, span);
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

struct mylite_sql_ast_node *mylite_sql_parser_append_values_value(
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

struct mylite_sql_ast_node *mylite_sql_parser_make_values_row(
    struct mylite_sql_parser_state *state,
    struct mylite_sql_token row_token,
    struct mylite_sql_ast_node *values,
    struct mylite_sql_token right_paren
) {
    if (!is_parse_ok(state) || values == NULL) {
        return values;
    }

    mylite_sql_ast_node_set_span(
        values,
        span_join(span_from_token(&row_token), span_from_token(&right_paren))
    );
    return values;
}

static bool map_lexer_token(
    const struct mylite_sql_parser_state *state,
    const struct mylite_sql_token *token,
    bool previous_token_was_dot,
    int previous_parser_token,
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
        if (!map_keyword_token(
                token,
                previous_token_was_dot,
                previous_parser_token,
                &parser_token
            )) {
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
        if (!map_operator_token(state, token, &parser_token)) {
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
    case MYLITE_SQL_TOKEN_PARAMETER:
        return false;
    case MYLITE_SQL_TOKEN_USER_VARIABLE:
        parser_token = MYLITE_SQL_PARSE_USER_VARIABLE;
        break;
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
    int previous_parser_token,
    int *out_parser_token
) {
    static const struct {
        const char *keyword;
        int parser_token;
    } keyword_mappings[] = {
        {"SELECT", MYLITE_SQL_PARSE_SELECT},
        {"ALL", MYLITE_SQL_PARSE_ALL},
        {"ALGORITHM", MYLITE_SQL_PARSE_ALGORITHM},
        {"ALTER", MYLITE_SQL_PARSE_ALTER},
        {"AS", MYLITE_SQL_PARSE_AS},
        {"CAST", MYLITE_SQL_PARSE_CAST},
        {"CONVERT", MYLITE_SQL_PARSE_CONVERT},
        {"FROM", MYLITE_SQL_PARSE_FROM},
        {"UNION", MYLITE_SQL_PARSE_UNION},
        {"WHERE", MYLITE_SQL_PARSE_WHERE},
        {"AND", MYLITE_SQL_PARSE_AND},
        {"BETWEEN", MYLITE_SQL_PARSE_BETWEEN},
        {"OR", MYLITE_SQL_PARSE_OR},
        {"XOR", MYLITE_SQL_PARSE_XOR},
        {"GROUP", MYLITE_SQL_PARSE_GROUP},
        {"GROUP_CONCAT", MYLITE_SQL_PARSE_GROUP_CONCAT},
        {"ANY_VALUE", MYLITE_SQL_PARSE_ANY_VALUE},
        {"HAVING", MYLITE_SQL_PARSE_HAVING},
        {"ORDER", MYLITE_SQL_PARSE_ORDER},
        {"BY", MYLITE_SQL_PARSE_BY},
        {"BINARY", MYLITE_SQL_PARSE_BINARY},
        {"USING", MYLITE_SQL_PARSE_USING},
        {"BIT", MYLITE_SQL_PARSE_BIT},
        {"BIN", MYLITE_SQL_PARSE_BIN},
        {"BIT_LENGTH", MYLITE_SQL_PARSE_BIT_LENGTH},
        {"OCT", MYLITE_SQL_PARSE_OCT},
        {"OCTET_LENGTH", MYLITE_SQL_PARSE_OCTET_LENGTH},
        {"ORD", MYLITE_SQL_PARSE_ORD},
        {"ABS", MYLITE_SQL_PARSE_ABS},
        {"ACOS", MYLITE_SQL_PARSE_ACOS},
        {"ASCII", MYLITE_SQL_PARSE_ASCII},
        {"ASIN", MYLITE_SQL_PARSE_ASIN},
        {"ATAN", MYLITE_SQL_PARSE_ATAN},
        {"ATAN2", MYLITE_SQL_PARSE_ATAN2},
        {"COS", MYLITE_SQL_PARSE_COS},
        {"COT", MYLITE_SQL_PARSE_COT},
        {"EXP", MYLITE_SQL_PARSE_EXP},
        {"LN", MYLITE_SQL_PARSE_LN},
        {"LOG", MYLITE_SQL_PARSE_LOG},
        {"LOGS", MYLITE_SQL_PARSE_LOGS},
        {"LOG10", MYLITE_SQL_PARSE_LOG10},
        {"LOG2", MYLITE_SQL_PARSE_LOG2},
        {"POW", MYLITE_SQL_PARSE_POW},
        {"POWER", MYLITE_SQL_PARSE_POWER},
        {"SIGN", MYLITE_SQL_PARSE_SIGN},
        {"CEIL", MYLITE_SQL_PARSE_CEIL},
        {"CEILING", MYLITE_SQL_PARSE_CEILING},
        {"FLOOR", MYLITE_SQL_PARSE_FLOOR},
        {"ROUND", MYLITE_SQL_PARSE_ROUND},
        {"PI", MYLITE_SQL_PARSE_PI},
        {"RAND", MYLITE_SQL_PARSE_RAND},
        {"REPLICA", MYLITE_SQL_PARSE_REPLICA},
        {"REPLICAS", MYLITE_SQL_PARSE_REPLICAS},
        {"SIN", MYLITE_SQL_PARSE_SIN},
        {"SQRT", MYLITE_SQL_PARSE_SQRT},
        {"TAN", MYLITE_SQL_PARSE_TAN},
        {"DEGREES", MYLITE_SQL_PARSE_DEGREES},
        {"RADIANS", MYLITE_SQL_PARSE_RADIANS},
        {"CONNECTION_ID", MYLITE_SQL_PARSE_CONNECTION_ID},
        {"COUNT", MYLITE_SQL_PARSE_COUNT},
        {"CRC32", MYLITE_SQL_PARSE_CRC32},
        {"FROM_BASE64", MYLITE_SQL_PARSE_FROM_BASE64},
        {"HEX", MYLITE_SQL_PARSE_HEX},
        {"TO_BASE64", MYLITE_SQL_PARSE_TO_BASE64},
        {"UNHEX", MYLITE_SQL_PARSE_UNHEX},
        {"IS_UUID", MYLITE_SQL_PARSE_IS_UUID},
        {"UUID", MYLITE_SQL_PARSE_UUID},
        {"UUID_TO_BIN", MYLITE_SQL_PARSE_UUID_TO_BIN},
        {"BIN_TO_UUID", MYLITE_SQL_PARSE_BIN_TO_UUID},
        {"AVG", MYLITE_SQL_PARSE_AVG},
        {"BIT_AND", MYLITE_SQL_PARSE_BIT_AND},
        {"BIT_COUNT", MYLITE_SQL_PARSE_BIT_COUNT},
        {"BIT_OR", MYLITE_SQL_PARSE_BIT_OR},
        {"BIT_XOR", MYLITE_SQL_PARSE_BIT_XOR},
        {"BOTH", MYLITE_SQL_PARSE_BOTH},
        {"CROSS", MYLITE_SQL_PARSE_CROSS},
        {"DISTINCT", MYLITE_SQL_PARSE_DISTINCT},
        {"DISTINCTROW", MYLITE_SQL_PARSE_DISTINCTROW},
        {"CURDATE", MYLITE_SQL_PARSE_CURDATE},
        {"CURRENT_DATE", MYLITE_SQL_PARSE_CURRENT_DATE},
        {"CURRENT_ROLE", MYLITE_SQL_PARSE_CURRENT_ROLE},
        {"CURRENT_TIME", MYLITE_SQL_PARSE_CURRENT_TIME},
        {"CURRENT_TIMESTAMP", MYLITE_SQL_PARSE_CURRENT_TIMESTAMP},
        {"CURRENT_USER", MYLITE_SQL_PARSE_CURRENT_USER},
        {"CURTIME", MYLITE_SQL_PARSE_CURTIME},
        {"UTC_DATE", MYLITE_SQL_PARSE_UTC_DATE},
        {"UTC_TIME", MYLITE_SQL_PARSE_UTC_TIME},
        {"UTC_TIMESTAMP", MYLITE_SQL_PARSE_UTC_TIMESTAMP},
        {"ASC", MYLITE_SQL_PARSE_ASC},
        {"DESC", MYLITE_SQL_PARSE_DESC},
        {"AUTO_INCREMENT", MYLITE_SQL_PARSE_AUTO_INCREMENT},
        {"LAST_INSERT_ID", MYLITE_SQL_PARSE_LAST_INSERT_ID},
        {"LCASE", MYLITE_SQL_PARSE_LCASE},
        {"LEADING", MYLITE_SQL_PARSE_LEADING},
        {"LENGTH", MYLITE_SQL_PARSE_LENGTH},
        {"LOCATE", MYLITE_SQL_PARSE_LOCATE},
        {"LPAD", MYLITE_SQL_PARSE_LPAD},
        {"MID", MYLITE_SQL_PARSE_MID},
        {"MINUTE", MYLITE_SQL_PARSE_MINUTE},
        {"MONTH", MYLITE_SQL_PARSE_MONTH},
        {"RIGHT", MYLITE_SQL_PARSE_RIGHT},
        {"REPEAT", MYLITE_SQL_PARSE_REPEAT},
        {"REVERSE", MYLITE_SQL_PARSE_REVERSE},
        {"QUOTE", MYLITE_SQL_PARSE_QUOTE},
        {"RPAD", MYLITE_SQL_PARSE_RPAD},
        {"INSTR", MYLITE_SQL_PARSE_INSTR},
        {"LOWER", MYLITE_SQL_PARSE_LOWER},
        {"LTRIM", MYLITE_SQL_PARSE_LTRIM},
        {"MAX", MYLITE_SQL_PARSE_MAX},
        {"MIN", MYLITE_SQL_PARSE_MIN},
        {"SUM", MYLITE_SQL_PARSE_SUM},
        {"LIMIT", MYLITE_SQL_PARSE_LIMIT},
        {"OFFSET", MYLITE_SQL_PARSE_OFFSET},
        {"SPACE", MYLITE_SQL_PARSE_SPACE},
        {"USE", MYLITE_SQL_PARSE_USE},
        {"CREATE", MYLITE_SQL_PARSE_CREATE},
        {"TABLE", MYLITE_SQL_PARSE_TABLE},
        {"VIEW", MYLITE_SQL_PARSE_VIEW},
        {"TEMPORARY", MYLITE_SQL_PARSE_TEMPORARY},
        {"GENERATED", MYLITE_SQL_PARSE_GENERATED},
        {"ALWAYS", MYLITE_SQL_PARSE_ALWAYS},
        {"VIRTUAL", MYLITE_SQL_PARSE_VIRTUAL},
        {"STORED", MYLITE_SQL_PARSE_STORED},
        {"IF", MYLITE_SQL_PARSE_IF},
        {"IFNULL", MYLITE_SQL_PARSE_IFNULL},
        {"COALESCE", MYLITE_SQL_PARSE_COALESCE},
        {"COERCIBILITY", MYLITE_SQL_PARSE_COERCIBILITY},
        {"CONCAT", MYLITE_SQL_PARSE_CONCAT},
        {"CONCAT_WS", MYLITE_SQL_PARSE_CONCAT_WS},
        {"CONV", MYLITE_SQL_PARSE_CONV},
        {"POSITION", MYLITE_SQL_PARSE_POSITION},
        {"NULLIF", MYLITE_SQL_PARSE_NULLIF},
        {"ISNULL", MYLITE_SQL_PARSE_ISNULL},
        {"CASE", MYLITE_SQL_PARSE_CASE},
        {"WHEN", MYLITE_SQL_PARSE_WHEN},
        {"THEN", MYLITE_SQL_PARSE_THEN},
        {"ELSE", MYLITE_SQL_PARSE_ELSE},
        {"END", MYLITE_SQL_PARSE_END},
        {"MOD", MYLITE_SQL_PARSE_MOD},
        {"DIV", MYLITE_SQL_PARSE_DIV},
        {"IGNORE", MYLITE_SQL_PARSE_IGNORE},
        {"EXISTS", MYLITE_SQL_PARSE_EXISTS},
        {"DATABASE", MYLITE_SQL_PARSE_DATABASE},
        {"DATABASES", MYLITE_SQL_PARSE_DATABASES},
        {"DATA", MYLITE_SQL_PARSE_DATA},
        {"DAY", MYLITE_SQL_PARSE_DAY},
        {"DAYNAME", MYLITE_SQL_PARSE_DAYNAME},
        {"DAY_HOUR", MYLITE_SQL_PARSE_DAY_HOUR},
        {"DAY_MICROSECOND", MYLITE_SQL_PARSE_DAY_MICROSECOND},
        {"DAY_MINUTE", MYLITE_SQL_PARSE_DAY_MINUTE},
        {"DAYOFMONTH", MYLITE_SQL_PARSE_DAYOFMONTH},
        {"DAYOFWEEK", MYLITE_SQL_PARSE_DAYOFWEEK},
        {"DAYOFYEAR", MYLITE_SQL_PARSE_DAYOFYEAR},
        {"DAY_SECOND", MYLITE_SQL_PARSE_DAY_SECOND},
        {"ADDDATE", MYLITE_SQL_PARSE_ADDDATE},
        {"ADDTIME", MYLITE_SQL_PARSE_ADDTIME},
        {"DATEDIFF", MYLITE_SQL_PARSE_DATEDIFF},
        {"DATE_ADD", MYLITE_SQL_PARSE_DATE_ADD},
        {"DATE_SUB", MYLITE_SQL_PARSE_DATE_SUB},
        {"DATE_FORMAT", MYLITE_SQL_PARSE_DATE_FORMAT},
        {"EXTRACT", MYLITE_SQL_PARSE_EXTRACT},
        {"FROM_DAYS", MYLITE_SQL_PARSE_FROM_DAYS},
        {"FROM_UNIXTIME", MYLITE_SQL_PARSE_FROM_UNIXTIME},
        {"HOUR_MICROSECOND", MYLITE_SQL_PARSE_HOUR_MICROSECOND},
        {"HOUR_MINUTE", MYLITE_SQL_PARSE_HOUR_MINUTE},
        {"HOUR_SECOND", MYLITE_SQL_PARSE_HOUR_SECOND},
        {"MICROSECOND", MYLITE_SQL_PARSE_MICROSECOND},
        {"MINUTE_MICROSECOND", MYLITE_SQL_PARSE_MINUTE_MICROSECOND},
        {"MINUTE_SECOND", MYLITE_SQL_PARSE_MINUTE_SECOND},
        {"MAKEDATE", MYLITE_SQL_PARSE_MAKEDATE},
        {"MAKETIME", MYLITE_SQL_PARSE_MAKETIME},
        {"MONTHNAME", MYLITE_SQL_PARSE_MONTHNAME},
        {"QUARTER", MYLITE_SQL_PARSE_QUARTER},
        {"SECOND_MICROSECOND", MYLITE_SQL_PARSE_SECOND_MICROSECOND},
        {"SQL_TSI_DAY", MYLITE_SQL_PARSE_SQL_TSI_DAY},
        {"SQL_TSI_HOUR", MYLITE_SQL_PARSE_SQL_TSI_HOUR},
        {"SQL_TSI_MINUTE", MYLITE_SQL_PARSE_SQL_TSI_MINUTE},
        {"SQL_TSI_MONTH", MYLITE_SQL_PARSE_SQL_TSI_MONTH},
        {"SQL_TSI_QUARTER", MYLITE_SQL_PARSE_SQL_TSI_QUARTER},
        {"SQL_TSI_SECOND", MYLITE_SQL_PARSE_SQL_TSI_SECOND},
        {"SQL_TSI_WEEK", MYLITE_SQL_PARSE_SQL_TSI_WEEK},
        {"SQL_TSI_YEAR", MYLITE_SQL_PARSE_SQL_TSI_YEAR},
        {"TIMEDIFF", MYLITE_SQL_PARSE_TIMEDIFF},
        {"TIMESTAMPADD", MYLITE_SQL_PARSE_TIMESTAMPADD},
        {"TIMESTAMPDIFF", MYLITE_SQL_PARSE_TIMESTAMPDIFF},
        {"UNIX_TIMESTAMP", MYLITE_SQL_PARSE_UNIX_TIMESTAMP},
        {"TIME_FORMAT", MYLITE_SQL_PARSE_TIME_FORMAT},
        {"TIME_TO_SEC", MYLITE_SQL_PARSE_TIME_TO_SEC},
        {"TO_DAYS", MYLITE_SQL_PARSE_TO_DAYS},
        {"TO_SECONDS", MYLITE_SQL_PARSE_TO_SECONDS},
        {"SEC_TO_TIME", MYLITE_SQL_PARSE_SEC_TO_TIME},
        {"STR_TO_DATE", MYLITE_SQL_PARSE_STR_TO_DATE},
        {"REGEXP_LIKE", MYLITE_SQL_PARSE_REGEXP_LIKE},
        {"LAST_DAY", MYLITE_SQL_PARSE_LAST_DAY},
        {"WEEK", MYLITE_SQL_PARSE_WEEK},
        {"WEEKDAY", MYLITE_SQL_PARSE_WEEKDAY},
        {"WEEKOFYEAR", MYLITE_SQL_PARSE_WEEKOFYEAR},
        {"YEAR_MONTH", MYLITE_SQL_PARSE_YEAR_MONTH},
        {"YEARWEEK", MYLITE_SQL_PARSE_YEARWEEK},
        {"DROP", MYLITE_SQL_PARSE_DROP},
        {"TRUNCATE", MYLITE_SQL_PARSE_TRUNCATE},
        {"SUBSTR", MYLITE_SQL_PARSE_SUBSTR},
        {"SUBSTRING", MYLITE_SQL_PARSE_SUBSTRING},
        {"SUBSTRING_INDEX", MYLITE_SQL_PARSE_SUBSTRING_INDEX},
        {"STRCMP", MYLITE_SQL_PARSE_STRCMP},
        {"RTRIM", MYLITE_SQL_PARSE_RTRIM},
        {"TRAILING", MYLITE_SQL_PARSE_TRAILING},
        {"TRIM", MYLITE_SQL_PARSE_TRIM},
        {"SUBDATE", MYLITE_SQL_PARSE_SUBDATE},
        {"SUBTIME", MYLITE_SQL_PARSE_SUBTIME},
        {"UCASE", MYLITE_SQL_PARSE_UCASE},
        {"UPPER", MYLITE_SQL_PARSE_UPPER},
        {"SHOW", MYLITE_SQL_PARSE_SHOW},
        {"TABLES", MYLITE_SQL_PARSE_TABLES},
        {"COLUMNS", MYLITE_SQL_PARSE_COLUMNS},
        {"ELT", MYLITE_SQL_PARSE_ELT},
        {"FIELD", MYLITE_SQL_PARSE_FIELD},
        {"FIELDS", MYLITE_SQL_PARSE_FIELDS},
        {"FIND_IN_SET", MYLITE_SQL_PARSE_FIND_IN_SET},
        {"FORMAT", MYLITE_SQL_PARSE_FORMAT},
        {"GREATEST", MYLITE_SQL_PARSE_GREATEST},
        {"INDEX", MYLITE_SQL_PARSE_INDEX},
        {"INDEXES", MYLITE_SQL_PARSE_INDEXES},
        {"LEAST", MYLITE_SQL_PARSE_LEAST},
        {"CONSTRAINT", MYLITE_SQL_PARSE_CONSTRAINT},
        {"FOREIGN", MYLITE_SQL_PARSE_FOREIGN},
        {"KEY", MYLITE_SQL_PARSE_KEY},
        {"KEYS", MYLITE_SQL_PARSE_KEYS},
        {"REFERENCES", MYLITE_SQL_PARSE_REFERENCES},
        {"ACTION", MYLITE_SQL_PARSE_ACTION},
        {"CASCADE", MYLITE_SQL_PARSE_CASCADE},
        {"ENFORCED", MYLITE_SQL_PARSE_ENFORCED},
        {"PRIMARY", MYLITE_SQL_PARSE_PRIMARY},
        {"RESTRICT", MYLITE_SQL_PARSE_RESTRICT},
        {"UNIQUE", MYLITE_SQL_PARSE_UNIQUE},
        {"FULLTEXT", MYLITE_SQL_PARSE_FULLTEXT},
        {"SPATIAL", MYLITE_SQL_PARSE_SPATIAL},
        {"GEOMETRY", MYLITE_SQL_PARSE_GEOMETRY},
        {"GEOMETRYCOLLECTION", MYLITE_SQL_PARSE_GEOMETRYCOLLECTION},
        {"LINESTRING", MYLITE_SQL_PARSE_LINESTRING},
        {"MULTILINESTRING", MYLITE_SQL_PARSE_MULTILINESTRING},
        {"MULTIPOINT", MYLITE_SQL_PARSE_MULTIPOINT},
        {"MULTIPOLYGON", MYLITE_SQL_PARSE_MULTIPOLYGON},
        {"POINT", MYLITE_SQL_PARSE_POINT},
        {"POLYGON", MYLITE_SQL_PARSE_POLYGON},
        {"FULL", MYLITE_SQL_PARSE_FULL},
        {"TRIGGERS", MYLITE_SQL_PARSE_TRIGGERS},
        {"EVENTS", MYLITE_SQL_PARSE_EVENTS},
        {"OPEN", MYLITE_SQL_PARSE_OPEN},
        {"PROCESSLIST", MYLITE_SQL_PARSE_PROCESSLIST},
        {"GRANTS", MYLITE_SQL_PARSE_GRANTS},
        {"WARNINGS", MYLITE_SQL_PARSE_WARNINGS},
        {"ERRORS", MYLITE_SQL_PARSE_ERRORS},
        {"PROCEDURE", MYLITE_SQL_PARSE_PROCEDURE},
        {"FUNCTION", MYLITE_SQL_PARSE_FUNCTION},
        {"ENGINE", MYLITE_SQL_PARSE_ENGINE},
        {"ENGINES", MYLITE_SQL_PARSE_ENGINES},
        {"PLUGINS", MYLITE_SQL_PARSE_PLUGINS},
        {"PRIVILEGES", MYLITE_SQL_PARSE_PRIVILEGES},
        {"ENUM", MYLITE_SQL_PARSE_ENUM},
        {"COMMENT", MYLITE_SQL_PARSE_COMMENT},
        {"STATUS", MYLITE_SQL_PARSE_STATUS},
        {"STORAGE", MYLITE_SQL_PARSE_STORAGE},
        {"VARIABLES", MYLITE_SQL_PARSE_VARIABLES},
        {"DEFAULT", MYLITE_SQL_PARSE_DEFAULT},
        {"CHAR", MYLITE_SQL_PARSE_CHAR},
        {"CHARACTER", MYLITE_SQL_PARSE_CHARACTER},
        {"CHARACTER_LENGTH", MYLITE_SQL_PARSE_CHARACTER_LENGTH},
        {"CHAR_LENGTH", MYLITE_SQL_PARSE_CHAR_LENGTH},
        {"CHARSET", MYLITE_SQL_PARSE_CHARSET},
        {"COLLATE", MYLITE_SQL_PARSE_COLLATE},
        {"COLLATION", MYLITE_SQL_PARSE_COLLATION},
        {"LIKE", MYLITE_SQL_PARSE_LIKE},
        {"REGEXP", MYLITE_SQL_PARSE_REGEXP},
        {"RLIKE", MYLITE_SQL_PARSE_RLIKE},
        {"SCHEMA", MYLITE_SQL_PARSE_SCHEMA},
        {"SCHEMAS", MYLITE_SQL_PARSE_SCHEMAS},
        {"DESCRIBE", MYLITE_SQL_PARSE_DESCRIBE},
        {"EXPLAIN", MYLITE_SQL_PARSE_EXPLAIN},
        {"SESSION_USER", MYLITE_SQL_PARSE_SESSION_USER},
        {"RENAME", MYLITE_SQL_PARSE_RENAME},
        {"ADD", MYLITE_SQL_PARSE_ADD},
        {"AFTER", MYLITE_SQL_PARSE_AFTER},
        {"MODIFY", MYLITE_SQL_PARSE_MODIFY},
        {"CHANGE", MYLITE_SQL_PARSE_CHANGE},
        {"COLUMN", MYLITE_SQL_PARSE_COLUMN},
        {"FIRST", MYLITE_SQL_PARSE_FIRST},
        {"FOR", MYLITE_SQL_PARSE_FOR},
        {"FORCE", MYLITE_SQL_PARSE_FORCE},
        {"INSERT", MYLITE_SQL_PARSE_INSERT},
        {"INFILE", MYLITE_SQL_PARSE_INFILE},
        {"INNER", MYLITE_SQL_PARSE_INNER},
        {"JOIN", MYLITE_SQL_PARSE_JOIN},
        {"LEFT", MYLITE_SQL_PARSE_LEFT},
        {"OUTER", MYLITE_SQL_PARSE_OUTER},
        {"REPLACE", MYLITE_SQL_PARSE_REPLACE},
        {"LOW_PRIORITY", MYLITE_SQL_PARSE_LOW_PRIORITY},
        {"HIGH_PRIORITY", MYLITE_SQL_PARSE_HIGH_PRIORITY},
        {"DELAYED", MYLITE_SQL_PARSE_DELAYED},
        {"INTO", MYLITE_SQL_PARSE_INTO},
        {"LOCK", MYLITE_SQL_PARSE_LOCK},
        {"LOAD", MYLITE_SQL_PARSE_LOAD},
        {"MODE", MYLITE_SQL_PARSE_MODE},
        {"READ", MYLITE_SQL_PARSE_READ},
        {"COMMITTED", MYLITE_SQL_PARSE_COMMITTED},
        {"ISOLATION", MYLITE_SQL_PARSE_ISOLATION},
        {"LEVEL", MYLITE_SQL_PARSE_LEVEL},
        {"ONLY", MYLITE_SQL_PARSE_ONLY},
        {"REPEATABLE", MYLITE_SQL_PARSE_REPEATABLE},
        {"SERIALIZABLE", MYLITE_SQL_PARSE_SERIALIZABLE},
        {"UNCOMMITTED", MYLITE_SQL_PARSE_UNCOMMITTED},
        {"ROW", MYLITE_SQL_PARSE_ROW},
        {"VALUE", MYLITE_SQL_PARSE_VALUE},
        {"VALUES", MYLITE_SQL_PARSE_VALUES},
        {"DUPLICATE", MYLITE_SQL_PARSE_DUPLICATE},
        {"TO", MYLITE_SQL_PARSE_TO},
        {"DELETE", MYLITE_SQL_PARSE_DELETE},
        {"DEALLOCATE", MYLITE_SQL_PARSE_DEALLOCATE},
        {"DO", MYLITE_SQL_PARSE_DO},
        {"EXECUTE", MYLITE_SQL_PARSE_EXECUTE},
        {"PREPARE", MYLITE_SQL_PARSE_PREPARE},
        {"UPDATE", MYLITE_SQL_PARSE_UPDATE},
        {"START", MYLITE_SQL_PARSE_START},
        {"TRANSACTION", MYLITE_SQL_PARSE_TRANSACTION},
        {"WITH", MYLITE_SQL_PARSE_WITH},
        {"CONSISTENT", MYLITE_SQL_PARSE_CONSISTENT},
        {"SNAPSHOT", MYLITE_SQL_PARSE_SNAPSHOT},
        {"BEGIN", MYLITE_SQL_PARSE_BEGIN},
        {"WORK", MYLITE_SQL_PARSE_WORK},
        {"COMMIT", MYLITE_SQL_PARSE_COMMIT},
        {"ROLLBACK", MYLITE_SQL_PARSE_ROLLBACK},
        {"SAVEPOINT", MYLITE_SQL_PARSE_SAVEPOINT},
        {"RELEASE", MYLITE_SQL_PARSE_RELEASE},
        {"UNLOCK", MYLITE_SQL_PARSE_UNLOCK},
        {"WRITE", MYLITE_SQL_PARSE_WRITE},
        {"ANALYZE", MYLITE_SQL_PARSE_ANALYZE},
        {"CHECK", MYLITE_SQL_PARSE_CHECK},
        {"OPTIMIZE", MYLITE_SQL_PARSE_OPTIMIZE},
        {"REPAIR", MYLITE_SQL_PARSE_REPAIR},
        {"NO_WRITE_TO_BINLOG", MYLITE_SQL_PARSE_NO_WRITE_TO_BINLOG},
        {"QUICK", MYLITE_SQL_PARSE_QUICK},
        {"FAST", MYLITE_SQL_PARSE_FAST},
        {"MEDIUM", MYLITE_SQL_PARSE_MEDIUM},
        {"EXTENDED", MYLITE_SQL_PARSE_EXTENDED},
        {"CHANGED", MYLITE_SQL_PARSE_CHANGED},
        {"UPGRADE", MYLITE_SQL_PARSE_UPGRADE},
        {"USE_FRM", MYLITE_SQL_PARSE_USE_FRM},
        {"SET", MYLITE_SQL_PARSE_SET},
        {"SESSION", MYLITE_SQL_PARSE_SESSION},
        {"LOCAL", MYLITE_SQL_PARSE_LOCAL},
        {"LINES", MYLITE_SQL_PARSE_LINES},
        {"LOCALTIME", MYLITE_SQL_PARSE_LOCALTIME},
        {"LOCALTIMESTAMP", MYLITE_SQL_PARSE_LOCALTIMESTAMP},
        {"GLOBAL", MYLITE_SQL_PARSE_GLOBAL},
        {"SYSTEM", MYLITE_SQL_PARSE_SYSTEM},
        {"ON", MYLITE_SQL_PARSE_ON},
        {"NO", MYLITE_SQL_PARSE_NO},
        {"OFF", MYLITE_SQL_PARSE_OFF},
        {"NAMES", MYLITE_SQL_PARSE_NAMES},
        {"NATIONAL", MYLITE_SQL_PARSE_NATIONAL},
        {"NCHAR", MYLITE_SQL_PARSE_NCHAR},
        {"INT", MYLITE_SQL_PARSE_INT},
        {"TINYINT", MYLITE_SQL_PARSE_TINYINT},
        {"SMALLINT", MYLITE_SQL_PARSE_SMALLINT},
        {"MEDIUMINT", MYLITE_SQL_PARSE_MEDIUMINT},
        {"INTEGER", MYLITE_SQL_PARSE_INTEGER_TYPE},
        {"BIGINT", MYLITE_SQL_PARSE_BIGINT},
        {"DECIMAL", MYLITE_SQL_PARSE_DECIMAL_TYPE},
        {"DEC", MYLITE_SQL_PARSE_DEC},
        {"NUMERIC", MYLITE_SQL_PARSE_NUMERIC},
        {"FIXED", MYLITE_SQL_PARSE_FIXED},
        {"ROW_FORMAT", MYLITE_SQL_PARSE_ROW_FORMAT},
        {"KEY_BLOCK_SIZE", MYLITE_SQL_PARSE_KEY_BLOCK_SIZE},
        {"PACK_KEYS", MYLITE_SQL_PARSE_PACK_KEYS},
        {"DISABLE", MYLITE_SQL_PARSE_DISABLE},
        {"ENABLE", MYLITE_SQL_PARSE_ENABLE},
        {"CHECKSUM", MYLITE_SQL_PARSE_CHECKSUM},
        {"STATS_PERSISTENT", MYLITE_SQL_PARSE_STATS_PERSISTENT},
        {"STATS_AUTO_RECALC", MYLITE_SQL_PARSE_STATS_AUTO_RECALC},
        {"STATS_SAMPLE_PAGES", MYLITE_SQL_PARSE_STATS_SAMPLE_PAGES},
        {"DYNAMIC", MYLITE_SQL_PARSE_DYNAMIC},
        {"COMPACT", MYLITE_SQL_PARSE_COMPACT},
        {"REDUNDANT", MYLITE_SQL_PARSE_REDUNDANT},
        {"COMPRESSED", MYLITE_SQL_PARSE_COMPRESSED},
        {"FLOAT", MYLITE_SQL_PARSE_FLOAT_TYPE},
        {"FLOAT4", MYLITE_SQL_PARSE_FLOAT4},
        {"FLOAT8", MYLITE_SQL_PARSE_FLOAT8},
        {"DOUBLE", MYLITE_SQL_PARSE_DOUBLE},
        {"PRECISION", MYLITE_SQL_PARSE_PRECISION},
        {"REAL", MYLITE_SQL_PARSE_REAL},
        {"DATE", MYLITE_SQL_PARSE_DATE},
        {"DATETIME", MYLITE_SQL_PARSE_DATETIME},
        {"HOUR", MYLITE_SQL_PARSE_HOUR},
        {"INTERVAL", MYLITE_SQL_PARSE_INTERVAL},
        {"SECOND", MYLITE_SQL_PARSE_SECOND},
        {"TIME", MYLITE_SQL_PARSE_TIME},
        {"TIMESTAMP", MYLITE_SQL_PARSE_TIMESTAMP},
        {"YEAR", MYLITE_SQL_PARSE_YEAR},
        {"VARCHAR", MYLITE_SQL_PARSE_VARCHAR},
        {"NVARCHAR", MYLITE_SQL_PARSE_NVARCHAR},
        {"VARBINARY", MYLITE_SQL_PARSE_VARBINARY},
        {"BYTE", MYLITE_SQL_PARSE_BYTE},
        {"TINYBLOB", MYLITE_SQL_PARSE_TINYBLOB},
        {"BLOB", MYLITE_SQL_PARSE_BLOB},
        {"MEDIUMBLOB", MYLITE_SQL_PARSE_MEDIUMBLOB},
        {"LONGBLOB", MYLITE_SQL_PARSE_LONGBLOB},
        {"LONG", MYLITE_SQL_PARSE_LONG},
        {"VARYING", MYLITE_SQL_PARSE_VARYING},
        {"TINYTEXT", MYLITE_SQL_PARSE_TINYTEXT},
        {"TEXT", MYLITE_SQL_PARSE_TEXT},
        {"MEDIUMTEXT", MYLITE_SQL_PARSE_MEDIUMTEXT},
        {"LONGTEXT", MYLITE_SQL_PARSE_LONGTEXT},
        {"JSON", MYLITE_SQL_PARSE_JSON},
        {"JSON_ARRAY", MYLITE_SQL_PARSE_JSON_ARRAY},
        {"JSON_CONTAINS", MYLITE_SQL_PARSE_JSON_CONTAINS},
        {"JSON_CONTAINS_PATH", MYLITE_SQL_PARSE_JSON_CONTAINS_PATH},
        {"JSON_EXTRACT", MYLITE_SQL_PARSE_JSON_EXTRACT},
        {"JSON_INSERT", MYLITE_SQL_PARSE_JSON_INSERT},
        {"JSON_KEYS", MYLITE_SQL_PARSE_JSON_KEYS},
        {"JSON_LENGTH", MYLITE_SQL_PARSE_JSON_LENGTH},
        {"JSON_OBJECT", MYLITE_SQL_PARSE_JSON_OBJECT},
        {"JSON_QUOTE", MYLITE_SQL_PARSE_JSON_QUOTE},
        {"JSON_REMOVE", MYLITE_SQL_PARSE_JSON_REMOVE},
        {"JSON_REPLACE", MYLITE_SQL_PARSE_JSON_REPLACE},
        {"JSON_SET", MYLITE_SQL_PARSE_JSON_SET},
        {"JSON_TYPE", MYLITE_SQL_PARSE_JSON_TYPE},
        {"JSON_UNQUOTE", MYLITE_SQL_PARSE_JSON_UNQUOTE},
        {"JSON_VALUE", MYLITE_SQL_PARSE_JSON_VALUE},
        {"JSON_VALID", MYLITE_SQL_PARSE_JSON_VALID},
        {"BOOL", MYLITE_SQL_PARSE_BOOL},
        {"BOOLEAN", MYLITE_SQL_PARSE_BOOLEAN},
        {"INVISIBLE", MYLITE_SQL_PARSE_INVISIBLE},
        {"VISIBLE", MYLITE_SQL_PARSE_VISIBLE},
        {"INT1", MYLITE_SQL_PARSE_INT1},
        {"INT2", MYLITE_SQL_PARSE_INT2},
        {"INT3", MYLITE_SQL_PARSE_INT3},
        {"INT4", MYLITE_SQL_PARSE_INT4},
        {"INT8", MYLITE_SQL_PARSE_INT8},
        {"SIGNED", MYLITE_SQL_PARSE_SIGNED},
        {"UNSIGNED", MYLITE_SQL_PARSE_UNSIGNED},
        {"NOT", MYLITE_SQL_PARSE_NOT},
        {"NOW", MYLITE_SQL_PARSE_NOW},
        {"IS", MYLITE_SQL_PARSE_IS},
        {"IN", MYLITE_SQL_PARSE_IN},
        {"TRUE", MYLITE_SQL_PARSE_TRUE},
        {"FALSE", MYLITE_SQL_PARSE_FALSE},
        {"FOUND_ROWS", MYLITE_SQL_PARSE_FOUND_ROWS},
        {"UNKNOWN", MYLITE_SQL_PARSE_UNKNOWN},
        {"NULL", MYLITE_SQL_PARSE_NULL},
        {"DUAL", MYLITE_SQL_PARSE_DUAL},
        {"USER", MYLITE_SQL_PARSE_USER},
        {"UTC", MYLITE_SQL_PARSE_UTC},
        {"VERSION", MYLITE_SQL_PARSE_VERSION},
        {"ROW_COUNT", MYLITE_SQL_PARSE_ROW_COUNT},
        {"SEPARATOR", MYLITE_SQL_PARSE_SEPARATOR},
        {"SERIAL", MYLITE_SQL_PARSE_SERIAL},
        {"SHARE", MYLITE_SQL_PARSE_SHARE},
        {"SQL_CALC_FOUND_ROWS", MYLITE_SQL_PARSE_SQL_CALC_FOUND_ROWS},
        {"SQL_BIG_RESULT", MYLITE_SQL_PARSE_SQL_BIG_RESULT},
        {"SQL_SMALL_RESULT", MYLITE_SQL_PARSE_SQL_SMALL_RESULT},
        {"STRAIGHT_JOIN", MYLITE_SQL_PARSE_STRAIGHT_JOIN},
        {"SYSTEM_USER", MYLITE_SQL_PARSE_SYSTEM_USER},
    };

    if (previous_token_was_dot) {
        *out_parser_token = MYLITE_SQL_PARSE_IDENTIFIER;
        return true;
    }

    if (previous_token_allows_select_noop_modifier(previous_parser_token)) {
        if (token_text_equals(token, "SQL_BUFFER_RESULT")) {
            *out_parser_token = MYLITE_SQL_PARSE_SQL_BUFFER_RESULT;
            return true;
        }
        if (token_text_equals(token, "SQL_NO_CACHE")) {
            *out_parser_token = MYLITE_SQL_PARSE_SQL_NO_CACHE;
            return true;
        }
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

static bool map_operator_token(
    const struct mylite_sql_parser_state *state,
    const struct mylite_sql_token *token,
    int *out_parser_token
) {
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
    case MYLITE_SQL_OPERATOR_PERCENT:
        *out_parser_token = MYLITE_SQL_PARSE_PERCENT;
        return true;
    case MYLITE_SQL_OPERATOR_LOGICAL_AND:
        *out_parser_token = MYLITE_SQL_PARSE_LOGICAL_AND;
        return true;
    case MYLITE_SQL_OPERATOR_LOGICAL_OR:
        if (parser_sql_mode_has(state, MYLITE_SQL_MODE_PIPES_AS_CONCAT)) {
            *out_parser_token = MYLITE_SQL_PARSE_CONCAT_OPERATOR;
        } else {
            *out_parser_token = MYLITE_SQL_PARSE_LOGICAL_OR;
        }
        return true;
    case MYLITE_SQL_OPERATOR_LEFT_SHIFT:
        *out_parser_token = MYLITE_SQL_PARSE_LEFT_SHIFT;
        return true;
    case MYLITE_SQL_OPERATOR_RIGHT_SHIFT:
        *out_parser_token = MYLITE_SQL_PARSE_RIGHT_SHIFT;
        return true;
    case MYLITE_SQL_OPERATOR_BITWISE_NOT:
        *out_parser_token = MYLITE_SQL_PARSE_BITWISE_NOT;
        return true;
    case MYLITE_SQL_OPERATOR_BITWISE_XOR:
        *out_parser_token = MYLITE_SQL_PARSE_BITWISE_XOR;
        return true;
    case MYLITE_SQL_OPERATOR_BITWISE_AND:
        *out_parser_token = MYLITE_SQL_PARSE_BITWISE_AND;
        return true;
    case MYLITE_SQL_OPERATOR_BITWISE_OR:
        *out_parser_token = MYLITE_SQL_PARSE_BITWISE_OR;
        return true;
    case MYLITE_SQL_OPERATOR_JSON_UNQUOTE_EXTRACT:
        *out_parser_token = MYLITE_SQL_PARSE_JSON_UNQUOTE_EXTRACT_OPERATOR;
        return true;
    case MYLITE_SQL_OPERATOR_JSON_EXTRACT:
        *out_parser_token = MYLITE_SQL_PARSE_JSON_EXTRACT_OPERATOR;
        return true;
    case MYLITE_SQL_OPERATOR_ASSIGN:
        *out_parser_token = MYLITE_SQL_PARSE_ASSIGN;
        return true;
    case MYLITE_SQL_OPERATOR_NONE:
    case MYLITE_SQL_OPERATOR_NOT:
        return false;
    }

    return false;
}

static bool previous_token_allows_select_noop_modifier(int previous_parser_token) {
    switch (previous_parser_token) {
    case MYLITE_SQL_PARSE_SELECT:
    case MYLITE_SQL_PARSE_ALL:
    case MYLITE_SQL_PARSE_DISTINCT:
    case MYLITE_SQL_PARSE_DISTINCTROW:
    case MYLITE_SQL_PARSE_HIGH_PRIORITY:
    case MYLITE_SQL_PARSE_STRAIGHT_JOIN:
    case MYLITE_SQL_PARSE_SQL_SMALL_RESULT:
    case MYLITE_SQL_PARSE_SQL_BIG_RESULT:
    case MYLITE_SQL_PARSE_SQL_BUFFER_RESULT:
    case MYLITE_SQL_PARSE_SQL_NO_CACHE:
    case MYLITE_SQL_PARSE_SQL_CALC_FOUND_ROWS:
        return true;
    default:
        return false;
    }
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

static bool parser_sql_mode_has(
    const struct mylite_sql_parser_state *state,
    enum mylite_sql_mode mode
) {
    if (state == NULL) {
        return false;
    }
    return (state->modes & (unsigned int)mode) != 0U;
}

static bool create_table_name_is_no_space_function_identifier(
    const struct mylite_sql_parser_state *state,
    const struct mylite_sql_ast_node *table_name,
    const struct mylite_sql_token *left_paren
) {
    const struct mylite_sql_ast_node *last_identifier = NULL;

    if (parser_sql_mode_has(state, MYLITE_SQL_MODE_IGNORE_SPACE) || table_name == NULL ||
        left_paren == NULL) {
        return false;
    }
    last_identifier = last_identifier_component(table_name);
    if (last_identifier == NULL) {
        return false;
    }
    if (left_paren->offset != last_identifier->span.offset + last_identifier->span.length) {
        return false;
    }
    return span_text_matches_ignore_space_function_name(&last_identifier->span);
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

static const struct mylite_sql_ast_node *last_identifier_component(
    const struct mylite_sql_ast_node *identifier
) {
    const struct mylite_sql_ast_node *current = identifier;

    while (current != NULL && current->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        current = current->last_child;
    }
    if (current != NULL && current->kind == MYLITE_SQL_AST_IDENTIFIER) {
        return current;
    }
    return NULL;
}

static bool span_text_equals(const struct mylite_sql_source_span *span, const char *text) {
    size_t length = strlen(text);

    if (span == NULL || span->text == NULL || span->length != length) {
        return false;
    }

    for (size_t index = 0U; index < length; ++index) {
        if (ascii_upper((unsigned char)span->text[index]) != text[index]) {
            return false;
        }
    }

    return true;
}

static bool span_text_matches_ignore_space_function_name(
    const struct mylite_sql_source_span *span
) {
    static const char *const function_names[] = {
        "BIT_AND",
        "BIT_OR",
        "BIT_XOR",
        "CAST",
        "CONVERT",
        "COUNT",
        "CURDATE",
        "CURTIME",
        "DATE_ADD",
        "DATE_SUB",
        "GROUP_CONCAT",
        "MAX",
        "MIN",
        "SESSION_USER",
        "SUM",
        "SYSTEM_USER",
    };

    for (size_t index = 0U; index < sizeof(function_names) / sizeof(function_names[0]); ++index) {
        if (span_text_equals(span, function_names[index])) {
            return true;
        }
    }
    return false;
}
