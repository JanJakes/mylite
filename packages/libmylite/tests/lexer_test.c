#include "sql/mylite_lexer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct expected_token {
    const char *text;
    enum mylite_sql_token_kind kind;
    enum mylite_sql_operator_kind operator_kind;
    enum mylite_sql_lexer_error error;
    unsigned int keyword_flags;
};

struct single_token_expectation {
    const char *sql;
    unsigned int modes;
    enum mylite_sql_token_kind kind;
    enum mylite_sql_lexer_error error;
};

static const size_t corpus_error_preview_bytes = 32U;
static const size_t corpus_minimum_token_count = 1000U;

#define EXPECTED_TOKEN(kind_value, text_value, operator_value, error_value, keyword_flags_value)   \
    {                                                                                              \
        .text = (text_value),                                                                      \
        .kind = (kind_value),                                                                      \
        .operator_kind = (operator_value),                                                         \
        .error = (error_value),                                                                    \
        .keyword_flags = (keyword_flags_value),                                                    \
    }

#define EXPECT_SINGLE_TOKEN(sql_value, modes_value, kind_value, error_value)                       \
    expect_single_token((                                                                          \
        struct single_token_expectation                                                            \
    ){.sql = (sql_value), .modes = (modes_value), .kind = (kind_value), .error = (error_value)})

static int test_keywords(void);
static int test_keyword_catalog_iteration(void);

static int test_basic_select_tokens(void);

static int test_literals(void);

static int test_comments(void);

static int test_whitespace(void);

static int test_modes(void);

static int test_variables(void);

static int test_errors(void);

static int test_leading_space(void);

static int test_corpus(void);

static int expect_keyword(const char *word, unsigned int required_flags);

static int expect_not_keyword(const char *word);

static int expect_sequence(
    const char *sql,
    unsigned int modes,
    const struct expected_token *expected,
    size_t expected_count
);

static int expect_single_token(struct single_token_expectation expected);

static int compare_token(
    const char *sql,
    size_t index,
    const struct mylite_sql_token *actual,
    const struct expected_token *expected
);

static int load_file(const char *path, char **out_data, size_t *out_length);

int main(void) {
    int failures = 0;

    failures += test_keywords();
    failures += test_keyword_catalog_iteration();
    failures += test_basic_select_tokens();
    failures += test_literals();
    failures += test_comments();
    failures += test_whitespace();
    failures += test_modes();
    failures += test_variables();
    failures += test_errors();
    failures += test_leading_space();
    failures += test_corpus();

    return failures == 0 ? 0 : 1;
}

static int test_keywords(void) {
    int failures = 0;

    failures += expect_keyword("select", MYLITE_SQL_KEYWORD_RESERVED);
    failures += expect_keyword("BEGIN", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL);
    failures += expect_keyword("event", MYLITE_SQL_KEYWORD_RESTRICTED_ROLE);
    failures += expect_keyword(
        "execute",
        MYLITE_SQL_KEYWORD_RESTRICTED_LABEL | MYLITE_SQL_KEYWORD_RESTRICTED_ROLE
    );
    failures += expect_keyword("_filename", MYLITE_SQL_KEYWORD_RESERVED);
    failures += expect_not_keyword("mylite_private_name");

    return failures;
}

static int test_keyword_catalog_iteration(void) {
    enum {
        minimum_keyword_catalog_entries = 5,
        stale_keyword_flags = 42,
    };

    const char *word = NULL;
    unsigned int flags = 0U;
    int failures = 0;

    if (mylite_sql_keyword_catalog_count() <= (size_t)minimum_keyword_catalog_entries) {
        fprintf(stderr, "keyword catalog returned too few keywords\n");
        failures = 1;
    }
    if (mylite_sql_keyword_catalog_at(0U, &word, &flags)) {
        if (strcmp(word, "ACCESSIBLE") != 0) {
            fprintf(stderr, "expected first keyword ACCESSIBLE, got %s\n", word);
            failures = 1;
        }
        if ((flags & MYLITE_SQL_KEYWORD_RESERVED) == 0U) {
            fprintf(stderr, "expected first keyword to be reserved\n");
            failures = 1;
        }
    } else {
        fprintf(stderr, "keyword catalog did not return first keyword\n");
        failures = 1;
    }

    if (mylite_sql_keyword_catalog_at(1U, &word, &flags)) {
        if (strcmp(word, "ACCOUNT") != 0) {
            fprintf(stderr, "expected second keyword ACCOUNT, got %s\n", word);
            failures = 1;
        }
        if ((flags & MYLITE_SQL_KEYWORD_RESERVED) != 0U) {
            fprintf(stderr, "expected second keyword to be nonreserved\n");
            failures = 1;
        }
    } else {
        fprintf(stderr, "keyword catalog did not return second keyword\n");
        failures = 1;
    }

    if (mylite_sql_keyword_catalog_at(mylite_sql_keyword_catalog_count(), &word, &flags)) {
        fprintf(stderr, "keyword catalog unexpectedly returned out-of-range keyword\n");
        failures = 1;
    }
    if (word != NULL || flags != 0U) {
        fprintf(stderr, "keyword catalog did not clear outputs for out-of-range lookup\n");
        failures = 1;
    }

    word = "stale";
    flags = stale_keyword_flags;
    if (mylite_sql_keyword_catalog_at(0U, NULL, &flags)) {
        fprintf(stderr, "keyword catalog unexpectedly accepted NULL word output\n");
        failures = 1;
    }
    if (flags != 0U) {
        fprintf(stderr, "keyword catalog did not clear flags for NULL word output\n");
        failures = 1;
    }

    if (mylite_sql_keyword_catalog_at(0U, &word, NULL)) {
        fprintf(stderr, "keyword catalog unexpectedly accepted NULL flags output\n");
        failures = 1;
    }
    if (word != NULL) {
        fprintf(stderr, "keyword catalog did not clear word for NULL flags output\n");
        failures = 1;
    }

    return failures;
}

static int test_basic_select_tokens(void) {
    static const struct expected_token expected[] = {
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_KEYWORD,
            "SELECT",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            MYLITE_SQL_KEYWORD_RESERVED
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_QUOTED_IDENTIFIER,
            "`select`",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_PUNCTUATION,
            ",",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_IDENTIFIER,
            "mylite_id",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_KEYWORD,
            "FROM",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            MYLITE_SQL_KEYWORD_RESERVED
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_IDENTIFIER,
            "mylite_posts",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_KEYWORD,
            "WHERE",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            MYLITE_SQL_KEYWORD_RESERVED
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_IDENTIFIER,
            "mylite_id",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_OPERATOR,
            "<=>",
            MYLITE_SQL_OPERATOR_NULL_SAFE_EQUAL,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_PARAMETER,
            "?",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_PUNCTUATION,
            ";",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_EOF,
            "",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
    };

    return expect_sequence(
        "SELECT `select`, mylite_id FROM mylite_posts WHERE mylite_id <=> ?;",
        0U,
        expected,
        sizeof(expected) / sizeof(expected[0])
    );
}

static int test_literals(void) {
    static const struct expected_token expected[] = {
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_KEYWORD,
            "SELECT",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            MYLITE_SQL_KEYWORD_RESERVED
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_NATIONAL_STRING,
            "N'a'",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_PUNCTUATION,
            ",",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_HEX_LITERAL,
            "X'0AFF'",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_PUNCTUATION,
            ",",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_HEX_LITERAL,
            "0xabc",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_PUNCTUATION,
            ",",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_BIT_LITERAL,
            "b''",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_PUNCTUATION,
            ",",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_BIT_LITERAL,
            "0b101",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_PUNCTUATION,
            ",",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_FLOAT,
            "1e+3",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_PUNCTUATION,
            ",",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_IDENTIFIER,
            "1e",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_OPERATOR,
            "+",
            MYLITE_SQL_OPERATOR_PLUS,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_INTEGER,
            "3",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_PUNCTUATION,
            ",",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_DECIMAL,
            ".25",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_PUNCTUATION,
            ",",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_DECIMAL,
            "3.",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_PUNCTUATION,
            ",",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_IDENTIFIER,
            "0XCAFE",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_PUNCTUATION,
            ",",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_IDENTIFIER,
            "0B101",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_PUNCTUATION,
            ",",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_IDENTIFIER,
            "0x1G",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_PUNCTUATION,
            ",",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_IDENTIFIER,
            "0b102",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_EOF,
            "",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
    };

    return expect_sequence(
        "SELECT N'a', X'0AFF', 0xabc, b'', 0b101, 1e+3, 1e + 3, .25, 3., "
        "0XCAFE, 0B101, 0x1G, 0b102",
        0U,
        expected,
        sizeof(expected) / sizeof(expected[0])
    );
}

static int test_comments(void) {
    static const struct expected_token expected[] = {
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_KEYWORD,
            "SELECT",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            MYLITE_SQL_KEYWORD_RESERVED
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_INTEGER,
            "1",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_OPERATOR,
            "-",
            MYLITE_SQL_OPERATOR_MINUS,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_OPERATOR,
            "-",
            MYLITE_SQL_OPERATOR_MINUS,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_INTEGER,
            "2",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_PUNCTUATION,
            ";",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_KEYWORD,
            "SELECT",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            MYLITE_SQL_KEYWORD_RESERVED
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_INTEGER,
            "1",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_COMMENT,
            "-- comment",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_OPERATOR,
            "+",
            MYLITE_SQL_OPERATOR_PLUS,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_INTEGER,
            "2",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_PUNCTUATION,
            ";",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_KEYWORD,
            "SELECT",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            MYLITE_SQL_KEYWORD_RESERVED
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_VERSION_COMMENT,
            "/*!80409 STRAIGHT_JOIN */",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_INTEGER,
            "1",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_HINT_COMMENT,
            "/*+ HINT */",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_PUNCTUATION,
            ";",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_EOF,
            "",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
    };

    return expect_sequence(
        "SELECT 1--2; SELECT 1 -- comment\n+2; SELECT /*!80409 STRAIGHT_JOIN */ 1 /*+ HINT */;",
        0U,
        expected,
        sizeof(expected) / sizeof(expected[0])
    );
}

static int test_whitespace(void) {
    static const struct expected_token expected[] = {
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_KEYWORD,
            "SELECT",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            MYLITE_SQL_KEYWORD_RESERVED
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_INTEGER,
            "1",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_PUNCTUATION,
            ";",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_KEYWORD,
            "SELECT",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            MYLITE_SQL_KEYWORD_RESERVED
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_INTEGER,
            "1",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_COMMENT,
            "--\vcomment",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_OPERATOR,
            "+",
            MYLITE_SQL_OPERATOR_PLUS,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_INTEGER,
            "2",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_PUNCTUATION,
            ";",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_EOF,
            "",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
    };

    return expect_sequence(
        "SELECT\v1; SELECT 1--\vcomment\n+2;",
        0U,
        expected,
        sizeof(expected) / sizeof(expected[0])
    );
}

static int test_modes(void) {
    int failures = 0;

    failures += EXPECT_SINGLE_TOKEN(
        "\"mylite_name\"",
        0U,
        MYLITE_SQL_TOKEN_STRING,
        MYLITE_SQL_LEXER_ERROR_NONE
    );
    failures += EXPECT_SINGLE_TOKEN(
        "\"mylite_name\"",
        MYLITE_SQL_MODE_ANSI_QUOTES,
        MYLITE_SQL_TOKEN_QUOTED_IDENTIFIER,
        MYLITE_SQL_LEXER_ERROR_NONE
    );
    failures += EXPECT_SINGLE_TOKEN(
        "'a\\'",
        0U,
        MYLITE_SQL_TOKEN_ERROR,
        MYLITE_SQL_LEXER_ERROR_UNTERMINATED_STRING
    );
    failures += EXPECT_SINGLE_TOKEN(
        "'a\\'",
        MYLITE_SQL_MODE_NO_BACKSLASH_ESCAPES,
        MYLITE_SQL_TOKEN_STRING,
        MYLITE_SQL_LEXER_ERROR_NONE
    );

    return failures;
}

static int test_variables(void) {
    static const struct expected_token expected[] = {
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_KEYWORD,
            "SELECT",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            MYLITE_SQL_KEYWORD_RESERVED
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_SYSTEM_VARIABLE,
            "@@session.sql_mode",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_PUNCTUATION,
            ",",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_SYSTEM_VARIABLE,
            "@@`default`.key_buffer_size",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_PUNCTUATION,
            ",",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_SYSTEM_VARIABLE,
            "@@`default`.key_cache_block_size",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_PUNCTUATION,
            ",",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_SYSTEM_VARIABLE,
            "@@`default`.key_cache_division_limit",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_PUNCTUATION,
            ",",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_SYSTEM_VARIABLE,
            "@@`default`.key_cache_age_threshold",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_PUNCTUATION,
            ",",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_USER_VARIABLE,
            "@plain",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_PUNCTUATION,
            ",",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_USER_VARIABLE,
            "@'dash-name'",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_PUNCTUATION,
            ",",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_USER_VARIABLE,
            "@\"double-name\"",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_PUNCTUATION,
            ",",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_USER_VARIABLE,
            "@`tick-name`",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_EOF,
            "",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
    };

    return expect_sequence(
        "SELECT @@session.sql_mode, @@`default`.key_buffer_size, "
        "@@`default`.key_cache_block_size, @@`default`.key_cache_division_limit, "
        "@@`default`.key_cache_age_threshold, @plain, @'dash-name', @\"double-name\", "
        "@`tick-name`",
        0U,
        expected,
        sizeof(expected) / sizeof(expected[0])
    );
}

static int test_errors(void) {
    int failures = 0;

    failures += EXPECT_SINGLE_TOKEN(
        "X'FFF'",
        0U,
        MYLITE_SQL_TOKEN_ERROR,
        MYLITE_SQL_LEXER_ERROR_INVALID_HEX_LITERAL
    );
    failures += EXPECT_SINGLE_TOKEN(
        "X'0G'",
        0U,
        MYLITE_SQL_TOKEN_ERROR,
        MYLITE_SQL_LEXER_ERROR_INVALID_HEX_LITERAL
    );
    failures += EXPECT_SINGLE_TOKEN(
        "b'2'",
        0U,
        MYLITE_SQL_TOKEN_ERROR,
        MYLITE_SQL_LEXER_ERROR_INVALID_BIT_LITERAL
    );
    failures += EXPECT_SINGLE_TOKEN(
        "'unterminated",
        0U,
        MYLITE_SQL_TOKEN_ERROR,
        MYLITE_SQL_LEXER_ERROR_UNTERMINATED_STRING
    );
    failures += EXPECT_SINGLE_TOKEN(
        "`unterminated",
        0U,
        MYLITE_SQL_TOKEN_ERROR,
        MYLITE_SQL_LEXER_ERROR_UNTERMINATED_IDENTIFIER
    );
    failures += EXPECT_SINGLE_TOKEN(
        "/* unterminated",
        0U,
        MYLITE_SQL_TOKEN_ERROR,
        MYLITE_SQL_LEXER_ERROR_UNTERMINATED_COMMENT
    );
    failures += EXPECT_SINGLE_TOKEN(
        "@",
        0U,
        MYLITE_SQL_TOKEN_ERROR,
        MYLITE_SQL_LEXER_ERROR_INVALID_VARIABLE
    );
    failures += EXPECT_SINGLE_TOKEN(
        "@@`default",
        0U,
        MYLITE_SQL_TOKEN_ERROR,
        MYLITE_SQL_LEXER_ERROR_INVALID_VARIABLE
    );

    return failures;
}

static int test_leading_space(void) {
    struct mylite_sql_lexer lexer;
    struct mylite_sql_token token;
    int failures = 0;

    mylite_sql_lexer_init(
        &lexer,
        (struct mylite_sql_lexer_config){
            .input = "COUNT (*)",
            .length = strlen("COUNT (*)"),
            .modes = 0U,
        }
    );

    if (mylite_sql_lexer_next(&lexer, &token) != 0 ||
        (token.flags & MYLITE_SQL_TOKEN_HAS_LEADING_SPACE) != 0U) {
        fprintf(stderr, "COUNT token unexpectedly had leading space\n");
        failures = 1;
    }

    if (mylite_sql_lexer_next(&lexer, &token) != 0 ||
        (token.flags & MYLITE_SQL_TOKEN_HAS_LEADING_SPACE) == 0U) {
        fprintf(stderr, "expected parenthesis token to record leading space\n");
        failures = 1;
    }

    return failures;
}

static int test_corpus(void) {
    char *data = NULL;
    size_t length = 0U;
    struct mylite_sql_lexer lexer;
    struct mylite_sql_token token;
    size_t token_count = 0U;
    int failures = 0;

    if (load_file(MYLITE_LEXER_CORPUS_PATH, &data, &length) != 0) {
        return 1;
    }

    mylite_sql_lexer_init(
        &lexer,
        (struct mylite_sql_lexer_config){
            .input = data,
            .length = length,
            .modes = 0U,
        }
    );
    do {
        if (mylite_sql_lexer_next(&lexer, &token) != 0) {
            fprintf(stderr, "lexer returned misuse while reading corpus\n");
            failures = 1;
            break;
        }
        ++token_count;
        if (token.kind == MYLITE_SQL_TOKEN_ERROR) {
            fprintf(
                stderr,
                "unexpected corpus lexer error %s at offset %zu line %zu column %zu near '%.*s'\n",
                mylite_sql_lexer_error_name(token.error),
                token.offset,
                token.line,
                token.column,
                (int)(token.length < corpus_error_preview_bytes ? token.length
                                                                : corpus_error_preview_bytes),
                token.text
            );
            failures = 1;
            break;
        }
    } while (token.kind != MYLITE_SQL_TOKEN_EOF);

    if (token_count < corpus_minimum_token_count) {
        fprintf(stderr, "expected corpus to produce at least 1000 tokens, got %zu\n", token_count);
        failures = 1;
    }

    free(data);
    return failures;
}

static int expect_keyword(const char *word, unsigned int required_flags) {
    unsigned int flags = 0U;
    if (!mylite_sql_keyword_lookup(word, strlen(word), &flags)) {
        fprintf(stderr, "expected '%s' to be a keyword\n", word);
        return 1;
    }
    if ((flags & required_flags) != required_flags) {
        fprintf(stderr, "keyword '%s' missing flags 0x%x, got 0x%x\n", word, required_flags, flags);
        return 1;
    }
    return 0;
}

static int expect_not_keyword(const char *word) {
    unsigned int flags = 0U;
    if (mylite_sql_keyword_lookup(word, strlen(word), &flags)) {
        fprintf(stderr, "expected '%s' not to be a keyword, got flags 0x%x\n", word, flags);
        return 1;
    }
    return 0;
}

static int expect_sequence(
    const char *sql,
    unsigned int modes,
    const struct expected_token *expected,
    size_t expected_count
) {
    struct mylite_sql_lexer lexer;
    struct mylite_sql_token token;
    int failures = 0;

    mylite_sql_lexer_init(
        &lexer,
        (struct mylite_sql_lexer_config){
            .input = sql,
            .length = strlen(sql),
            .modes = modes,
        }
    );
    for (size_t index = 0U; index < expected_count; ++index) {
        if (mylite_sql_lexer_next(&lexer, &token) != 0) {
            fprintf(stderr, "lexer returned misuse for sequence index %zu\n", index);
            return 1;
        }
        failures += compare_token(sql, index, &token, &expected[index]);
    }

    return failures;
}

static int expect_single_token(struct single_token_expectation expected) {
    struct expected_token tokens[] = {
        EXPECTED_TOKEN(expected.kind, expected.sql, MYLITE_SQL_OPERATOR_NONE, expected.error, 0U),
        EXPECTED_TOKEN(
            MYLITE_SQL_TOKEN_EOF,
            "",
            MYLITE_SQL_OPERATOR_NONE,
            MYLITE_SQL_LEXER_ERROR_NONE,
            0U
        ),
    };

    return expect_sequence(
        expected.sql,
        expected.modes,
        tokens,
        sizeof(tokens) / sizeof(tokens[0])
    );
}

static int compare_token(
    const char *sql,
    size_t index,
    const struct mylite_sql_token *actual,
    const struct expected_token *expected
) {
    size_t expected_length = strlen(expected->text);

    if (actual->kind != expected->kind) {
        fprintf(
            stderr,
            "token %zu in '%s': expected kind %s, got %s near '%.*s'\n",
            index,
            sql,
            mylite_sql_token_kind_name(expected->kind),
            mylite_sql_token_kind_name(actual->kind),
            (int)actual->length,
            actual->text == NULL ? "" : actual->text
        );
        return 1;
    }

    if (actual->length != expected_length ||
        (expected_length > 0U && memcmp(actual->text, expected->text, expected_length) != 0)) {
        fprintf(
            stderr,
            "token %zu in '%s': expected text '%s', got '%.*s'\n",
            index,
            sql,
            expected->text,
            (int)actual->length,
            actual->text == NULL ? "" : actual->text
        );
        return 1;
    }

    if (actual->operator_kind != expected->operator_kind) {
        fprintf(
            stderr,
            "token %zu in '%s': expected operator %s, got %s\n",
            index,
            sql,
            mylite_sql_operator_kind_name(expected->operator_kind),
            mylite_sql_operator_kind_name(actual->operator_kind)
        );
        return 1;
    }

    if (actual->error != expected->error) {
        fprintf(
            stderr,
            "token %zu in '%s': expected error %s, got %s\n",
            index,
            sql,
            mylite_sql_lexer_error_name(expected->error),
            mylite_sql_lexer_error_name(actual->error)
        );
        return 1;
    }

    if ((actual->keyword_flags & expected->keyword_flags) != expected->keyword_flags) {
        fprintf(
            stderr,
            "token %zu in '%s': expected keyword flags 0x%x, got 0x%x\n",
            index,
            sql,
            expected->keyword_flags,
            actual->keyword_flags
        );
        return 1;
    }

    return 0;
}

static int load_file(const char *path, char **out_data, size_t *out_length) {
    FILE *file = fopen(path, "rb");
    long file_size = 0;
    char *data = NULL;
    size_t bytes_read = 0U;

    if (file == NULL) {
        fprintf(stderr, "failed to open %s\n", path);
        return 1;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fprintf(stderr, "failed to seek %s\n", path);
        fclose(file);
        return 1;
    }

    file_size = ftell(file);
    if (file_size < 0) {
        fprintf(stderr, "failed to tell %s\n", path);
        fclose(file);
        return 1;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fprintf(stderr, "failed to rewind %s\n", path);
        fclose(file);
        return 1;
    }

    data = malloc((size_t)file_size + 1U);
    if (data == NULL) {
        fprintf(stderr, "failed to allocate %ld bytes\n", file_size);
        fclose(file);
        return 1;
    }

    bytes_read = fread(data, 1U, (size_t)file_size, file);
    if (bytes_read != (size_t)file_size) {
        fprintf(stderr, "failed to read %s\n", path);
        free(data);
        fclose(file);
        return 1;
    }
    fclose(file);
    *out_data = data;
    *out_length = bytes_read;
    return 0;
}
