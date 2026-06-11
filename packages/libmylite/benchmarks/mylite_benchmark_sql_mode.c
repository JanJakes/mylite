#include "mylite_benchmark_sql_mode.h"

#include "sql/mylite_ast.h"
#include "sql/mylite_lexer.h"
#include "sql/mylite_parser.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

enum {
    sql_mode_user_variable_capacity = 32,
    sql_mode_user_variable_name_capacity = 64,
    sql_mode_value_capacity = 1024,
};

struct sql_mode_descriptor {
    const char *name;
    unsigned int lexer_modes;
};

struct sql_mode_user_variable {
    char name[sql_mode_user_variable_name_capacity];
    unsigned int modes;
    bool known;
};

struct sql_mode_context {
    unsigned int modes;
    struct sql_mode_user_variable variables[sql_mode_user_variable_capacity];
    size_t variable_count;
};

static int apply_query_sql_mode_effect(
    struct sql_mode_context *context,
    const struct mylite_benchmark_owned_query *query
);
static void apply_set_statement(
    struct sql_mode_context *context,
    const struct mylite_sql_ast_node *statement
);
static void apply_placeholder_sql_mode_statement(
    struct sql_mode_context *context,
    const struct mylite_sql_ast_node *statement
);
static void apply_set_assignment(
    struct sql_mode_context *context,
    const struct mylite_sql_ast_node *assignment
);
static bool evaluate_sql_mode_value(
    const struct sql_mode_context *context,
    const struct mylite_sql_ast_node *value,
    unsigned int *out_modes
);
static bool evaluate_functional_sql_mode_value(
    const struct sql_mode_context *context,
    const struct mylite_sql_ast_node *value,
    unsigned int *out_modes
);
static bool parse_sql_mode_value_span(
    const struct mylite_sql_source_span *span,
    unsigned int current_modes,
    unsigned int *out_modes
);
static bool parse_sql_mode_value_text(const char *text, size_t length, unsigned int *out_modes);
static bool collect_sql_mode_tokens(
    const char *text,
    size_t length,
    unsigned int *out_modes,
    bool *out_found
);
static bool lookup_sql_mode_token(const char *text, size_t length, unsigned int *out_modes);
static bool decode_quoted_sql_mode_value(
    const char *text,
    size_t length,
    unsigned int current_modes,
    char *buffer,
    size_t buffer_size,
    size_t *out_length
);
static bool unquoted_sql_mode_text(
    const char *text,
    size_t length,
    unsigned int current_modes,
    char *buffer,
    size_t buffer_size,
    size_t *out_length
);
static bool target_is_session_sql_mode(const struct mylite_sql_ast_node *target);
static bool span_is_session_sql_mode_target(const struct mylite_sql_source_span *span);
static bool span_is_global_or_persist_target(const struct mylite_sql_source_span *span);
static bool node_is_sql_mode_system_variable(const struct mylite_sql_ast_node *node);
static bool user_variable_name(
    const struct mylite_sql_ast_node *node,
    char *buffer,
    size_t buffer_size
);
static void save_user_variable(
    struct sql_mode_context *context,
    const char *name,
    unsigned int modes
);
static bool load_user_variable(
    const struct sql_mode_context *context,
    const char *name,
    unsigned int *out_modes
);
static const struct mylite_sql_ast_node *child_at(
    const struct mylite_sql_ast_node *node,
    size_t index
);
static const char *trim_span(const char *text, size_t *length);
static bool span_contains_token_ci(const struct mylite_sql_source_span *span, const char *token);
static bool text_contains_token_ci(const char *text, size_t length, const char *token);
static bool token_equals_ci(const char *text, size_t length, const char *expected);
static bool is_identifier_byte(char byte);
static char ascii_lower(char byte);

static const struct sql_mode_descriptor sql_mode_descriptors[] = {
    {"ALLOW_INVALID_DATES", 0U},
    {"ANSI_QUOTES", MYLITE_SQL_MODE_ANSI_QUOTES},
    {"ERROR_FOR_DIVISION_BY_ZERO", 0U},
    {"HIGH_NOT_PRECEDENCE", 0U},
    {"IGNORE_SPACE", MYLITE_SQL_MODE_IGNORE_SPACE},
    {"NO_AUTO_VALUE_ON_ZERO", 0U},
    {"NO_BACKSLASH_ESCAPES", MYLITE_SQL_MODE_NO_BACKSLASH_ESCAPES},
    {"NO_DIR_IN_CREATE", 0U},
    {"NO_ENGINE_SUBSTITUTION", 0U},
    {"NO_UNSIGNED_SUBTRACTION", 0U},
    {"NO_ZERO_DATE", 0U},
    {"NO_ZERO_IN_DATE", 0U},
    {"ONLY_FULL_GROUP_BY", 0U},
    {"PAD_CHAR_TO_FULL_LENGTH", 0U},
    {"PIPES_AS_CONCAT", MYLITE_SQL_MODE_PIPES_AS_CONCAT},
    {"REAL_AS_FLOAT", 0U},
    {"STRICT_ALL_TABLES", 0U},
    {"STRICT_TRANS_TABLES", 0U},
    {"TIME_TRUNCATE_FRACTIONAL", 0U},
    {"ANSI",
     MYLITE_SQL_MODE_ANSI_QUOTES | MYLITE_SQL_MODE_IGNORE_SPACE | MYLITE_SQL_MODE_PIPES_AS_CONCAT},
    {"TRADITIONAL", 0U},
};

int mylite_benchmark_assign_sql_modes(struct mylite_benchmark_owned_query_list *queries) {
    struct sql_mode_context context = {0};

    if (queries == NULL) {
        return 1;
    }
    for (size_t index = 0U; index < queries->count; ++index) {
        queries->items[index].modes = context.modes;
        if (apply_query_sql_mode_effect(&context, &queries->items[index]) != 0) {
            return 1;
        }
    }
    return 0;
}

static int apply_query_sql_mode_effect(
    struct sql_mode_context *context,
    const struct mylite_benchmark_owned_query *query
) {
    struct mylite_sql_parse_result result = {0};
    enum mylite_sql_parse_status status = mylite_sql_parse(
        (struct mylite_sql_parse_config){
            .input = query->sql,
            .length = query->length,
            .modes = context->modes,
        },
        &result
    );

    if (status == MYLITE_SQL_PARSE_NOMEM || status == MYLITE_SQL_PARSE_MISUSE) {
        fprintf(stderr, "failed to parse CSV query while assigning SQL modes\n");
        mylite_sql_parse_result_deinit(&result);
        return 1;
    }
    if (status == MYLITE_SQL_PARSE_OK && result.root != NULL &&
        result.root->kind == MYLITE_SQL_AST_SCRIPT && result.root->first_child != NULL &&
        result.root->first_child->next_sibling == NULL) {
        const struct mylite_sql_ast_node *statement = result.root->first_child;

        if (statement->kind == MYLITE_SQL_AST_SET_STATEMENT) {
            apply_set_statement(context, statement);
        } else {
            apply_placeholder_sql_mode_statement(context, statement);
        }
    }
    mylite_sql_parse_result_deinit(&result);
    return 0;
}

static void apply_set_statement(
    struct sql_mode_context *context,
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *assignment_list = NULL;
    const struct mylite_sql_ast_node *assignment = NULL;

    if (statement == NULL || statement->kind != MYLITE_SQL_AST_SET_STATEMENT) {
        return;
    }
    assignment_list = child_at(statement, 0U);
    if (assignment_list == NULL || assignment_list->kind != MYLITE_SQL_AST_SET_ASSIGNMENT_LIST) {
        return;
    }
    assignment = child_at(assignment_list, 0U);
    while (assignment != NULL) {
        apply_set_assignment(context, assignment);
        assignment = assignment->next_sibling;
    }
}

static void apply_placeholder_sql_mode_statement(
    struct sql_mode_context *context,
    const struct mylite_sql_ast_node *statement
) {
    unsigned int modes = 0U;
    bool found = false;

    if (statement == NULL || !span_contains_token_ci(&statement->span, "set") ||
        !span_contains_token_ci(&statement->span, "sql_mode") ||
        span_is_global_or_persist_target(&statement->span) ||
        !collect_sql_mode_tokens(statement->span.text, statement->span.length, &modes, &found) ||
        !found) {
        return;
    }
    if (span_contains_token_ci(&statement->span, "list_add")) {
        context->modes |= modes;
        return;
    }
    if (span_contains_token_ci(&statement->span, "list_drop")) {
        context->modes &= ~modes;
        return;
    }
}

static void apply_set_assignment(
    struct sql_mode_context *context,
    const struct mylite_sql_ast_node *assignment
) {
    const struct mylite_sql_ast_node *target = NULL;
    const struct mylite_sql_ast_node *value = NULL;
    unsigned int modes = 0U;
    char variable_name[sql_mode_user_variable_name_capacity];

    if (assignment == NULL || assignment->kind != MYLITE_SQL_AST_SET_ASSIGNMENT) {
        return;
    }
    target = child_at(assignment, 0U);
    value = child_at(assignment, 1U);
    if (target != NULL && target->kind == MYLITE_SQL_AST_USER_VARIABLE &&
        user_variable_name(target, variable_name, sizeof(variable_name)) &&
        evaluate_sql_mode_value(context, value, &modes)) {
        save_user_variable(context, variable_name, modes);
        return;
    }
    if (!target_is_session_sql_mode(target)) {
        return;
    }
    if (evaluate_sql_mode_value(context, value, &modes)) {
        context->modes = modes;
    }
}

static bool evaluate_sql_mode_value(
    const struct sql_mode_context *context,
    const struct mylite_sql_ast_node *value,
    unsigned int *out_modes
) {
    char variable_name[sql_mode_user_variable_name_capacity];

    if (value == NULL || out_modes == NULL) {
        return false;
    }
    if (value->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        *out_modes = 0U;
        return true;
    }
    if (node_is_sql_mode_system_variable(value)) {
        *out_modes = context->modes;
        return true;
    }
    if (value->kind == MYLITE_SQL_AST_USER_VARIABLE &&
        user_variable_name(value, variable_name, sizeof(variable_name))) {
        return load_user_variable(context, variable_name, out_modes);
    }
    if (evaluate_functional_sql_mode_value(context, value, out_modes)) {
        return true;
    }
    return parse_sql_mode_value_span(&value->span, context->modes, out_modes);
}

static bool evaluate_functional_sql_mode_value(
    const struct sql_mode_context *context,
    const struct mylite_sql_ast_node *value,
    unsigned int *out_modes
) {
    unsigned int modes = 0U;
    bool found = false;

    if (value == NULL || out_modes == NULL || !span_contains_token_ci(&value->span, "sql_mode")) {
        return false;
    }
    if (span_contains_token_ci(&value->span, "list_add")) {
        if (!collect_sql_mode_tokens(value->span.text, value->span.length, &modes, &found) ||
            !found) {
            return false;
        }
        *out_modes = context->modes | modes;
        return true;
    }
    if (span_contains_token_ci(&value->span, "list_drop")) {
        if (!collect_sql_mode_tokens(value->span.text, value->span.length, &modes, &found) ||
            !found) {
            return false;
        }
        *out_modes = context->modes & ~modes;
        return true;
    }
    if (span_contains_token_ci(&value->span, "concat")) {
        if (!collect_sql_mode_tokens(value->span.text, value->span.length, &modes, &found) ||
            !found) {
            return false;
        }
        *out_modes = context->modes | modes;
        return true;
    }
    return false;
}

static bool parse_sql_mode_value_span(
    const struct mylite_sql_source_span *span,
    unsigned int current_modes,
    unsigned int *out_modes
) {
    char buffer[sql_mode_value_capacity];
    size_t length = 0U;

    if (span == NULL || out_modes == NULL ||
        !unquoted_sql_mode_text(
            span->text,
            span->length,
            current_modes,
            buffer,
            sizeof(buffer),
            &length
        )) {
        return false;
    }
    return parse_sql_mode_value_text(buffer, length, out_modes);
}

static bool parse_sql_mode_value_text(const char *text, size_t length, unsigned int *out_modes) {
    const char *trimmed = trim_span(text, &length);
    size_t token_start = 0U;
    unsigned int modes = 0U;

    if (out_modes == NULL || trimmed == NULL) {
        return false;
    }
    if (length == 0U || token_equals_ci(trimmed, length, "default") ||
        (length == 1U && trimmed[0] == '0')) {
        *out_modes = 0U;
        return true;
    }
    for (size_t index = 0U;; ++index) {
        if (index == length || trimmed[index] == ',') {
            const char *token = &trimmed[token_start];
            size_t token_length = index - token_start;
            unsigned int token_modes = 0U;

            token = trim_span(token, &token_length);
            if (token_length == 0U || !lookup_sql_mode_token(token, token_length, &token_modes)) {
                return false;
            }
            modes |= token_modes;
            token_start = index + 1U;
        }
        if (index == length) {
            break;
        }
    }
    *out_modes = modes;
    return true;
}

static bool collect_sql_mode_tokens(
    const char *text,
    size_t length,
    unsigned int *out_modes,
    bool *out_found
) {
    size_t index = 0U;
    unsigned int modes = 0U;
    bool found = false;

    if (text == NULL || out_modes == NULL || out_found == NULL) {
        return false;
    }
    while (index < length) {
        if (is_identifier_byte(text[index])) {
            size_t token_start = index;
            size_t token_length = 0U;
            unsigned int token_modes = 0U;

            while (index < length && is_identifier_byte(text[index])) {
                ++index;
            }
            token_length = index - token_start;
            if (lookup_sql_mode_token(&text[token_start], token_length, &token_modes)) {
                modes |= token_modes;
                found = true;
            }
        } else {
            ++index;
        }
    }
    *out_modes = modes;
    *out_found = found;
    return true;
}

static bool lookup_sql_mode_token(const char *text, size_t length, unsigned int *out_modes) {
    if (out_modes == NULL) {
        return false;
    }
    for (size_t index = 0U; index < sizeof(sql_mode_descriptors) / sizeof(sql_mode_descriptors[0]);
         ++index) {
        if (token_equals_ci(text, length, sql_mode_descriptors[index].name)) {
            *out_modes = sql_mode_descriptors[index].lexer_modes;
            return true;
        }
    }
    return false;
}

static bool decode_quoted_sql_mode_value(
    const char *text,
    size_t length,
    unsigned int current_modes,
    char *buffer,
    size_t buffer_size,
    size_t *out_length
) {
    char quote = '\0';
    size_t offset = 0U;

    if (text == NULL || buffer == NULL || out_length == NULL || length < 2U) {
        return false;
    }
    quote = text[0];
    if ((quote != '\'' && quote != '"') || text[length - 1U] != quote) {
        return false;
    }
    for (size_t index = 1U; index + 1U < length; ++index) {
        char byte = text[index];

        if (byte == quote && index + 1U < length - 1U && text[index + 1U] == quote) {
            ++index;
        } else if (byte == '\\' && (current_modes & MYLITE_SQL_MODE_NO_BACKSLASH_ESCAPES) == 0U &&
                   index + 1U < length - 1U) {
            ++index;
            byte = text[index];
        }
        if (offset + 1U >= buffer_size) {
            return false;
        }
        buffer[offset] = byte;
        ++offset;
    }
    buffer[offset] = '\0';
    *out_length = offset;
    return true;
}

static bool unquoted_sql_mode_text(
    const char *text,
    size_t length,
    unsigned int current_modes,
    char *buffer,
    size_t buffer_size,
    size_t *out_length
) {
    const char *trimmed = trim_span(text, &length);

    if (trimmed == NULL || buffer == NULL || out_length == NULL || buffer_size == 0U) {
        return false;
    }
    if (length >= 2U && (trimmed[0] == '\'' || trimmed[0] == '"')) {
        return decode_quoted_sql_mode_value(
            trimmed,
            length,
            current_modes,
            buffer,
            buffer_size,
            out_length
        );
    }
    if (length + 1U > buffer_size) {
        return false;
    }
    if (length > 0U) {
        memcpy(buffer, trimmed, length);
    }
    buffer[length] = '\0';
    *out_length = length;
    return true;
}

static bool target_is_session_sql_mode(const struct mylite_sql_ast_node *target) {
    return target != NULL && target->kind == MYLITE_SQL_AST_SET_SYSTEM_VARIABLE_TARGET &&
           span_is_session_sql_mode_target(&target->span);
}

static bool span_is_session_sql_mode_target(const struct mylite_sql_source_span *span) {
    return span != NULL && text_contains_token_ci(span->text, span->length, "sql_mode") &&
           !span_is_global_or_persist_target(span);
}

static bool span_is_global_or_persist_target(const struct mylite_sql_source_span *span) {
    return span_contains_token_ci(span, "global") || span_contains_token_ci(span, "persist") ||
           span_contains_token_ci(span, "persist_only");
}

static bool node_is_sql_mode_system_variable(const struct mylite_sql_ast_node *node) {
    return node != NULL && node->kind == MYLITE_SQL_AST_SYSTEM_VARIABLE &&
           span_is_session_sql_mode_target(&node->span);
}

static bool user_variable_name(
    const struct mylite_sql_ast_node *node,
    char *buffer,
    size_t buffer_size
) {
    const char *text = NULL;
    size_t length = 0U;
    size_t offset = 0U;

    if (node == NULL || node->kind != MYLITE_SQL_AST_USER_VARIABLE || buffer == NULL ||
        buffer_size == 0U) {
        return false;
    }
    text = node->span.text;
    length = node->span.length;
    while (length > 0U && *text == '@') {
        ++text;
        --length;
    }
    if (length == 0U || length >= buffer_size) {
        return false;
    }
    for (size_t index = 0U; index < length; ++index) {
        buffer[offset] = ascii_lower(text[index]);
        ++offset;
    }
    buffer[offset] = '\0';
    return true;
}

static void save_user_variable(
    struct sql_mode_context *context,
    const char *name,
    unsigned int modes
) {
    if (context == NULL || name == NULL) {
        return;
    }
    for (size_t index = 0U; index < context->variable_count; ++index) {
        if (strcmp(context->variables[index].name, name) == 0) {
            context->variables[index].modes = modes;
            context->variables[index].known = true;
            return;
        }
    }
    if (context->variable_count >= sql_mode_user_variable_capacity) {
        return;
    }
    snprintf(
        context->variables[context->variable_count].name,
        sizeof(context->variables[context->variable_count].name),
        "%s",
        name
    );
    context->variables[context->variable_count].modes = modes;
    context->variables[context->variable_count].known = true;
    ++context->variable_count;
}

static bool load_user_variable(
    const struct sql_mode_context *context,
    const char *name,
    unsigned int *out_modes
) {
    if (context == NULL || name == NULL || out_modes == NULL) {
        return false;
    }
    for (size_t index = 0U; index < context->variable_count; ++index) {
        if (context->variables[index].known && strcmp(context->variables[index].name, name) == 0) {
            *out_modes = context->variables[index].modes;
            return true;
        }
    }
    return false;
}

static const struct mylite_sql_ast_node *child_at(
    const struct mylite_sql_ast_node *node,
    size_t index
) {
    const struct mylite_sql_ast_node *child = NULL;

    if (node == NULL) {
        return NULL;
    }
    child = node->first_child;
    for (size_t current = 0U; current < index && child != NULL; ++current) {
        child = child->next_sibling;
    }
    return child;
}

static const char *trim_span(const char *text, size_t *length) {
    if (text == NULL || length == NULL) {
        return NULL;
    }
    while (*length > 0U && (text[0] == ' ' || text[0] == '\t' || text[0] == '\r' || text[0] == '\n')
    ) {
        ++text;
        --*length;
    }
    while (*length > 0U && (text[*length - 1U] == ' ' || text[*length - 1U] == '\t' ||
                            text[*length - 1U] == '\r' || text[*length - 1U] == '\n')) {
        --*length;
    }
    return text;
}

static bool span_contains_token_ci(const struct mylite_sql_source_span *span, const char *token) {
    return span != NULL && text_contains_token_ci(span->text, span->length, token);
}

static bool text_contains_token_ci(const char *text, size_t length, const char *token) {
    size_t token_length = token == NULL ? 0U : strlen(token);

    if (text == NULL || token == NULL || token_length == 0U || token_length > length) {
        return false;
    }
    for (size_t index = 0U; index + token_length <= length; ++index) {
        bool left_boundary = index == 0U || !is_identifier_byte(text[index - 1U]);
        bool right_boundary =
            index + token_length == length || !is_identifier_byte(text[index + token_length]);

        if (left_boundary && right_boundary && token_equals_ci(&text[index], token_length, token)) {
            return true;
        }
    }
    return false;
}

static bool token_equals_ci(const char *text, size_t length, const char *expected) {
    if (text == NULL || expected == NULL || strlen(expected) != length) {
        return false;
    }
    for (size_t index = 0U; index < length; ++index) {
        if (ascii_lower(text[index]) != ascii_lower(expected[index])) {
            return false;
        }
    }
    return true;
}

static bool is_identifier_byte(char byte) {
    return (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') ||
           (byte >= '0' && byte <= '9') || byte == '_';
}

static char ascii_lower(char byte) {
    if (byte >= 'A' && byte <= 'Z') {
        return (char)(byte - 'A' + 'a');
    }
    return byte;
}
