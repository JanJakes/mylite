#include "mylite_lexer.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

struct mylite_keyword_entry {
    const char *word;
    unsigned int flags;
};

struct mylite_token_start {
    size_t offset;
    size_t line;
    size_t column;
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

static bool scan_comment(struct mylite_sql_lexer *lexer, unsigned int flags,
                         struct mylite_sql_token *out_token);
static bool scan_word(struct mylite_sql_lexer *lexer, unsigned int flags,
                      struct mylite_sql_token *out_token);
static bool scan_digit_leading_token(struct mylite_sql_lexer *lexer, unsigned int flags,
                                     struct mylite_sql_token *out_token);
static bool scan_dot_or_number(struct mylite_sql_lexer *lexer, unsigned int flags,
                               struct mylite_sql_token *out_token);
static bool scan_variable(struct mylite_sql_lexer *lexer, unsigned int flags,
                          struct mylite_sql_token *out_token);
static bool scan_operator_or_punctuation(struct mylite_sql_lexer *lexer, unsigned int flags,
                                         struct mylite_sql_token *out_token);
static bool scan_quoted_string(struct mylite_sql_lexer *lexer, struct mylite_token_start start,
                               struct mylite_quoted_string_options options,
                               struct mylite_sql_token *out_token);
static bool scan_quoted_identifier(struct mylite_sql_lexer *lexer, struct mylite_token_start start,
                                   char quote, struct mylite_sql_token *out_token);
static bool scan_quoted_hex_or_bit_literal(struct mylite_sql_lexer *lexer,
                                           struct mylite_token_start start,
                                           struct mylite_binary_literal_options options,
                                           struct mylite_sql_token *out_token);
static bool scan_prefixed_hex_or_bit_literal(struct mylite_sql_lexer *lexer,
                                             struct mylite_token_start start,
                                             struct mylite_binary_literal_options options,
                                             struct mylite_sql_token *out_token);
static bool scan_digit_leading_identifier(struct mylite_sql_lexer *lexer, unsigned int flags,
                                          struct mylite_sql_token *out_token);
static bool scan_unquoted_identifier(struct mylite_sql_lexer *lexer, unsigned int flags,
                                     struct mylite_sql_token *out_token);
static bool scan_quoted_variable(struct mylite_sql_lexer *lexer, struct mylite_token_start start,
                                 char quote, struct mylite_sql_token *out_token);
static struct mylite_token_start make_token_start(const struct mylite_sql_lexer *lexer,
                                                  unsigned int flags);
static void set_token(const struct mylite_sql_lexer *lexer, struct mylite_sql_token *out_token,
                      enum mylite_sql_token_kind kind, struct mylite_token_start start);
static void set_error_token(const struct mylite_sql_lexer *lexer,
                            struct mylite_sql_token *out_token, enum mylite_sql_lexer_error error,
                            struct mylite_token_start start);
static bool consume_whitespace(struct mylite_sql_lexer *lexer);
static void advance_one(struct mylite_sql_lexer *lexer);
static unsigned char peek_at(const struct mylite_sql_lexer *lexer, size_t lookahead);
static bool has_at(const struct mylite_sql_lexer *lexer, size_t lookahead);
static bool starts_with(const struct mylite_sql_lexer *lexer, const char *text);
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
static bool scan_exponent_span(const char *input, size_t length, size_t *cursor);
static char ascii_upper(unsigned char byte);

void mylite_sql_lexer_init(struct mylite_sql_lexer *lexer, struct mylite_sql_lexer_config config)
{
    if (lexer == NULL) {
        return;
    }

    lexer->input = config.input;
    lexer->length = config.input == NULL ? 0U : config.length;
    lexer->offset = 0U;
    lexer->line = 1U;
    lexer->column = 1U;
    lexer->modes = config.modes;
}

int mylite_sql_lexer_next(struct mylite_sql_lexer *lexer, struct mylite_sql_token *out_token)
{
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
                lexer, make_token_start(lexer, flags),
                (struct mylite_quoted_string_options){
                    .kind = MYLITE_SQL_TOKEN_STRING,
                    .quote = (char)byte,
                    .allow_backslash = (lexer->modes & MYLITE_SQL_MODE_NO_BACKSLASH_ESCAPES) == 0U,
                },
                out_token);
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

bool mylite_sql_keyword_lookup(const char *text, size_t length, unsigned int *out_flags)
{
    enum { keyword_buffer_size = 129 };
    char folded[keyword_buffer_size];
    size_t low = 0U;
    size_t high = 0U;

    static const struct mylite_keyword_entry keywords[] = {
        {"ACCESSIBLE", MYLITE_SQL_KEYWORD_RESERVED},
        {"ACCOUNT", 0U},
        {"ACTION", 0U},
        {"ACTIVE", 0U},
        {"ADD", MYLITE_SQL_KEYWORD_RESERVED},
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
        {"ARRAY", 0U},
        {"AS", MYLITE_SQL_KEYWORD_RESERVED},
        {"ASC", MYLITE_SQL_KEYWORD_RESERVED},
        {"ASCII", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"ASENSITIVE", MYLITE_SQL_KEYWORD_RESERVED},
        {"ASSIGN_GTIDS_TO_ANONYMOUS_TRANSACTIONS", 0U},
        {"AT", 0U},
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
        {"BINARY", MYLITE_SQL_KEYWORD_RESERVED},
        {"BINLOG", 0U},
        {"BIT", 0U},
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
        {"CAST", MYLITE_SQL_KEYWORD_RESERVED},
        {"CATALOG_NAME", 0U},
        {"CHAIN", 0U},
        {"CHALLENGE_RESPONSE", 0U},
        {"CHANGE", MYLITE_SQL_KEYWORD_RESERVED},
        {"CHANGED", 0U},
        {"CHANNEL", 0U},
        {"CHAR", MYLITE_SQL_KEYWORD_RESERVED},
        {"CHARACTER", MYLITE_SQL_KEYWORD_RESERVED},
        {"CHARSET", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"CHECK", MYLITE_SQL_KEYWORD_RESERVED},
        {"CHECKSUM", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"CIPHER", 0U},
        {"CLASS_ORIGIN", 0U},
        {"CLIENT", 0U},
        {"CLONE", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"CLOSE", 0U},
        {"COALESCE", 0U},
        {"CODE", 0U},
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
        {"COMPRESSED", 0U},
        {"COMPRESSION", 0U},
        {"CONCURRENT", 0U},
        {"CONDITION", MYLITE_SQL_KEYWORD_RESERVED},
        {"CONNECTION", 0U},
        {"CONSISTENT", 0U},
        {"CONSTRAINT", MYLITE_SQL_KEYWORD_RESERVED},
        {"CONSTRAINT_CATALOG", 0U},
        {"CONSTRAINT_NAME", 0U},
        {"CONSTRAINT_SCHEMA", 0U},
        {"CONTAINS", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"CONTEXT", 0U},
        {"CONTINUE", MYLITE_SQL_KEYWORD_RESERVED},
        {"CONVERT", MYLITE_SQL_KEYWORD_RESERVED},
        {"COPY", 0U},
        {"CPU", 0U},
        {"CREATE", MYLITE_SQL_KEYWORD_RESERVED},
        {"CROSS", MYLITE_SQL_KEYWORD_RESERVED},
        {"CUBE", 0U},
        {"CUME_DIST", MYLITE_SQL_KEYWORD_RESERVED},
        {"CURRENT", 0U},
        {"CURRENT_DATE", MYLITE_SQL_KEYWORD_RESERVED},
        {"CURRENT_TIME", MYLITE_SQL_KEYWORD_RESERVED},
        {"CURRENT_TIMESTAMP", MYLITE_SQL_KEYWORD_RESERVED},
        {"CURRENT_USER", MYLITE_SQL_KEYWORD_RESERVED},
        {"CURSOR", MYLITE_SQL_KEYWORD_RESERVED},
        {"CURSOR_NAME", 0U},
        {"DATA", 0U},
        {"DATABASE", MYLITE_SQL_KEYWORD_RESERVED},
        {"DATABASES", MYLITE_SQL_KEYWORD_RESERVED},
        {"DATAFILE", 0U},
        {"DATE", 0U},
        {"DATETIME", 0U},
        {"DAY", 0U},
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
        {"EXCLUSIVE", 0U},
        {"EXECUTE", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL | MYLITE_SQL_KEYWORD_RESTRICTED_ROLE},
        {"EXISTS", MYLITE_SQL_KEYWORD_RESERVED},
        {"EXIT", MYLITE_SQL_KEYWORD_RESERVED},
        {"EXPANSION", 0U},
        {"EXPIRE", 0U},
        {"EXPLAIN", MYLITE_SQL_KEYWORD_RESERVED},
        {"EXPORT", 0U},
        {"EXTENDED", 0U},
        {"EXTENT_SIZE", 0U},
        {"FACTOR", 0U},
        {"FAILED_LOGIN_ATTEMPTS", 0U},
        {"FALSE", MYLITE_SQL_KEYWORD_RESERVED},
        {"FAST", 0U},
        {"FAULTS", 0U},
        {"FETCH", MYLITE_SQL_KEYWORD_RESERVED},
        {"FIELDS", 0U},
        {"FILE", MYLITE_SQL_KEYWORD_RESTRICTED_ROLE},
        {"FILE_BLOCK_SIZE", 0U},
        {"FILTER", 0U},
        {"FINISH", 0U},
        {"FIRST", 0U},
        {"FIRST_VALUE", MYLITE_SQL_KEYWORD_RESERVED},
        {"FIXED", 0U},
        {"FLOAT", MYLITE_SQL_KEYWORD_RESERVED},
        {"FLOAT4", MYLITE_SQL_KEYWORD_RESERVED},
        {"FLOAT8", MYLITE_SQL_KEYWORD_RESERVED},
        {"FLUSH", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"FOLLOWING", 0U},
        {"FOLLOWS", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"FOR", MYLITE_SQL_KEYWORD_RESERVED},
        {"FORCE", MYLITE_SQL_KEYWORD_RESERVED},
        {"FOREIGN", MYLITE_SQL_KEYWORD_RESERVED},
        {"FORMAT", 0U},
        {"FOUND", 0U},
        {"FROM", MYLITE_SQL_KEYWORD_RESERVED},
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
        {"GROUP", MYLITE_SQL_KEYWORD_RESERVED},
        {"GROUPING", MYLITE_SQL_KEYWORD_RESERVED},
        {"GROUPS", MYLITE_SQL_KEYWORD_RESERVED},
        {"GROUP_REPLICATION", 0U},
        {"GTIDS", 0U},
        {"GTID_ONLY", 0U},
        {"HANDLER", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"HASH", 0U},
        {"HAVING", MYLITE_SQL_KEYWORD_RESERVED},
        {"HELP", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
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
        {"INPLACE", 0U},
        {"INSENSITIVE", MYLITE_SQL_KEYWORD_RESERVED},
        {"INSERT", MYLITE_SQL_KEYWORD_RESERVED},
        {"INSERT_METHOD", 0U},
        {"INSTALL", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"INSTANCE", 0U},
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
        {"ISOLATION", 0U},
        {"ISSUER", 0U},
        {"ITERATE", MYLITE_SQL_KEYWORD_RESERVED},
        {"JOIN", MYLITE_SQL_KEYWORD_RESERVED},
        {"JSON", 0U},
        {"JSON_TABLE", MYLITE_SQL_KEYWORD_RESERVED},
        {"JSON_VALUE", 0U},
        {"KEY", MYLITE_SQL_KEYWORD_RESERVED},
        {"KEYRING", 0U},
        {"KEYS", MYLITE_SQL_KEYWORD_RESERVED},
        {"KEY_BLOCK_SIZE", 0U},
        {"KILL", MYLITE_SQL_KEYWORD_RESERVED},
        {"LAG", MYLITE_SQL_KEYWORD_RESERVED},
        {"LANGUAGE", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"LAST", 0U},
        {"LAST_VALUE", MYLITE_SQL_KEYWORD_RESERVED},
        {"LATERAL", MYLITE_SQL_KEYWORD_RESERVED},
        {"LEAD", MYLITE_SQL_KEYWORD_RESERVED},
        {"LEADING", MYLITE_SQL_KEYWORD_RESERVED},
        {"LEAVE", MYLITE_SQL_KEYWORD_RESERVED},
        {"LEAVES", 0U},
        {"LEFT", MYLITE_SQL_KEYWORD_RESERVED},
        {"LESS", 0U},
        {"LEVEL", 0U},
        {"LIKE", MYLITE_SQL_KEYWORD_RESERVED},
        {"LIMIT", MYLITE_SQL_KEYWORD_RESERVED},
        {"LINEAR", MYLITE_SQL_KEYWORD_RESERVED},
        {"LINES", MYLITE_SQL_KEYWORD_RESERVED},
        {"LINESTRING", 0U},
        {"LIST", 0U},
        {"LOAD", MYLITE_SQL_KEYWORD_RESERVED},
        {"LOCAL", 0U},
        {"LOCALTIME", MYLITE_SQL_KEYWORD_RESERVED},
        {"LOCALTIMESTAMP", MYLITE_SQL_KEYWORD_RESERVED},
        {"LOCK", MYLITE_SQL_KEYWORD_RESERVED},
        {"LOCKED", 0U},
        {"LOCKS", 0U},
        {"LOG", 0U},
        {"LOGFILE", 0U},
        {"LOGS", 0U},
        {"LONG", MYLITE_SQL_KEYWORD_RESERVED},
        {"LONGBLOB", MYLITE_SQL_KEYWORD_RESERVED},
        {"LONGTEXT", MYLITE_SQL_KEYWORD_RESERVED},
        {"LOOP", MYLITE_SQL_KEYWORD_RESERVED},
        {"LOW_PRIORITY", MYLITE_SQL_KEYWORD_RESERVED},
        {"MANUAL", 0U},
        {"MASTER", 0U},
        {"MATCH", MYLITE_SQL_KEYWORD_RESERVED},
        {"MAXVALUE", MYLITE_SQL_KEYWORD_RESERVED},
        {"MAX_CONNECTIONS_PER_HOUR", 0U},
        {"MAX_QUERIES_PER_HOUR", 0U},
        {"MAX_ROWS", 0U},
        {"MAX_SIZE", 0U},
        {"MAX_UPDATES_PER_HOUR", 0U},
        {"MAX_USER_CONNECTIONS", 0U},
        {"MEDIUM", 0U},
        {"MEDIUMBLOB", MYLITE_SQL_KEYWORD_RESERVED},
        {"MEDIUMINT", MYLITE_SQL_KEYWORD_RESERVED},
        {"MEDIUMTEXT", MYLITE_SQL_KEYWORD_RESERVED},
        {"MEMBER", 0U},
        {"MEMORY", 0U},
        {"MERGE", 0U},
        {"MESSAGE_TEXT", 0U},
        {"MICROSECOND", 0U},
        {"MIDDLEINT", MYLITE_SQL_KEYWORD_RESERVED},
        {"MIGRATE", 0U},
        {"MINUTE", 0U},
        {"MINUTE_MICROSECOND", MYLITE_SQL_KEYWORD_RESERVED},
        {"MINUTE_SECOND", MYLITE_SQL_KEYWORD_RESERVED},
        {"MIN_ROWS", 0U},
        {"MOD", MYLITE_SQL_KEYWORD_RESERVED},
        {"MODE", 0U},
        {"MODIFIES", MYLITE_SQL_KEYWORD_RESERVED},
        {"MODIFY", 0U},
        {"MONTH", 0U},
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
        {"NOWAIT", 0U},
        {"NO_WAIT", 0U},
        {"NO_WRITE_TO_BINLOG", MYLITE_SQL_KEYWORD_RESERVED},
        {"NTH_VALUE", MYLITE_SQL_KEYWORD_RESERVED},
        {"NTILE", MYLITE_SQL_KEYWORD_RESERVED},
        {"NULL", MYLITE_SQL_KEYWORD_RESERVED},
        {"NULLS", 0U},
        {"NUMBER", 0U},
        {"NUMERIC", MYLITE_SQL_KEYWORD_RESERVED},
        {"NVARCHAR", 0U},
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
        {"PERSIST", 0U},
        {"PERSIST_ONLY", 0U},
        {"PHASE", 0U},
        {"PLUGIN", 0U},
        {"PLUGINS", 0U},
        {"PLUGIN_DIR", 0U},
        {"POINT", 0U},
        {"POLYGON", 0U},
        {"PORT", 0U},
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
        {"RANDOM", 0U},
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
        {"ROUTINE", 0U},
        {"ROW", MYLITE_SQL_KEYWORD_RESERVED},
        {"ROWS", MYLITE_SQL_KEYWORD_RESERVED},
        {"ROW_COUNT", 0U},
        {"ROW_FORMAT", 0U},
        {"ROW_NUMBER", MYLITE_SQL_KEYWORD_RESERVED},
        {"RTREE", 0U},
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
        {"SELECT", MYLITE_SQL_KEYWORD_RESERVED},
        {"SENSITIVE", MYLITE_SQL_KEYWORD_RESERVED},
        {"SEPARATOR", MYLITE_SQL_KEYWORD_RESERVED},
        {"SERIAL", 0U},
        {"SERIALIZABLE", 0U},
        {"SERVER", 0U},
        {"SESSION", 0U},
        {"SET", MYLITE_SQL_KEYWORD_RESERVED},
        {"SHARE", 0U},
        {"SHARED", 0U},
        {"SHOW", MYLITE_SQL_KEYWORD_RESERVED},
        {"SHUTDOWN", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL | MYLITE_SQL_KEYWORD_RESTRICTED_ROLE},
        {"SIGNAL", MYLITE_SQL_KEYWORD_RESERVED},
        {"SIGNED", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"SIMPLE", 0U},
        {"SKIP", 0U},
        {"SLAVE", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"SLOW", 0U},
        {"SMALLINT", MYLITE_SQL_KEYWORD_RESERVED},
        {"SNAPSHOT", 0U},
        {"SOCKET", 0U},
        {"SOME", 0U},
        {"SONAME", 0U},
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
        {"STREAM", 0U},
        {"STRING", 0U},
        {"SUBCLASS_ORIGIN", 0U},
        {"SUBJECT", 0U},
        {"SUBPARTITION", 0U},
        {"SUBPARTITIONS", 0U},
        {"SUPER", MYLITE_SQL_KEYWORD_RESTRICTED_ROLE},
        {"SUSPEND", 0U},
        {"SWAPS", 0U},
        {"SWITCHES", 0U},
        {"SYSTEM", MYLITE_SQL_KEYWORD_RESERVED},
        {"TABLE", MYLITE_SQL_KEYWORD_RESERVED},
        {"TABLES", 0U},
        {"TABLESAMPLE", 0U},
        {"TABLESPACE", 0U},
        {"TABLE_CHECKSUM", 0U},
        {"TABLE_NAME", 0U},
        {"TEMPORARY", 0U},
        {"TEMPTABLE", 0U},
        {"TERMINATED", MYLITE_SQL_KEYWORD_RESERVED},
        {"TEXT", 0U},
        {"THAN", 0U},
        {"THEN", MYLITE_SQL_KEYWORD_RESERVED},
        {"THREAD_PRIORITY", 0U},
        {"TIES", 0U},
        {"TIME", 0U},
        {"TIMESTAMP", 0U},
        {"TIMESTAMPADD", 0U},
        {"TIMESTAMPDIFF", 0U},
        {"TINYBLOB", MYLITE_SQL_KEYWORD_RESERVED},
        {"TINYINT", MYLITE_SQL_KEYWORD_RESERVED},
        {"TINYTEXT", MYLITE_SQL_KEYWORD_RESERVED},
        {"TLS", 0U},
        {"TO", MYLITE_SQL_KEYWORD_RESERVED},
        {"TRAILING", MYLITE_SQL_KEYWORD_RESERVED},
        {"TRANSACTION", 0U},
        {"TRIGGER", MYLITE_SQL_KEYWORD_RESERVED},
        {"TRIGGERS", 0U},
        {"TRUE", MYLITE_SQL_KEYWORD_RESERVED},
        {"TRUNCATE", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"TYPE", 0U},
        {"TYPES", 0U},
        {"UNBOUNDED", 0U},
        {"UNCOMMITTED", 0U},
        {"UNDEFINED", 0U},
        {"UNDO", MYLITE_SQL_KEYWORD_RESERVED},
        {"UNDOFILE", 0U},
        {"UNDO_BUFFER_SIZE", 0U},
        {"UNICODE", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"UNINSTALL", MYLITE_SQL_KEYWORD_RESTRICTED_LABEL},
        {"UNION", MYLITE_SQL_KEYWORD_RESERVED},
        {"UNIQUE", MYLITE_SQL_KEYWORD_RESERVED},
        {"UNKNOWN", 0U},
        {"UNLOCK", MYLITE_SQL_KEYWORD_RESERVED},
        {"UNREGISTER", 0U},
        {"UNSIGNED", MYLITE_SQL_KEYWORD_RESERVED},
        {"UNTIL", 0U},
        {"UPDATE", MYLITE_SQL_KEYWORD_RESERVED},
        {"UPGRADE", 0U},
        {"URL", 0U},
        {"USAGE", MYLITE_SQL_KEYWORD_RESERVED},
        {"USE", MYLITE_SQL_KEYWORD_RESERVED},
        {"USER", 0U},
        {"USER_RESOURCES", 0U},
        {"USE_FRM", 0U},
        {"USING", MYLITE_SQL_KEYWORD_RESERVED},
        {"UTC_DATE", MYLITE_SQL_KEYWORD_RESERVED},
        {"UTC_TIME", MYLITE_SQL_KEYWORD_RESERVED},
        {"UTC_TIMESTAMP", MYLITE_SQL_KEYWORD_RESERVED},
        {"VALIDATION", 0U},
        {"VALUE", 0U},
        {"VALUES", MYLITE_SQL_KEYWORD_RESERVED},
        {"VARBINARY", MYLITE_SQL_KEYWORD_RESERVED},
        {"VARCHAR", MYLITE_SQL_KEYWORD_RESERVED},
        {"VARCHARACTER", MYLITE_SQL_KEYWORD_RESERVED},
        {"VARIABLES", 0U},
        {"VARYING", MYLITE_SQL_KEYWORD_RESERVED},
        {"VCPU", 0U},
        {"VIEW", 0U},
        {"VIRTUAL", MYLITE_SQL_KEYWORD_RESERVED},
        {"VISIBLE", 0U},
        {"WAIT", 0U},
        {"WARNINGS", 0U},
        {"WEEK", 0U},
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
        {"YEAR_MONTH", MYLITE_SQL_KEYWORD_RESERVED},
        {"ZEROFILL", MYLITE_SQL_KEYWORD_RESERVED},
        {"ZONE", 0U},
        {"_FILENAME", MYLITE_SQL_KEYWORD_RESERVED}};

    if (out_flags != NULL) {
        *out_flags = 0U;
    }

    if (text == NULL || length == 0U || length >= sizeof(folded)) {
        return false;
    }

    for (size_t index = 0U; index < length; ++index) {
        unsigned char byte = (unsigned char)text[index];
        if (byte >= mysql_non_ascii_min) {
            return false;
        }
        folded[index] = ascii_upper(byte);
    }
    folded[length] = '\0';

    high = sizeof(keywords) / sizeof(keywords[0]);
    while (low < high) {
        size_t middle = low + ((high - low) / 2U);
        int compare = strcmp(folded, keywords[middle].word);
        if (compare == 0) {
            if (out_flags != NULL) {
                *out_flags = keywords[middle].flags;
            }
            return true;
        }
        if (compare < 0) {
            high = middle;
        } else {
            low = middle + 1U;
        }
    }

    return false;
}

const char *mylite_sql_token_kind_name(enum mylite_sql_token_kind kind)
{
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
    }

    return "unknown";
}

const char *mylite_sql_operator_kind_name(enum mylite_sql_operator_kind kind)
{
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

const char *mylite_sql_lexer_error_name(enum mylite_sql_lexer_error error)
{
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

static bool scan_comment(struct mylite_sql_lexer *lexer, unsigned int flags,
                         struct mylite_sql_token *out_token)
{
    struct mylite_token_start start = make_token_start(lexer, flags);
    enum mylite_sql_token_kind kind = MYLITE_SQL_TOKEN_COMMENT;

    assert(lexer != NULL);
    assert(out_token != NULL);

    if (peek_at(lexer, 0U) == '#' || (peek_at(lexer, 0U) == '-' && peek_at(lexer, 1U) == '-')) {
        while (lexer->offset < lexer->length) {
            unsigned char byte = peek_at(lexer, 0U);
            if (byte == '\n' || byte == '\r') {
                break;
            }
            advance_one(lexer);
        }
        set_token(lexer, out_token, kind, start);
        return true;
    }

    if (peek_at(lexer, 2U) == '!') {
        kind = MYLITE_SQL_TOKEN_VERSION_COMMENT;
    } else if (peek_at(lexer, 2U) == '+') {
        kind = MYLITE_SQL_TOKEN_HINT_COMMENT;
    }

    advance_one(lexer);
    advance_one(lexer);
    while (lexer->offset < lexer->length) {
        if (peek_at(lexer, 0U) == '*' && peek_at(lexer, 1U) == '/') {
            advance_one(lexer);
            advance_one(lexer);
            set_token(lexer, out_token, kind, start);
            return true;
        }
        advance_one(lexer);
    }

    set_error_token(lexer, out_token, MYLITE_SQL_LEXER_ERROR_UNTERMINATED_COMMENT, start);
    return true;
}

static bool scan_word(struct mylite_sql_lexer *lexer, unsigned int flags,
                      struct mylite_sql_token *out_token)
{
    unsigned char byte = peek_at(lexer, 0U);
    unsigned char next = peek_at(lexer, 1U);
    struct mylite_token_start start = make_token_start(lexer, flags);

    if ((byte == 'N' || byte == 'n') && next == '\'') {
        advance_one(lexer);
        (void)scan_quoted_string(
            lexer, start,
            (struct mylite_quoted_string_options){
                .kind = MYLITE_SQL_TOKEN_NATIONAL_STRING,
                .quote = '\'',
                .allow_backslash = (lexer->modes & MYLITE_SQL_MODE_NO_BACKSLASH_ESCAPES) == 0U,
            },
            out_token);
        return true;
    }

    if ((byte == 'X' || byte == 'x') && next == '\'') {
        advance_one(lexer);
        (void)scan_quoted_hex_or_bit_literal(lexer, start,
                                             (struct mylite_binary_literal_options){
                                                 .kind = MYLITE_SQL_TOKEN_HEX_LITERAL,
                                                 .hex = true,
                                             },
                                             out_token);
        return true;
    }

    if ((byte == 'B' || byte == 'b') && next == '\'') {
        advance_one(lexer);
        (void)scan_quoted_hex_or_bit_literal(lexer, start,
                                             (struct mylite_binary_literal_options){
                                                 .kind = MYLITE_SQL_TOKEN_BIT_LITERAL,
                                                 .hex = false,
                                             },
                                             out_token);
        return true;
    }

    return scan_unquoted_identifier(lexer, flags, out_token);
}

static bool scan_digit_leading_token(struct mylite_sql_lexer *lexer, unsigned int flags,
                                     struct mylite_sql_token *out_token)
{
    struct mylite_token_start start = make_token_start(lexer, flags);
    size_t cursor = lexer->offset;
    bool saw_dot = false;
    bool saw_exponent = false;
    enum mylite_sql_token_kind kind = MYLITE_SQL_TOKEN_INTEGER;

    if (starts_with(lexer, "0x") && is_hex_digit(peek_at(lexer, 2U))) {
        return scan_prefixed_hex_or_bit_literal(lexer, start,
                                                (struct mylite_binary_literal_options){
                                                    .kind = MYLITE_SQL_TOKEN_HEX_LITERAL,
                                                    .hex = true,
                                                },
                                                out_token);
    }

    if (starts_with(lexer, "0b") && is_bit_digit(peek_at(lexer, 2U))) {
        return scan_prefixed_hex_or_bit_literal(lexer, start,
                                                (struct mylite_binary_literal_options){
                                                    .kind = MYLITE_SQL_TOKEN_BIT_LITERAL,
                                                    .hex = false,
                                                },
                                                out_token);
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

    while (lexer->offset < cursor) {
        advance_one(lexer);
    }

    if (saw_exponent) {
        kind = MYLITE_SQL_TOKEN_FLOAT;
    } else if (saw_dot) {
        kind = MYLITE_SQL_TOKEN_DECIMAL;
    }

    set_token(lexer, out_token, kind, start);
    return true;
}

static bool scan_dot_or_number(struct mylite_sql_lexer *lexer, unsigned int flags,
                               struct mylite_sql_token *out_token)
{
    struct mylite_token_start start = make_token_start(lexer, flags);
    bool saw_exponent = false;
    enum mylite_sql_token_kind kind = MYLITE_SQL_TOKEN_DECIMAL;

    if (!is_digit(peek_at(lexer, 1U))) {
        advance_one(lexer);
        set_token(lexer, out_token, MYLITE_SQL_TOKEN_PUNCTUATION, start);
        return true;
    }

    advance_one(lexer);
    while (is_digit(peek_at(lexer, 0U))) {
        advance_one(lexer);
    }

    if (peek_at(lexer, 0U) == 'e' || peek_at(lexer, 0U) == 'E') {
        size_t exponent = lexer->offset + 1U;
        if (exponent < lexer->length &&
            (lexer->input[exponent] == '+' || lexer->input[exponent] == '-')) {
            ++exponent;
        }
        if (exponent < lexer->length && is_digit((unsigned char)lexer->input[exponent])) {
            saw_exponent = true;
            advance_one(lexer);
            if (peek_at(lexer, 0U) == '+' || peek_at(lexer, 0U) == '-') {
                advance_one(lexer);
            }
            while (is_digit(peek_at(lexer, 0U))) {
                advance_one(lexer);
            }
        }
    }

    if (saw_exponent) {
        kind = MYLITE_SQL_TOKEN_FLOAT;
    }

    set_token(lexer, out_token, kind, start);
    return true;
}

static bool scan_variable(struct mylite_sql_lexer *lexer, unsigned int flags,
                          struct mylite_sql_token *out_token)
{
    struct mylite_token_start start = make_token_start(lexer, flags);

    assert(peek_at(lexer, 0U) == '@');

    advance_one(lexer);
    if (peek_at(lexer, 0U) == '@') {
        advance_one(lexer);
        if (!is_identifier_part(peek_at(lexer, 0U)) && peek_at(lexer, 0U) != '.') {
            set_error_token(lexer, out_token, MYLITE_SQL_LEXER_ERROR_INVALID_VARIABLE, start);
            return true;
        }
        while (is_identifier_part(peek_at(lexer, 0U)) || peek_at(lexer, 0U) == '.') {
            advance_one(lexer);
        }
        set_token(lexer, out_token, MYLITE_SQL_TOKEN_SYSTEM_VARIABLE, start);
        return true;
    }

    if (peek_at(lexer, 0U) == '\'' || peek_at(lexer, 0U) == '"' || peek_at(lexer, 0U) == '`') {
        return scan_quoted_variable(lexer, start, (char)peek_at(lexer, 0U), out_token);
    }

    if (!is_user_variable_part(peek_at(lexer, 0U))) {
        set_error_token(lexer, out_token, MYLITE_SQL_LEXER_ERROR_INVALID_VARIABLE, start);
        return true;
    }

    while (is_user_variable_part(peek_at(lexer, 0U))) {
        advance_one(lexer);
    }
    set_token(lexer, out_token, MYLITE_SQL_TOKEN_USER_VARIABLE, start);
    return true;
}

static bool scan_operator_or_punctuation(struct mylite_sql_lexer *lexer, unsigned int flags,
                                         struct mylite_sql_token *out_token)
{
    struct operator_candidate {
        const char *text;
        enum mylite_sql_operator_kind kind;
    };
    static const struct operator_candidate operators[] = {
        {"->>", MYLITE_SQL_OPERATOR_JSON_UNQUOTE_EXTRACT},
        {"<=>", MYLITE_SQL_OPERATOR_NULL_SAFE_EQUAL},
        {"<<", MYLITE_SQL_OPERATOR_LEFT_SHIFT},
        {">>", MYLITE_SQL_OPERATOR_RIGHT_SHIFT},
        {"<=", MYLITE_SQL_OPERATOR_LESS_EQUAL},
        {">=", MYLITE_SQL_OPERATOR_GREATER_EQUAL},
        {"<>", MYLITE_SQL_OPERATOR_NOT_EQUAL},
        {"!=", MYLITE_SQL_OPERATOR_NOT_EQUAL},
        {"&&", MYLITE_SQL_OPERATOR_LOGICAL_AND},
        {"||", MYLITE_SQL_OPERATOR_LOGICAL_OR},
        {":=", MYLITE_SQL_OPERATOR_ASSIGN},
        {"->", MYLITE_SQL_OPERATOR_JSON_EXTRACT},
        {"=", MYLITE_SQL_OPERATOR_EQUAL},
        {"<", MYLITE_SQL_OPERATOR_LESS},
        {">", MYLITE_SQL_OPERATOR_GREATER},
        {"+", MYLITE_SQL_OPERATOR_PLUS},
        {"-", MYLITE_SQL_OPERATOR_MINUS},
        {"*", MYLITE_SQL_OPERATOR_STAR},
        {"/", MYLITE_SQL_OPERATOR_SLASH},
        {"%", MYLITE_SQL_OPERATOR_PERCENT},
        {"!", MYLITE_SQL_OPERATOR_NOT},
        {"~", MYLITE_SQL_OPERATOR_BITWISE_NOT},
        {"^", MYLITE_SQL_OPERATOR_BITWISE_XOR},
        {"&", MYLITE_SQL_OPERATOR_BITWISE_AND},
        {"|", MYLITE_SQL_OPERATOR_BITWISE_OR},
    };
    struct mylite_token_start start = make_token_start(lexer, flags);

    for (size_t index = 0U; index < (sizeof(operators) / sizeof(operators[0])); ++index) {
        if (starts_with(lexer, operators[index].text)) {
            size_t length = strlen(operators[index].text);
            for (size_t consumed = 0U; consumed < length; ++consumed) {
                advance_one(lexer);
            }
            set_token(lexer, out_token, MYLITE_SQL_TOKEN_OPERATOR, start);
            out_token->operator_kind = operators[index].kind;
            return true;
        }
    }

    if (is_punctuation(peek_at(lexer, 0U))) {
        advance_one(lexer);
        set_token(lexer, out_token, MYLITE_SQL_TOKEN_PUNCTUATION, start);
        return true;
    }

    return false;
}

static bool scan_quoted_string(struct mylite_sql_lexer *lexer, struct mylite_token_start start,
                               struct mylite_quoted_string_options options,
                               struct mylite_sql_token *out_token)
{
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

static bool scan_quoted_identifier(struct mylite_sql_lexer *lexer, struct mylite_token_start start,
                                   char quote, struct mylite_sql_token *out_token)
{
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

static bool scan_quoted_hex_or_bit_literal(struct mylite_sql_lexer *lexer,
                                           struct mylite_token_start start,
                                           struct mylite_binary_literal_options options,
                                           struct mylite_sql_token *out_token)
{
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

static bool scan_prefixed_hex_or_bit_literal(struct mylite_sql_lexer *lexer,
                                             struct mylite_token_start start,
                                             struct mylite_binary_literal_options options,
                                             struct mylite_sql_token *out_token)
{
    size_t cursor = lexer->offset + 2U;

    while (cursor < lexer->length &&
           is_binary_literal_digit((unsigned char)lexer->input[cursor], options.hex)) {
        ++cursor;
    }

    if (cursor < lexer->length && is_identifier_part((unsigned char)lexer->input[cursor])) {
        return scan_digit_leading_identifier(lexer, start.flags, out_token);
    }

    while (lexer->offset < cursor) {
        advance_one(lexer);
    }

    set_token(lexer, out_token, options.kind, start);
    return true;
}

static bool scan_digit_leading_identifier(struct mylite_sql_lexer *lexer, unsigned int flags,
                                          struct mylite_sql_token *out_token)
{
    struct mylite_token_start start = make_token_start(lexer, flags);

    while (is_identifier_part(peek_at(lexer, 0U))) {
        advance_one(lexer);
    }

    set_token(lexer, out_token, MYLITE_SQL_TOKEN_IDENTIFIER, start);
    return true;
}

static bool scan_unquoted_identifier(struct mylite_sql_lexer *lexer, unsigned int flags,
                                     struct mylite_sql_token *out_token)
{
    struct mylite_token_start start = make_token_start(lexer, flags);
    unsigned int keyword_flags = 0U;
    enum mylite_sql_token_kind kind = MYLITE_SQL_TOKEN_IDENTIFIER;

    while (is_identifier_part(peek_at(lexer, 0U))) {
        advance_one(lexer);
    }

    if (mylite_sql_keyword_lookup(&lexer->input[start.offset], lexer->offset - start.offset,
                                  &keyword_flags)) {
        kind = MYLITE_SQL_TOKEN_KEYWORD;
    }

    set_token(lexer, out_token, kind, start);
    out_token->keyword_flags = keyword_flags;
    return true;
}

static bool scan_quoted_variable(struct mylite_sql_lexer *lexer, struct mylite_token_start start,
                                 char quote, struct mylite_sql_token *out_token)
{
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

static struct mylite_token_start make_token_start(const struct mylite_sql_lexer *lexer,
                                                  unsigned int flags)
{
    return (struct mylite_token_start){
        .offset = lexer->offset,
        .line = lexer->line,
        .column = lexer->column,
        .flags = flags,
    };
}

static void set_token(const struct mylite_sql_lexer *lexer, struct mylite_sql_token *out_token,
                      enum mylite_sql_token_kind kind, struct mylite_token_start start)
{
    out_token->kind = kind;
    out_token->operator_kind = MYLITE_SQL_OPERATOR_NONE;
    out_token->error = MYLITE_SQL_LEXER_ERROR_NONE;
    out_token->text = lexer->input == NULL ? NULL : &lexer->input[start.offset];
    out_token->length = lexer->offset - start.offset;
    out_token->offset = start.offset;
    out_token->line = start.line;
    out_token->column = start.column;
    out_token->flags = start.flags;
    out_token->keyword_flags = 0U;
}

static void set_error_token(const struct mylite_sql_lexer *lexer,
                            struct mylite_sql_token *out_token, enum mylite_sql_lexer_error error,
                            struct mylite_token_start start)
{
    set_token(lexer, out_token, MYLITE_SQL_TOKEN_ERROR, start);
    out_token->error = error;
}

static bool consume_whitespace(struct mylite_sql_lexer *lexer)
{
    bool consumed = false;

    while (is_space(peek_at(lexer, 0U))) {
        consumed = true;
        advance_one(lexer);
    }

    return consumed;
}

static void advance_one(struct mylite_sql_lexer *lexer)
{
    unsigned char byte = 0U;

    if (lexer->offset >= lexer->length) {
        return;
    }

    byte = (unsigned char)lexer->input[lexer->offset];
    if (byte == '\r') {
        ++lexer->line;
        lexer->column = 1U;
    } else if (byte == '\n') {
        if (lexer->offset == 0U || lexer->input[lexer->offset - 1U] != '\r') {
            ++lexer->line;
        }
        lexer->column = 1U;
    } else {
        ++lexer->column;
    }
    ++lexer->offset;
}

static unsigned char peek_at(const struct mylite_sql_lexer *lexer, size_t lookahead)
{
    size_t index = 0U;
    if (lexer->input == NULL || lexer->offset >= lexer->length ||
        lookahead >= lexer->length - lexer->offset) {
        return '\0';
    }
    index = lexer->offset + lookahead;
    return (unsigned char)lexer->input[index];
}

static bool has_at(const struct mylite_sql_lexer *lexer, size_t lookahead)
{
    if (lexer->input != NULL && lexer->offset < lexer->length &&
        lookahead < lexer->length - lexer->offset) {
        return true;
    }
    return false;
}

static bool starts_with(const struct mylite_sql_lexer *lexer, const char *text)
{
    size_t length = strlen(text);
    if (lexer->input == NULL || lexer->offset > lexer->length ||
        length > lexer->length - lexer->offset) {
        return false;
    }
    return memcmp(&lexer->input[lexer->offset], text, length) == 0;
}

static bool starts_comment(const struct mylite_sql_lexer *lexer)
{
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

static bool is_mysql_comment_space(unsigned char byte)
{
    if (byte <= mysql_comment_space_max) {
        return true;
    }
    return false;
}

static bool is_space(unsigned char byte)
{
    if (byte == ' ' || byte == '\t' || byte == '\n' || byte == '\r' || byte == '\f' ||
        byte == '\v') {
        return true;
    }
    return false;
}

static bool is_digit(unsigned char byte)
{
    if (byte >= '0' && byte <= '9') {
        return true;
    }
    return false;
}

static bool is_hex_digit(unsigned char byte)
{
    if (is_digit(byte) || (byte >= 'A' && byte <= 'F') || (byte >= 'a' && byte <= 'f')) {
        return true;
    }
    return false;
}

static bool is_bit_digit(unsigned char byte)
{
    if (byte == '0' || byte == '1') {
        return true;
    }
    return false;
}

static bool is_identifier_start(unsigned char byte)
{
    if ((byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') || byte == '_' ||
        byte == '$' || byte >= mysql_non_ascii_min) {
        return true;
    }
    return false;
}

static bool is_identifier_part(unsigned char byte)
{
    if (is_identifier_start(byte) || is_digit(byte)) {
        return true;
    }
    return false;
}

static bool is_user_variable_part(unsigned char byte)
{
    if (is_digit(byte) || (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') ||
        byte == '.' || byte == '_' || byte == '$') {
        return true;
    }
    return false;
}

static bool is_punctuation(unsigned char byte)
{
    if (byte == '(' || byte == ')' || byte == ',' || byte == ';' || byte == '.' || byte == '{' ||
        byte == '}' || byte == '[' || byte == ']' || byte == ':') {
        return true;
    }
    return false;
}

static bool is_exponent_marker(unsigned char byte)
{
    if (byte == 'e' || byte == 'E') {
        return true;
    }
    return false;
}

static bool is_sign(unsigned char byte)
{
    if (byte == '+' || byte == '-') {
        return true;
    }
    return false;
}

static bool is_binary_literal_digit(unsigned char byte, bool hex)
{
    if (hex) {
        return is_hex_digit(byte);
    }
    return is_bit_digit(byte);
}

static enum mylite_sql_lexer_error binary_literal_error(bool hex)
{
    if (hex) {
        return MYLITE_SQL_LEXER_ERROR_INVALID_HEX_LITERAL;
    }
    return MYLITE_SQL_LEXER_ERROR_INVALID_BIT_LITERAL;
}

static size_t skip_digits(const char *input, size_t length, size_t cursor)
{
    while (cursor < length && is_digit((unsigned char)input[cursor])) {
        ++cursor;
    }
    return cursor;
}

static bool scan_exponent_span(const char *input, size_t length, size_t *cursor)
{
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

static char ascii_upper(unsigned char byte)
{
    if (byte >= 'a' && byte <= 'z') {
        return (char)(byte - ('a' - 'A'));
    }
    return (char)byte;
}
