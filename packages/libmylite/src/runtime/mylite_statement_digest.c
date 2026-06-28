#include "mylite_statement_digest.h"

#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_dynamic_string.h"
#include "mylite_mysql_error_codes.h"
#include "mylite_parser.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    digest_error_near_capacity = 64,
};

static int validate_statement_digest_sql(
    struct mylite_db *database,
    const char *sql,
    size_t sql_size
);
static int normalize_statement_digest_sql(
    struct mylite_db *database,
    const char *sql,
    size_t sql_size,
    char **out_text,
    size_t *out_text_size
);
static int append_normalized_token(
    struct mylite_dynamic_string *output,
    const struct mylite_sql_token *token
);
static int append_output_token(struct mylite_dynamic_string *output, const char *text);
static int append_output_token_bytes(
    struct mylite_dynamic_string *output,
    const char *text,
    size_t text_size
);
static int append_uppercase_token(
    struct mylite_dynamic_string *output,
    const char *text,
    size_t text_size
);
static int append_quoted_identifier_token(
    struct mylite_dynamic_string *output,
    const struct mylite_sql_token *token
);
static int append_operator_token(
    struct mylite_dynamic_string *output,
    const struct mylite_sql_token *token
);
static int append_in_list_placeholder(
    struct mylite_dynamic_string *output,
    struct mylite_sql_lexer *lexer
);
static bool next_significant_token(
    struct mylite_sql_lexer *lexer,
    struct mylite_sql_token *out_token
);
static bool token_is_ascii_keyword(const struct mylite_sql_token *token, const char *keyword);
static bool token_is_punctuation(const struct mylite_sql_token *token, char punctuation);
static unsigned int lexer_modes_for_database_session(const struct mylite_db *database);
static bool session_sql_mode_has(const struct mylite_session_state *session, uint64_t mode);
static void set_statement_digest_parse_error(
    struct mylite_db *database,
    const struct mylite_sql_token *token
);
static void copy_error_near_text(const struct mylite_sql_token *token, char out_text[]);

int mylite_statement_digest_text(
    struct mylite_db *database,
    const char *sql,
    size_t sql_size,
    char **out_text,
    size_t *out_text_size
) {
    int rc = MYLITE_OK;

    if (database == NULL || (sql == NULL && sql_size != 0U) || out_text == NULL ||
        out_text_size == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    *out_text_size = 0U;

    rc = validate_statement_digest_sql(database, sql, sql_size);
    if (rc != MYLITE_OK) {
        return rc;
    }
    return normalize_statement_digest_sql(database, sql, sql_size, out_text, out_text_size);
}

static int validate_statement_digest_sql(
    struct mylite_db *database,
    const char *sql,
    size_t sql_size
) {
    struct mylite_sql_parse_result result = {0};
    enum mylite_sql_parse_status status = MYLITE_SQL_PARSE_OK;

    status = mylite_sql_parse(
        (struct mylite_sql_parse_config){
            .input = sql,
            .length = sql_size,
            .modes = lexer_modes_for_database_session(database),
        },
        &result
    );
    if (status == MYLITE_SQL_PARSE_OK) {
        mylite_sql_parse_result_deinit(&result);
        return MYLITE_OK;
    }
    if (status == MYLITE_SQL_PARSE_NOMEM) {
        mylite_sql_parse_result_deinit(&result);
        return MYLITE_NOMEM;
    }
    if (status == MYLITE_SQL_PARSE_MISUSE) {
        mylite_sql_parse_result_deinit(&result);
        return MYLITE_MISUSE;
    }

    set_statement_digest_parse_error(database, &result.error_token);
    mylite_sql_parse_result_deinit(&result);
    return MYLITE_ERROR;
}

static int normalize_statement_digest_sql(
    struct mylite_db *database,
    const char *sql,
    size_t sql_size,
    char **out_text,
    size_t *out_text_size
) {
    struct mylite_sql_lexer lexer;
    struct mylite_dynamic_string output;
    struct mylite_sql_token token = {0};
    bool saw_statement_terminator = false;
    bool previous_token_was_is = false;
    bool previous_tokens_were_is_not = false;
    int rc = MYLITE_OK;

    mylite_sql_lexer_init(
        &lexer,
        (struct mylite_sql_lexer_config){
            .input = sql,
            .length = sql_size,
            .modes = lexer_modes_for_database_session(database),
        }
    );
    mylite_dynamic_string_init(&output);
    for (;;) {
        if (!next_significant_token(&lexer, &token)) {
            rc = MYLITE_MISUSE;
            break;
        }
        if (token.kind == MYLITE_SQL_TOKEN_EOF) {
            break;
        }
        if (saw_statement_terminator) {
            set_statement_digest_parse_error(database, &token);
            rc = MYLITE_ERROR;
            break;
        }
        if (token.kind == MYLITE_SQL_TOKEN_PARAMETER) {
            set_statement_digest_parse_error(database, &token);
            rc = MYLITE_ERROR;
            break;
        }
        if (token_is_ascii_keyword(&token, "IN")) {
            rc = append_output_token(&output, "IN");
            if (rc == MYLITE_OK) {
                rc = append_in_list_placeholder(&output, &lexer);
            }
            if (rc != MYLITE_OK) {
                break;
            }
            continue;
        }
        if (token_is_ascii_keyword(&token, "NULL") &&
            (previous_token_was_is || previous_tokens_were_is_not)) {
            rc = append_output_token(&output, "NULL");
        } else {
            rc = append_normalized_token(&output, &token);
        }
        if (rc != MYLITE_OK) {
            break;
        }
        if (token_is_punctuation(&token, ';')) {
            saw_statement_terminator = true;
        }
        previous_tokens_were_is_not =
            previous_token_was_is && token_is_ascii_keyword(&token, "NOT");
        previous_token_was_is = token_is_ascii_keyword(&token, "IS");
    }
    if (rc == MYLITE_OK) {
        *out_text_size = output.length;
        *out_text = mylite_dynamic_string_take(&output);
        if (*out_text == NULL) {
            rc = MYLITE_NOMEM;
        }
    }
    mylite_dynamic_string_deinit(&output);
    return rc;
}

static int append_normalized_token(
    struct mylite_dynamic_string *output,
    const struct mylite_sql_token *token
) {
    if (output == NULL || token == NULL) {
        return MYLITE_MISUSE;
    }

    switch (token->kind) {
    case MYLITE_SQL_TOKEN_IDENTIFIER:
    case MYLITE_SQL_TOKEN_QUOTED_IDENTIFIER:
        return append_quoted_identifier_token(output, token);
    case MYLITE_SQL_TOKEN_KEYWORD:
        if (token_is_ascii_keyword(token, "NULL")) {
            return append_output_token(output, "?");
        }
        if (token_is_ascii_keyword(token, "INT")) {
            return append_output_token(output, "INTEGER");
        }
        if (token_is_ascii_keyword(token, "VARCHAR")) {
            return append_output_token(output, "VARCHARACTER");
        }
        return append_uppercase_token(output, token->text, token->length);
    case MYLITE_SQL_TOKEN_STRING:
    case MYLITE_SQL_TOKEN_NATIONAL_STRING:
    case MYLITE_SQL_TOKEN_HEX_LITERAL:
    case MYLITE_SQL_TOKEN_BIT_LITERAL:
    case MYLITE_SQL_TOKEN_INTEGER:
    case MYLITE_SQL_TOKEN_DECIMAL:
    case MYLITE_SQL_TOKEN_FLOAT:
        return append_output_token(output, "?");
    case MYLITE_SQL_TOKEN_OPERATOR:
        return append_operator_token(output, token);
    case MYLITE_SQL_TOKEN_PUNCTUATION:
    case MYLITE_SQL_TOKEN_USER_VARIABLE:
    case MYLITE_SQL_TOKEN_SYSTEM_VARIABLE:
    case MYLITE_SQL_TOKEN_TEMPORAL_LITERAL_INTRODUCER:
        return append_output_token_bytes(output, token->text, token->length);
    case MYLITE_SQL_TOKEN_CHARSET_INTRODUCER:
    case MYLITE_SQL_TOKEN_COMMENT:
    case MYLITE_SQL_TOKEN_VERSION_COMMENT:
    case MYLITE_SQL_TOKEN_HINT_COMMENT:
    case MYLITE_SQL_TOKEN_EOF:
        return MYLITE_OK;
    case MYLITE_SQL_TOKEN_ERROR:
    case MYLITE_SQL_TOKEN_PARAMETER:
        break;
    }
    return MYLITE_MISUSE;
}

static int append_output_token(struct mylite_dynamic_string *output, const char *text) {
    if (text == NULL) {
        return MYLITE_MISUSE;
    }
    return append_output_token_bytes(output, text, strlen(text));
}

static int append_output_token_bytes(
    struct mylite_dynamic_string *output,
    const char *text,
    size_t text_size
) {
    int rc = MYLITE_OK;

    if (output == NULL || (text == NULL && text_size != 0U)) {
        return MYLITE_MISUSE;
    }
    if (output->length != 0U) {
        rc = mylite_dynamic_string_append_char(output, ' ');
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_bytes(output, text, text_size);
    }
    return rc;
}

static int append_uppercase_token(
    struct mylite_dynamic_string *output,
    const char *text,
    size_t text_size
) {
    int rc = MYLITE_OK;

    if (output == NULL || (text == NULL && text_size != 0U)) {
        return MYLITE_MISUSE;
    }
    if (output->length != 0U) {
        rc = mylite_dynamic_string_append_char(output, ' ');
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < text_size; ++index) {
        rc = mylite_dynamic_string_append_char(output, (char)toupper((unsigned char)text[index]));
    }
    return rc;
}

static int append_quoted_identifier_token(
    struct mylite_dynamic_string *output,
    const struct mylite_sql_token *token
) {
    const char *text = NULL;
    size_t start = 0U;
    size_t end = 0U;
    char quote = '\0';
    int rc = MYLITE_OK;

    if (output == NULL || token == NULL || token->text == NULL) {
        return MYLITE_MISUSE;
    }
    text = token->text;
    end = token->length;
    if (token->kind == MYLITE_SQL_TOKEN_QUOTED_IDENTIFIER && token->length >= 2U) {
        quote = token->text[0];
        start = 1U;
        end = token->length - 1U;
    }

    if (output->length != 0U) {
        rc = mylite_dynamic_string_append_char(output, ' ');
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(output, '`');
    }
    for (size_t index = start; rc == MYLITE_OK && index < end; ++index) {
        if (quote != '\0' && text[index] == quote && index + 1U < end &&
            text[index + 1U] == quote) {
            ++index;
        }
        if (text[index] == '`') {
            rc = mylite_dynamic_string_append(output, "``");
        } else {
            rc = mylite_dynamic_string_append_char(output, text[index]);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(output, '`');
    }
    return rc;
}

static int append_operator_token(
    struct mylite_dynamic_string *output,
    const struct mylite_sql_token *token
) {
    const char *text = NULL;

    if (token == NULL) {
        return MYLITE_MISUSE;
    }
    switch (token->operator_kind) {
    case MYLITE_SQL_OPERATOR_JSON_UNQUOTE_EXTRACT:
        text = "->>";
        break;
    case MYLITE_SQL_OPERATOR_JSON_EXTRACT:
        text = "->";
        break;
    case MYLITE_SQL_OPERATOR_NULL_SAFE_EQUAL:
        text = "<=>";
        break;
    case MYLITE_SQL_OPERATOR_LEFT_SHIFT:
        text = "<<";
        break;
    case MYLITE_SQL_OPERATOR_RIGHT_SHIFT:
        text = ">>";
        break;
    case MYLITE_SQL_OPERATOR_LESS_EQUAL:
        text = "<=";
        break;
    case MYLITE_SQL_OPERATOR_GREATER_EQUAL:
        text = ">=";
        break;
    case MYLITE_SQL_OPERATOR_NOT_EQUAL:
        text = "!=";
        break;
    case MYLITE_SQL_OPERATOR_LOGICAL_AND:
        text = "AND";
        break;
    case MYLITE_SQL_OPERATOR_LOGICAL_OR:
        text = "OR";
        break;
    case MYLITE_SQL_OPERATOR_ASSIGN:
        text = ":=";
        break;
    case MYLITE_SQL_OPERATOR_EQUAL:
        text = "=";
        break;
    case MYLITE_SQL_OPERATOR_LESS:
        text = "<";
        break;
    case MYLITE_SQL_OPERATOR_GREATER:
        text = ">";
        break;
    case MYLITE_SQL_OPERATOR_PLUS:
        text = "+";
        break;
    case MYLITE_SQL_OPERATOR_MINUS:
        text = "-";
        break;
    case MYLITE_SQL_OPERATOR_STAR:
        text = "*";
        break;
    case MYLITE_SQL_OPERATOR_SLASH:
        text = "/";
        break;
    case MYLITE_SQL_OPERATOR_PERCENT:
        text = "%";
        break;
    case MYLITE_SQL_OPERATOR_NOT:
        text = "!";
        break;
    case MYLITE_SQL_OPERATOR_BITWISE_NOT:
        text = "~";
        break;
    case MYLITE_SQL_OPERATOR_BITWISE_XOR:
        text = "^";
        break;
    case MYLITE_SQL_OPERATOR_BITWISE_AND:
        text = "&";
        break;
    case MYLITE_SQL_OPERATOR_BITWISE_OR:
        text = "|";
        break;
    case MYLITE_SQL_OPERATOR_NONE:
        break;
    }
    if (text != NULL) {
        return append_output_token(output, text);
    }
    return append_output_token_bytes(output, token->text, token->length);
}

static int append_in_list_placeholder(
    struct mylite_dynamic_string *output,
    struct mylite_sql_lexer *lexer
) {
    struct mylite_sql_lexer lookahead = {0};
    struct mylite_sql_lexer inner_lookahead = {0};
    struct mylite_sql_token token = {0};
    size_t depth = 0U;
    int rc = MYLITE_OK;

    if (output == NULL || lexer == NULL) {
        return MYLITE_MISUSE;
    }
    lookahead = *lexer;
    if (!next_significant_token(&lookahead, &token) || !token_is_punctuation(&token, '(')) {
        return MYLITE_OK;
    }
    inner_lookahead = lookahead;
    if (next_significant_token(&inner_lookahead, &token) &&
        token_is_ascii_keyword(&token, "SELECT")) {
        return MYLITE_OK;
    }
    *lexer = lookahead;
    rc = append_output_token(output, "(...)");
    if (rc != MYLITE_OK) {
        return rc;
    }
    depth = 1U;
    while (depth != 0U) {
        if (!next_significant_token(lexer, &token)) {
            return MYLITE_MISUSE;
        }
        if (token.kind == MYLITE_SQL_TOKEN_EOF) {
            return MYLITE_MISUSE;
        }
        if (token_is_punctuation(&token, '(')) {
            ++depth;
        } else if (token_is_punctuation(&token, ')')) {
            --depth;
        }
    }
    return MYLITE_OK;
}

static bool next_significant_token(
    struct mylite_sql_lexer *lexer,
    struct mylite_sql_token *out_token
) {
    if (lexer == NULL || out_token == NULL) {
        return false;
    }
    do {
        if (mylite_sql_lexer_next(lexer, out_token) != 0) {
            return false;
        }
    } while (out_token->kind == MYLITE_SQL_TOKEN_COMMENT ||
             out_token->kind == MYLITE_SQL_TOKEN_VERSION_COMMENT ||
             out_token->kind == MYLITE_SQL_TOKEN_HINT_COMMENT);
    return true;
}

static bool token_is_ascii_keyword(const struct mylite_sql_token *token, const char *keyword) {
    size_t keyword_size = keyword == NULL ? 0U : strlen(keyword);

    if (token == NULL || token->kind != MYLITE_SQL_TOKEN_KEYWORD || keyword == NULL ||
        token->length != keyword_size) {
        return false;
    }
    for (size_t index = 0U; index < token->length; ++index) {
        if (toupper((unsigned char)token->text[index]) != toupper((unsigned char)keyword[index])) {
            return false;
        }
    }
    return true;
}

static bool token_is_punctuation(const struct mylite_sql_token *token, char punctuation) {
    return token != NULL && token->kind == MYLITE_SQL_TOKEN_PUNCTUATION && token->length == 1U &&
           token->text != NULL && token->text[0] == punctuation;
}

static unsigned int lexer_modes_for_database_session(const struct mylite_db *database) {
    const struct mylite_session_state *session =
        database == NULL ? NULL : mylite_connection_session_state(database);
    unsigned int modes = 0U;

    if (session_sql_mode_has(session, MYLITE_SESSION_SQL_MODE_ANSI_QUOTES)) {
        modes |= MYLITE_SQL_MODE_ANSI_QUOTES;
    }
    if (session_sql_mode_has(session, MYLITE_SESSION_SQL_MODE_NO_BACKSLASH_ESCAPES)) {
        modes |= MYLITE_SQL_MODE_NO_BACKSLASH_ESCAPES;
    }
    if (session_sql_mode_has(session, MYLITE_SESSION_SQL_MODE_IGNORE_SPACE)) {
        modes |= MYLITE_SQL_MODE_IGNORE_SPACE;
    }
    if (session_sql_mode_has(session, MYLITE_SESSION_SQL_MODE_PIPES_AS_CONCAT)) {
        modes |= MYLITE_SQL_MODE_PIPES_AS_CONCAT;
    }
    return modes;
}

static bool session_sql_mode_has(const struct mylite_session_state *session, uint64_t mode) {
    return session != NULL && (session->sql_mode & mode) != 0U;
}

static void set_statement_digest_parse_error(
    struct mylite_db *database,
    const struct mylite_sql_token *token
) {
    char near_text[digest_error_near_capacity];
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = 0;

    copy_error_near_text(token, near_text);
    written = snprintf(
        message,
        sizeof(message),
        "Could not parse argument to digest function: \"You have an error in your SQL syntax; "
        "check the manual that corresponds to your MySQL server version for the right syntax to "
        "use near '%s' at line 1\".",
        near_text
    );
    if (written < 0 || (size_t)written >= sizeof(message)) {
        strcpy(message, "Could not parse argument to digest function.");
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_statement_digest_parse,
        "HY000",
        message
    );
}

static void copy_error_near_text(const struct mylite_sql_token *token, char out_text[]) {
    size_t length = 0U;

    if (out_text == NULL) {
        return;
    }
    out_text[0] = '\0';
    if (token == NULL || token->text == NULL || token->length == 0U) {
        return;
    }
    length = token->length;
    if (length >= digest_error_near_capacity) {
        length = digest_error_near_capacity - 1U;
    }
    memcpy(out_text, token->text, length);
    out_text[length] = '\0';
}
