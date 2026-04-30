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
	{ "ALTER", ALTER_T },
	{ "ANALYZE", ANALYZE_T },
	{ "BEGIN", BEGIN_T },
	{ "BINLOG", BINLOG_T },
	{ "CACHE", CACHE_T },
	{ "CALL", CALL_T },
	{ "CASE", CASE_T },
	{ "CHANGE", CHANGE_T },
	{ "CHECK", CHECK_T },
	{ "CHECKSUM", CHECKSUM_T },
	{ "COMMIT", COMMIT_T },
	{ "CREATE", CREATE_T },
	{ "DEALLOCATE", DEALLOCATE_T },
	{ "DELETE", DELETE_T },
	{ "DESC", DESC_T },
	{ "DESCRIBE", DESCRIBE_T },
	{ "DO", DO_T },
	{ "DROP", DROP_T },
	{ "ELSE", ELSE_T },
	{ "ELSEIF", ELSEIF_T },
	{ "END", END_T },
	{ "EXECUTE", EXECUTE_T },
	{ "EXPLAIN", EXPLAIN_T },
	{ "FLUSH", FLUSH_T },
	{ "GET", GET_T },
	{ "GRANT", GRANT_T },
	{ "HANDLER", HANDLER_T },
	{ "HELP", HELP_T },
	{ "IF", IF_T },
	{ "IMPORT", IMPORT_T },
	{ "INSERT", INSERT_T },
	{ "INSTALL", INSTALL_T },
	{ "KILL", KILL_T },
	{ "LOAD", LOAD_T },
	{ "LOCK", LOCK_T },
	{ "LOOP", LOOP_T },
	{ "OPTIMIZE", OPTIMIZE_T },
	{ "PREPARE", PREPARE_T },
	{ "PURGE", PURGE_T },
	{ "RELEASE", RELEASE_T },
	{ "RENAME", RENAME_T },
	{ "REPAIR", REPAIR_T },
	{ "REPEAT", REPEAT_T },
	{ "REPLACE", REPLACE_T },
	{ "RESET", RESET_T },
	{ "RESIGNAL", RESIGNAL_T },
	{ "RESTART", RESTART_T },
	{ "REVOKE", REVOKE_T },
	{ "ROLLBACK", ROLLBACK_T },
	{ "SAVEPOINT", SAVEPOINT_T },
	{ "SELECT", SELECT_T },
	{ "SET", SET_T },
	{ "SHOW", SHOW_T },
	{ "SHUTDOWN", SHUTDOWN_T },
	{ "SIGNAL", SIGNAL_T },
	{ "START", START_T },
	{ "TABLE", TABLE_T },
	{ "THEN", THEN_T },
	{ "TRUNCATE", TRUNCATE_T },
	{ "UNINSTALL", UNINSTALL_T },
	{ "UNLOCK", UNLOCK_T },
	{ "UNTIL", UNTIL_T },
	{ "UPDATE", UPDATE_T },
	{ "USE", USE_T },
	{ "VALUES", VALUES_T },
	{ "WHILE", WHILE_T },
	{ "WITH", WITH_T },
	{ "XA", XA_T }
};

static void lexer_set_error(mylite_parser *parser, const char *message);
static int next_token(mylite_parser *parser);
static int skip_space_and_comments(mylite_parser *parser);
static void enter_executable_comment(mylite_lexer *lexer);
static int skip_block_comment(mylite_parser *parser);
static int skip_line_comment(mylite_parser *parser);
static int read_word(mylite_parser *parser);
static int read_end_compound(mylite_parser *parser, int fallback);
static int read_quoted_identifier(mylite_parser *parser);
static int read_string(mylite_parser *parser, int quote);
static int read_number(mylite_parser *parser);
static int read_variable(mylite_parser *parser);
static int read_operator_or_punctuation(mylite_parser *parser);
static int lookup_keyword(const char *start, size_t length);
static int is_word_start(unsigned char ch);
static int is_word_part(unsigned char ch);
static int is_operator_char(unsigned char ch);
static int peek_keyword_after_space(mylite_lexer *lexer, const char *word, size_t length, size_t *end_offset);
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
	if (is_word_start(ch)) {
		return read_word(parser);
	}
	if (isdigit(ch) || (ch == '.' && isdigit(peek_char(lexer, 1)))) {
		return read_number(parser);
	}
	if (ch == '`') {
		return read_quoted_identifier(parser);
	}
	if (ch == '\'' || ch == '"') {
		return read_string(parser, ch);
	}
	if (ch == '@') {
		return read_variable(parser);
	}
	if (ch == '?') {
		advance_byte(lexer);
		return PARAM;
	}
	return read_operator_or_punctuation(parser);
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

static int read_word(mylite_parser *parser)
{
	mylite_lexer *lexer = &parser->lexer;
	size_t start = lexer->offset;
	int token;

	while (is_word_part(current_char(lexer))) {
		advance_byte(lexer);
	}
	token = lookup_keyword(lexer->input + start, lexer->offset - start);
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
		lexer->offset = end_offset;
		return END_IF_T;
	}
	if (peek_keyword_after_space(lexer, "LOOP", 4, &end_offset)) {
		lexer->offset = end_offset;
		return END_LOOP_T;
	}
	if (peek_keyword_after_space(lexer, "REPEAT", 6, &end_offset)) {
		lexer->offset = end_offset;
		return END_REPEAT_T;
	}
	if (peek_keyword_after_space(lexer, "WHILE", 5, &end_offset)) {
		lexer->offset = end_offset;
		return END_WHILE_T;
	}
	if (peek_keyword_after_space(lexer, "CASE", 4, &end_offset)) {
		lexer->offset = end_offset;
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
