#include "mylite_execution_ast_internal.h"

#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_mysql_error_codes.h"

#include <mylite/mylite.h>

#include <limits.h>
#include <stddef.h>
#include <stdio.h>

enum {
    parse_error_near_preview_capacity = 80,
};

struct parse_error_location {
    const char *text;
    size_t length;
    unsigned int line;
};

static void set_parse_error_diagnostic(
    struct mylite_db *database,
    struct parse_error_location location
);
static struct parse_error_location parse_error_location_for_token(
    const struct mylite_sql_token *token
);
static struct parse_error_location parse_error_location_for_span(
    const struct mylite_sql_source_span *span
);
static size_t parse_error_near_length(const char *text, size_t length);
static bool parse_error_is_space(unsigned char byte);
static unsigned int parse_error_line_for_offset(const char *text, size_t offset);

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
    set_parse_error_diagnostic(
        database,
        parse_error_location_for_token(parse_result == NULL ? NULL : &parse_result->error_token)
    );
}

void mylite_execution_set_multi_statement_parse_error(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *root
) {
    const struct mylite_sql_ast_node *second_statement = mylite_execution_child_at(root, 1U);
    const struct mylite_sql_source_span *span =
        second_statement == NULL ? NULL : &second_statement->span;

    set_parse_error_diagnostic(database, parse_error_location_for_span(span));
}

static void set_parse_error_diagnostic(
    struct mylite_db *database,
    struct parse_error_location location
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    const char *near_text = location.text == NULL ? "" : location.text;
    int written = 0;

    if (location.length > (size_t)parse_error_near_preview_capacity) {
        location.length = (size_t)parse_error_near_preview_capacity;
    }
    written = snprintf(
        message,
        sizeof(message),
        "You have an error in your SQL syntax; check the manual that corresponds to your "
        "MySQL server version for the right syntax to use near '%.*s' at line %u",
        (int)location.length,
        near_text,
        location.line
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

static struct parse_error_location parse_error_location_for_token(
    const struct mylite_sql_token *token
) {
    struct parse_error_location location = {
        .text = "",
        .line = 1U,
    };
    const char *source = NULL;
    size_t remaining_length = 0U;

    if (token == NULL || token->text == NULL || token->offset > token->source_length ||
        token->length > token->source_length - token->offset) {
        return location;
    }

    source = token->text - token->offset;
    location.line = parse_error_line_for_offset(source, token->offset);
    if (token->kind == MYLITE_SQL_TOKEN_EOF || (token->length == 1U && token->text[0] == ';')) {
        return location;
    }

    remaining_length = token->source_length - token->offset;
    location.text = token->text;
    if (token->length >= (size_t)parse_error_near_preview_capacity) {
        location.length = (size_t)parse_error_near_preview_capacity;
    } else {
        location.length = parse_error_near_length(token->text, remaining_length);
    }
    return location;
}

static struct parse_error_location parse_error_location_for_span(
    const struct mylite_sql_source_span *span
) {
    struct parse_error_location location = {
        .text = "",
        .line = 1U,
    };
    const char *source = NULL;

    if (span == NULL || !mylite_sql_source_span_is_valid(*span) || span->text == NULL) {
        return location;
    }

    source = span->text - span->offset;
    location.text = span->text;
    location.length = parse_error_near_length(span->text, span->length);
    if (location.length > (size_t)parse_error_near_preview_capacity) {
        location.length = (size_t)parse_error_near_preview_capacity;
    }
    location.line = parse_error_line_for_offset(source, span->offset);
    return location;
}

static size_t parse_error_near_length(const char *text, size_t length) {
    if (text == NULL) {
        return 0U;
    }

    while (length != 0U && parse_error_is_space((unsigned char)text[length - 1U])) {
        --length;
    }
    if (length != 0U && text[length - 1U] == ';') {
        --length;
    }
    while (length != 0U && parse_error_is_space((unsigned char)text[length - 1U])) {
        --length;
    }
    return length < (size_t)parse_error_near_preview_capacity
               ? length
               : (size_t)parse_error_near_preview_capacity;
}

static bool parse_error_is_space(unsigned char byte) {
    return byte == ' ' || byte == '\t' || byte == '\n' || byte == '\r' || byte == '\f' ||
           byte == '\v';
}

static unsigned int parse_error_line_for_offset(const char *text, size_t offset) {
    unsigned int line = 1U;

    for (size_t index = 0U; text != NULL && index < offset; ++index) {
        if (text[index] == '\n' && line != UINT_MAX) {
            ++line;
        }
    }
    return line;
}
