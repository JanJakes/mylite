#include "mylite_parser.h"

#include "mylite_parse.h"
#include "mylite_parser_driver.h"
#include "mylite_parser_helpers.h"
#include "mylite_parser_internal.h"
#include "mylite_parser_placeholders.h"
#include "mylite_parser_token_map.h"

#include <stdbool.h>
#include <stddef.h>
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

struct version_comment_payload {
    const char *text;
    size_t length;
    bool active;
};

struct mylite_sql_parse_error {
    enum mylite_sql_parse_status status;
    int parser_token;
    struct mylite_sql_token token;
};

enum {
    mylite_mysql_version_comment_gate = 80409,
    version_comment_min_token_length = 5,
    version_comment_decimal_radix = 10,
};

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
static bool parse_version_comment_payload(
    const struct mylite_sql_token *token,
    struct version_comment_payload *out_payload
);
static bool ascii_byte_is_digit(char byte);
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
static bool lexer_parenthesized_expression_has_top_level_comma(const struct mylite_sql_lexer *lexer
);
static struct mylite_sql_token make_synthetic_row_constructor_token(
    const struct mylite_sql_token *left_paren
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

    if (status == MYLITE_SQL_PARSE_SYNTAX_ERROR) {
        bool handled = false;

        enum mylite_sql_parse_status modifier_status =
            mylite_sql_parser_try_parse_select_result_option_before_duplicate_statement(
                config,
                out_result,
                &handled
            );

        if (handled) {
            status = modifier_status;
            out_result->status = modifier_status;
        }
    }

    if (status == MYLITE_SQL_PARSE_SYNTAX_ERROR) {
        bool handled = false;

        enum mylite_sql_parse_status row_constructor_status =
            mylite_sql_parser_try_parse_parenthesized_row_constructor_statement(
                config,
                out_result,
                &handled
            );

        if (handled) {
            status = row_constructor_status;
            out_result->status = row_constructor_status;
        }
    }

    if (status == MYLITE_SQL_PARSE_SYNTAX_ERROR) {
        bool handled = false;

        enum mylite_sql_parse_status row_arithmetic_status =
            mylite_sql_parser_try_parse_parenthesized_row_arithmetic_predicate_statement(
                config,
                out_result,
                &handled
            );

        if (handled) {
            status = row_arithmetic_status;
            out_result->status = row_arithmetic_status;
        }
    }

    if (status == MYLITE_SQL_PARSE_SYNTAX_ERROR) {
        bool handled = false;

        enum mylite_sql_parse_status tableless_limit_status =
            mylite_sql_parser_try_parse_tableless_select_limit_statement(
                config,
                out_result,
                &handled
            );

        if (handled) {
            status = tableless_limit_status;
            out_result->status = tableless_limit_status;
        }
    }

    if (status == MYLITE_SQL_PARSE_SYNTAX_ERROR) {
        bool handled = false;

        enum mylite_sql_parse_status locking_status =
            mylite_sql_parser_try_parse_repeated_select_locking_statement(
                config,
                out_result,
                &handled
            );

        if (handled) {
            status = locking_status;
            out_result->status = locking_status;
        }
    }

    if (status == MYLITE_SQL_PARSE_SYNTAX_ERROR) {
        bool handled = false;

        enum mylite_sql_parse_status legacy_index_status =
            mylite_sql_parser_try_parse_legacy_create_index_type_statement(
                config,
                out_result,
                &handled
            );

        if (handled) {
            status = legacy_index_status;
            out_result->status = legacy_index_status;
        }
    }

    if (status == MYLITE_SQL_PARSE_SYNTAX_ERROR) {
        bool handled = false;
        enum mylite_sql_parse_status placeholder_status =
            mylite_sql_parser_try_parse_placeholder_statement(config, out_result, &handled);

        if (handled) {
            status = placeholder_status;
            out_result->status = placeholder_status;
        }
    }

    return status;
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

    return out_result->status;
}

void mylite_sql_parser_reset_parse_result(struct mylite_sql_parse_result *out_result) {
    out_result->root = NULL;
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
    size_t lexer_count = 1U;

    if (context == NULL || lexer == NULL || context->state == NULL ||
        context->state->result == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }

    lexer_stack[0] = *lexer;
    for (;;) {
        struct mylite_sql_token token;
        enum mylite_sql_parse_status status;
        struct mylite_sql_lexer *current_lexer = &lexer_stack[lexer_count - 1U];

        if (mylite_sql_lexer_next(current_lexer, &token) != 0) {
            return MYLITE_SQL_PARSE_MISUSE;
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

static bool parse_version_comment_payload(
    const struct mylite_sql_token *token,
    struct version_comment_payload *out_payload
) {
    const char *payload_start;
    const char *payload_end;
    const char *cursor;
    unsigned int version = 0U;
    bool has_version = false;

    if (out_payload == NULL) {
        return false;
    }
    *out_payload = (struct version_comment_payload){0};

    if (token == NULL || token->kind != MYLITE_SQL_TOKEN_VERSION_COMMENT || token->text == NULL ||
        token->length < (size_t)version_comment_min_token_length) {
        return false;
    }

    payload_start = token->text + 3U;
    payload_end = token->text + token->length - 2U;
    if (payload_end < payload_start) {
        return false;
    }

    cursor = payload_start;
    while (cursor < payload_end && ascii_byte_is_digit(*cursor)) {
        has_version = true;
        if (version <= (unsigned int)mylite_mysql_version_comment_gate) {
            version = (version * (unsigned int)version_comment_decimal_radix) +
                      (unsigned int)(*cursor - '0');
        }
        ++cursor;
    }

    out_payload->text = cursor;
    out_payload->length = (size_t)(payload_end - cursor);
    out_payload->active =
        !has_version || version <= (unsigned int)mylite_mysql_version_comment_gate;
    return true;
}

enum mylite_sql_parse_status mylite_sql_parser_push_version_comment_payload_lexer(
    const struct mylite_sql_token *token,
    unsigned int modes,
    struct mylite_sql_lexer *lexer_stack,
    size_t *lexer_count
) {
    struct version_comment_payload payload;

    if (lexer_stack == NULL || lexer_count == NULL) {
        return MYLITE_SQL_PARSE_MISUSE;
    }
    if (!parse_version_comment_payload(token, &payload)) {
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

static bool ascii_byte_is_digit(char byte) {
    return byte >= '0' && byte <= '9';
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

static bool lexer_parenthesized_expression_has_top_level_comma(const struct mylite_sql_lexer *lexer
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
