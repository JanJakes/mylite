#include "mylite_parser.h"

#include "mylite_parse.h"
#include "mylite_parser_driver.h"
#include "mylite_parser_helpers.h"
#include "mylite_parser_internal.h"
#include "mylite_parser_placeholders.h"
#include "mylite_parser_token_map.h"
#include "mylite_version_comment.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct parenthesized_row_constructor_injection {
    bool enabled;
    const struct mylite_sql_lexer *lexer;
    const struct mylite_sql_token *left_paren;
    const struct mylite_sql_token *previous_token;
    bool has_previous_token;
};

struct mylite_sql_parser_feed_context {
    void *parser;
    struct mylite_sql_parser_state *state;
    struct mylite_sql_parser_token_history *token_history;
    bool *previous_token_was_dot;
    struct mylite_sql_token *previous_token;
    bool *has_previous_token;
    bool inject_parenthesized_row_constructors;
};

struct mylite_sql_parse_error {
    enum mylite_sql_parse_status status;
    int parser_token;
    struct mylite_sql_token token;
};

typedef enum mylite_sql_parse_status (*mylite_sql_parse_retry_callback)(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    bool *out_handled
);

static enum mylite_sql_parse_status feed_lexer_tokens(
    struct mylite_sql_parser_feed_context *context,
    struct mylite_sql_lexer *lexer,
    bool feed_eof
);
static enum mylite_sql_parse_status feed_lexer_token(
    struct mylite_sql_parser_feed_context *context,
    struct mylite_sql_lexer *lexer,
    struct mylite_sql_token *token
);
static bool rebase_token_to_root_input(
    struct mylite_sql_token *token,
    const char *root_input,
    size_t root_length
);
static bool feed_parenthesized_row_constructor_if_needed(
    void *parser,
    struct mylite_sql_parser_state *state,
    struct mylite_sql_parser_token_history *history,
    bool *previous_token_was_dot,
    const struct parenthesized_row_constructor_injection *injection
);
static bool should_inject_parenthesized_row_constructor(
    const struct parenthesized_row_constructor_injection *injection
);
static bool token_can_name_immediate_function(const struct mylite_sql_token *token);
static bool lexer_parenthesized_expression_has_top_level_comma(
    const struct mylite_sql_lexer *lexer
);
static struct mylite_sql_token make_synthetic_row_constructor_token(
    const struct mylite_sql_token *left_paren
);
static bool parse_result_is_unsupported_utility_script(
    const struct mylite_sql_parse_result *result
);
static enum mylite_sql_parse_status retry_unsupported_utility_parse(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    enum mylite_sql_parse_status status,
    mylite_sql_parse_retry_callback callback
);
static enum mylite_sql_parse_status retry_syntax_error_parse(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    enum mylite_sql_parse_status status,
    mylite_sql_parse_retry_callback callback
);
static enum mylite_sql_parse_status retry_parse_with_callback(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    enum mylite_sql_parse_status status,
    mylite_sql_parse_retry_callback callback
);
static void record_parse_error(
    struct mylite_sql_parse_result *result,
    struct mylite_sql_parse_error error
);

enum mylite_sql_parse_status mylite_sql_parse(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *out_result
) {
    enum mylite_sql_parse_status status = mylite_sql_parser_parse_with_lemon(config, out_result);

    status = retry_unsupported_utility_parse(
        config,
        out_result,
        status,
        mylite_sql_parser_try_parse_parenthesized_row_arithmetic_predicate_statement
    );
    status = retry_unsupported_utility_parse(
        config,
        out_result,
        status,
        mylite_sql_parser_try_parse_row_constructor_predicate_statement
    );
    status = retry_syntax_error_parse(
        config,
        out_result,
        status,
        mylite_sql_parser_try_parse_row_constructor_predicate_statement
    );
    status = retry_syntax_error_parse(
        config,
        out_result,
        status,
        mylite_sql_parser_try_parse_select_result_option_before_duplicate_statement
    );
    status = retry_syntax_error_parse(
        config,
        out_result,
        status,
        mylite_sql_parser_try_parse_parenthesized_row_constructor_statement
    );
    status = retry_syntax_error_parse(
        config,
        out_result,
        status,
        mylite_sql_parser_try_parse_parenthesized_row_arithmetic_predicate_statement
    );
    status = retry_syntax_error_parse(
        config,
        out_result,
        status,
        mylite_sql_parser_try_parse_tableless_select_limit_statement
    );
    status = retry_syntax_error_parse(
        config,
        out_result,
        status,
        mylite_sql_parser_try_parse_repeated_select_locking_statement
    );
    status = retry_syntax_error_parse(
        config,
        out_result,
        status,
        mylite_sql_parser_try_parse_legacy_create_index_type_statement
    );
    status = retry_syntax_error_parse(
        config,
        out_result,
        status,
        mylite_sql_parser_try_parse_placeholder_statement
    );

    if (status == MYLITE_SQL_PARSE_OK &&
        !mylite_sql_ast_spans_are_within_source(&out_result->ast, config.input, config.length)) {
        out_result->status = MYLITE_SQL_PARSE_SYNTAX_ERROR;
        status = out_result->status;
    }

    return status;
}

static enum mylite_sql_parse_status retry_unsupported_utility_parse(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    enum mylite_sql_parse_status status,
    mylite_sql_parse_retry_callback callback
) {
    if (status != MYLITE_SQL_PARSE_OK || !parse_result_is_unsupported_utility_script(result)) {
        return status;
    }
    return retry_parse_with_callback(config, result, status, callback);
}

static enum mylite_sql_parse_status retry_syntax_error_parse(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    enum mylite_sql_parse_status status,
    mylite_sql_parse_retry_callback callback
) {
    if (status != MYLITE_SQL_PARSE_SYNTAX_ERROR) {
        return status;
    }
    return retry_parse_with_callback(config, result, status, callback);
}

static enum mylite_sql_parse_status retry_parse_with_callback(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *result,
    enum mylite_sql_parse_status status,
    mylite_sql_parse_retry_callback callback
) {
    bool handled = false;
    size_t retry_callback_count = result->retry_callback_count + 1U;
    size_t retry_handled_count = result->retry_handled_count;
    enum mylite_sql_parse_status retry_status = callback(config, result, &handled);

    result->retry_callback_count = retry_callback_count;
    result->retry_handled_count = retry_handled_count + (handled ? 1U : 0U);
    if (!handled) {
        return status;
    }
    result->status = retry_status;
    return retry_status;
}

enum mylite_sql_parse_status mylite_sql_parser_parse_with_lemon(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *out_result
) {
    return mylite_sql_parser_parse_with_lemon_options(config, out_result, false);
}

enum mylite_sql_parse_status mylite_sql_parser_parse_with_lemon_options(
    struct mylite_sql_parse_config config,
    struct mylite_sql_parse_result *out_result,
    bool inject_parenthesized_row_constructors
) {
    struct mylite_sql_parser_state state;
    struct mylite_sql_lexer lexer;
    void *parser = NULL;
    bool previous_token_was_dot = false;
    struct mylite_sql_parser_token_history token_history = {0};
    struct mylite_sql_token previous_token = {0};
    bool has_previous_token = false;
    enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;

    if (out_result == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }

    mylite_sql_parser_reset_parse_result(out_result);

    if (config.input == NULL && config.length != 0U) {
        out_result->status = MYLITE_SQL_PARSE_MISUSE;
        return out_result->status;
    }

    state = (struct mylite_sql_parser_state){
        .result = out_result,
        .next_parameter_index = 0U,
        .modes = config.modes,
        .allow_parameters = config.allow_parameters,
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
    status = feed_lexer_tokens(
        &(struct mylite_sql_parser_feed_context){
            .parser = parser,
            .state = &state,
            .token_history = &token_history,
            .previous_token_was_dot = &previous_token_was_dot,
            .previous_token = &previous_token,
            .has_previous_token = &has_previous_token,
            .inject_parenthesized_row_constructors = inject_parenthesized_row_constructors,
        },
        &lexer,
        true
    );

    mylite_sql_lemonFree(parser, free);

    if (out_result->status == MYLITE_SQL_PARSE_OK && status != MYLITE_SQL_PARSE_OK) {
        out_result->status = status;
    }
    if (out_result->status == MYLITE_SQL_PARSE_OK && !state.accepted) {
        out_result->status = MYLITE_SQL_PARSE_SYNTAX_ERROR;
    }
    if (out_result->status == MYLITE_SQL_PARSE_OK) {
        out_result->parameter_count = state.next_parameter_index;
    }

    return out_result->status;
}

void mylite_sql_parser_reset_parse_result(struct mylite_sql_parse_result *out_result) {
    out_result->root = NULL;
    out_result->parameter_count = 0U;
    out_result->retry_callback_count = 0U;
    out_result->retry_handled_count = 0U;
    out_result->status = MYLITE_SQL_PARSE_OK;
    out_result->error_token = (struct mylite_sql_token){0};
    out_result->parser_token = 0;
    mylite_sql_ast_init(&out_result->ast);
}

static enum mylite_sql_parse_status feed_lexer_tokens(
    struct mylite_sql_parser_feed_context *context,
    struct mylite_sql_lexer *lexer,
    bool feed_eof
) {
    struct mylite_sql_lexer lexer_stack[mylite_sql_parser_version_comment_lexer_stack_limit];
    const char *root_input = NULL;
    size_t root_length = 0U;
    size_t lexer_count = 1U;

    if (context == NULL || lexer == NULL || context->state == NULL ||
        context->state->result == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }

    root_input = lexer->input;
    root_length = lexer->length;
    lexer_stack[0] = *lexer;
    for (;;) {
        struct mylite_sql_token token;
        enum mylite_sql_parse_status status;
        struct mylite_sql_lexer *current_lexer = &lexer_stack[lexer_count - 1U];

        if (mylite_sql_lexer_next(current_lexer, &token) != 0) {
            return MYLITE_SQL_PARSE_MISUSE;
        }
        if (!rebase_token_to_root_input(&token, root_input, root_length)) {
            return MYLITE_SQL_PARSE_SYNTAX_ERROR;
        }
        if (token.kind == MYLITE_SQL_TOKEN_EOF) {
            if (lexer_count > 1U) {
                --lexer_count;
                continue;
            }
            if (!feed_eof) {
                return MYLITE_SQL_PARSE_OK;
            }
        } else if (token.kind == MYLITE_SQL_TOKEN_VERSION_COMMENT) {
            status = mylite_sql_parser_push_version_comment_payload_lexer(
                &token,
                context->state->modes,
                lexer_stack,
                &lexer_count
            );
            if (status != MYLITE_SQL_PARSE_OK) {
                return status;
            }
            continue;
        }

        status = feed_lexer_token(context, current_lexer, &token);
        if (status != MYLITE_SQL_PARSE_OK || token.kind == MYLITE_SQL_TOKEN_EOF) {
            return status;
        }
    }
}

static bool rebase_token_to_root_input(
    struct mylite_sql_token *token,
    const char *root_input,
    size_t root_length
) {
    uintptr_t root_address = 0U;
    uintptr_t token_address = 0U;
    size_t root_offset = 0U;

    if (token == NULL || token->text == NULL || root_input == NULL) {
        return token != NULL && token->text == NULL && root_input == NULL && root_length == 0U;
    }

    root_address = (uintptr_t)root_input;
    token_address = (uintptr_t)token->text;
    if (token_address < root_address) {
        return false;
    }
    root_offset = (size_t)(token_address - root_address);
    if (root_offset > root_length || token->length > root_length - root_offset) {
        return false;
    }

    token->offset = root_offset;
    token->source_length = root_length;
    return true;
}

static enum mylite_sql_parse_status feed_lexer_token(
    struct mylite_sql_parser_feed_context *context,
    struct mylite_sql_lexer *lexer,
    struct mylite_sql_token *token
) {
    struct mylite_sql_parser_token_map token_map;

    if (context == NULL || context->parser == NULL || context->state == NULL ||
        context->state->result == NULL || context->token_history == NULL ||
        context->previous_token_was_dot == NULL || context->previous_token == NULL ||
        context->has_previous_token == NULL || lexer == NULL || token == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }

    if (mylite_sql_parser_token_is_comment(token->kind)) {
        return MYLITE_SQL_PARSE_OK;
    }

    if (token->kind == MYLITE_SQL_TOKEN_ERROR) {
        record_parse_error(
            context->state->result,
            (struct mylite_sql_parse_error){
                .status = MYLITE_SQL_PARSE_LEXER_ERROR,
                .parser_token = 0,
                .token = *token,
            }
        );
        return context->state->result->status;
    }

    /* Lock targets do not affect MyLite's embedded no-op locking behavior. */
    if (mylite_sql_parser_should_skip_select_lock_target_list(token, context->token_history)) {
        enum mylite_sql_parse_status status =
            mylite_sql_parser_skip_select_lock_target_list(lexer, token);

        if (status != MYLITE_SQL_PARSE_OK) {
            record_parse_error(
                context->state->result,
                (struct mylite_sql_parse_error){
                    .status = status,
                    .parser_token = 0,
                    .token = *token,
                }
            );
            return context->state->result->status;
        }
    }

    if (!feed_parenthesized_row_constructor_if_needed(
            context->parser,
            context->state,
            context->token_history,
            context->previous_token_was_dot,
            &(struct parenthesized_row_constructor_injection){
                .enabled = context->inject_parenthesized_row_constructors,
                .lexer = lexer,
                .left_paren = token,
                .previous_token = context->previous_token,
                .has_previous_token = *context->has_previous_token,
            }
        )) {
        return context->state->result->status;
    }

    if (!mylite_sql_parser_map_lexer_token(
            context->state,
            lexer,
            token,
            *context->previous_token_was_dot,
            context->token_history,
            &token_map
        )) {
        record_parse_error(
            context->state->result,
            (struct mylite_sql_parse_error){
                .status = MYLITE_SQL_PARSE_SYNTAX_ERROR,
                .parser_token = -1,
                .token = *token,
            }
        );
        return context->state->result->status;
    }

    mylite_sql_lemon(context->parser, token_map.parser_token, *token, context->state);
    *context->previous_token_was_dot = token_map.previous_token_was_dot;
    mylite_sql_parser_update_token_history(context->token_history, token_map.parser_token);
    if (context->inject_parenthesized_row_constructors) {
        *context->previous_token = *token;
        *context->has_previous_token = token->kind != MYLITE_SQL_TOKEN_EOF;
    }

    return context->state->result->status;
}

enum mylite_sql_parse_status mylite_sql_parser_push_version_comment_payload_lexer(
    const struct mylite_sql_token *token,
    unsigned int modes,
    struct mylite_sql_lexer *lexer_stack,
    size_t *lexer_count
) {
    struct mylite_sql_version_comment_payload payload;

    if (lexer_stack == NULL || lexer_count == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    if (token == NULL || token->kind != MYLITE_SQL_TOKEN_VERSION_COMMENT ||
        !mylite_sql_version_comment_parse(token->text, token->length, &payload)) {
        return MYLITE_SQL_PARSE_SYNTAX_ERROR;
    }
    if (!payload.active || payload.length == 0U) {
        return MYLITE_SQL_PARSE_OK;
    }
    if (*lexer_count >= (size_t)mylite_sql_parser_version_comment_lexer_stack_limit) {
        return MYLITE_SQL_PARSE_SYNTAX_ERROR;
    }

    mylite_sql_lexer_init(
        &lexer_stack[*lexer_count],
        (struct mylite_sql_lexer_config){
            .input = payload.text,
            .length = payload.length,
            .modes = modes,
        }
    );
    ++*lexer_count;
    return MYLITE_SQL_PARSE_OK;
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
    if (!mylite_sql_parser_is_parse_ok(state)) {
        return;
    }

    state->result->root = root;
}

void mylite_sql_parser_state_syntax_error(
    struct mylite_sql_parser_state *state,
    int parser_token,
    struct mylite_sql_token token
) {
    if (!mylite_sql_parser_is_parse_ok(state)) {
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
    if (!mylite_sql_parser_is_parse_ok(state)) {
        return;
    }

    mylite_sql_parser_set_state_status(state, MYLITE_SQL_PARSE_SYNTAX_ERROR);
}

void mylite_sql_parser_state_accept(struct mylite_sql_parser_state *state) {
    if (!mylite_sql_parser_is_parse_ok(state)) {
        return;
    }

    state->accepted = true;
}

void mylite_sql_parser_state_stack_overflow(struct mylite_sql_parser_state *state) {
    if (!mylite_sql_parser_is_parse_ok(state)) {
        return;
    }

    mylite_sql_parser_set_state_status(state, MYLITE_SQL_PARSE_STACK_OVERFLOW);
}

static bool feed_parenthesized_row_constructor_if_needed(
    void *parser,
    struct mylite_sql_parser_state *state,
    struct mylite_sql_parser_token_history *history,
    bool *previous_token_was_dot,
    const struct parenthesized_row_constructor_injection *injection
) {
    struct mylite_sql_token synthetic_row;

    if (!should_inject_parenthesized_row_constructor(injection)) {
        return true;
    }

    synthetic_row = make_synthetic_row_constructor_token(injection->left_paren);
    mylite_sql_lemon(parser, MYLITE_SQL_PARSE_ROW, synthetic_row, state);
    if (previous_token_was_dot != NULL) {
        *previous_token_was_dot = false;
    }
    mylite_sql_parser_update_token_history(history, MYLITE_SQL_PARSE_ROW);

    return state != NULL && state->result != NULL && state->result->status == MYLITE_SQL_PARSE_OK;
}

static bool should_inject_parenthesized_row_constructor(
    const struct parenthesized_row_constructor_injection *injection
) {
    const struct mylite_sql_token *previous_token = NULL;

    if (injection == NULL || !injection->enabled ||
        !mylite_sql_parser_token_is_left_paren(injection->left_paren)) {
        return false;
    }
    if (injection->has_previous_token) {
        previous_token = injection->previous_token;
    }
    if (token_can_name_immediate_function(previous_token)) {
        return false;
    }
    return lexer_parenthesized_expression_has_top_level_comma(injection->lexer);
}

static bool token_can_name_immediate_function(const struct mylite_sql_token *token) {
    if (token == NULL) {
        return false;
    }
    if (token->kind == MYLITE_SQL_TOKEN_IDENTIFIER ||
        token->kind == MYLITE_SQL_TOKEN_QUOTED_IDENTIFIER) {
        return true;
    }
    if (token->kind != MYLITE_SQL_TOKEN_KEYWORD) {
        return false;
    }
    return !mylite_sql_parser_token_text_equals(token, "SELECT") &&
           !mylite_sql_parser_token_text_equals(token, "FROM") &&
           !mylite_sql_parser_token_text_equals(token, "WHERE") &&
           !mylite_sql_parser_token_text_equals(token, "HAVING") &&
           !mylite_sql_parser_token_text_equals(token, "ON") &&
           !mylite_sql_parser_token_text_equals(token, "GROUP") &&
           !mylite_sql_parser_token_text_equals(token, "ORDER") &&
           !mylite_sql_parser_token_text_equals(token, "BY") &&
           !mylite_sql_parser_token_text_equals(token, "LIMIT") &&
           !mylite_sql_parser_token_text_equals(token, "UNION") &&
           !mylite_sql_parser_token_text_equals(token, "IN") &&
           !mylite_sql_parser_token_text_equals(token, "NOT") &&
           !mylite_sql_parser_token_text_equals(token, "VALUES") &&
           !mylite_sql_parser_token_text_equals(token, "SET") &&
           !mylite_sql_parser_token_text_equals(token, "UPDATE") &&
           !mylite_sql_parser_token_text_equals(token, "DELETE") &&
           !mylite_sql_parser_token_text_equals(token, "INSERT") &&
           !mylite_sql_parser_token_text_equals(token, "REPLACE") &&
           !mylite_sql_parser_token_text_equals(token, "JOIN");
}

static bool lexer_parenthesized_expression_has_top_level_comma(
    const struct mylite_sql_lexer *lexer
) {
    struct mylite_sql_lexer lookahead;
    int paren_depth = 1;
    bool has_top_level_comma = false;

    if (lexer == NULL) {
        return false;
    }

    lookahead = *lexer;
    for (;;) {
        struct mylite_sql_token token;

        if (mylite_sql_lexer_next(&lookahead, &token) != 0) {
            return false;
        }
        if (mylite_sql_parser_token_is_comment(token.kind)) {
            continue;
        }
        if (token.kind == MYLITE_SQL_TOKEN_ERROR || token.kind == MYLITE_SQL_TOKEN_EOF) {
            return false;
        }
        if (mylite_sql_parser_token_is_left_paren(&token)) {
            ++paren_depth;
            continue;
        }
        if (mylite_sql_parser_token_is_right_paren(&token)) {
            --paren_depth;
            if (paren_depth <= 0) {
                return has_top_level_comma;
            }
            continue;
        }
        if (paren_depth == 1 && mylite_sql_parser_token_is_comma(&token)) {
            has_top_level_comma = true;
        }
    }
}

static struct mylite_sql_token make_synthetic_row_constructor_token(
    const struct mylite_sql_token *left_paren
) {
    struct mylite_sql_token token = *left_paren;

    token.flags |= MYLITE_SQL_TOKEN_SYNTHETIC_ROW_CONSTRUCTOR;
    return token;
}

static bool parse_result_is_unsupported_utility_script(
    const struct mylite_sql_parse_result *result
) {
    struct mylite_sql_ast_node *statement = NULL;

    if (result == NULL || result->root == NULL || result->root->kind != MYLITE_SQL_AST_SCRIPT) {
        return false;
    }

    statement = mylite_sql_parser_child_at(result->root, 0U);
    return statement != NULL && statement->kind == MYLITE_SQL_AST_UNSUPPORTED_UTILITY_STATEMENT;
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
