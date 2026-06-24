#include "mylite_lexer.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

struct mylite_keyword_entry {
    const char *word;
    unsigned int flags;
};

struct mylite_token_start {
    size_t offset;
    unsigned int flags;
};

struct mylite_quoted_string_options {
    enum mylite_sql_token_kind kind;
    char quote;
    bool allow_backslash;
};

struct mylite_binary_literal_options {
    enum mylite_sql_token_kind kind;
    bool hex;
};

static const unsigned char mysql_comment_space_max = 0x20U;
static const unsigned char mysql_non_ascii_min = 0x80U;

static bool scan_comment(
    struct mylite_sql_lexer *lexer,
    unsigned int flags,
    struct mylite_sql_token *out_token
);
static bool starts_line_comment(const struct mylite_sql_lexer *lexer);
static void scan_line_comment_body(struct mylite_sql_lexer *lexer);
static enum mylite_sql_token_kind block_comment_token_kind(const struct mylite_sql_lexer *lexer);
static bool scan_block_comment_body(
    struct mylite_sql_lexer *lexer,
    enum mylite_sql_token_kind kind
);
static bool starts_block_comment(const struct mylite_sql_lexer *lexer);
static bool ends_block_comment(const struct mylite_sql_lexer *lexer);
static void scan_inner_block_comment_body(struct mylite_sql_lexer *lexer);
static void advance_two(struct mylite_sql_lexer *lexer);
static void advance_to_offset_no_newline(struct mylite_sql_lexer *lexer, size_t offset);
static bool scan_word(
    struct mylite_sql_lexer *lexer,
    unsigned int flags,
    struct mylite_sql_token *out_token
);
static bool scan_digit_leading_token(
    struct mylite_sql_lexer *lexer,
    unsigned int flags,
    struct mylite_sql_token *out_token
);
static bool scan_dot_or_number(
    struct mylite_sql_lexer *lexer,
    unsigned int flags,
    struct mylite_sql_token *out_token
);
static bool scan_variable(
    struct mylite_sql_lexer *lexer,
    unsigned int flags,
    struct mylite_sql_token *out_token
);
static bool scan_operator_or_punctuation(
    struct mylite_sql_lexer *lexer,
    unsigned int flags,
    struct mylite_sql_token *out_token
);

struct mylite_sql_operator_match {
    enum mylite_sql_operator_kind kind;
    size_t length;
};

struct mylite_sql_operator_bytes {
    unsigned char first;
    unsigned char second;
    unsigned char third;
};
static struct mylite_sql_operator_match classify_operator(struct mylite_sql_operator_bytes bytes);
static struct mylite_sql_operator_match classify_minus_operator(
    struct mylite_sql_operator_bytes bytes
);
static struct mylite_sql_operator_match classify_less_operator(
    struct mylite_sql_operator_bytes bytes
);
static struct mylite_sql_operator_match classify_greater_operator(
    struct mylite_sql_operator_bytes bytes
);
static struct mylite_sql_operator_match classify_bang_operator(
    struct mylite_sql_operator_bytes bytes
);
static struct mylite_sql_operator_match classify_ampersand_operator(
    struct mylite_sql_operator_bytes bytes
);
static struct mylite_sql_operator_match classify_pipe_operator(
    struct mylite_sql_operator_bytes bytes
);
static struct mylite_sql_operator_match classify_colon_operator(
    struct mylite_sql_operator_bytes bytes
);
static struct mylite_sql_operator_match make_operator_match(
    enum mylite_sql_operator_kind kind,
    size_t length
);
static bool scan_quoted_string(
    struct mylite_sql_lexer *lexer,
    struct mylite_token_start start,
    struct mylite_quoted_string_options options,
    struct mylite_sql_token *out_token
);
static bool scan_quoted_identifier(
    struct mylite_sql_lexer *lexer,
    struct mylite_token_start start,
    char quote,
    struct mylite_sql_token *out_token
);
static bool scan_quoted_hex_or_bit_literal(
    struct mylite_sql_lexer *lexer,
    struct mylite_token_start start,
    struct mylite_binary_literal_options options,
    struct mylite_sql_token *out_token
);
static bool scan_prefixed_hex_or_bit_literal(
    struct mylite_sql_lexer *lexer,
    struct mylite_token_start start,
    struct mylite_binary_literal_options options,
    struct mylite_sql_token *out_token
);
static bool scan_digit_leading_identifier(
    struct mylite_sql_lexer *lexer,
    unsigned int flags,
    struct mylite_sql_token *out_token
);
static bool scan_unquoted_identifier(
    struct mylite_sql_lexer *lexer,
    unsigned int flags,
    struct mylite_sql_token *out_token
);
static bool identifier_span_is_charset_introducer(
    const struct mylite_sql_lexer *lexer,
    size_t start,
    size_t end
);
static bool starts_literal_after_charset_introducer(
    const struct mylite_sql_lexer *lexer,
    size_t cursor
);
static bool identifier_span_is_temporal_literal_introducer(
    const struct mylite_sql_lexer *lexer,
    size_t start,
    size_t end
);
static bool identifier_span_equals_keyword(
    const struct mylite_sql_lexer *lexer,
    size_t start,
    size_t end,
    const char *keyword
);
static bool scan_system_variable(
    struct mylite_sql_lexer *lexer,
    struct mylite_token_start start,
    struct mylite_sql_token *out_token
);
static bool scan_quoted_system_variable_component(struct mylite_sql_lexer *lexer);
static bool scan_quoted_variable(
    struct mylite_sql_lexer *lexer,
    struct mylite_token_start start,
    char quote,
    struct mylite_sql_token *out_token
);
static struct mylite_token_start make_token_start(
    const struct mylite_sql_lexer *lexer,
    unsigned int flags
);
static void set_token(
    const struct mylite_sql_lexer *lexer,
    struct mylite_sql_token *out_token,
    enum mylite_sql_token_kind kind,
    struct mylite_token_start start
);
static void set_error_token(
    const struct mylite_sql_lexer *lexer,
    struct mylite_sql_token *out_token,
    enum mylite_sql_lexer_error error,
    struct mylite_token_start start
);
static bool consume_whitespace(struct mylite_sql_lexer *lexer);
static void advance_one(struct mylite_sql_lexer *lexer);
static unsigned char peek_at(const struct mylite_sql_lexer *lexer, size_t lookahead);
static bool has_at(const struct mylite_sql_lexer *lexer, size_t lookahead);
static bool starts_comment(const struct mylite_sql_lexer *lexer);
static bool is_mysql_comment_space(unsigned char byte);
static bool is_space(unsigned char byte);
static bool is_digit(unsigned char byte);
static bool is_hex_digit(unsigned char byte);
static bool is_bit_digit(unsigned char byte);
static bool is_identifier_start(unsigned char byte);
static bool is_identifier_part(unsigned char byte);
static bool is_user_variable_part(unsigned char byte);
static bool is_punctuation(unsigned char byte);
static bool is_exponent_marker(unsigned char byte);
static bool is_sign(unsigned char byte);
static bool is_binary_literal_digit(unsigned char byte, bool hex);
static enum mylite_sql_lexer_error binary_literal_error(bool hex);
static size_t skip_digits(const char *input, size_t length, size_t cursor);
static size_t skip_identifier_parts(const char *input, size_t length, size_t cursor);
static size_t skip_user_variable_parts(const char *input, size_t length, size_t cursor);
static bool scan_exponent_span(const char *input, size_t length, size_t *cursor);

struct mylite_sql_keyword_range {
    size_t low;
    size_t high;
    bool found;
};
static struct mylite_sql_keyword_range keyword_search_range(unsigned char first);
static bool keyword_length_is_possible(unsigned char first, size_t length);
static unsigned int lookup_hot_keyword_index(const char *text, size_t length, unsigned char first);

struct mylite_sql_keyword_second_char_probe {
    unsigned char first;
    unsigned char second;
    size_t length;
};

static bool keyword_second_char_is_possible(struct mylite_sql_keyword_second_char_probe probe);
static int compare_keyword_text(const char *text, size_t length, const char *keyword);
static char ascii_upper(unsigned char byte);

void mylite_sql_lexer_init(struct mylite_sql_lexer *lexer, struct mylite_sql_lexer_config config) {
    if (lexer == NULL) {
        return;
    }

    lexer->input = config.input;
    lexer->length = config.input == NULL ? 0U : config.length;
    lexer->offset = 0U;
    lexer->modes = config.modes;
}

int mylite_sql_lexer_next(struct mylite_sql_lexer *lexer, struct mylite_sql_token *out_token) {
    unsigned int flags = 0U;
    unsigned char byte = 0U;

    if (lexer == NULL || out_token == NULL) {
        return 1;
    }

    if (consume_whitespace(lexer)) {
        flags |= MYLITE_SQL_TOKEN_HAS_LEADING_SPACE;
    }

    if (lexer->offset >= lexer->length) {
        set_token(lexer, out_token, MYLITE_SQL_TOKEN_EOF, make_token_start(lexer, flags));
        return 0;
    }

    byte = peek_at(lexer, 0U);
    if (starts_comment(lexer)) {
        (void)scan_comment(lexer, flags, out_token);
        return 0;
    }

    if (byte == '`') {
        (void)scan_quoted_identifier(lexer, make_token_start(lexer, flags), '`', out_token);
        return 0;
    }

    if (byte == '\'' || byte == '"') {
        if (byte == '"' && (lexer->modes & MYLITE_SQL_MODE_ANSI_QUOTES) != 0U) {
            (void)scan_quoted_identifier(lexer, make_token_start(lexer, flags), '"', out_token);
        } else {
            (void)scan_quoted_string(
                lexer,
                make_token_start(lexer, flags),
                (struct mylite_quoted_string_options){
                    .kind = MYLITE_SQL_TOKEN_STRING,
                    .quote = (char)byte,
                    .allow_backslash = (lexer->modes & MYLITE_SQL_MODE_NO_BACKSLASH_ESCAPES) == 0U,
                },
                out_token
            );
        }
        return 0;
    }

    if (is_identifier_start(byte)) {
        if (scan_word(lexer, flags, out_token)) {
            return 0;
        }
    }

    if (is_digit(byte)) {
        (void)scan_digit_leading_token(lexer, flags, out_token);
        return 0;
    }

    if (byte == '.') {
        (void)scan_dot_or_number(lexer, flags, out_token);
        return 0;
    }

    if (byte == '@') {
        (void)scan_variable(lexer, flags, out_token);
        return 0;
    }

    if (byte == '?') {
        struct mylite_token_start start = make_token_start(lexer, flags);
        advance_one(lexer);
        set_token(lexer, out_token, MYLITE_SQL_TOKEN_PARAMETER, start);
        return 0;
    }

    if (scan_operator_or_punctuation(lexer, flags, out_token)) {
        return 0;
    }

    {
        struct mylite_token_start start = make_token_start(lexer, flags);
        advance_one(lexer);
        set_error_token(lexer, out_token, MYLITE_SQL_LEXER_ERROR_UNEXPECTED_BYTE, start);
    }
    return 0;
}

bool mylite_sql_keyword_lookup(
    const char *text,
    size_t length,
    struct mylite_sql_keyword_lookup_result *out_result
) {
    enum { maximum_keyword_length = 38 };

    struct mylite_sql_keyword_range range = {0};
    unsigned char first = 0U;

    static const struct mylite_keyword_entry keywords[] = {
        {"ABS", 0U},
        {"ACCESSIBLE", MYLITE_SQL_KEYWORD_RESERVED},
        {"ACCOUNT", 0U},
        {"ACOS", 0U},
        {"ACTION", 0U},
        {"ACTIVE", 0U},
        {"ADD", MYLITE_SQL_KEYWORD_RESERVED},
        {"ADDDATE", 0U},
        {"ADDTIME", 0U},
        {"ADMIN", 0U},
        {"AFTER", 0U},
        {"AGAINST", 0U},
        {"AGGREGATE", 0U},
        {"ALGORITHM", 0U},
        {"ALL", MYLITE_SQL_KEYWORD_RESERVED},
        {"ALTER", MYLITE_SQL_KEYWORD_RESERVED},
        {"ALWAYS", 0U},
        {"ANALYZE", MYLITE_SQL_KEYWORD_RESERVED},
        {"AND", MYLITE_SQL_KEYWORD_RESERVED},
        {"ANY", 0U},
        {"ANY_VALUE", 0U},
        {"ARRAY", 0U},
        {"AS", MYLITE_SQL_KEYWORD_RESERVED},
        {"ASC", MYLITE_SQL_KEYWORD_RESERVED},
        {"ASCII", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"ASENSITIVE", MYLITE_SQL_KEYWORD_RESERVED},
        {"ASIN", 0U},
        {"ASSIGN_GTIDS_TO_ANONYMOUS_TRANSACTIONS", 0U},
        {"AT", 0U},
        {"ATAN", 0U},
        {"ATAN2", 0U},
        {"ATTRIBUTE", 0U},
        {"AUTHENTICATION", 0U},
        {"AUTO", 0U},
        {"AUTOEXTEND_SIZE", 0U},
        {"AUTO_INCREMENT", 0U},
        {"AVG", 0U},
        {"AVG_ROW_LENGTH", 0U},
        {"BACKUP", 0U},
        {"BEFORE", MYLITE_SQL_KEYWORD_RESERVED},
        {"BEGIN", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"BERNOULLI", 0U},
        {"BETWEEN", MYLITE_SQL_KEYWORD_RESERVED},
        {"BIGINT", MYLITE_SQL_KEYWORD_RESERVED},
        {"BIN", 0U},
        {"BINARY", MYLITE_SQL_KEYWORD_RESERVED},
        {"BINLOG", 0U},
        {"BIN_TO_UUID", 0U},
        {"BIT", 0U},
        {"BIT_AND", 0U},
        {"BIT_COUNT", 0U},
        {"BIT_LENGTH", 0U},
        {"BIT_OR", 0U},
        {"BIT_XOR", 0U},
        {"BLOB", MYLITE_SQL_KEYWORD_RESERVED},
        {"BLOCK", 0U},
        {"BOOL", 0U},
        {"BOOLEAN", 0U},
        {"BOTH", MYLITE_SQL_KEYWORD_RESERVED},
        {"BTREE", 0U},
        {"BUCKETS", 0U},
        {"BULK", 0U},
        {"BY", MYLITE_SQL_KEYWORD_RESERVED},
        {"BYTE", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"CACHE", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"CALL", MYLITE_SQL_KEYWORD_RESERVED},
        {"CASCADE", MYLITE_SQL_KEYWORD_RESERVED},
        {"CASCADED", 0U},
        {"CASE", MYLITE_SQL_KEYWORD_RESERVED},
        {"CAST", 0U},
        {"CATALOG_NAME", 0U},
        {"CEIL", 0U},
        {"CEILING", 0U},
        {"CHAIN", 0U},
        {"CHALLENGE_RESPONSE", 0U},
        {"CHANGE", MYLITE_SQL_KEYWORD_RESERVED},
        {"CHANGED", 0U},
        {"CHANNEL", 0U},
        {"CHAR", MYLITE_SQL_KEYWORD_RESERVED},
        {"CHARACTER", MYLITE_SQL_KEYWORD_RESERVED},
        {"CHARACTER_LENGTH", 0U},
        {"CHARSET", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"CHAR_LENGTH", 0U},
        {"CHECK", MYLITE_SQL_KEYWORD_RESERVED},
        {"CHECKSUM", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"CIPHER", 0U},
        {"CLASS_ORIGIN", 0U},
        {"CLIENT", 0U},
        {"CLONE", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"CLOSE", 0U},
        {"COALESCE", 0U},
        {"CODE", 0U},
        {"COERCIBILITY", 0U},
        {"COLLATE", MYLITE_SQL_KEYWORD_RESERVED},
        {"COLLATION", 0U},
        {"COLUMN", MYLITE_SQL_KEYWORD_RESERVED},
        {"COLUMNS", 0U},
        {"COLUMN_FORMAT", 0U},
        {"COLUMN_NAME", 0U},
        {"COMMENT", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"COMMIT", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"COMMITTED", 0U},
        {"COMPACT", 0U},
        {"COMPLETION", 0U},
        {"COMPONENT", 0U},
        {"COMPRESS", 0U},
        {"COMPRESSED", 0U},
        {"COMPRESSION", 0U},
        {"CONCAT", 0U},
        {"CONCAT_WS", 0U},
        {"CONCURRENT", 0U},
        {"CONDITION", MYLITE_SQL_KEYWORD_RESERVED},
        {"CONNECTION", 0U},
        {"CONNECTION_ID", 0U},
        {"CONSISTENT", 0U},
        {"CONSTRAINT", MYLITE_SQL_KEYWORD_RESERVED},
        {"CONSTRAINT_CATALOG", 0U},
        {"CONSTRAINT_NAME", 0U},
        {"CONSTRAINT_SCHEMA", 0U},
        {"CONTAINS", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"CONTEXT", 0U},
        {"CONTINUE", MYLITE_SQL_KEYWORD_RESERVED},
        {"CONV", 0U},
        {"CONVERT", MYLITE_SQL_KEYWORD_RESERVED},
        {"CONVERT_TZ", 0U},
        {"COS", 0U},
        {"COT", 0U},
        {"COUNT", 0U},
        {"CPU", 0U},
        {"CRC32", 0U},
        {"CREATE", MYLITE_SQL_KEYWORD_RESERVED},
        {"CROSS", MYLITE_SQL_KEYWORD_RESERVED},
        {"CUBE", 0U},
        {"CUME_DIST", MYLITE_SQL_KEYWORD_RESERVED},
        {"CURDATE", 0U},
        {"CURRENT", 0U},
        {"CURRENT_DATE", MYLITE_SQL_KEYWORD_RESERVED},
        {"CURRENT_ROLE", 0U},
        {"CURRENT_TIME", MYLITE_SQL_KEYWORD_RESERVED},
        {"CURRENT_TIMESTAMP", MYLITE_SQL_KEYWORD_RESERVED},
        {"CURRENT_USER", MYLITE_SQL_KEYWORD_RESERVED},
        {"CURSOR", MYLITE_SQL_KEYWORD_RESERVED},
        {"CURSOR_NAME", 0U},
        {"CURTIME", 0U},
        {"DATA", 0U},
        {"DATABASE", MYLITE_SQL_KEYWORD_RESERVED},
        {"DATABASES", MYLITE_SQL_KEYWORD_RESERVED},
        {"DATAFILE", 0U},
        {"DATE", 0U},
        {"DATEDIFF", 0U},
        {"DATETIME", 0U},
        {"DATE_ADD", 0U},
        {"DATE_FORMAT", 0U},
        {"DATE_SUB", 0U},
        {"DAY", 0U},
        {"DAYNAME", 0U},
        {"DAYOFMONTH", 0U},
        {"DAYOFWEEK", 0U},
        {"DAYOFYEAR", 0U},
        {"DAY_HOUR", MYLITE_SQL_KEYWORD_RESERVED},
        {"DAY_MICROSECOND", MYLITE_SQL_KEYWORD_RESERVED},
        {"DAY_MINUTE", MYLITE_SQL_KEYWORD_RESERVED},
        {"DAY_SECOND", MYLITE_SQL_KEYWORD_RESERVED},
        {"DEALLOCATE", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"DEC", MYLITE_SQL_KEYWORD_RESERVED},
        {"DECIMAL", MYLITE_SQL_KEYWORD_RESERVED},
        {"DECLARE", MYLITE_SQL_KEYWORD_RESERVED},
        {"DEFAULT", MYLITE_SQL_KEYWORD_RESERVED},
        {"DEFAULT_AUTH", 0U},
        {"DEFINER", 0U},
        {"DEFINITION", 0U},
        {"DEGREES", 0U},
        {"DELAYED", MYLITE_SQL_KEYWORD_RESERVED},
        {"DELAY_KEY_WRITE", 0U},
        {"DELETE", MYLITE_SQL_KEYWORD_RESERVED},
        {"DENSE_RANK", MYLITE_SQL_KEYWORD_RESERVED},
        {"DESC", MYLITE_SQL_KEYWORD_RESERVED},
        {"DESCRIBE", MYLITE_SQL_KEYWORD_RESERVED},
        {"DESCRIPTION", 0U},
        {"DETERMINISTIC", MYLITE_SQL_KEYWORD_RESERVED},
        {"DIAGNOSTICS", 0U},
        {"DIRECTORY", 0U},
        {"DISABLE", 0U},
        {"DISCARD", 0U},
        {"DISK", 0U},
        {"DISTINCT", MYLITE_SQL_KEYWORD_RESERVED},
        {"DISTINCTROW", MYLITE_SQL_KEYWORD_RESERVED},
        {"DIV", MYLITE_SQL_KEYWORD_RESERVED},
        {"DO", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"DOUBLE", MYLITE_SQL_KEYWORD_RESERVED},
        {"DROP", MYLITE_SQL_KEYWORD_RESERVED},
        {"DUAL", MYLITE_SQL_KEYWORD_RESERVED},
        {"DUMPFILE", 0U},
        {"DUPLICATE", 0U},
        {"DYNAMIC", 0U},
        {"EACH", MYLITE_SQL_KEYWORD_RESERVED},
        {"ELSE", MYLITE_SQL_KEYWORD_RESERVED},
        {"ELSEIF", MYLITE_SQL_KEYWORD_RESERVED},
        {"ELT", 0U},
        {"EMPTY", MYLITE_SQL_KEYWORD_RESERVED},
        {"ENABLE", 0U},
        {"ENCLOSED", MYLITE_SQL_KEYWORD_RESERVED},
        {"ENCRYPTION", 0U},
        {"END", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"ENDS", 0U},
        {"ENFORCED", 0U},
        {"ENGINE", 0U},
        {"ENGINES", 0U},
        {"ENGINE_ATTRIBUTE", 0U},
        {"ENUM", 0U},
        {"ERROR", 0U},
        {"ERRORS", 0U},
        {"ESCAPE", 0U},
        {"ESCAPED", MYLITE_SQL_KEYWORD_RESERVED},
        {"EVENT", MYLITE_SQL_KEYWORD_RESTRICTED_ROLE},
        {"EVENTS", 0U},
        {"EVERY", 0U},
        {"EXCEPT", MYLITE_SQL_KEYWORD_RESERVED},
        {"EXCHANGE", 0U},
        {"EXCLUDE", 0U},
        {"EXECUTE", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL | MYLITE_SQL_KEYWORD_RESTRICTED_ROLE},
        {"EXISTS", MYLITE_SQL_KEYWORD_RESERVED},
        {"EXIT", MYLITE_SQL_KEYWORD_RESERVED},
        {"EXP", 0U},
        {"EXPANSION", 0U},
        {"EXPIRE", 0U},
        {"EXPLAIN", MYLITE_SQL_KEYWORD_RESERVED},
        {"EXPORT", 0U},
        {"EXPORT_SET", 0U},
        {"EXTENDED", 0U},
        {"EXTENT_SIZE", 0U},
        {"EXTRACT", 0U},
        {"FACTOR", 0U},
        {"FAILED_LOGIN_ATTEMPTS", 0U},
        {"FALSE", MYLITE_SQL_KEYWORD_RESERVED},
        {"FAST", 0U},
        {"FAULTS", 0U},
        {"FETCH", MYLITE_SQL_KEYWORD_RESERVED},
        {"FIELD", 0U},
        {"FIELDS", 0U},
        {"FILE", MYLITE_SQL_KEYWORD_RESTRICTED_ROLE},
        {"FILE_BLOCK_SIZE", 0U},
        {"FILTER", 0U},
        {"FIND_IN_SET", 0U},
        {"FINISH", 0U},
        {"FIRST", 0U},
        {"FIRST_VALUE", MYLITE_SQL_KEYWORD_RESERVED},
        {"FIXED", 0U},
        {"FLOAT", MYLITE_SQL_KEYWORD_RESERVED},
        {"FLOAT4", MYLITE_SQL_KEYWORD_RESERVED},
        {"FLOAT8", MYLITE_SQL_KEYWORD_RESERVED},
        {"FLOOR", 0U},
        {"FLUSH", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"FOLLOWING", 0U},
        {"FOLLOWS", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"FOR", MYLITE_SQL_KEYWORD_RESERVED},
        {"FORCE", MYLITE_SQL_KEYWORD_RESERVED},
        {"FOREIGN", MYLITE_SQL_KEYWORD_RESERVED},
        {"FORMAT", 0U},
        {"FOUND", 0U},
        {"FOUND_ROWS", 0U},
        {"FROM", MYLITE_SQL_KEYWORD_RESERVED},
        {"FROM_BASE64", 0U},
        {"FROM_DAYS", 0U},
        {"FROM_UNIXTIME", 0U},
        {"FULL", 0U},
        {"FULLTEXT", MYLITE_SQL_KEYWORD_RESERVED},
        {"FUNCTION", MYLITE_SQL_KEYWORD_RESERVED},
        {"GENERAL", 0U},
        {"GENERATE", 0U},
        {"GENERATED", MYLITE_SQL_KEYWORD_RESERVED},
        {"GEOMCOLLECTION", 0U},
        {"GEOMETRY", 0U},
        {"GEOMETRYCOLLECTION", 0U},
        {"GET", MYLITE_SQL_KEYWORD_RESERVED},
        {"GET_FORMAT", 0U},
        {"GET_SOURCE_PUBLIC_KEY", 0U},
        {"GLOBAL", 0U},
        {"GRANT", MYLITE_SQL_KEYWORD_RESERVED},
        {"GRANTS", 0U},
        {"GREATEST", 0U},
        {"GROUP", MYLITE_SQL_KEYWORD_RESERVED},
        {"GROUPING", MYLITE_SQL_KEYWORD_RESERVED},
        {"GROUPS", MYLITE_SQL_KEYWORD_RESERVED},
        {"GROUP_CONCAT", 0U},
        {"GROUP_REPLICATION", 0U},
        {"GTIDS", 0U},
        {"GTID_ONLY", 0U},
        {"HANDLER", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"HASH", 0U},
        {"HAVING", MYLITE_SQL_KEYWORD_RESERVED},
        {"HELP", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"HEX", 0U},
        {"HIGH_PRIORITY", MYLITE_SQL_KEYWORD_RESERVED},
        {"HISTOGRAM", 0U},
        {"HISTORY", 0U},
        {"HOST", 0U},
        {"HOSTS", 0U},
        {"HOUR", 0U},
        {"HOUR_MICROSECOND", MYLITE_SQL_KEYWORD_RESERVED},
        {"HOUR_MINUTE", MYLITE_SQL_KEYWORD_RESERVED},
        {"HOUR_SECOND", MYLITE_SQL_KEYWORD_RESERVED},
        {"IDENTIFIED", 0U},
        {"IF", MYLITE_SQL_KEYWORD_RESERVED},
        {"IFNULL", 0U},
        {"IGNORE", MYLITE_SQL_KEYWORD_RESERVED},
        {"IGNORE_SERVER_IDS", 0U},
        {"IMPORT", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"IN", MYLITE_SQL_KEYWORD_RESERVED},
        {"INACTIVE", 0U},
        {"INDEX", MYLITE_SQL_KEYWORD_RESERVED},
        {"INDEXES", 0U},
        {"INFILE", MYLITE_SQL_KEYWORD_RESERVED},
        {"INITIAL", 0U},
        {"INITIAL_SIZE", 0U},
        {"INITIATE", 0U},
        {"INNER", MYLITE_SQL_KEYWORD_RESERVED},
        {"INOUT", MYLITE_SQL_KEYWORD_RESERVED},
        {"INSENSITIVE", MYLITE_SQL_KEYWORD_RESERVED},
        {"INSERT", MYLITE_SQL_KEYWORD_RESERVED},
        {"INSERT_METHOD", 0U},
        {"INSTALL", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"INSTANCE", 0U},
        {"INSTR", 0U},
        {"INT", MYLITE_SQL_KEYWORD_RESERVED},
        {"INT1", MYLITE_SQL_KEYWORD_RESERVED},
        {"INT2", MYLITE_SQL_KEYWORD_RESERVED},
        {"INT3", MYLITE_SQL_KEYWORD_RESERVED},
        {"INT4", MYLITE_SQL_KEYWORD_RESERVED},
        {"INT8", MYLITE_SQL_KEYWORD_RESERVED},
        {"INTEGER", MYLITE_SQL_KEYWORD_RESERVED},
        {"INTERSECT", MYLITE_SQL_KEYWORD_RESERVED},
        {"INTERVAL", MYLITE_SQL_KEYWORD_RESERVED},
        {"INTO", MYLITE_SQL_KEYWORD_RESERVED},
        {"INVISIBLE", 0U},
        {"INVOKER", 0U},
        {"IO", 0U},
        {"IO_AFTER_GTIDS", MYLITE_SQL_KEYWORD_RESERVED},
        {"IO_BEFORE_GTIDS", MYLITE_SQL_KEYWORD_RESERVED},
        {"IO_THREAD", 0U},
        {"IPC", 0U},
        {"IS", MYLITE_SQL_KEYWORD_RESERVED},
        {"ISNULL", 0U},
        {"ISOLATION", 0U},
        {"ISSUER", 0U},
        {"IS_UUID", 0U},
        {"ITERATE", MYLITE_SQL_KEYWORD_RESERVED},
        {"JOIN", MYLITE_SQL_KEYWORD_RESERVED},
        {"JSON", 0U},
        {"JSON_ARRAY", 0U},
        {"JSON_CONTAINS", 0U},
        {"JSON_CONTAINS_PATH", 0U},
        {"JSON_EXTRACT", 0U},
        {"JSON_INSERT", 0U},
        {"JSON_KEYS", 0U},
        {"JSON_LENGTH", 0U},
        {"JSON_OBJECT", 0U},
        {"JSON_QUOTE", 0U},
        {"JSON_REMOVE", 0U},
        {"JSON_REPLACE", 0U},
        {"JSON_SET", 0U},
        {"JSON_TABLE", MYLITE_SQL_KEYWORD_RESERVED},
        {"JSON_TYPE", 0U},
        {"JSON_UNQUOTE", 0U},
        {"JSON_VALID", 0U},
        {"JSON_VALUE", 0U},
        {"KEY", MYLITE_SQL_KEYWORD_RESERVED},
        {"KEYRING", 0U},
        {"KEYS", MYLITE_SQL_KEYWORD_RESERVED},
        {"KEY_BLOCK_SIZE", 0U},
        {"KILL", MYLITE_SQL_KEYWORD_RESERVED},
        {"LAG", MYLITE_SQL_KEYWORD_RESERVED},
        {"LANGUAGE", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"LAST", 0U},
        {"LAST_DAY", 0U},
        {"LAST_INSERT_ID", 0U},
        {"LAST_VALUE", MYLITE_SQL_KEYWORD_RESERVED},
        {"LATERAL", MYLITE_SQL_KEYWORD_RESERVED},
        {"LCASE", 0U},
        {"LEAD", MYLITE_SQL_KEYWORD_RESERVED},
        {"LEADING", MYLITE_SQL_KEYWORD_RESERVED},
        {"LEAST", 0U},
        {"LEAVE", MYLITE_SQL_KEYWORD_RESERVED},
        {"LEAVES", 0U},
        {"LEFT", MYLITE_SQL_KEYWORD_RESERVED},
        {"LENGTH", 0U},
        {"LESS", 0U},
        {"LEVEL", 0U},
        {"LIKE", MYLITE_SQL_KEYWORD_RESERVED},
        {"LIMIT", MYLITE_SQL_KEYWORD_RESERVED},
        {"LINEAR", MYLITE_SQL_KEYWORD_RESERVED},
        {"LINES", MYLITE_SQL_KEYWORD_RESERVED},
        {"LINESTRING", 0U},
        {"LIST", 0U},
        {"LN", 0U},
        {"LOAD", MYLITE_SQL_KEYWORD_RESERVED},
        {"LOCAL", 0U},
        {"LOCALTIME", MYLITE_SQL_KEYWORD_RESERVED},
        {"LOCALTIMESTAMP", MYLITE_SQL_KEYWORD_RESERVED},
        {"LOCATE", 0U},
        {"LOCK", MYLITE_SQL_KEYWORD_RESERVED},
        {"LOCKED", 0U},
        {"LOCKS", 0U},
        {"LOG", 0U},
        {"LOG10", 0U},
        {"LOG2", 0U},
        {"LOGFILE", 0U},
        {"LOGS", 0U},
        {"LONG", MYLITE_SQL_KEYWORD_RESERVED},
        {"LONGBLOB", MYLITE_SQL_KEYWORD_RESERVED},
        {"LONGTEXT", MYLITE_SQL_KEYWORD_RESERVED},
        {"LOOP", MYLITE_SQL_KEYWORD_RESERVED},
        {"LOWER", 0U},
        {"LOW_PRIORITY", MYLITE_SQL_KEYWORD_RESERVED},
        {"LPAD", 0U},
        {"LTRIM", 0U},
        {"MAKEDATE", 0U},
        {"MAKETIME", 0U},
        {"MAKE_SET", 0U},
        {"MANUAL", 0U},
        {"MASTER", 0U},
        {"MATCH", MYLITE_SQL_KEYWORD_RESERVED},
        {"MAX", 0U},
        {"MAXVALUE", MYLITE_SQL_KEYWORD_RESERVED},
        {"MAX_CONNECTIONS_PER_HOUR", 0U},
        {"MAX_QUERIES_PER_HOUR", 0U},
        {"MAX_ROWS", 0U},
        {"MAX_SIZE", 0U},
        {"MAX_UPDATES_PER_HOUR", 0U},
        {"MAX_USER_CONNECTIONS", 0U},
        {"MD5", 0U},
        {"MEDIUM", 0U},
        {"MEDIUMBLOB", MYLITE_SQL_KEYWORD_RESERVED},
        {"MEDIUMINT", MYLITE_SQL_KEYWORD_RESERVED},
        {"MEDIUMTEXT", MYLITE_SQL_KEYWORD_RESERVED},
        {"MEMBER", 0U},
        {"MEMORY", 0U},
        {"MERGE", 0U},
        {"MESSAGE_TEXT", 0U},
        {"MICROSECOND", 0U},
        {"MID", 0U},
        {"MIDDLEINT", MYLITE_SQL_KEYWORD_RESERVED},
        {"MIGRATE", 0U},
        {"MIN", 0U},
        {"MINUTE", 0U},
        {"MINUTE_MICROSECOND", MYLITE_SQL_KEYWORD_RESERVED},
        {"MINUTE_SECOND", MYLITE_SQL_KEYWORD_RESERVED},
        {"MIN_ROWS", 0U},
        {"MOD", MYLITE_SQL_KEYWORD_RESERVED},
        {"MODE", 0U},
        {"MODIFIES", MYLITE_SQL_KEYWORD_RESERVED},
        {"MODIFY", 0U},
        {"MONTH", 0U},
        {"MONTHNAME", 0U},
        {"MULTILINESTRING", 0U},
        {"MULTIPOINT", 0U},
        {"MULTIPOLYGON", 0U},
        {"MUTEX", 0U},
        {"MYSQL_ERRNO", 0U},
        {"NAME", 0U},
        {"NAMES", 0U},
        {"NATIONAL", 0U},
        {"NATURAL", MYLITE_SQL_KEYWORD_RESERVED},
        {"NCHAR", 0U},
        {"NDB", 0U},
        {"NDBCLUSTER", 0U},
        {"NESTED", 0U},
        {"NETWORK_NAMESPACE", 0U},
        {"NEVER", 0U},
        {"NEW", 0U},
        {"NEXT", 0U},
        {"NO", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"NODEGROUP", 0U},
        {"NONE", MYLITE_SQL_KEYWORD_RESTRICTED_ROLE},
        {"NOT", MYLITE_SQL_KEYWORD_RESERVED},
        {"NOW", 0U},
        {"NOWAIT", 0U},
        {"NO_WAIT", 0U},
        {"NO_WRITE_TO_BINLOG", MYLITE_SQL_KEYWORD_RESERVED},
        {"NTH_VALUE", MYLITE_SQL_KEYWORD_RESERVED},
        {"NTILE", MYLITE_SQL_KEYWORD_RESERVED},
        {"NULL", MYLITE_SQL_KEYWORD_RESERVED},
        {"NULLIF", 0U},
        {"NULLS", 0U},
        {"NUMBER", 0U},
        {"NUMERIC", MYLITE_SQL_KEYWORD_RESERVED},
        {"NVARCHAR", 0U},
        {"OCT", 0U},
        {"OCTET_LENGTH", 0U},
        {"OF", MYLITE_SQL_KEYWORD_RESERVED},
        {"OFF", 0U},
        {"OFFSET", 0U},
        {"OJ", 0U},
        {"OLD", 0U},
        {"ON", MYLITE_SQL_KEYWORD_RESERVED},
        {"ONE", 0U},
        {"ONLY", 0U},
        {"OPEN", 0U},
        {"OPTIMIZE", MYLITE_SQL_KEYWORD_RESERVED},
        {"OPTIMIZER_COSTS", MYLITE_SQL_KEYWORD_RESERVED},
        {"OPTION", MYLITE_SQL_KEYWORD_RESERVED},
        {"OPTIONAL", 0U},
        {"OPTIONALLY", MYLITE_SQL_KEYWORD_RESERVED},
        {"OPTIONS", 0U},
        {"OR", MYLITE_SQL_KEYWORD_RESERVED},
        {"ORD", 0U},
        {"ORDER", MYLITE_SQL_KEYWORD_RESERVED},
        {"ORDINALITY", 0U},
        {"ORGANIZATION", 0U},
        {"OTHERS", 0U},
        {"OUT", MYLITE_SQL_KEYWORD_RESERVED},
        {"OUTER", MYLITE_SQL_KEYWORD_RESERVED},
        {"OUTFILE", MYLITE_SQL_KEYWORD_RESERVED},
        {"OVER", MYLITE_SQL_KEYWORD_RESERVED},
        {"OWNER", 0U},
        {"PACK_KEYS", 0U},
        {"PAGE", 0U},
        {"PARALLEL", 0U},
        {"PARSER", 0U},
        {"PARSE_TREE", 0U},
        {"PARTIAL", 0U},
        {"PARTITION", MYLITE_SQL_KEYWORD_RESERVED},
        {"PARTITIONING", 0U},
        {"PARTITIONS", 0U},
        {"PASSWORD", 0U},
        {"PASSWORD_LOCK_TIME", 0U},
        {"PATH", 0U},
        {"PERCENT_RANK", MYLITE_SQL_KEYWORD_RESERVED},
        {"PERIOD_ADD", 0U},
        {"PERIOD_DIFF", 0U},
        {"PERSIST", 0U},
        {"PERSIST_ONLY", 0U},
        {"PHASE", 0U},
        {"PI", 0U},
        {"PLUGIN", 0U},
        {"PLUGINS", 0U},
        {"PLUGIN_DIR", 0U},
        {"POINT", 0U},
        {"POLYGON", 0U},
        {"PORT", 0U},
        {"POSITION", 0U},
        {"POW", 0U},
        {"POWER", 0U},
        {"PRECEDES", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"PRECEDING", 0U},
        {"PRECISION", MYLITE_SQL_KEYWORD_RESERVED},
        {"PREPARE", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"PRESERVE", 0U},
        {"PREV", 0U},
        {"PRIMARY", MYLITE_SQL_KEYWORD_RESERVED},
        {"PRIVILEGES", 0U},
        {"PRIVILEGE_CHECKS_USER", 0U},
        {"PROCEDURE", MYLITE_SQL_KEYWORD_RESERVED},
        {"PROCESS", MYLITE_SQL_KEYWORD_RESTRICTED_ROLE},
        {"PROCESSLIST", 0U},
        {"PROFILE", 0U},
        {"PROFILES", 0U},
        {"PROXY", MYLITE_SQL_KEYWORD_RESTRICTED_ROLE},
        {"PURGE", MYLITE_SQL_KEYWORD_RESERVED},
        {"QUALIFY", 0U},
        {"QUARTER", 0U},
        {"QUERY", 0U},
        {"QUICK", 0U},
        {"QUOTE", 0U},
        {"RADIANS", 0U},
        {"RAND", 0U},
        {"RANDOM", 0U},
        {"RANDOM_BYTES", 0U},
        {"RANGE", MYLITE_SQL_KEYWORD_RESERVED},
        {"RANK", MYLITE_SQL_KEYWORD_RESERVED},
        {"READ", MYLITE_SQL_KEYWORD_RESERVED},
        {"READS", MYLITE_SQL_KEYWORD_RESERVED},
        {"READ_ONLY", 0U},
        {"READ_WRITE", MYLITE_SQL_KEYWORD_RESERVED},
        {"REAL", MYLITE_SQL_KEYWORD_RESERVED},
        {"REBUILD", 0U},
        {"RECOVER", 0U},
        {"RECURSIVE", MYLITE_SQL_KEYWORD_RESERVED},
        {"REDO_BUFFER_SIZE", 0U},
        {"REDUNDANT", 0U},
        {"REFERENCE", 0U},
        {"REFERENCES", MYLITE_SQL_KEYWORD_RESERVED},
        {"REGEXP", MYLITE_SQL_KEYWORD_RESERVED},
        {"REGEXP_INSTR", 0U},
        {"REGEXP_LIKE", 0U},
        {"REGEXP_REPLACE", 0U},
        {"REGEXP_SUBSTR", 0U},
        {"REGISTRATION", 0U},
        {"RELAY", 0U},
        {"RELAYLOG", 0U},
        {"RELAY_LOG_FILE", 0U},
        {"RELAY_LOG_POS", 0U},
        {"RELAY_THREAD", 0U},
        {"RELEASE", MYLITE_SQL_KEYWORD_RESERVED},
        {"RELOAD", MYLITE_SQL_KEYWORD_RESTRICTED_ROLE},
        {"REMOVE", 0U},
        {"RENAME", MYLITE_SQL_KEYWORD_RESERVED},
        {"REORGANIZE", 0U},
        {"REPAIR", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"REPEAT", MYLITE_SQL_KEYWORD_RESERVED},
        {"REPEATABLE", 0U},
        {"REPLACE", MYLITE_SQL_KEYWORD_RESERVED},
        {"REPLICA", 0U},
        {"REPLICAS", 0U},
        {"REPLICATE_DO_DB", 0U},
        {"REPLICATE_DO_TABLE", 0U},
        {"REPLICATE_IGNORE_DB", 0U},
        {"REPLICATE_IGNORE_TABLE", 0U},
        {"REPLICATE_REWRITE_DB", 0U},
        {"REPLICATE_WILD_DO_TABLE", 0U},
        {"REPLICATE_WILD_IGNORE_TABLE", 0U},
        {"REPLICATION", MYLITE_SQL_KEYWORD_RESTRICTED_ROLE},
        {"REQUIRE", MYLITE_SQL_KEYWORD_RESERVED},
        {"REQUIRE_ROW_FORMAT", 0U},
        {"REQUIRE_TABLE_PRIMARY_KEY_CHECK", 0U},
        {"RESET", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"RESIGNAL", MYLITE_SQL_KEYWORD_RESERVED},
        {"RESOURCE", MYLITE_SQL_KEYWORD_RESTRICTED_ROLE},
        {"RESPECT", 0U},
        {"RESTART", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL | MYLITE_SQL_KEYWORD_RESTRICTED_ROLE},
        {"RESTORE", 0U},
        {"RESTRICT", MYLITE_SQL_KEYWORD_RESERVED},
        {"RESUME", 0U},
        {"RETAIN", 0U},
        {"RETURN", MYLITE_SQL_KEYWORD_RESERVED},
        {"RETURNED_SQLSTATE", 0U},
        {"RETURNING", 0U},
        {"RETURNS", 0U},
        {"REUSE", 0U},
        {"REVERSE", 0U},
        {"REVOKE", MYLITE_SQL_KEYWORD_RESERVED},
        {"RIGHT", MYLITE_SQL_KEYWORD_RESERVED},
        {"RLIKE", MYLITE_SQL_KEYWORD_RESERVED},
        {"ROLE", 0U},
        {"ROLLBACK", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"ROLLUP", 0U},
        {"ROTATE", 0U},
        {"ROUND", 0U},
        {"ROUTINE", 0U},
        {"ROW", MYLITE_SQL_KEYWORD_RESERVED},
        {"ROWS", MYLITE_SQL_KEYWORD_RESERVED},
        {"ROW_COUNT", 0U},
        {"ROW_FORMAT", 0U},
        {"ROW_NUMBER", MYLITE_SQL_KEYWORD_RESERVED},
        {"RPAD", 0U},
        {"RTREE", 0U},
        {"RTRIM", 0U},
        {"S3", 0U},
        {"SAVEPOINT", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"SCHEDULE", 0U},
        {"SCHEMA", MYLITE_SQL_KEYWORD_RESERVED},
        {"SCHEMAS", MYLITE_SQL_KEYWORD_RESERVED},
        {"SCHEMA_NAME", 0U},
        {"SECOND", 0U},
        {"SECONDARY", 0U},
        {"SECONDARY_ENGINE", 0U},
        {"SECONDARY_ENGINE_ATTRIBUTE", 0U},
        {"SECONDARY_LOAD", 0U},
        {"SECONDARY_UNLOAD", 0U},
        {"SECOND_MICROSECOND", MYLITE_SQL_KEYWORD_RESERVED},
        {"SECURITY", 0U},
        {"SEC_TO_TIME", 0U},
        {"SELECT", MYLITE_SQL_KEYWORD_RESERVED},
        {"SENSITIVE", MYLITE_SQL_KEYWORD_RESERVED},
        {"SEPARATOR", MYLITE_SQL_KEYWORD_RESERVED},
        {"SERIAL", 0U},
        {"SERIALIZABLE", 0U},
        {"SERVER", 0U},
        {"SESSION", 0U},
        {"SESSION_USER", 0U},
        {"SET", MYLITE_SQL_KEYWORD_RESERVED},
        {"SHA", 0U},
        {"SHA1", 0U},
        {"SHA2", 0U},
        {"SHARE", 0U},
        {"SHOW", MYLITE_SQL_KEYWORD_RESERVED},
        {"SHUTDOWN", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL | MYLITE_SQL_KEYWORD_RESTRICTED_ROLE},
        {"SIGN", 0U},
        {"SIGNAL", MYLITE_SQL_KEYWORD_RESERVED},
        {"SIGNED", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"SIMPLE", 0U},
        {"SIN", 0U},
        {"SKIP", 0U},
        {"SLAVE", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"SLOW", 0U},
        {"SMALLINT", MYLITE_SQL_KEYWORD_RESERVED},
        {"SNAPSHOT", 0U},
        {"SOCKET", 0U},
        {"SOME", 0U},
        {"SONAME", 0U},
        {"SOUNDEX", 0U},
        {"SOUNDS", 0U},
        {"SOURCE", 0U},
        {"SOURCE_AUTO_POSITION", 0U},
        {"SOURCE_BIND", 0U},
        {"SOURCE_COMPRESSION_ALGORITHMS", 0U},
        {"SOURCE_CONNECTION_AUTO_FAILOVER", 0U},
        {"SOURCE_CONNECT_RETRY", 0U},
        {"SOURCE_DELAY", 0U},
        {"SOURCE_HEARTBEAT_PERIOD", 0U},
        {"SOURCE_HOST", 0U},
        {"SOURCE_LOG_FILE", 0U},
        {"SOURCE_LOG_POS", 0U},
        {"SOURCE_PASSWORD", 0U},
        {"SOURCE_PORT", 0U},
        {"SOURCE_PUBLIC_KEY_PATH", 0U},
        {"SOURCE_RETRY_COUNT", 0U},
        {"SOURCE_SSL", 0U},
        {"SOURCE_SSL_CA", 0U},
        {"SOURCE_SSL_CAPATH", 0U},
        {"SOURCE_SSL_CERT", 0U},
        {"SOURCE_SSL_CIPHER", 0U},
        {"SOURCE_SSL_CRL", 0U},
        {"SOURCE_SSL_CRLPATH", 0U},
        {"SOURCE_SSL_KEY", 0U},
        {"SOURCE_SSL_VERIFY_SERVER_CERT", 0U},
        {"SOURCE_TLS_CIPHERSUITES", 0U},
        {"SOURCE_TLS_VERSION", 0U},
        {"SOURCE_USER", 0U},
        {"SOURCE_ZSTD_COMPRESSION_LEVEL", 0U},
        {"SPACE", 0U},
        {"SPATIAL", MYLITE_SQL_KEYWORD_RESERVED},
        {"SPECIFIC", MYLITE_SQL_KEYWORD_RESERVED},
        {"SQL", MYLITE_SQL_KEYWORD_RESERVED},
        {"SQLEXCEPTION", MYLITE_SQL_KEYWORD_RESERVED},
        {"SQLSTATE", MYLITE_SQL_KEYWORD_RESERVED},
        {"SQLWARNING", MYLITE_SQL_KEYWORD_RESERVED},
        {"SQL_AFTER_GTIDS", 0U},
        {"SQL_AFTER_MTS_GAPS", 0U},
        {"SQL_BEFORE_GTIDS", 0U},
        {"SQL_BIG_RESULT", MYLITE_SQL_KEYWORD_RESERVED},
        {"SQL_BUFFER_RESULT", 0U},
        {"SQL_CALC_FOUND_ROWS", MYLITE_SQL_KEYWORD_RESERVED},
        {"SQL_NO_CACHE", 0U},
        {"SQL_SMALL_RESULT", MYLITE_SQL_KEYWORD_RESERVED},
        {"SQL_THREAD", 0U},
        {"SQL_TSI_DAY", 0U},
        {"SQL_TSI_HOUR", 0U},
        {"SQL_TSI_MINUTE", 0U},
        {"SQL_TSI_MONTH", 0U},
        {"SQL_TSI_QUARTER", 0U},
        {"SQL_TSI_SECOND", 0U},
        {"SQL_TSI_WEEK", 0U},
        {"SQL_TSI_YEAR", 0U},
        {"SQRT", 0U},
        {"SRID", 0U},
        {"SSL", MYLITE_SQL_KEYWORD_RESERVED},
        {"STACKED", 0U},
        {"START", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"STARTING", MYLITE_SQL_KEYWORD_RESERVED},
        {"STARTS", 0U},
        {"STATS_AUTO_RECALC", 0U},
        {"STATS_PERSISTENT", 0U},
        {"STATS_SAMPLE_PAGES", 0U},
        {"STATUS", 0U},
        {"STOP", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"STORAGE", 0U},
        {"STORED", MYLITE_SQL_KEYWORD_RESERVED},
        {"STRAIGHT_JOIN", MYLITE_SQL_KEYWORD_RESERVED},
        {"STRCMP", 0U},
        {"STREAM", 0U},
        {"STRING", 0U},
        {"STR_TO_DATE", 0U},
        {"SUBCLASS_ORIGIN", 0U},
        {"SUBDATE", 0U},
        {"SUBJECT", 0U},
        {"SUBPARTITION", 0U},
        {"SUBPARTITIONS", 0U},
        {"SUBSTR", 0U},
        {"SUBSTRING", 0U},
        {"SUBSTRING_INDEX", 0U},
        {"SUBTIME", 0U},
        {"SUM", 0U},
        {"SUPER", MYLITE_SQL_KEYWORD_RESTRICTED_ROLE},
        {"SUSPEND", 0U},
        {"SWAPS", 0U},
        {"SWITCHES", 0U},
        {"SYSDATE", 0U},
        {"SYSTEM", MYLITE_SQL_KEYWORD_RESERVED},
        {"SYSTEM_USER", 0U},
        {"TABLE", MYLITE_SQL_KEYWORD_RESERVED},
        {"TABLES", 0U},
        {"TABLESAMPLE", 0U},
        {"TABLESPACE", 0U},
        {"TABLE_CHECKSUM", 0U},
        {"TABLE_NAME", 0U},
        {"TAN", 0U},
        {"TEMPORARY", 0U},
        {"TEMPTABLE", 0U},
        {"TERMINATED", MYLITE_SQL_KEYWORD_RESERVED},
        {"TEXT", 0U},
        {"THAN", 0U},
        {"THEN", MYLITE_SQL_KEYWORD_RESERVED},
        {"THREAD_PRIORITY", 0U},
        {"TIES", 0U},
        {"TIME", 0U},
        {"TIMEDIFF", 0U},
        {"TIMESTAMP", 0U},
        {"TIMESTAMPADD", 0U},
        {"TIMESTAMPDIFF", 0U},
        {"TIME_FORMAT", 0U},
        {"TIME_TO_SEC", 0U},
        {"TINYBLOB", MYLITE_SQL_KEYWORD_RESERVED},
        {"TINYINT", MYLITE_SQL_KEYWORD_RESERVED},
        {"TINYTEXT", MYLITE_SQL_KEYWORD_RESERVED},
        {"TLS", 0U},
        {"TO", MYLITE_SQL_KEYWORD_RESERVED},
        {"TO_BASE64", 0U},
        {"TO_DAYS", 0U},
        {"TO_SECONDS", 0U},
        {"TRAILING", MYLITE_SQL_KEYWORD_RESERVED},
        {"TRANSACTION", 0U},
        {"TRIGGER", MYLITE_SQL_KEYWORD_RESERVED},
        {"TRIGGERS", 0U},
        {"TRIM", 0U},
        {"TRUE", MYLITE_SQL_KEYWORD_RESERVED},
        {"TRUNCATE", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"TYPE", 0U},
        {"TYPES", 0U},
        {"UCASE", 0U},
        {"UNBOUNDED", 0U},
        {"UNCOMMITTED", 0U},
        {"UNCOMPRESS", 0U},
        {"UNCOMPRESSED_LENGTH", 0U},
        {"UNDEFINED", 0U},
        {"UNDO", MYLITE_SQL_KEYWORD_RESERVED},
        {"UNDOFILE", 0U},
        {"UNDO_BUFFER_SIZE", 0U},
        {"UNHEX", 0U},
        {"UNICODE", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"UNINSTALL", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"UNION", MYLITE_SQL_KEYWORD_RESERVED},
        {"UNIQUE", MYLITE_SQL_KEYWORD_RESERVED},
        {"UNIX_TIMESTAMP", 0U},
        {"UNKNOWN", 0U},
        {"UNLOCK", MYLITE_SQL_KEYWORD_RESERVED},
        {"UNREGISTER", 0U},
        {"UNSIGNED", MYLITE_SQL_KEYWORD_RESERVED},
        {"UNTIL", 0U},
        {"UPDATE", MYLITE_SQL_KEYWORD_RESERVED},
        {"UPGRADE", 0U},
        {"UPPER", 0U},
        {"URL", 0U},
        {"USAGE", MYLITE_SQL_KEYWORD_RESERVED},
        {"USE", MYLITE_SQL_KEYWORD_RESERVED},
        {"USER", 0U},
        {"USER_RESOURCES", 0U},
        {"USE_FRM", 0U},
        {"USING", MYLITE_SQL_KEYWORD_RESERVED},
        {"UTC", 0U},
        {"UTC_DATE", MYLITE_SQL_KEYWORD_RESERVED},
        {"UTC_TIME", MYLITE_SQL_KEYWORD_RESERVED},
        {"UTC_TIMESTAMP", MYLITE_SQL_KEYWORD_RESERVED},
        {"UUID", 0U},
        {"UUID_TO_BIN", 0U},
        {"VALIDATION", 0U},
        {"VALUE", 0U},
        {"VALUES", MYLITE_SQL_KEYWORD_RESERVED},
        {"VARBINARY", MYLITE_SQL_KEYWORD_RESERVED},
        {"VARCHAR", MYLITE_SQL_KEYWORD_RESERVED},
        {"VARCHARACTER", MYLITE_SQL_KEYWORD_RESERVED},
        {"VARIABLES", 0U},
        {"VARYING", MYLITE_SQL_KEYWORD_RESERVED},
        {"VCPU", 0U},
        {"VERSION", 0U},
        {"VIEW", 0U},
        {"VIRTUAL", MYLITE_SQL_KEYWORD_RESERVED},
        {"VISIBLE", 0U},
        {"WAIT", 0U},
        {"WARNINGS", 0U},
        {"WEEK", 0U},
        {"WEEKDAY", 0U},
        {"WEEKOFYEAR", 0U},
        {"WEIGHT_STRING", 0U},
        {"WHEN", MYLITE_SQL_KEYWORD_RESERVED},
        {"WHERE", MYLITE_SQL_KEYWORD_RESERVED},
        {"WHILE", MYLITE_SQL_KEYWORD_RESERVED},
        {"WINDOW", MYLITE_SQL_KEYWORD_RESERVED},
        {"WITH", MYLITE_SQL_KEYWORD_RESERVED},
        {"WITHOUT", 0U},
        {"WORK", 0U},
        {"WRAPPER", 0U},
        {"WRITE", MYLITE_SQL_KEYWORD_RESERVED},
        {"X509", 0U},
        {"XA", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"XID", 0U},
        {"XML", 0U},
        {"XOR", MYLITE_SQL_KEYWORD_RESERVED},
        {"YEAR", 0U},
        {"YEARWEEK", 0U},
        {"YEAR_MONTH", MYLITE_SQL_KEYWORD_RESERVED},
        {"ZEROFILL", MYLITE_SQL_KEYWORD_RESERVED},
        {"ZONE", 0U},
        {"_FILENAME", MYLITE_SQL_KEYWORD_RESERVED},
        {"JSON_DEPTH", 0U},
        {"JSON_PRETTY", 0U},
        {"JSON_OVERLAPS", 0U}
    };

    if (out_result != NULL) {
        *out_result = (struct mylite_sql_keyword_lookup_result){
            .flags = 0U,
            .keyword_index = (unsigned int)-1,
        };
    }

    if (text == NULL || length == 0U || length > maximum_keyword_length) {
        return false;
    }

    first = (unsigned char)ascii_upper((unsigned char)text[0]);
    range = keyword_search_range(first);
    if (!range.found || !keyword_length_is_possible(first, length)) {
        return false;
    }

    {
        unsigned int hot_index = lookup_hot_keyword_index(text, length, first);
        if (hot_index != (unsigned int)-1) {
            if (out_result != NULL) {
                *out_result = (struct mylite_sql_keyword_lookup_result){
                    .flags = keywords[hot_index].flags,
                    .keyword_index = hot_index,
                };
            }
            return true;
        }
    }

    if (!keyword_second_char_is_possible((struct mylite_sql_keyword_second_char_probe){
            .first = first,
            .second = length > 1U ? (unsigned char)text[1] : 0U,
            .length = length,
        })) {
        return false;
    }

    while (range.low < range.high) {
        size_t middle = range.low + ((range.high - range.low) / 2U);
        int compare = compare_keyword_text(text, length, keywords[middle].word);
        if (compare == 0) {
            if (out_result != NULL) {
                *out_result = (struct mylite_sql_keyword_lookup_result){
                    .flags = keywords[middle].flags,
                    .keyword_index = (unsigned int)middle,
                };
            }
            return true;
        }
        if (compare < 0) {
            range.high = middle;
        } else {
            range.low = middle + 1U;
        }
    }

    return false;
}

const char *mylite_sql_token_kind_name(enum mylite_sql_token_kind kind) {
    switch (kind) {
    case MYLITE_SQL_TOKEN_EOF:
        return "eof";
    case MYLITE_SQL_TOKEN_ERROR:
        return "error";
    case MYLITE_SQL_TOKEN_COMMENT:
        return "comment";
    case MYLITE_SQL_TOKEN_VERSION_COMMENT:
        return "version_comment";
    case MYLITE_SQL_TOKEN_HINT_COMMENT:
        return "hint_comment";
    case MYLITE_SQL_TOKEN_IDENTIFIER:
        return "identifier";
    case MYLITE_SQL_TOKEN_QUOTED_IDENTIFIER:
        return "quoted_identifier";
    case MYLITE_SQL_TOKEN_KEYWORD:
        return "keyword";
    case MYLITE_SQL_TOKEN_STRING:
        return "string";
    case MYLITE_SQL_TOKEN_NATIONAL_STRING:
        return "national_string";
    case MYLITE_SQL_TOKEN_HEX_LITERAL:
        return "hex_literal";
    case MYLITE_SQL_TOKEN_BIT_LITERAL:
        return "bit_literal";
    case MYLITE_SQL_TOKEN_INTEGER:
        return "integer";
    case MYLITE_SQL_TOKEN_DECIMAL:
        return "decimal";
    case MYLITE_SQL_TOKEN_FLOAT:
        return "float";
    case MYLITE_SQL_TOKEN_USER_VARIABLE:
        return "user_variable";
    case MYLITE_SQL_TOKEN_SYSTEM_VARIABLE:
        return "system_variable";
    case MYLITE_SQL_TOKEN_PARAMETER:
        return "parameter";
    case MYLITE_SQL_TOKEN_OPERATOR:
        return "operator";
    case MYLITE_SQL_TOKEN_PUNCTUATION:
        return "punctuation";
    case MYLITE_SQL_TOKEN_CHARSET_INTRODUCER:
        return "charset_introducer";
    case MYLITE_SQL_TOKEN_TEMPORAL_LITERAL_INTRODUCER:
        return "temporal_literal_introducer";
    }

    return "unknown";
}

const char *mylite_sql_operator_kind_name(enum mylite_sql_operator_kind kind) {
    switch (kind) {
    case MYLITE_SQL_OPERATOR_NONE:
        return "none";
    case MYLITE_SQL_OPERATOR_JSON_UNQUOTE_EXTRACT:
        return "json_unquote_extract";
    case MYLITE_SQL_OPERATOR_JSON_EXTRACT:
        return "json_extract";
    case MYLITE_SQL_OPERATOR_NULL_SAFE_EQUAL:
        return "null_safe_equal";
    case MYLITE_SQL_OPERATOR_LEFT_SHIFT:
        return "left_shift";
    case MYLITE_SQL_OPERATOR_RIGHT_SHIFT:
        return "right_shift";
    case MYLITE_SQL_OPERATOR_LESS_EQUAL:
        return "less_equal";
    case MYLITE_SQL_OPERATOR_GREATER_EQUAL:
        return "greater_equal";
    case MYLITE_SQL_OPERATOR_NOT_EQUAL:
        return "not_equal";
    case MYLITE_SQL_OPERATOR_LOGICAL_AND:
        return "logical_and";
    case MYLITE_SQL_OPERATOR_LOGICAL_OR:
        return "logical_or";
    case MYLITE_SQL_OPERATOR_ASSIGN:
        return "assign";
    case MYLITE_SQL_OPERATOR_EQUAL:
        return "equal";
    case MYLITE_SQL_OPERATOR_LESS:
        return "less";
    case MYLITE_SQL_OPERATOR_GREATER:
        return "greater";
    case MYLITE_SQL_OPERATOR_PLUS:
        return "plus";
    case MYLITE_SQL_OPERATOR_MINUS:
        return "minus";
    case MYLITE_SQL_OPERATOR_STAR:
        return "star";
    case MYLITE_SQL_OPERATOR_SLASH:
        return "slash";
    case MYLITE_SQL_OPERATOR_PERCENT:
        return "percent";
    case MYLITE_SQL_OPERATOR_NOT:
        return "not";
    case MYLITE_SQL_OPERATOR_BITWISE_NOT:
        return "bitwise_not";
    case MYLITE_SQL_OPERATOR_BITWISE_XOR:
        return "bitwise_xor";
    case MYLITE_SQL_OPERATOR_BITWISE_AND:
        return "bitwise_and";
    case MYLITE_SQL_OPERATOR_BITWISE_OR:
        return "bitwise_or";
    }

    return "unknown";
}

const char *mylite_sql_lexer_error_name(enum mylite_sql_lexer_error error) {
    switch (error) {
    case MYLITE_SQL_LEXER_ERROR_NONE:
        return "none";
    case MYLITE_SQL_LEXER_ERROR_UNEXPECTED_BYTE:
        return "unexpected_byte";
    case MYLITE_SQL_LEXER_ERROR_UNTERMINATED_STRING:
        return "unterminated_string";
    case MYLITE_SQL_LEXER_ERROR_UNTERMINATED_IDENTIFIER:
        return "unterminated_identifier";
    case MYLITE_SQL_LEXER_ERROR_UNTERMINATED_COMMENT:
        return "unterminated_comment";
    case MYLITE_SQL_LEXER_ERROR_INVALID_HEX_LITERAL:
        return "invalid_hex_literal";
    case MYLITE_SQL_LEXER_ERROR_INVALID_BIT_LITERAL:
        return "invalid_bit_literal";
    case MYLITE_SQL_LEXER_ERROR_INVALID_VARIABLE:
        return "invalid_variable";
    }

    return "unknown";
}

static bool scan_comment(
    struct mylite_sql_lexer *lexer,
    unsigned int flags,
    struct mylite_sql_token *out_token
) {
    struct mylite_token_start start = make_token_start(lexer, flags);
    enum mylite_sql_token_kind kind = MYLITE_SQL_TOKEN_COMMENT;

    assert(lexer != NULL);
    assert(out_token != NULL);

    if (starts_line_comment(lexer)) {
        scan_line_comment_body(lexer);
        set_token(lexer, out_token, kind, start);
        return true;
    }

    kind = block_comment_token_kind(lexer);
    advance_two(lexer);
    if (scan_block_comment_body(lexer, kind)) {
        set_token(lexer, out_token, kind, start);
        return true;
    }

    set_error_token(lexer, out_token, MYLITE_SQL_LEXER_ERROR_UNTERMINATED_COMMENT, start);
    return true;
}

static bool starts_line_comment(const struct mylite_sql_lexer *lexer) {
    return peek_at(lexer, 0U) == '#' || (peek_at(lexer, 0U) == '-' && peek_at(lexer, 1U) == '-');
}

static void scan_line_comment_body(struct mylite_sql_lexer *lexer) {
    while (lexer->offset < lexer->length) {
        unsigned char byte = peek_at(lexer, 0U);
        if (byte == '\n' || byte == '\r') {
            break;
        }
        advance_one(lexer);
    }
}

static enum mylite_sql_token_kind block_comment_token_kind(const struct mylite_sql_lexer *lexer) {
    if (peek_at(lexer, 2U) == '!') {
        return MYLITE_SQL_TOKEN_VERSION_COMMENT;
    }
    if (peek_at(lexer, 2U) == '+') {
        return MYLITE_SQL_TOKEN_HINT_COMMENT;
    }
    return MYLITE_SQL_TOKEN_COMMENT;
}

static bool scan_block_comment_body(
    struct mylite_sql_lexer *lexer,
    enum mylite_sql_token_kind kind
) {
    while (lexer->offset < lexer->length) {
        if (kind == MYLITE_SQL_TOKEN_VERSION_COMMENT && starts_block_comment(lexer)) {
            scan_inner_block_comment_body(lexer);
            continue;
        }
        if (ends_block_comment(lexer)) {
            advance_two(lexer);
            return true;
        }
        advance_one(lexer);
    }
    return false;
}

static bool starts_block_comment(const struct mylite_sql_lexer *lexer) {
    return peek_at(lexer, 0U) == '/' && peek_at(lexer, 1U) == '*';
}

static bool ends_block_comment(const struct mylite_sql_lexer *lexer) {
    return peek_at(lexer, 0U) == '*' && peek_at(lexer, 1U) == '/';
}

static void scan_inner_block_comment_body(struct mylite_sql_lexer *lexer) {
    advance_two(lexer);
    while (lexer->offset < lexer->length) {
        if (ends_block_comment(lexer)) {
            advance_two(lexer);
            break;
        }
        advance_one(lexer);
    }
}

static void advance_two(struct mylite_sql_lexer *lexer) {
    advance_one(lexer);
    advance_one(lexer);
}

static void advance_to_offset_no_newline(struct mylite_sql_lexer *lexer, size_t offset) {
    assert(lexer != NULL);
    assert(offset >= lexer->offset);
    assert(offset <= lexer->length);

    lexer->offset = offset;
}

static bool scan_word(
    struct mylite_sql_lexer *lexer,
    unsigned int flags,
    struct mylite_sql_token *out_token
) {
    unsigned char byte = peek_at(lexer, 0U);
    unsigned char next = peek_at(lexer, 1U);
    struct mylite_token_start start = make_token_start(lexer, flags);

    if ((byte == 'N' || byte == 'n') && next == '\'') {
        advance_one(lexer);
        (void)scan_quoted_string(
            lexer,
            start,
            (struct mylite_quoted_string_options){
                .kind = MYLITE_SQL_TOKEN_NATIONAL_STRING,
                .quote = '\'',
                .allow_backslash = (lexer->modes & MYLITE_SQL_MODE_NO_BACKSLASH_ESCAPES) == 0U,
            },
            out_token
        );
        return true;
    }

    if ((byte == 'X' || byte == 'x') && next == '\'') {
        advance_one(lexer);
        (void)scan_quoted_hex_or_bit_literal(
            lexer,
            start,
            (struct mylite_binary_literal_options){
                .kind = MYLITE_SQL_TOKEN_HEX_LITERAL,
                .hex = true,
            },
            out_token
        );
        return true;
    }

    if ((byte == 'B' || byte == 'b') && next == '\'') {
        advance_one(lexer);
        (void)scan_quoted_hex_or_bit_literal(
            lexer,
            start,
            (struct mylite_binary_literal_options){
                .kind = MYLITE_SQL_TOKEN_BIT_LITERAL,
                .hex = false,
            },
            out_token
        );
        return true;
    }

    return scan_unquoted_identifier(lexer, flags, out_token);
}

static bool scan_digit_leading_token(
    struct mylite_sql_lexer *lexer,
    unsigned int flags,
    struct mylite_sql_token *out_token
) {
    struct mylite_token_start start = make_token_start(lexer, flags);
    size_t cursor = lexer->offset;
    bool saw_dot = false;
    bool saw_exponent = false;
    enum mylite_sql_token_kind kind = MYLITE_SQL_TOKEN_INTEGER;

    if (lexer->input[cursor] == '0' && cursor + 2U < lexer->length &&
        lexer->input[cursor + 1U] == 'x' &&
        is_hex_digit((unsigned char)lexer->input[cursor + 2U])) {
        return scan_prefixed_hex_or_bit_literal(
            lexer,
            start,
            (struct mylite_binary_literal_options){
                .kind = MYLITE_SQL_TOKEN_HEX_LITERAL,
                .hex = true,
            },
            out_token
        );
    }

    if (lexer->input[cursor] == '0' && cursor + 2U < lexer->length &&
        lexer->input[cursor + 1U] == 'b' &&
        is_bit_digit((unsigned char)lexer->input[cursor + 2U])) {
        return scan_prefixed_hex_or_bit_literal(
            lexer,
            start,
            (struct mylite_binary_literal_options){
                .kind = MYLITE_SQL_TOKEN_BIT_LITERAL,
                .hex = false,
            },
            out_token
        );
    }

    cursor = skip_digits(lexer->input, lexer->length, cursor);

    if (cursor < lexer->length && is_exponent_marker((unsigned char)lexer->input[cursor])) {
        if (!scan_exponent_span(lexer->input, lexer->length, &cursor)) {
            return scan_digit_leading_identifier(lexer, flags, out_token);
        }
        saw_exponent = true;
    }

    if (!saw_exponent && cursor < lexer->length && lexer->input[cursor] == '.') {
        saw_dot = true;
        ++cursor;
        cursor = skip_digits(lexer->input, lexer->length, cursor);
    }

    if (!saw_exponent && cursor < lexer->length &&
        is_exponent_marker((unsigned char)lexer->input[cursor]) &&
        scan_exponent_span(lexer->input, lexer->length, &cursor)) {
        saw_exponent = true;
    }

    if (!saw_dot && !saw_exponent && cursor < lexer->length &&
        is_identifier_part((unsigned char)lexer->input[cursor])) {
        return scan_digit_leading_identifier(lexer, flags, out_token);
    }

    advance_to_offset_no_newline(lexer, cursor);

    if (saw_exponent) {
        kind = MYLITE_SQL_TOKEN_FLOAT;
    } else if (saw_dot) {
        kind = MYLITE_SQL_TOKEN_DECIMAL;
    }

    set_token(lexer, out_token, kind, start);
    return true;
}

static bool scan_dot_or_number(
    struct mylite_sql_lexer *lexer,
    unsigned int flags,
    struct mylite_sql_token *out_token
) {
    struct mylite_token_start start = make_token_start(lexer, flags);
    size_t cursor = lexer->offset;
    bool saw_exponent = false;
    enum mylite_sql_token_kind kind = MYLITE_SQL_TOKEN_DECIMAL;

    if (!is_digit(peek_at(lexer, 1U))) {
        advance_one(lexer);
        set_token(lexer, out_token, MYLITE_SQL_TOKEN_PUNCTUATION, start);
        return true;
    }

    ++cursor;
    cursor = skip_digits(lexer->input, lexer->length, cursor);

    if (cursor < lexer->length && is_exponent_marker((unsigned char)lexer->input[cursor]) &&
        scan_exponent_span(lexer->input, lexer->length, &cursor)) {
        saw_exponent = true;
    }
    advance_to_offset_no_newline(lexer, cursor);

    if (saw_exponent) {
        kind = MYLITE_SQL_TOKEN_FLOAT;
    }

    set_token(lexer, out_token, kind, start);
    return true;
}

static bool scan_variable(
    struct mylite_sql_lexer *lexer,
    unsigned int flags,
    struct mylite_sql_token *out_token
) {
    struct mylite_token_start start = make_token_start(lexer, flags);

    assert(peek_at(lexer, 0U) == '@');

    advance_one(lexer);
    if (peek_at(lexer, 0U) == '@') {
        advance_one(lexer);
        return scan_system_variable(lexer, start, out_token);
    }

    if (peek_at(lexer, 0U) == '\'' || peek_at(lexer, 0U) == '"' || peek_at(lexer, 0U) == '`') {
        return scan_quoted_variable(lexer, start, (char)peek_at(lexer, 0U), out_token);
    }

    if (!is_user_variable_part(peek_at(lexer, 0U))) {
        set_token(lexer, out_token, MYLITE_SQL_TOKEN_USER_VARIABLE, start);
        return true;
    }

    advance_to_offset_no_newline(
        lexer,
        skip_user_variable_parts(lexer->input, lexer->length, lexer->offset)
    );
    set_token(lexer, out_token, MYLITE_SQL_TOKEN_USER_VARIABLE, start);
    return true;
}

static bool scan_system_variable(
    struct mylite_sql_lexer *lexer,
    struct mylite_token_start start,
    struct mylite_sql_token *out_token
) {
    unsigned char byte = peek_at(lexer, 0U);

    if (!is_identifier_part(byte) && byte != '.' && byte != '`') {
        set_error_token(lexer, out_token, MYLITE_SQL_LEXER_ERROR_INVALID_VARIABLE, start);
        return true;
    }

    while (lexer->offset < lexer->length) {
        byte = peek_at(lexer, 0U);
        if (is_identifier_part(byte) || byte == '.') {
            advance_one(lexer);
            continue;
        }
        if (byte == '`') {
            if (!scan_quoted_system_variable_component(lexer)) {
                set_error_token(lexer, out_token, MYLITE_SQL_LEXER_ERROR_INVALID_VARIABLE, start);
                return true;
            }
            continue;
        }
        break;
    }

    set_token(lexer, out_token, MYLITE_SQL_TOKEN_SYSTEM_VARIABLE, start);
    return true;
}

static bool scan_quoted_system_variable_component(struct mylite_sql_lexer *lexer) {
    assert(peek_at(lexer, 0U) == '`');

    advance_one(lexer);
    while (lexer->offset < lexer->length) {
        if (peek_at(lexer, 0U) == '`') {
            advance_one(lexer);
            if (peek_at(lexer, 0U) == '`') {
                advance_one(lexer);
                continue;
            }
            return true;
        }
        advance_one(lexer);
    }

    return false;
}

static bool scan_operator_or_punctuation(
    struct mylite_sql_lexer *lexer,
    unsigned int flags,
    struct mylite_sql_token *out_token
) {
    struct mylite_token_start start = make_token_start(lexer, flags);
    struct mylite_sql_operator_match operator_match =
        classify_operator((struct mylite_sql_operator_bytes){
            .first = peek_at(lexer, 0U),
            .second = peek_at(lexer, 1U),
            .third = peek_at(lexer, 2U),
        });

    if (operator_match.kind != MYLITE_SQL_OPERATOR_NONE) {
        advance_to_offset_no_newline(lexer, lexer->offset + operator_match.length);
        set_token(lexer, out_token, MYLITE_SQL_TOKEN_OPERATOR, start);
        out_token->operator_kind = operator_match.kind;
        return true;
    }

    if (is_punctuation(peek_at(lexer, 0U))) {
        advance_to_offset_no_newline(lexer, lexer->offset + 1U);
        set_token(lexer, out_token, MYLITE_SQL_TOKEN_PUNCTUATION, start);
        return true;
    }

    return false;
}

static struct mylite_sql_operator_match classify_operator(struct mylite_sql_operator_bytes bytes) {
    switch (bytes.first) {
    case '-':
        return classify_minus_operator(bytes);
    case '<':
        return classify_less_operator(bytes);
    case '>':
        return classify_greater_operator(bytes);
    case '!':
        return classify_bang_operator(bytes);
    case '&':
        return classify_ampersand_operator(bytes);
    case '|':
        return classify_pipe_operator(bytes);
    case ':':
        return classify_colon_operator(bytes);
    case '=':
        return make_operator_match(MYLITE_SQL_OPERATOR_EQUAL, 1U);
    case '+':
        return make_operator_match(MYLITE_SQL_OPERATOR_PLUS, 1U);
    case '*':
        return make_operator_match(MYLITE_SQL_OPERATOR_STAR, 1U);
    case '/':
        return make_operator_match(MYLITE_SQL_OPERATOR_SLASH, 1U);
    case '%':
        return make_operator_match(MYLITE_SQL_OPERATOR_PERCENT, 1U);
    case '~':
        return make_operator_match(MYLITE_SQL_OPERATOR_BITWISE_NOT, 1U);
    case '^':
        return make_operator_match(MYLITE_SQL_OPERATOR_BITWISE_XOR, 1U);
    default:
        return make_operator_match(MYLITE_SQL_OPERATOR_NONE, 0U);
    }
}

static struct mylite_sql_operator_match classify_minus_operator(
    struct mylite_sql_operator_bytes bytes
) {
    if (bytes.second != '>') {
        return make_operator_match(MYLITE_SQL_OPERATOR_MINUS, 1U);
    }
    if (bytes.third == '>') {
        return make_operator_match(MYLITE_SQL_OPERATOR_JSON_UNQUOTE_EXTRACT, 3U);
    }
    return make_operator_match(MYLITE_SQL_OPERATOR_JSON_EXTRACT, 2U);
}

static struct mylite_sql_operator_match classify_less_operator(
    struct mylite_sql_operator_bytes bytes
) {
    if (bytes.second == '=' && bytes.third == '>') {
        return make_operator_match(MYLITE_SQL_OPERATOR_NULL_SAFE_EQUAL, 3U);
    }
    if (bytes.second == '<') {
        return make_operator_match(MYLITE_SQL_OPERATOR_LEFT_SHIFT, 2U);
    }
    if (bytes.second == '=') {
        return make_operator_match(MYLITE_SQL_OPERATOR_LESS_EQUAL, 2U);
    }
    if (bytes.second == '>') {
        return make_operator_match(MYLITE_SQL_OPERATOR_NOT_EQUAL, 2U);
    }
    return make_operator_match(MYLITE_SQL_OPERATOR_LESS, 1U);
}

static struct mylite_sql_operator_match classify_greater_operator(
    struct mylite_sql_operator_bytes bytes
) {
    if (bytes.second == '>') {
        return make_operator_match(MYLITE_SQL_OPERATOR_RIGHT_SHIFT, 2U);
    }
    if (bytes.second == '=') {
        return make_operator_match(MYLITE_SQL_OPERATOR_GREATER_EQUAL, 2U);
    }
    return make_operator_match(MYLITE_SQL_OPERATOR_GREATER, 1U);
}

static struct mylite_sql_operator_match classify_bang_operator(
    struct mylite_sql_operator_bytes bytes
) {
    if (bytes.second == '=') {
        return make_operator_match(MYLITE_SQL_OPERATOR_NOT_EQUAL, 2U);
    }
    return make_operator_match(MYLITE_SQL_OPERATOR_NOT, 1U);
}

static struct mylite_sql_operator_match classify_ampersand_operator(
    struct mylite_sql_operator_bytes bytes
) {
    if (bytes.second == '&') {
        return make_operator_match(MYLITE_SQL_OPERATOR_LOGICAL_AND, 2U);
    }
    return make_operator_match(MYLITE_SQL_OPERATOR_BITWISE_AND, 1U);
}

static struct mylite_sql_operator_match classify_pipe_operator(
    struct mylite_sql_operator_bytes bytes
) {
    if (bytes.second == '|') {
        return make_operator_match(MYLITE_SQL_OPERATOR_LOGICAL_OR, 2U);
    }
    return make_operator_match(MYLITE_SQL_OPERATOR_BITWISE_OR, 1U);
}

static struct mylite_sql_operator_match classify_colon_operator(
    struct mylite_sql_operator_bytes bytes
) {
    if (bytes.second == '=') {
        return make_operator_match(MYLITE_SQL_OPERATOR_ASSIGN, 2U);
    }
    return make_operator_match(MYLITE_SQL_OPERATOR_NONE, 0U);
}

static struct mylite_sql_operator_match make_operator_match(
    enum mylite_sql_operator_kind kind,
    size_t length
) {
    return (struct mylite_sql_operator_match){
        .kind = kind,
        .length = length,
    };
}

static bool scan_quoted_string(
    struct mylite_sql_lexer *lexer,
    struct mylite_token_start start,
    struct mylite_quoted_string_options options,
    struct mylite_sql_token *out_token
) {
    assert(peek_at(lexer, 0U) == (unsigned char)options.quote);

    advance_one(lexer);
    while (lexer->offset < lexer->length) {
        unsigned char byte = peek_at(lexer, 0U);
        if (byte == (unsigned char)options.quote) {
            advance_one(lexer);
            if (peek_at(lexer, 0U) == (unsigned char)options.quote) {
                advance_one(lexer);
                continue;
            }
            set_token(lexer, out_token, options.kind, start);
            return true;
        }
        if (byte == '\\' && options.allow_backslash && has_at(lexer, 1U)) {
            advance_one(lexer);
            advance_one(lexer);
            continue;
        }
        advance_one(lexer);
    }

    set_error_token(lexer, out_token, MYLITE_SQL_LEXER_ERROR_UNTERMINATED_STRING, start);
    return true;
}

static bool scan_quoted_identifier(
    struct mylite_sql_lexer *lexer,
    struct mylite_token_start start,
    char quote,
    struct mylite_sql_token *out_token
) {
    assert(peek_at(lexer, 0U) == (unsigned char)quote);

    advance_one(lexer);
    while (lexer->offset < lexer->length) {
        unsigned char byte = peek_at(lexer, 0U);
        if (byte == (unsigned char)quote) {
            advance_one(lexer);
            if (peek_at(lexer, 0U) == (unsigned char)quote) {
                advance_one(lexer);
                continue;
            }
            set_token(lexer, out_token, MYLITE_SQL_TOKEN_QUOTED_IDENTIFIER, start);
            return true;
        }
        advance_one(lexer);
    }

    set_error_token(lexer, out_token, MYLITE_SQL_LEXER_ERROR_UNTERMINATED_IDENTIFIER, start);
    return true;
}

static bool scan_quoted_hex_or_bit_literal(
    struct mylite_sql_lexer *lexer,
    struct mylite_token_start start,
    struct mylite_binary_literal_options options,
    struct mylite_sql_token *out_token
) {
    size_t digits = 0U;
    bool valid = true;

    assert(peek_at(lexer, 0U) == '\'');

    advance_one(lexer);
    while (lexer->offset < lexer->length) {
        unsigned char byte = peek_at(lexer, 0U);
        if (byte == '\'') {
            advance_one(lexer);
            if (valid && (!options.hex || (digits % 2U) == 0U)) {
                set_token(lexer, out_token, options.kind, start);
            } else {
                set_error_token(lexer, out_token, binary_literal_error(options.hex), start);
            }
            return true;
        }
        if (!is_binary_literal_digit(byte, options.hex)) {
            valid = false;
        }
        ++digits;
        advance_one(lexer);
    }

    set_error_token(lexer, out_token, binary_literal_error(options.hex), start);
    return true;
}

static bool scan_prefixed_hex_or_bit_literal(
    struct mylite_sql_lexer *lexer,
    struct mylite_token_start start,
    struct mylite_binary_literal_options options,
    struct mylite_sql_token *out_token
) {
    size_t cursor = lexer->offset + 2U;

    while (cursor < lexer->length &&
           is_binary_literal_digit((unsigned char)lexer->input[cursor], options.hex)) {
        ++cursor;
    }

    if (cursor < lexer->length && is_identifier_part((unsigned char)lexer->input[cursor])) {
        return scan_digit_leading_identifier(lexer, start.flags, out_token);
    }

    advance_to_offset_no_newline(lexer, cursor);

    set_token(lexer, out_token, options.kind, start);
    return true;
}

static bool scan_digit_leading_identifier(
    struct mylite_sql_lexer *lexer,
    unsigned int flags,
    struct mylite_sql_token *out_token
) {
    struct mylite_token_start start = make_token_start(lexer, flags);

    advance_to_offset_no_newline(
        lexer,
        skip_identifier_parts(lexer->input, lexer->length, lexer->offset)
    );

    set_token(lexer, out_token, MYLITE_SQL_TOKEN_IDENTIFIER, start);
    return true;
}

static bool scan_unquoted_identifier(
    struct mylite_sql_lexer *lexer,
    unsigned int flags,
    struct mylite_sql_token *out_token
) {
    struct mylite_token_start start = make_token_start(lexer, flags);
    struct mylite_sql_keyword_lookup_result keyword = {
        .flags = 0U,
        .keyword_index = (unsigned int)-1,
    };
    enum mylite_sql_token_kind kind = MYLITE_SQL_TOKEN_IDENTIFIER;
    size_t end = 0U;

    advance_to_offset_no_newline(
        lexer,
        skip_identifier_parts(lexer->input, lexer->length, lexer->offset)
    );
    end = lexer->offset;

    if (identifier_span_is_charset_introducer(lexer, start.offset, end)) {
        kind = MYLITE_SQL_TOKEN_CHARSET_INTRODUCER;
    } else if (identifier_span_is_temporal_literal_introducer(lexer, start.offset, end)) {
        kind = MYLITE_SQL_TOKEN_TEMPORAL_LITERAL_INTRODUCER;
    } else if (mylite_sql_keyword_lookup(
                   &lexer->input[start.offset],
                   end - start.offset,
                   &keyword
               )) {
        kind = MYLITE_SQL_TOKEN_KEYWORD;
    }

    set_token(lexer, out_token, kind, start);
    out_token->keyword_flags = keyword.flags;
    out_token->keyword_index = keyword.keyword_index;
    return true;
}

static bool identifier_span_is_charset_introducer(
    const struct mylite_sql_lexer *lexer,
    size_t start,
    size_t end
) {
    size_t cursor = end;

    if (lexer == NULL || lexer->input == NULL || start >= end || lexer->input[start] != '_') {
        return false;
    }
    if (end - start < 2U) {
        return false;
    }
    while (cursor < lexer->length && is_space((unsigned char)lexer->input[cursor])) {
        ++cursor;
    }
    return starts_literal_after_charset_introducer(lexer, cursor);
}

static bool starts_literal_after_charset_introducer(
    const struct mylite_sql_lexer *lexer,
    size_t cursor
) {
    unsigned char first = 0U;
    unsigned char second = 0U;

    if (lexer == NULL || lexer->input == NULL || cursor >= lexer->length) {
        return false;
    }
    first = (unsigned char)lexer->input[cursor];
    second = cursor + 1U < lexer->length ? (unsigned char)lexer->input[cursor + 1U] : 0U;
    if (first == '\'' || first == '"') {
        return true;
    }
    if ((first == 'X' || first == 'x' || first == 'B' || first == 'b') && second == '\'') {
        return true;
    }
    if (first == '0' && (second == 'X' || second == 'x' || second == 'B' || second == 'b')) {
        return true;
    }
    return false;
}

static bool identifier_span_is_temporal_literal_introducer(
    const struct mylite_sql_lexer *lexer,
    size_t start,
    size_t end
) {
    enum { date_keyword_length = 4, time_keyword_length = 4, timestamp_keyword_length = 9 };

    size_t cursor = end;
    size_t length = 0U;
    unsigned char first = 0U;

    if (lexer == NULL || lexer->input == NULL || start >= end) {
        return false;
    }
    length = end - start;
    first = (unsigned char)ascii_upper((unsigned char)lexer->input[start]);
    if ((first == 'D' && length != date_keyword_length) ||
        (first == 'T' && length != time_keyword_length && length != timestamp_keyword_length) ||
        (first != 'D' && first != 'T')) {
        return false;
    }
    if (first == 'D' && !identifier_span_equals_keyword(lexer, start, end, "DATE")) {
        return false;
    }
    if (first == 'T' && length == time_keyword_length &&
        !identifier_span_equals_keyword(lexer, start, end, "TIME")) {
        return false;
    }
    if (first == 'T' && length == timestamp_keyword_length &&
        !identifier_span_equals_keyword(lexer, start, end, "TIMESTAMP")) {
        return false;
    }
    while (cursor < lexer->length && is_space((unsigned char)lexer->input[cursor])) {
        ++cursor;
    }
    return cursor < lexer->length &&
           (lexer->input[cursor] == '\'' ||
            (lexer->input[cursor] == '"' && (lexer->modes & MYLITE_SQL_MODE_ANSI_QUOTES) == 0U));
}

static bool identifier_span_equals_keyword(
    const struct mylite_sql_lexer *lexer,
    size_t start,
    size_t end,
    const char *keyword
) {
    size_t keyword_length = keyword == NULL ? 0U : strlen(keyword);

    if (lexer == NULL || lexer->input == NULL || end < start || end - start != keyword_length) {
        return false;
    }
    for (size_t index = 0U; index < keyword_length; ++index) {
        unsigned char actual = (unsigned char)lexer->input[start + index];
        unsigned char expected = (unsigned char)keyword[index];

        if ((unsigned char)ascii_upper(actual) != expected) {
            return false;
        }
    }
    return true;
}

static bool scan_quoted_variable(
    struct mylite_sql_lexer *lexer,
    struct mylite_token_start start,
    char quote,
    struct mylite_sql_token *out_token
) {
    bool allow_backslash = false;

    if (quote != '`' && (lexer->modes & MYLITE_SQL_MODE_NO_BACKSLASH_ESCAPES) == 0U) {
        allow_backslash = true;
    }

    assert(peek_at(lexer, 0U) == (unsigned char)quote);

    advance_one(lexer);
    while (lexer->offset < lexer->length) {
        unsigned char byte = peek_at(lexer, 0U);
        if (byte == (unsigned char)quote) {
            advance_one(lexer);
            if (peek_at(lexer, 0U) == (unsigned char)quote) {
                advance_one(lexer);
                continue;
            }
            set_token(lexer, out_token, MYLITE_SQL_TOKEN_USER_VARIABLE, start);
            return true;
        }
        if (byte == '\\' && allow_backslash && has_at(lexer, 1U)) {
            advance_one(lexer);
            advance_one(lexer);
            continue;
        }
        advance_one(lexer);
    }

    set_error_token(lexer, out_token, MYLITE_SQL_LEXER_ERROR_INVALID_VARIABLE, start);
    return true;
}

static struct mylite_token_start make_token_start(
    const struct mylite_sql_lexer *lexer,
    unsigned int flags
) {
    return (struct mylite_token_start){
        .offset = lexer->offset,
        .flags = flags,
    };
}

static void set_token(
    const struct mylite_sql_lexer *lexer,
    struct mylite_sql_token *out_token,
    enum mylite_sql_token_kind kind,
    struct mylite_token_start start
) {
    out_token->kind = kind;
    out_token->operator_kind = MYLITE_SQL_OPERATOR_NONE;
    out_token->error = MYLITE_SQL_LEXER_ERROR_NONE;
    out_token->flags = start.flags;
    out_token->keyword_flags = 0U;
    out_token->keyword_index = (unsigned int)-1;
    out_token->text = lexer->input == NULL ? NULL : &lexer->input[start.offset];
    out_token->length = lexer->offset - start.offset;
    out_token->offset = start.offset;
}

static void set_error_token(
    const struct mylite_sql_lexer *lexer,
    struct mylite_sql_token *out_token,
    enum mylite_sql_lexer_error error,
    struct mylite_token_start start
) {
    set_token(lexer, out_token, MYLITE_SQL_TOKEN_ERROR, start);
    out_token->error = error;
}

static bool consume_whitespace(struct mylite_sql_lexer *lexer) {
    size_t cursor = 0U;

    if (lexer == NULL || lexer->input == NULL || lexer->offset >= lexer->length) {
        return false;
    }

    cursor = lexer->offset;
    while (cursor < lexer->length && is_space((unsigned char)lexer->input[cursor])) {
        ++cursor;
    }

    if (cursor == lexer->offset) {
        return false;
    }
    lexer->offset = cursor;
    return true;
}

static void advance_one(struct mylite_sql_lexer *lexer) {
    if (lexer->offset >= lexer->length) {
        return;
    }
    ++lexer->offset;
}

static unsigned char peek_at(const struct mylite_sql_lexer *lexer, size_t lookahead) {
    size_t index = 0U;
    if (lexer->input == NULL || lexer->offset >= lexer->length ||
        lookahead >= lexer->length - lexer->offset) {
        return '\0';
    }
    index = lexer->offset + lookahead;
    return (unsigned char)lexer->input[index];
}

static bool has_at(const struct mylite_sql_lexer *lexer, size_t lookahead) {
    if (lexer->input != NULL && lexer->offset < lexer->length &&
        lookahead < lexer->length - lexer->offset) {
        return true;
    }
    return false;
}

static bool starts_comment(const struct mylite_sql_lexer *lexer) {
    unsigned char first = peek_at(lexer, 0U);
    unsigned char second = peek_at(lexer, 1U);
    unsigned char third = peek_at(lexer, 2U);

    if (first == '#') {
        return true;
    }
    if (first == '-' && second == '-' && has_at(lexer, 2U) && is_mysql_comment_space(third)) {
        return true;
    }
    if (first == '/' && second == '*') {
        return true;
    }
    return false;
}

static bool is_mysql_comment_space(unsigned char byte) {
    if (byte <= mysql_comment_space_max) {
        return true;
    }
    return false;
}

static bool is_space(unsigned char byte) {
    if (byte == ' ' || byte == '\t' || byte == '\n' || byte == '\r' || byte == '\f' ||
        byte == '\v') {
        return true;
    }
    return false;
}

static bool is_digit(unsigned char byte) {
    if (byte >= '0' && byte <= '9') {
        return true;
    }
    return false;
}

static bool is_hex_digit(unsigned char byte) {
    if (is_digit(byte) || (byte >= 'A' && byte <= 'F') || (byte >= 'a' && byte <= 'f')) {
        return true;
    }
    return false;
}

static bool is_bit_digit(unsigned char byte) {
    if (byte == '0' || byte == '1') {
        return true;
    }
    return false;
}

static bool is_identifier_start(unsigned char byte) {
    if ((byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') || byte == '_' ||
        byte == '$' || byte >= mysql_non_ascii_min) {
        return true;
    }
    return false;
}

static bool is_identifier_part(unsigned char byte) {
    if (is_identifier_start(byte) || is_digit(byte)) {
        return true;
    }
    return false;
}

static bool is_user_variable_part(unsigned char byte) {
    if (is_digit(byte) || (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') ||
        byte == '.' || byte == '_' || byte == '$') {
        return true;
    }
    return false;
}

static bool is_punctuation(unsigned char byte) {
    if (byte == '(' || byte == ')' || byte == ',' || byte == ';' || byte == '.' || byte == '{' ||
        byte == '}' || byte == '[' || byte == ']' || byte == ':') {
        return true;
    }
    return false;
}

static bool is_exponent_marker(unsigned char byte) {
    if (byte == 'e' || byte == 'E') {
        return true;
    }
    return false;
}

static bool is_sign(unsigned char byte) {
    if (byte == '+' || byte == '-') {
        return true;
    }
    return false;
}

static bool is_binary_literal_digit(unsigned char byte, bool hex) {
    if (hex) {
        return is_hex_digit(byte);
    }
    return is_bit_digit(byte);
}

static enum mylite_sql_lexer_error binary_literal_error(bool hex) {
    if (hex) {
        return MYLITE_SQL_LEXER_ERROR_INVALID_HEX_LITERAL;
    }
    return MYLITE_SQL_LEXER_ERROR_INVALID_BIT_LITERAL;
}

static size_t skip_digits(const char *input, size_t length, size_t cursor) {
    while (cursor < length && is_digit((unsigned char)input[cursor])) {
        ++cursor;
    }
    return cursor;
}

static size_t skip_identifier_parts(const char *input, size_t length, size_t cursor) {
    while (cursor < length && is_identifier_part((unsigned char)input[cursor])) {
        ++cursor;
    }
    return cursor;
}

static size_t skip_user_variable_parts(const char *input, size_t length, size_t cursor) {
    while (cursor < length && is_user_variable_part((unsigned char)input[cursor])) {
        ++cursor;
    }
    return cursor;
}

static bool scan_exponent_span(const char *input, size_t length, size_t *cursor) {
    size_t exponent = *cursor + 1U;

    if (*cursor >= length || !is_exponent_marker((unsigned char)input[*cursor])) {
        return false;
    }

    if (exponent < length && is_sign((unsigned char)input[exponent])) {
        ++exponent;
    }

    if (exponent >= length || !is_digit((unsigned char)input[exponent])) {
        return false;
    }

    *cursor = skip_digits(input, length, exponent + 1U);
    return true;
}

// NOLINTBEGIN(readability-function-cognitive-complexity, readability-magic-numbers)
/* Derived keyword-table indexes and masks. */
static struct mylite_sql_keyword_range keyword_search_range(unsigned char first) {
#define KEYWORD_RANGE(LOW, HIGH)                                                                   \
    (struct mylite_sql_keyword_range) {                                                            \
        .low = (LOW), .high = (HIGH), .found = true                                                \
    }

    switch (first) {
    case 'A':
        return KEYWORD_RANGE(0U, 38U);
    case 'B':
        return KEYWORD_RANGE(38U, 64U);
    case 'C':
        return KEYWORD_RANGE(64U, 144U);
    case 'D':
        return KEYWORD_RANGE(144U, 195U);
    case 'E':
        return KEYWORD_RANGE(195U, 232U);
    case 'F':
        return KEYWORD_RANGE(232U, 268U);
    case 'G':
        return KEYWORD_RANGE(268U, 288U);
    case 'H':
        return KEYWORD_RANGE(288U, 302U);
    case 'I':
        return KEYWORD_RANGE(302U, 347U);
    case 'J':
        return KEYWORD_RANGE(347U, 366U);
    case 'K':
        return KEYWORD_RANGE(366U, 371U);
    case 'L':
        return KEYWORD_RANGE(371U, 416U);
    case 'M':
        return KEYWORD_RANGE(416U, 459U);
    case 'N':
        return KEYWORD_RANGE(459U, 487U);
    case 'O':
        return KEYWORD_RANGE(487U, 515U);
    case 'P':
        return KEYWORD_RANGE(515U, 559U);
    case 'Q':
        return KEYWORD_RANGE(559U, 564U);
    case 'R':
        return KEYWORD_RANGE(564U, 647U);
    case 'S':
        return KEYWORD_RANGE(647U, 780U);
    case 'T':
        return KEYWORD_RANGE(780U, 819U);
    case 'U':
        return KEYWORD_RANGE(819U, 855U);
    case 'V':
        return KEYWORD_RANGE(855U, 868U);
    case 'W':
        return KEYWORD_RANGE(868U, 883U);
    case 'X':
        return KEYWORD_RANGE(883U, 888U);
    case 'Y':
        return KEYWORD_RANGE(888U, 891U);
    case 'Z':
        return KEYWORD_RANGE(891U, 893U);
    case '_':
        return KEYWORD_RANGE(893U, 894U);
    default:
        return (struct mylite_sql_keyword_range){0};
    }

#undef KEYWORD_RANGE
}

static bool keyword_length_is_possible(unsigned char first, size_t length) {
    if (length >= 64U) {
        return false;
    }

    switch (first) {
    case 'A':
        return (UINT64_C(0x000000400000c6fc) & (UINT64_C(1) << length)) != 0U;
    case 'B':
        return (UINT64_C(0x0000000000000efc) & (UINT64_C(1) << length)) != 0U;
    case 'C':
        return (UINT64_C(0x000000000007bff8) & (UINT64_C(1) << length)) != 0U;
    case 'D':
        return (UINT64_C(0x000000000000bfdc) & (UINT64_C(1) << length)) != 0U;
    case 'E':
        return (UINT64_C(0x0000000000010ff8) & (UINT64_C(1) << length)) != 0U;
    case 'F':
        return (UINT64_C(0x000000000020aff8) & (UINT64_C(1) << length)) != 0U;
    case 'G':
        return (UINT64_C(0x00000000002657e8) & (UINT64_C(1) << length)) != 0U;
    case 'H':
        return (UINT64_C(0x0000000000012af8) & (UINT64_C(1) << length)) != 0U;
    case 'I':
        return (UINT64_C(0x000000000002fffc) & (UINT64_C(1) << length)) != 0U;
    case 'J':
        return (UINT64_C(0x0000000000043f10) & (UINT64_C(1) << length)) != 0U;
    case 'K':
        return (UINT64_C(0x0000000000004098) & (UINT64_C(1) << length)) != 0U;
    case 'L':
        return (UINT64_C(0x00000000000057fc) & (UINT64_C(1) << length)) != 0U;
    case 'M':
        return (UINT64_C(0x000000000114bff8) & (UINT64_C(1) << length)) != 0U;
    case 'N':
        return (UINT64_C(0x00000000000607fc) & (UINT64_C(1) << length)) != 0U;
    case 'O':
        return (UINT64_C(0x00000000000095fc) & (UINT64_C(1) << length)) != 0U;
    case 'P':
        return (UINT64_C(0x0000000000241ffc) & (UINT64_C(1) << length)) != 0U;
    case 'Q':
        return (UINT64_C(0x00000000000000a0) & (UINT64_C(1) << length)) != 0U;
    case 'R':
        return (UINT64_C(0x0000000088dffff8) & (UINT64_C(1) << length)) != 0U;
    case 'S':
        return (UINT64_C(0x00000000a4dffffc) & (UINT64_C(1) << length)) != 0U;
    case 'T':
        return (UINT64_C(0x000000000000fffc) & (UINT64_C(1) << length)) != 0U;
    case 'U':
        return (UINT64_C(0x0000000000096ff8) & (UINT64_C(1) << length)) != 0U;
    case 'V':
        return (UINT64_C(0x00000000000016f0) & (UINT64_C(1) << length)) != 0U;
    case 'W':
        return (UINT64_C(0x00000000000025f0) & (UINT64_C(1) << length)) != 0U;
    case 'X':
        return (UINT64_C(0x000000000000001c) & (UINT64_C(1) << length)) != 0U;
    case 'Y':
        return (UINT64_C(0x0000000000000510) & (UINT64_C(1) << length)) != 0U;
    case 'Z':
        return (UINT64_C(0x0000000000000110) & (UINT64_C(1) << length)) != 0U;
    case '_':
        return (UINT64_C(0x0000000000000200) & (UINT64_C(1) << length)) != 0U;
    default:
        return false;
    }
}

static unsigned int lookup_hot_keyword_index(const char *text, size_t length, unsigned char first) {
    /* Returned indices are positions in mylite_sql_keyword_lookup()'s keyword table. */
    switch (length) {
    case 2U:
        switch (first) {
        case 'A':
            if (compare_keyword_text(text, length, "AS") == 0) {
                return 22U;
            }
            break;
        case 'B':
            if (compare_keyword_text(text, length, "BY") == 0) {
                return 62U;
            }
            break;
        case 'I':
            if (compare_keyword_text(text, length, "IN") == 0) {
                return 308U;
            }
            if (compare_keyword_text(text, length, "IF") == 0) {
                return 303U;
            }
            if (compare_keyword_text(text, length, "IS") == 0) {
                return 341U;
            }
            break;
        case 'O':
            if (compare_keyword_text(text, length, "ON") == 0) {
                return 494U;
            }
            if (compare_keyword_text(text, length, "OR") == 0) {
                return 504U;
            }
            break;
        case 'T':
            if (compare_keyword_text(text, length, "TO") == 0) {
                return 806U;
            }
            break;
        default:
            break;
        }
        break;
    case 3U:
        switch (first) {
        case 'A':
            if (compare_keyword_text(text, length, "AND") == 0) {
                return 18U;
            }
            if (compare_keyword_text(text, length, "ADD") == 0) {
                return 6U;
            }
            if (compare_keyword_text(text, length, "ALL") == 0) {
                return 14U;
            }
            break;
        case 'E':
            if (compare_keyword_text(text, length, "END") == 0) {
                return 203U;
            }
            break;
        case 'F':
            if (compare_keyword_text(text, length, "FOR") == 0) {
                return 255U;
            }
            break;
        case 'H':
            if (compare_keyword_text(text, length, "HEX") == 0) {
                return 292U;
            }
            break;
        case 'I':
            if (compare_keyword_text(text, length, "INT") == 0) {
                return 324U;
            }
            break;
        case 'K':
            if (compare_keyword_text(text, length, "KEY") == 0) {
                return 366U;
            }
            break;
        case 'M':
            if (compare_keyword_text(text, length, "MAX") == 0) {
                return 422U;
            }
            if (compare_keyword_text(text, length, "MIN") == 0) {
                return 443U;
            }
            break;
        case 'N':
            if (compare_keyword_text(text, length, "NOT") == 0) {
                return 474U;
            }
            break;
        case 'R':
            if (compare_keyword_text(text, length, "ROW") == 0) {
                return 639U;
            }
            break;
        case 'S':
            if (compare_keyword_text(text, length, "SET") == 0) {
                return 670U;
            }
            if (compare_keyword_text(text, length, "SUM") == 0) {
                return 772U;
            }
            break;
        default:
            break;
        }
        break;
    case 4U:
        switch (first) {
        case 'C':
            if (compare_keyword_text(text, length, "CHAR") == 0) {
                return 78U;
            }
            if (compare_keyword_text(text, length, "CAST") == 0) {
                return 69U;
            }
            if (compare_keyword_text(text, length, "CALL") == 0) {
                return 65U;
            }
            break;
        case 'D':
            if (compare_keyword_text(text, length, "DROP") == 0) {
                return 190U;
            }
            if (compare_keyword_text(text, length, "DATE") == 0) {
                return 148U;
            }
            if (compare_keyword_text(text, length, "DESC") == 0) {
                return 176U;
            }
            if (compare_keyword_text(text, length, "DATA") == 0) {
                return 144U;
            }
            break;
        case 'F':
            if (compare_keyword_text(text, length, "FROM") == 0) {
                return 261U;
            }
            break;
        case 'I':
            if (compare_keyword_text(text, length, "INTO") == 0) {
                return 333U;
            }
            break;
        case 'J':
            if (compare_keyword_text(text, length, "JOIN") == 0) {
                return 347U;
            }
            if (compare_keyword_text(text, length, "JSON") == 0) {
                return 348U;
            }
            break;
        case 'L':
            if (compare_keyword_text(text, length, "LIKE") == 0) {
                return 388U;
            }
            if (compare_keyword_text(text, length, "LEFT") == 0) {
                return 384U;
            }
            if (compare_keyword_text(text, length, "LESS") == 0) {
                return 386U;
            }
            break;
        case 'N':
            if (compare_keyword_text(text, length, "NULL") == 0) {
                return 481U;
            }
            if (compare_keyword_text(text, length, "NAME") == 0) {
                return 459U;
            }
            break;
        case 'O':
            if (compare_keyword_text(text, length, "OVER") == 0) {
                return 513U;
            }
            break;
        case 'S':
            if (compare_keyword_text(text, length, "SHOW") == 0) {
                return 675U;
            }
            break;
        case 'T':
            if (compare_keyword_text(text, length, "TIME") == 0) {
                return 795U;
            }
            if (compare_keyword_text(text, length, "THAN") == 0) {
                return 791U;
            }
            break;
        case 'U':
            if (compare_keyword_text(text, length, "USER") == 0) {
                return 845U;
            }
            break;
        case 'V':
            if (compare_keyword_text(text, length, "VIEW") == 0) {
                return 865U;
            }
            break;
        case 'W':
            if (compare_keyword_text(text, length, "WITH") == 0) {
                return 878U;
            }
            break;
        default:
            break;
        }
        break;
    case 5U:
        switch (first) {
        case 'A':
            if (compare_keyword_text(text, length, "ALTER") == 0) {
                return 15U;
            }
            break;
        case 'B':
            if (compare_keyword_text(text, length, "BEGIN") == 0) {
                return 40U;
            }
            break;
        case 'C':
            if (compare_keyword_text(text, length, "COUNT") == 0) {
                return 127U;
            }
            break;
        case 'G':
            if (compare_keyword_text(text, length, "GROUP") == 0) {
                return 281U;
            }
            if (compare_keyword_text(text, length, "GRANT") == 0) {
                return 278U;
            }
            break;
        case 'I':
            if (compare_keyword_text(text, length, "INDEX") == 0) {
                return 310U;
            }
            break;
        case 'L':
            if (compare_keyword_text(text, length, "LIMIT") == 0) {
                return 389U;
            }
            break;
        case 'O':
            if (compare_keyword_text(text, length, "ORDER") == 0) {
                return 506U;
            }
            break;
        case 'P':
            if (compare_keyword_text(text, length, "POINT") == 0) {
                return 537U;
            }
            break;
        case 'T':
            if (compare_keyword_text(text, length, "TABLE") == 0) {
                return 780U;
            }
            break;
        case 'U':
            if (compare_keyword_text(text, length, "USING") == 0) {
                return 848U;
            }
            if (compare_keyword_text(text, length, "UNION") == 0) {
                return 831U;
            }
            break;
        case 'W':
            if (compare_keyword_text(text, length, "WHERE") == 0) {
                return 875U;
            }
            break;
        default:
            break;
        }
        break;
    case 6U:
        switch (first) {
        case 'C':
            if (compare_keyword_text(text, length, "CREATE") == 0) {
                return 130U;
            }
            if (compare_keyword_text(text, length, "CONCAT") == 0) {
                return 108U;
            }
            if (compare_keyword_text(text, length, "COLUMN") == 0) {
                return 95U;
            }
            break;
        case 'D':
            if (compare_keyword_text(text, length, "DELETE") == 0) {
                return 174U;
            }
            break;
        case 'E':
            if (compare_keyword_text(text, length, "ENGINE") == 0) {
                return 206U;
            }
            if (compare_keyword_text(text, length, "EXISTS") == 0) {
                return 221U;
            }
            break;
        case 'F':
            if (compare_keyword_text(text, length, "FORMAT") == 0) {
                return 258U;
            }
            break;
        case 'G':
            if (compare_keyword_text(text, length, "GLOBAL") == 0) {
                return 277U;
            }
            break;
        case 'I':
            if (compare_keyword_text(text, length, "INSERT") == 0) {
                return 319U;
            }
            break;
        case 'M':
            if (compare_keyword_text(text, length, "MEMBER") == 0) {
                return 435U;
            }
            break;
        case 'R':
            if (compare_keyword_text(text, length, "RETURN") == 0) {
                return 624U;
            }
            break;
        case 'S':
            if (compare_keyword_text(text, length, "SELECT") == 0) {
                return 662U;
            }
            break;
        case 'U':
            if (compare_keyword_text(text, length, "UPDATE") == 0) {
                return 839U;
            }
            break;
        case 'V':
            if (compare_keyword_text(text, length, "VALUES") == 0) {
                return 857U;
            }
            break;
        case 'W':
            if (compare_keyword_text(text, length, "WINDOW") == 0) {
                return 877U;
            }
            break;
        default:
            break;
        }
        break;
    case 7U:
        switch (first) {
        case 'B':
            if (compare_keyword_text(text, length, "BETWEEN") == 0) {
                return 42U;
            }
            break;
        case 'C':
            if (compare_keyword_text(text, length, "COLLATE") == 0) {
                return 93U;
            }
            if (compare_keyword_text(text, length, "CHARSET") == 0) {
                return 81U;
            }
            if (compare_keyword_text(text, length, "CONVERT") == 0) {
                return 123U;
            }
            if (compare_keyword_text(text, length, "COMMENT") == 0) {
                return 99U;
            }
            break;
        case 'D':
            if (compare_keyword_text(text, length, "DEFAULT") == 0) {
                return 167U;
            }
            if (compare_keyword_text(text, length, "DECLARE") == 0) {
                return 166U;
            }
            break;
        case 'E':
            if (compare_keyword_text(text, length, "EXPLAIN") == 0) {
                return 226U;
            }
            if (compare_keyword_text(text, length, "EXECUTE") == 0) {
                return 220U;
            }
            break;
        case 'I':
            if (compare_keyword_text(text, length, "INTEGER") == 0) {
                return 330U;
            }
            break;
        case 'P':
            if (compare_keyword_text(text, length, "PRIMARY") == 0) {
                return 549U;
            }
            if (compare_keyword_text(text, length, "PREPARE") == 0) {
                return 546U;
            }
            break;
        case 'S':
            if (compare_keyword_text(text, length, "SESSION") == 0) {
                return 668U;
            }
            break;
        case 'T':
            if (compare_keyword_text(text, length, "TINYINT") == 0) {
                return 803U;
            }
            break;
        case 'V':
            if (compare_keyword_text(text, length, "VARCHAR") == 0) {
                return 859U;
            }
            break;
        default:
            break;
        }
        break;
    case 8U:
        switch (first) {
        case 'D':
            if (compare_keyword_text(text, length, "DATETIME") == 0) {
                return 150U;
            }
            if (compare_keyword_text(text, length, "DISTINCT") == 0) {
                return 185U;
            }
            if (compare_keyword_text(text, length, "DATABASE") == 0) {
                return 145U;
            }
            break;
        case 'F':
            if (compare_keyword_text(text, length, "FUNCTION") == 0) {
                return 267U;
            }
            break;
        case 'U':
            if (compare_keyword_text(text, length, "UNSIGNED") == 0) {
                return 837U;
            }
            break;
        default:
            break;
        }
        break;
    case 9U:
        switch (first) {
        case 'C':
            if (compare_keyword_text(text, length, "CHARACTER") == 0) {
                return 79U;
            }
            break;
        case 'P':
            if (compare_keyword_text(text, length, "PARTITION") == 0) {
                return 521U;
            }
            if (compare_keyword_text(text, length, "PROCEDURE") == 0) {
                return 552U;
            }
            if (compare_keyword_text(text, length, "PRECEDING") == 0) {
                return 544U;
            }
            break;
        case 'T':
            if (compare_keyword_text(text, length, "TIMESTAMP") == 0) {
                return 797U;
            }
            break;
        default:
            break;
        }
        break;
    case 10U:
        switch (first) {
        case 'J':
            if (compare_keyword_text(text, length, "JSON_DEPTH") == 0) {
                return 894U;
            }
            break;
        case 'T':
            if (compare_keyword_text(text, length, "TABLE_NAME") == 0) {
                return 785U;
            }
            break;
        default:
            break;
        }
        break;
    case 11U:
        switch (first) {
        case 'J':
            if (compare_keyword_text(text, length, "JSON_PRETTY") == 0) {
                return 895U;
            }
            break;
        case 'R':
            if (compare_keyword_text(text, length, "REGEXP_LIKE") == 0) {
                return 584U;
            }
            break;
        default:
            break;
        }
        break;
    case 13U:
        switch (first) {
        case 'J':
            if (compare_keyword_text(text, length, "JSON_OVERLAPS") == 0) {
                return 896U;
            }
            break;
        default:
            break;
        }
        break;
    case 14U:
        switch (first) {
        case 'A':
            if (compare_keyword_text(text, length, "AUTO_INCREMENT") == 0) {
                return 35U;
            }
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }
    return (unsigned int)-1;
}

static bool keyword_second_char_is_possible(struct mylite_sql_keyword_second_char_probe probe) {
    enum {
        keyword_first_count = 27,
        keyword_second_count = 37,
        keyword_length_count = 39,
        keyword_invalid_index = 64
    };

    /* Bit masks are derived from the same keyword table and reject impossible second bytes. */
    static const uint64_t second_masks[keyword_first_count][keyword_length_count] = {
        [0U][2U] = UINT64_C(0x00000c0000),   [0U][3U] = UINT64_C(0x000024280a),
        [0U][4U] = UINT64_C(0x00001c0004),   [0U][5U] = UINT64_C(0x00000e0828),
        [0U][6U] = UINT64_C(0x0000000804),   [0U][7U] = UINT64_C(0x000000204c),
        [0U][9U] = UINT64_C(0x0000082840),   [0U][10U] = UINT64_C(0x0000040004),
        [0U][14U] = UINT64_C(0x0000300000),  [0U][15U] = UINT64_C(0x0000100000),
        [0U][38U] = UINT64_C(0x0000040000),  [1U][2U] = UINT64_C(0x0001000000),
        [1U][3U] = UINT64_C(0x0000000100),   [1U][4U] = UINT64_C(0x0001104800),
        [1U][5U] = UINT64_C(0x0000080810),   [1U][6U] = UINT64_C(0x0000000111),
        [1U][7U] = UINT64_C(0x0000104110),   [1U][9U] = UINT64_C(0x0000000110),
        [1U][10U] = UINT64_C(0x0000000100),  [1U][11U] = UINT64_C(0x0000000100),
        [2U][3U] = UINT64_C(0x000000c000),   [2U][4U] = UINT64_C(0x0000104091),
        [2U][5U] = UINT64_C(0x0000024881),   [2U][6U] = UINT64_C(0x0000124980),
        [2U][7U] = UINT64_C(0x0000104091),   [2U][8U] = UINT64_C(0x0000004081),
        [2U][9U] = UINT64_C(0x0000104080),   [2U][10U] = UINT64_C(0x0000004000),
        [2U][11U] = UINT64_C(0x0000104080),  [2U][12U] = UINT64_C(0x0000104801),
        [2U][13U] = UINT64_C(0x0000004000),  [2U][15U] = UINT64_C(0x0000004000),
        [2U][16U] = UINT64_C(0x0000000080),  [2U][17U] = UINT64_C(0x0000104000),
        [2U][18U] = UINT64_C(0x0000004080),  [3U][2U] = UINT64_C(0x0000004000),
        [3U][3U] = UINT64_C(0x0000000111),   [3U][4U] = UINT64_C(0x0000120111),
        [3U][6U] = UINT64_C(0x0000004010),   [3U][7U] = UINT64_C(0x0001000111),
        [3U][8U] = UINT64_C(0x0000100111),   [3U][9U] = UINT64_C(0x0000100101),
        [3U][10U] = UINT64_C(0x0000000011),  [3U][11U] = UINT64_C(0x0000000111),
        [3U][12U] = UINT64_C(0x0000000010),  [3U][13U] = UINT64_C(0x0000000010),
        [3U][15U] = UINT64_C(0x0000000011),  [4U][3U] = UINT64_C(0x0000802800),
        [4U][4U] = UINT64_C(0x0000802801),   [4U][5U] = UINT64_C(0x0000221000),
        [4U][6U] = UINT64_C(0x0000a62800),   [4U][7U] = UINT64_C(0x0000842000),
        [4U][8U] = UINT64_C(0x0000802000),   [4U][9U] = UINT64_C(0x0000800000),
        [4U][10U] = UINT64_C(0x0000802000),  [4U][11U] = UINT64_C(0x0000800000),
        [4U][16U] = UINT64_C(0x0000002000),  [5U][3U] = UINT64_C(0x0000004000),
        [5U][4U] = UINT64_C(0x0000120101),   [5U][5U] = UINT64_C(0x0000004911),
        [5U][6U] = UINT64_C(0x0000004901),   [5U][7U] = UINT64_C(0x0000004000),
        [5U][8U] = UINT64_C(0x0000100000),   [5U][9U] = UINT64_C(0x0000024000),
        [5U][10U] = UINT64_C(0x0000004000),  [5U][11U] = UINT64_C(0x0000020100),
        [5U][13U] = UINT64_C(0x0000020000),  [5U][15U] = UINT64_C(0x0000000100),
        [5U][21U] = UINT64_C(0x0000000001),  [6U][3U] = UINT64_C(0x0000000010),
        [6U][5U] = UINT64_C(0x00000a0000),   [6U][6U] = UINT64_C(0x0000020800),
        [6U][7U] = UINT64_C(0x0000000010),   [6U][8U] = UINT64_C(0x0000020010),
        [6U][9U] = UINT64_C(0x0000080010),   [6U][10U] = UINT64_C(0x0000000010),
        [6U][12U] = UINT64_C(0x0000020000),  [6U][14U] = UINT64_C(0x0000000010),
        [6U][17U] = UINT64_C(0x0000020000),  [6U][18U] = UINT64_C(0x0000000010),
        [6U][21U] = UINT64_C(0x0000000010),  [7U][3U] = UINT64_C(0x0000000010),
        [7U][4U] = UINT64_C(0x0000004011),   [7U][5U] = UINT64_C(0x0000004000),
        [7U][6U] = UINT64_C(0x0000000001),   [7U][7U] = UINT64_C(0x0000000101),
        [7U][9U] = UINT64_C(0x0000000100),   [7U][11U] = UINT64_C(0x0000004000),
        [7U][13U] = UINT64_C(0x0000000100),  [7U][16U] = UINT64_C(0x0000004000),
        [8U][2U] = UINT64_C(0x0000046020),   [8U][3U] = UINT64_C(0x000000a000),
        [8U][4U] = UINT64_C(0x0000002000),   [8U][5U] = UINT64_C(0x0000002000),
        [8U][6U] = UINT64_C(0x0000043060),   [8U][7U] = UINT64_C(0x00000c2000),
        [8U][8U] = UINT64_C(0x0000002000),   [8U][9U] = UINT64_C(0x0000046000),
        [8U][10U] = UINT64_C(0x0000000008),  [8U][11U] = UINT64_C(0x0000002000),
        [8U][12U] = UINT64_C(0x0000002000),  [8U][13U] = UINT64_C(0x0000002000),
        [8U][14U] = UINT64_C(0x0000004000),  [8U][15U] = UINT64_C(0x0000004000),
        [8U][17U] = UINT64_C(0x0000000040),  [9U][4U] = UINT64_C(0x0000044000),
        [9U][8U] = UINT64_C(0x0000040000),   [9U][9U] = UINT64_C(0x0000040000),
        [9U][10U] = UINT64_C(0x0000040000),  [9U][11U] = UINT64_C(0x0000040000),
        [9U][12U] = UINT64_C(0x0000040000),  [9U][13U] = UINT64_C(0x0000040000),
        [9U][18U] = UINT64_C(0x0000040000),  [10U][3U] = UINT64_C(0x0000000010),
        [10U][4U] = UINT64_C(0x0000000110),  [10U][7U] = UINT64_C(0x0000000010),
        [10U][14U] = UINT64_C(0x0000000010), [11U][2U] = UINT64_C(0x0000002000),
        [11U][3U] = UINT64_C(0x0000004001),  [11U][4U] = UINT64_C(0x000000c111),
        [11U][5U] = UINT64_C(0x0000084114),  [11U][6U] = UINT64_C(0x0000004110),
        [11U][7U] = UINT64_C(0x0000004011),  [11U][8U] = UINT64_C(0x0000004001),
        [11U][9U] = UINT64_C(0x0000004000),  [11U][10U] = UINT64_C(0x0000000101),
        [11U][12U] = UINT64_C(0x0000004000), [11U][14U] = UINT64_C(0x0000004001),
        [12U][3U] = UINT64_C(0x0000004109),  [12U][4U] = UINT64_C(0x0000004000),
        [12U][5U] = UINT64_C(0x0000104011),  [12U][6U] = UINT64_C(0x0000004111),
        [12U][7U] = UINT64_C(0x0000000100),  [12U][8U] = UINT64_C(0x0000004101),
        [12U][9U] = UINT64_C(0x0000004110),  [12U][10U] = UINT64_C(0x0000100010),
        [12U][11U] = UINT64_C(0x0001000100), [12U][12U] = UINT64_C(0x0000100010),
        [12U][13U] = UINT64_C(0x0000000100), [12U][15U] = UINT64_C(0x0000100000),
        [12U][18U] = UINT64_C(0x0000000100), [12U][20U] = UINT64_C(0x0000000001),
        [12U][24U] = UINT64_C(0x0000000001), [13U][2U] = UINT64_C(0x0000004000),
        [13U][3U] = UINT64_C(0x0000004018),  [13U][4U] = UINT64_C(0x0000104011),
        [13U][5U] = UINT64_C(0x0000180015),  [13U][6U] = UINT64_C(0x0000104010),
        [13U][7U] = UINT64_C(0x0000104001),  [13U][8U] = UINT64_C(0x0000200001),
        [13U][9U] = UINT64_C(0x0000084000),  [13U][10U] = UINT64_C(0x0000000008),
        [13U][17U] = UINT64_C(0x0000000010), [13U][18U] = UINT64_C(0x0000004000),
        [14U][2U] = UINT64_C(0x0000022220),  [14U][3U] = UINT64_C(0x0000122824),
        [14U][4U] = UINT64_C(0x000020a000),  [14U][5U] = UINT64_C(0x0000520000),
        [14U][6U] = UINT64_C(0x0000088020),  [14U][7U] = UINT64_C(0x0000108000),
        [14U][8U] = UINT64_C(0x0000008000),  [14U][10U] = UINT64_C(0x0000028000),
        [14U][12U] = UINT64_C(0x0000020004), [14U][15U] = UINT64_C(0x0000008000),
        [15U][2U] = UINT64_C(0x0000000100),  [15U][3U] = UINT64_C(0x0000004000),
        [15U][4U] = UINT64_C(0x0000024001),  [15U][5U] = UINT64_C(0x0000124080),
        [15U][6U] = UINT64_C(0x0000000801),  [15U][7U] = UINT64_C(0x0000024811),
        [15U][8U] = UINT64_C(0x0000024001),  [15U][9U] = UINT64_C(0x0000020001),
        [15U][10U] = UINT64_C(0x0000020811), [15U][11U] = UINT64_C(0x0000020010),
        [15U][12U] = UINT64_C(0x0000000011), [15U][18U] = UINT64_C(0x0000000001),
        [15U][21U] = UINT64_C(0x0000020000), [16U][5U] = UINT64_C(0x0000100000),
        [16U][7U] = UINT64_C(0x0000100000),  [17U][3U] = UINT64_C(0x0000004000),
        [17U][4U] = UINT64_C(0x000000c011),  [17U][5U] = UINT64_C(0x0000084911),
        [17U][6U] = UINT64_C(0x0000004011),  [17U][7U] = UINT64_C(0x0000004011),
        [17U][8U] = UINT64_C(0x0000004010),  [17U][9U] = UINT64_C(0x0000004010),
        [17U][10U] = UINT64_C(0x0000004010), [17U][11U] = UINT64_C(0x0000000010),
        [17U][12U] = UINT64_C(0x0000000011), [17U][13U] = UINT64_C(0x0000000010),
        [17U][14U] = UINT64_C(0x0000000010), [17U][15U] = UINT64_C(0x0000000010),
        [17U][16U] = UINT64_C(0x0000000010), [17U][17U] = UINT64_C(0x0000000010),
        [17U][18U] = UINT64_C(0x0000000010), [17U][19U] = UINT64_C(0x0000000010),
        [17U][20U] = UINT64_C(0x0000000010), [17U][22U] = UINT64_C(0x0000000010),
        [17U][23U] = UINT64_C(0x0000000010), [17U][27U] = UINT64_C(0x0000000010),
        [17U][31U] = UINT64_C(0x0000000010), [18U][2U] = UINT64_C(0x0040000000),
        [18U][3U] = UINT64_C(0x0000150190),  [18U][4U] = UINT64_C(0x00000b4d80),
        [18U][5U] = UINT64_C(0x0000588880),  [18U][6U] = UINT64_C(0x0001184114),
        [18U][7U] = UINT64_C(0x000118c014),  [18U][8U] = UINT64_C(0x000049b094),
        [18U][9U] = UINT64_C(0x0000100011),  [18U][10U] = UINT64_C(0x0000014000),
        [18U][11U] = UINT64_C(0x0001094014), [18U][12U] = UINT64_C(0x0000114010),
        [18U][13U] = UINT64_C(0x0000194000), [18U][14U] = UINT64_C(0x0000014010),
        [18U][15U] = UINT64_C(0x0000114000), [18U][16U] = UINT64_C(0x0000090010),
        [18U][17U] = UINT64_C(0x0000094000), [18U][18U] = UINT64_C(0x0000094010),
        [18U][19U] = UINT64_C(0x0000010000), [18U][20U] = UINT64_C(0x0000004000),
        [18U][22U] = UINT64_C(0x0000004000), [18U][23U] = UINT64_C(0x0000004000),
        [18U][26U] = UINT64_C(0x0000000010), [18U][29U] = UINT64_C(0x0000004000),
        [18U][31U] = UINT64_C(0x0000004000), [19U][2U] = UINT64_C(0x0000004000),
        [19U][3U] = UINT64_C(0x0000000801),  [19U][4U] = UINT64_C(0x0001020190),
        [19U][5U] = UINT64_C(0x0001000001),  [19U][6U] = UINT64_C(0x0000000001),
        [19U][7U] = UINT64_C(0x0000024100),  [19U][8U] = UINT64_C(0x0000020100),
        [19U][9U] = UINT64_C(0x0000004110),  [19U][10U] = UINT64_C(0x0000004011),
        [19U][11U] = UINT64_C(0x0000020101), [19U][12U] = UINT64_C(0x0000000100),
        [19U][13U] = UINT64_C(0x0000000100), [19U][14U] = UINT64_C(0x0000000001),
        [19U][15U] = UINT64_C(0x0000000080), [20U][3U] = UINT64_C(0x00000e0000),
        [20U][4U] = UINT64_C(0x0000142000),  [20U][5U] = UINT64_C(0x000004a004),
        [20U][6U] = UINT64_C(0x000000a000),  [20U][7U] = UINT64_C(0x000004a000),
        [20U][8U] = UINT64_C(0x0000082000),  [20U][9U] = UINT64_C(0x0000002000),
        [20U][10U] = UINT64_C(0x0000002000), [20U][11U] = UINT64_C(0x0000102000),
        [20U][13U] = UINT64_C(0x0000080000), [20U][14U] = UINT64_C(0x0000042000),
        [20U][16U] = UINT64_C(0x0000002000), [20U][19U] = UINT64_C(0x0000002000),
        [21U][4U] = UINT64_C(0x0000000104),  [21U][5U] = UINT64_C(0x0000000001),
        [21U][6U] = UINT64_C(0x0000000001),  [21U][7U] = UINT64_C(0x0000000111),
        [21U][9U] = UINT64_C(0x0000000001),  [21U][10U] = UINT64_C(0x0000000001),
        [21U][12U] = UINT64_C(0x0000000001), [22U][4U] = UINT64_C(0x0000004191),
        [22U][5U] = UINT64_C(0x0000020080),  [22U][6U] = UINT64_C(0x0000000100),
        [22U][7U] = UINT64_C(0x0000020110),  [22U][8U] = UINT64_C(0x0000000001),
        [22U][10U] = UINT64_C(0x0000000010), [22U][13U] = UINT64_C(0x0000000010),
        [23U][2U] = UINT64_C(0x0000000001),  [23U][3U] = UINT64_C(0x0000005100),
        [23U][4U] = UINT64_C(0x0100000000),  [24U][4U] = UINT64_C(0x0000000010),
        [24U][8U] = UINT64_C(0x0000000010),  [24U][10U] = UINT64_C(0x0000000010),
        [25U][4U] = UINT64_C(0x0000004000),  [25U][8U] = UINT64_C(0x0000000010),
        [26U][9U] = UINT64_C(0x0000000020),
    };

    unsigned int first_index = keyword_invalid_index;
    unsigned int second_index = keyword_invalid_index;

    if (probe.length <= 1U) {
        return true;
    }
    if (probe.length >= (size_t)keyword_length_count) {
        return false;
    }

    if (probe.first >= 'A' && probe.first <= 'Z') {
        first_index = (unsigned int)(probe.first - 'A');
    } else if (probe.first == '_') {
        first_index = 26U;
    }

    probe.second = (unsigned char)ascii_upper(probe.second);
    if (probe.second >= 'A' && probe.second <= 'Z') {
        second_index = (unsigned int)(probe.second - 'A');
    } else if (probe.second == '_') {
        second_index = 26U;
    } else if (probe.second >= '0' && probe.second <= '9') {
        second_index = 27U + (unsigned int)(probe.second - '0');
    }

    if (first_index >= (unsigned int)keyword_first_count ||
        second_index >= (unsigned int)keyword_second_count) {
        return false;
    }
    return (second_masks[first_index][probe.length] & (UINT64_C(1) << second_index)) != 0U;
}

// NOLINTEND(readability-function-cognitive-complexity, readability-magic-numbers)

static int compare_keyword_text(const char *text, size_t length, const char *keyword) {
    size_t index = 1U;

    while (index < length && keyword[index] != '\0') {
        unsigned char actual = (unsigned char)ascii_upper((unsigned char)text[index]);
        unsigned char expected = (unsigned char)keyword[index];

        if (actual < expected) {
            return -1;
        }
        if (actual > expected) {
            return 1;
        }
        ++index;
    }

    if (index < length) {
        return 1;
    }
    if (keyword[index] != '\0') {
        return -1;
    }
    return 0;
}

static char ascii_upper(unsigned char byte) {
    if (byte >= 'a' && byte <= 'z') {
        return (char)(byte - ('a' - 'A'));
    }
    return (char)byte;
}
