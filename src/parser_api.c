#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "parser_bison.h"
#include "parser_internal.h"

int yyparse(mylite_parser *parser);

void mylite_parser_init(mylite_parser *parser, const char *sql, size_t length)
{
	memset(parser, 0, sizeof(*parser));
	mylite_lexer_init(&parser->lexer, sql, length);
}

void mylite_parser_destroy(mylite_parser *parser)
{
	free(parser->statements);
	parser->statements = NULL;
	parser->statement_count = 0;
	parser->statement_capacity = 0;
}

int mylite_parser_add_statement(mylite_parser *parser, mylite_statement_kind kind)
{
	mylite_statement *statements;
	size_t new_capacity;

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
	parser->statements[parser->statement_count].first_token = parser->active_statement_first_token;
	parser->statements[parser->statement_count].last_token = parser->lexer.token_count;
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

	result->ok = rc == 0 && parser.error[0] == '\0';
	result->statement_count = parser.statement_count;
	if (parser.statement_count > 0) {
		result->statements = (mylite_statement *)malloc(parser.statement_count * sizeof(parser.statements[0]));
		if (result->statements == NULL) {
			result->ok = 0;
			snprintf(result->error, sizeof(result->error), "out of memory");
			result->error_line = parser.lexer.line;
			result->error_column = parser.lexer.column;
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
