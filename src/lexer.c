#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "lexer.h"
#include "parser_bison.h"

typedef struct keyword {
	const char *word;
	int token;
} keyword;

static const keyword keywords[] = {
	{ "ALL", ALL_T },
	{ "ALTER", ALTER_T },
	{ "ANALYZE", ANALYZE_T },
	{ "AND", AND_T },
	{ "AS", AS_T },
	{ "AUTO_INCREMENT", AUTO_INCREMENT_T },
	{ "BEGIN", BEGIN_T },
	{ "BETWEEN", BETWEEN_T },
	{ "BINLOG", BINLOG_T },
	{ "BY", BY_T },
	{ "CACHE", CACHE_T },
	{ "CALL", CALL_T },
	{ "CASE", CASE_T },
	{ "CHAIN", CHAIN_T },
	{ "CHANGE", CHANGE_T },
	{ "CHARACTER", CHARACTER_T },
	{ "CHARSET", CHARSET_T },
	{ "CHECK", CHECK_T },
	{ "CHECKSUM", CHECKSUM_T },
	{ "CLONE", CLONE_T },
	{ "CLOSE", CLOSE_T },
	{ "COLLATE", COLLATE_T },
	{ "COLUMNS", COLUMNS_T },
	{ "COMMIT", COMMIT_T },
	{ "CONSTRAINT", CONSTRAINT_T },
	{ "CREATE", CREATE_T },
	{ "CROSS", CROSS_T },
	{ "CURSOR", CURSOR_T },
	{ "DATA", DATA_T },
	{ "DATABASE", DATABASE_T },
	{ "DEALLOCATE", DEALLOCATE_T },
	{ "DECLARE", DECLARE_T },
	{ "DEFAULT", DEFAULT_T },
	{ "DELAYED", DELAYED_T },
	{ "DELETE", DELETE_T },
	{ "DESC", DESC_T },
	{ "DESCRIBE", DESCRIBE_T },
	{ "DISTINCT", DISTINCT_T },
	{ "DO", DO_T },
	{ "DROP", DROP_T },
	{ "ELSE", ELSE_T },
	{ "ELSEIF", ELSEIF_T },
	{ "END", END_T },
	{ "ENGINE", ENGINE_T },
	{ "EVENT", EVENT_T },
	{ "EXECUTE", EXECUTE_T },
	{ "EXISTS", EXISTS_T },
	{ "EXPLAIN", EXPLAIN_T },
	{ "FALSE", FALSE_T },
	{ "FETCH", FETCH_T },
	{ "FIELDS", FIELDS_T },
	{ "FLUSH", FLUSH_T },
	{ "FORMAT", FORMAT_T },
	{ "FROM", FROM_T },
	{ "FULL", FULL_T },
	{ "FUNCTION", FUNCTION_T },
	{ "GET", GET_T },
	{ "GRANT", GRANT_T },
	{ "GROUP", GROUP_T },
	{ "HANDLER", HANDLER_T },
	{ "HAVING", HAVING_T },
	{ "HELP", HELP_T },
	{ "HIGH_PRIORITY", HIGH_PRIORITY_T },
	{ "IF", IF_T },
	{ "IGNORE", IGNORE_T },
	{ "IMPORT", IMPORT_T },
	{ "IN", IN_T },
	{ "INDEX", INDEX_T },
	{ "INFILE", INFILE_T },
	{ "INNER", INNER_T },
	{ "INSERT", INSERT_T },
	{ "INSTALL", INSTALL_T },
	{ "INTO", INTO_T },
	{ "IS", IS_T },
	{ "ITERATE", ITERATE_T },
	{ "JOIN", JOIN_T },
	{ "JSON", JSON_T },
	{ "KEY", KEY_T },
	{ "KILL", KILL_T },
	{ "LEAVE", LEAVE_T },
	{ "LEFT", LEFT_T },
	{ "LIKE", LIKE_T },
	{ "LIMIT", LIMIT_T },
	{ "LOAD", LOAD_T },
	{ "LOCAL", LOCAL_T },
	{ "LOCK", LOCK_T },
	{ "LOOP", LOOP_T },
	{ "LOW_PRIORITY", LOW_PRIORITY_T },
	{ "NATURAL", NATURAL_T },
	{ "NO", NO_T },
	{ "NOT", NOT_T },
	{ "NULL", NULL_T },
	{ "OFFSET", OFFSET_T },
	{ "ON", ON_T },
	{ "OPEN", OPEN_T },
	{ "OPTIMIZE", OPTIMIZE_T },
	{ "OR", OR_T },
	{ "ORDER", ORDER_T },
	{ "OUTER", OUTER_T },
	{ "PREPARE", PREPARE_T },
	{ "PRIMARY", PRIMARY_T },
	{ "PROCEDURE", PROCEDURE_T },
	{ "PURGE", PURGE_T },
	{ "QUICK", QUICK_T },
	{ "READ", READ_T },
	{ "REFERENCES", REFERENCES_T },
	{ "REGEXP", REGEXP_T },
	{ "RELEASE", RELEASE_T },
	{ "RENAME", RENAME_T },
	{ "REPAIR", REPAIR_T },
	{ "REPEAT", REPEAT_T },
	{ "REPLACE", REPLACE_T },
	{ "RESET", RESET_T },
	{ "RESIGNAL", RESIGNAL_T },
	{ "RESTART", RESTART_T },
	{ "RETURN", RETURN_T },
	{ "REVOKE", REVOKE_T },
	{ "RIGHT", RIGHT_T },
	{ "RLIKE", RLIKE_T },
	{ "ROLE", ROLE_T },
	{ "ROLLBACK", ROLLBACK_T },
	{ "SAVEPOINT", SAVEPOINT_T },
	{ "SCHEMA", SCHEMA_T },
	{ "SELECT", SELECT_T },
	{ "SET", SET_T },
	{ "SHOW", SHOW_T },
	{ "SHUTDOWN", SHUTDOWN_T },
	{ "SIGNAL", SIGNAL_T },
	{ "SPATIAL", SPATIAL_T },
	{ "START", START_T },
	{ "STOP", STOP_T },
	{ "TABLE", TABLE_T },
	{ "TEMPORARY", TEMPORARY_T },
	{ "THEN", THEN_T },
	{ "TO", TO_T },
	{ "TRANSACTION", TRANSACTION_T },
	{ "TRIGGER", TRIGGER_T },
	{ "TRUE", TRUE_T },
	{ "TRUNCATE", TRUNCATE_T },
	{ "UNINSTALL", UNINSTALL_T },
	{ "UNION", UNION_T },
	{ "UNIQUE", UNIQUE_T },
	{ "UNLOCK", UNLOCK_T },
	{ "UNTIL", UNTIL_T },
	{ "UPDATE", UPDATE_T },
	{ "USE", USE_T },
	{ "USER", USER_T },
	{ "USING", USING_T },
	{ "VALUE", VALUE_T },
	{ "VALUES", VALUES_T },
	{ "VIEW", VIEW_T },
	{ "WHEN", WHEN_T },
	{ "WHERE", WHERE_T },
	{ "WHILE", WHILE_T },
	{ "WITH", WITH_T },
	{ "WRITE", WRITE_T },
	{ "XA", XA_T },
	{ "XOR", XOR_T }
};

static void lexer_set_error(mylite_parser *parser, const char *message);
static int next_token(mylite_parser *parser);
static int finish_token(mylite_lexer *lexer, int token);
static int skip_space_and_comments(mylite_parser *parser);
static void enter_executable_comment(mylite_lexer *lexer);
static int skip_block_comment(mylite_parser *parser);
static int skip_line_comment(mylite_parser *parser);
static int is_prefixed_literal_start(const mylite_lexer *lexer);
static int read_prefixed_literal(mylite_parser *parser);
static int read_word(mylite_parser *parser);
static int read_end_compound(mylite_parser *parser, int fallback);
static int read_quoted_identifier(mylite_parser *parser);
static int read_string(mylite_parser *parser, int quote);
static int read_number(mylite_parser *parser);
static int read_variable(mylite_parser *parser);
static int read_operator_or_punctuation(mylite_parser *parser);
static int lookup_keyword(const char *start, size_t length);
static int previous_token_allows_keyword_identifier(const mylite_parser *parser);
static int is_word_start(unsigned char ch);
static int is_word_part(unsigned char ch);
static int is_operator_char(unsigned char ch);
static int peek_keyword_after_space(mylite_lexer *lexer, const char *word, size_t length, size_t *end_offset);
static void advance_to_offset(mylite_lexer *lexer, size_t end_offset);
static void advance_byte(mylite_lexer *lexer);
static unsigned char current_char(const mylite_lexer *lexer);
static unsigned char peek_char(const mylite_lexer *lexer, size_t lookahead);

void mylite_lexer_init(mylite_lexer *lexer, const char *input, size_t length)
{
	lexer->input = input;
	lexer->length = length;
	lexer->offset = 0;
	lexer->line = 1;
	lexer->column = 1;
	lexer->token_count = 0;
	lexer->token_start_offset = 0;
	lexer->token_end_offset = 0;
	lexer->token_start_line = 1;
	lexer->token_start_column = 1;
	lexer->token_end_line = 1;
	lexer->token_end_column = 1;
	lexer->last_significant_token = 0;
	lexer->last_significant_token_end_offset = 0;
	lexer->last_significant_token_end_line = 1;
	lexer->last_significant_token_end_column = 1;
	lexer->executable_comment = 0;
	lexer->error[0] = '\0';
	lexer->error_line = 1;
	lexer->error_column = 1;
}

int mylite_lexer_next(mylite_parser *parser)
{
	int token = next_token(parser);
	if (token > 0) {
		parser->lexer.token_count++;
		if (token != ';') {
			parser->lexer.last_significant_token = parser->lexer.token_count;
			parser->lexer.last_significant_token_end_offset = parser->lexer.token_end_offset;
			parser->lexer.last_significant_token_end_line = parser->lexer.token_end_line;
			parser->lexer.last_significant_token_end_column = parser->lexer.token_end_column;
		}
		if (!mylite_parser_record_token(parser, token)) {
			return 0;
		}
	}
	return token;
}

static void lexer_set_error(mylite_parser *parser, const char *message)
{
	mylite_lexer *lexer = &parser->lexer;
	if (lexer->error[0] == '\0') {
		snprintf(lexer->error, sizeof(lexer->error), "%s", message);
		lexer->error_line = lexer->line;
		lexer->error_column = lexer->column;
	}
}

static int next_token(mylite_parser *parser)
{
	mylite_lexer *lexer = &parser->lexer;
	unsigned char ch;

	if (!skip_space_and_comments(parser)) {
		return 0;
	}

	ch = current_char(lexer);
	if (ch == '\0') {
		if (lexer->executable_comment) {
			lexer_set_error(parser, "unterminated executable comment");
		}
		return 0;
	}
	lexer->token_start_offset = lexer->offset;
	lexer->token_start_line = lexer->line;
	lexer->token_start_column = lexer->column;
	if (is_prefixed_literal_start(lexer)) {
		return finish_token(lexer, read_prefixed_literal(parser));
	}
	if (is_word_start(ch)) {
		return finish_token(lexer, read_word(parser));
	}
	if (isdigit(ch) || (ch == '.' && isdigit(peek_char(lexer, 1)))) {
		return finish_token(lexer, read_number(parser));
	}
	if (ch == '`') {
		return finish_token(lexer, read_quoted_identifier(parser));
	}
	if (ch == '\'' || ch == '"') {
		return finish_token(lexer, read_string(parser, ch));
	}
	if (ch == '@') {
		return finish_token(lexer, read_variable(parser));
	}
	if (ch == '?') {
		advance_byte(lexer);
		return finish_token(lexer, PARAM);
	}
	return finish_token(lexer, read_operator_or_punctuation(parser));
}

static int finish_token(mylite_lexer *lexer, int token)
{
	if (token <= 0) {
		return token;
	}
	lexer->token_end_offset = lexer->offset;
	lexer->token_end_line = lexer->line;
	lexer->token_end_column = lexer->column;
	return token;
}

static int skip_space_and_comments(mylite_parser *parser)
{
	mylite_lexer *lexer = &parser->lexer;
	int advanced = 1;

	while (advanced) {
		advanced = 0;
		while (isspace(current_char(lexer))) {
			advance_byte(lexer);
			advanced = 1;
		}
		if (lexer->executable_comment && current_char(lexer) == '*' && peek_char(lexer, 1) == '/') {
			advance_byte(lexer);
			advance_byte(lexer);
			lexer->executable_comment = 0;
			advanced = 1;
			continue;
		}
		if (current_char(lexer) == '#' && skip_line_comment(parser)) {
			advanced = 1;
			continue;
		}
		if (current_char(lexer) == '-' && peek_char(lexer, 1) == '-' &&
		    (isspace(peek_char(lexer, 2)) || peek_char(lexer, 2) == '\0')) {
			if (!skip_line_comment(parser)) {
				return 0;
			}
			advanced = 1;
			continue;
		}
		if (current_char(lexer) == '/' && peek_char(lexer, 1) == '*') {
			if (peek_char(lexer, 2) == '!') {
				enter_executable_comment(lexer);
				advanced = 1;
				continue;
			}
			if (!skip_block_comment(parser)) {
				return 0;
			}
			advanced = 1;
		}
	}
	return lexer->error[0] == '\0';
}

static void enter_executable_comment(mylite_lexer *lexer)
{
	advance_byte(lexer);
	advance_byte(lexer);
	advance_byte(lexer);
	while (isdigit(current_char(lexer))) {
		advance_byte(lexer);
	}
	lexer->executable_comment = 1;
}

static int skip_block_comment(mylite_parser *parser)
{
	mylite_lexer *lexer = &parser->lexer;

	advance_byte(lexer);
	advance_byte(lexer);
	while (current_char(lexer) != '\0') {
		if (current_char(lexer) == '*' && peek_char(lexer, 1) == '/') {
			advance_byte(lexer);
			advance_byte(lexer);
			return 1;
		}
		advance_byte(lexer);
	}
	lexer_set_error(parser, "unterminated block comment");
	return 0;
}

static int skip_line_comment(mylite_parser *parser)
{
	mylite_lexer *lexer = &parser->lexer;

	while (current_char(lexer) != '\0' && current_char(lexer) != '\n') {
		advance_byte(lexer);
	}
	return 1;
}

static int is_prefixed_literal_start(const mylite_lexer *lexer)
{
	unsigned char ch = current_char(lexer);
	size_t offset;

	if ((ch == 'N' || ch == 'n' || ch == 'X' || ch == 'x' || ch == 'B' || ch == 'b') &&
	    peek_char(lexer, 1) == '\'') {
		return 1;
	}
	if (ch != '_') {
		return 0;
	}

	offset = lexer->offset + 1;
	if (offset >= lexer->length || !is_word_part((unsigned char)lexer->input[offset])) {
		return 0;
	}
	while (offset < lexer->length && is_word_part((unsigned char)lexer->input[offset])) {
		offset++;
	}
	return offset < lexer->length && lexer->input[offset] == '\'';
}

static int read_prefixed_literal(mylite_parser *parser)
{
	mylite_lexer *lexer = &parser->lexer;
	unsigned char prefix = current_char(lexer);
	int token = STRING;

	if (prefix == 'X' || prefix == 'x' || prefix == 'B' || prefix == 'b') {
		token = NUMBER;
	}

	if (prefix == '_') {
		while (is_word_part(current_char(lexer))) {
			advance_byte(lexer);
		}
	} else {
		advance_byte(lexer);
	}

	if (read_string(parser, '\'') == 0) {
		return 0;
	}
	return token;
}

static int read_word(mylite_parser *parser)
{
	mylite_lexer *lexer = &parser->lexer;
	size_t start = lexer->offset;
	int token;

	while (is_word_part(current_char(lexer))) {
		advance_byte(lexer);
	}
	token = lookup_keyword(lexer->input + start, lexer->offset - start);
	if ((token == BEGIN_T || token == END_T) &&
	    previous_token_allows_keyword_identifier(parser)) {
		return IDENT;
	}
	if (token == END_T) {
		return read_end_compound(parser, token);
	}
	return token;
}

static int read_end_compound(mylite_parser *parser, int fallback)
{
	mylite_lexer *lexer = &parser->lexer;
	size_t end_offset = lexer->offset;

	if (peek_keyword_after_space(lexer, "IF", 2, &end_offset)) {
		advance_to_offset(lexer, end_offset);
		return END_IF_T;
	}
	if (peek_keyword_after_space(lexer, "LOOP", 4, &end_offset)) {
		advance_to_offset(lexer, end_offset);
		return END_LOOP_T;
	}
	if (peek_keyword_after_space(lexer, "REPEAT", 6, &end_offset)) {
		advance_to_offset(lexer, end_offset);
		return END_REPEAT_T;
	}
	if (peek_keyword_after_space(lexer, "WHILE", 5, &end_offset)) {
		advance_to_offset(lexer, end_offset);
		return END_WHILE_T;
	}
	if (peek_keyword_after_space(lexer, "CASE", 4, &end_offset)) {
		advance_to_offset(lexer, end_offset);
		return END_CASE_T;
	}
	return fallback;
}

static int read_quoted_identifier(mylite_parser *parser)
{
	mylite_lexer *lexer = &parser->lexer;

	advance_byte(lexer);
	while (current_char(lexer) != '\0') {
		if (current_char(lexer) == '`') {
			advance_byte(lexer);
			if (current_char(lexer) != '`') {
				return QUOTED_IDENT;
			}
		}
		advance_byte(lexer);
	}
	lexer_set_error(parser, "unterminated quoted identifier");
	return 0;
}

static int read_string(mylite_parser *parser, int quote)
{
	mylite_lexer *lexer = &parser->lexer;

	advance_byte(lexer);
	while (current_char(lexer) != '\0') {
		if (current_char(lexer) == '\\') {
			advance_byte(lexer);
			if (current_char(lexer) != '\0') {
				advance_byte(lexer);
			}
			continue;
		}
		if (current_char(lexer) == quote) {
			advance_byte(lexer);
			if (current_char(lexer) != quote) {
				return STRING;
			}
		}
		advance_byte(lexer);
	}
	lexer_set_error(parser, "unterminated string literal");
	return 0;
}

static int read_number(mylite_parser *parser)
{
	mylite_lexer *lexer = &parser->lexer;

	if (current_char(lexer) == '0' && (peek_char(lexer, 1) == 'x' || peek_char(lexer, 1) == 'X')) {
		advance_byte(lexer);
		advance_byte(lexer);
		while (isxdigit(current_char(lexer))) {
			advance_byte(lexer);
		}
		return NUMBER;
	}
	if (current_char(lexer) == '0' && (peek_char(lexer, 1) == 'b' || peek_char(lexer, 1) == 'B')) {
		advance_byte(lexer);
		advance_byte(lexer);
		while (current_char(lexer) == '0' || current_char(lexer) == '1') {
			advance_byte(lexer);
		}
		return NUMBER;
	}

	while (isdigit(current_char(lexer))) {
		advance_byte(lexer);
	}
	if (current_char(lexer) == '.') {
		advance_byte(lexer);
		while (isdigit(current_char(lexer))) {
			advance_byte(lexer);
		}
	}
	if (current_char(lexer) == 'e' || current_char(lexer) == 'E') {
		advance_byte(lexer);
		if (current_char(lexer) == '+' || current_char(lexer) == '-') {
			advance_byte(lexer);
		}
		while (isdigit(current_char(lexer))) {
			advance_byte(lexer);
		}
	}
	return NUMBER;
}

static int read_variable(mylite_parser *parser)
{
	mylite_lexer *lexer = &parser->lexer;
	int system_variable = 0;

	advance_byte(lexer);
	if (current_char(lexer) == '@') {
		system_variable = 1;
		advance_byte(lexer);
	}
	while (is_word_part(current_char(lexer)) || current_char(lexer) == '.' || current_char(lexer) == '$') {
		advance_byte(lexer);
	}
	return system_variable ? SYSTEM_VARIABLE : USER_VARIABLE;
}

static int read_operator_or_punctuation(mylite_parser *parser)
{
	mylite_lexer *lexer = &parser->lexer;
	unsigned char ch = current_char(lexer);

	if (ch == '(' || ch == ')' || ch == '[' || ch == ']' || ch == '{' || ch == '}' ||
	    ch == ',' || ch == ';' || ch == '.' || ch == '*' || ch == '+' || ch == '-' ||
	    ch == '/' || ch == '%') {
		advance_byte(lexer);
		return ch;
	}

	if (is_operator_char(ch)) {
		while (is_operator_char(current_char(lexer)) &&
		       current_char(lexer) != '(' && current_char(lexer) != ')' &&
		       current_char(lexer) != '[' && current_char(lexer) != ']' &&
		       current_char(lexer) != '{' && current_char(lexer) != '}' &&
		       current_char(lexer) != ',' && current_char(lexer) != ';') {
			advance_byte(lexer);
		}
		return OPERATOR;
	}

	advance_byte(lexer);
	return IDENT;
}

static int lookup_keyword(const char *start, size_t length)
{
	size_t left = 0;
	size_t right = sizeof(keywords) / sizeof(keywords[0]);
	char upper[32];
	size_t i;

	if (length >= sizeof(upper)) {
		return IDENT;
	}
	for (i = 0; i < length; i++) {
		upper[i] = (char)toupper((unsigned char)start[i]);
	}
	upper[length] = '\0';

	while (left < right) {
		size_t mid = left + (right - left) / 2;
		int cmp = strcmp(upper, keywords[mid].word);
		if (cmp == 0) {
			return keywords[mid].token;
		}
		if (cmp < 0) {
			right = mid;
		} else {
			left = mid + 1;
		}
	}
	return IDENT;
}

static int previous_token_allows_keyword_identifier(const mylite_parser *parser)
{
	const mylite_lexer *lexer = &parser->lexer;
	int previous_token;

	if (lexer->last_significant_token == 0 ||
	    lexer->last_significant_token > parser->token_count) {
		return 0;
	}

	previous_token = parser->tokens[lexer->last_significant_token - 1].parser_token;
	switch (previous_token) {
	case '(':
	case ',':
	case '.':
	case DATABASE_T:
	case EVENT_T:
	case FUNCTION_T:
	case INDEX_T:
	case INTO_T:
	case KEY_T:
	case PROCEDURE_T:
	case ROLE_T:
	case SCHEMA_T:
	case TABLE_T:
	case TO_T:
	case TRIGGER_T:
	case USER_T:
	case VIEW_T:
		return 1;
	default:
		return 0;
	}
}

static int is_word_start(unsigned char ch)
{
	return isalpha(ch) || ch == '_' || ch == '$' || ch >= 128;
}

static int is_word_part(unsigned char ch)
{
	return isalnum(ch) || ch == '_' || ch == '$' || ch >= 128;
}

static int is_operator_char(unsigned char ch)
{
	return strchr("!<>=|&^~:.-+*/%", ch) != NULL;
}

static int peek_keyword_after_space(mylite_lexer *lexer, const char *word, size_t length, size_t *end_offset)
{
	size_t offset = lexer->offset;
	size_t i;

	while (offset < lexer->length && isspace((unsigned char)lexer->input[offset])) {
		offset++;
	}
	for (i = 0; i < length; i++) {
		if (offset + i >= lexer->length ||
		    toupper((unsigned char)lexer->input[offset + i]) != word[i]) {
			return 0;
		}
	}
	if (offset + length < lexer->length &&
	    is_word_part((unsigned char)lexer->input[offset + length])) {
		return 0;
	}
	*end_offset = offset + length;
	return 1;
}

static void advance_to_offset(mylite_lexer *lexer, size_t end_offset)
{
	while (lexer->offset < end_offset) {
		advance_byte(lexer);
	}
}

static void advance_byte(mylite_lexer *lexer)
{
	if (lexer->offset >= lexer->length) {
		return;
	}
	if (lexer->input[lexer->offset] == '\n') {
		lexer->line++;
		lexer->column = 1;
	} else {
		lexer->column++;
	}
	lexer->offset++;
}

static unsigned char current_char(const mylite_lexer *lexer)
{
	if (lexer->offset >= lexer->length) {
		return '\0';
	}
	return (unsigned char)lexer->input[lexer->offset];
}

static unsigned char peek_char(const mylite_lexer *lexer, size_t lookahead)
{
	if (lexer->offset + lookahead >= lexer->length) {
		return '\0';
	}
	return (unsigned char)lexer->input[lexer->offset + lookahead];
}
