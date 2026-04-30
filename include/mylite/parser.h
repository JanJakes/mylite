#ifndef MYLITE_PARSER_H
#define MYLITE_PARSER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum mylite_statement_kind {
	MYLITE_STATEMENT_UNKNOWN = 0,
	MYLITE_STATEMENT_SELECT,
	MYLITE_STATEMENT_INSERT,
	MYLITE_STATEMENT_REPLACE,
	MYLITE_STATEMENT_UPDATE,
	MYLITE_STATEMENT_DELETE,
	MYLITE_STATEMENT_CREATE,
	MYLITE_STATEMENT_ALTER,
	MYLITE_STATEMENT_DROP,
	MYLITE_STATEMENT_TRUNCATE,
	MYLITE_STATEMENT_RENAME,
	MYLITE_STATEMENT_CALL,
	MYLITE_STATEMENT_DO,
	MYLITE_STATEMENT_HANDLER,
	MYLITE_STATEMENT_IMPORT,
	MYLITE_STATEMENT_LOAD,
	MYLITE_STATEMENT_TABLE,
	MYLITE_STATEMENT_VALUES,
	MYLITE_STATEMENT_SET,
	MYLITE_STATEMENT_SHOW,
	MYLITE_STATEMENT_USE,
	MYLITE_STATEMENT_DESCRIBE,
	MYLITE_STATEMENT_EXPLAIN,
	MYLITE_STATEMENT_HELP,
	MYLITE_STATEMENT_START,
	MYLITE_STATEMENT_BEGIN,
	MYLITE_STATEMENT_COMMIT,
	MYLITE_STATEMENT_ROLLBACK,
	MYLITE_STATEMENT_SAVEPOINT,
	MYLITE_STATEMENT_RELEASE,
	MYLITE_STATEMENT_LOCK,
	MYLITE_STATEMENT_UNLOCK,
	MYLITE_STATEMENT_XA,
	MYLITE_STATEMENT_PREPARE,
	MYLITE_STATEMENT_EXECUTE,
	MYLITE_STATEMENT_DEALLOCATE,
	MYLITE_STATEMENT_ANALYZE,
	MYLITE_STATEMENT_CHECK,
	MYLITE_STATEMENT_CHECKSUM,
	MYLITE_STATEMENT_OPTIMIZE,
	MYLITE_STATEMENT_REPAIR,
	MYLITE_STATEMENT_FLUSH,
	MYLITE_STATEMENT_KILL,
	MYLITE_STATEMENT_RESET,
	MYLITE_STATEMENT_RESTART,
	MYLITE_STATEMENT_SHUTDOWN,
	MYLITE_STATEMENT_GRANT,
	MYLITE_STATEMENT_REVOKE,
	MYLITE_STATEMENT_INSTALL,
	MYLITE_STATEMENT_UNINSTALL,
	MYLITE_STATEMENT_CACHE,
	MYLITE_STATEMENT_CHANGE,
	MYLITE_STATEMENT_BINLOG,
	MYLITE_STATEMENT_PURGE,
	MYLITE_STATEMENT_SIGNAL,
	MYLITE_STATEMENT_RESIGNAL,
	MYLITE_STATEMENT_GET,
	MYLITE_STATEMENT_IF
} mylite_statement_kind;

typedef enum mylite_token_kind {
	MYLITE_TOKEN_UNKNOWN = 0,
	MYLITE_TOKEN_IDENTIFIER,
	MYLITE_TOKEN_QUOTED_IDENTIFIER,
	MYLITE_TOKEN_STRING,
	MYLITE_TOKEN_NUMBER,
	MYLITE_TOKEN_PARAMETER,
	MYLITE_TOKEN_USER_VARIABLE,
	MYLITE_TOKEN_SYSTEM_VARIABLE,
	MYLITE_TOKEN_OPERATOR,
	MYLITE_TOKEN_PUNCTUATION,
	MYLITE_TOKEN_KEYWORD
} mylite_token_kind;

typedef struct mylite_token {
	mylite_token_kind kind;
	int parser_token;
	size_t start_offset;
	size_t end_offset;
	unsigned int start_line;
	unsigned int start_column;
	unsigned int end_line;
	unsigned int end_column;
} mylite_token;

typedef struct mylite_statement {
	mylite_statement_kind kind;
	size_t first_token;
	size_t last_token;
	size_t start_offset;
	size_t end_offset;
	unsigned int start_line;
	unsigned int start_column;
	unsigned int end_line;
	unsigned int end_column;
} mylite_statement;

typedef struct mylite_parse_result {
	int ok;
	size_t token_count;
	mylite_token *tokens;
	size_t statement_count;
	mylite_statement *statements;
	char error[256];
	unsigned int error_line;
	unsigned int error_column;
} mylite_parse_result;

int mylite_parse_sql(const char *sql, size_t length, mylite_parse_result *result);
void mylite_parse_result_free(mylite_parse_result *result);
const char *mylite_statement_kind_name(mylite_statement_kind kind);
const char *mylite_token_kind_name(mylite_token_kind kind);

#ifdef __cplusplus
}
#endif

#endif
