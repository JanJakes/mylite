#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "lexer.h"
#include "parser_bison.h"
#include "parser_internal.h"

int yyparse(mylite_parser *parser);

static void match_compound_control_tokens(mylite_parser *parser);
static int is_compound_control_start_token(const mylite_parser *parser, size_t token_index);
static int is_if_function_call(const mylite_parser *parser, size_t token_index);
static int is_if_exists_clause(const mylite_parser *parser, size_t token_index);
static int is_compound_control_end_token(int token);
static int compound_control_tokens_match(int start_token, int end_token);
static void classify_statement_metadata(mylite_parser *parser);
static void classify_grouped_query_statement_kinds(mylite_parser *parser);
static mylite_statement_kind classify_grouped_query_statement_kind(const mylite_parser *parser,
                                                                   const mylite_statement *statement);
static mylite_statement_kind query_statement_kind_from_token(int token);
static void classify_with_statement_kinds(mylite_parser *parser);
static mylite_statement_kind classify_with_statement_kind(const mylite_parser *parser,
                                                          const mylite_statement *statement);
static void classify_labeled_statement_metadata(mylite_parser *parser);
static int classify_labeled_statement(mylite_parser *parser, mylite_statement *statement);
static mylite_statement_kind labeled_statement_kind_from_token(int token);
static void classify_statement_objects(mylite_parser *parser);
static void classify_statement_object(const mylite_parser *parser, mylite_statement *statement);
static int classify_dml_statement_object(const mylite_parser *parser, mylite_statement *statement);
static size_t find_statement_kind_token(const mylite_parser *parser, const mylite_statement *statement);
static size_t find_insert_or_replace_name_token(const mylite_parser *parser,
                                                size_t token_index,
                                                size_t last_token_index);
static size_t find_update_name_token(const mylite_parser *parser,
                                     size_t token_index,
                                     size_t last_token_index);
static size_t find_delete_name_token(const mylite_parser *parser,
                                     size_t token_index,
                                     size_t last_token_index);
static size_t skip_dml_modifiers(const mylite_parser *parser,
                                 size_t token_index,
                                 size_t last_token_index);
static int is_dml_modifier_token(int token);
static int classify_direct_statement_object(const mylite_parser *parser, mylite_statement *statement);
static size_t find_import_sdi_file_token(const mylite_parser *parser,
                                         size_t token_index,
                                         size_t last_token_index);
static size_t find_call_procedure_name_token(const mylite_parser *parser,
                                             size_t token_index,
                                             size_t last_token_index);
static int classify_signal_statement_object(const mylite_parser *parser,
                                            mylite_statement *statement,
                                            size_t token_index,
                                            size_t last_token_index);
static int classify_condition_value_statement_object(const mylite_parser *parser,
                                                     mylite_statement *statement,
                                                     size_t token_index,
                                                     size_t last_token_index,
                                                     int allow_condition_class);
static int classify_get_statement_object(const mylite_parser *parser,
                                         mylite_statement *statement,
                                         size_t token_index,
                                         size_t last_token_index);
static int token_can_start_diagnostics_condition_number(const mylite_token *token);
static int classify_describe_or_explain_statement_object(const mylite_parser *parser,
                                                         mylite_statement *statement,
                                                         size_t token_index,
                                                         size_t last_token_index);
static size_t find_explain_connection_id_token(const mylite_parser *parser,
                                               size_t token_index,
                                               size_t last_token_index);
static size_t find_describe_or_explain_table_name_token(const mylite_parser *parser,
                                                        size_t token_index,
                                                        size_t last_token_index);
static size_t find_load_table_name_token(const mylite_parser *parser,
                                         size_t token_index,
                                         size_t last_token_index);
static size_t find_cache_index_name_token(const mylite_parser *parser,
                                          size_t token_index,
                                          size_t last_token_index);
static size_t find_lock_table_name_token(const mylite_parser *parser,
                                         size_t token_index,
                                         size_t last_token_index);
static size_t find_flush_table_name_token(const mylite_parser *parser,
                                          size_t token_index,
                                          size_t last_token_index);
static int classify_flush_statement_object(const mylite_parser *parser,
                                           mylite_statement *statement,
                                           size_t token_index,
                                           size_t last_token_index);
static size_t find_maintenance_table_name_token(const mylite_parser *parser,
                                                size_t token_index,
                                                size_t last_token_index);
static int classify_instance_statement_object(const mylite_parser *parser,
                                              mylite_statement *statement,
                                              size_t token_index,
                                              size_t last_token_index);
static int classify_kill_statement_object(const mylite_parser *parser,
                                          mylite_statement *statement,
                                          size_t token_index,
                                          size_t last_token_index);
static int classify_purge_statement_object(const mylite_parser *parser,
                                           mylite_statement *statement,
                                           size_t token_index,
                                           size_t last_token_index);
static int classify_replication_channel_statement_object(const mylite_parser *parser,
                                                         mylite_statement *statement,
                                                         size_t token_index,
                                                         size_t last_token_index);
static int classify_reset_statement_object(const mylite_parser *parser,
                                           mylite_statement *statement,
                                           size_t token_index,
                                           size_t last_token_index);
static size_t find_replication_channel_name_token(const mylite_parser *parser,
                                                  size_t token_index,
                                                  size_t last_token_index);
static int classify_set_statement_object(const mylite_parser *parser,
                                         mylite_statement *statement,
                                         size_t token_index,
                                         size_t last_token_index);
static size_t find_set_role_name_token(const mylite_parser *parser,
                                       size_t token_index,
                                       size_t last_token_index);
static size_t find_set_default_role_user_name_token(const mylite_parser *parser,
                                                    size_t token_index,
                                                    size_t last_token_index);
static size_t find_set_password_name_token(const mylite_parser *parser,
                                           size_t token_index,
                                           size_t last_token_index);
static int classify_install_statement_object(const mylite_parser *parser,
                                             mylite_statement *statement,
                                             size_t token_index,
                                             size_t last_token_index);
static int classify_xa_statement_object(const mylite_parser *parser,
                                        mylite_statement *statement,
                                        size_t token_index,
                                        size_t last_token_index);
static int classify_prepared_statement_object(const mylite_parser *parser,
                                              mylite_statement *statement,
                                              size_t token_index,
                                              size_t last_token_index);
static int classify_principal_statement_object(const mylite_parser *parser,
                                               mylite_statement *statement,
                                               size_t token_index,
                                               size_t last_token_index);
static int classify_savepoint_statement_object(const mylite_parser *parser,
                                               mylite_statement *statement,
                                               size_t token_index,
                                               size_t last_token_index);
static int classify_declare_statement_object(const mylite_parser *parser,
                                             mylite_statement *statement,
                                             size_t token_index,
                                             size_t last_token_index);
static size_t find_declare_handler_condition_token(const mylite_parser *parser,
                                                   size_t token_index,
                                                   size_t last_token_index);
static int classify_cursor_statement_object(const mylite_parser *parser,
                                            mylite_statement *statement,
                                            size_t token_index,
                                            size_t last_token_index);
static int statement_contains_token(const mylite_parser *parser,
                                    size_t token_index,
                                    size_t last_token_index,
                                    int wanted_token);
static size_t find_savepoint_name_token(const mylite_parser *parser,
                                        size_t token_index,
                                        size_t last_token_index);
static size_t find_principal_name_token(const mylite_parser *parser,
                                        size_t token_index,
                                        size_t last_token_index,
                                        int marker_token);
static void set_statement_account_name_from_first_token(const mylite_parser *parser,
                                                        mylite_statement *statement,
                                                        size_t first_name_token,
                                                        size_t last_token_index);
static size_t last_account_name_token(const mylite_parser *parser,
                                      size_t first_name_token,
                                      size_t last_token_index);
static int token_is_account_at_marker(const mylite_parser *parser, size_t token_index);
static int classify_show_statement_object(const mylite_parser *parser,
                                          mylite_statement *statement,
                                          size_t token_index,
                                          size_t last_token_index);
static size_t find_show_profile_query_id_token(const mylite_parser *parser,
                                               size_t token_index,
                                               size_t last_token_index);
static size_t find_show_binlog_events_name_token(const mylite_parser *parser,
                                                 size_t token_index,
                                                 size_t last_token_index);
static size_t skip_show_modifiers(const mylite_parser *parser,
                                  size_t token_index,
                                  size_t last_token_index);
static int is_show_modifier_token(const mylite_parser *parser, size_t token_index);
static size_t find_show_from_name_token(const mylite_parser *parser,
                                        size_t token_index,
                                        size_t last_token_index);
static size_t find_show_grants_name_token(const mylite_parser *parser,
                                          size_t token_index,
                                          size_t last_token_index);
static int set_statement_direct_object(mylite_statement *statement,
                                       mylite_statement_object_kind object_kind);
static int set_statement_direct_object_name(const mylite_parser *parser,
                                            mylite_statement *statement,
                                            mylite_statement_object_kind object_kind,
                                            size_t name_token_index,
                                            size_t last_token_index);
static int set_statement_direct_object_name_range(const mylite_parser *parser,
                                                  mylite_statement *statement,
                                                  mylite_statement_object_kind object_kind,
                                                  size_t first_name_token,
                                                  size_t last_name_token);
static mylite_statement_object_kind object_kind_from_token_sequence(const mylite_parser *parser,
                                                                    size_t token_index,
                                                                    size_t last_token_index);
static void set_statement_object_name(const mylite_parser *parser,
                                      mylite_statement *statement,
                                      size_t object_token_index,
                                      size_t last_token_index);
static void set_statement_object_name_from_first_token(const mylite_parser *parser,
                                                       mylite_statement *statement,
                                                       size_t first_name_token,
                                                       size_t last_token_index);
static size_t first_name_token_after_object(const mylite_parser *parser,
                                            size_t object_token_index,
                                            size_t last_token_index);
static size_t last_qualified_name_token(const mylite_parser *parser,
                                        size_t first_name_token,
                                        size_t last_token_index);
static int token_can_start_object_name(const mylite_token *token);
static int token_can_continue_object_name(const mylite_token *token);
static int token_can_be_unquoted_object_name_keyword(int token);
static int token_can_start_label_name(const mylite_token *token);
static int is_optional_name_modifier(int token);
static int token_text_equals(const mylite_parser *parser, size_t token_index, const char *expected);
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
	mylite_parser_begin_statement_at_token(parser, kind, requires_body, parser->lexer.token_count);
}

void mylite_parser_begin_statement_at_token(mylite_parser *parser,
                                            mylite_statement_kind kind,
                                            int requires_body,
                                            size_t first_token)
{
	parser->active_statement_kind = kind;
	parser->active_statement_first_token = first_token;
	if (first_token > 0 && first_token <= parser->token_count) {
		const mylite_token *token = &parser->tokens[first_token - 1];
		parser->active_statement_start_offset = token->start_offset;
		parser->active_statement_start_line = token->start_line;
		parser->active_statement_start_column = token->start_column;
	} else {
		parser->active_statement_first_token = parser->lexer.token_count;
		parser->active_statement_start_offset = parser->lexer.token_start_offset;
		parser->active_statement_start_line = parser->lexer.token_start_line;
		parser->active_statement_start_column = parser->lexer.token_start_column;
	}
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
	parser->tokens[parser->token_count].matching_token = 0;
	parser->tokens[parser->token_count].start_offset = parser->lexer.token_start_offset;
	parser->tokens[parser->token_count].end_offset = parser->lexer.token_end_offset;
	parser->tokens[parser->token_count].start_line = parser->lexer.token_start_line;
	parser->tokens[parser->token_count].start_column = parser->lexer.token_start_column;
	parser->tokens[parser->token_count].end_line = parser->lexer.token_end_line;
	parser->tokens[parser->token_count].end_column = parser->lexer.token_end_column;
	parser->token_count++;
	return 1;
}

void mylite_parser_match_tokens(mylite_parser *parser, size_t left_token, size_t right_token)
{
	if (left_token == 0 || right_token == 0 ||
	    left_token > parser->token_count || right_token > parser->token_count) {
		return;
	}
	parser->tokens[left_token - 1].matching_token = right_token;
	parser->tokens[right_token - 1].matching_token = left_token;
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
		match_compound_control_tokens(&parser);
		classify_statement_metadata(&parser);
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

static void match_compound_control_tokens(mylite_parser *parser)
{
	size_t *stack;
	size_t stack_size = 0;
	size_t token_index;

	if (parser->token_count == 0) {
		return;
	}

	stack = (size_t *)malloc(parser->token_count * sizeof(stack[0]));
	if (stack == NULL) {
		mylite_parser_set_error(parser, "out of memory");
		return;
	}

	for (token_index = 0; token_index < parser->token_count; token_index++) {
		int token = parser->tokens[token_index].parser_token;

		if (is_compound_control_start_token(parser, token_index)) {
			stack[stack_size++] = token_index;
			continue;
		}
		if (is_compound_control_end_token(token)) {
			while (stack_size > 0) {
				size_t start_token_index = stack[--stack_size];
				int start_token = parser->tokens[start_token_index].parser_token;
				if (compound_control_tokens_match(start_token, token)) {
					mylite_parser_match_tokens(parser, start_token_index + 1, token_index + 1);
					break;
				}
			}
		}
	}

	free(stack);
}

static int is_compound_control_start_token(const mylite_parser *parser, size_t token_index)
{
	int token = parser->tokens[token_index].parser_token;

	switch (token) {
	case IF_T:
		return !is_if_function_call(parser, token_index) &&
		       !is_if_exists_clause(parser, token_index);
	case CASE_T:
		return parser->tokens[token_index].matching_token == 0;
	case LOOP_T:
	case REPEAT_T:
	case WHILE_T:
		return parser->tokens[token_index].matching_token == 0;
	default:
		return 0;
	}
}

static int is_if_function_call(const mylite_parser *parser, size_t token_index)
{
	return token_index + 1 < parser->token_count &&
	       parser->tokens[token_index + 1].parser_token == '(';
}

static int is_if_exists_clause(const mylite_parser *parser, size_t token_index)
{
	if (token_index + 1 < parser->token_count &&
	    parser->tokens[token_index + 1].parser_token == EXISTS_T) {
		return 1;
	}
	return token_index + 2 < parser->token_count &&
	       parser->tokens[token_index + 1].parser_token == NOT_T &&
	       parser->tokens[token_index + 2].parser_token == EXISTS_T;
}

static int is_compound_control_end_token(int token)
{
	return token == END_IF_T ||
	       token == END_CASE_T ||
	       token == END_LOOP_T ||
	       token == END_REPEAT_T ||
	       token == END_WHILE_T;
}

static int compound_control_tokens_match(int start_token, int end_token)
{
	return (start_token == IF_T && end_token == END_IF_T) ||
	       (start_token == CASE_T && end_token == END_CASE_T) ||
	       (start_token == LOOP_T && end_token == END_LOOP_T) ||
	       (start_token == REPEAT_T && end_token == END_REPEAT_T) ||
	       (start_token == WHILE_T && end_token == END_WHILE_T);
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
	case MYLITE_STATEMENT_STOP: return "stop";
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
	case MYLITE_STATEMENT_CLONE: return "clone";
	case MYLITE_STATEMENT_CHANGE: return "change";
	case MYLITE_STATEMENT_BINLOG: return "binlog";
	case MYLITE_STATEMENT_PURGE: return "purge";
	case MYLITE_STATEMENT_SIGNAL: return "signal";
	case MYLITE_STATEMENT_RESIGNAL: return "resignal";
	case MYLITE_STATEMENT_GET: return "get";
	case MYLITE_STATEMENT_DECLARE: return "declare";
	case MYLITE_STATEMENT_OPEN: return "open";
	case MYLITE_STATEMENT_FETCH: return "fetch";
	case MYLITE_STATEMENT_CLOSE: return "close";
	case MYLITE_STATEMENT_IF: return "if";
	case MYLITE_STATEMENT_CASE: return "case";
	case MYLITE_STATEMENT_LOOP: return "loop";
	case MYLITE_STATEMENT_REPEAT: return "repeat";
	case MYLITE_STATEMENT_WHILE: return "while";
	case MYLITE_STATEMENT_LEAVE: return "leave";
	case MYLITE_STATEMENT_ITERATE: return "iterate";
	case MYLITE_STATEMENT_RETURN: return "return";
	case MYLITE_STATEMENT_UNKNOWN:
	default:
		return "unknown";
	}
}

const char *mylite_statement_object_kind_name(mylite_statement_object_kind kind)
{
	switch (kind) {
	case MYLITE_STATEMENT_OBJECT_BINARY_LOG: return "binary_log";
	case MYLITE_STATEMENT_OBJECT_BINARY_LOG_EVENT: return "binary_log_event";
	case MYLITE_STATEMENT_OBJECT_COMPONENT: return "component";
	case MYLITE_STATEMENT_OBJECT_CONDITION: return "condition";
	case MYLITE_STATEMENT_OBJECT_CONNECTION: return "connection";
	case MYLITE_STATEMENT_OBJECT_CURSOR: return "cursor";
	case MYLITE_STATEMENT_OBJECT_DATABASE: return "database";
	case MYLITE_STATEMENT_OBJECT_DIAGNOSTICS_CONDITION: return "diagnostics_condition";
	case MYLITE_STATEMENT_OBJECT_ENGINE: return "engine";
	case MYLITE_STATEMENT_OBJECT_EVENT: return "event";
	case MYLITE_STATEMENT_OBJECT_FUNCTION: return "function";
	case MYLITE_STATEMENT_OBJECT_HELP_TOPIC: return "help_topic";
	case MYLITE_STATEMENT_OBJECT_INDEX: return "index";
	case MYLITE_STATEMENT_OBJECT_INSTANCE: return "instance";
	case MYLITE_STATEMENT_OBJECT_LABEL: return "label";
	case MYLITE_STATEMENT_OBJECT_LOGFILE_GROUP: return "logfile_group";
	case MYLITE_STATEMENT_OBJECT_PLUGIN: return "plugin";
	case MYLITE_STATEMENT_OBJECT_PREPARED_STATEMENT: return "prepared_statement";
	case MYLITE_STATEMENT_OBJECT_PROCEDURE: return "procedure";
	case MYLITE_STATEMENT_OBJECT_QUERY: return "query";
	case MYLITE_STATEMENT_OBJECT_RELAY_LOG: return "relay_log";
	case MYLITE_STATEMENT_OBJECT_REPLICATION_CHANNEL: return "replication_channel";
	case MYLITE_STATEMENT_OBJECT_RESOURCE_GROUP: return "resource_group";
	case MYLITE_STATEMENT_OBJECT_ROLE: return "role";
	case MYLITE_STATEMENT_OBJECT_SAVEPOINT: return "savepoint";
	case MYLITE_STATEMENT_OBJECT_SCHEMA: return "schema";
	case MYLITE_STATEMENT_OBJECT_SDI_FILE: return "sdi_file";
	case MYLITE_STATEMENT_OBJECT_SERVER: return "server";
	case MYLITE_STATEMENT_OBJECT_SPATIAL_REFERENCE_SYSTEM: return "spatial_reference_system";
	case MYLITE_STATEMENT_OBJECT_SQLSTATE: return "sqlstate";
	case MYLITE_STATEMENT_OBJECT_SYSTEM_VARIABLE: return "system_variable";
	case MYLITE_STATEMENT_OBJECT_TABLE: return "table";
	case MYLITE_STATEMENT_OBJECT_TABLESPACE: return "tablespace";
	case MYLITE_STATEMENT_OBJECT_TRIGGER: return "trigger";
	case MYLITE_STATEMENT_OBJECT_USER: return "user";
	case MYLITE_STATEMENT_OBJECT_VIEW: return "view";
	case MYLITE_STATEMENT_OBJECT_XA_TRANSACTION: return "xa_transaction";
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

static void classify_statement_metadata(mylite_parser *parser)
{
	classify_grouped_query_statement_kinds(parser);
	classify_with_statement_kinds(parser);
	classify_labeled_statement_metadata(parser);
	classify_statement_objects(parser);
}

static void classify_grouped_query_statement_kinds(mylite_parser *parser)
{
	size_t i;

	for (i = 0; i < parser->statement_count; i++) {
		parser->statements[i].kind = classify_grouped_query_statement_kind(parser, &parser->statements[i]);
	}
}

static mylite_statement_kind classify_grouped_query_statement_kind(const mylite_parser *parser,
                                                                   const mylite_statement *statement)
{
	size_t token_index;
	size_t last_token_index;
	mylite_statement_kind kind;

	if (statement->kind != MYLITE_STATEMENT_UNKNOWN ||
	    statement->first_token == 0 ||
	    statement->last_token < statement->first_token) {
		return statement->kind;
	}

	token_index = statement->first_token - 1;
	last_token_index = statement->last_token - 1;
	if (token_index >= parser->token_count || parser->tokens[token_index].parser_token != '(') {
		return statement->kind;
	}

	while (token_index <= last_token_index &&
	       token_index < parser->token_count &&
	       parser->tokens[token_index].parser_token == '(') {
		size_t matching_token = parser->tokens[token_index].matching_token;
		if (matching_token == 0 ||
		    matching_token <= token_index + 1 ||
		    matching_token > statement->last_token) {
			return statement->kind;
		}
		token_index++;
	}
	if (token_index > last_token_index || token_index >= parser->token_count) {
		return statement->kind;
	}

	kind = query_statement_kind_from_token(parser->tokens[token_index].parser_token);
	return kind == MYLITE_STATEMENT_UNKNOWN ? statement->kind : kind;
}

static mylite_statement_kind query_statement_kind_from_token(int token)
{
	switch (token) {
	case SELECT_T:
	case WITH_T:
		return MYLITE_STATEMENT_SELECT;
	case TABLE_T:
		return MYLITE_STATEMENT_TABLE;
	case VALUES_T:
		return MYLITE_STATEMENT_VALUES;
	default:
		return MYLITE_STATEMENT_UNKNOWN;
	}
}

static void classify_with_statement_kinds(mylite_parser *parser)
{
	size_t i;

	for (i = 0; i < parser->statement_count; i++) {
		if (parser->statements[i].first_token == 0 ||
		    parser->statements[i].first_token > parser->token_count ||
		    parser->tokens[parser->statements[i].first_token - 1].parser_token != WITH_T) {
			continue;
		}
		parser->statements[i].kind = classify_with_statement_kind(parser, &parser->statements[i]);
	}
}

static mylite_statement_kind classify_with_statement_kind(const mylite_parser *parser,
                                                          const mylite_statement *statement)
{
	size_t token_index = statement->first_token;
	size_t last_token_index;

	if (statement->last_token < statement->first_token) {
		return statement->kind;
	}
	last_token_index = statement->last_token - 1;

	while (token_index <= last_token_index && token_index < parser->token_count) {
		int token = parser->tokens[token_index].parser_token;
		size_t matching_token = parser->tokens[token_index].matching_token;

		if (matching_token > token_index + 1) {
			token_index = matching_token;
			continue;
		}

		switch (token) {
		case SELECT_T: return MYLITE_STATEMENT_SELECT;
		case INSERT_T: return MYLITE_STATEMENT_INSERT;
		case REPLACE_T: return MYLITE_STATEMENT_REPLACE;
		case UPDATE_T: return MYLITE_STATEMENT_UPDATE;
		case DELETE_T: return MYLITE_STATEMENT_DELETE;
		default:
			token_index++;
			break;
		}
	}
	return statement->kind;
}

static void classify_labeled_statement_metadata(mylite_parser *parser)
{
	size_t i;

	for (i = 0; i < parser->statement_count; i++) {
		classify_labeled_statement(parser, &parser->statements[i]);
	}
}

static int classify_labeled_statement(mylite_parser *parser, mylite_statement *statement)
{
	size_t label_token_index;
	size_t separator_token_index;
	size_t head_token_index;
	mylite_statement_kind labeled_kind;

	if (statement->kind != MYLITE_STATEMENT_UNKNOWN ||
	    statement->first_token == 0 ||
	    statement->last_token < statement->first_token + 2 ||
	    statement->last_token > parser->token_count) {
		return 0;
	}

	label_token_index = statement->first_token - 1;
	separator_token_index = label_token_index + 1;
	head_token_index = label_token_index + 2;
	if (!token_can_start_label_name(&parser->tokens[label_token_index]) ||
	    !token_text_equals(parser, separator_token_index, ":")) {
		return 0;
	}

	labeled_kind = labeled_statement_kind_from_token(parser->tokens[head_token_index].parser_token);
	if (labeled_kind == MYLITE_STATEMENT_UNKNOWN) {
		return 0;
	}

	statement->kind = labeled_kind;
	statement->object_kind = MYLITE_STATEMENT_OBJECT_LABEL;
	set_statement_object_name_from_first_token(parser, statement, label_token_index, label_token_index);
	return 1;
}

static mylite_statement_kind labeled_statement_kind_from_token(int token)
{
	switch (token) {
	case BEGIN_T: return MYLITE_STATEMENT_BEGIN;
	case LOOP_T: return MYLITE_STATEMENT_LOOP;
	case REPEAT_T: return MYLITE_STATEMENT_REPEAT;
	case WHILE_T: return MYLITE_STATEMENT_WHILE;
	default: return MYLITE_STATEMENT_UNKNOWN;
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

	if (classify_dml_statement_object(parser, statement)) {
		return;
	}
	if (classify_direct_statement_object(parser, statement)) {
		return;
	}

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

static int classify_dml_statement_object(const mylite_parser *parser, mylite_statement *statement)
{
	size_t verb_token_index = find_statement_kind_token(parser, statement);
	size_t last_token_index;
	size_t name_token_index;

	if (verb_token_index >= parser->token_count || statement->last_token < statement->first_token) {
		return 0;
	}

	last_token_index = statement->last_token - 1;
	switch (statement->kind) {
	case MYLITE_STATEMENT_INSERT:
	case MYLITE_STATEMENT_REPLACE:
		name_token_index = find_insert_or_replace_name_token(parser, verb_token_index + 1, last_token_index);
		break;
	case MYLITE_STATEMENT_UPDATE:
		name_token_index = find_update_name_token(parser, verb_token_index + 1, last_token_index);
		break;
	case MYLITE_STATEMENT_DELETE:
		name_token_index = find_delete_name_token(parser, verb_token_index + 1, last_token_index);
		break;
	default:
		return 0;
	}

	if (name_token_index >= parser->token_count) {
		return 0;
	}

	statement->object_kind = MYLITE_STATEMENT_OBJECT_TABLE;
	set_statement_object_name_from_first_token(parser, statement, name_token_index, last_token_index);
	return 1;
}

static size_t find_statement_kind_token(const mylite_parser *parser, const mylite_statement *statement)
{
	size_t token_index;
	size_t last_token_index;
	int desired_token = 0;

	if (statement->first_token == 0 || statement->last_token < statement->first_token) {
		return parser->token_count;
	}

	switch (statement->kind) {
	case MYLITE_STATEMENT_SELECT: desired_token = SELECT_T; break;
	case MYLITE_STATEMENT_INSERT: desired_token = INSERT_T; break;
	case MYLITE_STATEMENT_REPLACE: desired_token = REPLACE_T; break;
	case MYLITE_STATEMENT_UPDATE: desired_token = UPDATE_T; break;
	case MYLITE_STATEMENT_DELETE: desired_token = DELETE_T; break;
	default:
		return statement->first_token - 1;
	}

	token_index = statement->first_token - 1;
	last_token_index = statement->last_token - 1;
	while (token_index <= last_token_index && token_index < parser->token_count) {
		size_t matching_token = parser->tokens[token_index].matching_token;
		if (matching_token > token_index + 1) {
			token_index = matching_token;
			continue;
		}
		if (parser->tokens[token_index].parser_token == desired_token) {
			return token_index;
		}
		token_index++;
	}
	return parser->token_count;
}

static size_t find_insert_or_replace_name_token(const mylite_parser *parser,
                                                size_t token_index,
                                                size_t last_token_index)
{
	token_index = skip_dml_modifiers(parser, token_index, last_token_index);
	if (token_index <= last_token_index && parser->tokens[token_index].parser_token == INTO_T) {
		token_index++;
	}
	if (token_index <= last_token_index && token_can_start_object_name(&parser->tokens[token_index])) {
		return token_index;
	}
	return parser->token_count;
}

static size_t find_update_name_token(const mylite_parser *parser,
                                     size_t token_index,
                                     size_t last_token_index)
{
	token_index = skip_dml_modifiers(parser, token_index, last_token_index);
	if (token_index <= last_token_index && token_can_start_object_name(&parser->tokens[token_index])) {
		return token_index;
	}
	return parser->token_count;
}

static size_t find_delete_name_token(const mylite_parser *parser,
                                     size_t token_index,
                                     size_t last_token_index)
{
	token_index = skip_dml_modifiers(parser, token_index, last_token_index);
	if (token_index <= last_token_index && parser->tokens[token_index].parser_token == FROM_T) {
		token_index++;
	}
	if (token_index <= last_token_index && token_can_start_object_name(&parser->tokens[token_index])) {
		return token_index;
	}
	return parser->token_count;
}

static size_t skip_dml_modifiers(const mylite_parser *parser,
                                 size_t token_index,
                                 size_t last_token_index)
{
	while (token_index <= last_token_index && is_dml_modifier_token(parser->tokens[token_index].parser_token)) {
		token_index++;
	}
	return token_index;
}

static int is_dml_modifier_token(int token)
{
	return token == LOW_PRIORITY_T ||
	       token == DELAYED_T ||
	       token == HIGH_PRIORITY_T ||
	       token == IGNORE_T ||
	       token == QUICK_T;
}

static int classify_direct_statement_object(const mylite_parser *parser, mylite_statement *statement)
{
	size_t token_index;
	size_t last_token_index;
	size_t name_token_index;
	mylite_statement_object_kind object_kind = MYLITE_STATEMENT_OBJECT_NONE;

	if (statement->first_token == 0 ||
	    statement->last_token < statement->first_token ||
	    statement->first_token > parser->token_count) {
		return 0;
	}

	token_index = statement->first_token - 1;
	last_token_index = statement->last_token - 1;
	name_token_index = token_index + 1;

	switch (statement->kind) {
	case MYLITE_STATEMENT_ALTER:
		return classify_instance_statement_object(parser, statement, name_token_index, last_token_index);
	case MYLITE_STATEMENT_RESTART:
	case MYLITE_STATEMENT_SHUTDOWN:
		return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_INSTANCE);
	case MYLITE_STATEMENT_IMPORT:
		object_kind = MYLITE_STATEMENT_OBJECT_SDI_FILE;
		name_token_index = find_import_sdi_file_token(parser, name_token_index, last_token_index);
		break;
	case MYLITE_STATEMENT_CALL:
		object_kind = MYLITE_STATEMENT_OBJECT_PROCEDURE;
		name_token_index = find_call_procedure_name_token(parser, name_token_index, last_token_index);
		break;
	case MYLITE_STATEMENT_SIGNAL:
	case MYLITE_STATEMENT_RESIGNAL:
		return classify_signal_statement_object(parser, statement, name_token_index, last_token_index);
	case MYLITE_STATEMENT_GET:
		return classify_get_statement_object(parser, statement, name_token_index, last_token_index);
	case MYLITE_STATEMENT_USE:
		object_kind = MYLITE_STATEMENT_OBJECT_DATABASE;
		break;
	case MYLITE_STATEMENT_DESCRIBE:
	case MYLITE_STATEMENT_EXPLAIN:
		return classify_describe_or_explain_statement_object(parser,
		                                                     statement,
		                                                     name_token_index,
		                                                     last_token_index);
	case MYLITE_STATEMENT_HELP:
		if (name_token_index > last_token_index ||
		    name_token_index >= parser->token_count ||
		    parser->tokens[name_token_index].kind != MYLITE_TOKEN_STRING) {
			return 0;
		}
		object_kind = MYLITE_STATEMENT_OBJECT_HELP_TOPIC;
		break;
	case MYLITE_STATEMENT_TABLE:
	case MYLITE_STATEMENT_HANDLER:
		object_kind = MYLITE_STATEMENT_OBJECT_TABLE;
		break;
	case MYLITE_STATEMENT_TRUNCATE:
		object_kind = MYLITE_STATEMENT_OBJECT_TABLE;
		if (name_token_index <= last_token_index &&
		    parser->tokens[name_token_index].parser_token == TABLE_T) {
			name_token_index++;
		}
		break;
	case MYLITE_STATEMENT_ANALYZE:
	case MYLITE_STATEMENT_CHECK:
	case MYLITE_STATEMENT_CHECKSUM:
	case MYLITE_STATEMENT_OPTIMIZE:
	case MYLITE_STATEMENT_REPAIR:
		object_kind = MYLITE_STATEMENT_OBJECT_TABLE;
		name_token_index = find_maintenance_table_name_token(parser, name_token_index, last_token_index);
		break;
	case MYLITE_STATEMENT_LOAD:
		object_kind = MYLITE_STATEMENT_OBJECT_TABLE;
		name_token_index = find_load_table_name_token(parser, name_token_index, last_token_index);
		break;
	case MYLITE_STATEMENT_CACHE:
		object_kind = MYLITE_STATEMENT_OBJECT_TABLE;
		name_token_index = find_cache_index_name_token(parser, name_token_index, last_token_index);
		break;
	case MYLITE_STATEMENT_LOCK:
		if (classify_instance_statement_object(parser, statement, name_token_index, last_token_index)) {
			return 1;
		}
		object_kind = MYLITE_STATEMENT_OBJECT_TABLE;
		name_token_index = find_lock_table_name_token(parser, name_token_index, last_token_index);
		break;
	case MYLITE_STATEMENT_UNLOCK:
		return classify_instance_statement_object(parser, statement, name_token_index, last_token_index);
	case MYLITE_STATEMENT_FLUSH:
		return classify_flush_statement_object(parser, statement, name_token_index, last_token_index);
	case MYLITE_STATEMENT_START:
	case MYLITE_STATEMENT_STOP:
	case MYLITE_STATEMENT_CHANGE:
		return classify_replication_channel_statement_object(parser, statement, name_token_index, last_token_index);
	case MYLITE_STATEMENT_KILL:
		return classify_kill_statement_object(parser, statement, name_token_index, last_token_index);
	case MYLITE_STATEMENT_PURGE:
		return classify_purge_statement_object(parser, statement, name_token_index, last_token_index);
	case MYLITE_STATEMENT_BINLOG:
		if (name_token_index > last_token_index ||
		    name_token_index >= parser->token_count ||
		    parser->tokens[name_token_index].kind != MYLITE_TOKEN_STRING) {
			return 0;
		}
		object_kind = MYLITE_STATEMENT_OBJECT_BINARY_LOG_EVENT;
		break;
	case MYLITE_STATEMENT_RESET:
		return classify_reset_statement_object(parser, statement, name_token_index, last_token_index);
	case MYLITE_STATEMENT_SET:
		return classify_set_statement_object(parser, statement, name_token_index, last_token_index);
	case MYLITE_STATEMENT_INSTALL:
	case MYLITE_STATEMENT_UNINSTALL:
		return classify_install_statement_object(parser, statement, name_token_index, last_token_index);
	case MYLITE_STATEMENT_XA:
		return classify_xa_statement_object(parser, statement, name_token_index, last_token_index);
	case MYLITE_STATEMENT_PREPARE:
	case MYLITE_STATEMENT_EXECUTE:
	case MYLITE_STATEMENT_DEALLOCATE:
		return classify_prepared_statement_object(parser, statement, name_token_index, last_token_index);
	case MYLITE_STATEMENT_GRANT:
	case MYLITE_STATEMENT_REVOKE:
		return classify_principal_statement_object(parser, statement, name_token_index, last_token_index);
	case MYLITE_STATEMENT_SAVEPOINT:
	case MYLITE_STATEMENT_RELEASE:
	case MYLITE_STATEMENT_ROLLBACK:
		return classify_savepoint_statement_object(parser, statement, name_token_index, last_token_index);
	case MYLITE_STATEMENT_DECLARE:
		return classify_declare_statement_object(parser, statement, name_token_index, last_token_index);
	case MYLITE_STATEMENT_OPEN:
	case MYLITE_STATEMENT_FETCH:
	case MYLITE_STATEMENT_CLOSE:
		return classify_cursor_statement_object(parser, statement, name_token_index, last_token_index);
	case MYLITE_STATEMENT_LEAVE:
	case MYLITE_STATEMENT_ITERATE:
		return set_statement_direct_object_name(parser,
		                                        statement,
		                                        MYLITE_STATEMENT_OBJECT_LABEL,
		                                        name_token_index,
		                                        last_token_index);
	case MYLITE_STATEMENT_SHOW:
		return classify_show_statement_object(parser, statement, name_token_index, last_token_index);
	default:
		return 0;
	}

	return set_statement_direct_object_name(parser, statement, object_kind, name_token_index, last_token_index);
}

static size_t find_import_sdi_file_token(const mylite_parser *parser,
                                         size_t token_index,
                                         size_t last_token_index)
{
	while (token_index + 1 <= last_token_index && token_index < parser->token_count) {
		if (parser->tokens[token_index].parser_token == FROM_T &&
		    parser->tokens[token_index + 1].kind == MYLITE_TOKEN_STRING) {
			return token_index + 1;
		}
		token_index++;
	}
	return parser->token_count;
}

static size_t find_call_procedure_name_token(const mylite_parser *parser,
                                             size_t token_index,
                                             size_t last_token_index)
{
	if (token_index <= last_token_index &&
	    token_index < parser->token_count &&
	    token_can_continue_object_name(&parser->tokens[token_index])) {
		return token_index;
	}
	return parser->token_count;
}

static int classify_signal_statement_object(const mylite_parser *parser,
                                            mylite_statement *statement,
                                            size_t token_index,
                                            size_t last_token_index)
{
	return classify_condition_value_statement_object(parser, statement, token_index, last_token_index, 0);
}

static int classify_condition_value_statement_object(const mylite_parser *parser,
                                                     mylite_statement *statement,
                                                     size_t token_index,
                                                     size_t last_token_index,
                                                     int allow_condition_class)
{
	if (token_index > last_token_index || token_index >= parser->token_count) {
		return 0;
	}

	if (token_text_equals(parser, token_index, "SET")) {
		return 0;
	}

	if (token_text_equals(parser, token_index, "SQLSTATE")) {
		size_t name_token_index = token_index + 1;

		if (name_token_index <= last_token_index &&
		    token_text_equals(parser, name_token_index, "VALUE")) {
			name_token_index++;
		}
		if (name_token_index > last_token_index ||
		    name_token_index >= parser->token_count ||
		    parser->tokens[name_token_index].kind != MYLITE_TOKEN_STRING) {
			return 0;
		}
		return set_statement_direct_object_name(parser,
		                                        statement,
		                                        MYLITE_STATEMENT_OBJECT_SQLSTATE,
		                                        name_token_index,
		                                        last_token_index);
	}

	if (allow_condition_class &&
	    token_index + 1 <= last_token_index &&
	    token_text_equals(parser, token_index, "NOT") &&
	    token_text_equals(parser, token_index + 1, "FOUND")) {
		return set_statement_direct_object_name_range(parser,
		                                              statement,
		                                              MYLITE_STATEMENT_OBJECT_CONDITION,
		                                              token_index,
		                                              token_index + 1);
	}

	if (allow_condition_class && parser->tokens[token_index].kind == MYLITE_TOKEN_NUMBER) {
		return set_statement_direct_object_name(parser,
		                                        statement,
		                                        MYLITE_STATEMENT_OBJECT_CONDITION,
		                                        token_index,
		                                        last_token_index);
	}

	if (!token_can_continue_object_name(&parser->tokens[token_index])) {
		return 0;
	}
	return set_statement_direct_object_name(parser,
	                                        statement,
	                                        MYLITE_STATEMENT_OBJECT_CONDITION,
	                                        token_index,
	                                        last_token_index);
}

static int classify_get_statement_object(const mylite_parser *parser,
                                         mylite_statement *statement,
                                         size_t token_index,
                                         size_t last_token_index)
{
	while (token_index <= last_token_index && token_index < parser->token_count) {
		if (token_text_equals(parser, token_index, "DIAGNOSTICS")) {
			token_index++;
			break;
		}
		token_index++;
	}

	while (token_index + 1 <= last_token_index && token_index < parser->token_count) {
		if (token_text_equals(parser, token_index, "CONDITION") &&
		    token_can_start_diagnostics_condition_number(&parser->tokens[token_index + 1])) {
			return set_statement_direct_object_name_range(parser,
			                                              statement,
			                                              MYLITE_STATEMENT_OBJECT_DIAGNOSTICS_CONDITION,
			                                              token_index + 1,
			                                              token_index + 1);
		}
		token_index++;
	}
	return 0;
}

static int token_can_start_diagnostics_condition_number(const mylite_token *token)
{
	return token->kind == MYLITE_TOKEN_IDENTIFIER ||
	       token->kind == MYLITE_TOKEN_QUOTED_IDENTIFIER ||
	       token->kind == MYLITE_TOKEN_NUMBER ||
	       token->kind == MYLITE_TOKEN_USER_VARIABLE ||
	       token->kind == MYLITE_TOKEN_SYSTEM_VARIABLE;
}

static int classify_describe_or_explain_statement_object(const mylite_parser *parser,
                                                         mylite_statement *statement,
                                                         size_t token_index,
                                                         size_t last_token_index)
{
	size_t name_token_index = find_describe_or_explain_table_name_token(parser,
	                                                                   token_index,
	                                                                   last_token_index);
	if (name_token_index < parser->token_count) {
		return set_statement_direct_object_name(parser,
		                                        statement,
		                                        MYLITE_STATEMENT_OBJECT_TABLE,
		                                        name_token_index,
		                                        last_token_index);
	}

	name_token_index = find_explain_connection_id_token(parser, token_index, last_token_index);
	return set_statement_direct_object_name(parser,
	                                        statement,
	                                        MYLITE_STATEMENT_OBJECT_CONNECTION,
	                                        name_token_index,
	                                        last_token_index);
}

static size_t find_explain_connection_id_token(const mylite_parser *parser,
                                               size_t token_index,
                                               size_t last_token_index)
{
	while (token_index + 2 <= last_token_index && token_index < parser->token_count) {
		if (parser->tokens[token_index].parser_token == SELECT_T ||
		    parser->tokens[token_index].parser_token == TABLE_T ||
		    parser->tokens[token_index].parser_token == DELETE_T ||
		    parser->tokens[token_index].parser_token == INSERT_T ||
		    parser->tokens[token_index].parser_token == REPLACE_T ||
		    parser->tokens[token_index].parser_token == UPDATE_T ||
		    parser->tokens[token_index].parser_token == ANALYZE_T) {
			return parser->token_count;
		}
		if (token_text_equals(parser, token_index, "FOR") &&
		    token_text_equals(parser, token_index + 1, "CONNECTION") &&
		    token_can_start_object_name(&parser->tokens[token_index + 2])) {
			return token_index + 2;
		}
		token_index++;
	}
	return parser->token_count;
}

static size_t find_describe_or_explain_table_name_token(const mylite_parser *parser,
                                                        size_t token_index,
                                                        size_t last_token_index)
{
	if (token_index > last_token_index ||
	    token_index >= parser->token_count ||
	    token_text_equals(parser, token_index, "FOR") ||
	    token_text_equals(parser, token_index, "FORMAT") ||
	    token_text_equals(parser, token_index, "EXTENDED") ||
	    token_text_equals(parser, token_index, "PARTITIONS") ||
	    token_text_equals(parser, token_index, "CONNECTION")) {
		return parser->token_count;
	}
	if (token_can_start_object_name(&parser->tokens[token_index])) {
		return token_index;
	}
	return parser->token_count;
}

static size_t find_load_table_name_token(const mylite_parser *parser,
                                         size_t token_index,
                                         size_t last_token_index)
{
	if (token_index + 3 <= last_token_index &&
	    parser->tokens[token_index].parser_token == INDEX_T &&
	    parser->tokens[token_index + 1].parser_token == INTO_T &&
	    parser->tokens[token_index + 2].parser_token == CACHE_T &&
	    token_can_start_object_name(&parser->tokens[token_index + 3])) {
		return token_index + 3;
	}

	while (token_index + 2 <= last_token_index && token_index < parser->token_count) {
		if (parser->tokens[token_index].parser_token == INTO_T &&
		    parser->tokens[token_index + 1].parser_token == TABLE_T &&
		    token_can_start_object_name(&parser->tokens[token_index + 2])) {
			return token_index + 2;
		}
		token_index++;
	}
	return parser->token_count;
}

static size_t find_cache_index_name_token(const mylite_parser *parser,
                                          size_t token_index,
                                          size_t last_token_index)
{
	if (token_index + 1 <= last_token_index &&
	    token_index + 1 < parser->token_count &&
	    parser->tokens[token_index].parser_token == INDEX_T &&
	    token_can_start_object_name(&parser->tokens[token_index + 1])) {
		return token_index + 1;
	}
	return parser->token_count;
}

static size_t find_lock_table_name_token(const mylite_parser *parser,
                                         size_t token_index,
                                         size_t last_token_index)
{
	if (token_index > last_token_index ||
	    token_index >= parser->token_count ||
	    (parser->tokens[token_index].parser_token != TABLE_T &&
	     !token_text_equals(parser, token_index, "TABLES"))) {
		return parser->token_count;
	}

	token_index++;
	if (token_index <= last_token_index && token_can_start_object_name(&parser->tokens[token_index])) {
		return token_index;
	}
	return parser->token_count;
}

static size_t find_flush_table_name_token(const mylite_parser *parser,
                                          size_t token_index,
                                          size_t last_token_index)
{
	if (token_index > last_token_index ||
	    token_index >= parser->token_count ||
	    (parser->tokens[token_index].parser_token != TABLE_T &&
	     !token_text_equals(parser, token_index, "TABLES"))) {
		return parser->token_count;
	}

	token_index++;
	if (token_index <= last_token_index && token_can_start_object_name(&parser->tokens[token_index])) {
		return token_index;
	}
	return parser->token_count;
}

static int classify_flush_statement_object(const mylite_parser *parser,
                                           mylite_statement *statement,
                                           size_t token_index,
                                           size_t last_token_index)
{
	size_t name_token_index;

	if (token_index + 1 <= last_token_index &&
	    token_text_equals(parser, token_index, "RELAY") &&
	    token_text_equals(parser, token_index + 1, "LOGS")) {
		name_token_index = find_replication_channel_name_token(parser, token_index + 2, last_token_index);
		return set_statement_direct_object_name(parser,
		                                        statement,
		                                        MYLITE_STATEMENT_OBJECT_REPLICATION_CHANNEL,
		                                        name_token_index,
		                                        last_token_index);
	}

	name_token_index = find_flush_table_name_token(parser, token_index, last_token_index);
	return set_statement_direct_object_name(parser,
	                                        statement,
	                                        MYLITE_STATEMENT_OBJECT_TABLE,
	                                        name_token_index,
	                                        last_token_index);
}

static size_t find_maintenance_table_name_token(const mylite_parser *parser,
                                                size_t token_index,
                                                size_t last_token_index)
{
	while (token_index + 1 <= last_token_index && token_index < parser->token_count) {
		if (parser->tokens[token_index].parser_token == TABLE_T &&
		    token_can_start_object_name(&parser->tokens[token_index + 1])) {
			return token_index + 1;
		}
		token_index++;
	}
	return parser->token_count;
}

static int classify_instance_statement_object(const mylite_parser *parser,
                                              mylite_statement *statement,
                                              size_t token_index,
                                              size_t last_token_index)
{
	if (token_index <= last_token_index &&
	    token_index < parser->token_count &&
	    token_text_equals(parser, token_index, "INSTANCE")) {
		return set_statement_direct_object(statement, MYLITE_STATEMENT_OBJECT_INSTANCE);
	}
	return 0;
}

static int classify_kill_statement_object(const mylite_parser *parser,
                                          mylite_statement *statement,
                                          size_t token_index,
                                          size_t last_token_index)
{
	if (token_index > last_token_index || token_index >= parser->token_count) {
		return 0;
	}

	if (token_text_equals(parser, token_index, "QUERY") ||
	    token_text_equals(parser, token_index, "CONNECTION")) {
		token_index++;
	}

	return set_statement_direct_object_name(parser,
	                                        statement,
	                                        MYLITE_STATEMENT_OBJECT_CONNECTION,
	                                        token_index,
	                                        last_token_index);
}

static int classify_purge_statement_object(const mylite_parser *parser,
                                           mylite_statement *statement,
                                           size_t token_index,
                                           size_t last_token_index)
{
	size_t name_token_index;

	if (token_index + 3 > last_token_index ||
	    token_index + 3 >= parser->token_count ||
	    (!token_text_equals(parser, token_index, "BINARY") &&
	     !token_text_equals(parser, token_index, "MASTER")) ||
	    !token_text_equals(parser, token_index + 1, "LOGS")) {
		return 0;
	}

	name_token_index = token_index + 2;
	while (name_token_index + 1 <= last_token_index && name_token_index < parser->token_count) {
		if (parser->tokens[name_token_index].parser_token == TO_T &&
		    token_can_start_object_name(&parser->tokens[name_token_index + 1])) {
			return set_statement_direct_object_name(parser,
			                                        statement,
			                                        MYLITE_STATEMENT_OBJECT_BINARY_LOG,
			                                        name_token_index + 1,
			                                        last_token_index);
		}
		name_token_index++;
	}

	return 0;
}

static int classify_replication_channel_statement_object(const mylite_parser *parser,
                                                         mylite_statement *statement,
                                                         size_t token_index,
                                                         size_t last_token_index)
{
	size_t name_token_index = find_replication_channel_name_token(parser, token_index, last_token_index);

	return set_statement_direct_object_name(parser,
	                                        statement,
	                                        MYLITE_STATEMENT_OBJECT_REPLICATION_CHANNEL,
	                                        name_token_index,
	                                        last_token_index);
}

static int classify_reset_statement_object(const mylite_parser *parser,
                                           mylite_statement *statement,
                                           size_t token_index,
                                           size_t last_token_index)
{
	size_t name_token_index;

	if (token_index > last_token_index ||
	    token_index >= parser->token_count) {
		return 0;
	}

	if (!token_text_equals(parser, token_index, "PERSIST")) {
		name_token_index = find_replication_channel_name_token(parser, token_index, last_token_index);
		return set_statement_direct_object_name(parser,
		                                        statement,
		                                        MYLITE_STATEMENT_OBJECT_REPLICATION_CHANNEL,
		                                        name_token_index,
		                                        last_token_index);
	}

	token_index++;
	if (token_index + 2 <= last_token_index &&
	    parser->tokens[token_index].parser_token == IF_T &&
	    parser->tokens[token_index + 1].parser_token == EXISTS_T) {
		token_index += 2;
	}

	name_token_index = token_index;
	return set_statement_direct_object_name(parser,
	                                        statement,
	                                        MYLITE_STATEMENT_OBJECT_SYSTEM_VARIABLE,
	                                        name_token_index,
	                                        last_token_index);
}

static size_t find_replication_channel_name_token(const mylite_parser *parser,
                                                  size_t token_index,
                                                  size_t last_token_index)
{
	while (token_index + 2 <= last_token_index && token_index < parser->token_count) {
		if (token_text_equals(parser, token_index, "FOR") &&
		    token_text_equals(parser, token_index + 1, "CHANNEL") &&
		    token_can_start_object_name(&parser->tokens[token_index + 2])) {
			return token_index + 2;
		}
		token_index++;
	}
	return parser->token_count;
}

static int classify_set_statement_object(const mylite_parser *parser,
                                         mylite_statement *statement,
                                         size_t token_index,
                                         size_t last_token_index)
{
	size_t name_token_index;

	if (token_index > last_token_index || token_index >= parser->token_count) {
		return 0;
	}

	if (parser->tokens[token_index].parser_token == ROLE_T) {
		name_token_index = find_set_role_name_token(parser, token_index + 1, last_token_index);
		if (name_token_index >= parser->token_count) {
			return 0;
		}
		statement->object_kind = MYLITE_STATEMENT_OBJECT_ROLE;
		set_statement_account_name_from_first_token(parser, statement, name_token_index, last_token_index);
		return 1;
	}

	if (token_text_equals(parser, token_index, "RESOURCE") &&
	    token_index + 2 <= last_token_index &&
	    parser->tokens[token_index + 1].parser_token == GROUP_T &&
	    token_can_start_object_name(&parser->tokens[token_index + 2])) {
		statement->object_kind = MYLITE_STATEMENT_OBJECT_RESOURCE_GROUP;
		set_statement_object_name_from_first_token(parser, statement, token_index + 2, last_token_index);
		return 1;
	}

	if (parser->tokens[token_index].parser_token == DEFAULT_T &&
	    token_index + 1 <= last_token_index &&
	    parser->tokens[token_index + 1].parser_token == ROLE_T) {
		name_token_index = find_set_role_name_token(parser, token_index + 2, last_token_index);
		if (name_token_index < parser->token_count) {
			statement->object_kind = MYLITE_STATEMENT_OBJECT_ROLE;
			set_statement_account_name_from_first_token(parser, statement, name_token_index, last_token_index);
			return 1;
		}

		name_token_index = find_set_default_role_user_name_token(parser, token_index + 2, last_token_index);
		if (name_token_index >= parser->token_count) {
			return 0;
		}
		statement->object_kind = MYLITE_STATEMENT_OBJECT_USER;
		set_statement_account_name_from_first_token(parser, statement, name_token_index, last_token_index);
		return 1;
	}

	if (token_text_equals(parser, token_index, "PASSWORD")) {
		name_token_index = find_set_password_name_token(parser, token_index + 1, last_token_index);
		if (name_token_index >= parser->token_count) {
			return 0;
		}
		statement->object_kind = MYLITE_STATEMENT_OBJECT_USER;
		set_statement_account_name_from_first_token(parser, statement, name_token_index, last_token_index);
		return 1;
	}

	return 0;
}

static size_t find_set_role_name_token(const mylite_parser *parser,
                                       size_t token_index,
                                       size_t last_token_index)
{
	if (token_index > last_token_index || token_index >= parser->token_count) {
		return parser->token_count;
	}

	if (parser->tokens[token_index].parser_token == ALL_T) {
		if (token_index + 2 <= last_token_index &&
		    token_text_equals(parser, token_index + 1, "EXCEPT") &&
		    token_can_start_object_name(&parser->tokens[token_index + 2])) {
			return token_index + 2;
		}
		return parser->token_count;
	}

	if (parser->tokens[token_index].parser_token == DEFAULT_T ||
	    token_text_equals(parser, token_index, "NONE")) {
		return parser->token_count;
	}

	if (token_can_start_object_name(&parser->tokens[token_index])) {
		return token_index;
	}
	return parser->token_count;
}

static size_t find_set_default_role_user_name_token(const mylite_parser *parser,
                                                    size_t token_index,
                                                    size_t last_token_index)
{
	while (token_index + 1 <= last_token_index && token_index < parser->token_count) {
		if (parser->tokens[token_index].parser_token == TO_T &&
		    token_can_start_object_name(&parser->tokens[token_index + 1])) {
			return token_index + 1;
		}
		token_index++;
	}
	return parser->token_count;
}

static size_t find_set_password_name_token(const mylite_parser *parser,
                                           size_t token_index,
                                           size_t last_token_index)
{
	if (token_index + 1 <= last_token_index &&
	    token_text_equals(parser, token_index, "FOR") &&
	    token_can_start_object_name(&parser->tokens[token_index + 1])) {
		return token_index + 1;
	}
	return parser->token_count;
}

static int classify_install_statement_object(const mylite_parser *parser,
                                             mylite_statement *statement,
                                             size_t token_index,
                                             size_t last_token_index)
{
	mylite_statement_object_kind object_kind;

	if (token_index + 1 > last_token_index ||
	    token_index + 1 >= parser->token_count ||
	    !token_can_start_object_name(&parser->tokens[token_index + 1])) {
		return 0;
	}

	if (token_text_equals(parser, token_index, "COMPONENT")) {
		object_kind = MYLITE_STATEMENT_OBJECT_COMPONENT;
	} else if (token_text_equals(parser, token_index, "PLUGIN")) {
		object_kind = MYLITE_STATEMENT_OBJECT_PLUGIN;
	} else {
		return 0;
	}

	return set_statement_direct_object_name(parser,
	                                        statement,
	                                        object_kind,
	                                        token_index + 1,
	                                        last_token_index);
}

static int classify_xa_statement_object(const mylite_parser *parser,
                                        mylite_statement *statement,
                                        size_t token_index,
                                        size_t last_token_index)
{
	if (token_index + 1 > last_token_index ||
	    token_index + 1 >= parser->token_count ||
	    !token_can_start_object_name(&parser->tokens[token_index + 1])) {
		return 0;
	}

	if (parser->tokens[token_index].parser_token != START_T &&
	    parser->tokens[token_index].parser_token != END_T &&
	    parser->tokens[token_index].parser_token != PREPARE_T &&
	    parser->tokens[token_index].parser_token != COMMIT_T &&
	    parser->tokens[token_index].parser_token != ROLLBACK_T) {
		return 0;
	}

	return set_statement_direct_object_name(parser,
	                                        statement,
	                                        MYLITE_STATEMENT_OBJECT_XA_TRANSACTION,
	                                        token_index + 1,
	                                        last_token_index);
}

static int classify_prepared_statement_object(const mylite_parser *parser,
                                              mylite_statement *statement,
                                              size_t token_index,
                                              size_t last_token_index)
{
	if (statement->kind == MYLITE_STATEMENT_DEALLOCATE &&
	    token_index <= last_token_index &&
	    token_index < parser->token_count &&
	    parser->tokens[token_index].parser_token == PREPARE_T) {
		token_index++;
	}

	return set_statement_direct_object_name(parser,
	                                        statement,
	                                        MYLITE_STATEMENT_OBJECT_PREPARED_STATEMENT,
	                                        token_index,
	                                        last_token_index);
}

static int classify_principal_statement_object(const mylite_parser *parser,
                                               mylite_statement *statement,
                                               size_t token_index,
                                               size_t last_token_index)
{
	int marker_token = statement->kind == MYLITE_STATEMENT_GRANT ? TO_T : FROM_T;
	size_t name_token_index = find_principal_name_token(parser, token_index, last_token_index, marker_token);

	if (name_token_index > last_token_index || name_token_index >= parser->token_count) {
		return 0;
	}

	statement->object_kind = MYLITE_STATEMENT_OBJECT_USER;
	set_statement_account_name_from_first_token(parser, statement, name_token_index, last_token_index);
	return 1;
}

static int classify_savepoint_statement_object(const mylite_parser *parser,
                                               mylite_statement *statement,
                                               size_t token_index,
                                               size_t last_token_index)
{
	size_t name_token_index;

	if (statement->kind == MYLITE_STATEMENT_SAVEPOINT) {
		name_token_index = token_index;
	} else {
		name_token_index = find_savepoint_name_token(parser, token_index, last_token_index);
	}

	return set_statement_direct_object_name(parser,
	                                        statement,
	                                        MYLITE_STATEMENT_OBJECT_SAVEPOINT,
	                                        name_token_index,
	                                        last_token_index);
}

static int classify_declare_statement_object(const mylite_parser *parser,
                                             mylite_statement *statement,
                                             size_t token_index,
                                             size_t last_token_index)
{
	size_t handler_condition_token;

	if (token_index + 1 <= last_token_index &&
	    token_index + 1 < parser->token_count &&
	    token_can_continue_object_name(&parser->tokens[token_index]) &&
	    token_text_equals(parser, token_index + 1, "CONDITION")) {
		return set_statement_direct_object_name(parser,
		                                        statement,
		                                        MYLITE_STATEMENT_OBJECT_CONDITION,
		                                        token_index,
		                                        last_token_index);
	}

	handler_condition_token = find_declare_handler_condition_token(parser, token_index, last_token_index);
	if (handler_condition_token < parser->token_count) {
		return classify_condition_value_statement_object(parser,
		                                                 statement,
		                                                 handler_condition_token,
		                                                 last_token_index,
		                                                 1);
	}

	if (!statement_contains_token(parser, token_index, last_token_index, CURSOR_T)) {
		return 0;
	}
	return classify_cursor_statement_object(parser, statement, token_index, last_token_index);
}

static size_t find_declare_handler_condition_token(const mylite_parser *parser,
                                                   size_t token_index,
                                                   size_t last_token_index)
{
	while (token_index <= last_token_index && token_index < parser->token_count) {
		if (parser->tokens[token_index].parser_token == HANDLER_T) {
			token_index++;
			break;
		}
		token_index++;
	}
	while (token_index + 1 <= last_token_index && token_index < parser->token_count) {
		if (token_text_equals(parser, token_index, "FOR")) {
			return token_index + 1;
		}
		token_index++;
	}
	return parser->token_count;
}

static int classify_cursor_statement_object(const mylite_parser *parser,
                                            mylite_statement *statement,
                                            size_t token_index,
                                            size_t last_token_index)
{
	return set_statement_direct_object_name(parser,
	                                        statement,
	                                        MYLITE_STATEMENT_OBJECT_CURSOR,
	                                        token_index,
	                                        last_token_index);
}

static int statement_contains_token(const mylite_parser *parser,
                                    size_t token_index,
                                    size_t last_token_index,
                                    int wanted_token)
{
	while (token_index <= last_token_index && token_index < parser->token_count) {
		if (parser->tokens[token_index].parser_token == wanted_token) {
			return 1;
		}
		token_index++;
	}
	return 0;
}

static size_t find_savepoint_name_token(const mylite_parser *parser,
                                        size_t token_index,
                                        size_t last_token_index)
{
	while (token_index + 1 <= last_token_index && token_index < parser->token_count) {
		if (parser->tokens[token_index].parser_token == SAVEPOINT_T &&
		    token_can_start_object_name(&parser->tokens[token_index + 1])) {
			return token_index + 1;
		}
		token_index++;
	}
	return parser->token_count;
}

static size_t find_principal_name_token(const mylite_parser *parser,
                                        size_t token_index,
                                        size_t last_token_index,
                                        int marker_token)
{
	while (token_index + 1 <= last_token_index && token_index < parser->token_count) {
		if (parser->tokens[token_index].parser_token == marker_token &&
		    token_can_start_object_name(&parser->tokens[token_index + 1])) {
			return token_index + 1;
		}
		token_index++;
	}
	return parser->token_count;
}

static void set_statement_account_name_from_first_token(const mylite_parser *parser,
                                                        mylite_statement *statement,
                                                        size_t first_name_token,
                                                        size_t last_token_index)
{
	size_t last_name_token;
	const mylite_token *first;
	const mylite_token *last;

	if (first_name_token >= parser->token_count) {
		return;
	}

	last_name_token = last_account_name_token(parser, first_name_token, last_token_index);
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

static size_t last_account_name_token(const mylite_parser *parser,
                                      size_t first_name_token,
                                      size_t last_token_index)
{
	if (first_name_token + 2 <= last_token_index &&
	    token_is_account_at_marker(parser, first_name_token + 1) &&
	    token_can_start_object_name(&parser->tokens[first_name_token + 2])) {
		return first_name_token + 2;
	}
	return first_name_token;
}

static int token_is_account_at_marker(const mylite_parser *parser, size_t token_index)
{
	return token_index < parser->token_count &&
	       parser->tokens[token_index].parser_token == USER_VARIABLE &&
	       parser->tokens[token_index].end_offset - parser->tokens[token_index].start_offset == 1 &&
	       parser->lexer.input[parser->tokens[token_index].start_offset] == '@';
}

static int classify_show_statement_object(const mylite_parser *parser,
                                          mylite_statement *statement,
                                          size_t token_index,
                                          size_t last_token_index)
{
	mylite_statement_object_kind object_kind;
	size_t name_token_index;

	token_index = skip_show_modifiers(parser, token_index, last_token_index);
	if (token_index > last_token_index || token_index >= parser->token_count) {
		return 0;
	}

	if (parser->tokens[token_index].parser_token == CREATE_T && token_index + 1 <= last_token_index) {
		size_t object_token_index = token_index + 1;
		object_kind = object_kind_from_token_sequence(parser, object_token_index, last_token_index);
		if (object_kind == MYLITE_STATEMENT_OBJECT_NONE) {
			return 0;
		}
		name_token_index = first_name_token_after_object(parser, object_token_index, last_token_index);
		return set_statement_direct_object_name(parser,
		                                        statement,
		                                        object_kind,
		                                        name_token_index,
		                                        last_token_index);
	}

	if (token_text_equals(parser, token_index, "GRANTS")) {
		name_token_index = find_show_grants_name_token(parser, token_index + 1, last_token_index);
		return set_statement_direct_object_name(parser,
		                                        statement,
		                                        MYLITE_STATEMENT_OBJECT_USER,
		                                        name_token_index,
		                                        last_token_index);
	}

	if (parser->tokens[token_index].parser_token == BINLOG_T &&
	    token_index + 1 <= last_token_index &&
	    token_text_equals(parser, token_index + 1, "EVENTS")) {
		name_token_index = find_show_binlog_events_name_token(parser, token_index + 2, last_token_index);
		return set_statement_direct_object_name(parser,
		                                        statement,
		                                        MYLITE_STATEMENT_OBJECT_BINARY_LOG,
		                                        name_token_index,
		                                        last_token_index);
	}

	if (token_text_equals(parser, token_index, "RELAYLOG") &&
	    token_index + 1 <= last_token_index &&
	    token_text_equals(parser, token_index + 1, "EVENTS")) {
		name_token_index = find_show_binlog_events_name_token(parser, token_index + 2, last_token_index);
		if (name_token_index < parser->token_count) {
			return set_statement_direct_object_name(parser,
			                                        statement,
			                                        MYLITE_STATEMENT_OBJECT_RELAY_LOG,
			                                        name_token_index,
			                                        last_token_index);
		}
		name_token_index = find_replication_channel_name_token(parser, token_index + 2, last_token_index);
		return set_statement_direct_object_name(parser,
		                                        statement,
		                                        MYLITE_STATEMENT_OBJECT_REPLICATION_CHANNEL,
		                                        name_token_index,
		                                        last_token_index);
	}

	if (token_text_equals(parser, token_index, "REPLICA") &&
	    token_index + 1 <= last_token_index &&
	    token_text_equals(parser, token_index + 1, "STATUS")) {
		name_token_index = find_replication_channel_name_token(parser, token_index + 2, last_token_index);
		return set_statement_direct_object_name(parser,
		                                        statement,
		                                        MYLITE_STATEMENT_OBJECT_REPLICATION_CHANNEL,
		                                        name_token_index,
		                                        last_token_index);
	}

	if (token_text_equals(parser, token_index, "PROFILE")) {
		name_token_index = find_show_profile_query_id_token(parser, token_index + 1, last_token_index);
		return set_statement_direct_object_name(parser,
		                                        statement,
		                                        MYLITE_STATEMENT_OBJECT_QUERY,
		                                        name_token_index,
		                                        last_token_index);
	}

	if (parser->tokens[token_index].parser_token == ENGINE_T &&
	    token_index + 2 <= last_token_index &&
	    token_can_start_object_name(&parser->tokens[token_index + 1]) &&
	    (token_text_equals(parser, token_index + 2, "STATUS") ||
	     token_text_equals(parser, token_index + 2, "MUTEX"))) {
		return set_statement_direct_object_name(parser,
		                                        statement,
		                                        MYLITE_STATEMENT_OBJECT_ENGINE,
		                                        token_index + 1,
		                                        last_token_index);
	}

	if ((parser->tokens[token_index].parser_token == FUNCTION_T ||
	     parser->tokens[token_index].parser_token == PROCEDURE_T) &&
	    token_index + 1 <= last_token_index &&
	    token_text_equals(parser, token_index + 1, "CODE")) {
		object_kind = parser->tokens[token_index].parser_token == FUNCTION_T ?
			MYLITE_STATEMENT_OBJECT_FUNCTION :
			MYLITE_STATEMENT_OBJECT_PROCEDURE;
		return set_statement_direct_object_name(parser,
		                                        statement,
		                                        object_kind,
		                                        token_index + 2,
		                                        last_token_index);
	}

	if (token_text_equals(parser, token_index, "COLUMNS") ||
	    token_text_equals(parser, token_index, "FIELDS")) {
		name_token_index = find_show_from_name_token(parser, token_index + 1, last_token_index);
		return set_statement_direct_object_name(parser,
		                                        statement,
		                                        MYLITE_STATEMENT_OBJECT_TABLE,
		                                        name_token_index,
		                                        last_token_index);
	}

	if (parser->tokens[token_index].parser_token == INDEX_T ||
	    parser->tokens[token_index].parser_token == KEY_T ||
	    token_text_equals(parser, token_index, "INDEXES") ||
	    token_text_equals(parser, token_index, "KEYS")) {
		name_token_index = find_show_from_name_token(parser, token_index + 1, last_token_index);
		return set_statement_direct_object_name(parser,
		                                        statement,
		                                        MYLITE_STATEMENT_OBJECT_TABLE,
		                                        name_token_index,
		                                        last_token_index);
	}

	if (token_text_equals(parser, token_index, "TABLES")) {
		name_token_index = find_show_from_name_token(parser, token_index + 1, last_token_index);
		return set_statement_direct_object_name(parser,
		                                        statement,
		                                        MYLITE_STATEMENT_OBJECT_DATABASE,
		                                        name_token_index,
		                                        last_token_index);
	}

	if (parser->tokens[token_index].parser_token == TABLE_T &&
	    token_index + 1 <= last_token_index &&
	    token_text_equals(parser, token_index + 1, "STATUS")) {
		name_token_index = find_show_from_name_token(parser, token_index + 2, last_token_index);
		return set_statement_direct_object_name(parser,
		                                        statement,
		                                        MYLITE_STATEMENT_OBJECT_DATABASE,
		                                        name_token_index,
		                                        last_token_index);
	}

	if (parser->tokens[token_index].parser_token == OPEN_T &&
	    token_index + 1 <= last_token_index &&
	    token_text_equals(parser, token_index + 1, "TABLES")) {
		name_token_index = find_show_from_name_token(parser, token_index + 2, last_token_index);
		return set_statement_direct_object_name(parser,
		                                        statement,
		                                        MYLITE_STATEMENT_OBJECT_DATABASE,
		                                        name_token_index,
		                                        last_token_index);
	}

	if (token_text_equals(parser, token_index, "EVENTS") ||
	    token_text_equals(parser, token_index, "TRIGGERS")) {
		name_token_index = find_show_from_name_token(parser, token_index + 1, last_token_index);
		return set_statement_direct_object_name(parser,
		                                        statement,
		                                        MYLITE_STATEMENT_OBJECT_DATABASE,
		                                        name_token_index,
		                                        last_token_index);
	}

	return 0;
}

static size_t find_show_profile_query_id_token(const mylite_parser *parser,
                                               size_t token_index,
                                               size_t last_token_index)
{
	while (token_index + 2 <= last_token_index && token_index < parser->token_count) {
		if (token_text_equals(parser, token_index, "FOR") &&
		    token_text_equals(parser, token_index + 1, "QUERY") &&
		    parser->tokens[token_index + 2].kind == MYLITE_TOKEN_NUMBER) {
			return token_index + 2;
		}
		token_index++;
	}
	return parser->token_count;
}

static size_t find_show_binlog_events_name_token(const mylite_parser *parser,
                                                 size_t token_index,
                                                 size_t last_token_index)
{
	while (token_index + 1 <= last_token_index && token_index < parser->token_count) {
		if (parser->tokens[token_index].parser_token == IN_T &&
		    token_can_start_object_name(&parser->tokens[token_index + 1])) {
			return token_index + 1;
		}
		token_index++;
	}
	return parser->token_count;
}

static size_t skip_show_modifiers(const mylite_parser *parser,
                                  size_t token_index,
                                  size_t last_token_index)
{
	while (token_index <= last_token_index && is_show_modifier_token(parser, token_index)) {
		token_index++;
	}
	return token_index;
}

static int is_show_modifier_token(const mylite_parser *parser, size_t token_index)
{
	return token_text_equals(parser, token_index, "FULL") ||
	       token_text_equals(parser, token_index, "EXTENDED");
}

static size_t find_show_from_name_token(const mylite_parser *parser,
                                        size_t token_index,
                                        size_t last_token_index)
{
	while (token_index + 1 <= last_token_index && token_index < parser->token_count) {
		if ((parser->tokens[token_index].parser_token == FROM_T ||
		     parser->tokens[token_index].parser_token == IN_T) &&
		    token_can_start_object_name(&parser->tokens[token_index + 1])) {
			return token_index + 1;
		}
		token_index++;
	}
	return parser->token_count;
}

static size_t find_show_grants_name_token(const mylite_parser *parser,
                                          size_t token_index,
                                          size_t last_token_index)
{
	while (token_index + 1 <= last_token_index && token_index < parser->token_count) {
		if (token_text_equals(parser, token_index, "FOR") &&
		    token_can_start_object_name(&parser->tokens[token_index + 1])) {
			return token_index + 1;
		}
		token_index++;
	}
	return parser->token_count;
}

static int set_statement_direct_object(mylite_statement *statement, mylite_statement_object_kind object_kind)
{
	statement->object_kind = object_kind;
	return 1;
}

static int set_statement_direct_object_name(const mylite_parser *parser,
                                            mylite_statement *statement,
                                            mylite_statement_object_kind object_kind,
                                            size_t name_token_index,
                                            size_t last_token_index)
{
	if (name_token_index > last_token_index ||
	    name_token_index >= parser->token_count ||
	    !token_can_start_object_name(&parser->tokens[name_token_index])) {
		return 0;
	}

	statement->object_kind = object_kind;
	if (object_kind == MYLITE_STATEMENT_OBJECT_USER ||
	    object_kind == MYLITE_STATEMENT_OBJECT_ROLE) {
		set_statement_account_name_from_first_token(parser, statement, name_token_index, last_token_index);
	} else {
		set_statement_object_name_from_first_token(parser, statement, name_token_index, last_token_index);
	}
	return 1;
}

static int set_statement_direct_object_name_range(const mylite_parser *parser,
                                                  mylite_statement *statement,
                                                  mylite_statement_object_kind object_kind,
                                                  size_t first_name_token,
                                                  size_t last_name_token)
{
	const mylite_token *first;
	const mylite_token *last;

	if (first_name_token > last_name_token ||
	    last_name_token >= parser->token_count) {
		return 0;
	}

	first = &parser->tokens[first_name_token];
	last = &parser->tokens[last_name_token];
	statement->object_kind = object_kind;
	statement->object_name_first_token = first_name_token + 1;
	statement->object_name_last_token = last_name_token + 1;
	statement->object_name_start_offset = first->start_offset;
	statement->object_name_end_offset = last->end_offset;
	statement->object_name_start_line = first->start_line;
	statement->object_name_start_column = first->start_column;
	statement->object_name_end_line = last->end_line;
	statement->object_name_end_column = last->end_column;
	return 1;
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
	case PREPARE_T: return MYLITE_STATEMENT_OBJECT_PREPARED_STATEMENT;
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
		if (token_text_equals(parser, token_index, "LOGFILE") &&
		    token_index + 1 <= last_token_index &&
		    parser->tokens[token_index + 1].parser_token == GROUP_T) {
			return MYLITE_STATEMENT_OBJECT_LOGFILE_GROUP;
		}
		if (token_text_equals(parser, token_index, "RESOURCE") &&
		    token_index + 1 <= last_token_index &&
		    parser->tokens[token_index + 1].parser_token == GROUP_T) {
			return MYLITE_STATEMENT_OBJECT_RESOURCE_GROUP;
		}
		if (token_text_equals(parser, token_index, "SERVER")) {
			return MYLITE_STATEMENT_OBJECT_SERVER;
		}
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

	if (statement->object_kind == MYLITE_STATEMENT_OBJECT_USER ||
	    statement->object_kind == MYLITE_STATEMENT_OBJECT_ROLE) {
		set_statement_account_name_from_first_token(parser, statement, first_name_token, last_token_index);
		return;
	}

	set_statement_object_name_from_first_token(parser, statement, first_name_token, last_token_index);
}

static void set_statement_object_name_from_first_token(const mylite_parser *parser,
                                                       mylite_statement *statement,
                                                       size_t first_name_token,
                                                       size_t last_token_index)
{
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
	if (token_text_equals(parser, object_token_index, "LOGFILE") &&
	    token_index <= last_token_index &&
	    parser->tokens[token_index].parser_token == GROUP_T) {
		token_index++;
	}
	if (token_text_equals(parser, object_token_index, "RESOURCE") &&
	    token_index <= last_token_index &&
	    parser->tokens[token_index].parser_token == GROUP_T) {
		token_index++;
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
	       token->kind == MYLITE_TOKEN_NUMBER ||
	       token_can_be_unquoted_object_name_keyword(token->parser_token);
}

static int token_can_continue_object_name(const mylite_token *token)
{
	return token->kind == MYLITE_TOKEN_IDENTIFIER ||
	       token->kind == MYLITE_TOKEN_QUOTED_IDENTIFIER ||
	       token_can_be_unquoted_object_name_keyword(token->parser_token);
}

static int token_can_be_unquoted_object_name_keyword(int token)
{
	switch (token) {
	case CHAIN_T:
	case CLOSE_T:
	case CURSOR_T:
	case COLUMNS_T:
	case DATA_T:
	case DECLARE_T:
	case FETCH_T:
	case FIELDS_T:
	case FORMAT_T:
	case FULL_T:
	case INFILE_T:
	case ITERATE_T:
	case JSON_T:
	case LEAVE_T:
	case LOCAL_T:
	case NO_T:
	case OPEN_T:
	case READ_T:
	case RETURN_T:
	case STOP_T:
	case TO_T:
	case TRANSACTION_T:
	case WRITE_T:
		return 1;
	default:
		return 0;
	}
}

static int token_can_start_label_name(const mylite_token *token)
{
	return token->kind == MYLITE_TOKEN_IDENTIFIER ||
	       token->kind == MYLITE_TOKEN_QUOTED_IDENTIFIER ||
	       token_can_be_unquoted_object_name_keyword(token->parser_token);
}

static int is_optional_name_modifier(int token)
{
	return token == IF_T ||
	       token == NOT_T ||
	       token == EXISTS_T ||
	       token == TEMPORARY_T;
}

static int token_text_equals(const mylite_parser *parser, size_t token_index, const char *expected)
{
	const mylite_token *token;
	size_t expected_length = strlen(expected);

	if (token_index >= parser->token_count) {
		return 0;
	}

	token = &parser->tokens[token_index];
	return token->end_offset - token->start_offset == expected_length &&
	       strncasecmp(expected, parser->lexer.input + token->start_offset, expected_length) == 0;
}

static int statement_kind_uses_object_scan(mylite_statement_kind kind)
{
	switch (kind) {
	case MYLITE_STATEMENT_CREATE:
	case MYLITE_STATEMENT_ALTER:
	case MYLITE_STATEMENT_DROP:
	case MYLITE_STATEMENT_TRUNCATE:
	case MYLITE_STATEMENT_RENAME:
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
