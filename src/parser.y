%{
#include <stdio.h>

#include "lexer.h"
#include "parser_internal.h"

#define YYSTYPE int

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
	  statement_start statement_head top_body { mylite_parser_add_statement(parser, parser->active_statement_kind); }
	;

statement_start:
	  /* empty */ { parser->active_statement_first_token = parser->lexer.token_count + 1; }
	;

statement_head:
	  SELECT_T      { parser->active_statement_kind = MYLITE_STATEMENT_SELECT; }
	| WITH_T        { parser->active_statement_kind = MYLITE_STATEMENT_SELECT; }
	| INSERT_T      { parser->active_statement_kind = MYLITE_STATEMENT_INSERT; }
	| REPLACE_T     { parser->active_statement_kind = MYLITE_STATEMENT_REPLACE; }
	| UPDATE_T      { parser->active_statement_kind = MYLITE_STATEMENT_UPDATE; }
	| DELETE_T      { parser->active_statement_kind = MYLITE_STATEMENT_DELETE; }
	| CREATE_T      { parser->active_statement_kind = MYLITE_STATEMENT_CREATE; }
	| ALTER_T       { parser->active_statement_kind = MYLITE_STATEMENT_ALTER; }
	| DROP_T        { parser->active_statement_kind = MYLITE_STATEMENT_DROP; }
	| TRUNCATE_T    { parser->active_statement_kind = MYLITE_STATEMENT_TRUNCATE; }
	| RENAME_T      { parser->active_statement_kind = MYLITE_STATEMENT_RENAME; }
	| CALL_T        { parser->active_statement_kind = MYLITE_STATEMENT_CALL; }
	| DO_T          { parser->active_statement_kind = MYLITE_STATEMENT_DO; }
	| HANDLER_T     { parser->active_statement_kind = MYLITE_STATEMENT_HANDLER; }
	| IMPORT_T      { parser->active_statement_kind = MYLITE_STATEMENT_IMPORT; }
	| LOAD_T        { parser->active_statement_kind = MYLITE_STATEMENT_LOAD; }
	| TABLE_T       { parser->active_statement_kind = MYLITE_STATEMENT_TABLE; }
	| VALUES_T      { parser->active_statement_kind = MYLITE_STATEMENT_VALUES; }
	| SET_T         { parser->active_statement_kind = MYLITE_STATEMENT_SET; }
	| SHOW_T        { parser->active_statement_kind = MYLITE_STATEMENT_SHOW; }
	| USE_T         { parser->active_statement_kind = MYLITE_STATEMENT_USE; }
	| DESCRIBE_T    { parser->active_statement_kind = MYLITE_STATEMENT_DESCRIBE; }
	| DESC_T        { parser->active_statement_kind = MYLITE_STATEMENT_DESCRIBE; }
	| EXPLAIN_T     { parser->active_statement_kind = MYLITE_STATEMENT_EXPLAIN; }
	| HELP_T        { parser->active_statement_kind = MYLITE_STATEMENT_HELP; }
	| START_T       { parser->active_statement_kind = MYLITE_STATEMENT_START; }
	| BEGIN_T       { parser->active_statement_kind = MYLITE_STATEMENT_BEGIN; }
	| COMMIT_T      { parser->active_statement_kind = MYLITE_STATEMENT_COMMIT; }
	| ROLLBACK_T    { parser->active_statement_kind = MYLITE_STATEMENT_ROLLBACK; }
	| SAVEPOINT_T   { parser->active_statement_kind = MYLITE_STATEMENT_SAVEPOINT; }
	| RELEASE_T     { parser->active_statement_kind = MYLITE_STATEMENT_RELEASE; }
	| LOCK_T        { parser->active_statement_kind = MYLITE_STATEMENT_LOCK; }
	| UNLOCK_T      { parser->active_statement_kind = MYLITE_STATEMENT_UNLOCK; }
	| XA_T          { parser->active_statement_kind = MYLITE_STATEMENT_XA; }
	| PREPARE_T     { parser->active_statement_kind = MYLITE_STATEMENT_PREPARE; }
	| EXECUTE_T     { parser->active_statement_kind = MYLITE_STATEMENT_EXECUTE; }
	| DEALLOCATE_T  { parser->active_statement_kind = MYLITE_STATEMENT_DEALLOCATE; }
	| ANALYZE_T     { parser->active_statement_kind = MYLITE_STATEMENT_ANALYZE; }
	| CHECK_T       { parser->active_statement_kind = MYLITE_STATEMENT_CHECK; }
	| CHECKSUM_T    { parser->active_statement_kind = MYLITE_STATEMENT_CHECKSUM; }
	| OPTIMIZE_T    { parser->active_statement_kind = MYLITE_STATEMENT_OPTIMIZE; }
	| REPAIR_T      { parser->active_statement_kind = MYLITE_STATEMENT_REPAIR; }
	| FLUSH_T       { parser->active_statement_kind = MYLITE_STATEMENT_FLUSH; }
	| KILL_T        { parser->active_statement_kind = MYLITE_STATEMENT_KILL; }
	| RESET_T       { parser->active_statement_kind = MYLITE_STATEMENT_RESET; }
	| RESTART_T     { parser->active_statement_kind = MYLITE_STATEMENT_RESTART; }
	| SHUTDOWN_T    { parser->active_statement_kind = MYLITE_STATEMENT_SHUTDOWN; }
	| GRANT_T       { parser->active_statement_kind = MYLITE_STATEMENT_GRANT; }
	| REVOKE_T      { parser->active_statement_kind = MYLITE_STATEMENT_REVOKE; }
	| INSTALL_T     { parser->active_statement_kind = MYLITE_STATEMENT_INSTALL; }
	| UNINSTALL_T   { parser->active_statement_kind = MYLITE_STATEMENT_UNINSTALL; }
	| CACHE_T       { parser->active_statement_kind = MYLITE_STATEMENT_CACHE; }
	| CHANGE_T      { parser->active_statement_kind = MYLITE_STATEMENT_CHANGE; }
	| BINLOG_T      { parser->active_statement_kind = MYLITE_STATEMENT_BINLOG; }
	| PURGE_T       { parser->active_statement_kind = MYLITE_STATEMENT_PURGE; }
	| SIGNAL_T      { parser->active_statement_kind = MYLITE_STATEMENT_SIGNAL; }
	| RESIGNAL_T    { parser->active_statement_kind = MYLITE_STATEMENT_RESIGNAL; }
	| GET_T         { parser->active_statement_kind = MYLITE_STATEMENT_GET; }
	| IF_T          { parser->active_statement_kind = MYLITE_STATEMENT_IF; }
	| unknown_head  { parser->active_statement_kind = MYLITE_STATEMENT_UNKNOWN; }
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
	| group
	;

top_body:
	  /* empty */
	| top_body top_item
	;

top_item:
	  body_item
	| END_T
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
	  BEGIN_T body END_T
	;

case_block:
	  CASE_T body END_T
	| CASE_T body END_CASE_T
	;

group:
	  '(' group_body ')'
	| '[' group_body ']'
	| '{' group_body '}'
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
	;

%%

static int yylex(YYSTYPE *yylval, mylite_parser *parser)
{
	(void)yylval;
	return mylite_lexer_next(parser);
}

static void yyerror(mylite_parser *parser, const char *message)
{
	mylite_parser_set_error(parser, message);
}
