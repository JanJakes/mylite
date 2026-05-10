%name mylite_sql_lemon
%token_prefix MYLITE_SQL_PARSE_
%token_type { struct mylite_sql_token }
%default_type { struct mylite_sql_ast_node * }
%extra_argument { struct mylite_sql_parser_state *state }

%include {
#define YYNOERRORRECOVERY 1
#include "mylite_parser_internal.h"
}

%syntax_error {
    mylite_sql_parser_state_syntax_error(state, yymajor, TOKEN);
}

%parse_failure {
    mylite_sql_parser_state_parse_failed(state);
}

%parse_accept {
    mylite_sql_parser_state_accept(state);
}

%stack_overflow {
    mylite_sql_parser_state_stack_overflow(state);
}

%left PLUS MINUS.
%left STAR SLASH.
%right UPLUS UMINUS.

%type integer_type_name { struct mylite_sql_integer_type_name_tokens }
%type integer_display_width_opt { struct mylite_sql_integer_display_width_tokens }
%type integer_signedness_opt { struct mylite_sql_integer_signedness_tokens }

input ::= statement_list(A). {
    mylite_sql_parser_state_set_root(state, A);
}

statement_list(A) ::= . {
    A = mylite_sql_parser_make_script(state);
}
statement_list(A) ::= statements(B). {
    A = B;
}

statements(A) ::= statement(B). {
    A = mylite_sql_parser_make_script_with_statement(state, B);
}
statements(A) ::= statements(B) SEMICOLON. {
    A = B;
}
statements(A) ::= statements(B) SEMICOLON statement(C). {
    A = mylite_sql_parser_append_statement(state, B, C);
}

statement(A) ::= select_statement(B). {
    A = B;
}
statement(A) ::= use_statement(B). {
    A = B;
}
statement(A) ::= create_table_statement(B). {
    A = B;
}
statement(A) ::= create_schema_statement(B). {
    A = B;
}
statement(A) ::= drop_table_statement(B). {
    A = B;
}
statement(A) ::= drop_schema_statement(B). {
    A = B;
}
statement(A) ::= truncate_table_statement(B). {
    A = B;
}
statement(A) ::= show_tables_statement(B). {
    A = B;
}
statement(A) ::= show_table_status_statement(B). {
    A = B;
}
statement(A) ::= show_character_set_statement(B). {
    A = B;
}
statement(A) ::= show_collation_statement(B). {
    A = B;
}
statement(A) ::= show_triggers_statement(B). {
    A = B;
}
statement(A) ::= show_events_statement(B). {
    A = B;
}
statement(A) ::= show_open_tables_statement(B). {
    A = B;
}
statement(A) ::= show_routine_status_statement(B). {
    A = B;
}
statement(A) ::= show_processlist_statement(B). {
    A = B;
}
statement(A) ::= show_warnings_statement(B). {
    A = B;
}
statement(A) ::= show_errors_statement(B). {
    A = B;
}
statement(A) ::= show_columns_statement(B). {
    A = B;
}
statement(A) ::= show_index_statement(B). {
    A = B;
}
statement(A) ::= show_create_table_statement(B). {
    A = B;
}
statement(A) ::= show_create_database_statement(B). {
    A = B;
}
statement(A) ::= show_engines_statement(B). {
    A = B;
}
statement(A) ::= show_databases_statement(B). {
    A = B;
}
statement(A) ::= describe_table_statement(B). {
    A = B;
}
statement(A) ::= explain_table_statement(B). {
    A = B;
}
statement(A) ::= rename_table_statement(B). {
    A = B;
}
statement(A) ::= alter_table_rename_statement(B). {
    A = B;
}
statement(A) ::= alter_table_add_column_statement(B). {
    A = B;
}
statement(A) ::= alter_table_drop_column_statement(B). {
    A = B;
}
statement(A) ::= alter_table_rename_column_statement(B). {
    A = B;
}
statement(A) ::= alter_table_modify_column_statement(B). {
    A = B;
}
statement(A) ::= alter_table_change_column_statement(B). {
    A = B;
}
statement(A) ::= alter_table_set_default_statement(B). {
    A = B;
}
statement(A) ::= alter_table_drop_default_statement(B). {
    A = B;
}
statement(A) ::= alter_table_column_visibility_statement(B). {
    A = B;
}
statement(A) ::= insert_values_statement(B). {
    A = B;
}
statement(A) ::= insert_set_statement(B). {
    A = B;
}
statement(A) ::= delete_statement(B). {
    A = B;
}
statement(A) ::= update_statement(B). {
    A = B;
}

use_statement(A) ::= USE(T) identifier(B). {
    A = mylite_sql_parser_make_use_statement(state, T, B);
}

create_table_statement(A) ::=
    CREATE(C) TABLE create_if_not_exists_opt(E) table_name(T) LPAREN
    column_definition_list(L) RPAREN(R) table_option_list_opt(O). {
    A = mylite_sql_parser_make_create_table_statement(state, C, E, T, L, R, O);
}

create_if_not_exists_opt(A) ::= . {
    A = NULL;
}
create_if_not_exists_opt(A) ::= IF(I) NOT EXISTS(E). {
    A = mylite_sql_parser_make_create_if_not_exists_clause(state, I, E);
}

table_option_list_opt(A) ::= . {
    A = NULL;
}
table_option_list_opt(A) ::= table_option_list(B). {
    A = B;
}

table_option_list(A) ::= table_option(B). {
    A = mylite_sql_parser_make_table_option_list(state, B);
}
table_option_list(A) ::= table_option_list(B) table_option(C). {
    A = mylite_sql_parser_append_table_option(state, B, C);
}

table_option(A) ::= ENGINE(E) equal_opt option_name(N). {
    A = mylite_sql_parser_make_table_engine_option(state, E, N);
}
table_option(A) ::= default_opt CHARSET(C) equal_opt option_name(N). {
    A = mylite_sql_parser_make_table_charset_option(state, C, N);
}
table_option(A) ::= default_opt CHARACTER(C) SET equal_opt option_name(N). {
    A = mylite_sql_parser_make_table_charset_option(state, C, N);
}
table_option(A) ::= default_opt COLLATE(C) equal_opt option_name(N). {
    A = mylite_sql_parser_make_table_collation_option(state, C, N);
}

default_opt ::= .
default_opt ::= DEFAULT.

equal_opt ::= .
equal_opt ::= EQUAL.

option_name(A) ::= identifier(B). {
    A = B;
}
option_name(A) ::= STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}

create_schema_statement(A) ::= CREATE(C) DATABASE create_schema_if_not_exists_opt(E) identifier(S). {
    A = mylite_sql_parser_make_create_schema_statement(state, C, E, S);
}
create_schema_statement(A) ::= CREATE(C) SCHEMA create_schema_if_not_exists_opt(E) identifier(S). {
    A = mylite_sql_parser_make_create_schema_statement(state, C, E, S);
}

create_schema_if_not_exists_opt(A) ::= . {
    A = NULL;
}
create_schema_if_not_exists_opt(A) ::= IF(I) NOT EXISTS(E). {
    A = mylite_sql_parser_make_create_schema_if_not_exists_clause(state, I, E);
}

drop_table_statement(A) ::= DROP(D) TABLE drop_if_exists_opt(E) table_name_list(T). {
    A = mylite_sql_parser_make_drop_table_statement(state, D, E, T);
}

drop_if_exists_opt(A) ::= . {
    A = NULL;
}
drop_if_exists_opt(A) ::= IF(I) EXISTS(E). {
    A = mylite_sql_parser_make_drop_if_exists_clause(state, I, E);
}

table_name_list(A) ::= table_name(T). {
    A = mylite_sql_parser_make_table_name_list(state, T);
}
table_name_list(A) ::= table_name_list(L) COMMA table_name(T). {
    A = mylite_sql_parser_append_table_name(state, L, T);
}

drop_schema_statement(A) ::= DROP(D) DATABASE drop_schema_if_exists_opt(E) identifier(S). {
    A = mylite_sql_parser_make_drop_schema_statement(state, D, E, S);
}
drop_schema_statement(A) ::= DROP(D) SCHEMA drop_schema_if_exists_opt(E) identifier(S). {
    A = mylite_sql_parser_make_drop_schema_statement(state, D, E, S);
}

drop_schema_if_exists_opt(A) ::= . {
    A = NULL;
}
drop_schema_if_exists_opt(A) ::= IF(I) EXISTS(E). {
    A = mylite_sql_parser_make_drop_schema_if_exists_clause(state, I, E);
}

truncate_table_statement(A) ::= TRUNCATE(T) table_name(N). {
    A = mylite_sql_parser_make_truncate_table_statement(state, T, N);
}
truncate_table_statement(A) ::= TRUNCATE(T) TABLE table_name(N). {
    A = mylite_sql_parser_make_truncate_table_statement(state, T, N);
}

show_tables_statement(A) ::= SHOW(S) TABLES(T) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_tables_statement(state, S, T, NULL, L);
}
show_tables_statement(A) ::= SHOW(S) TABLES(T) FROM identifier(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_tables_statement(state, S, T, D, L);
}
show_tables_statement(A) ::= SHOW(S) TABLES(T) IN identifier(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_tables_statement(state, S, T, D, L);
}

show_table_status_statement(A) ::= SHOW(S) TABLE STATUS(T) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_table_status_statement(state, S, T, NULL, L);
}
show_table_status_statement(A) ::= SHOW(S) TABLE STATUS(T) FROM identifier(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_table_status_statement(state, S, T, D, L);
}
show_table_status_statement(A) ::= SHOW(S) TABLE STATUS(T) IN identifier(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_table_status_statement(state, S, T, D, L);
}

show_character_set_statement(A) ::= SHOW(S) CHARACTER SET(T) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_character_set_statement(state, S, T, L);
}
show_character_set_statement(A) ::= SHOW(S) CHARSET(T) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_character_set_statement(state, S, T, L);
}

show_collation_statement(A) ::= SHOW(S) COLLATION(C) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_collation_statement(state, S, C, L);
}

show_triggers_statement(A) ::= SHOW(S) TRIGGERS(T) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_triggers_statement(state, S, T, NULL, L);
}
show_triggers_statement(A) ::= SHOW(S) FULL TRIGGERS(T) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_triggers_statement(state, S, T, NULL, L);
}
show_triggers_statement(A) ::= SHOW(S) TRIGGERS(T) FROM identifier(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_triggers_statement(state, S, T, D, L);
}
show_triggers_statement(A) ::= SHOW(S) TRIGGERS(T) IN identifier(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_triggers_statement(state, S, T, D, L);
}
show_triggers_statement(A) ::= SHOW(S) FULL TRIGGERS(T) FROM identifier(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_triggers_statement(state, S, T, D, L);
}
show_triggers_statement(A) ::= SHOW(S) FULL TRIGGERS(T) IN identifier(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_triggers_statement(state, S, T, D, L);
}

show_events_statement(A) ::= SHOW(S) EVENTS(E) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_events_statement(state, S, E, NULL, L);
}
show_events_statement(A) ::= SHOW(S) EVENTS(E) FROM identifier(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_events_statement(state, S, E, D, L);
}
show_events_statement(A) ::= SHOW(S) EVENTS(E) IN identifier(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_events_statement(state, S, E, D, L);
}

show_open_tables_statement(A) ::= SHOW(S) OPEN TABLES(T) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_open_tables_statement(state, S, T, NULL, L);
}
show_open_tables_statement(A) ::= SHOW(S) OPEN TABLES(T) FROM identifier(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_open_tables_statement(state, S, T, D, L);
}
show_open_tables_statement(A) ::= SHOW(S) OPEN TABLES(T) IN identifier(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_open_tables_statement(state, S, T, D, L);
}

show_routine_status_statement(A) ::= SHOW(S) PROCEDURE STATUS(T) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_routine_status_statement(
        state, S, T, MYLITE_SQL_AST_SHOW_PROCEDURE_STATUS_STATEMENT, L);
}
show_routine_status_statement(A) ::= SHOW(S) FUNCTION STATUS(T) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_routine_status_statement(
        state, S, T, MYLITE_SQL_AST_SHOW_FUNCTION_STATUS_STATEMENT, L);
}

show_processlist_statement(A) ::= SHOW(S) PROCESSLIST(P). {
    A = mylite_sql_parser_make_show_processlist_statement(
        state, S, P, MYLITE_SQL_AST_SHOW_PROCESSLIST_STATEMENT);
}
show_processlist_statement(A) ::= SHOW(S) FULL PROCESSLIST(P). {
    A = mylite_sql_parser_make_show_processlist_statement(
        state, S, P, MYLITE_SQL_AST_SHOW_FULL_PROCESSLIST_STATEMENT);
}

show_warnings_statement(A) ::= SHOW(S) WARNINGS(W) limit_clause_opt(L). {
    A = mylite_sql_parser_make_show_warnings_statement(state, S, W, L);
}
show_warnings_statement(A) ::= SHOW(S) COUNT(C) LPAREN(L) STAR RPAREN WARNINGS(W). {
    A = mylite_sql_parser_make_show_count_warnings_statement(
        state,
        (struct mylite_sql_show_count_warnings_tokens){
            .show = S,
            .count = C,
            .left_paren = L,
            .warnings = W,
        });
}

show_errors_statement(A) ::= SHOW(S) ERRORS(E) limit_clause_opt(L). {
    A = mylite_sql_parser_make_show_errors_statement(state, S, E, L);
}
show_errors_statement(A) ::= SHOW(S) COUNT(C) LPAREN(L) STAR RPAREN ERRORS(E). {
    A = mylite_sql_parser_make_show_count_errors_statement(
        state,
        (struct mylite_sql_show_count_errors_tokens){
            .show = S,
            .count = C,
            .left_paren = L,
            .errors = E,
        });
}

show_columns_statement(A) ::= SHOW(S) COLUMNS FROM table_name(T) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_columns_statement(state, S, T, NULL, L);
}
show_columns_statement(A) ::= SHOW(S) COLUMNS IN table_name(T) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_columns_statement(state, S, T, NULL, L);
}
show_columns_statement(A) ::= SHOW(S) FIELDS FROM table_name(T) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_columns_statement(state, S, T, NULL, L);
}
show_columns_statement(A) ::= SHOW(S) FIELDS IN table_name(T) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_columns_statement(state, S, T, NULL, L);
}
show_columns_statement(A) ::= SHOW(S) COLUMNS FROM table_name(T) FROM identifier(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_columns_statement(state, S, T, D, L);
}
show_columns_statement(A) ::= SHOW(S) COLUMNS FROM table_name(T) IN identifier(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_columns_statement(state, S, T, D, L);
}
show_columns_statement(A) ::= SHOW(S) COLUMNS IN table_name(T) FROM identifier(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_columns_statement(state, S, T, D, L);
}
show_columns_statement(A) ::= SHOW(S) COLUMNS IN table_name(T) IN identifier(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_columns_statement(state, S, T, D, L);
}
show_columns_statement(A) ::= SHOW(S) FIELDS FROM table_name(T) FROM identifier(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_columns_statement(state, S, T, D, L);
}
show_columns_statement(A) ::= SHOW(S) FIELDS FROM table_name(T) IN identifier(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_columns_statement(state, S, T, D, L);
}
show_columns_statement(A) ::= SHOW(S) FIELDS IN table_name(T) FROM identifier(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_columns_statement(state, S, T, D, L);
}
show_columns_statement(A) ::= SHOW(S) FIELDS IN table_name(T) IN identifier(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_columns_statement(state, S, T, D, L);
}

show_index_statement(A) ::= SHOW(S) show_index_keyword FROM table_name(T). {
    A = mylite_sql_parser_make_show_index_statement(state, S, T, NULL);
}
show_index_statement(A) ::= SHOW(S) show_index_keyword IN table_name(T). {
    A = mylite_sql_parser_make_show_index_statement(state, S, T, NULL);
}
show_index_statement(A) ::= SHOW(S) show_index_keyword FROM table_name(T) FROM identifier(D). {
    A = mylite_sql_parser_make_show_index_statement(state, S, T, D);
}
show_index_statement(A) ::= SHOW(S) show_index_keyword FROM table_name(T) IN identifier(D). {
    A = mylite_sql_parser_make_show_index_statement(state, S, T, D);
}
show_index_statement(A) ::= SHOW(S) show_index_keyword IN table_name(T) FROM identifier(D). {
    A = mylite_sql_parser_make_show_index_statement(state, S, T, D);
}
show_index_statement(A) ::= SHOW(S) show_index_keyword IN table_name(T) IN identifier(D). {
    A = mylite_sql_parser_make_show_index_statement(state, S, T, D);
}

show_index_keyword ::= INDEX.
show_index_keyword ::= INDEXES.
show_index_keyword ::= KEYS.

show_databases_statement(A) ::= SHOW(S) DATABASES(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_databases_statement(state, S, D, L);
}
show_databases_statement(A) ::= SHOW(S) SCHEMAS(D) show_like_clause_opt(L). {
    A = mylite_sql_parser_make_show_databases_statement(state, S, D, L);
}

show_create_table_statement(A) ::= SHOW(S) CREATE TABLE table_name(T). {
    A = mylite_sql_parser_make_show_create_table_statement(state, S, T);
}

show_create_database_statement(A) ::= SHOW(S) CREATE DATABASE identifier(D). {
    A = mylite_sql_parser_make_show_create_database_statement(state, S, D);
}
show_create_database_statement(A) ::= SHOW(S) CREATE SCHEMA identifier(D). {
    A = mylite_sql_parser_make_show_create_database_statement(state, S, D);
}

show_engines_statement(A) ::= SHOW(S) ENGINES(E). {
    A = mylite_sql_parser_make_show_engines_statement(state, S, E);
}
show_engines_statement(A) ::= SHOW(S) STORAGE ENGINES(E). {
    A = mylite_sql_parser_make_show_engines_statement(state, S, E);
}

describe_table_statement(A) ::= DESCRIBE(D) table_name(T). {
    A = mylite_sql_parser_make_show_columns_statement(state, D, T, NULL, NULL);
}
describe_table_statement(A) ::= DESC(D) table_name(T). {
    A = mylite_sql_parser_make_show_columns_statement(state, D, T, NULL, NULL);
}

explain_table_statement(A) ::= EXPLAIN(E) table_name(T). {
    A = mylite_sql_parser_make_show_columns_statement(state, E, T, NULL, NULL);
}

show_like_clause_opt(A) ::= . {
    A = NULL;
}
show_like_clause_opt(A) ::= LIKE STRING(P). {
    A = mylite_sql_parser_make_literal(state, P, MYLITE_SQL_AST_LITERAL_STRING);
}

rename_table_statement(A) ::= RENAME(R) TABLE rename_table_pair_list(L). {
    A = mylite_sql_parser_make_rename_table_statement(state, R, L);
}

rename_table_pair_list(A) ::= rename_table_pair(P). {
    A = mylite_sql_parser_make_rename_table_pair_list(state, P);
}
rename_table_pair_list(A) ::= rename_table_pair_list(L) COMMA rename_table_pair(P). {
    A = mylite_sql_parser_append_rename_table_pair(state, L, P);
}

rename_table_pair(A) ::= table_name(S) TO(T) table_name(N). {
    A = mylite_sql_parser_make_rename_table_pair(state, S, T, N);
}

alter_table_rename_statement(A) ::=
    ALTER(A1) TABLE table_name(S) RENAME table_rename_connector_opt table_name(T). {
    A = mylite_sql_parser_make_alter_table_rename_statement(state, A1, S, T);
}

alter_table_add_column_statement(A) ::=
    ALTER(A1) TABLE table_name(T) ADD column_keyword_opt column_definition(C). {
    A = mylite_sql_parser_make_alter_table_add_column_statement(state, A1, T, C);
}

alter_table_drop_column_statement(A) ::=
    ALTER(A1) TABLE table_name(T) DROP column_keyword_opt identifier(C). {
    A = mylite_sql_parser_make_alter_table_drop_column_statement(state, A1, T, C);
}

alter_table_rename_column_statement(A) ::=
    ALTER(A1) TABLE table_name(T) RENAME COLUMN identifier(O) TO identifier(N). {
    A = mylite_sql_parser_make_alter_table_rename_column_statement(state, A1, T, O, N);
}

alter_table_modify_column_statement(A) ::=
    ALTER(A1) TABLE table_name(T) MODIFY column_keyword_opt column_definition(C). {
    A = mylite_sql_parser_make_alter_table_modify_column_statement(state, A1, T, C);
}

alter_table_change_column_statement(A) ::=
    ALTER(A1) TABLE table_name(T) CHANGE column_keyword_opt identifier(O) column_definition(C). {
    A = mylite_sql_parser_make_alter_table_change_column_statement(state, A1, T, O, C);
}

alter_table_set_default_statement(A) ::=
    ALTER(A1) TABLE table_name(T) ALTER column_keyword_opt identifier(C) SET DEFAULT(D) NULL(N). {
    A = mylite_sql_parser_make_alter_table_set_default_statement(
        state, A1, T, C, mylite_sql_parser_make_column_default_null(state, D, N));
}
alter_table_set_default_statement(A) ::=
    ALTER(A1) TABLE table_name(T) ALTER column_keyword_opt identifier(C) SET DEFAULT(D)
    column_default_value(V). {
    A = mylite_sql_parser_make_alter_table_set_default_statement(
        state, A1, T, C, mylite_sql_parser_make_column_default_value(state, D, V));
}

alter_table_drop_default_statement(A) ::=
    ALTER(A1) TABLE table_name(T) ALTER column_keyword_opt identifier(C) DROP DEFAULT(D). {
    A = mylite_sql_parser_make_alter_table_drop_default_statement(state, A1, T, C, D);
}

alter_table_column_visibility_statement(A) ::=
    ALTER(A1) TABLE table_name(T) ALTER column_keyword_opt identifier(C) SET VISIBLE(V). {
    A = mylite_sql_parser_make_alter_table_column_visibility_statement(
        state, A1, T, C, V, MYLITE_SQL_AST_COLUMN_VISIBILITY_VISIBLE);
}
alter_table_column_visibility_statement(A) ::=
    ALTER(A1) TABLE table_name(T) ALTER column_keyword_opt identifier(C) SET INVISIBLE(V). {
    A = mylite_sql_parser_make_alter_table_column_visibility_statement(
        state, A1, T, C, V, MYLITE_SQL_AST_COLUMN_VISIBILITY_INVISIBLE);
}

column_keyword_opt(A) ::= . {
    A = NULL;
}
column_keyword_opt(A) ::= COLUMN. {
    A = NULL;
}

table_rename_connector_opt(A) ::= . {
    A = NULL;
}
table_rename_connector_opt(A) ::= TO. {
    A = NULL;
}
table_rename_connector_opt(A) ::= AS. {
    A = NULL;
}

insert_values_statement(A) ::=
    INSERT(I) INTO table_name(T) insert_column_list_opt(C) VALUES insert_row_list(R). {
    A = mylite_sql_parser_make_insert_statement(state, I, T, C, R);
}

insert_set_statement(A) ::=
    INSERT(I) INTO table_name(T) SET insert_assignment_list(S). {
    A = mylite_sql_parser_make_insert_set_statement(state, I, T, S);
}
insert_set_statement(A) ::=
    INSERT(I) table_name(T) SET insert_assignment_list(S). {
    A = mylite_sql_parser_make_insert_set_statement(state, I, T, S);
}

delete_statement(A) ::=
    DELETE(D) FROM table_name(T) where_clause_opt(W) order_clause_opt(O) delete_limit_clause_opt(L). {
    A = mylite_sql_parser_make_delete_statement(state, D, T, W, O, L);
}

update_statement(A) ::=
    UPDATE(U) table_name(T) SET update_assignment_list(S) where_clause_opt(W)
    order_clause_opt(O) update_limit_clause_opt(L). {
    A = mylite_sql_parser_make_update_statement(state, U, T, S, W, O, L);
}

update_assignment_list(A) ::= update_assignment(B). {
    A = mylite_sql_parser_make_update_assignment_list(state, B);
}
update_assignment_list(A) ::= update_assignment_list(B) COMMA update_assignment(C). {
    A = mylite_sql_parser_append_update_assignment(state, B, C);
}

update_assignment(A) ::= qualified_identifier(T) EQUAL(E) update_value(V). {
    A = mylite_sql_parser_make_update_assignment(state, T, E, V);
}

insert_assignment_list(A) ::= insert_assignment(B). {
    A = mylite_sql_parser_make_insert_assignment_list(state, B);
}
insert_assignment_list(A) ::= insert_assignment_list(B) COMMA insert_assignment(C). {
    A = mylite_sql_parser_append_insert_assignment(state, B, C);
}

insert_assignment(A) ::= qualified_identifier(T) EQUAL(E) insert_value(V). {
    A = mylite_sql_parser_make_insert_assignment(state, T, E, V);
}

insert_column_list_opt(A) ::= . {
    A = mylite_sql_parser_make_identifier_list(state, NULL);
}
insert_column_list_opt(A) ::= LPAREN identifier_list(L) RPAREN. {
    A = L;
}

identifier_list(A) ::= identifier(B). {
    A = mylite_sql_parser_make_identifier_list(state, B);
}
identifier_list(A) ::= identifier_list(B) COMMA identifier(C). {
    A = mylite_sql_parser_append_identifier(state, B, C);
}

insert_row_list(A) ::= insert_row(B). {
    A = mylite_sql_parser_make_insert_row_list(state, B);
}
insert_row_list(A) ::= insert_row_list(B) COMMA insert_row(C). {
    A = mylite_sql_parser_append_insert_row(state, B, C);
}

insert_row(A) ::= LPAREN(L) insert_value_list(V) RPAREN(R). {
    A = mylite_sql_parser_make_insert_row(state, L, V, R);
}

insert_value_list(A) ::= insert_value(B). {
    A = mylite_sql_parser_make_insert_row_values(state, B);
}
insert_value_list(A) ::= insert_value_list(B) COMMA insert_value(C). {
    A = mylite_sql_parser_append_insert_value(state, B, C);
}

insert_value(A) ::= INTEGER(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER);
}
insert_value(A) ::= PLUS(P) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, P, MYLITE_SQL_AST_OPERATOR_POSITIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
insert_value(A) ::= MINUS(M) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, M, MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
insert_value(A) ::= NULL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_NULL);
}
insert_value(A) ::= TRUE(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_TRUE);
}
insert_value(A) ::= FALSE(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FALSE);
}

update_value(A) ::= INTEGER(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER);
}
update_value(A) ::= PLUS(P) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, P, MYLITE_SQL_AST_OPERATOR_POSITIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
update_value(A) ::= MINUS(M) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, M, MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
update_value(A) ::= NULL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_NULL);
}
update_value(A) ::= TRUE(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_TRUE);
}
update_value(A) ::= FALSE(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FALSE);
}

select_statement(A) ::= SELECT(T) select_item_list(B). {
    A = mylite_sql_parser_make_select_statement(state, T, B, NULL, NULL, NULL, NULL);
}
select_statement(A) ::= SELECT(T) select_item_list(B) FROM(F) DUAL(D). {
    A = mylite_sql_parser_make_select_statement(
        state, T, B, mylite_sql_parser_make_from_dual(state, F, D), NULL, NULL, NULL);
}
select_statement(A) ::=
    SELECT(T) select_item_list(B) FROM(F) table_name(N) where_clause_opt(W)
    order_clause_opt(O) limit_clause_opt(L). {
    A = mylite_sql_parser_make_select_statement(
        state, T, B, mylite_sql_parser_make_from_table(state, F, N), W, O, L);
}
select_statement(A) ::= SELECT(T) STAR(S). {
    A = mylite_sql_parser_make_select_statement(
        state, T, mylite_sql_parser_make_wildcard_select_list(state, S), NULL, NULL, NULL, NULL);
}
select_statement(A) ::= SELECT(T) STAR(S) FROM(F) DUAL(D). {
    A = mylite_sql_parser_make_select_statement(
        state, T, mylite_sql_parser_make_wildcard_select_list(state, S),
        mylite_sql_parser_make_from_dual(state, F, D), NULL, NULL, NULL);
}
select_statement(A) ::=
    SELECT(T) STAR(S) FROM(F) table_name(N) where_clause_opt(W)
    order_clause_opt(O) limit_clause_opt(L). {
    A = mylite_sql_parser_make_select_statement(
        state, T, mylite_sql_parser_make_wildcard_select_list(state, S),
        mylite_sql_parser_make_from_table(state, F, N), W, O, L);
}

where_clause_opt(A) ::= . {
    A = NULL;
}
where_clause_opt(A) ::= WHERE(W) predicate(P). {
    A = mylite_sql_parser_make_where_clause(state, W, P);
}

predicate(A) ::= predicate_atom(B). {
    A = B;
}
predicate(A) ::= LPAREN(L) predicate(B) RPAREN(R). {
    A = mylite_sql_parser_make_parenthesized_expression(state, L, B, R);
}

predicate_atom(A) ::= qualified_identifier(C) EQUAL(O) predicate_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_EQUAL, V);
}
predicate_atom(A) ::= qualified_identifier(C) NULL_SAFE_EQUAL(O) predicate_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL, V);
}
predicate_atom(A) ::= qualified_identifier(C) NOT_EQUAL(O) predicate_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_NOT_EQUAL, V);
}
predicate_atom(A) ::= qualified_identifier(C) LESS(O) predicate_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_LESS, V);
}
predicate_atom(A) ::= qualified_identifier(C) LESS_EQUAL(O) predicate_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_LESS_EQUAL, V);
}
predicate_atom(A) ::= qualified_identifier(C) GREATER(O) predicate_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_GREATER, V);
}
predicate_atom(A) ::= qualified_identifier(C) GREATER_EQUAL(O) predicate_integer_value(V). {
    A = mylite_sql_parser_make_comparison_predicate(
        state, C, O, MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL, V);
}
predicate_atom(A) ::= qualified_identifier(C) IS(I) NULL(N). {
    A = mylite_sql_parser_make_is_null_predicate(
        state, C, I, MYLITE_SQL_AST_OPERATOR_IS_NULL, N);
}
predicate_atom(A) ::= qualified_identifier(C) IS(I) NOT NULL(N). {
    A = mylite_sql_parser_make_is_null_predicate(
        state, C, I, MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL, N);
}

predicate_integer_value(A) ::= INTEGER(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER);
}
predicate_integer_value(A) ::= PLUS(P) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, P, MYLITE_SQL_AST_OPERATOR_POSITIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
predicate_integer_value(A) ::= MINUS(M) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, M, MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
predicate_integer_value(A) ::= TRUE(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_TRUE);
}
predicate_integer_value(A) ::= FALSE(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FALSE);
}

order_clause_opt(A) ::= . {
    A = NULL;
}
order_clause_opt(A) ::= ORDER(O) BY qualified_identifier(K) order_direction_opt(D). {
    A = mylite_sql_parser_make_order_by_clause(state, O, K, D);
}

order_direction_opt(A) ::= . {
    A = NULL;
}
order_direction_opt(A) ::= ASC(T). {
    A = mylite_sql_parser_make_order_direction(
        state, T, MYLITE_SQL_AST_ORDER_DIRECTION_ASC);
}
order_direction_opt(A) ::= DESC(T). {
    A = mylite_sql_parser_make_order_direction(
        state, T, MYLITE_SQL_AST_ORDER_DIRECTION_DESC);
}

limit_clause_opt(A) ::= . {
    A = NULL;
}
limit_clause_opt(A) ::= LIMIT(L) limit_integer(C). {
    A = mylite_sql_parser_make_limit_clause(state, L, C, NULL);
}
limit_clause_opt(A) ::= LIMIT(L) limit_integer(C) OFFSET limit_integer(O). {
    A = mylite_sql_parser_make_limit_clause(state, L, C, O);
}
limit_clause_opt(A) ::= LIMIT(L) limit_integer(O) COMMA limit_integer(C). {
    A = mylite_sql_parser_make_limit_clause(state, L, C, O);
}

delete_limit_clause_opt(A) ::= . {
    A = NULL;
}
delete_limit_clause_opt(A) ::= LIMIT(L) limit_integer(C). {
    A = mylite_sql_parser_make_limit_clause(state, L, C, NULL);
}

update_limit_clause_opt(A) ::= . {
    A = NULL;
}
update_limit_clause_opt(A) ::= LIMIT(L) limit_integer(C). {
    A = mylite_sql_parser_make_limit_clause(state, L, C, NULL);
}

limit_integer(A) ::= INTEGER(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER);
}

select_item_list(A) ::= select_item(B). {
    A = mylite_sql_parser_make_select_list(state, B);
}
select_item_list(A) ::= select_item_list(B) COMMA select_item(C). {
    A = mylite_sql_parser_append_select_item(state, B, C);
}

select_item(A) ::= expression(B). {
    A = mylite_sql_parser_make_select_item(state, B);
}

expression(A) ::= literal(B). {
    A = B;
}
expression(A) ::= SYSTEM_VARIABLE(T). {
    A = mylite_sql_parser_make_system_variable(state, T);
}
expression(A) ::= qualified_identifier(B). {
    A = B;
}
expression(A) ::= LPAREN(L) expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_parenthesized_expression(state, L, B, R);
}
expression(A) ::= DATABASE(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_DATABASE_FUNCTION, R);
}
expression(A) ::= SCHEMA(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_SCHEMA_FUNCTION, R);
}
expression(A) ::= USER(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_USER_FUNCTION, R);
}
expression(A) ::= SESSION_USER(T) LPAREN(L) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_zero_argument_function(
        state, T, L, MYLITE_SQL_AST_SESSION_USER_FUNCTION, R);
}
expression(A) ::= SYSTEM_USER(T) LPAREN(L) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_zero_argument_function(
        state, T, L, MYLITE_SQL_AST_SYSTEM_USER_FUNCTION, R);
}
expression(A) ::= CURRENT_USER(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_CURRENT_USER_FUNCTION, R);
}
expression(A) ::= CURRENT_USER(T). {
    A = mylite_sql_parser_make_current_user_keyword(state, T);
}
expression(A) ::= CURRENT_ROLE(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_CURRENT_ROLE_FUNCTION, R);
}
expression(A) ::= CURRENT_ROLE(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_CURRENT_ROLE_ARGUMENT_COUNT_ERROR, B, R);
}
expression(A) ::= CONNECTION_ID(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_CONNECTION_ID_FUNCTION, R);
}
expression(A) ::= CONNECTION_ID(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_CONNECTION_ID_ARGUMENT_COUNT_ERROR, B, R);
}
expression(A) ::= COUNT(T) LPAREN(L) STAR RPAREN(R). {
    A = mylite_sql_parser_make_no_space_zero_argument_function(
        state, T, L, MYLITE_SQL_AST_COUNT_STAR_FUNCTION, R);
}
expression(A) ::= COUNT(T) LPAREN(L) identifier(B) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_one_argument_function(
        state, T, L, MYLITE_SQL_AST_COUNT_COLUMN_FUNCTION, B, R);
}
expression(A) ::= COUNT(T) LPAREN(L) count_literal(B) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_one_argument_function(
        state, T, L, MYLITE_SQL_AST_COUNT_LITERAL_FUNCTION, B, R);
}
expression(A) ::= MIN(T) LPAREN(L) identifier(B) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_one_argument_function(
        state, T, L, MYLITE_SQL_AST_MIN_AGGREGATE_FUNCTION, B, R);
}
expression(A) ::= MAX(T) LPAREN(L) identifier(B) RPAREN(R). {
    A = mylite_sql_parser_make_no_space_one_argument_function(
        state, T, L, MYLITE_SQL_AST_MAX_AGGREGATE_FUNCTION, B, R);
}
expression(A) ::= VERSION(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_VERSION_FUNCTION, R);
}
expression(A) ::= VERSION(T) LPAREN function_argument_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_function_argument_count_error(
        state, T, MYLITE_SQL_AST_VERSION_ARGUMENT_COUNT_ERROR, B, R);
}
expression(A) ::= ROW_COUNT(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_ROW_COUNT_FUNCTION, R);
}
expression(A) ::= LAST_INSERT_ID(T) LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_zero_argument_function(
        state, T, MYLITE_SQL_AST_LAST_INSERT_ID_FUNCTION, R);
}
expression(A) ::= PLUS(T) expression(B). [UPLUS] {
    A = mylite_sql_parser_make_unary_expression(
        state, T, MYLITE_SQL_AST_OPERATOR_POSITIVE, B);
}
expression(A) ::= MINUS(T) expression(B). [UMINUS] {
    A = mylite_sql_parser_make_unary_expression(
        state, T, MYLITE_SQL_AST_OPERATOR_NEGATIVE, B);
}
expression(A) ::= expression(B) PLUS(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_ADD, C);
}
expression(A) ::= expression(B) MINUS(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_SUBTRACT, C);
}
expression(A) ::= expression(B) STAR(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_MULTIPLY, C);
}
expression(A) ::= expression(B) SLASH(T) expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_DIVIDE, C);
}

literal(A) ::= INTEGER(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER);
}
literal(A) ::= DECIMAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_DECIMAL);
}
literal(A) ::= FLOAT(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FLOAT);
}
literal(A) ::= STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
literal(A) ::= NATIONAL_STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_NATIONAL_STRING);
}
literal(A) ::= HEX_LITERAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_HEX);
}
literal(A) ::= BIT_LITERAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_BIT);
}
literal(A) ::= TRUE(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_TRUE);
}
literal(A) ::= FALSE(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FALSE);
}
literal(A) ::= NULL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_NULL);
}

count_literal(A) ::= INTEGER(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER);
}
count_literal(A) ::= PLUS(P) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, P, MYLITE_SQL_AST_OPERATOR_POSITIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
count_literal(A) ::= MINUS(M) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, M, MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
count_literal(A) ::= NULL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_NULL);
}
count_literal(A) ::= TRUE(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_TRUE);
}
count_literal(A) ::= FALSE(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FALSE);
}

function_argument_list(A) ::= expression(B). {
    A = mylite_sql_parser_make_function_argument_list(state, B);
}
function_argument_list(A) ::= function_argument_list(B) COMMA expression(C). {
    A = mylite_sql_parser_append_function_argument(state, B, C);
}

qualified_identifier(A) ::= identifier(B). {
    A = B;
}
qualified_identifier(A) ::= qualified_identifier(B) DOT identifier(C). {
    A = mylite_sql_parser_make_qualified_identifier(state, B, C);
}

table_name(A) ::= identifier(B). {
    A = B;
}
table_name(A) ::= identifier(B) DOT identifier(C). {
    A = mylite_sql_parser_make_qualified_identifier(state, B, C);
}

identifier(A) ::= IDENTIFIER(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= QUOTED_IDENTIFIER(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= USER(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= SESSION_USER(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= SYSTEM_USER(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= BOOL(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= BOOLEAN(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= CONNECTION_ID(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= CURRENT_ROLE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= COUNT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= MIN(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= MAX(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= VERSION(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= ROW_COUNT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= LAST_INSERT_ID(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= COLUMNS(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= FIELDS(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= INDEXES(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= FULL(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= TRIGGERS(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= EVENTS(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= OPEN(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= PROCESSLIST(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= WARNINGS(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= ERRORS(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= ENGINE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= ENGINES(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= STATUS(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= STORAGE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= CHARSET(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= CHARACTER(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= COLLATE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= COLLATION(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= VISIBLE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= INVISIBLE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}

column_definition_list(A) ::= column_definition(B). {
    A = mylite_sql_parser_make_column_definition_list(state, B);
}
column_definition_list(A) ::= column_definition_list(B) COMMA column_definition(C). {
    A = mylite_sql_parser_append_column_definition(state, B, C);
}

column_definition(A) ::= identifier(N) integer_type(T) nullability_opt(U) column_default_opt(D). {
    A = mylite_sql_parser_make_column_definition(state, N, T, U, D);
}

integer_type(A) ::= integer_type_name(T) integer_display_width_opt(W) integer_signedness_opt(S). {
    A = mylite_sql_parser_make_integer_type(
        state,
        T.type_token,
        T.integer_type,
        W.width_token,
        W.end_token,
        S.attribute_token,
        S.is_unsigned,
        0);
}
integer_type(A) ::= BOOL(T). {
    A = mylite_sql_parser_make_integer_type(
        state,
        T,
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        (struct mylite_sql_token){0},
        (struct mylite_sql_token){0},
        (struct mylite_sql_token){0},
        0,
        1);
}
integer_type(A) ::= BOOLEAN(T). {
    A = mylite_sql_parser_make_integer_type(
        state,
        T,
        MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
        (struct mylite_sql_token){0},
        (struct mylite_sql_token){0},
        (struct mylite_sql_token){0},
        0,
        1);
}

integer_type_name(A) ::= INT(T). {
    A = (struct mylite_sql_integer_type_name_tokens){
        .type_token = T,
        .integer_type = MYLITE_SQL_AST_INTEGER_TYPE_INT,
    };
}
integer_type_name(A) ::= TINYINT(T). {
    A = (struct mylite_sql_integer_type_name_tokens){
        .type_token = T,
        .integer_type = MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
    };
}
integer_type_name(A) ::= SMALLINT(T). {
    A = (struct mylite_sql_integer_type_name_tokens){
        .type_token = T,
        .integer_type = MYLITE_SQL_AST_INTEGER_TYPE_SMALLINT,
    };
}
integer_type_name(A) ::= MEDIUMINT(T). {
    A = (struct mylite_sql_integer_type_name_tokens){
        .type_token = T,
        .integer_type = MYLITE_SQL_AST_INTEGER_TYPE_MEDIUMINT,
    };
}
integer_type_name(A) ::= INTEGER_TYPE(T). {
    A = (struct mylite_sql_integer_type_name_tokens){
        .type_token = T,
        .integer_type = MYLITE_SQL_AST_INTEGER_TYPE_INT,
    };
}
integer_type_name(A) ::= BIGINT(T). {
    A = (struct mylite_sql_integer_type_name_tokens){
        .type_token = T,
        .integer_type = MYLITE_SQL_AST_INTEGER_TYPE_BIGINT,
    };
}
integer_type_name(A) ::= INT1(T). {
    A = (struct mylite_sql_integer_type_name_tokens){
        .type_token = T,
        .integer_type = MYLITE_SQL_AST_INTEGER_TYPE_TINYINT,
    };
}
integer_type_name(A) ::= INT2(T). {
    A = (struct mylite_sql_integer_type_name_tokens){
        .type_token = T,
        .integer_type = MYLITE_SQL_AST_INTEGER_TYPE_SMALLINT,
    };
}
integer_type_name(A) ::= INT3(T). {
    A = (struct mylite_sql_integer_type_name_tokens){
        .type_token = T,
        .integer_type = MYLITE_SQL_AST_INTEGER_TYPE_MEDIUMINT,
    };
}
integer_type_name(A) ::= INT4(T). {
    A = (struct mylite_sql_integer_type_name_tokens){
        .type_token = T,
        .integer_type = MYLITE_SQL_AST_INTEGER_TYPE_INT,
    };
}
integer_type_name(A) ::= INT8(T). {
    A = (struct mylite_sql_integer_type_name_tokens){
        .type_token = T,
        .integer_type = MYLITE_SQL_AST_INTEGER_TYPE_BIGINT,
    };
}

integer_display_width_opt(A) ::= . {
    A = (struct mylite_sql_integer_display_width_tokens){0};
}
integer_display_width_opt(A) ::= LPAREN INTEGER(W) RPAREN(R). {
    A = (struct mylite_sql_integer_display_width_tokens){
        .width_token = W,
        .end_token = R,
    };
}

integer_signedness_opt(A) ::= . {
    A = (struct mylite_sql_integer_signedness_tokens){0};
}
integer_signedness_opt(A) ::= SIGNED(S). {
    A = (struct mylite_sql_integer_signedness_tokens){
        .attribute_token = S,
        .is_unsigned = 0,
    };
}
integer_signedness_opt(A) ::= UNSIGNED(U). {
    A = (struct mylite_sql_integer_signedness_tokens){
        .attribute_token = U,
        .is_unsigned = 1,
    };
}

nullability_opt(A) ::= . {
    A = NULL;
}
nullability_opt(A) ::= NULL(T). {
    A = mylite_sql_parser_make_nullability(
        state, MYLITE_SQL_AST_NULLABILITY_NULL, T, T);
}
nullability_opt(A) ::= NOT(N) NULL(T). {
    A = mylite_sql_parser_make_nullability(
        state, MYLITE_SQL_AST_NULLABILITY_NOT_NULL, N, T);
}

column_default_opt(A) ::= . {
    A = NULL;
}
column_default_opt(A) ::= DEFAULT(D) NULL(N). {
    A = mylite_sql_parser_make_column_default_null(state, D, N);
}
column_default_opt(A) ::= DEFAULT(D) column_default_value(V). {
    A = mylite_sql_parser_make_column_default_value(state, D, V);
}

column_default_value(A) ::= INTEGER(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER);
}
column_default_value(A) ::= PLUS(P) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, P, MYLITE_SQL_AST_OPERATOR_POSITIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
column_default_value(A) ::= MINUS(M) INTEGER(T). {
    A = mylite_sql_parser_make_unary_expression(
        state, M, MYLITE_SQL_AST_OPERATOR_NEGATIVE,
        mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER));
}
column_default_value(A) ::= TRUE(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_TRUE);
}
column_default_value(A) ::= FALSE(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FALSE);
}
