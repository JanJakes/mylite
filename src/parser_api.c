#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "lexer.h"
#include "parser_bison.h"
#include "parser_internal.h"

int yyparse(mylite_parser *parser);

static void classify_statement_objects(mylite_parser *parser);
static void classify_statement_object(const mylite_parser *parser, mylite_statement *statement);
static mylite_statement_object_kind object_kind_from_token_sequence(const mylite_parser *parser,
                                                                    size_t token_index,
                                                                    size_t last_token_index);
static void set_statement_object_name(const mylite_parser *parser,
                                      mylite_statement *statement,
                                      size_t object_token_index,
                                      size_t last_token_index);
static size_t first_name_token_after_object(const mylite_parser *parser,
                                            size_t object_token_index,
                                            size_t last_token_index);
static size_t last_qualified_name_token(const mylite_parser *parser,
                                        size_t first_name_token,
                                        size_t last_token_index);
static int token_can_start_object_name(const mylite_token *token);
static int token_can_continue_object_name(const mylite_token *token);
static int is_optional_name_modifier(int token);
static int statement_kind_uses_object_scan(mylite_statement_kind kind);
static mylite_token_kind token_kind_from_parser_token(int token);

void mylite_parser_init(mylite_parser *parser, const char *sql, size_t length)
{
	memset(parser, 0, sizeof(*parser));
	mylite_lexer_init(&parser->lexer, sql, length);
}

void mylite_parser_destroy(mylite_parser *parser)
{
	free(parser->tokens);
	free(parser->statements);
	parser->tokens = NULL;
	parser->token_count = 0;
	parser->token_capacity = 0;
	parser->statements = NULL;
	parser->statement_count = 0;
	parser->statement_capacity = 0;
}

void mylite_parser_begin_statement(mylite_parser *parser, mylite_statement_kind kind, int requires_body)
{
	parser->active_statement_kind = kind;
	parser->active_statement_first_token = parser->lexer.token_count;
	parser->active_statement_start_offset = parser->lexer.token_start_offset;
	parser->active_statement_start_line = parser->lexer.token_start_line;
	parser->active_statement_start_column = parser->lexer.token_start_column;
	parser->active_statement_body_items = 0;
	parser->active_statement_requires_body = requires_body;
}

int mylite_parser_record_token(mylite_parser *parser, int token)
{
	mylite_token *tokens;
	size_t new_capacity;

	if (parser->token_count == parser->token_capacity) {
		new_capacity = parser->token_capacity == 0 ? 32 : parser->token_capacity * 2;
		tokens = (mylite_token *)realloc(parser->tokens, new_capacity * sizeof(parser->tokens[0]));
		if (tokens == NULL) {
			mylite_parser_set_error(parser, "out of memory");
			return 0;
		}
		parser->tokens = tokens;
		parser->token_capacity = new_capacity;
	}

	parser->tokens[parser->token_count].kind = token_kind_from_parser_token(token);
	parser->tokens[parser->token_count].parser_token = token;
	parser->tokens[parser->token_count].start_offset = parser->lexer.token_start_offset;
	parser->tokens[parser->token_count].end_offset = parser->lexer.token_end_offset;
	parser->tokens[parser->token_count].start_line = parser->lexer.token_start_line;
	parser->tokens[parser->token_count].start_column = parser->lexer.token_start_column;
	parser->tokens[parser->token_count].end_line = parser->lexer.token_end_line;
	parser->tokens[parser->token_count].end_column = parser->lexer.token_end_column;
	parser->token_count++;
	return 1;
}

int mylite_parser_add_statement(mylite_parser *parser, mylite_statement_kind kind)
{
	mylite_statement *statements;
	size_t new_capacity;

	if (parser->active_statement_requires_body && parser->active_statement_body_items == 0) {
		mylite_parser_set_error(parser, "statement requires a body");
		return 0;
	}
	if (parser->statement_count == parser->statement_capacity) {
		new_capacity = parser->statement_capacity == 0 ? 4 : parser->statement_capacity * 2;
		statements = (mylite_statement *)realloc(parser->statements,
		                                        new_capacity * sizeof(parser->statements[0]));
		if (statements == NULL) {
			mylite_parser_set_error(parser, "out of memory");
			return 0;
		}
		parser->statements = statements;
		parser->statement_capacity = new_capacity;
	}

	parser->statements[parser->statement_count].kind = kind;
	parser->statements[parser->statement_count].object_kind = MYLITE_STATEMENT_OBJECT_NONE;
	parser->statements[parser->statement_count].first_token = parser->active_statement_first_token;
	parser->statements[parser->statement_count].last_token = parser->lexer.last_significant_token;
	parser->statements[parser->statement_count].object_name_first_token = 0;
	parser->statements[parser->statement_count].object_name_last_token = 0;
	parser->statements[parser->statement_count].start_offset = parser->active_statement_start_offset;
	parser->statements[parser->statement_count].end_offset = parser->lexer.last_significant_token_end_offset;
	parser->statements[parser->statement_count].object_name_start_offset = 0;
	parser->statements[parser->statement_count].object_name_end_offset = 0;
	parser->statements[parser->statement_count].start_line = parser->active_statement_start_line;
	parser->statements[parser->statement_count].start_column = parser->active_statement_start_column;
	parser->statements[parser->statement_count].end_line = parser->lexer.last_significant_token_end_line;
	parser->statements[parser->statement_count].end_column = parser->lexer.last_significant_token_end_column;
	parser->statements[parser->statement_count].object_name_start_line = 0;
	parser->statements[parser->statement_count].object_name_start_column = 0;
	parser->statements[parser->statement_count].object_name_end_line = 0;
	parser->statements[parser->statement_count].object_name_end_column = 0;
	parser->statement_count++;
	return 1;
}

void mylite_parser_set_error(mylite_parser *parser, const char *message)
{
	if (parser->error[0] != '\0') {
		return;
	}
	if (parser->lexer.error[0] != '\0') {
		snprintf(parser->error, sizeof(parser->error), "%s", parser->lexer.error);
		parser->error_line = parser->lexer.error_line;
		parser->error_column = parser->lexer.error_column;
		return;
	}
	snprintf(parser->error, sizeof(parser->error), "%s", message);
	parser->error_line = parser->lexer.line;
	parser->error_column = parser->lexer.column;
}

int mylite_parse_sql(const char *sql, size_t length, mylite_parse_result *result)
{
	mylite_parser parser;
	int rc;

	memset(result, 0, sizeof(*result));
	mylite_parser_init(&parser, sql, length);
	rc = yyparse(&parser);
	if (parser.lexer.error[0] != '\0') {
		mylite_parser_set_error(&parser, parser.lexer.error);
	}
	if (parser.error[0] == '\0') {
		classify_statement_objects(&parser);
	}

	result->ok = rc == 0 && parser.error[0] == '\0';
	result->token_count = parser.token_count;
	if (parser.token_count > 0) {
		result->tokens = (mylite_token *)malloc(parser.token_count * sizeof(parser.tokens[0]));
		if (result->tokens == NULL) {
			result->ok = 0;
			result->token_count = 0;
			snprintf(result->error, sizeof(result->error), "out of memory");
			result->error_line = parser.lexer.line;
			result->error_column = parser.lexer.column;
			mylite_parser_destroy(&parser);
			return 0;
		}
		memcpy(result->tokens, parser.tokens, parser.token_count * sizeof(parser.tokens[0]));
	}
	result->statement_count = parser.statement_count;
	if (parser.statement_count > 0) {
		result->statements = (mylite_statement *)malloc(parser.statement_count * sizeof(parser.statements[0]));
		if (result->statements == NULL) {
			result->ok = 0;
			snprintf(result->error, sizeof(result->error), "out of memory");
			result->error_line = parser.lexer.line;
			result->error_column = parser.lexer.column;
			free(result->tokens);
			result->tokens = NULL;
			result->token_count = 0;
			mylite_parser_destroy(&parser);
			return 0;
		}
		memcpy(result->statements, parser.statements, parser.statement_count * sizeof(parser.statements[0]));
	}

	if (!result->ok) {
		snprintf(result->error, sizeof(result->error), "%s",
		         parser.error[0] == '\0' ? "syntax error" : parser.error);
		result->error_line = parser.error_line == 0 ? parser.lexer.line : parser.error_line;
		result->error_column = parser.error_column == 0 ? parser.lexer.column : parser.error_column;
	}

	mylite_parser_destroy(&parser);
	return result->ok;
}

void mylite_parse_result_free(mylite_parse_result *result)
{
	free(result->tokens);
	free(result->statements);
	memset(result, 0, sizeof(*result));
}

const char *mylite_statement_kind_name(mylite_statement_kind kind)
{
	switch (kind) {
	case MYLITE_STATEMENT_SELECT: return "select";
	case MYLITE_STATEMENT_INSERT: return "insert";
	case MYLITE_STATEMENT_REPLACE: return "replace";
	case MYLITE_STATEMENT_UPDATE: return "update";
	case MYLITE_STATEMENT_DELETE: return "delete";
	case MYLITE_STATEMENT_CREATE: return "create";
	case MYLITE_STATEMENT_ALTER: return "alter";
	case MYLITE_STATEMENT_DROP: return "drop";
	case MYLITE_STATEMENT_TRUNCATE: return "truncate";
	case MYLITE_STATEMENT_RENAME: return "rename";
	case MYLITE_STATEMENT_CALL: return "call";
	case MYLITE_STATEMENT_DO: return "do";
	case MYLITE_STATEMENT_HANDLER: return "handler";
	case MYLITE_STATEMENT_IMPORT: return "import";
	case MYLITE_STATEMENT_LOAD: return "load";
	case MYLITE_STATEMENT_TABLE: return "table";
	case MYLITE_STATEMENT_VALUES: return "values";
	case MYLITE_STATEMENT_SET: return "set";
	case MYLITE_STATEMENT_SHOW: return "show";
	case MYLITE_STATEMENT_USE: return "use";
	case MYLITE_STATEMENT_DESCRIBE: return "describe";
	case MYLITE_STATEMENT_EXPLAIN: return "explain";
	case MYLITE_STATEMENT_HELP: return "help";
	case MYLITE_STATEMENT_START: return "start";
	case MYLITE_STATEMENT_BEGIN: return "begin";
	case MYLITE_STATEMENT_COMMIT: return "commit";
	case MYLITE_STATEMENT_ROLLBACK: return "rollback";
	case MYLITE_STATEMENT_SAVEPOINT: return "savepoint";
	case MYLITE_STATEMENT_RELEASE: return "release";
	case MYLITE_STATEMENT_LOCK: return "lock";
	case MYLITE_STATEMENT_UNLOCK: return "unlock";
	case MYLITE_STATEMENT_XA: return "xa";
	case MYLITE_STATEMENT_PREPARE: return "prepare";
	case MYLITE_STATEMENT_EXECUTE: return "execute";
	case MYLITE_STATEMENT_DEALLOCATE: return "deallocate";
	case MYLITE_STATEMENT_ANALYZE: return "analyze";
	case MYLITE_STATEMENT_CHECK: return "check";
	case MYLITE_STATEMENT_CHECKSUM: return "checksum";
	case MYLITE_STATEMENT_OPTIMIZE: return "optimize";
	case MYLITE_STATEMENT_REPAIR: return "repair";
	case MYLITE_STATEMENT_FLUSH: return "flush";
	case MYLITE_STATEMENT_KILL: return "kill";
	case MYLITE_STATEMENT_RESET: return "reset";
	case MYLITE_STATEMENT_RESTART: return "restart";
	case MYLITE_STATEMENT_SHUTDOWN: return "shutdown";
	case MYLITE_STATEMENT_GRANT: return "grant";
	case MYLITE_STATEMENT_REVOKE: return "revoke";
	case MYLITE_STATEMENT_INSTALL: return "install";
	case MYLITE_STATEMENT_UNINSTALL: return "uninstall";
	case MYLITE_STATEMENT_CACHE: return "cache";
	case MYLITE_STATEMENT_CHANGE: return "change";
	case MYLITE_STATEMENT_BINLOG: return "binlog";
	case MYLITE_STATEMENT_PURGE: return "purge";
	case MYLITE_STATEMENT_SIGNAL: return "signal";
	case MYLITE_STATEMENT_RESIGNAL: return "resignal";
	case MYLITE_STATEMENT_GET: return "get";
	case MYLITE_STATEMENT_IF: return "if";
	case MYLITE_STATEMENT_UNKNOWN:
	default:
		return "unknown";
	}
}

const char *mylite_statement_object_kind_name(mylite_statement_object_kind kind)
{
	switch (kind) {
	case MYLITE_STATEMENT_OBJECT_DATABASE: return "database";
	case MYLITE_STATEMENT_OBJECT_EVENT: return "event";
	case MYLITE_STATEMENT_OBJECT_FUNCTION: return "function";
	case MYLITE_STATEMENT_OBJECT_INDEX: return "index";
	case MYLITE_STATEMENT_OBJECT_PROCEDURE: return "procedure";
	case MYLITE_STATEMENT_OBJECT_ROLE: return "role";
	case MYLITE_STATEMENT_OBJECT_SCHEMA: return "schema";
	case MYLITE_STATEMENT_OBJECT_SPATIAL_REFERENCE_SYSTEM: return "spatial_reference_system";
	case MYLITE_STATEMENT_OBJECT_TABLE: return "table";
	case MYLITE_STATEMENT_OBJECT_TABLESPACE: return "tablespace";
	case MYLITE_STATEMENT_OBJECT_TRIGGER: return "trigger";
	case MYLITE_STATEMENT_OBJECT_USER: return "user";
	case MYLITE_STATEMENT_OBJECT_VIEW: return "view";
	case MYLITE_STATEMENT_OBJECT_NONE:
	default:
		return "none";
	}
}

const char *mylite_token_kind_name(mylite_token_kind kind)
{
	switch (kind) {
	case MYLITE_TOKEN_IDENTIFIER: return "identifier";
	case MYLITE_TOKEN_QUOTED_IDENTIFIER: return "quoted_identifier";
	case MYLITE_TOKEN_STRING: return "string";
	case MYLITE_TOKEN_NUMBER: return "number";
	case MYLITE_TOKEN_PARAMETER: return "parameter";
	case MYLITE_TOKEN_USER_VARIABLE: return "user_variable";
	case MYLITE_TOKEN_SYSTEM_VARIABLE: return "system_variable";
	case MYLITE_TOKEN_OPERATOR: return "operator";
	case MYLITE_TOKEN_PUNCTUATION: return "punctuation";
	case MYLITE_TOKEN_KEYWORD: return "keyword";
	case MYLITE_TOKEN_UNKNOWN:
	default:
		return "unknown";
	}
}

static void classify_statement_objects(mylite_parser *parser)
{
	size_t i;

	for (i = 0; i < parser->statement_count; i++) {
		classify_statement_object(parser, &parser->statements[i]);
	}
}

static void classify_statement_object(const mylite_parser *parser, mylite_statement *statement)
{
	size_t token_index;
	size_t last_token_index;

	if (!statement_kind_uses_object_scan(statement->kind) ||
	    statement->first_token == 0 ||
	    statement->last_token < statement->first_token) {
		return;
	}

	token_index = statement->first_token;
	last_token_index = statement->last_token - 1;
	for (; token_index <= last_token_index && token_index < parser->token_count; token_index++) {
		mylite_statement_object_kind object_kind =
			object_kind_from_token_sequence(parser, token_index, last_token_index);
		if (object_kind != MYLITE_STATEMENT_OBJECT_NONE) {
			statement->object_kind = object_kind;
			set_statement_object_name(parser, statement, token_index, last_token_index);
			return;
		}
	}
}

static mylite_statement_object_kind object_kind_from_token_sequence(const mylite_parser *parser,
                                                                    size_t token_index,
                                                                    size_t last_token_index)
{
	int token = parser->tokens[token_index].parser_token;

	switch (token) {
	case DATABASE_T: return MYLITE_STATEMENT_OBJECT_DATABASE;
	case EVENT_T: return MYLITE_STATEMENT_OBJECT_EVENT;
	case FUNCTION_T: return MYLITE_STATEMENT_OBJECT_FUNCTION;
	case INDEX_T: return MYLITE_STATEMENT_OBJECT_INDEX;
	case PROCEDURE_T: return MYLITE_STATEMENT_OBJECT_PROCEDURE;
	case ROLE_T: return MYLITE_STATEMENT_OBJECT_ROLE;
	case SCHEMA_T: return MYLITE_STATEMENT_OBJECT_SCHEMA;
	case TABLE_T: return MYLITE_STATEMENT_OBJECT_TABLE;
	case TRIGGER_T: return MYLITE_STATEMENT_OBJECT_TRIGGER;
	case USER_T: return MYLITE_STATEMENT_OBJECT_USER;
	case VIEW_T: return MYLITE_STATEMENT_OBJECT_VIEW;
	case SPATIAL_T:
		if (token_index + 2 <= last_token_index &&
		    parser->tokens[token_index + 1].kind == MYLITE_TOKEN_IDENTIFIER &&
		    parser->tokens[token_index + 2].kind == MYLITE_TOKEN_IDENTIFIER) {
			return MYLITE_STATEMENT_OBJECT_SPATIAL_REFERENCE_SYSTEM;
		}
		return MYLITE_STATEMENT_OBJECT_NONE;
	default:
		if (parser->tokens[token_index].kind == MYLITE_TOKEN_IDENTIFIER &&
		    parser->tokens[token_index].end_offset - parser->tokens[token_index].start_offset == 10 &&
		    strncasecmp("TABLESPACE",
		                parser->lexer.input + parser->tokens[token_index].start_offset,
		                10) == 0) {
			return MYLITE_STATEMENT_OBJECT_TABLESPACE;
		}
		return MYLITE_STATEMENT_OBJECT_NONE;
	}
}

static void set_statement_object_name(const mylite_parser *parser,
                                      mylite_statement *statement,
                                      size_t object_token_index,
                                      size_t last_token_index)
{
	size_t first_name_token = first_name_token_after_object(parser, object_token_index, last_token_index);
	size_t last_name_token;
	const mylite_token *first;
	const mylite_token *last;

	if (first_name_token >= parser->token_count) {
		return;
	}

	last_name_token = last_qualified_name_token(parser, first_name_token, last_token_index);
	first = &parser->tokens[first_name_token];
	last = &parser->tokens[last_name_token];

	statement->object_name_first_token = first_name_token + 1;
	statement->object_name_last_token = last_name_token + 1;
	statement->object_name_start_offset = first->start_offset;
	statement->object_name_end_offset = last->end_offset;
	statement->object_name_start_line = first->start_line;
	statement->object_name_start_column = first->start_column;
	statement->object_name_end_line = last->end_line;
	statement->object_name_end_column = last->end_column;
}

static size_t first_name_token_after_object(const mylite_parser *parser,
                                            size_t object_token_index,
                                            size_t last_token_index)
{
	size_t token_index = object_token_index + 1;

	if (parser->tokens[object_token_index].parser_token == SPATIAL_T && token_index + 2 <= last_token_index) {
		token_index += 2;
	}

	while (token_index <= last_token_index) {
		if (is_optional_name_modifier(parser->tokens[token_index].parser_token)) {
			token_index++;
			continue;
		}
		if (token_can_start_object_name(&parser->tokens[token_index])) {
			return token_index;
		}
		token_index++;
	}
	return parser->token_count;
}

static size_t last_qualified_name_token(const mylite_parser *parser,
                                        size_t first_name_token,
                                        size_t last_token_index)
{
	size_t token_index = first_name_token;

	while (token_index + 2 <= last_token_index &&
	       parser->tokens[token_index + 1].parser_token == '.' &&
	       token_can_continue_object_name(&parser->tokens[token_index + 2])) {
		token_index += 2;
	}
	return token_index;
}

static int token_can_start_object_name(const mylite_token *token)
{
	return token->kind == MYLITE_TOKEN_IDENTIFIER ||
	       token->kind == MYLITE_TOKEN_QUOTED_IDENTIFIER ||
	       token->kind == MYLITE_TOKEN_STRING ||
	       token->kind == MYLITE_TOKEN_NUMBER;
}

static int token_can_continue_object_name(const mylite_token *token)
{
	return token->kind == MYLITE_TOKEN_IDENTIFIER ||
	       token->kind == MYLITE_TOKEN_QUOTED_IDENTIFIER;
}

static int is_optional_name_modifier(int token)
{
	return token == IF_T ||
	       token == NOT_T ||
	       token == EXISTS_T ||
	       token == TEMPORARY_T;
}

static int statement_kind_uses_object_scan(mylite_statement_kind kind)
{
	switch (kind) {
	case MYLITE_STATEMENT_CREATE:
	case MYLITE_STATEMENT_ALTER:
	case MYLITE_STATEMENT_DROP:
	case MYLITE_STATEMENT_TRUNCATE:
	case MYLITE_STATEMENT_RENAME:
	case MYLITE_STATEMENT_ANALYZE:
	case MYLITE_STATEMENT_CHECK:
	case MYLITE_STATEMENT_CHECKSUM:
	case MYLITE_STATEMENT_OPTIMIZE:
	case MYLITE_STATEMENT_REPAIR:
		return 1;
	default:
		return 0;
	}
}

static mylite_token_kind token_kind_from_parser_token(int token)
{
	switch (token) {
	case IDENT: return MYLITE_TOKEN_IDENTIFIER;
	case QUOTED_IDENT: return MYLITE_TOKEN_QUOTED_IDENTIFIER;
	case STRING: return MYLITE_TOKEN_STRING;
	case NUMBER: return MYLITE_TOKEN_NUMBER;
	case PARAM: return MYLITE_TOKEN_PARAMETER;
	case USER_VARIABLE: return MYLITE_TOKEN_USER_VARIABLE;
	case SYSTEM_VARIABLE: return MYLITE_TOKEN_SYSTEM_VARIABLE;
	case OPERATOR: return MYLITE_TOKEN_OPERATOR;
	default:
		if (token > 0 && token < 256) {
			return MYLITE_TOKEN_PUNCTUATION;
		}
		return MYLITE_TOKEN_KEYWORD;
	}
}
