#ifndef MYLITE_SQL_MYLITE_LEXER_H
#define MYLITE_SQL_MYLITE_LEXER_H

#include <stdbool.h>
#include <stddef.h>

enum mylite_sql_mode {
    MYLITE_SQL_MODE_ANSI_QUOTES = 1U << 0U,
    MYLITE_SQL_MODE_NO_BACKSLASH_ESCAPES = 1U << 1U,
    MYLITE_SQL_MODE_IGNORE_SPACE = 1U << 2U,
    MYLITE_SQL_MODE_PIPES_AS_CONCAT = 1U << 3U,
};

enum mylite_sql_keyword_flags {
    MYLITE_SQL_KEYWORD_RESERVED = 1U << 0U,
    MYLITE_SQL_KEYWORD_RESTRICTED_LABEL = 1U << 1U,
    MYLITE_SQL_KEYWORD_RESTRICTED_ROLE = 1U << 2U,
};

enum mylite_sql_token_flags {
    MYLITE_SQL_TOKEN_HAS_LEADING_SPACE = 1U << 0U,
    MYLITE_SQL_TOKEN_SYNTHETIC_ROW_CONSTRUCTOR = 1U << 1U,
};

enum mylite_sql_token_kind {
    MYLITE_SQL_TOKEN_EOF = 0,
    MYLITE_SQL_TOKEN_ERROR = 1,
    MYLITE_SQL_TOKEN_COMMENT = 2,
    MYLITE_SQL_TOKEN_VERSION_COMMENT = 3,
    MYLITE_SQL_TOKEN_HINT_COMMENT = 4,
    MYLITE_SQL_TOKEN_IDENTIFIER = 5,
    MYLITE_SQL_TOKEN_QUOTED_IDENTIFIER = 6,
    MYLITE_SQL_TOKEN_KEYWORD = 7,
    MYLITE_SQL_TOKEN_STRING = 8,
    MYLITE_SQL_TOKEN_NATIONAL_STRING = 9,
    MYLITE_SQL_TOKEN_HEX_LITERAL = 10,
    MYLITE_SQL_TOKEN_BIT_LITERAL = 11,
    MYLITE_SQL_TOKEN_INTEGER = 12,
    MYLITE_SQL_TOKEN_DECIMAL = 13,
    MYLITE_SQL_TOKEN_FLOAT = 14,
    MYLITE_SQL_TOKEN_USER_VARIABLE = 15,
    MYLITE_SQL_TOKEN_SYSTEM_VARIABLE = 16,
    MYLITE_SQL_TOKEN_PARAMETER = 17,
    MYLITE_SQL_TOKEN_OPERATOR = 18,
    MYLITE_SQL_TOKEN_PUNCTUATION = 19,
    MYLITE_SQL_TOKEN_CHARSET_INTRODUCER = 20,
    MYLITE_SQL_TOKEN_TEMPORAL_LITERAL_INTRODUCER = 21,
};

enum mylite_sql_operator_kind {
    MYLITE_SQL_OPERATOR_NONE = 0,
    MYLITE_SQL_OPERATOR_JSON_UNQUOTE_EXTRACT = 1,
    MYLITE_SQL_OPERATOR_JSON_EXTRACT = 2,
    MYLITE_SQL_OPERATOR_NULL_SAFE_EQUAL = 3,
    MYLITE_SQL_OPERATOR_LEFT_SHIFT = 4,
    MYLITE_SQL_OPERATOR_RIGHT_SHIFT = 5,
    MYLITE_SQL_OPERATOR_LESS_EQUAL = 6,
    MYLITE_SQL_OPERATOR_GREATER_EQUAL = 7,
    MYLITE_SQL_OPERATOR_NOT_EQUAL = 8,
    MYLITE_SQL_OPERATOR_LOGICAL_AND = 9,
    MYLITE_SQL_OPERATOR_LOGICAL_OR = 10,
    MYLITE_SQL_OPERATOR_ASSIGN = 11,
    MYLITE_SQL_OPERATOR_EQUAL = 12,
    MYLITE_SQL_OPERATOR_LESS = 13,
    MYLITE_SQL_OPERATOR_GREATER = 14,
    MYLITE_SQL_OPERATOR_PLUS = 15,
    MYLITE_SQL_OPERATOR_MINUS = 16,
    MYLITE_SQL_OPERATOR_STAR = 17,
    MYLITE_SQL_OPERATOR_SLASH = 18,
    MYLITE_SQL_OPERATOR_PERCENT = 19,
    MYLITE_SQL_OPERATOR_NOT = 20,
    MYLITE_SQL_OPERATOR_BITWISE_NOT = 21,
    MYLITE_SQL_OPERATOR_BITWISE_XOR = 22,
    MYLITE_SQL_OPERATOR_BITWISE_AND = 23,
    MYLITE_SQL_OPERATOR_BITWISE_OR = 24,
};

enum mylite_sql_lexer_error {
    MYLITE_SQL_LEXER_ERROR_NONE = 0,
    MYLITE_SQL_LEXER_ERROR_UNEXPECTED_BYTE = 1,
    MYLITE_SQL_LEXER_ERROR_UNTERMINATED_STRING = 2,
    MYLITE_SQL_LEXER_ERROR_UNTERMINATED_IDENTIFIER = 3,
    MYLITE_SQL_LEXER_ERROR_UNTERMINATED_COMMENT = 4,
    MYLITE_SQL_LEXER_ERROR_INVALID_HEX_LITERAL = 5,
    MYLITE_SQL_LEXER_ERROR_INVALID_BIT_LITERAL = 6,
    MYLITE_SQL_LEXER_ERROR_INVALID_VARIABLE = 7,
};

struct mylite_sql_token {
    enum mylite_sql_token_kind kind;
    enum mylite_sql_operator_kind operator_kind;
    enum mylite_sql_lexer_error error;
    unsigned int flags;
    unsigned int keyword_flags;
    unsigned int keyword_index;
    const char *text;
    size_t length;
    size_t offset;
    size_t source_length;
};

struct mylite_sql_keyword_lookup_result {
    unsigned int flags;
    unsigned int keyword_index;
};

struct mylite_sql_lexer {
    const char *input;
    size_t length;
    size_t offset;
    unsigned int modes;
};

struct mylite_sql_lexer_config {
    const char *input;
    size_t length;
    unsigned int modes;
};

void mylite_sql_lexer_init(struct mylite_sql_lexer *lexer, struct mylite_sql_lexer_config config);

int mylite_sql_lexer_next(struct mylite_sql_lexer *lexer, struct mylite_sql_token *out_token);

bool mylite_sql_keyword_lookup(
    const char *text,
    size_t length,
    struct mylite_sql_keyword_lookup_result *out_result
);

const char *mylite_sql_token_kind_name(enum mylite_sql_token_kind kind);
const char *mylite_sql_operator_kind_name(enum mylite_sql_operator_kind kind);
const char *mylite_sql_lexer_error_name(enum mylite_sql_lexer_error error);

#endif
