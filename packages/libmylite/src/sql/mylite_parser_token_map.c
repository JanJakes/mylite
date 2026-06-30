#include "mylite_parser_token_map.h"

#include "mylite_parse.h"
#include "mylite_parser_helpers.h"

#include <stddef.h>
#include <string.h>

static bool map_direct_lexer_token(enum mylite_sql_token_kind kind, int *out_parser_token);
static bool map_keyword_token(
    const struct mylite_sql_parser_state *state,
    const struct mylite_sql_lexer *lexer,
    const struct mylite_sql_token *token,
    bool previous_token_was_dot,
    int previous_parser_token,
    int token_before_previous_parser_token,
    int *out_parser_token
);
static bool map_punctuation_token(const struct mylite_sql_token *token, int *out_parser_token);
static bool map_operator_token(
    const struct mylite_sql_parser_state *state,
    const struct mylite_sql_token *token,
    int *out_parser_token
);
static bool previous_token_allows_select_noop_modifier(int previous_parser_token);
static bool previous_token_allows_delete_quick_modifier(
    int previous_parser_token,
    int token_before_previous_parser_token
);
static bool map_keyword_index(unsigned int keyword_index, int *out_parser_token);
static bool lexer_token_has_immediate_left_paren(
    const struct mylite_sql_lexer *lexer,
    const struct mylite_sql_token *token
);
static bool token_text_matches_keyword_mapping(
    const struct mylite_sql_token *token,
    const char *text,
    unsigned char token_first,
    size_t token_length
);

bool mylite_sql_parser_map_lexer_token(
    const struct mylite_sql_parser_state *state,
    const struct mylite_sql_lexer *lexer,
    const struct mylite_sql_token *token,
    bool previous_token_was_dot,
    const struct mylite_sql_parser_token_history *history,
    struct mylite_sql_parser_token_map *out_map
) {
    int parser_token = 0;
    int previous_parser_token = history == NULL ? 0 : history->previous_parser_token;
    int token_before_previous_parser_token =
        history == NULL ? 0 : history->token_before_previous_parser_token;

    if (token == NULL || out_map == NULL) {
        return false;
    }

    if (token->kind == MYLITE_SQL_TOKEN_EOF) {
        *out_map = (struct mylite_sql_parser_token_map){
            .parser_token = 0,
            .previous_token_was_dot = false,
        };
        return true;
    }

    if (!map_direct_lexer_token(token->kind, &parser_token)) {
        switch (token->kind) {
        case MYLITE_SQL_TOKEN_KEYWORD:
            if (!map_keyword_token(
                    state,
                    lexer,
                    token,
                    previous_token_was_dot,
                    previous_parser_token,
                    token_before_previous_parser_token,
                    &parser_token
                )) {
                return false;
            }
            break;
        case MYLITE_SQL_TOKEN_OPERATOR:
            if (!map_operator_token(state, token, &parser_token)) {
                return false;
            }
            break;
        case MYLITE_SQL_TOKEN_PUNCTUATION:
            if (!map_punctuation_token(token, &parser_token)) {
                return false;
            }
            break;
        default:
            return false;
        }
    }

    *out_map = (struct mylite_sql_parser_token_map){
        .parser_token = parser_token,
        .previous_token_was_dot = parser_token == MYLITE_SQL_PARSE_DOT,
    };
    return true;
}

bool mylite_sql_parser_should_skip_select_lock_target_list(
    const struct mylite_sql_token *token,
    const struct mylite_sql_parser_token_history *history
) {
    if (token == NULL || !mylite_sql_parser_token_text_equals(token, "OF")) {
        return false;
    }
    if (history == NULL || history->token_before_previous_parser_token != MYLITE_SQL_PARSE_FOR) {
        return false;
    }
    return history->previous_parser_token == MYLITE_SQL_PARSE_UPDATE ||
           history->previous_parser_token == MYLITE_SQL_PARSE_SHARE;
}

enum mylite_sql_parse_status mylite_sql_parser_skip_select_lock_target_list(
    struct mylite_sql_lexer *lexer,
    struct mylite_sql_token *out_next_token
) {
    bool expecting_identifier = true;

    for (;;) {
        if (mylite_sql_lexer_next(lexer, out_next_token) != 0) {
            return MYLITE_SQL_PARSE_MISUSE;
        }
        if (mylite_sql_parser_token_is_comment(out_next_token->kind)) {
            continue;
        }
        if (out_next_token->kind == MYLITE_SQL_TOKEN_ERROR) {
            return MYLITE_SQL_PARSE_LEXER_ERROR;
        }
        if (out_next_token->kind == MYLITE_SQL_TOKEN_EOF) {
            return expecting_identifier ? MYLITE_SQL_PARSE_SYNTAX_ERROR : MYLITE_SQL_PARSE_OK;
        }

        if (expecting_identifier) {
            if (!mylite_sql_parser_token_can_be_select_lock_target_identifier(out_next_token)) {
                return MYLITE_SQL_PARSE_SYNTAX_ERROR;
            }
            expecting_identifier = false;
            continue;
        }

        if (out_next_token->kind == MYLITE_SQL_TOKEN_PUNCTUATION && out_next_token->length == 1U &&
            (out_next_token->text[0] == '.' || out_next_token->text[0] == ',')) {
            expecting_identifier = true;
            continue;
        }

        return MYLITE_SQL_PARSE_OK;
    }
}

bool mylite_sql_parser_token_can_be_select_lock_target_identifier(
    const struct mylite_sql_token *token
) {
    if (token->kind == MYLITE_SQL_TOKEN_IDENTIFIER ||
        token->kind == MYLITE_SQL_TOKEN_QUOTED_IDENTIFIER) {
        return true;
    }
    if (token->kind != MYLITE_SQL_TOKEN_KEYWORD) {
        return false;
    }
    if ((token->keyword_flags & MYLITE_SQL_KEYWORD_RESERVED) != 0U) {
        return false;
    }
    return !mylite_sql_parser_token_text_equals(token, "FOR") &&
           !mylite_sql_parser_token_text_equals(token, "LOCK") &&
           !mylite_sql_parser_token_text_equals(token, "LOCKED") &&
           !mylite_sql_parser_token_text_equals(token, "NOWAIT") &&
           !mylite_sql_parser_token_text_equals(token, "SHARE") &&
           !mylite_sql_parser_token_text_equals(token, "SKIP") &&
           !mylite_sql_parser_token_text_equals(token, "UPDATE");
}

bool mylite_sql_parser_token_is_comment(enum mylite_sql_token_kind kind) {
    if (kind == MYLITE_SQL_TOKEN_COMMENT || kind == MYLITE_SQL_TOKEN_VERSION_COMMENT ||
        kind == MYLITE_SQL_TOKEN_HINT_COMMENT) {
        return true;
    }
    return false;
}

bool mylite_sql_parser_token_is_left_paren(const struct mylite_sql_token *token) {
    return token != NULL && token->kind == MYLITE_SQL_TOKEN_PUNCTUATION && token->length == 1U &&
           token->text != NULL && token->text[0] == '(';
}

bool mylite_sql_parser_token_is_right_paren(const struct mylite_sql_token *token) {
    return token != NULL && token->kind == MYLITE_SQL_TOKEN_PUNCTUATION && token->length == 1U &&
           token->text != NULL && token->text[0] == ')';
}

bool mylite_sql_parser_token_is_comma(const struct mylite_sql_token *token) {
    return token != NULL && token->kind == MYLITE_SQL_TOKEN_PUNCTUATION && token->length == 1U &&
           token->text != NULL && token->text[0] == ',';
}

bool mylite_sql_parser_token_is_equal_sign(const struct mylite_sql_token *token) {
    return (token != NULL && token->kind == MYLITE_SQL_TOKEN_PUNCTUATION && token->length == 1U &&
            token->text != NULL && token->text[0] == '=') ||
           (token != NULL && token->kind == MYLITE_SQL_TOKEN_OPERATOR &&
            token->operator_kind == MYLITE_SQL_OPERATOR_EQUAL);
}

bool mylite_sql_parser_token_is_string_literal(const struct mylite_sql_token *token) {
    return token != NULL && (token->kind == MYLITE_SQL_TOKEN_STRING ||
                             token->kind == MYLITE_SQL_TOKEN_NATIONAL_STRING);
}

void mylite_sql_parser_update_token_history(
    struct mylite_sql_parser_token_history *history,
    int parser_token
) {
    if (history == NULL) {
        return;
    }
    history->token_before_previous_parser_token = history->previous_parser_token;
    history->previous_parser_token = parser_token;
}

static bool map_direct_lexer_token(enum mylite_sql_token_kind kind, int *out_parser_token) {
    switch (kind) {
    case MYLITE_SQL_TOKEN_IDENTIFIER:
        *out_parser_token = MYLITE_SQL_PARSE_IDENTIFIER;
        return true;
    case MYLITE_SQL_TOKEN_QUOTED_IDENTIFIER:
        *out_parser_token = MYLITE_SQL_PARSE_QUOTED_IDENTIFIER;
        return true;
    case MYLITE_SQL_TOKEN_STRING:
        *out_parser_token = MYLITE_SQL_PARSE_STRING;
        return true;
    case MYLITE_SQL_TOKEN_NATIONAL_STRING:
        *out_parser_token = MYLITE_SQL_PARSE_NATIONAL_STRING;
        return true;
    case MYLITE_SQL_TOKEN_HEX_LITERAL:
        *out_parser_token = MYLITE_SQL_PARSE_HEX_LITERAL;
        return true;
    case MYLITE_SQL_TOKEN_BIT_LITERAL:
        *out_parser_token = MYLITE_SQL_PARSE_BIT_LITERAL;
        return true;
    case MYLITE_SQL_TOKEN_INTEGER:
        *out_parser_token = MYLITE_SQL_PARSE_INTEGER;
        return true;
    case MYLITE_SQL_TOKEN_DECIMAL:
        *out_parser_token = MYLITE_SQL_PARSE_DECIMAL;
        return true;
    case MYLITE_SQL_TOKEN_FLOAT:
        *out_parser_token = MYLITE_SQL_PARSE_FLOAT;
        return true;
    case MYLITE_SQL_TOKEN_USER_VARIABLE:
        *out_parser_token = MYLITE_SQL_PARSE_USER_VARIABLE;
        return true;
    case MYLITE_SQL_TOKEN_SYSTEM_VARIABLE:
        *out_parser_token = MYLITE_SQL_PARSE_SYSTEM_VARIABLE;
        return true;
    case MYLITE_SQL_TOKEN_CHARSET_INTRODUCER:
        *out_parser_token = MYLITE_SQL_PARSE_CHARSET_INTRODUCER;
        return true;
    case MYLITE_SQL_TOKEN_TEMPORAL_LITERAL_INTRODUCER:
        *out_parser_token = MYLITE_SQL_PARSE_TEMPORAL_LITERAL_INTRODUCER;
        return true;
    default:
        return false;
    }
}

static bool lexer_token_has_immediate_left_paren(
    const struct mylite_sql_lexer *lexer,
    const struct mylite_sql_token *token
) {
    if (lexer == NULL || lexer->input == NULL || token == NULL || token->offset > lexer->offset ||
        token->length != lexer->offset - token->offset || lexer->offset >= lexer->length) {
        return false;
    }

    return lexer->input[lexer->offset] == '(';
}

static bool map_keyword_token(
    const struct mylite_sql_parser_state *state,
    const struct mylite_sql_lexer *lexer,
    const struct mylite_sql_token *token,
    bool previous_token_was_dot,
    int previous_parser_token,
    int token_before_previous_parser_token,
    int *out_parser_token
) {
    static const struct {
        const char *keyword;
        int parser_token;
    } keyword_mappings[] = {
        {"SELECT", MYLITE_SQL_PARSE_SELECT},
        {"ALL", MYLITE_SQL_PARSE_ALL},
        {"ALGORITHM", MYLITE_SQL_PARSE_ALGORITHM},
        {"ALTER", MYLITE_SQL_PARSE_ALTER},
        {"AS", MYLITE_SQL_PARSE_AS},
        {"CASCADED", MYLITE_SQL_PARSE_CASCADED},
        {"CAST", MYLITE_SQL_PARSE_CAST},
        {"CONVERT", MYLITE_SQL_PARSE_CONVERT},
        {"CONVERT_TZ", MYLITE_SQL_PARSE_CONVERT_TZ},
        {"DEFINER", MYLITE_SQL_PARSE_DEFINER},
        {"ESCAPE", MYLITE_SQL_PARSE_ESCAPE},
        {"EXCEPT", MYLITE_SQL_PARSE_EXCEPT},
        {"AGAINST", MYLITE_SQL_PARSE_AGAINST},
        {"FROM", MYLITE_SQL_PARSE_FROM},
        {"INVOKER", MYLITE_SQL_PARSE_INVOKER},
        {"INTERSECT", MYLITE_SQL_PARSE_INTERSECT},
        {"LANGUAGE", MYLITE_SQL_PARSE_LANGUAGE},
        {"MATCH", MYLITE_SQL_PARSE_MATCH},
        {"MERGE", MYLITE_SQL_PARSE_MERGE},
        {"NATURAL", MYLITE_SQL_PARSE_NATURAL},
        {"OPTION", MYLITE_SQL_PARSE_OPTION},
        {"QUERY", MYLITE_SQL_PARSE_QUERY},
        {"ROLLUP", MYLITE_SQL_PARSE_ROLLUP},
        {"SECURITY", MYLITE_SQL_PARSE_SECURITY},
        {"SQL", MYLITE_SQL_PARSE_SQL},
        {"TEMPTABLE", MYLITE_SQL_PARSE_TEMPTABLE},
        {"UNDEFINED", MYLITE_SQL_PARSE_UNDEFINED},
        {"UNION", MYLITE_SQL_PARSE_UNION},
        {"WHERE", MYLITE_SQL_PARSE_WHERE},
        {"AND", MYLITE_SQL_PARSE_AND},
        {"BETWEEN", MYLITE_SQL_PARSE_BETWEEN},
        {"OR", MYLITE_SQL_PARSE_OR},
        {"XOR", MYLITE_SQL_PARSE_XOR},
        {"GROUP", MYLITE_SQL_PARSE_GROUP},
        {"GROUP_CONCAT", MYLITE_SQL_PARSE_GROUP_CONCAT},
        {"GROUPING", MYLITE_SQL_PARSE_GROUPING},
        {"ANY", MYLITE_SQL_PARSE_ANY},
        {"ANY_VALUE", MYLITE_SQL_PARSE_ANY_VALUE},
        {"HAVING", MYLITE_SQL_PARSE_HAVING},
        {"ORDER", MYLITE_SQL_PARSE_ORDER},
        {"BY", MYLITE_SQL_PARSE_BY},
        {"BINLOG", MYLITE_SQL_PARSE_BINLOG},
        {"BINARY", MYLITE_SQL_PARSE_BINARY},
        {"USING", MYLITE_SQL_PARSE_USING},
        {"BIT", MYLITE_SQL_PARSE_BIT},
        {"BIN", MYLITE_SQL_PARSE_BIN},
        {"BIT_LENGTH", MYLITE_SQL_PARSE_BIT_LENGTH},
        {"OCT", MYLITE_SQL_PARSE_OCT},
        {"OCTET_LENGTH", MYLITE_SQL_PARSE_OCTET_LENGTH},
        {"ORD", MYLITE_SQL_PARSE_ORD},
        {"ABS", MYLITE_SQL_PARSE_ABS},
        {"ACOS", MYLITE_SQL_PARSE_ACOS},
        {"ASCII", MYLITE_SQL_PARSE_ASCII},
        {"ASIN", MYLITE_SQL_PARSE_ASIN},
        {"ATAN", MYLITE_SQL_PARSE_ATAN},
        {"ATAN2", MYLITE_SQL_PARSE_ATAN2},
        {"COS", MYLITE_SQL_PARSE_COS},
        {"COT", MYLITE_SQL_PARSE_COT},
        {"EXP", MYLITE_SQL_PARSE_EXP},
        {"LN", MYLITE_SQL_PARSE_LN},
        {"LOG", MYLITE_SQL_PARSE_LOG},
        {"LOGS", MYLITE_SQL_PARSE_LOGS},
        {"LOG10", MYLITE_SQL_PARSE_LOG10},
        {"LOG2", MYLITE_SQL_PARSE_LOG2},
        {"POW", MYLITE_SQL_PARSE_POW},
        {"POWER", MYLITE_SQL_PARSE_POWER},
        {"SIGN", MYLITE_SQL_PARSE_SIGN},
        {"CEIL", MYLITE_SQL_PARSE_CEIL},
        {"CEILING", MYLITE_SQL_PARSE_CEILING},
        {"FLOOR", MYLITE_SQL_PARSE_FLOOR},
        {"ROUND", MYLITE_SQL_PARSE_ROUND},
        {"PI", MYLITE_SQL_PARSE_PI},
        {"RAND", MYLITE_SQL_PARSE_RAND},
        {"REPLICA", MYLITE_SQL_PARSE_REPLICA},
        {"REPLICAS", MYLITE_SQL_PARSE_REPLICAS},
        {"RELAYLOG", MYLITE_SQL_PARSE_RELAYLOG},
        {"SIN", MYLITE_SQL_PARSE_SIN},
        {"SQRT", MYLITE_SQL_PARSE_SQRT},
        {"TAN", MYLITE_SQL_PARSE_TAN},
        {"DEGREES", MYLITE_SQL_PARSE_DEGREES},
        {"RADIANS", MYLITE_SQL_PARSE_RADIANS},
        {"CONNECTION_ID", MYLITE_SQL_PARSE_CONNECTION_ID},
        {"COUNT", MYLITE_SQL_PARSE_COUNT},
        {"COMPRESS", MYLITE_SQL_PARSE_COMPRESS},
        {"CRC32", MYLITE_SQL_PARSE_CRC32},
        {"FROM_BASE64", MYLITE_SQL_PARSE_FROM_BASE64},
        {"HEX", MYLITE_SQL_PARSE_HEX},
        {"MD5", MYLITE_SQL_PARSE_MD5},
        {"RANDOM_BYTES", MYLITE_SQL_PARSE_RANDOM_BYTES},
        {"SHA", MYLITE_SQL_PARSE_SHA},
        {"SHA1", MYLITE_SQL_PARSE_SHA1},
        {"SHA2", MYLITE_SQL_PARSE_SHA2},
        {"TO_BASE64", MYLITE_SQL_PARSE_TO_BASE64},
        {"UNCOMPRESS", MYLITE_SQL_PARSE_UNCOMPRESS},
        {"UNCOMPRESSED_LENGTH", MYLITE_SQL_PARSE_UNCOMPRESSED_LENGTH},
        {"UNHEX", MYLITE_SQL_PARSE_UNHEX},
        {"IS_UUID", MYLITE_SQL_PARSE_IS_UUID},
        {"UUID", MYLITE_SQL_PARSE_UUID},
        {"UUID_TO_BIN", MYLITE_SQL_PARSE_UUID_TO_BIN},
        {"BIN_TO_UUID", MYLITE_SQL_PARSE_BIN_TO_UUID},
        {"AVG", MYLITE_SQL_PARSE_AVG},
        {"BIT_AND", MYLITE_SQL_PARSE_BIT_AND},
        {"BIT_COUNT", MYLITE_SQL_PARSE_BIT_COUNT},
        {"BIT_OR", MYLITE_SQL_PARSE_BIT_OR},
        {"BIT_XOR", MYLITE_SQL_PARSE_BIT_XOR},
        {"BOTH", MYLITE_SQL_PARSE_BOTH},
        {"CROSS", MYLITE_SQL_PARSE_CROSS},
        {"DISTINCT", MYLITE_SQL_PARSE_DISTINCT},
        {"DISTINCTROW", MYLITE_SQL_PARSE_DISTINCTROW},
        {"CURDATE", MYLITE_SQL_PARSE_CURDATE},
        {"CURRENT_DATE", MYLITE_SQL_PARSE_CURRENT_DATE},
        {"CURRENT_ROLE", MYLITE_SQL_PARSE_CURRENT_ROLE},
        {"CURRENT_TIME", MYLITE_SQL_PARSE_CURRENT_TIME},
        {"CURRENT_TIMESTAMP", MYLITE_SQL_PARSE_CURRENT_TIMESTAMP},
        {"CURRENT_USER", MYLITE_SQL_PARSE_CURRENT_USER},
        {"CURTIME", MYLITE_SQL_PARSE_CURTIME},
        {"UTC_DATE", MYLITE_SQL_PARSE_UTC_DATE},
        {"UTC_TIME", MYLITE_SQL_PARSE_UTC_TIME},
        {"UTC_TIMESTAMP", MYLITE_SQL_PARSE_UTC_TIMESTAMP},
        {"SYSDATE", MYLITE_SQL_PARSE_SYSDATE},
        {"ASC", MYLITE_SQL_PARSE_ASC},
        {"DESC", MYLITE_SQL_PARSE_DESC},
        {"AUTO_INCREMENT", MYLITE_SQL_PARSE_AUTO_INCREMENT},
        {"LAST_INSERT_ID", MYLITE_SQL_PARSE_LAST_INSERT_ID},
        {"LCASE", MYLITE_SQL_PARSE_LCASE},
        {"LEADING", MYLITE_SQL_PARSE_LEADING},
        {"LENGTH", MYLITE_SQL_PARSE_LENGTH},
        {"LOCATE", MYLITE_SQL_PARSE_LOCATE},
        {"LPAD", MYLITE_SQL_PARSE_LPAD},
        {"MID", MYLITE_SQL_PARSE_MID},
        {"MINUTE", MYLITE_SQL_PARSE_MINUTE},
        {"MONTH", MYLITE_SQL_PARSE_MONTH},
        {"RIGHT", MYLITE_SQL_PARSE_RIGHT},
        {"REPEAT", MYLITE_SQL_PARSE_REPEAT},
        {"REVERSE", MYLITE_SQL_PARSE_REVERSE},
        {"QUOTE", MYLITE_SQL_PARSE_QUOTE},
        {"SOUNDEX", MYLITE_SQL_PARSE_SOUNDEX},
        {"SOUNDS", MYLITE_SQL_PARSE_SOUNDS},
        {"RPAD", MYLITE_SQL_PARSE_RPAD},
        {"INSTR", MYLITE_SQL_PARSE_INSTR},
        {"LOWER", MYLITE_SQL_PARSE_LOWER},
        {"LTRIM", MYLITE_SQL_PARSE_LTRIM},
        {"MAX", MYLITE_SQL_PARSE_MAX},
        {"MIN", MYLITE_SQL_PARSE_MIN},
        {"SUM", MYLITE_SQL_PARSE_SUM},
        {"LIMIT", MYLITE_SQL_PARSE_LIMIT},
        {"OFFSET", MYLITE_SQL_PARSE_OFFSET},
        {"SPACE", MYLITE_SQL_PARSE_SPACE},
        {"USE", MYLITE_SQL_PARSE_USE},
        {"CALL", MYLITE_SQL_PARSE_CALL},
        {"CREATE", MYLITE_SQL_PARSE_CREATE},
        {"TABLE", MYLITE_SQL_PARSE_TABLE},
        {"VIEW", MYLITE_SQL_PARSE_VIEW},
        {"TEMPORARY", MYLITE_SQL_PARSE_TEMPORARY},
        {"GENERATED", MYLITE_SQL_PARSE_GENERATED},
        {"ALWAYS", MYLITE_SQL_PARSE_ALWAYS},
        {"ARRAY", MYLITE_SQL_PARSE_ARRAY},
        {"VIRTUAL", MYLITE_SQL_PARSE_VIRTUAL},
        {"STORED", MYLITE_SQL_PARSE_STORED},
        {"IF", MYLITE_SQL_PARSE_IF},
        {"IFNULL", MYLITE_SQL_PARSE_IFNULL},
        {"COALESCE", MYLITE_SQL_PARSE_COALESCE},
        {"COERCIBILITY", MYLITE_SQL_PARSE_COERCIBILITY},
        {"CONCAT", MYLITE_SQL_PARSE_CONCAT},
        {"CONCAT_WS", MYLITE_SQL_PARSE_CONCAT_WS},
        {"CONV", MYLITE_SQL_PARSE_CONV},
        {"POSITION", MYLITE_SQL_PARSE_POSITION},
        {"NULLIF", MYLITE_SQL_PARSE_NULLIF},
        {"ISNULL", MYLITE_SQL_PARSE_ISNULL},
        {"CASE", MYLITE_SQL_PARSE_CASE},
        {"WHEN", MYLITE_SQL_PARSE_WHEN},
        {"THEN", MYLITE_SQL_PARSE_THEN},
        {"ELSE", MYLITE_SQL_PARSE_ELSE},
        {"END", MYLITE_SQL_PARSE_END},
        {"CHAIN", MYLITE_SQL_PARSE_CHAIN},
        {"MOD", MYLITE_SQL_PARSE_MOD},
        {"DIV", MYLITE_SQL_PARSE_DIV},
        {"IGNORE", MYLITE_SQL_PARSE_IGNORE},
        {"EXISTS", MYLITE_SQL_PARSE_EXISTS},
        {"DATABASE", MYLITE_SQL_PARSE_DATABASE},
        {"DATABASES", MYLITE_SQL_PARSE_DATABASES},
        {"DATA", MYLITE_SQL_PARSE_DATA},
        {"DAY", MYLITE_SQL_PARSE_DAY},
        {"DAYNAME", MYLITE_SQL_PARSE_DAYNAME},
        {"DAY_HOUR", MYLITE_SQL_PARSE_DAY_HOUR},
        {"DAY_MICROSECOND", MYLITE_SQL_PARSE_DAY_MICROSECOND},
        {"DAY_MINUTE", MYLITE_SQL_PARSE_DAY_MINUTE},
        {"DAYOFMONTH", MYLITE_SQL_PARSE_DAYOFMONTH},
        {"DAYOFWEEK", MYLITE_SQL_PARSE_DAYOFWEEK},
        {"DAYOFYEAR", MYLITE_SQL_PARSE_DAYOFYEAR},
        {"DAY_SECOND", MYLITE_SQL_PARSE_DAY_SECOND},
        {"ADDDATE", MYLITE_SQL_PARSE_ADDDATE},
        {"ADDTIME", MYLITE_SQL_PARSE_ADDTIME},
        {"DATEDIFF", MYLITE_SQL_PARSE_DATEDIFF},
        {"DATE_ADD", MYLITE_SQL_PARSE_DATE_ADD},
        {"DATE_SUB", MYLITE_SQL_PARSE_DATE_SUB},
        {"DATE_FORMAT", MYLITE_SQL_PARSE_DATE_FORMAT},
        {"GET_FORMAT", MYLITE_SQL_PARSE_GET_FORMAT},
        {"EXTRACT", MYLITE_SQL_PARSE_EXTRACT},
        {"FROM_DAYS", MYLITE_SQL_PARSE_FROM_DAYS},
        {"FROM_UNIXTIME", MYLITE_SQL_PARSE_FROM_UNIXTIME},
        {"HOUR_MICROSECOND", MYLITE_SQL_PARSE_HOUR_MICROSECOND},
        {"HOUR_MINUTE", MYLITE_SQL_PARSE_HOUR_MINUTE},
        {"HOUR_SECOND", MYLITE_SQL_PARSE_HOUR_SECOND},
        {"MICROSECOND", MYLITE_SQL_PARSE_MICROSECOND},
        {"MINUTE_MICROSECOND", MYLITE_SQL_PARSE_MINUTE_MICROSECOND},
        {"MINUTE_SECOND", MYLITE_SQL_PARSE_MINUTE_SECOND},
        {"MAKEDATE", MYLITE_SQL_PARSE_MAKEDATE},
        {"MAKETIME", MYLITE_SQL_PARSE_MAKETIME},
        {"MONTHNAME", MYLITE_SQL_PARSE_MONTHNAME},
        {"PERIOD_ADD", MYLITE_SQL_PARSE_PERIOD_ADD},
        {"PERIOD_DIFF", MYLITE_SQL_PARSE_PERIOD_DIFF},
        {"QUARTER", MYLITE_SQL_PARSE_QUARTER},
        {"SECOND_MICROSECOND", MYLITE_SQL_PARSE_SECOND_MICROSECOND},
        {"SQL_TSI_DAY", MYLITE_SQL_PARSE_SQL_TSI_DAY},
        {"SQL_TSI_HOUR", MYLITE_SQL_PARSE_SQL_TSI_HOUR},
        {"SQL_TSI_MINUTE", MYLITE_SQL_PARSE_SQL_TSI_MINUTE},
        {"SQL_TSI_MONTH", MYLITE_SQL_PARSE_SQL_TSI_MONTH},
        {"SQL_TSI_QUARTER", MYLITE_SQL_PARSE_SQL_TSI_QUARTER},
        {"SQL_TSI_SECOND", MYLITE_SQL_PARSE_SQL_TSI_SECOND},
        {"SQL_TSI_WEEK", MYLITE_SQL_PARSE_SQL_TSI_WEEK},
        {"SQL_TSI_YEAR", MYLITE_SQL_PARSE_SQL_TSI_YEAR},
        {"TIMEDIFF", MYLITE_SQL_PARSE_TIMEDIFF},
        {"TIMESTAMPADD", MYLITE_SQL_PARSE_TIMESTAMPADD},
        {"TIMESTAMPDIFF", MYLITE_SQL_PARSE_TIMESTAMPDIFF},
        {"UNIX_TIMESTAMP", MYLITE_SQL_PARSE_UNIX_TIMESTAMP},
        {"TIME_FORMAT", MYLITE_SQL_PARSE_TIME_FORMAT},
        {"TIME_TO_SEC", MYLITE_SQL_PARSE_TIME_TO_SEC},
        {"TO_DAYS", MYLITE_SQL_PARSE_TO_DAYS},
        {"TO_SECONDS", MYLITE_SQL_PARSE_TO_SECONDS},
        {"SEC_TO_TIME", MYLITE_SQL_PARSE_SEC_TO_TIME},
        {"STR_TO_DATE", MYLITE_SQL_PARSE_STR_TO_DATE},
        {"REGEXP_INSTR", MYLITE_SQL_PARSE_REGEXP_INSTR},
        {"REGEXP_LIKE", MYLITE_SQL_PARSE_REGEXP_LIKE},
        {"REGEXP_REPLACE", MYLITE_SQL_PARSE_REGEXP_REPLACE},
        {"REGEXP_SUBSTR", MYLITE_SQL_PARSE_REGEXP_SUBSTR},
        {"LAST_DAY", MYLITE_SQL_PARSE_LAST_DAY},
        {"WEEK", MYLITE_SQL_PARSE_WEEK},
        {"WEEKDAY", MYLITE_SQL_PARSE_WEEKDAY},
        {"WEEKOFYEAR", MYLITE_SQL_PARSE_WEEKOFYEAR},
        {"YEAR_MONTH", MYLITE_SQL_PARSE_YEAR_MONTH},
        {"YEARWEEK", MYLITE_SQL_PARSE_YEARWEEK},
        {"DROP", MYLITE_SQL_PARSE_DROP},
        {"TRUNCATE", MYLITE_SQL_PARSE_TRUNCATE},
        {"SUBSTR", MYLITE_SQL_PARSE_SUBSTR},
        {"SUBSTRING", MYLITE_SQL_PARSE_SUBSTRING},
        {"SUBSTRING_INDEX", MYLITE_SQL_PARSE_SUBSTRING_INDEX},
        {"STRCMP", MYLITE_SQL_PARSE_STRCMP},
        {"RTRIM", MYLITE_SQL_PARSE_RTRIM},
        {"TRAILING", MYLITE_SQL_PARSE_TRAILING},
        {"TRIM", MYLITE_SQL_PARSE_TRIM},
        {"SUBDATE", MYLITE_SQL_PARSE_SUBDATE},
        {"SUBTIME", MYLITE_SQL_PARSE_SUBTIME},
        {"UCASE", MYLITE_SQL_PARSE_UCASE},
        {"UPPER", MYLITE_SQL_PARSE_UPPER},
        {"SHOW", MYLITE_SQL_PARSE_SHOW},
        {"TABLES", MYLITE_SQL_PARSE_TABLES},
        {"COLUMNS", MYLITE_SQL_PARSE_COLUMNS},
        {"ELT", MYLITE_SQL_PARSE_ELT},
        {"EXPORT_SET", MYLITE_SQL_PARSE_EXPORT_SET},
        {"FIELD", MYLITE_SQL_PARSE_FIELD},
        {"FIELDS", MYLITE_SQL_PARSE_FIELDS},
        {"FIND_IN_SET", MYLITE_SQL_PARSE_FIND_IN_SET},
        {"FORMAT", MYLITE_SQL_PARSE_FORMAT},
        {"GREATEST", MYLITE_SQL_PARSE_GREATEST},
        {"INDEX", MYLITE_SQL_PARSE_INDEX},
        {"INDEXES", MYLITE_SQL_PARSE_INDEXES},
        {"LEAST", MYLITE_SQL_PARSE_LEAST},
        {"MAKE_SET", MYLITE_SQL_PARSE_MAKE_SET},
        {"CONSTRAINT", MYLITE_SQL_PARSE_CONSTRAINT},
        {"FOREIGN", MYLITE_SQL_PARSE_FOREIGN},
        {"CHANNEL", MYLITE_SQL_PARSE_CHANNEL},
        {"KEY", MYLITE_SQL_PARSE_KEY},
        {"KEYS", MYLITE_SQL_PARSE_KEYS},
        {"REFERENCES", MYLITE_SQL_PARSE_REFERENCES},
        {"ACTION", MYLITE_SQL_PARSE_ACTION},
        {"CASCADE", MYLITE_SQL_PARSE_CASCADE},
        {"ENFORCED", MYLITE_SQL_PARSE_ENFORCED},
        {"PRIMARY", MYLITE_SQL_PARSE_PRIMARY},
        {"RESTRICT", MYLITE_SQL_PARSE_RESTRICT},
        {"UNIQUE", MYLITE_SQL_PARSE_UNIQUE},
        {"FULLTEXT", MYLITE_SQL_PARSE_FULLTEXT},
        {"SPATIAL", MYLITE_SQL_PARSE_SPATIAL},
        {"GEOMETRY", MYLITE_SQL_PARSE_GEOMETRY},
        {"GEOMCOLLECTION", MYLITE_SQL_PARSE_GEOMCOLLECTION},
        {"GEOMETRYCOLLECTION", MYLITE_SQL_PARSE_GEOMETRYCOLLECTION},
        {"LINESTRING", MYLITE_SQL_PARSE_LINESTRING},
        {"MULTILINESTRING", MYLITE_SQL_PARSE_MULTILINESTRING},
        {"MULTIPOINT", MYLITE_SQL_PARSE_MULTIPOINT},
        {"MULTIPOLYGON", MYLITE_SQL_PARSE_MULTIPOLYGON},
        {"POINT", MYLITE_SQL_PARSE_POINT},
        {"POLYGON", MYLITE_SQL_PARSE_POLYGON},
        {"FULL", MYLITE_SQL_PARSE_FULL},
        {"TRIGGER", MYLITE_SQL_PARSE_TRIGGER},
        {"TRIGGERS", MYLITE_SQL_PARSE_TRIGGERS},
        {"EVENT", MYLITE_SQL_PARSE_EVENT},
        {"EVENTS", MYLITE_SQL_PARSE_EVENTS},
        {"OPEN", MYLITE_SQL_PARSE_OPEN},
        {"PROCESSLIST", MYLITE_SQL_PARSE_PROCESSLIST},
        {"GRANTS", MYLITE_SQL_PARSE_GRANTS},
        {"WARNINGS", MYLITE_SQL_PARSE_WARNINGS},
        {"ERRORS", MYLITE_SQL_PARSE_ERRORS},
        {"PROCEDURE", MYLITE_SQL_PARSE_PROCEDURE},
        {"FUNCTION", MYLITE_SQL_PARSE_FUNCTION},
        {"ENGINE", MYLITE_SQL_PARSE_ENGINE},
        {"ENGINES", MYLITE_SQL_PARSE_ENGINES},
        {"PLUGINS", MYLITE_SQL_PARSE_PLUGINS},
        {"PRIVILEGES", MYLITE_SQL_PARSE_PRIVILEGES},
        {"ENUM", MYLITE_SQL_PARSE_ENUM},
        {"COMMENT", MYLITE_SQL_PARSE_COMMENT},
        {"STATUS", MYLITE_SQL_PARSE_STATUS},
        {"DISK", MYLITE_SQL_PARSE_DISK},
        {"STORAGE", MYLITE_SQL_PARSE_STORAGE},
        {"TABLESPACE", MYLITE_SQL_PARSE_TABLESPACE},
        {"INSERT_METHOD", MYLITE_SQL_PARSE_INSERT_METHOD},
        {"VARIABLES", MYLITE_SQL_PARSE_VARIABLES},
        {"DEFAULT", MYLITE_SQL_PARSE_DEFAULT},
        {"CHAR", MYLITE_SQL_PARSE_CHAR},
        {"CHARACTER", MYLITE_SQL_PARSE_CHARACTER},
        {"CHARACTER_LENGTH", MYLITE_SQL_PARSE_CHARACTER_LENGTH},
        {"CHAR_LENGTH", MYLITE_SQL_PARSE_CHAR_LENGTH},
        {"CHARSET", MYLITE_SQL_PARSE_CHARSET},
        {"COLLATE", MYLITE_SQL_PARSE_COLLATE},
        {"COLLATION", MYLITE_SQL_PARSE_COLLATION},
        {"LIKE", MYLITE_SQL_PARSE_LIKE},
        {"REGEXP", MYLITE_SQL_PARSE_REGEXP},
        {"RLIKE", MYLITE_SQL_PARSE_RLIKE},
        {"SCHEMA", MYLITE_SQL_PARSE_SCHEMA},
        {"SCHEMAS", MYLITE_SQL_PARSE_SCHEMAS},
        {"DESCRIBE", MYLITE_SQL_PARSE_DESCRIBE},
        {"EXPLAIN", MYLITE_SQL_PARSE_EXPLAIN},
        {"SESSION_USER", MYLITE_SQL_PARSE_SESSION_USER},
        {"RENAME", MYLITE_SQL_PARSE_RENAME},
        {"ADD", MYLITE_SQL_PARSE_ADD},
        {"AFTER", MYLITE_SQL_PARSE_AFTER},
        {"MODIFY", MYLITE_SQL_PARSE_MODIFY},
        {"CHANGE", MYLITE_SQL_PARSE_CHANGE},
        {"COLUMN", MYLITE_SQL_PARSE_COLUMN},
        {"FIRST", MYLITE_SQL_PARSE_FIRST},
        {"FOR", MYLITE_SQL_PARSE_FOR},
        {"FORCE", MYLITE_SQL_PARSE_FORCE},
        {"INSERT", MYLITE_SQL_PARSE_INSERT},
        {"INFILE", MYLITE_SQL_PARSE_INFILE},
        {"INNER", MYLITE_SQL_PARSE_INNER},
        {"JOIN", MYLITE_SQL_PARSE_JOIN},
        {"LEFT", MYLITE_SQL_PARSE_LEFT},
        {"OUTER", MYLITE_SQL_PARSE_OUTER},
        {"REPLACE", MYLITE_SQL_PARSE_REPLACE},
        {"LOW_PRIORITY", MYLITE_SQL_PARSE_LOW_PRIORITY},
        {"HIGH_PRIORITY", MYLITE_SQL_PARSE_HIGH_PRIORITY},
        {"DELAYED", MYLITE_SQL_PARSE_DELAYED},
        {"INTO", MYLITE_SQL_PARSE_INTO},
        {"LOCK", MYLITE_SQL_PARSE_LOCK},
        {"LOCKED", MYLITE_SQL_PARSE_LOCKED},
        {"LOAD", MYLITE_SQL_PARSE_LOAD},
        {"LAST", MYLITE_SQL_PARSE_LAST},
        {"MEMBER", MYLITE_SQL_PARSE_MEMBER},
        {"MEMORY", MYLITE_SQL_PARSE_MEMORY},
        {"MODE", MYLITE_SQL_PARSE_MODE},
        {"NOWAIT", MYLITE_SQL_PARSE_NOWAIT},
        {"READ", MYLITE_SQL_PARSE_READ},
        {"COMMITTED", MYLITE_SQL_PARSE_COMMITTED},
        {"ISOLATION", MYLITE_SQL_PARSE_ISOLATION},
        {"LEVEL", MYLITE_SQL_PARSE_LEVEL},
        {"ONLY", MYLITE_SQL_PARSE_ONLY},
        {"REPEATABLE", MYLITE_SQL_PARSE_REPEATABLE},
        {"SERIALIZABLE", MYLITE_SQL_PARSE_SERIALIZABLE},
        {"UNCOMMITTED", MYLITE_SQL_PARSE_UNCOMMITTED},
        {"ROW", MYLITE_SQL_PARSE_ROW},
        {"VALUE", MYLITE_SQL_PARSE_VALUE},
        {"VALUES", MYLITE_SQL_PARSE_VALUES},
        {"DUPLICATE", MYLITE_SQL_PARSE_DUPLICATE},
        {"TO", MYLITE_SQL_PARSE_TO},
        {"DELETE", MYLITE_SQL_PARSE_DELETE},
        {"DEALLOCATE", MYLITE_SQL_PARSE_DEALLOCATE},
        {"DO", MYLITE_SQL_PARSE_DO},
        {"EXECUTE", MYLITE_SQL_PARSE_EXECUTE},
        {"PREPARE", MYLITE_SQL_PARSE_PREPARE},
        {"UPDATE", MYLITE_SQL_PARSE_UPDATE},
        {"START", MYLITE_SQL_PARSE_START},
        {"TRANSACTION", MYLITE_SQL_PARSE_TRANSACTION},
        {"WITH", MYLITE_SQL_PARSE_WITH},
        {"CONSISTENT", MYLITE_SQL_PARSE_CONSISTENT},
        {"SNAPSHOT", MYLITE_SQL_PARSE_SNAPSHOT},
        {"BEGIN", MYLITE_SQL_PARSE_BEGIN},
        {"WORK", MYLITE_SQL_PARSE_WORK},
        {"COMMIT", MYLITE_SQL_PARSE_COMMIT},
        {"ROLLBACK", MYLITE_SQL_PARSE_ROLLBACK},
        {"SAVEPOINT", MYLITE_SQL_PARSE_SAVEPOINT},
        {"RELEASE", MYLITE_SQL_PARSE_RELEASE},
        {"FLUSH", MYLITE_SQL_PARSE_FLUSH},
        {"UNLOCK", MYLITE_SQL_PARSE_UNLOCK},
        {"WRITE", MYLITE_SQL_PARSE_WRITE},
        {"ANALYZE", MYLITE_SQL_PARSE_ANALYZE},
        {"CHECK", MYLITE_SQL_PARSE_CHECK},
        {"OPTIMIZE", MYLITE_SQL_PARSE_OPTIMIZE},
        {"REPAIR", MYLITE_SQL_PARSE_REPAIR},
        {"NO_WRITE_TO_BINLOG", MYLITE_SQL_PARSE_NO_WRITE_TO_BINLOG},
        {"QUICK", MYLITE_SQL_PARSE_QUICK},
        {"FAST", MYLITE_SQL_PARSE_FAST},
        {"MEDIUM", MYLITE_SQL_PARSE_MEDIUM},
        {"EXTENDED", MYLITE_SQL_PARSE_EXTENDED},
        {"CHANGED", MYLITE_SQL_PARSE_CHANGED},
        {"UPGRADE", MYLITE_SQL_PARSE_UPGRADE},
        {"USE_FRM", MYLITE_SQL_PARSE_USE_FRM},
        {"SET", MYLITE_SQL_PARSE_SET},
        {"SESSION", MYLITE_SQL_PARSE_SESSION},
        {"LOCAL", MYLITE_SQL_PARSE_LOCAL},
        {"LINES", MYLITE_SQL_PARSE_LINES},
        {"LOCALTIME", MYLITE_SQL_PARSE_LOCALTIME},
        {"LOCALTIMESTAMP", MYLITE_SQL_PARSE_LOCALTIMESTAMP},
        {"GLOBAL", MYLITE_SQL_PARSE_GLOBAL},
        {"SYSTEM", MYLITE_SQL_PARSE_SYSTEM},
        {"ON", MYLITE_SQL_PARSE_ON},
        {"OVER", MYLITE_SQL_PARSE_OVER},
        {"WINDOW", MYLITE_SQL_PARSE_WINDOW},
        {"NULLS", MYLITE_SQL_PARSE_NULLS},
        {"RESPECT", MYLITE_SQL_PARSE_RESPECT},
        {"ROWS", MYLITE_SQL_PARSE_ROWS},
        {"RANGE", MYLITE_SQL_PARSE_RANGE},
        {"UNBOUNDED", MYLITE_SQL_PARSE_UNBOUNDED},
        {"PRECEDING", MYLITE_SQL_PARSE_PRECEDING},
        {"FOLLOWING", MYLITE_SQL_PARSE_FOLLOWING},
        {"CURRENT", MYLITE_SQL_PARSE_CURRENT},
        {"NO", MYLITE_SQL_PARSE_NO},
        {"OFF", MYLITE_SQL_PARSE_OFF},
        {"NAMES", MYLITE_SQL_PARSE_NAMES},
        {"NATIONAL", MYLITE_SQL_PARSE_NATIONAL},
        {"NCHAR", MYLITE_SQL_PARSE_NCHAR},
        {"INT", MYLITE_SQL_PARSE_INT},
        {"TINYINT", MYLITE_SQL_PARSE_TINYINT},
        {"SMALLINT", MYLITE_SQL_PARSE_SMALLINT},
        {"MEDIUMINT", MYLITE_SQL_PARSE_MEDIUMINT},
        {"INTEGER", MYLITE_SQL_PARSE_INTEGER_TYPE},
        {"BIGINT", MYLITE_SQL_PARSE_BIGINT},
        {"DECIMAL", MYLITE_SQL_PARSE_DECIMAL_TYPE},
        {"DEC", MYLITE_SQL_PARSE_DEC},
        {"NUMERIC", MYLITE_SQL_PARSE_NUMERIC},
        {"FIXED", MYLITE_SQL_PARSE_FIXED},
        {"ROW_FORMAT", MYLITE_SQL_PARSE_ROW_FORMAT},
        {"PARTITION", MYLITE_SQL_PARSE_PARTITION},
        {"KEY_BLOCK_SIZE", MYLITE_SQL_PARSE_KEY_BLOCK_SIZE},
        {"PACK_KEYS", MYLITE_SQL_PARSE_PACK_KEYS},
        {"DISABLE", MYLITE_SQL_PARSE_DISABLE},
        {"ENABLE", MYLITE_SQL_PARSE_ENABLE},
        {"CHECKSUM", MYLITE_SQL_PARSE_CHECKSUM},
        {"STATS_PERSISTENT", MYLITE_SQL_PARSE_STATS_PERSISTENT},
        {"STATS_AUTO_RECALC", MYLITE_SQL_PARSE_STATS_AUTO_RECALC},
        {"STATS_SAMPLE_PAGES", MYLITE_SQL_PARSE_STATS_SAMPLE_PAGES},
        {"MIN_ROWS", MYLITE_SQL_PARSE_MIN_ROWS},
        {"MAX_ROWS", MYLITE_SQL_PARSE_MAX_ROWS},
        {"AVG_ROW_LENGTH", MYLITE_SQL_PARSE_AVG_ROW_LENGTH},
        {"DELAY_KEY_WRITE", MYLITE_SQL_PARSE_DELAY_KEY_WRITE},
        {"DYNAMIC", MYLITE_SQL_PARSE_DYNAMIC},
        {"COMPACT", MYLITE_SQL_PARSE_COMPACT},
        {"REDUNDANT", MYLITE_SQL_PARSE_REDUNDANT},
        {"COMPRESSED", MYLITE_SQL_PARSE_COMPRESSED},
        {"FLOAT", MYLITE_SQL_PARSE_FLOAT_TYPE},
        {"FLOAT4", MYLITE_SQL_PARSE_FLOAT4},
        {"FLOAT8", MYLITE_SQL_PARSE_FLOAT8},
        {"DOUBLE", MYLITE_SQL_PARSE_DOUBLE},
        {"EXPANSION", MYLITE_SQL_PARSE_EXPANSION},
        {"PRECISION", MYLITE_SQL_PARSE_PRECISION},
        {"REAL", MYLITE_SQL_PARSE_REAL},
        {"DATE", MYLITE_SQL_PARSE_DATE},
        {"DATETIME", MYLITE_SQL_PARSE_DATETIME},
        {"HOUR", MYLITE_SQL_PARSE_HOUR},
        {"INTERVAL", MYLITE_SQL_PARSE_INTERVAL},
        {"SECOND", MYLITE_SQL_PARSE_SECOND},
        {"TIME", MYLITE_SQL_PARSE_TIME},
        {"TIMESTAMP", MYLITE_SQL_PARSE_TIMESTAMP},
        {"YEAR", MYLITE_SQL_PARSE_YEAR},
        {"VARCHAR", MYLITE_SQL_PARSE_VARCHAR},
        {"NVARCHAR", MYLITE_SQL_PARSE_NVARCHAR},
        {"VARBINARY", MYLITE_SQL_PARSE_VARBINARY},
        {"BYTE", MYLITE_SQL_PARSE_BYTE},
        {"TINYBLOB", MYLITE_SQL_PARSE_TINYBLOB},
        {"BLOB", MYLITE_SQL_PARSE_BLOB},
        {"MEDIUMBLOB", MYLITE_SQL_PARSE_MEDIUMBLOB},
        {"LONGBLOB", MYLITE_SQL_PARSE_LONGBLOB},
        {"LONG", MYLITE_SQL_PARSE_LONG},
        {"VARYING", MYLITE_SQL_PARSE_VARYING},
        {"TINYTEXT", MYLITE_SQL_PARSE_TINYTEXT},
        {"TEXT", MYLITE_SQL_PARSE_TEXT},
        {"MEDIUMTEXT", MYLITE_SQL_PARSE_MEDIUMTEXT},
        {"LONGTEXT", MYLITE_SQL_PARSE_LONGTEXT},
        {"JSON", MYLITE_SQL_PARSE_JSON},
        {"JSON_ARRAY", MYLITE_SQL_PARSE_JSON_ARRAY},
        {"JSON_ARRAY_APPEND", MYLITE_SQL_PARSE_JSON_ARRAY_APPEND},
        {"JSON_ARRAY_INSERT", MYLITE_SQL_PARSE_JSON_ARRAY_INSERT},
        {"JSON_CONTAINS", MYLITE_SQL_PARSE_JSON_CONTAINS},
        {"JSON_CONTAINS_PATH", MYLITE_SQL_PARSE_JSON_CONTAINS_PATH},
        {"JSON_DEPTH", MYLITE_SQL_PARSE_JSON_DEPTH},
        {"JSON_EXTRACT", MYLITE_SQL_PARSE_JSON_EXTRACT},
        {"JSON_INSERT", MYLITE_SQL_PARSE_JSON_INSERT},
        {"JSON_KEYS", MYLITE_SQL_PARSE_JSON_KEYS},
        {"JSON_LENGTH", MYLITE_SQL_PARSE_JSON_LENGTH},
        {"JSON_MERGE", MYLITE_SQL_PARSE_JSON_MERGE},
        {"JSON_MERGE_PATCH", MYLITE_SQL_PARSE_JSON_MERGE_PATCH},
        {"JSON_MERGE_PRESERVE", MYLITE_SQL_PARSE_JSON_MERGE_PRESERVE},
        {"JSON_OBJECT", MYLITE_SQL_PARSE_JSON_OBJECT},
        {"JSON_OVERLAPS", MYLITE_SQL_PARSE_JSON_OVERLAPS},
        {"JSON_PRETTY", MYLITE_SQL_PARSE_JSON_PRETTY},
        {"JSON_QUOTE", MYLITE_SQL_PARSE_JSON_QUOTE},
        {"JSON_REMOVE", MYLITE_SQL_PARSE_JSON_REMOVE},
        {"JSON_REPLACE", MYLITE_SQL_PARSE_JSON_REPLACE},
        {"JSON_SEARCH", MYLITE_SQL_PARSE_JSON_SEARCH},
        {"JSON_SET", MYLITE_SQL_PARSE_JSON_SET},
        {"JSON_STORAGE_FREE", MYLITE_SQL_PARSE_JSON_STORAGE_FREE},
        {"JSON_STORAGE_SIZE", MYLITE_SQL_PARSE_JSON_STORAGE_SIZE},
        {"JSON_TYPE", MYLITE_SQL_PARSE_JSON_TYPE},
        {"JSON_UNQUOTE", MYLITE_SQL_PARSE_JSON_UNQUOTE},
        {"JSON_VALUE", MYLITE_SQL_PARSE_JSON_VALUE},
        {"JSON_VALID", MYLITE_SQL_PARSE_JSON_VALID},
        {"BOOL", MYLITE_SQL_PARSE_BOOL},
        {"BOOLEAN", MYLITE_SQL_PARSE_BOOLEAN},
        {"INVISIBLE", MYLITE_SQL_PARSE_INVISIBLE},
        {"VISIBLE", MYLITE_SQL_PARSE_VISIBLE},
        {"SRID", MYLITE_SQL_PARSE_SRID},
        {"INT1", MYLITE_SQL_PARSE_INT1},
        {"INT2", MYLITE_SQL_PARSE_INT2},
        {"INT3", MYLITE_SQL_PARSE_INT3},
        {"INT4", MYLITE_SQL_PARSE_INT4},
        {"INT8", MYLITE_SQL_PARSE_INT8},
        {"SIGNED", MYLITE_SQL_PARSE_SIGNED},
        {"UNSIGNED", MYLITE_SQL_PARSE_UNSIGNED},
        {"NOT", MYLITE_SQL_PARSE_NOT},
        {"NOW", MYLITE_SQL_PARSE_NOW},
        {"IS", MYLITE_SQL_PARSE_IS},
        {"IN", MYLITE_SQL_PARSE_IN},
        {"TRUE", MYLITE_SQL_PARSE_TRUE},
        {"FALSE", MYLITE_SQL_PARSE_FALSE},
        {"FOUND_ROWS", MYLITE_SQL_PARSE_FOUND_ROWS},
        {"UNICODE", MYLITE_SQL_PARSE_UNICODE},
        {"UNKNOWN", MYLITE_SQL_PARSE_UNKNOWN},
        {"NULL", MYLITE_SQL_PARSE_NULL},
        {"DUAL", MYLITE_SQL_PARSE_DUAL},
        {"USER", MYLITE_SQL_PARSE_USER},
        {"UTC", MYLITE_SQL_PARSE_UTC},
        {"VERSION", MYLITE_SQL_PARSE_VERSION},
        {"WEIGHT_STRING", MYLITE_SQL_PARSE_WEIGHT_STRING},
        {"ROW_COUNT", MYLITE_SQL_PARSE_ROW_COUNT},
        {"CUME_DIST", MYLITE_SQL_PARSE_CUME_DIST},
        {"DENSE_RANK", MYLITE_SQL_PARSE_DENSE_RANK},
        {"FIRST_VALUE", MYLITE_SQL_PARSE_FIRST_VALUE},
        {"LAG", MYLITE_SQL_PARSE_LAG},
        {"LAST_VALUE", MYLITE_SQL_PARSE_LAST_VALUE},
        {"LEAD", MYLITE_SQL_PARSE_LEAD},
        {"NTH_VALUE", MYLITE_SQL_PARSE_NTH_VALUE},
        {"NTILE", MYLITE_SQL_PARSE_NTILE},
        {"PERCENT_RANK", MYLITE_SQL_PARSE_PERCENT_RANK},
        {"RANK", MYLITE_SQL_PARSE_RANK},
        {"ROW_NUMBER", MYLITE_SQL_PARSE_ROW_NUMBER},
        {"SEPARATOR", MYLITE_SQL_PARSE_SEPARATOR},
        {"SERIAL", MYLITE_SQL_PARSE_SERIAL},
        {"SHARE", MYLITE_SQL_PARSE_SHARE},
        {"SKIP", MYLITE_SQL_PARSE_SKIP},
        {"SQL_CALC_FOUND_ROWS", MYLITE_SQL_PARSE_SQL_CALC_FOUND_ROWS},
        {"SQL_BIG_RESULT", MYLITE_SQL_PARSE_SQL_BIG_RESULT},
        {"SQL_SMALL_RESULT", MYLITE_SQL_PARSE_SQL_SMALL_RESULT},
        {"STRAIGHT_JOIN", MYLITE_SQL_PARSE_STRAIGHT_JOIN},
        {"SYSTEM_USER", MYLITE_SQL_PARSE_SYSTEM_USER},
    };

    if (previous_token_was_dot) {
        *out_parser_token = MYLITE_SQL_PARSE_IDENTIFIER;
        return true;
    }

    if (!mylite_sql_parser_sql_mode_has(state, MYLITE_SQL_MODE_IGNORE_SPACE) &&
        mylite_sql_parser_token_text_is_count_function_name(token) &&
        !lexer_token_has_immediate_left_paren(lexer, token)) {
        *out_parser_token = MYLITE_SQL_PARSE_IDENTIFIER;
        return true;
    }

    if (previous_token_allows_select_noop_modifier(previous_parser_token)) {
        if (mylite_sql_parser_token_text_equals(token, "SQL_BUFFER_RESULT")) {
            *out_parser_token = MYLITE_SQL_PARSE_SQL_BUFFER_RESULT;
            return true;
        }
        if (mylite_sql_parser_token_text_equals(token, "SQL_NO_CACHE")) {
            *out_parser_token = MYLITE_SQL_PARSE_SQL_NO_CACHE;
            return true;
        }
    }

    if (previous_token_allows_delete_quick_modifier(
            previous_parser_token,
            token_before_previous_parser_token
        ) &&
        mylite_sql_parser_token_text_equals(token, "QUICK")) {
        *out_parser_token = MYLITE_SQL_PARSE_DELETE_QUICK_MODIFIER;
        return true;
    }

    if (token != NULL && map_keyword_index(token->keyword_index, out_parser_token)) {
        return true;
    }

    if (token != NULL && token->keyword_index != (unsigned int)-1) {
        if ((token->keyword_flags & MYLITE_SQL_KEYWORD_RESERVED) != 0U) {
            return false;
        }
        *out_parser_token = MYLITE_SQL_PARSE_IDENTIFIER;
        return true;
    }

    {
        size_t token_length = token == NULL ? 0U : token->length;
        unsigned char token_first =
            token_length == 0U
                ? '\0'
                : (unsigned char)mylite_sql_parser_ascii_upper((unsigned char)token->text[0]);

        for (size_t index = 0U; index < sizeof(keyword_mappings) / sizeof(keyword_mappings[0]);
             ++index) {
            if (!token_text_matches_keyword_mapping(
                    token,
                    keyword_mappings[index].keyword,
                    token_first,
                    token_length
                )) {
                continue;
            }
            *out_parser_token = keyword_mappings[index].parser_token;
            return true;
        }
    }

    if (token == NULL || (token->keyword_flags & MYLITE_SQL_KEYWORD_RESERVED) != 0U) {
        return false;
    }

    *out_parser_token = MYLITE_SQL_PARSE_IDENTIFIER;
    return true;
}

// NOLINTBEGIN(readability-magic-numbers): derived lexer keyword-table indexes.
static bool map_keyword_index(unsigned int keyword_index, int *out_parser_token) {
    /* Keyword indices are positions in mylite_lexer.c's keyword table. */
    switch (keyword_index) {
    case 662U: /* SELECT */
        *out_parser_token = MYLITE_SQL_PARSE_SELECT;
        return true;
    case 261U: /* FROM */
        *out_parser_token = MYLITE_SQL_PARSE_FROM;
        return true;
    case 130U: /* CREATE */
        *out_parser_token = MYLITE_SQL_PARSE_CREATE;
        return true;
    case 319U: /* INSERT */
        *out_parser_token = MYLITE_SQL_PARSE_INSERT;
        return true;
    case 333U: /* INTO */
        *out_parser_token = MYLITE_SQL_PARSE_INTO;
        return true;
    case 857U: /* VALUES */
        *out_parser_token = MYLITE_SQL_PARSE_VALUES;
        return true;
    case 780U: /* TABLE */
        *out_parser_token = MYLITE_SQL_PARSE_TABLE;
        return true;
    case 481U: /* NULL */
        *out_parser_token = MYLITE_SQL_PARSE_NULL;
        return true;
    case 22U: /* AS */
        *out_parser_token = MYLITE_SQL_PARSE_AS;
        return true;
    case 670U: /* SET */
        *out_parser_token = MYLITE_SQL_PARSE_SET;
        return true;
    case 324U: /* INT */
        *out_parser_token = MYLITE_SQL_PARSE_INT;
        return true;
    case 875U: /* WHERE */
        *out_parser_token = MYLITE_SQL_PARSE_WHERE;
        return true;
    case 62U: /* BY */
        *out_parser_token = MYLITE_SQL_PARSE_BY;
        return true;
    case 474U: /* NOT */
        *out_parser_token = MYLITE_SQL_PARSE_NOT;
        return true;
    case 366U: /* KEY */
        *out_parser_token = MYLITE_SQL_PARSE_KEY;
        return true;
    case 167U: /* DEFAULT */
        *out_parser_token = MYLITE_SQL_PARSE_DEFAULT;
        return true;
    case 494U: /* ON */
        *out_parser_token = MYLITE_SQL_PARSE_ON;
        return true;
    case 190U: /* DROP */
        *out_parser_token = MYLITE_SQL_PARSE_DROP;
        return true;
    case 18U: /* AND */
        *out_parser_token = MYLITE_SQL_PARSE_AND;
        return true;
    case 506U: /* ORDER */
        *out_parser_token = MYLITE_SQL_PARSE_ORDER;
        return true;
    default:
        break;
    }

    static const int keyword_parser_tokens[] = {
        [0U] = MYLITE_SQL_PARSE_ABS,                   /* ABS */
        [3U] = MYLITE_SQL_PARSE_ACOS,                  /* ACOS */
        [4U] = MYLITE_SQL_PARSE_ACTION,                /* ACTION */
        [6U] = MYLITE_SQL_PARSE_ADD,                   /* ADD */
        [7U] = MYLITE_SQL_PARSE_ADDDATE,               /* ADDDATE */
        [8U] = MYLITE_SQL_PARSE_ADDTIME,               /* ADDTIME */
        [10U] = MYLITE_SQL_PARSE_AFTER,                /* AFTER */
        [11U] = MYLITE_SQL_PARSE_AGAINST,              /* AGAINST */
        [13U] = MYLITE_SQL_PARSE_ALGORITHM,            /* ALGORITHM */
        [14U] = MYLITE_SQL_PARSE_ALL,                  /* ALL */
        [15U] = MYLITE_SQL_PARSE_ALTER,                /* ALTER */
        [16U] = MYLITE_SQL_PARSE_ALWAYS,               /* ALWAYS */
        [17U] = MYLITE_SQL_PARSE_ANALYZE,              /* ANALYZE */
        [18U] = MYLITE_SQL_PARSE_AND,                  /* AND */
        [19U] = MYLITE_SQL_PARSE_ANY,                  /* ANY */
        [20U] = MYLITE_SQL_PARSE_ANY_VALUE,            /* ANY_VALUE */
        [21U] = MYLITE_SQL_PARSE_ARRAY,                /* ARRAY */
        [22U] = MYLITE_SQL_PARSE_AS,                   /* AS */
        [23U] = MYLITE_SQL_PARSE_ASC,                  /* ASC */
        [24U] = MYLITE_SQL_PARSE_ASCII,                /* ASCII */
        [26U] = MYLITE_SQL_PARSE_ASIN,                 /* ASIN */
        [29U] = MYLITE_SQL_PARSE_ATAN,                 /* ATAN */
        [30U] = MYLITE_SQL_PARSE_ATAN2,                /* ATAN2 */
        [35U] = MYLITE_SQL_PARSE_AUTO_INCREMENT,       /* AUTO_INCREMENT */
        [36U] = MYLITE_SQL_PARSE_AVG,                  /* AVG */
        [37U] = MYLITE_SQL_PARSE_AVG_ROW_LENGTH,       /* AVG_ROW_LENGTH */
        [40U] = MYLITE_SQL_PARSE_BEGIN,                /* BEGIN */
        [42U] = MYLITE_SQL_PARSE_BETWEEN,              /* BETWEEN */
        [43U] = MYLITE_SQL_PARSE_BIGINT,               /* BIGINT */
        [44U] = MYLITE_SQL_PARSE_BIN,                  /* BIN */
        [45U] = MYLITE_SQL_PARSE_BINARY,               /* BINARY */
        [46U] = MYLITE_SQL_PARSE_BINLOG,               /* BINLOG */
        [47U] = MYLITE_SQL_PARSE_BIN_TO_UUID,          /* BIN_TO_UUID */
        [48U] = MYLITE_SQL_PARSE_BIT,                  /* BIT */
        [49U] = MYLITE_SQL_PARSE_BIT_AND,              /* BIT_AND */
        [50U] = MYLITE_SQL_PARSE_BIT_COUNT,            /* BIT_COUNT */
        [51U] = MYLITE_SQL_PARSE_BIT_LENGTH,           /* BIT_LENGTH */
        [52U] = MYLITE_SQL_PARSE_BIT_OR,               /* BIT_OR */
        [53U] = MYLITE_SQL_PARSE_BIT_XOR,              /* BIT_XOR */
        [54U] = MYLITE_SQL_PARSE_BLOB,                 /* BLOB */
        [56U] = MYLITE_SQL_PARSE_BOOL,                 /* BOOL */
        [57U] = MYLITE_SQL_PARSE_BOOLEAN,              /* BOOLEAN */
        [58U] = MYLITE_SQL_PARSE_BOTH,                 /* BOTH */
        [62U] = MYLITE_SQL_PARSE_BY,                   /* BY */
        [63U] = MYLITE_SQL_PARSE_BYTE,                 /* BYTE */
        [65U] = MYLITE_SQL_PARSE_CALL,                 /* CALL */
        [66U] = MYLITE_SQL_PARSE_CASCADE,              /* CASCADE */
        [67U] = MYLITE_SQL_PARSE_CASCADED,             /* CASCADED */
        [68U] = MYLITE_SQL_PARSE_CASE,                 /* CASE */
        [69U] = MYLITE_SQL_PARSE_CAST,                 /* CAST */
        [71U] = MYLITE_SQL_PARSE_CEIL,                 /* CEIL */
        [72U] = MYLITE_SQL_PARSE_CEILING,              /* CEILING */
        [73U] = MYLITE_SQL_PARSE_CHAIN,                /* CHAIN */
        [75U] = MYLITE_SQL_PARSE_CHANGE,               /* CHANGE */
        [76U] = MYLITE_SQL_PARSE_CHANGED,              /* CHANGED */
        [77U] = MYLITE_SQL_PARSE_CHANNEL,              /* CHANNEL */
        [78U] = MYLITE_SQL_PARSE_CHAR,                 /* CHAR */
        [79U] = MYLITE_SQL_PARSE_CHARACTER,            /* CHARACTER */
        [80U] = MYLITE_SQL_PARSE_CHARACTER_LENGTH,     /* CHARACTER_LENGTH */
        [81U] = MYLITE_SQL_PARSE_CHARSET,              /* CHARSET */
        [82U] = MYLITE_SQL_PARSE_CHAR_LENGTH,          /* CHAR_LENGTH */
        [83U] = MYLITE_SQL_PARSE_CHECK,                /* CHECK */
        [84U] = MYLITE_SQL_PARSE_CHECKSUM,             /* CHECKSUM */
        [90U] = MYLITE_SQL_PARSE_COALESCE,             /* COALESCE */
        [92U] = MYLITE_SQL_PARSE_COERCIBILITY,         /* COERCIBILITY */
        [93U] = MYLITE_SQL_PARSE_COLLATE,              /* COLLATE */
        [94U] = MYLITE_SQL_PARSE_COLLATION,            /* COLLATION */
        [95U] = MYLITE_SQL_PARSE_COLUMN,               /* COLUMN */
        [96U] = MYLITE_SQL_PARSE_COLUMNS,              /* COLUMNS */
        [99U] = MYLITE_SQL_PARSE_COMMENT,              /* COMMENT */
        [100U] = MYLITE_SQL_PARSE_COMMIT,              /* COMMIT */
        [101U] = MYLITE_SQL_PARSE_COMMITTED,           /* COMMITTED */
        [102U] = MYLITE_SQL_PARSE_COMPACT,             /* COMPACT */
        [105U] = MYLITE_SQL_PARSE_COMPRESS,            /* COMPRESS */
        [106U] = MYLITE_SQL_PARSE_COMPRESSED,          /* COMPRESSED */
        [108U] = MYLITE_SQL_PARSE_CONCAT,              /* CONCAT */
        [109U] = MYLITE_SQL_PARSE_CONCAT_WS,           /* CONCAT_WS */
        [113U] = MYLITE_SQL_PARSE_CONNECTION_ID,       /* CONNECTION_ID */
        [114U] = MYLITE_SQL_PARSE_CONSISTENT,          /* CONSISTENT */
        [115U] = MYLITE_SQL_PARSE_CONSTRAINT,          /* CONSTRAINT */
        [122U] = MYLITE_SQL_PARSE_CONV,                /* CONV */
        [123U] = MYLITE_SQL_PARSE_CONVERT,             /* CONVERT */
        [124U] = MYLITE_SQL_PARSE_CONVERT_TZ,          /* CONVERT_TZ */
        [125U] = MYLITE_SQL_PARSE_COS,                 /* COS */
        [126U] = MYLITE_SQL_PARSE_COT,                 /* COT */
        [127U] = MYLITE_SQL_PARSE_COUNT,               /* COUNT */
        [129U] = MYLITE_SQL_PARSE_CRC32,               /* CRC32 */
        [130U] = MYLITE_SQL_PARSE_CREATE,              /* CREATE */
        [131U] = MYLITE_SQL_PARSE_CROSS,               /* CROSS */
        [133U] = MYLITE_SQL_PARSE_CUME_DIST,           /* CUME_DIST */
        [134U] = MYLITE_SQL_PARSE_CURDATE,             /* CURDATE */
        [135U] = MYLITE_SQL_PARSE_CURRENT,             /* CURRENT */
        [136U] = MYLITE_SQL_PARSE_CURRENT_DATE,        /* CURRENT_DATE */
        [137U] = MYLITE_SQL_PARSE_CURRENT_ROLE,        /* CURRENT_ROLE */
        [138U] = MYLITE_SQL_PARSE_CURRENT_TIME,        /* CURRENT_TIME */
        [139U] = MYLITE_SQL_PARSE_CURRENT_TIMESTAMP,   /* CURRENT_TIMESTAMP */
        [140U] = MYLITE_SQL_PARSE_CURRENT_USER,        /* CURRENT_USER */
        [143U] = MYLITE_SQL_PARSE_CURTIME,             /* CURTIME */
        [144U] = MYLITE_SQL_PARSE_DATA,                /* DATA */
        [145U] = MYLITE_SQL_PARSE_DATABASE,            /* DATABASE */
        [146U] = MYLITE_SQL_PARSE_DATABASES,           /* DATABASES */
        [148U] = MYLITE_SQL_PARSE_DATE,                /* DATE */
        [149U] = MYLITE_SQL_PARSE_DATEDIFF,            /* DATEDIFF */
        [150U] = MYLITE_SQL_PARSE_DATETIME,            /* DATETIME */
        [151U] = MYLITE_SQL_PARSE_DATE_ADD,            /* DATE_ADD */
        [152U] = MYLITE_SQL_PARSE_DATE_FORMAT,         /* DATE_FORMAT */
        [153U] = MYLITE_SQL_PARSE_DATE_SUB,            /* DATE_SUB */
        [154U] = MYLITE_SQL_PARSE_DAY,                 /* DAY */
        [155U] = MYLITE_SQL_PARSE_DAYNAME,             /* DAYNAME */
        [156U] = MYLITE_SQL_PARSE_DAYOFMONTH,          /* DAYOFMONTH */
        [157U] = MYLITE_SQL_PARSE_DAYOFWEEK,           /* DAYOFWEEK */
        [158U] = MYLITE_SQL_PARSE_DAYOFYEAR,           /* DAYOFYEAR */
        [159U] = MYLITE_SQL_PARSE_DAY_HOUR,            /* DAY_HOUR */
        [160U] = MYLITE_SQL_PARSE_DAY_MICROSECOND,     /* DAY_MICROSECOND */
        [161U] = MYLITE_SQL_PARSE_DAY_MINUTE,          /* DAY_MINUTE */
        [162U] = MYLITE_SQL_PARSE_DAY_SECOND,          /* DAY_SECOND */
        [163U] = MYLITE_SQL_PARSE_DEALLOCATE,          /* DEALLOCATE */
        [164U] = MYLITE_SQL_PARSE_DEC,                 /* DEC */
        [165U] = MYLITE_SQL_PARSE_DECIMAL_TYPE,        /* DECIMAL */
        [167U] = MYLITE_SQL_PARSE_DEFAULT,             /* DEFAULT */
        [169U] = MYLITE_SQL_PARSE_DEFINER,             /* DEFINER */
        [171U] = MYLITE_SQL_PARSE_DEGREES,             /* DEGREES */
        [172U] = MYLITE_SQL_PARSE_DELAYED,             /* DELAYED */
        [173U] = MYLITE_SQL_PARSE_DELAY_KEY_WRITE,     /* DELAY_KEY_WRITE */
        [174U] = MYLITE_SQL_PARSE_DELETE,              /* DELETE */
        [175U] = MYLITE_SQL_PARSE_DENSE_RANK,          /* DENSE_RANK */
        [176U] = MYLITE_SQL_PARSE_DESC,                /* DESC */
        [177U] = MYLITE_SQL_PARSE_DESCRIBE,            /* DESCRIBE */
        [182U] = MYLITE_SQL_PARSE_DISABLE,             /* DISABLE */
        [184U] = MYLITE_SQL_PARSE_DISK,                /* DISK */
        [185U] = MYLITE_SQL_PARSE_DISTINCT,            /* DISTINCT */
        [186U] = MYLITE_SQL_PARSE_DISTINCTROW,         /* DISTINCTROW */
        [187U] = MYLITE_SQL_PARSE_DIV,                 /* DIV */
        [188U] = MYLITE_SQL_PARSE_DO,                  /* DO */
        [189U] = MYLITE_SQL_PARSE_DOUBLE,              /* DOUBLE */
        [190U] = MYLITE_SQL_PARSE_DROP,                /* DROP */
        [191U] = MYLITE_SQL_PARSE_DUAL,                /* DUAL */
        [193U] = MYLITE_SQL_PARSE_DUPLICATE,           /* DUPLICATE */
        [194U] = MYLITE_SQL_PARSE_DYNAMIC,             /* DYNAMIC */
        [196U] = MYLITE_SQL_PARSE_ELSE,                /* ELSE */
        [198U] = MYLITE_SQL_PARSE_ELT,                 /* ELT */
        [199U] = MYLITE_SQL_PARSE_EMPTY,               /* EMPTY */
        [200U] = MYLITE_SQL_PARSE_ENABLE,              /* ENABLE */
        [203U] = MYLITE_SQL_PARSE_END,                 /* END */
        [205U] = MYLITE_SQL_PARSE_ENFORCED,            /* ENFORCED */
        [206U] = MYLITE_SQL_PARSE_ENGINE,              /* ENGINE */
        [207U] = MYLITE_SQL_PARSE_ENGINES,             /* ENGINES */
        [209U] = MYLITE_SQL_PARSE_ENUM,                /* ENUM */
        [210U] = MYLITE_SQL_PARSE_ERROR,               /* ERROR */
        [211U] = MYLITE_SQL_PARSE_ERRORS,              /* ERRORS */
        [212U] = MYLITE_SQL_PARSE_ESCAPE,              /* ESCAPE */
        [214U] = MYLITE_SQL_PARSE_EVENT,               /* EVENT */
        [215U] = MYLITE_SQL_PARSE_EVENTS,              /* EVENTS */
        [217U] = MYLITE_SQL_PARSE_EXCEPT,              /* EXCEPT */
        [220U] = MYLITE_SQL_PARSE_EXECUTE,             /* EXECUTE */
        [221U] = MYLITE_SQL_PARSE_EXISTS,              /* EXISTS */
        [223U] = MYLITE_SQL_PARSE_EXP,                 /* EXP */
        [224U] = MYLITE_SQL_PARSE_EXPANSION,           /* EXPANSION */
        [226U] = MYLITE_SQL_PARSE_EXPLAIN,             /* EXPLAIN */
        [228U] = MYLITE_SQL_PARSE_EXPORT_SET,          /* EXPORT_SET */
        [229U] = MYLITE_SQL_PARSE_EXTENDED,            /* EXTENDED */
        [231U] = MYLITE_SQL_PARSE_EXTRACT,             /* EXTRACT */
        [234U] = MYLITE_SQL_PARSE_FALSE,               /* FALSE */
        [235U] = MYLITE_SQL_PARSE_FAST,                /* FAST */
        [238U] = MYLITE_SQL_PARSE_FIELD,               /* FIELD */
        [239U] = MYLITE_SQL_PARSE_FIELDS,              /* FIELDS */
        [243U] = MYLITE_SQL_PARSE_FIND_IN_SET,         /* FIND_IN_SET */
        [245U] = MYLITE_SQL_PARSE_FIRST,               /* FIRST */
        [246U] = MYLITE_SQL_PARSE_FIRST_VALUE,         /* FIRST_VALUE */
        [247U] = MYLITE_SQL_PARSE_FIXED,               /* FIXED */
        [248U] = MYLITE_SQL_PARSE_FLOAT_TYPE,          /* FLOAT */
        [249U] = MYLITE_SQL_PARSE_FLOAT4,              /* FLOAT4 */
        [250U] = MYLITE_SQL_PARSE_FLOAT8,              /* FLOAT8 */
        [251U] = MYLITE_SQL_PARSE_FLOOR,               /* FLOOR */
        [252U] = MYLITE_SQL_PARSE_FLUSH,               /* FLUSH */
        [253U] = MYLITE_SQL_PARSE_FOLLOWING,           /* FOLLOWING */
        [255U] = MYLITE_SQL_PARSE_FOR,                 /* FOR */
        [256U] = MYLITE_SQL_PARSE_FORCE,               /* FORCE */
        [257U] = MYLITE_SQL_PARSE_FOREIGN,             /* FOREIGN */
        [258U] = MYLITE_SQL_PARSE_FORMAT,              /* FORMAT */
        [260U] = MYLITE_SQL_PARSE_FOUND_ROWS,          /* FOUND_ROWS */
        [261U] = MYLITE_SQL_PARSE_FROM,                /* FROM */
        [262U] = MYLITE_SQL_PARSE_FROM_BASE64,         /* FROM_BASE64 */
        [263U] = MYLITE_SQL_PARSE_FROM_DAYS,           /* FROM_DAYS */
        [264U] = MYLITE_SQL_PARSE_FROM_UNIXTIME,       /* FROM_UNIXTIME */
        [265U] = MYLITE_SQL_PARSE_FULL,                /* FULL */
        [266U] = MYLITE_SQL_PARSE_FULLTEXT,            /* FULLTEXT */
        [267U] = MYLITE_SQL_PARSE_FUNCTION,            /* FUNCTION */
        [270U] = MYLITE_SQL_PARSE_GENERATED,           /* GENERATED */
        [271U] = MYLITE_SQL_PARSE_GEOMCOLLECTION,      /* GEOMCOLLECTION */
        [272U] = MYLITE_SQL_PARSE_GEOMETRY,            /* GEOMETRY */
        [273U] = MYLITE_SQL_PARSE_GEOMETRYCOLLECTION,  /* GEOMETRYCOLLECTION */
        [275U] = MYLITE_SQL_PARSE_GET_FORMAT,          /* GET_FORMAT */
        [277U] = MYLITE_SQL_PARSE_GLOBAL,              /* GLOBAL */
        [279U] = MYLITE_SQL_PARSE_GRANTS,              /* GRANTS */
        [280U] = MYLITE_SQL_PARSE_GREATEST,            /* GREATEST */
        [281U] = MYLITE_SQL_PARSE_GROUP,               /* GROUP */
        [282U] = MYLITE_SQL_PARSE_GROUPING,            /* GROUPING */
        [284U] = MYLITE_SQL_PARSE_GROUP_CONCAT,        /* GROUP_CONCAT */
        [290U] = MYLITE_SQL_PARSE_HAVING,              /* HAVING */
        [292U] = MYLITE_SQL_PARSE_HEX,                 /* HEX */
        [293U] = MYLITE_SQL_PARSE_HIGH_PRIORITY,       /* HIGH_PRIORITY */
        [298U] = MYLITE_SQL_PARSE_HOUR,                /* HOUR */
        [299U] = MYLITE_SQL_PARSE_HOUR_MICROSECOND,    /* HOUR_MICROSECOND */
        [300U] = MYLITE_SQL_PARSE_HOUR_MINUTE,         /* HOUR_MINUTE */
        [301U] = MYLITE_SQL_PARSE_HOUR_SECOND,         /* HOUR_SECOND */
        [303U] = MYLITE_SQL_PARSE_IF,                  /* IF */
        [304U] = MYLITE_SQL_PARSE_IFNULL,              /* IFNULL */
        [305U] = MYLITE_SQL_PARSE_IGNORE,              /* IGNORE */
        [308U] = MYLITE_SQL_PARSE_IN,                  /* IN */
        [310U] = MYLITE_SQL_PARSE_INDEX,               /* INDEX */
        [311U] = MYLITE_SQL_PARSE_INDEXES,             /* INDEXES */
        [312U] = MYLITE_SQL_PARSE_INFILE,              /* INFILE */
        [316U] = MYLITE_SQL_PARSE_INNER,               /* INNER */
        [319U] = MYLITE_SQL_PARSE_INSERT,              /* INSERT */
        [320U] = MYLITE_SQL_PARSE_INSERT_METHOD,       /* INSERT_METHOD */
        [323U] = MYLITE_SQL_PARSE_INSTR,               /* INSTR */
        [324U] = MYLITE_SQL_PARSE_INT,                 /* INT */
        [325U] = MYLITE_SQL_PARSE_INT1,                /* INT1 */
        [326U] = MYLITE_SQL_PARSE_INT2,                /* INT2 */
        [327U] = MYLITE_SQL_PARSE_INT3,                /* INT3 */
        [328U] = MYLITE_SQL_PARSE_INT4,                /* INT4 */
        [329U] = MYLITE_SQL_PARSE_INT8,                /* INT8 */
        [330U] = MYLITE_SQL_PARSE_INTEGER_TYPE,        /* INTEGER */
        [331U] = MYLITE_SQL_PARSE_INTERSECT,           /* INTERSECT */
        [332U] = MYLITE_SQL_PARSE_INTERVAL,            /* INTERVAL */
        [333U] = MYLITE_SQL_PARSE_INTO,                /* INTO */
        [334U] = MYLITE_SQL_PARSE_INVISIBLE,           /* INVISIBLE */
        [335U] = MYLITE_SQL_PARSE_INVOKER,             /* INVOKER */
        [341U] = MYLITE_SQL_PARSE_IS,                  /* IS */
        [342U] = MYLITE_SQL_PARSE_ISNULL,              /* ISNULL */
        [343U] = MYLITE_SQL_PARSE_ISOLATION,           /* ISOLATION */
        [345U] = MYLITE_SQL_PARSE_IS_UUID,             /* IS_UUID */
        [347U] = MYLITE_SQL_PARSE_JOIN,                /* JOIN */
        [348U] = MYLITE_SQL_PARSE_JSON,                /* JSON */
        [349U] = MYLITE_SQL_PARSE_JSON_ARRAY,          /* JSON_ARRAY */
        [350U] = MYLITE_SQL_PARSE_JSON_CONTAINS,       /* JSON_CONTAINS */
        [351U] = MYLITE_SQL_PARSE_JSON_CONTAINS_PATH,  /* JSON_CONTAINS_PATH */
        [352U] = MYLITE_SQL_PARSE_JSON_EXTRACT,        /* JSON_EXTRACT */
        [353U] = MYLITE_SQL_PARSE_JSON_INSERT,         /* JSON_INSERT */
        [354U] = MYLITE_SQL_PARSE_JSON_KEYS,           /* JSON_KEYS */
        [355U] = MYLITE_SQL_PARSE_JSON_LENGTH,         /* JSON_LENGTH */
        [356U] = MYLITE_SQL_PARSE_JSON_OBJECT,         /* JSON_OBJECT */
        [357U] = MYLITE_SQL_PARSE_JSON_QUOTE,          /* JSON_QUOTE */
        [358U] = MYLITE_SQL_PARSE_JSON_REMOVE,         /* JSON_REMOVE */
        [359U] = MYLITE_SQL_PARSE_JSON_REPLACE,        /* JSON_REPLACE */
        [360U] = MYLITE_SQL_PARSE_JSON_SET,            /* JSON_SET */
        [361U] = MYLITE_SQL_PARSE_JSON_TABLE,          /* JSON_TABLE */
        [362U] = MYLITE_SQL_PARSE_JSON_TYPE,           /* JSON_TYPE */
        [363U] = MYLITE_SQL_PARSE_JSON_UNQUOTE,        /* JSON_UNQUOTE */
        [364U] = MYLITE_SQL_PARSE_JSON_VALID,          /* JSON_VALID */
        [365U] = MYLITE_SQL_PARSE_JSON_VALUE,          /* JSON_VALUE */
        [366U] = MYLITE_SQL_PARSE_KEY,                 /* KEY */
        [368U] = MYLITE_SQL_PARSE_KEYS,                /* KEYS */
        [369U] = MYLITE_SQL_PARSE_KEY_BLOCK_SIZE,      /* KEY_BLOCK_SIZE */
        [371U] = MYLITE_SQL_PARSE_LAG,                 /* LAG */
        [372U] = MYLITE_SQL_PARSE_LANGUAGE,            /* LANGUAGE */
        [373U] = MYLITE_SQL_PARSE_LAST,                /* LAST */
        [374U] = MYLITE_SQL_PARSE_LAST_DAY,            /* LAST_DAY */
        [375U] = MYLITE_SQL_PARSE_LAST_INSERT_ID,      /* LAST_INSERT_ID */
        [376U] = MYLITE_SQL_PARSE_LAST_VALUE,          /* LAST_VALUE */
        [378U] = MYLITE_SQL_PARSE_LCASE,               /* LCASE */
        [379U] = MYLITE_SQL_PARSE_LEAD,                /* LEAD */
        [380U] = MYLITE_SQL_PARSE_LEADING,             /* LEADING */
        [381U] = MYLITE_SQL_PARSE_LEAST,               /* LEAST */
        [384U] = MYLITE_SQL_PARSE_LEFT,                /* LEFT */
        [385U] = MYLITE_SQL_PARSE_LENGTH,              /* LENGTH */
        [387U] = MYLITE_SQL_PARSE_LEVEL,               /* LEVEL */
        [388U] = MYLITE_SQL_PARSE_LIKE,                /* LIKE */
        [389U] = MYLITE_SQL_PARSE_LIMIT,               /* LIMIT */
        [391U] = MYLITE_SQL_PARSE_LINES,               /* LINES */
        [392U] = MYLITE_SQL_PARSE_LINESTRING,          /* LINESTRING */
        [394U] = MYLITE_SQL_PARSE_LN,                  /* LN */
        [395U] = MYLITE_SQL_PARSE_LOAD,                /* LOAD */
        [396U] = MYLITE_SQL_PARSE_LOCAL,               /* LOCAL */
        [397U] = MYLITE_SQL_PARSE_LOCALTIME,           /* LOCALTIME */
        [398U] = MYLITE_SQL_PARSE_LOCALTIMESTAMP,      /* LOCALTIMESTAMP */
        [399U] = MYLITE_SQL_PARSE_LOCATE,              /* LOCATE */
        [400U] = MYLITE_SQL_PARSE_LOCK,                /* LOCK */
        [401U] = MYLITE_SQL_PARSE_LOCKED,              /* LOCKED */
        [403U] = MYLITE_SQL_PARSE_LOG,                 /* LOG */
        [404U] = MYLITE_SQL_PARSE_LOG10,               /* LOG10 */
        [405U] = MYLITE_SQL_PARSE_LOG2,                /* LOG2 */
        [407U] = MYLITE_SQL_PARSE_LOGS,                /* LOGS */
        [408U] = MYLITE_SQL_PARSE_LONG,                /* LONG */
        [409U] = MYLITE_SQL_PARSE_LONGBLOB,            /* LONGBLOB */
        [410U] = MYLITE_SQL_PARSE_LONGTEXT,            /* LONGTEXT */
        [412U] = MYLITE_SQL_PARSE_LOWER,               /* LOWER */
        [413U] = MYLITE_SQL_PARSE_LOW_PRIORITY,        /* LOW_PRIORITY */
        [414U] = MYLITE_SQL_PARSE_LPAD,                /* LPAD */
        [415U] = MYLITE_SQL_PARSE_LTRIM,               /* LTRIM */
        [416U] = MYLITE_SQL_PARSE_MAKEDATE,            /* MAKEDATE */
        [417U] = MYLITE_SQL_PARSE_MAKETIME,            /* MAKETIME */
        [418U] = MYLITE_SQL_PARSE_MAKE_SET,            /* MAKE_SET */
        [421U] = MYLITE_SQL_PARSE_MATCH,               /* MATCH */
        [422U] = MYLITE_SQL_PARSE_MAX,                 /* MAX */
        [426U] = MYLITE_SQL_PARSE_MAX_ROWS,            /* MAX_ROWS */
        [430U] = MYLITE_SQL_PARSE_MD5,                 /* MD5 */
        [431U] = MYLITE_SQL_PARSE_MEDIUM,              /* MEDIUM */
        [432U] = MYLITE_SQL_PARSE_MEDIUMBLOB,          /* MEDIUMBLOB */
        [433U] = MYLITE_SQL_PARSE_MEDIUMINT,           /* MEDIUMINT */
        [434U] = MYLITE_SQL_PARSE_MEDIUMTEXT,          /* MEDIUMTEXT */
        [435U] = MYLITE_SQL_PARSE_MEMBER,              /* MEMBER */
        [436U] = MYLITE_SQL_PARSE_MEMORY,              /* MEMORY */
        [437U] = MYLITE_SQL_PARSE_MERGE,               /* MERGE */
        [439U] = MYLITE_SQL_PARSE_MICROSECOND,         /* MICROSECOND */
        [440U] = MYLITE_SQL_PARSE_MID,                 /* MID */
        [443U] = MYLITE_SQL_PARSE_MIN,                 /* MIN */
        [444U] = MYLITE_SQL_PARSE_MINUTE,              /* MINUTE */
        [445U] = MYLITE_SQL_PARSE_MINUTE_MICROSECOND,  /* MINUTE_MICROSECOND */
        [446U] = MYLITE_SQL_PARSE_MINUTE_SECOND,       /* MINUTE_SECOND */
        [447U] = MYLITE_SQL_PARSE_MIN_ROWS,            /* MIN_ROWS */
        [448U] = MYLITE_SQL_PARSE_MOD,                 /* MOD */
        [449U] = MYLITE_SQL_PARSE_MODE,                /* MODE */
        [451U] = MYLITE_SQL_PARSE_MODIFY,              /* MODIFY */
        [452U] = MYLITE_SQL_PARSE_MONTH,               /* MONTH */
        [453U] = MYLITE_SQL_PARSE_MONTHNAME,           /* MONTHNAME */
        [454U] = MYLITE_SQL_PARSE_MULTILINESTRING,     /* MULTILINESTRING */
        [455U] = MYLITE_SQL_PARSE_MULTIPOINT,          /* MULTIPOINT */
        [456U] = MYLITE_SQL_PARSE_MULTIPOLYGON,        /* MULTIPOLYGON */
        [460U] = MYLITE_SQL_PARSE_NAMES,               /* NAMES */
        [461U] = MYLITE_SQL_PARSE_NATIONAL,            /* NATIONAL */
        [462U] = MYLITE_SQL_PARSE_NATURAL,             /* NATURAL */
        [463U] = MYLITE_SQL_PARSE_NCHAR,               /* NCHAR */
        [471U] = MYLITE_SQL_PARSE_NO,                  /* NO */
        [474U] = MYLITE_SQL_PARSE_NOT,                 /* NOT */
        [475U] = MYLITE_SQL_PARSE_NOW,                 /* NOW */
        [476U] = MYLITE_SQL_PARSE_NOWAIT,              /* NOWAIT */
        [478U] = MYLITE_SQL_PARSE_NO_WRITE_TO_BINLOG,  /* NO_WRITE_TO_BINLOG */
        [479U] = MYLITE_SQL_PARSE_NTH_VALUE,           /* NTH_VALUE */
        [480U] = MYLITE_SQL_PARSE_NTILE,               /* NTILE */
        [481U] = MYLITE_SQL_PARSE_NULL,                /* NULL */
        [482U] = MYLITE_SQL_PARSE_NULLIF,              /* NULLIF */
        [483U] = MYLITE_SQL_PARSE_NULLS,               /* NULLS */
        [485U] = MYLITE_SQL_PARSE_NUMERIC,             /* NUMERIC */
        [486U] = MYLITE_SQL_PARSE_NVARCHAR,            /* NVARCHAR */
        [487U] = MYLITE_SQL_PARSE_OCT,                 /* OCT */
        [488U] = MYLITE_SQL_PARSE_OCTET_LENGTH,        /* OCTET_LENGTH */
        [489U] = MYLITE_SQL_PARSE_OF,                  /* OF */
        [490U] = MYLITE_SQL_PARSE_OFF,                 /* OFF */
        [491U] = MYLITE_SQL_PARSE_OFFSET,              /* OFFSET */
        [494U] = MYLITE_SQL_PARSE_ON,                  /* ON */
        [496U] = MYLITE_SQL_PARSE_ONLY,                /* ONLY */
        [497U] = MYLITE_SQL_PARSE_OPEN,                /* OPEN */
        [498U] = MYLITE_SQL_PARSE_OPTIMIZE,            /* OPTIMIZE */
        [500U] = MYLITE_SQL_PARSE_OPTION,              /* OPTION */
        [504U] = MYLITE_SQL_PARSE_OR,                  /* OR */
        [505U] = MYLITE_SQL_PARSE_ORD,                 /* ORD */
        [506U] = MYLITE_SQL_PARSE_ORDER,               /* ORDER */
        [507U] = MYLITE_SQL_PARSE_ORDINALITY,          /* ORDINALITY */
        [511U] = MYLITE_SQL_PARSE_OUTER,               /* OUTER */
        [513U] = MYLITE_SQL_PARSE_OVER,                /* OVER */
        [515U] = MYLITE_SQL_PARSE_PACK_KEYS,           /* PACK_KEYS */
        [521U] = MYLITE_SQL_PARSE_PARTITION,           /* PARTITION */
        [526U] = MYLITE_SQL_PARSE_PATH,                /* PATH */
        [527U] = MYLITE_SQL_PARSE_PERCENT_RANK,        /* PERCENT_RANK */
        [528U] = MYLITE_SQL_PARSE_PERIOD_ADD,          /* PERIOD_ADD */
        [529U] = MYLITE_SQL_PARSE_PERIOD_DIFF,         /* PERIOD_DIFF */
        [533U] = MYLITE_SQL_PARSE_PI,                  /* PI */
        [535U] = MYLITE_SQL_PARSE_PLUGINS,             /* PLUGINS */
        [537U] = MYLITE_SQL_PARSE_POINT,               /* POINT */
        [538U] = MYLITE_SQL_PARSE_POLYGON,             /* POLYGON */
        [540U] = MYLITE_SQL_PARSE_POSITION,            /* POSITION */
        [541U] = MYLITE_SQL_PARSE_POW,                 /* POW */
        [542U] = MYLITE_SQL_PARSE_POWER,               /* POWER */
        [544U] = MYLITE_SQL_PARSE_PRECEDING,           /* PRECEDING */
        [545U] = MYLITE_SQL_PARSE_PRECISION,           /* PRECISION */
        [546U] = MYLITE_SQL_PARSE_PREPARE,             /* PREPARE */
        [549U] = MYLITE_SQL_PARSE_PRIMARY,             /* PRIMARY */
        [550U] = MYLITE_SQL_PARSE_PRIVILEGES,          /* PRIVILEGES */
        [552U] = MYLITE_SQL_PARSE_PROCEDURE,           /* PROCEDURE */
        [554U] = MYLITE_SQL_PARSE_PROCESSLIST,         /* PROCESSLIST */
        [560U] = MYLITE_SQL_PARSE_QUARTER,             /* QUARTER */
        [561U] = MYLITE_SQL_PARSE_QUERY,               /* QUERY */
        [562U] = MYLITE_SQL_PARSE_QUICK,               /* QUICK */
        [563U] = MYLITE_SQL_PARSE_QUOTE,               /* QUOTE */
        [564U] = MYLITE_SQL_PARSE_RADIANS,             /* RADIANS */
        [565U] = MYLITE_SQL_PARSE_RAND,                /* RAND */
        [567U] = MYLITE_SQL_PARSE_RANDOM_BYTES,        /* RANDOM_BYTES */
        [568U] = MYLITE_SQL_PARSE_RANGE,               /* RANGE */
        [569U] = MYLITE_SQL_PARSE_RANK,                /* RANK */
        [570U] = MYLITE_SQL_PARSE_READ,                /* READ */
        [574U] = MYLITE_SQL_PARSE_REAL,                /* REAL */
        [579U] = MYLITE_SQL_PARSE_REDUNDANT,           /* REDUNDANT */
        [581U] = MYLITE_SQL_PARSE_REFERENCES,          /* REFERENCES */
        [582U] = MYLITE_SQL_PARSE_REGEXP,              /* REGEXP */
        [583U] = MYLITE_SQL_PARSE_REGEXP_INSTR,        /* REGEXP_INSTR */
        [584U] = MYLITE_SQL_PARSE_REGEXP_LIKE,         /* REGEXP_LIKE */
        [585U] = MYLITE_SQL_PARSE_REGEXP_REPLACE,      /* REGEXP_REPLACE */
        [586U] = MYLITE_SQL_PARSE_REGEXP_SUBSTR,       /* REGEXP_SUBSTR */
        [589U] = MYLITE_SQL_PARSE_RELAYLOG,            /* RELAYLOG */
        [593U] = MYLITE_SQL_PARSE_RELEASE,             /* RELEASE */
        [596U] = MYLITE_SQL_PARSE_RENAME,              /* RENAME */
        [598U] = MYLITE_SQL_PARSE_REPAIR,              /* REPAIR */
        [599U] = MYLITE_SQL_PARSE_REPEAT,              /* REPEAT */
        [600U] = MYLITE_SQL_PARSE_REPEATABLE,          /* REPEATABLE */
        [601U] = MYLITE_SQL_PARSE_REPLACE,             /* REPLACE */
        [602U] = MYLITE_SQL_PARSE_REPLICA,             /* REPLICA */
        [603U] = MYLITE_SQL_PARSE_REPLICAS,            /* REPLICAS */
        [618U] = MYLITE_SQL_PARSE_RESPECT,             /* RESPECT */
        [621U] = MYLITE_SQL_PARSE_RESTRICT,            /* RESTRICT */
        [629U] = MYLITE_SQL_PARSE_REVERSE,             /* REVERSE */
        [631U] = MYLITE_SQL_PARSE_RIGHT,               /* RIGHT */
        [632U] = MYLITE_SQL_PARSE_RLIKE,               /* RLIKE */
        [634U] = MYLITE_SQL_PARSE_ROLLBACK,            /* ROLLBACK */
        [635U] = MYLITE_SQL_PARSE_ROLLUP,              /* ROLLUP */
        [637U] = MYLITE_SQL_PARSE_ROUND,               /* ROUND */
        [639U] = MYLITE_SQL_PARSE_ROW,                 /* ROW */
        [640U] = MYLITE_SQL_PARSE_ROWS,                /* ROWS */
        [641U] = MYLITE_SQL_PARSE_ROW_COUNT,           /* ROW_COUNT */
        [642U] = MYLITE_SQL_PARSE_ROW_FORMAT,          /* ROW_FORMAT */
        [643U] = MYLITE_SQL_PARSE_ROW_NUMBER,          /* ROW_NUMBER */
        [644U] = MYLITE_SQL_PARSE_RPAD,                /* RPAD */
        [646U] = MYLITE_SQL_PARSE_RTRIM,               /* RTRIM */
        [648U] = MYLITE_SQL_PARSE_SAVEPOINT,           /* SAVEPOINT */
        [650U] = MYLITE_SQL_PARSE_SCHEMA,              /* SCHEMA */
        [651U] = MYLITE_SQL_PARSE_SCHEMAS,             /* SCHEMAS */
        [653U] = MYLITE_SQL_PARSE_SECOND,              /* SECOND */
        [659U] = MYLITE_SQL_PARSE_SECOND_MICROSECOND,  /* SECOND_MICROSECOND */
        [660U] = MYLITE_SQL_PARSE_SECURITY,            /* SECURITY */
        [661U] = MYLITE_SQL_PARSE_SEC_TO_TIME,         /* SEC_TO_TIME */
        [662U] = MYLITE_SQL_PARSE_SELECT,              /* SELECT */
        [664U] = MYLITE_SQL_PARSE_SEPARATOR,           /* SEPARATOR */
        [665U] = MYLITE_SQL_PARSE_SERIAL,              /* SERIAL */
        [666U] = MYLITE_SQL_PARSE_SERIALIZABLE,        /* SERIALIZABLE */
        [668U] = MYLITE_SQL_PARSE_SESSION,             /* SESSION */
        [669U] = MYLITE_SQL_PARSE_SESSION_USER,        /* SESSION_USER */
        [670U] = MYLITE_SQL_PARSE_SET,                 /* SET */
        [671U] = MYLITE_SQL_PARSE_SHA,                 /* SHA */
        [672U] = MYLITE_SQL_PARSE_SHA1,                /* SHA1 */
        [673U] = MYLITE_SQL_PARSE_SHA2,                /* SHA2 */
        [674U] = MYLITE_SQL_PARSE_SHARE,               /* SHARE */
        [675U] = MYLITE_SQL_PARSE_SHOW,                /* SHOW */
        [677U] = MYLITE_SQL_PARSE_SIGN,                /* SIGN */
        [679U] = MYLITE_SQL_PARSE_SIGNED,              /* SIGNED */
        [681U] = MYLITE_SQL_PARSE_SIN,                 /* SIN */
        [682U] = MYLITE_SQL_PARSE_SKIP,                /* SKIP */
        [685U] = MYLITE_SQL_PARSE_SMALLINT,            /* SMALLINT */
        [686U] = MYLITE_SQL_PARSE_SNAPSHOT,            /* SNAPSHOT */
        [688U] = MYLITE_SQL_PARSE_SOME,                /* SOME */
        [690U] = MYLITE_SQL_PARSE_SOUNDEX,             /* SOUNDEX */
        [691U] = MYLITE_SQL_PARSE_SOUNDS,              /* SOUNDS */
        [720U] = MYLITE_SQL_PARSE_SPACE,               /* SPACE */
        [721U] = MYLITE_SQL_PARSE_SPATIAL,             /* SPATIAL */
        [723U] = MYLITE_SQL_PARSE_SQL,                 /* SQL */
        [730U] = MYLITE_SQL_PARSE_SQL_BIG_RESULT,      /* SQL_BIG_RESULT */
        [732U] = MYLITE_SQL_PARSE_SQL_CALC_FOUND_ROWS, /* SQL_CALC_FOUND_ROWS */
        [734U] = MYLITE_SQL_PARSE_SQL_SMALL_RESULT,    /* SQL_SMALL_RESULT */
        [736U] = MYLITE_SQL_PARSE_SQL_TSI_DAY,         /* SQL_TSI_DAY */
        [737U] = MYLITE_SQL_PARSE_SQL_TSI_HOUR,        /* SQL_TSI_HOUR */
        [738U] = MYLITE_SQL_PARSE_SQL_TSI_MINUTE,      /* SQL_TSI_MINUTE */
        [739U] = MYLITE_SQL_PARSE_SQL_TSI_MONTH,       /* SQL_TSI_MONTH */
        [740U] = MYLITE_SQL_PARSE_SQL_TSI_QUARTER,     /* SQL_TSI_QUARTER */
        [741U] = MYLITE_SQL_PARSE_SQL_TSI_SECOND,      /* SQL_TSI_SECOND */
        [742U] = MYLITE_SQL_PARSE_SQL_TSI_WEEK,        /* SQL_TSI_WEEK */
        [743U] = MYLITE_SQL_PARSE_SQL_TSI_YEAR,        /* SQL_TSI_YEAR */
        [744U] = MYLITE_SQL_PARSE_SQRT,                /* SQRT */
        [745U] = MYLITE_SQL_PARSE_SRID,                /* SRID */
        [748U] = MYLITE_SQL_PARSE_START,               /* START */
        [751U] = MYLITE_SQL_PARSE_STATS_AUTO_RECALC,   /* STATS_AUTO_RECALC */
        [752U] = MYLITE_SQL_PARSE_STATS_PERSISTENT,    /* STATS_PERSISTENT */
        [753U] = MYLITE_SQL_PARSE_STATS_SAMPLE_PAGES,  /* STATS_SAMPLE_PAGES */
        [754U] = MYLITE_SQL_PARSE_STATUS,              /* STATUS */
        [756U] = MYLITE_SQL_PARSE_STORAGE,             /* STORAGE */
        [757U] = MYLITE_SQL_PARSE_STORED,              /* STORED */
        [758U] = MYLITE_SQL_PARSE_STRAIGHT_JOIN,       /* STRAIGHT_JOIN */
        [759U] = MYLITE_SQL_PARSE_STRCMP,              /* STRCMP */
        [762U] = MYLITE_SQL_PARSE_STR_TO_DATE,         /* STR_TO_DATE */
        [764U] = MYLITE_SQL_PARSE_SUBDATE,             /* SUBDATE */
        [768U] = MYLITE_SQL_PARSE_SUBSTR,              /* SUBSTR */
        [769U] = MYLITE_SQL_PARSE_SUBSTRING,           /* SUBSTRING */
        [770U] = MYLITE_SQL_PARSE_SUBSTRING_INDEX,     /* SUBSTRING_INDEX */
        [771U] = MYLITE_SQL_PARSE_SUBTIME,             /* SUBTIME */
        [772U] = MYLITE_SQL_PARSE_SUM,                 /* SUM */
        [777U] = MYLITE_SQL_PARSE_SYSDATE,             /* SYSDATE */
        [778U] = MYLITE_SQL_PARSE_SYSTEM,              /* SYSTEM */
        [779U] = MYLITE_SQL_PARSE_SYSTEM_USER,         /* SYSTEM_USER */
        [780U] = MYLITE_SQL_PARSE_TABLE,               /* TABLE */
        [781U] = MYLITE_SQL_PARSE_TABLES,              /* TABLES */
        [783U] = MYLITE_SQL_PARSE_TABLESPACE,          /* TABLESPACE */
        [786U] = MYLITE_SQL_PARSE_TAN,                 /* TAN */
        [787U] = MYLITE_SQL_PARSE_TEMPORARY,           /* TEMPORARY */
        [788U] = MYLITE_SQL_PARSE_TEMPTABLE,           /* TEMPTABLE */
        [790U] = MYLITE_SQL_PARSE_TEXT,                /* TEXT */
        [792U] = MYLITE_SQL_PARSE_THEN,                /* THEN */
        [795U] = MYLITE_SQL_PARSE_TIME,                /* TIME */
        [796U] = MYLITE_SQL_PARSE_TIMEDIFF,            /* TIMEDIFF */
        [797U] = MYLITE_SQL_PARSE_TIMESTAMP,           /* TIMESTAMP */
        [798U] = MYLITE_SQL_PARSE_TIMESTAMPADD,        /* TIMESTAMPADD */
        [799U] = MYLITE_SQL_PARSE_TIMESTAMPDIFF,       /* TIMESTAMPDIFF */
        [800U] = MYLITE_SQL_PARSE_TIME_FORMAT,         /* TIME_FORMAT */
        [801U] = MYLITE_SQL_PARSE_TIME_TO_SEC,         /* TIME_TO_SEC */
        [802U] = MYLITE_SQL_PARSE_TINYBLOB,            /* TINYBLOB */
        [803U] = MYLITE_SQL_PARSE_TINYINT,             /* TINYINT */
        [804U] = MYLITE_SQL_PARSE_TINYTEXT,            /* TINYTEXT */
        [806U] = MYLITE_SQL_PARSE_TO,                  /* TO */
        [807U] = MYLITE_SQL_PARSE_TO_BASE64,           /* TO_BASE64 */
        [808U] = MYLITE_SQL_PARSE_TO_DAYS,             /* TO_DAYS */
        [809U] = MYLITE_SQL_PARSE_TO_SECONDS,          /* TO_SECONDS */
        [810U] = MYLITE_SQL_PARSE_TRAILING,            /* TRAILING */
        [811U] = MYLITE_SQL_PARSE_TRANSACTION,         /* TRANSACTION */
        [812U] = MYLITE_SQL_PARSE_TRIGGER,             /* TRIGGER */
        [813U] = MYLITE_SQL_PARSE_TRIGGERS,            /* TRIGGERS */
        [814U] = MYLITE_SQL_PARSE_TRIM,                /* TRIM */
        [815U] = MYLITE_SQL_PARSE_TRUE,                /* TRUE */
        [816U] = MYLITE_SQL_PARSE_TRUNCATE,            /* TRUNCATE */
        [819U] = MYLITE_SQL_PARSE_UCASE,               /* UCASE */
        [820U] = MYLITE_SQL_PARSE_UNBOUNDED,           /* UNBOUNDED */
        [821U] = MYLITE_SQL_PARSE_UNCOMMITTED,         /* UNCOMMITTED */
        [822U] = MYLITE_SQL_PARSE_UNCOMPRESS,          /* UNCOMPRESS */
        [823U] = MYLITE_SQL_PARSE_UNCOMPRESSED_LENGTH, /* UNCOMPRESSED_LENGTH */
        [824U] = MYLITE_SQL_PARSE_UNDEFINED,           /* UNDEFINED */
        [828U] = MYLITE_SQL_PARSE_UNHEX,               /* UNHEX */
        [829U] = MYLITE_SQL_PARSE_UNICODE,             /* UNICODE */
        [831U] = MYLITE_SQL_PARSE_UNION,               /* UNION */
        [832U] = MYLITE_SQL_PARSE_UNIQUE,              /* UNIQUE */
        [833U] = MYLITE_SQL_PARSE_UNIX_TIMESTAMP,      /* UNIX_TIMESTAMP */
        [834U] = MYLITE_SQL_PARSE_UNKNOWN,             /* UNKNOWN */
        [835U] = MYLITE_SQL_PARSE_UNLOCK,              /* UNLOCK */
        [837U] = MYLITE_SQL_PARSE_UNSIGNED,            /* UNSIGNED */
        [839U] = MYLITE_SQL_PARSE_UPDATE,              /* UPDATE */
        [840U] = MYLITE_SQL_PARSE_UPGRADE,             /* UPGRADE */
        [841U] = MYLITE_SQL_PARSE_UPPER,               /* UPPER */
        [844U] = MYLITE_SQL_PARSE_USE,                 /* USE */
        [845U] = MYLITE_SQL_PARSE_USER,                /* USER */
        [847U] = MYLITE_SQL_PARSE_USE_FRM,             /* USE_FRM */
        [848U] = MYLITE_SQL_PARSE_USING,               /* USING */
        [849U] = MYLITE_SQL_PARSE_UTC,                 /* UTC */
        [850U] = MYLITE_SQL_PARSE_UTC_DATE,            /* UTC_DATE */
        [851U] = MYLITE_SQL_PARSE_UTC_TIME,            /* UTC_TIME */
        [852U] = MYLITE_SQL_PARSE_UTC_TIMESTAMP,       /* UTC_TIMESTAMP */
        [853U] = MYLITE_SQL_PARSE_UUID,                /* UUID */
        [854U] = MYLITE_SQL_PARSE_UUID_TO_BIN,         /* UUID_TO_BIN */
        [856U] = MYLITE_SQL_PARSE_VALUE,               /* VALUE */
        [857U] = MYLITE_SQL_PARSE_VALUES,              /* VALUES */
        [858U] = MYLITE_SQL_PARSE_VARBINARY,           /* VARBINARY */
        [859U] = MYLITE_SQL_PARSE_VARCHAR,             /* VARCHAR */
        [861U] = MYLITE_SQL_PARSE_VARIABLES,           /* VARIABLES */
        [862U] = MYLITE_SQL_PARSE_VARYING,             /* VARYING */
        [864U] = MYLITE_SQL_PARSE_VERSION,             /* VERSION */
        [865U] = MYLITE_SQL_PARSE_VIEW,                /* VIEW */
        [866U] = MYLITE_SQL_PARSE_VIRTUAL,             /* VIRTUAL */
        [867U] = MYLITE_SQL_PARSE_VISIBLE,             /* VISIBLE */
        [869U] = MYLITE_SQL_PARSE_WARNINGS,            /* WARNINGS */
        [870U] = MYLITE_SQL_PARSE_WEEK,                /* WEEK */
        [871U] = MYLITE_SQL_PARSE_WEEKDAY,             /* WEEKDAY */
        [872U] = MYLITE_SQL_PARSE_WEEKOFYEAR,          /* WEEKOFYEAR */
        [873U] = MYLITE_SQL_PARSE_WEIGHT_STRING,       /* WEIGHT_STRING */
        [874U] = MYLITE_SQL_PARSE_WHEN,                /* WHEN */
        [875U] = MYLITE_SQL_PARSE_WHERE,               /* WHERE */
        [877U] = MYLITE_SQL_PARSE_WINDOW,              /* WINDOW */
        [878U] = MYLITE_SQL_PARSE_WITH,                /* WITH */
        [880U] = MYLITE_SQL_PARSE_WORK,                /* WORK */
        [882U] = MYLITE_SQL_PARSE_WRITE,               /* WRITE */
        [887U] = MYLITE_SQL_PARSE_XOR,                 /* XOR */
        [888U] = MYLITE_SQL_PARSE_YEAR,                /* YEAR */
        [889U] = MYLITE_SQL_PARSE_YEARWEEK,            /* YEARWEEK */
        [890U] = MYLITE_SQL_PARSE_YEAR_MONTH,          /* YEAR_MONTH */
        [894U] = MYLITE_SQL_PARSE_JSON_DEPTH,          /* JSON_DEPTH */
        [895U] = MYLITE_SQL_PARSE_JSON_PRETTY,         /* JSON_PRETTY */
        [896U] = MYLITE_SQL_PARSE_JSON_OVERLAPS,       /* JSON_OVERLAPS */
        [897U] = MYLITE_SQL_PARSE_JSON_STORAGE_SIZE,   /* JSON_STORAGE_SIZE */
        [898U] = MYLITE_SQL_PARSE_JSON_STORAGE_FREE,   /* JSON_STORAGE_FREE */
        [899U] = MYLITE_SQL_PARSE_JSON_ARRAY_APPEND,   /* JSON_ARRAY_APPEND */
        [900U] = MYLITE_SQL_PARSE_JSON_ARRAY_INSERT,   /* JSON_ARRAY_INSERT */
        [901U] = MYLITE_SQL_PARSE_JSON_MERGE,          /* JSON_MERGE */
        [902U] = MYLITE_SQL_PARSE_JSON_MERGE_PATCH,    /* JSON_MERGE_PATCH */
        [903U] = MYLITE_SQL_PARSE_JSON_MERGE_PRESERVE, /* JSON_MERGE_PRESERVE */
        [904U] = MYLITE_SQL_PARSE_JSON_SEARCH,         /* JSON_SEARCH */
    };
    if (keyword_index < sizeof(keyword_parser_tokens) / sizeof(keyword_parser_tokens[0]) &&
        keyword_parser_tokens[keyword_index] != 0) {
        *out_parser_token = keyword_parser_tokens[keyword_index];
        return true;
    }

    return false;
}

// NOLINTEND(readability-magic-numbers)
static bool map_punctuation_token(const struct mylite_sql_token *token, int *out_parser_token) {
    if (token->length != 1U) {
        return false;
    }

    switch (token->text[0]) {
    case ';':
        *out_parser_token = MYLITE_SQL_PARSE_SEMICOLON;
        return true;
    case ',':
        *out_parser_token = MYLITE_SQL_PARSE_COMMA;
        return true;
    case '.':
        *out_parser_token = MYLITE_SQL_PARSE_DOT;
        return true;
    case '(':
        *out_parser_token = MYLITE_SQL_PARSE_LPAREN;
        return true;
    case ')':
        *out_parser_token = MYLITE_SQL_PARSE_RPAREN;
        return true;
    default:
        return false;
    }
}

static bool map_operator_token(
    const struct mylite_sql_parser_state *state,
    const struct mylite_sql_token *token,
    int *out_parser_token
) {
    switch (token->operator_kind) {
    case MYLITE_SQL_OPERATOR_NULL_SAFE_EQUAL:
        *out_parser_token = MYLITE_SQL_PARSE_NULL_SAFE_EQUAL;
        return true;
    case MYLITE_SQL_OPERATOR_LESS_EQUAL:
        *out_parser_token = MYLITE_SQL_PARSE_LESS_EQUAL;
        return true;
    case MYLITE_SQL_OPERATOR_GREATER_EQUAL:
        *out_parser_token = MYLITE_SQL_PARSE_GREATER_EQUAL;
        return true;
    case MYLITE_SQL_OPERATOR_NOT_EQUAL:
        *out_parser_token = MYLITE_SQL_PARSE_NOT_EQUAL;
        return true;
    case MYLITE_SQL_OPERATOR_EQUAL:
        *out_parser_token = MYLITE_SQL_PARSE_EQUAL;
        return true;
    case MYLITE_SQL_OPERATOR_LESS:
        *out_parser_token = MYLITE_SQL_PARSE_LESS;
        return true;
    case MYLITE_SQL_OPERATOR_GREATER:
        *out_parser_token = MYLITE_SQL_PARSE_GREATER;
        return true;
    case MYLITE_SQL_OPERATOR_PLUS:
        *out_parser_token = MYLITE_SQL_PARSE_PLUS;
        return true;
    case MYLITE_SQL_OPERATOR_MINUS:
        *out_parser_token = MYLITE_SQL_PARSE_MINUS;
        return true;
    case MYLITE_SQL_OPERATOR_STAR:
        *out_parser_token = MYLITE_SQL_PARSE_STAR;
        return true;
    case MYLITE_SQL_OPERATOR_SLASH:
        *out_parser_token = MYLITE_SQL_PARSE_SLASH;
        return true;
    case MYLITE_SQL_OPERATOR_PERCENT:
        *out_parser_token = MYLITE_SQL_PARSE_PERCENT;
        return true;
    case MYLITE_SQL_OPERATOR_NOT:
        *out_parser_token = MYLITE_SQL_PARSE_LOGICAL_NOT;
        return true;
    case MYLITE_SQL_OPERATOR_LOGICAL_AND:
        *out_parser_token = MYLITE_SQL_PARSE_LOGICAL_AND;
        return true;
    case MYLITE_SQL_OPERATOR_LEFT_SHIFT:
        *out_parser_token = MYLITE_SQL_PARSE_LEFT_SHIFT;
        return true;
    case MYLITE_SQL_OPERATOR_RIGHT_SHIFT:
        *out_parser_token = MYLITE_SQL_PARSE_RIGHT_SHIFT;
        return true;
    case MYLITE_SQL_OPERATOR_BITWISE_NOT:
        *out_parser_token = MYLITE_SQL_PARSE_BITWISE_NOT;
        return true;
    case MYLITE_SQL_OPERATOR_BITWISE_XOR:
        *out_parser_token = MYLITE_SQL_PARSE_BITWISE_XOR;
        return true;
    case MYLITE_SQL_OPERATOR_BITWISE_AND:
        *out_parser_token = MYLITE_SQL_PARSE_BITWISE_AND;
        return true;
    case MYLITE_SQL_OPERATOR_BITWISE_OR:
        *out_parser_token = MYLITE_SQL_PARSE_BITWISE_OR;
        return true;
    case MYLITE_SQL_OPERATOR_JSON_UNQUOTE_EXTRACT:
        *out_parser_token = MYLITE_SQL_PARSE_JSON_UNQUOTE_EXTRACT_OPERATOR;
        return true;
    case MYLITE_SQL_OPERATOR_JSON_EXTRACT:
        *out_parser_token = MYLITE_SQL_PARSE_JSON_EXTRACT_OPERATOR;
        return true;
    case MYLITE_SQL_OPERATOR_ASSIGN:
        *out_parser_token = MYLITE_SQL_PARSE_ASSIGN;
        return true;
    case MYLITE_SQL_OPERATOR_LOGICAL_OR:
        if (mylite_sql_parser_sql_mode_has(state, MYLITE_SQL_MODE_PIPES_AS_CONCAT)) {
            *out_parser_token = MYLITE_SQL_PARSE_CONCAT_OPERATOR;
        } else {
            *out_parser_token = MYLITE_SQL_PARSE_LOGICAL_OR;
        }
        return true;
    default:
        return false;
    }

    return false;
}

static bool previous_token_allows_select_noop_modifier(int previous_parser_token) {
    switch (previous_parser_token) {
    case MYLITE_SQL_PARSE_SELECT:
    case MYLITE_SQL_PARSE_ALL:
    case MYLITE_SQL_PARSE_DISTINCT:
    case MYLITE_SQL_PARSE_DISTINCTROW:
    case MYLITE_SQL_PARSE_HIGH_PRIORITY:
    case MYLITE_SQL_PARSE_STRAIGHT_JOIN:
    case MYLITE_SQL_PARSE_SQL_SMALL_RESULT:
    case MYLITE_SQL_PARSE_SQL_BIG_RESULT:
    case MYLITE_SQL_PARSE_SQL_BUFFER_RESULT:
    case MYLITE_SQL_PARSE_SQL_NO_CACHE:
    case MYLITE_SQL_PARSE_SQL_CALC_FOUND_ROWS:
        return true;
    default:
        return false;
    }
}

static bool previous_token_allows_delete_quick_modifier(
    int previous_parser_token,
    int token_before_previous_parser_token
) {
    return previous_parser_token == MYLITE_SQL_PARSE_DELETE ||
           (previous_parser_token == MYLITE_SQL_PARSE_LOW_PRIORITY &&
            token_before_previous_parser_token == MYLITE_SQL_PARSE_DELETE);
}

static bool token_text_matches_keyword_mapping(
    const struct mylite_sql_token *token,
    const char *text,
    unsigned char token_first,
    size_t token_length
) {
    if (token == NULL || text == NULL || (unsigned char)text[0] != token_first ||
        strlen(text) != token_length) {
        return false;
    }

    for (size_t index = 1U; index < token_length; ++index) {
        if (mylite_sql_parser_ascii_upper((unsigned char)token->text[index]) != text[index]) {
            return false;
        }
    }

    return true;
}
