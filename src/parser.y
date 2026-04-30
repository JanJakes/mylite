%{
#include <stdio.h>

#include "lexer.h"
#include "parser_internal.h"

#define YYSTYPE int
#define BEGIN_STATEMENT(kind, requires_body) mylite_parser_begin_statement(parser, (kind), (requires_body))

static int yylex(YYSTYPE *yylval, mylite_parser *parser);
static void yyerror(mylite_parser *parser, const char *message);
%}

%pure-parser
%parse-param {mylite_parser *parser}
%lex-param {mylite_parser *parser}

%token IDENT QUOTED_IDENT STRING NUMBER PARAM USER_VARIABLE SYSTEM_VARIABLE OPERATOR
%token SELECT_T INSERT_T REPLACE_T UPDATE_T DELETE_T CREATE_T ALTER_T DROP_T
%token TRUNCATE_T RENAME_T CALL_T DO_T HANDLER_T IMPORT_T LOAD_T TABLE_T VALUES_T
%token SET_T SHOW_T USE_T DESCRIBE_T DESC_T EXPLAIN_T HELP_T START_T BEGIN_T
%token COMMIT_T ROLLBACK_T SAVEPOINT_T RELEASE_T LOCK_T UNLOCK_T XA_T PREPARE_T
%token EXECUTE_T DEALLOCATE_T ANALYZE_T CHECK_T CHECKSUM_T OPTIMIZE_T REPAIR_T
%token FLUSH_T KILL_T RESET_T RESTART_T SHUTDOWN_T GRANT_T REVOKE_T INSTALL_T
%token UNINSTALL_T CACHE_T CHANGE_T BINLOG_T PURGE_T SIGNAL_T RESIGNAL_T GET_T
%token WITH_T END_T CASE_T IF_T LOOP_T REPEAT_T WHILE_T END_IF_T END_LOOP_T
%token END_REPEAT_T END_WHILE_T END_CASE_T THEN_T ELSE_T ELSEIF_T UNTIL_T
%token ALL_T AND_T AS_T AUTO_INCREMENT_T BETWEEN_T BY_T CHARACTER_T CHARSET_T
%token COLLATE_T CONSTRAINT_T CROSS_T DATABASE_T DEFAULT_T DELAYED_T DISTINCT_T
%token ENGINE_T EVENT_T EXISTS_T FALSE_T FROM_T FUNCTION_T GROUP_T HAVING_T
%token HIGH_PRIORITY_T IGNORE_T IN_T INDEX_T INNER_T INTO_T IS_T JOIN_T KEY_T
%token LEFT_T LIKE_T LIMIT_T LOW_PRIORITY_T NATURAL_T NOT_T NULL_T OFFSET_T ON_T
%token OR_T ORDER_T OUTER_T PRIMARY_T PROCEDURE_T REFERENCES_T REGEXP_T RIGHT_T
%token RLIKE_T ROLE_T SCHEMA_T SPATIAL_T TEMPORARY_T TRIGGER_T TRUE_T UNION_T
%token UNIQUE_T USER_T USING_T VALUE_T VIEW_T WHEN_T WHERE_T XOR_T

%start input

%%

input:
	  separators_opt
	| separators_opt statement_list separators_opt
	;

statement_list:
	  statement
	| statement_list separators statement
	;

separators_opt:
	  /* empty */
	| separators
	;

separators:
	  ';'
	| separators ';'
	;

statement:
	  statement_head top_body
	  {
		  if (!mylite_parser_add_statement(parser, parser->active_statement_kind)) {
			  YYERROR;
		  }
	  }
	;

statement_head:
	  SELECT_T      { BEGIN_STATEMENT(MYLITE_STATEMENT_SELECT, 1); }
	| WITH_T        { BEGIN_STATEMENT(MYLITE_STATEMENT_SELECT, 1); }
	| INSERT_T      { BEGIN_STATEMENT(MYLITE_STATEMENT_INSERT, 1); }
	| REPLACE_T     { BEGIN_STATEMENT(MYLITE_STATEMENT_REPLACE, 1); }
	| UPDATE_T      { BEGIN_STATEMENT(MYLITE_STATEMENT_UPDATE, 1); }
	| DELETE_T      { BEGIN_STATEMENT(MYLITE_STATEMENT_DELETE, 1); }
	| CREATE_T      { BEGIN_STATEMENT(MYLITE_STATEMENT_CREATE, 1); }
	| ALTER_T       { BEGIN_STATEMENT(MYLITE_STATEMENT_ALTER, 1); }
	| DROP_T        { BEGIN_STATEMENT(MYLITE_STATEMENT_DROP, 1); }
	| TRUNCATE_T    { BEGIN_STATEMENT(MYLITE_STATEMENT_TRUNCATE, 1); }
	| RENAME_T      { BEGIN_STATEMENT(MYLITE_STATEMENT_RENAME, 1); }
	| CALL_T        { BEGIN_STATEMENT(MYLITE_STATEMENT_CALL, 1); }
	| DO_T          { BEGIN_STATEMENT(MYLITE_STATEMENT_DO, 1); }
	| HANDLER_T     { BEGIN_STATEMENT(MYLITE_STATEMENT_HANDLER, 1); }
	| IMPORT_T      { BEGIN_STATEMENT(MYLITE_STATEMENT_IMPORT, 1); }
	| LOAD_T        { BEGIN_STATEMENT(MYLITE_STATEMENT_LOAD, 1); }
	| TABLE_T       { BEGIN_STATEMENT(MYLITE_STATEMENT_TABLE, 1); }
	| VALUES_T      { BEGIN_STATEMENT(MYLITE_STATEMENT_VALUES, 1); }
	| SET_T         { BEGIN_STATEMENT(MYLITE_STATEMENT_SET, 1); }
	| SHOW_T        { BEGIN_STATEMENT(MYLITE_STATEMENT_SHOW, 1); }
	| USE_T         { BEGIN_STATEMENT(MYLITE_STATEMENT_USE, 1); }
	| DESCRIBE_T    { BEGIN_STATEMENT(MYLITE_STATEMENT_DESCRIBE, 1); }
	| DESC_T        { BEGIN_STATEMENT(MYLITE_STATEMENT_DESCRIBE, 1); }
	| EXPLAIN_T     { BEGIN_STATEMENT(MYLITE_STATEMENT_EXPLAIN, 1); }
	| HELP_T        { BEGIN_STATEMENT(MYLITE_STATEMENT_HELP, 1); }
	| START_T       { BEGIN_STATEMENT(MYLITE_STATEMENT_START, 1); }
	| BEGIN_T       { BEGIN_STATEMENT(MYLITE_STATEMENT_BEGIN, 0); }
	| COMMIT_T      { BEGIN_STATEMENT(MYLITE_STATEMENT_COMMIT, 0); }
	| ROLLBACK_T    { BEGIN_STATEMENT(MYLITE_STATEMENT_ROLLBACK, 0); }
	| SAVEPOINT_T   { BEGIN_STATEMENT(MYLITE_STATEMENT_SAVEPOINT, 1); }
	| RELEASE_T     { BEGIN_STATEMENT(MYLITE_STATEMENT_RELEASE, 1); }
	| LOCK_T        { BEGIN_STATEMENT(MYLITE_STATEMENT_LOCK, 1); }
	| UNLOCK_T      { BEGIN_STATEMENT(MYLITE_STATEMENT_UNLOCK, 1); }
	| XA_T          { BEGIN_STATEMENT(MYLITE_STATEMENT_XA, 1); }
	| PREPARE_T     { BEGIN_STATEMENT(MYLITE_STATEMENT_PREPARE, 1); }
	| EXECUTE_T     { BEGIN_STATEMENT(MYLITE_STATEMENT_EXECUTE, 1); }
	| DEALLOCATE_T  { BEGIN_STATEMENT(MYLITE_STATEMENT_DEALLOCATE, 1); }
	| ANALYZE_T     { BEGIN_STATEMENT(MYLITE_STATEMENT_ANALYZE, 1); }
	| CHECK_T       { BEGIN_STATEMENT(MYLITE_STATEMENT_CHECK, 1); }
	| CHECKSUM_T    { BEGIN_STATEMENT(MYLITE_STATEMENT_CHECKSUM, 1); }
	| OPTIMIZE_T    { BEGIN_STATEMENT(MYLITE_STATEMENT_OPTIMIZE, 1); }
	| REPAIR_T      { BEGIN_STATEMENT(MYLITE_STATEMENT_REPAIR, 1); }
	| FLUSH_T       { BEGIN_STATEMENT(MYLITE_STATEMENT_FLUSH, 1); }
	| KILL_T        { BEGIN_STATEMENT(MYLITE_STATEMENT_KILL, 1); }
	| RESET_T       { BEGIN_STATEMENT(MYLITE_STATEMENT_RESET, 1); }
	| RESTART_T     { BEGIN_STATEMENT(MYLITE_STATEMENT_RESTART, 0); }
	| SHUTDOWN_T    { BEGIN_STATEMENT(MYLITE_STATEMENT_SHUTDOWN, 0); }
	| GRANT_T       { BEGIN_STATEMENT(MYLITE_STATEMENT_GRANT, 1); }
	| REVOKE_T      { BEGIN_STATEMENT(MYLITE_STATEMENT_REVOKE, 1); }
	| INSTALL_T     { BEGIN_STATEMENT(MYLITE_STATEMENT_INSTALL, 1); }
	| UNINSTALL_T   { BEGIN_STATEMENT(MYLITE_STATEMENT_UNINSTALL, 1); }
	| CACHE_T       { BEGIN_STATEMENT(MYLITE_STATEMENT_CACHE, 1); }
	| CHANGE_T      { BEGIN_STATEMENT(MYLITE_STATEMENT_CHANGE, 1); }
	| BINLOG_T      { BEGIN_STATEMENT(MYLITE_STATEMENT_BINLOG, 1); }
	| PURGE_T       { BEGIN_STATEMENT(MYLITE_STATEMENT_PURGE, 1); }
	| SIGNAL_T      { BEGIN_STATEMENT(MYLITE_STATEMENT_SIGNAL, 1); }
	| RESIGNAL_T    { BEGIN_STATEMENT(MYLITE_STATEMENT_RESIGNAL, 1); }
	| GET_T         { BEGIN_STATEMENT(MYLITE_STATEMENT_GET, 1); }
	| IF_T          { BEGIN_STATEMENT(MYLITE_STATEMENT_IF, 1); }
	| unknown_head  { BEGIN_STATEMENT(MYLITE_STATEMENT_UNKNOWN, 0); }
	;

unknown_head:
	  IDENT
	| QUOTED_IDENT
	| STRING
	| NUMBER
	| PARAM
	| USER_VARIABLE
	| SYSTEM_VARIABLE
	| OPERATOR
	| ','
	| '.'
	| '*'
	| '+'
	| '-'
	| '/'
	| '%'
	| END_T
	| CASE_T
	| LOOP_T
	| REPEAT_T
	| WHILE_T
	| END_IF_T
	| END_LOOP_T
	| END_REPEAT_T
	| END_WHILE_T
	| THEN_T
	| ELSE_T
	| ELSEIF_T
	| UNTIL_T
	| secondary_keyword
	| group
	;

top_body:
	  /* empty */
	| top_body top_item
	;

top_item:
	  body_item { parser->active_statement_body_items++; }
	| END_T     { parser->active_statement_body_items++; }
	;

body:
	  /* empty */
	| body body_item
	| body ';'
	;

body_item:
	  atom
	| group
	| begin_block
	| case_block
	;

begin_block:
	  BEGIN_T body END_T { mylite_parser_match_tokens(parser, $1, $3); }
	;

case_block:
	  CASE_T body END_T      { mylite_parser_match_tokens(parser, $1, $3); }
	| CASE_T body END_CASE_T { mylite_parser_match_tokens(parser, $1, $3); }
	;

group:
	  '(' group_body ')' { mylite_parser_match_tokens(parser, $1, $3); }
	| '[' group_body ']' { mylite_parser_match_tokens(parser, $1, $3); }
	| '{' group_body '}' { mylite_parser_match_tokens(parser, $1, $3); }
	;

group_body:
	  /* empty */
	| group_body group_item
	;

group_item:
	  body_item
	| END_T
	| ';'
	;

atom:
	  IDENT
	| QUOTED_IDENT
	| STRING
	| NUMBER
	| PARAM
	| USER_VARIABLE
	| SYSTEM_VARIABLE
	| OPERATOR
	| ','
	| '.'
	| '*'
	| '+'
	| '-'
	| '/'
	| '%'
	| SELECT_T
	| WITH_T
	| INSERT_T
	| REPLACE_T
	| UPDATE_T
	| DELETE_T
	| CREATE_T
	| ALTER_T
	| DROP_T
	| TRUNCATE_T
	| RENAME_T
	| CALL_T
	| DO_T
	| HANDLER_T
	| IMPORT_T
	| LOAD_T
	| TABLE_T
	| VALUES_T
	| SET_T
	| SHOW_T
	| USE_T
	| DESCRIBE_T
	| DESC_T
	| EXPLAIN_T
	| HELP_T
	| START_T
	| COMMIT_T
	| ROLLBACK_T
	| SAVEPOINT_T
	| RELEASE_T
	| LOCK_T
	| UNLOCK_T
	| XA_T
	| PREPARE_T
	| EXECUTE_T
	| DEALLOCATE_T
	| ANALYZE_T
	| CHECK_T
	| CHECKSUM_T
	| OPTIMIZE_T
	| REPAIR_T
	| FLUSH_T
	| KILL_T
	| RESET_T
	| RESTART_T
	| SHUTDOWN_T
	| GRANT_T
	| REVOKE_T
	| INSTALL_T
	| UNINSTALL_T
	| CACHE_T
	| CHANGE_T
	| BINLOG_T
	| PURGE_T
	| SIGNAL_T
	| RESIGNAL_T
	| GET_T
	| IF_T
	| LOOP_T
	| REPEAT_T
	| WHILE_T
	| END_IF_T
	| END_LOOP_T
	| END_REPEAT_T
	| END_WHILE_T
	| THEN_T
	| ELSE_T
	| ELSEIF_T
	| UNTIL_T
	| secondary_keyword
	;

secondary_keyword:
	  ALL_T
	| AND_T
	| AS_T
	| AUTO_INCREMENT_T
	| BETWEEN_T
	| BY_T
	| CHARACTER_T
	| CHARSET_T
	| COLLATE_T
	| CONSTRAINT_T
	| CROSS_T
	| DATABASE_T
	| DEFAULT_T
	| DELAYED_T
	| DISTINCT_T
	| ENGINE_T
	| EVENT_T
	| EXISTS_T
	| FALSE_T
	| FROM_T
	| FUNCTION_T
	| GROUP_T
	| HAVING_T
	| HIGH_PRIORITY_T
	| IGNORE_T
	| IN_T
	| INDEX_T
	| INNER_T
	| INTO_T
	| IS_T
	| JOIN_T
	| KEY_T
	| LEFT_T
	| LIKE_T
	| LIMIT_T
	| LOW_PRIORITY_T
	| NATURAL_T
	| NOT_T
	| NULL_T
	| OFFSET_T
	| ON_T
	| OR_T
	| ORDER_T
	| OUTER_T
	| PRIMARY_T
	| PROCEDURE_T
	| REFERENCES_T
	| REGEXP_T
	| RIGHT_T
	| RLIKE_T
	| ROLE_T
	| SCHEMA_T
	| SPATIAL_T
	| TEMPORARY_T
	| TRIGGER_T
	| TRUE_T
	| UNION_T
	| UNIQUE_T
	| USER_T
	| USING_T
	| VALUE_T
	| VIEW_T
	| WHEN_T
	| WHERE_T
	| XOR_T
	;

%%

static int yylex(YYSTYPE *yylval, mylite_parser *parser)
{
	int token = mylite_lexer_next(parser);
	if (token > 0) {
		*yylval = (YYSTYPE)parser->lexer.token_count;
	}
	return token;
}

static void yyerror(mylite_parser *parser, const char *message)
{
	mylite_parser_set_error(parser, message);
}
