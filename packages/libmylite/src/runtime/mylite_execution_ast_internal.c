#include "mylite_execution_ast_internal.h"

#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_mysql_error_codes.h"

#include <mylite/mylite.h>

#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static unsigned int parse_error_line_for_offset(const char *text, size_t offset);
static unsigned int parse_error_line(const struct mylite_sql_token *token);
static unsigned int parse_error_span_line(const struct mylite_sql_source_span *span);

const struct mylite_sql_ast_node *mylite_execution_child_at(
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

const struct mylite_sql_ast_node *mylite_execution_child_with_kind(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_node_kind kind
) {
    const struct mylite_sql_ast_node *child = NULL;

    if (node == NULL) {
        return NULL;
    }

    child = node->first_child;
    while (child != NULL) {
        if (child->kind == kind) {
            return child;
        }
        child = child->next_sibling;
    }
    return NULL;
}

int mylite_execution_script_statement_count(
    const struct mylite_sql_ast_node *root,
    size_t *out_count
) {
    if (root == NULL || root->kind != MYLITE_SQL_AST_SCRIPT || out_count == NULL) {
        return MYLITE_ERROR;
    }

    *out_count = mylite_sql_ast_node_child_count(root);

    return MYLITE_OK;
}

void mylite_execution_set_parse_result_error(
    struct mylite_db *database,
    const struct mylite_sql_parse_result *parse_result
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    const char *near_text = "end of input";
    int written = 0;

    if (parse_result != NULL && parse_result->error_token.text != NULL &&
        parse_result->error_token.length != 0U) {
        near_text = parse_result->error_token.text;
        written = snprintf(
            message,
            sizeof(message),
            "You have an error in your SQL syntax near '%.*s' at line %u",
            (int)parse_result->error_token.length,
            near_text,
            parse_error_line(&parse_result->error_token)
        );
    } else {
        written = snprintf(
            message,
            sizeof(message),
            "You have an error in your SQL syntax near '%s'",
            near_text
        );
    }
    if (written < 0) {
        message[0] = '\0';
    }

    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_parse,
        "42000",
        message
    );
}

void mylite_execution_set_multi_statement_parse_error(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *root
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    const struct mylite_sql_ast_node *second_statement = mylite_execution_child_at(root, 1U);
    const struct mylite_sql_source_span *span =
        second_statement == NULL ? NULL : &second_statement->span;
    const char *near_text = "end of input";
    size_t near_length = strlen(near_text);
    unsigned int line = 1U;
    int written = 0;

    if (span != NULL && span->text != NULL && span->length != 0U) {
        near_text = span->text;
        near_length = span->length;
        line = parse_error_span_line(span);
    }
    if (near_length > (size_t)INT_MAX) {
        near_length = (size_t)INT_MAX;
    }

    written = snprintf(
        message,
        sizeof(message),
        "You have an error in your SQL syntax; check the manual that corresponds to your "
        "MySQL server version for the right syntax to use near '%.*s' at line %u",
        (int)near_length,
        near_text,
        line
    );
    if (written < 0) {
        message[0] = '\0';
    }

    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_parse,
        "42000",
        message
    );
}

static unsigned int parse_error_line_for_offset(const char *text, size_t offset) {
    unsigned int line = 1U;

    for (size_t index = 0U; text != NULL && index < offset; ++index) {
        if (text[index] == '\r' ||
            (text[index] == '\n' && (index == 0U || text[index - 1U] != '\r'))) {
            ++line;
        }
    }
    return line;
}

static unsigned int parse_error_line(const struct mylite_sql_token *token) {
    const char *text = NULL;

    if (token == NULL || token->text == NULL) {
        return 1U;
    }

    text = token->text - token->offset;
    return parse_error_line_for_offset(text, token->offset);
}

static unsigned int parse_error_span_line(const struct mylite_sql_source_span *span) {
    const char *text = NULL;

    if (span == NULL || span->text == NULL) {
        return 1U;
    }

    text = span->text - span->offset;
    return parse_error_line_for_offset(text, span->offset);
}
