%name mylite_sql_lemon
%token_prefix MYLITE_SQL_PARSE_
%token_type { struct mylite_sql_token }
%default_type { struct mylite_sql_ast_node * }
%realloc mylite_sql_parser_stack_realloc
%free mylite_sql_parser_stack_free
%type opt_into { struct mylite_sql_token }
%type opt_insert_ignore { struct mylite_sql_token }
%type opt_replace_modifier { struct mylite_sql_parser_replace_modifier }
%type opt_column { struct mylite_sql_token }
%type opt_table { struct mylite_sql_token }
%type opt_extended { struct mylite_sql_token }
%type opt_full { struct mylite_sql_token }
%type opt_temporary { struct mylite_sql_token }
%type opt_drop_table_mode { struct mylite_sql_token }
%type insert_values_keyword { struct mylite_sql_token }
%type opt_order_direction { struct mylite_sql_token }
%type group_concat_separator { struct mylite_sql_ast_node * }
%type opt_group_concat_separator { struct mylite_sql_ast_node * }
%type opt_work { struct mylite_sql_token }
%type opt_savepoint_keyword { struct mylite_sql_token }
%type ddl_algorithm { struct mylite_sql_token }
%type ddl_lock { struct mylite_sql_token }
%type opt_index_or_key { struct mylite_sql_token }
%type opt_index_hint_list { struct mylite_sql_token }
%type index_hint_list { struct mylite_sql_token }
%type index_hint { struct mylite_sql_token }
%type index_hint_index_or_key { struct mylite_sql_token }
%type opt_index_hint_scope { struct mylite_sql_token }
%type nonreserved_identifier_keyword { struct mylite_sql_token }
%type select_modifiers { struct mylite_sql_parser_select_duplicate_mode }
%type select_duplicate_mode { struct mylite_sql_parser_select_duplicate_mode }
%type select_duplicate_mode_list { struct mylite_sql_parser_select_duplicate_mode }
%type select_duplicate_mode_item { struct mylite_sql_parser_select_duplicate_mode }
%type union_or_except_operator { struct mylite_sql_parser_union_operator }
%type intersect_operator { struct mylite_sql_parser_union_operator }
%type window_function_name { struct mylite_sql_parser_window_function_token }
%type window_frame_unit { struct mylite_sql_parser_window_frame_unit_token }
%type window_null_treatment { struct mylite_sql_parser_window_null_treatment }
%type inner_join_operator { struct mylite_sql_parser_join_operator }
%type outer_join_operator { struct mylite_sql_parser_join_operator }
%type subquery { struct mylite_sql_parser_subquery }
%type quantified_comparison_operator { struct mylite_sql_parser_comparison_operator }
%type subquery_quantifier { enum mylite_sql_ast_subquery_quantifier }
%type trim_direction { enum mylite_sql_ast_trim_direction }
%type interval_unit { enum mylite_sql_ast_interval_unit }
%type date_interval_function_name { struct mylite_sql_ast_node * }
%type index_class { enum mylite_sql_ast_index_class }
%type fulltext_or_spatial { struct mylite_sql_parser_index_class_token }
%type index_or_key { struct mylite_sql_parser_alter_table_index_spelling_token }
%type check_or_constraint { struct mylite_sql_parser_alter_table_constraint_spelling_token }
%type opt_check_enforcement { struct mylite_sql_parser_constraint_enforcement }
%type check_enforcement { struct mylite_sql_parser_constraint_enforcement }
%type reference_action { struct mylite_sql_parser_reference_action }
%type reference_match_kind { struct mylite_sql_parser_reference_match }
%type fulltext_index_option_list { struct mylite_sql_ast_node * }
%type fulltext_index_option { struct mylite_sql_ast_node * }
%type opt_like_escape { struct mylite_sql_ast_node * }
%type opt_show_variables_scope { struct mylite_sql_parser_show_variables_scope }
%type opt_show_variables_filter { struct mylite_sql_ast_node * }
%type opt_show_status_scope { struct mylite_sql_parser_show_status_scope }
%type opt_show_status_filter { struct mylite_sql_ast_node * }
%type opt_show_storage { struct mylite_sql_token }
%type show_character_set_keyword { struct mylite_sql_token }
%type opt_show_character_set_filter { struct mylite_sql_ast_node * }
%type opt_show_collation_filter { struct mylite_sql_ast_node * }
%type opt_set_system_variable_scope { enum mylite_sql_ast_set_system_variable_scope }
%type set_system_variable_value { struct mylite_sql_ast_node * }
%type set_user_variable_assignment_list { struct mylite_sql_ast_node * }
%type set_user_variable_assignment { struct mylite_sql_ast_node * }
%type prepare_source { struct mylite_sql_ast_node * }
%type opt_execute_using_list { struct mylite_sql_ast_node * }
%type execute_using_list { struct mylite_sql_ast_node * }
%type routine_body { struct mylite_sql_ast_node * }
%type stored_program_statement { struct mylite_sql_ast_node * }
%type stored_return_statement { struct mylite_sql_ast_node * }
%type stored_set_statement { struct mylite_sql_ast_node * }
%type stored_set_assignment_list { struct mylite_sql_ast_node * }
%type stored_set_assignment { struct mylite_sql_ast_node * }
%type stored_set_target { struct mylite_sql_ast_node * }
%type create_function_tail { struct mylite_sql_ast_node * }
%type signal_condition_value { struct mylite_sql_ast_node * }
%type opt_signal_information_items { struct mylite_sql_ast_node * }
%type signal_information_item_list { struct mylite_sql_ast_node * }
%type signal_information_item { struct mylite_sql_ast_node * }
%type signal_simple_value { struct mylite_sql_ast_node * }
%type opt_show_tables_schema { struct mylite_sql_ast_node * }
%type opt_show_tables_filter { struct mylite_sql_ast_node * }
%type show_table_status_keyword { struct mylite_sql_token }
%type show_columns_keyword { struct mylite_sql_token }
%type opt_show_columns_schema { struct mylite_sql_ast_node * }
%type opt_show_columns_filter { struct mylite_sql_ast_node * }
%type show_index_keyword { struct mylite_sql_token }
%type opt_show_index_schema { struct mylite_sql_ast_node * }
%type opt_show_index_filter { struct mylite_sql_ast_node * }
%type show_create_schema_keyword { struct mylite_sql_token }
%type show_diagnostics_kind { struct mylite_sql_parser_show_diagnostics_kind }
%type opt_show_diagnostics_limit { struct mylite_sql_ast_node * }
%type describe_table_keyword { struct mylite_sql_token }
%type opt_describe_column_filter { struct mylite_sql_ast_node * }
%type explain_statement_tail { struct mylite_sql_ast_node * }
%type opt_explain_type { struct mylite_sql_ast_node * }
%type opt_explain_into { struct mylite_sql_ast_node * }
%type opt_explain_schema { struct mylite_sql_ast_node * }
%type opt_explain_analyze_format { struct mylite_sql_ast_node * }
%type explainable_statement { struct mylite_sql_ast_node * }
%type table_query_statement { struct mylite_sql_ast_node * }
%type cte_query_expression_placeholder { struct mylite_sql_ast_node * }
%type partition_options { struct mylite_sql_ast_node * }
%type parser_placeholder_tail { struct mylite_sql_ast_node * }
%type parser_placeholder_item { struct mylite_sql_ast_node * }
%type parser_placeholder_keyword { struct mylite_sql_token }
%extra_argument { struct mylite_sql_parser_state *state }

%include {
#define YYNOERRORRECOVERY 1
#include "mylite_parser_internal.h"

#include <stdlib.h>
#include <string.h>

static void *mylite_sql_parser_stack_realloc(void *pointer, size_t size, void *context)
{
    (void)context;
    return realloc(pointer, size);
}

static void mylite_sql_parser_stack_free(void *pointer, void *context)
{
    (void)context;
    free(pointer);
}
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

%left LOWEST.
%left UNION EXCEPT.
%left INTERSECT.
%left OR LOGICAL_OR.
%left XOR.
%left AND LOGICAL_AND.
%right NOT.
%left BETWEEN.
%left EQ NULL_SAFE_EQ NE LT LE GT GE IS LIKE REGEXP RLIKE IN.
%left BIT_OR.
%left BIT_AND.
%left SHIFT_LEFT SHIFT_RIGHT.
%left PLUS MINUS.
%left STAR SLASH DIV PERCENT MOD.
%left BIT_XOR.
%left JSON_EXTRACT JSON_UNQUOTE_EXTRACT.
%right UPLUS UMINUS BIT_NOT.
%right LOGICAL_NOT.
%right KEY.
%fallback IDENTIFIER ADDDATE AFTER AGGREGATE AT AUTO_INCREMENT BEGIN BOOL BOOLEAN BTREE CATALOG_NAME
    CHAIN CHARSET CLASS_ORIGIN COLLATION COLUMN_FORMAT COLUMN_NAME COMMENT COMMIT COMPLETION
    CONSISTENT CONSTRAINT_CATALOG CONSTRAINT_NAME CONSTRAINT_SCHEMA CONTAINS COUNT CURRENT
    CURSOR_NAME DATA DATE DATETIME DATE_ADD DATE_SUB DAY DEFINER DISABLE DISK DO DYNAMIC ENABLE ENDS
    ENGINE ENGINES ENGINE_ATTRIBUTE ENCRYPTION ERRORS EVENT EVERY EXTRACT FIRST FIXED FOLLOWING
    FOLLOWS HASH HOUR INSTANT
    INVISIBLE INVOKER KEY_BLOCK_SIZE LANGUAGE MEMORY MESSAGE_TEXT MINUTE MODIFY MONTH MYSQL_ERRNO
    NCHAR NO NULLS NVARCHAR OFFSET ONLY POSITION PRECEDES PRECEDING PRESERVE QUARTER REPLICA
    RESPECT RETURNS ROLLBACK SAVEPOINT SCHEDULE SCHEMA_NAME SECOND SECONDARY_ENGINE_ATTRIBUTE
    SECURITY SIGNED SLAVE SNAPSHOT SONAME START STARTS STORAGE STRINGKW SUBCLASS_ORIGIN SUBDATE
    TABLE_NAME TEMPORARY TEXT TIME TIMESTAMP TRANSACTION TYPE UNBOUNDED VISIBLE VALUE WARNINGS WEEK
    WORK YEAR.

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
statement(A) ::= union_query_expression(B). {
    A = B;
}
statement(A) ::= parenthesized_query_expression(B). {
    A = B;
}
statement(A) ::= cte_query_expression_placeholder(B). {
    A = B;
}
statement(A) ::= table_query_statement(B). {
    A = B;
}
statement(A) ::= values_query_statement(B). {
    A = B;
}
statement(A) ::= use_statement(B). {
    A = B;
}
statement(A) ::= create_schema_statement(B). {
    A = B;
}
statement(A) ::= alter_schema_statement(B). {
    A = B;
}
statement(A) ::= alter_table_partition_statement(B). {
    A = B;
}
statement(A) ::= alter_table_statement(B). {
    A = B;
}
statement(A) ::= drop_schema_statement(B). {
    A = B;
}
statement(A) ::= drop_table_statement(B). {
    A = B;
}
statement(A) ::= rename_table_statement(B). {
    A = B;
}
statement(A) ::= truncate_table_statement(B). {
    A = B;
}
statement(A) ::= table_maintenance_statement(B). {
    A = B;
}
statement(A) ::= lock_tables_statement(B). {
    A = B;
}
statement(A) ::= unlock_tables_statement(B). {
    A = B;
}
statement(A) ::= insert_values_statement(B). {
    A = B;
}
statement(A) ::= insert_set_statement(B). {
    A = B;
}
statement(A) ::= replace_values_statement(B). {
    A = B;
}
statement(A) ::= replace_set_statement(B). {
    A = B;
}
statement(A) ::= update_statement(B). {
    A = B;
}
statement(A) ::= delete_statement(B). {
    A = B;
}
statement(A) ::= start_transaction_statement(B). {
    A = B;
}
statement(A) ::= begin_transaction_statement(B). {
    A = B;
}
statement(A) ::= commit_statement(B). {
    A = B;
}
statement(A) ::= rollback_statement(B). {
    A = B;
}
statement(A) ::= savepoint_statement(B). {
    A = B;
}
statement(A) ::= rollback_to_savepoint_statement(B). {
    A = B;
}
statement(A) ::= release_savepoint_statement(B). {
    A = B;
}
statement(A) ::= show_schemas_statement(B). {
    A = B;
}
statement(A) ::= show_variables_statement(B). {
    A = B;
}
statement(A) ::= show_status_statement(B). {
    A = B;
}
statement(A) ::= show_engines_statement(B). {
    A = B;
}
statement(A) ::= show_character_set_statement(B). {
    A = B;
}
statement(A) ::= show_collation_statement(B). {
    A = B;
}
statement(A) ::= show_tables_statement(B). {
    A = B;
}
statement(A) ::= show_table_status_statement(B). {
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
statement(A) ::= show_create_schema_statement(B). {
    A = B;
}
statement(A) ::= show_diagnostics_statement(B). {
    A = B;
}
statement(A) ::= describe_table_statement(B). {
    A = B;
}
statement(A) ::= explain_statement(B). {
    A = B;
}
statement(A) ::= set_names_statement(B). {
    A = B;
}
statement(A) ::= set_character_set_statement(B). {
    A = B;
}
statement(A) ::= set_system_variable_statement(B). {
    A = B;
}
statement(A) ::= set_user_variable_statement(B). {
    A = B;
}
statement(A) ::= prepare_statement(B). {
    A = B;
}
statement(A) ::= execute_statement(B). {
    A = B;
}
statement(A) ::= deallocate_prepare_statement(B). {
    A = B;
}
statement(A) ::= call_statement(B). {
    A = B;
}
statement(A) ::= create_procedure_statement(B). {
    A = B;
}
statement(A) ::= create_function_statement(B). {
    A = B;
}
statement(A) ::= create_trigger_statement(B). {
    A = B;
}
statement(A) ::= create_event_statement(B). {
    A = B;
}
statement(A) ::= drop_procedure_statement(B). {
    A = B;
}
statement(A) ::= drop_function_statement(B). {
    A = B;
}
statement(A) ::= drop_trigger_statement(B). {
    A = B;
}
statement(A) ::= drop_event_statement(B). {
    A = B;
}
statement(A) ::= signal_statement(B). {
    A = B;
}
statement(A) ::= alter_user_statement(B). {
    A = B;
}
statement(A) ::= create_user_statement(B). {
    A = B;
}
statement(A) ::= create_role_statement(B). {
    A = B;
}
statement(A) ::= drop_user_statement(B). {
    A = B;
}
statement(A) ::= drop_role_statement(B). {
    A = B;
}
statement(A) ::= grant_statement(B). {
    A = B;
}
statement(A) ::= rename_user_statement(B). {
    A = B;
}
statement(A) ::= revoke_statement(B). {
    A = B;
}
statement(A) ::= set_default_role_statement(B). {
    A = B;
}
statement(A) ::= set_password_statement(B). {
    A = B;
}
statement(A) ::= set_role_statement(B). {
    A = B;
}
statement(A) ::= show_grants_statement(B). {
    A = B;
}
statement(A) ::= show_privileges_statement(B). {
    A = B;
}
statement(A) ::= create_partitioned_table_statement(B). {
    A = B;
}
statement(A) ::= create_table_statement(B). {
    A = B;
}
statement(A) ::= create_index_statement(B). {
    A = B;
}
statement(A) ::= drop_index_statement(B). {
    A = B;
}

use_statement(A) ::= USE(T) identifier(B). {
    A = mylite_sql_parser_make_use_statement(state, T, B);
}

create_schema_statement(A) ::= CREATE(T) DATABASE opt_if_not_exists(B) identifier(C) schema_create_option_list(D). {
    A = mylite_sql_parser_make_create_schema_statement(state, T, B, C, D);
}
create_schema_statement(A) ::= CREATE(T) SCHEMA opt_if_not_exists(B) identifier(C) schema_create_option_list(D). {
    A = mylite_sql_parser_make_create_schema_statement(state, T, B, C, D);
}

alter_schema_statement(A) ::= ALTER(T) DATABASE schema_alter_option_list(C). {
    A = mylite_sql_parser_make_alter_schema_statement(state, T, NULL, C);
}
alter_schema_statement(A) ::= ALTER(T) DATABASE identifier(B) schema_alter_option_list(C). {
    A = mylite_sql_parser_make_alter_schema_statement(state, T, B, C);
}
alter_schema_statement(A) ::= ALTER(T) SCHEMA schema_alter_option_list(C). {
    A = mylite_sql_parser_make_alter_schema_statement(state, T, NULL, C);
}
alter_schema_statement(A) ::= ALTER(T) SCHEMA identifier(B) schema_alter_option_list(C). {
    A = mylite_sql_parser_make_alter_schema_statement(state, T, B, C);
}

drop_schema_statement(A) ::= DROP(T) DATABASE opt_if_exists(B) identifier(C). {
    A = mylite_sql_parser_make_drop_schema_statement(state, T, B, C);
}
drop_schema_statement(A) ::= DROP(T) SCHEMA opt_if_exists(B) identifier(C). {
    A = mylite_sql_parser_make_drop_schema_statement(state, T, B, C);
}

drop_table_statement(A) ::= DROP(T) opt_temporary(U) TABLE opt_if_exists(B) drop_table_name_list(C) opt_drop_table_mode(D). {
    A = mylite_sql_parser_make_drop_table_statement(
        state,
        (struct mylite_sql_parser_drop_table_tokens){
            .drop = T,
            .temporary = U,
            .mode = D,
        },
        B,
        C);
}

rename_table_statement(A) ::= RENAME(T) TABLE rename_table_pair_list(P). {
    A = mylite_sql_parser_make_rename_table_statement(state, T, P);
}

truncate_table_statement(A) ::= TRUNCATE(T) opt_table table_name(N). {
    A = mylite_sql_parser_make_truncate_table_statement(state, T, N);
}

alter_table_statement(A) ::= ALTER(T) TABLE table_name(B) alter_table_item_list(C). {
    A = mylite_sql_parser_make_alter_table_statement(state, T, B, C);
}

alter_table_item_list(A) ::= alter_table_item(B). {
    A = mylite_sql_parser_make_alter_table_item_list(state, B);
}
alter_table_item_list(A) ::= alter_table_item_list(B) COMMA alter_table_item(C). {
    A = mylite_sql_parser_append_alter_table_item(state, B, C);
}

alter_table_item(A) ::= alter_table_action(B). {
    A = B;
}
alter_table_item(A) ::= ddl_table_option(B). {
    A = B;
}

alter_table_action(A) ::= ADD(T) opt_constraint_symbol(C) PRIMARY KEY opt_index_type(I) LPAREN key_part_list(K) RPAREN index_option_list(O). {
    A = mylite_sql_parser_make_alter_table_add_primary_key_action(state, T, C, I, K, O);
}
alter_table_action(A) ::= DROP(T) PRIMARY KEY(K). {
    A = mylite_sql_parser_make_alter_table_drop_primary_key_action(state, T, K);
}
alter_table_action(A) ::= ADD(T) opt_constraint_symbol(C) UNIQUE opt_index_or_key opt_index_name(N) opt_index_type(I) LPAREN key_part_list(K) RPAREN index_option_list(O). {
    A = mylite_sql_parser_make_alter_table_add_unique_index_action(state, T, C, N, I, K, O);
}
alter_table_action(A) ::= ADD(T) index_or_key opt_index_name(N) opt_index_type(I) LPAREN key_part_list(K) RPAREN index_option_list(O). {
    A = mylite_sql_parser_make_alter_table_add_secondary_index_action(state, T, N, I, K, O);
}
alter_table_action(A) ::= ADD fulltext_or_spatial(C) opt_index_or_key opt_index_name(N) LPAREN key_part_list(K) RPAREN fulltext_index_option_list(O). {
    A = mylite_sql_parser_make_alter_table_add_special_index_action(state, C, N, K, O);
}
alter_table_action(A) ::= DROP(T) index_or_key(K) identifier(N). {
    A = mylite_sql_parser_make_alter_table_drop_index_action(state, T, K, N);
}
alter_table_action(A) ::= RENAME(T) index_or_key(K) identifier(O) TO(X) identifier(N). {
    A = mylite_sql_parser_make_alter_table_rename_index_action(state, T, K, O, X, N);
}
alter_table_action(A) ::= ALTER(T) INDEX identifier(N) VISIBLE(V). {
    A = mylite_sql_parser_make_alter_table_alter_index_visibility_action(
        state, T, N, V, MYLITE_SQL_AST_INDEX_OPTION_VISIBLE);
}
alter_table_action(A) ::= ALTER(T) INDEX identifier(N) INVISIBLE(V). {
    A = mylite_sql_parser_make_alter_table_alter_index_visibility_action(
        state, T, N, V, MYLITE_SQL_AST_INDEX_OPTION_INVISIBLE);
}
alter_table_action(A) ::= ADD(T) opt_constraint_symbol(C) CHECK LPAREN expression(E) RPAREN opt_check_enforcement(N). {
    A = mylite_sql_parser_make_alter_table_add_check_action(state, T, C, E, N);
}
alter_table_action(A) ::= DROP(T) check_or_constraint(K) identifier(N). {
    A = mylite_sql_parser_make_alter_table_drop_check_or_constraint_action(state, T, K, N);
}
alter_table_action(A) ::= ALTER(T) check_or_constraint(K) identifier(N) check_enforcement(E). {
    A = mylite_sql_parser_make_alter_table_alter_check_or_constraint_action(state, T, K, N, E);
}
alter_table_action(A) ::= ADD(T) opt_constraint_symbol(C) FOREIGN KEY opt_index_name(N) LPAREN identifier_list(I) RPAREN reference_definition(R). {
    A = mylite_sql_parser_make_alter_table_add_foreign_key_action(state, T, C, N, I, R);
}
alter_table_action(A) ::= DROP(T) FOREIGN KEY(K) identifier(N). {
    A = mylite_sql_parser_make_alter_table_drop_foreign_key_action(state, T, K, N);
}
alter_table_action(A) ::= RENAME(T) table_name(N). {
    A = mylite_sql_parser_make_alter_table_rename_table_action(state, T, N);
}
alter_table_action(A) ::= RENAME(T) TO table_name(N). {
    A = mylite_sql_parser_make_alter_table_rename_table_action(state, T, N);
}
alter_table_action(A) ::= RENAME(T) AS table_name(N). {
    A = mylite_sql_parser_make_alter_table_rename_table_action(state, T, N);
}
alter_table_action(A) ::= ADD(T) opt_column(C) column_definition(D) opt_column_position(P). {
    A = mylite_sql_parser_make_alter_table_add_column_action(
        state,
        (struct mylite_sql_parser_alter_table_action_tokens){.action = T, .column = C},
        D, P);
}
alter_table_action(A) ::= DROP(T) opt_column(C) identifier(B). {
    A = mylite_sql_parser_make_alter_table_drop_column_action(
        state,
        (struct mylite_sql_parser_alter_table_action_tokens){.action = T, .column = C},
        B);
}
alter_table_action(A) ::= RENAME(T) COLUMN identifier(B) TO(O) identifier(C). {
    A = mylite_sql_parser_make_alter_table_rename_column_action(state, T, B, O, C);
}
alter_table_action(A) ::= CHANGE(T) opt_column(C) identifier(B) column_definition(D) opt_column_position(P). {
    A = mylite_sql_parser_make_alter_table_change_column_action(
        state,
        (struct mylite_sql_parser_alter_table_action_tokens){.action = T, .column = C},
        B, D, P);
}
alter_table_action(A) ::= MODIFY(T) opt_column(C) column_definition(D) opt_column_position(P). {
    A = mylite_sql_parser_make_alter_table_modify_column_action(
        state,
        (struct mylite_sql_parser_alter_table_action_tokens){.action = T, .column = C},
        D, P);
}

opt_index_or_key(A) ::= . {
    A = (struct mylite_sql_token){0};
}
opt_index_or_key(A) ::= INDEX(T). {
    A = T;
}
opt_index_or_key(A) ::= KEY(T). {
    A = T;
}

opt_constraint_symbol(A) ::= . {
    A = NULL;
}
opt_constraint_symbol(A) ::= CONSTRAINT. {
    A = NULL;
}
opt_constraint_symbol(A) ::= CONSTRAINT identifier(B). {
    A = B;
}

index_or_key(A) ::= INDEX(T). {
    A = (struct mylite_sql_parser_alter_table_index_spelling_token){
        .token = T,
        .spelling = MYLITE_SQL_AST_ALTER_TABLE_INDEX_SPELLING_INDEX,
    };
}
index_or_key(A) ::= KEY(T). {
    A = (struct mylite_sql_parser_alter_table_index_spelling_token){
        .token = T,
        .spelling = MYLITE_SQL_AST_ALTER_TABLE_INDEX_SPELLING_KEY,
    };
}

fulltext_or_spatial(A) ::= FULLTEXT(T). {
    A = (struct mylite_sql_parser_index_class_token){
        .token = T,
        .index_class = MYLITE_SQL_AST_INDEX_CLASS_FULLTEXT,
    };
}
fulltext_or_spatial(A) ::= SPATIAL(T). {
    A = (struct mylite_sql_parser_index_class_token){
        .token = T,
        .index_class = MYLITE_SQL_AST_INDEX_CLASS_SPATIAL,
    };
}

check_or_constraint(A) ::= CHECK(T). {
    A = (struct mylite_sql_parser_alter_table_constraint_spelling_token){
        .token = T,
        .spelling = MYLITE_SQL_AST_ALTER_TABLE_CONSTRAINT_SPELLING_CHECK,
    };
}
check_or_constraint(A) ::= CONSTRAINT(T). {
    A = (struct mylite_sql_parser_alter_table_constraint_spelling_token){
        .token = T,
        .spelling = MYLITE_SQL_AST_ALTER_TABLE_CONSTRAINT_SPELLING_CONSTRAINT,
    };
}

opt_check_enforcement(A) ::= . {
    A = (struct mylite_sql_parser_constraint_enforcement){
        .enforcement = MYLITE_SQL_AST_CONSTRAINT_ENFORCEMENT_DEFAULT,
    };
}
opt_check_enforcement(A) ::= check_enforcement(B). {
    A = B;
}

check_enforcement(A) ::= ENFORCED(T). {
    A = (struct mylite_sql_parser_constraint_enforcement){
        .start = T,
        .end = T,
        .enforcement = MYLITE_SQL_AST_CONSTRAINT_ENFORCEMENT_ENFORCED,
    };
}
check_enforcement(A) ::= NOT(T) ENFORCED(E). {
    A = (struct mylite_sql_parser_constraint_enforcement){
        .start = T,
        .end = E,
        .enforcement = MYLITE_SQL_AST_CONSTRAINT_ENFORCEMENT_NOT_ENFORCED,
    };
}

identifier_list(A) ::= identifier(B). {
    A = mylite_sql_parser_make_identifier_list(state, B);
}
identifier_list(A) ::= identifier_list(B) COMMA identifier(C). {
    A = mylite_sql_parser_append_identifier(state, B, C);
}

reference_definition(A) ::= REFERENCES(T) table_name(B) LPAREN identifier_list(C) RPAREN reference_option_list(O). {
    A = mylite_sql_parser_make_reference_definition(state, T, B, C, O);
}

reference_option_list(A) ::= . {
    A = mylite_sql_parser_make_reference_option_list(state);
}
reference_option_list(A) ::= reference_option_list(B) reference_option(C). {
    A = mylite_sql_parser_append_reference_option(state, B, C);
}

reference_option(A) ::= ON(T) DELETE reference_action(R). {
    A = mylite_sql_parser_make_reference_action_option(
        state, T, MYLITE_SQL_AST_REFERENCE_OPTION_ON_DELETE, R);
}
reference_option(A) ::= ON(T) UPDATE reference_action(R). {
    A = mylite_sql_parser_make_reference_action_option(
        state, T, MYLITE_SQL_AST_REFERENCE_OPTION_ON_UPDATE, R);
}
reference_option(A) ::= MATCH(T) reference_match_kind(M). {
    A = mylite_sql_parser_make_reference_match_option(state, T, M);
}

reference_action(A) ::= RESTRICT(T). {
    A = (struct mylite_sql_parser_reference_action){
        .start = T,
        .end = T,
        .action = MYLITE_SQL_AST_REFERENCE_ACTION_RESTRICT,
    };
}
reference_action(A) ::= CASCADE(T). {
    A = (struct mylite_sql_parser_reference_action){
        .start = T,
        .end = T,
        .action = MYLITE_SQL_AST_REFERENCE_ACTION_CASCADE,
    };
}
reference_action(A) ::= SET(T) NULL(N). {
    A = (struct mylite_sql_parser_reference_action){
        .start = T,
        .end = N,
        .action = MYLITE_SQL_AST_REFERENCE_ACTION_SET_NULL,
    };
}
reference_action(A) ::= NO(T) ACTION(N). {
    A = (struct mylite_sql_parser_reference_action){
        .start = T,
        .end = N,
        .action = MYLITE_SQL_AST_REFERENCE_ACTION_NO_ACTION,
    };
}
reference_action(A) ::= SET(T) DEFAULT(D). {
    A = (struct mylite_sql_parser_reference_action){
        .start = T,
        .end = D,
        .action = MYLITE_SQL_AST_REFERENCE_ACTION_SET_DEFAULT,
    };
}

reference_match_kind(A) ::= SIMPLE(T). {
    A = (struct mylite_sql_parser_reference_match){
        .token = T,
        .match = MYLITE_SQL_AST_REFERENCE_MATCH_SIMPLE,
    };
}
reference_match_kind(A) ::= FULL(T). {
    A = (struct mylite_sql_parser_reference_match){
        .token = T,
        .match = MYLITE_SQL_AST_REFERENCE_MATCH_FULL,
    };
}
reference_match_kind(A) ::= PARTIAL(T). {
    A = (struct mylite_sql_parser_reference_match){
        .token = T,
        .match = MYLITE_SQL_AST_REFERENCE_MATCH_PARTIAL,
    };
}

opt_column(A) ::= . {
    A = (struct mylite_sql_token){0};
}
opt_column(A) ::= COLUMN(T). {
    A = T;
}

opt_column_position(A) ::= . {
    A = NULL;
}
opt_column_position(A) ::= FIRST(T). {
    A = mylite_sql_parser_make_alter_table_column_position(
        state, T, MYLITE_SQL_AST_ALTER_TABLE_COLUMN_POSITION_FIRST, NULL);
}
opt_column_position(A) ::= AFTER(T) identifier(B). {
    A = mylite_sql_parser_make_alter_table_column_position(
        state, T, MYLITE_SQL_AST_ALTER_TABLE_COLUMN_POSITION_AFTER, B);
}

opt_temporary(A) ::= . {
    A = (struct mylite_sql_token){0};
}
opt_temporary(A) ::= TEMPORARY(T). {
    A = T;
}

opt_table(A) ::= . {
    A = (struct mylite_sql_token){0};
}
opt_table(A) ::= TABLE(T). {
    A = T;
}

drop_table_name_list(A) ::= table_name(B). {
    A = mylite_sql_parser_make_table_name_list(state, B);
}
drop_table_name_list(A) ::= drop_table_name_list(B) COMMA table_name(C). {
    A = mylite_sql_parser_append_table_name(state, B, C);
}

rename_table_pair_list(A) ::= rename_table_pair(B). {
    A = mylite_sql_parser_make_rename_table_pair_list(state, B);
}
rename_table_pair_list(A) ::= rename_table_pair_list(B) COMMA rename_table_pair(C). {
    A = mylite_sql_parser_append_rename_table_pair(state, B, C);
}

rename_table_pair(A) ::= table_name(O) TO(T) table_name(N). {
    A = mylite_sql_parser_make_rename_table_pair(state, O, T, N);
}

table_maintenance_statement(A) ::= CHECK(T) TABLE maintenance_table_name_list(B) check_table_option_list. {
    A = mylite_sql_parser_make_placeholder_statement_with_child(
        state, MYLITE_SQL_AST_PLACEHOLDER_CHECK_TABLE, T, B);
}
table_maintenance_statement(A) ::= OPTIMIZE(T) opt_write_to_binlog TABLE maintenance_table_name_list(B). {
    A = mylite_sql_parser_make_placeholder_statement_with_child(
        state, MYLITE_SQL_AST_PLACEHOLDER_OPTIMIZE_TABLE, T, B);
}
table_maintenance_statement(A) ::= REPAIR(T) opt_write_to_binlog TABLE maintenance_table_name_list(B) repair_table_option_list. {
    A = mylite_sql_parser_make_placeholder_statement_with_child(
        state, MYLITE_SQL_AST_PLACEHOLDER_REPAIR_TABLE, T, B);
}

lock_tables_statement(A) ::= LOCK(T) TABLE lock_table_name_list(B). {
    A = mylite_sql_parser_make_placeholder_statement_with_child(
        state, MYLITE_SQL_AST_PLACEHOLDER_LOCK_TABLES, T, B);
}
lock_tables_statement(A) ::= LOCK(T) TABLES lock_table_name_list(B). {
    A = mylite_sql_parser_make_placeholder_statement_with_child(
        state, MYLITE_SQL_AST_PLACEHOLDER_LOCK_TABLES, T, B);
}

unlock_tables_statement(A) ::= UNLOCK(T) TABLES(E). {
    A = mylite_sql_parser_make_placeholder_statement_with_end_token(
        state, MYLITE_SQL_AST_PLACEHOLDER_UNLOCK_TABLES, T, E);
}
unlock_tables_statement(A) ::= UNLOCK(T) TABLE(E). {
    A = mylite_sql_parser_make_placeholder_statement_with_end_token(
        state, MYLITE_SQL_AST_PLACEHOLDER_UNLOCK_TABLES, T, E);
}

lock_table_name_list(A) ::= lock_table_name(B). {
    A = mylite_sql_parser_make_table_name_list(state, B);
}
lock_table_name_list(A) ::= lock_table_name_list(B) COMMA lock_table_name(C). {
    A = mylite_sql_parser_append_table_name(state, B, C);
}

lock_table_name(A) ::= table_name(B) opt_table_alias lock_type. {
    A = B;
}

lock_type ::= READ opt_local.
lock_type ::= WRITE.

opt_local ::= .
opt_local ::= LOCAL.

maintenance_table_name_list(A) ::= table_name(B). {
    A = mylite_sql_parser_make_table_name_list(state, B);
}
maintenance_table_name_list(A) ::= maintenance_table_name_list(B) COMMA table_name(C). {
    A = mylite_sql_parser_append_table_name(state, B, C);
}

opt_write_to_binlog ::= .
opt_write_to_binlog ::= NO_WRITE_TO_BINLOG.
opt_write_to_binlog ::= LOCAL.

check_table_option_list ::= .
check_table_option_list ::= check_table_option_list check_table_option.
check_table_option ::= QUICK.
check_table_option ::= FAST.
check_table_option ::= MEDIUM.
check_table_option ::= EXTENDED.
check_table_option ::= CHANGED.
check_table_option ::= FOR UPGRADE.

repair_table_option_list ::= .
repair_table_option_list ::= repair_table_option_list repair_table_option.
repair_table_option ::= QUICK.
repair_table_option ::= EXTENDED.
repair_table_option ::= USE_FRM.

opt_drop_table_mode(A) ::= . {
    A = (struct mylite_sql_token){0};
}
opt_drop_table_mode(A) ::= RESTRICT(T). {
    A = T;
}
opt_drop_table_mode(A) ::= CASCADE(T). {
    A = T;
}

insert_values_statement(A) ::= INSERT(T) opt_insert_ignore(I) opt_into table_name(B) opt_insert_column_list(C)
        insert_values_keyword insert_row_list(D) opt_insert_row_alias(E) opt_insert_duplicate_update(F). {
    A = mylite_sql_parser_make_insert_values_statement(
        state,
        (struct mylite_sql_parser_insert_tokens){.insert = T, .ignore = I},
        B,
        C,
        D,
        E,
        F);
}
insert_set_statement(A) ::= INSERT(T) opt_insert_ignore(I) opt_into table_name(B) SET insert_set_assignment_list(C)
        opt_insert_row_alias(D) opt_insert_duplicate_update(E). {
    A = mylite_sql_parser_make_insert_set_statement(
        state,
        (struct mylite_sql_parser_insert_tokens){.insert = T, .ignore = I},
        B,
        C,
        D,
        E);
}
replace_values_statement(A) ::= REPLACE(T) opt_replace_modifier(M) opt_into table_name(B)
        opt_insert_column_list(C) insert_values_keyword insert_row_list(D). {
    A = mylite_sql_parser_make_replace_values_statement(
        state,
        (struct mylite_sql_parser_replace_tokens){.replace = T, .modifier = M},
        B,
        C,
        D);
}
replace_set_statement(A) ::= REPLACE(T) opt_replace_modifier(M) opt_into table_name(B)
        SET insert_set_assignment_list(C). {
    A = mylite_sql_parser_make_replace_set_statement(
        state,
        (struct mylite_sql_parser_replace_tokens){.replace = T, .modifier = M},
        B,
        C);
}

opt_insert_ignore(A) ::= . {
    A = (struct mylite_sql_token){0};
}
opt_insert_ignore(A) ::= IGNORE(T). {
    A = T;
}

opt_replace_modifier(A) ::= . {
    A = (struct mylite_sql_parser_replace_modifier){0};
}
opt_replace_modifier(A) ::= LOW_PRIORITY(T). {
    A = (struct mylite_sql_parser_replace_modifier){.low_priority = T};
}
opt_replace_modifier(A) ::= DELAYED(T). {
    A = (struct mylite_sql_parser_replace_modifier){.delayed = T};
}

opt_into(A) ::= . {
    A = (struct mylite_sql_token){0};
}
opt_into(A) ::= INTO(T). {
    A = T;
}

opt_insert_column_list(A) ::= . {
    A = NULL;
}
opt_insert_column_list(A) ::= LPAREN RPAREN. {
    A = mylite_sql_parser_make_insert_column_list(state, NULL);
}
opt_insert_column_list(A) ::= LPAREN insert_column_list(B) RPAREN. {
    A = B;
}

insert_column_list(A) ::= identifier(B). {
    A = mylite_sql_parser_make_insert_column_list(state, B);
}
insert_column_list(A) ::= insert_column_list(B) COMMA identifier(C). {
    A = mylite_sql_parser_append_insert_column(state, B, C);
}

insert_values_keyword(A) ::= VALUES(T). {
    A = T;
}
insert_values_keyword(A) ::= VALUE(T). {
    A = T;
}

insert_row_list(A) ::= insert_row(B). {
    A = mylite_sql_parser_make_insert_row_list(state, B);
}
insert_row_list(A) ::= insert_row_list(B) COMMA insert_row(C). {
    A = mylite_sql_parser_append_insert_row(state, B, C);
}

insert_row(A) ::= LPAREN(L) opt_insert_value_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_insert_row(state, L, B, R);
}
insert_row(A) ::= ROW(T) LPAREN opt_insert_value_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_insert_row(state, T, B, R);
}

opt_insert_value_list(A) ::= . {
    A = NULL;
}
opt_insert_value_list(A) ::= insert_value_list(B). {
    A = B;
}

insert_value_list(A) ::= insert_value(B). {
    A = mylite_sql_parser_make_insert_value_list(state, B);
}
insert_value_list(A) ::= insert_value_list(B) COMMA insert_value(C). {
    A = mylite_sql_parser_append_insert_value(state, B, C);
}

insert_value(A) ::= expression(B). {
    A = B;
}
insert_value(A) ::= DEFAULT(T). {
    A = mylite_sql_parser_make_default(state, T);
}

opt_insert_row_alias(A) ::= . {
    A = NULL;
}
opt_insert_row_alias(A) ::= AS(T) identifier(B). {
    A = mylite_sql_parser_make_insert_row_alias(state, T, B, NULL);
}
opt_insert_row_alias(A) ::= AS(T) identifier(B) LPAREN insert_alias_column_list(C) RPAREN. {
    A = mylite_sql_parser_make_insert_row_alias(state, T, B, C);
}

insert_alias_column_list(A) ::= identifier(B). {
    A = mylite_sql_parser_make_insert_alias_column_list(state, B);
}
insert_alias_column_list(A) ::= insert_alias_column_list(B) COMMA identifier(C). {
    A = mylite_sql_parser_append_insert_alias_column(state, B, C);
}

opt_insert_duplicate_update(A) ::= . {
    A = NULL;
}
opt_insert_duplicate_update(A) ::= ON(T) DUPLICATE KEY UPDATE insert_update_assignment_list(B). {
    A = mylite_sql_parser_make_insert_duplicate_update_clause(state, T, B);
}

insert_update_assignment_list(A) ::= insert_update_assignment(B). {
    A = mylite_sql_parser_make_insert_update_assignment_list(state, B);
}
insert_update_assignment_list(A) ::= insert_update_assignment_list(B) COMMA insert_update_assignment(C). {
    A = mylite_sql_parser_append_insert_update_assignment(state, B, C);
}

insert_update_assignment(A) ::= qualified_identifier(B) EQ(T) insert_update_value(C). {
    A = mylite_sql_parser_make_insert_update_assignment(state, B, T, C);
}

insert_update_value(A) ::= expression(B). {
    A = B;
}
insert_update_value(A) ::= DEFAULT(T). {
    A = mylite_sql_parser_make_default(state, T);
}

insert_set_assignment_list(A) ::= insert_set_assignment(B). {
    A = mylite_sql_parser_make_insert_set_assignment_list(state, B);
}
insert_set_assignment_list(A) ::= insert_set_assignment_list(B) COMMA insert_set_assignment(C). {
    A = mylite_sql_parser_append_insert_set_assignment(state, B, C);
}

insert_set_assignment(A) ::= qualified_identifier(B) EQ(T) insert_set_value(C). {
    A = mylite_sql_parser_make_insert_set_assignment(state, B, T, C);
}

insert_set_value(A) ::= expression(B). {
    A = B;
}
insert_set_value(A) ::= DEFAULT(T). {
    A = mylite_sql_parser_make_default(state, T);
}

update_statement(A) ::= UPDATE(T) single_update_target(B) SET update_assignment_list(C)
        opt_where_clause(D) opt_order_by_clause(E) opt_update_limit_clause(F). {
    A = mylite_sql_parser_make_update_statement(state, T, B, C, D, E, F);
}
update_statement(A) ::= UPDATE(T) joined_update_table_references(B) SET update_assignment_list(C)
        opt_where_clause(D). {
    A = mylite_sql_parser_make_update_statement(
        state, T, mylite_sql_parser_make_from_table_references(state, T, B), C, D, NULL, NULL);
}

single_update_target(A) ::= table_name(B) opt_table_alias(C) opt_index_hint_list. {
    A = mylite_sql_parser_make_update_target(state, B, C);
}

update_assignment_list(A) ::= update_assignment(B). {
    A = mylite_sql_parser_make_update_assignment_list(state, B);
}
update_assignment_list(A) ::= update_assignment_list(B) COMMA update_assignment(C). {
    A = mylite_sql_parser_append_update_assignment(state, B, C);
}

update_assignment(A) ::= qualified_identifier(B) EQ(T) update_assignment_value(C). {
    A = mylite_sql_parser_make_update_assignment(state, B, T, C);
}

update_assignment_value(A) ::= expression(B). {
    A = B;
}
update_assignment_value(A) ::= DEFAULT(T). {
    A = mylite_sql_parser_make_default(state, T);
}

opt_update_limit_clause(A) ::= . {
    A = NULL;
}
opt_update_limit_clause(A) ::= LIMIT(T) limit_bound(B). {
    A = mylite_sql_parser_make_update_limit_clause(state, T, B);
}

joined_update_table_references(A) ::= joined_update_explicit_reference(B). {
    A = mylite_sql_parser_make_table_reference_list(state, B);
}
joined_update_table_references(A) ::= table_factor(B) COMMA joined_table_reference(C). {
    A = mylite_sql_parser_append_table_reference(
        state, mylite_sql_parser_make_table_reference_list(state, B), C);
}
joined_update_table_references(A) ::= joined_update_table_references(B) COMMA joined_table_reference(C). {
    A = mylite_sql_parser_append_table_reference(state, B, C);
}

joined_update_explicit_reference(A) ::= table_factor(B) inner_join_operator(C) table_factor(D)
        opt_inner_join_condition(E). {
    A = mylite_sql_parser_make_join_expression(state, B, C, D, E);
}
joined_update_explicit_reference(A) ::= table_factor(B) outer_join_operator(C) table_factor(D)
        outer_join_condition(E). {
    A = mylite_sql_parser_make_join_expression(state, B, C, D, E);
}
joined_update_explicit_reference(A) ::= joined_update_explicit_reference(B) inner_join_operator(C)
        table_factor(D) opt_inner_join_condition(E). {
    A = mylite_sql_parser_make_join_expression(state, B, C, D, E);
}
joined_update_explicit_reference(A) ::= joined_update_explicit_reference(B) outer_join_operator(C)
        table_factor(D) outer_join_condition(E). {
    A = mylite_sql_parser_make_join_expression(state, B, C, D, E);
}

delete_statement(A) ::= DELETE(T) FROM single_delete_target(B) opt_where_clause(C)
        opt_order_by_clause(D) opt_delete_limit_clause(E). {
    A = mylite_sql_parser_make_delete_statement(state, T, B, C, D, E);
}
delete_statement(A) ::= DELETE(T) delete_target_list(B) FROM(F) table_references(C)
        opt_where_clause(D). {
    A = mylite_sql_parser_make_multi_delete_statement(
        state, T, MYLITE_SQL_AST_DELETE_TARGETS_FROM, B,
        mylite_sql_parser_make_from_table_references(state, F, C), D);
}
delete_statement(A) ::= DELETE(T) FROM delete_using_target_list(B) USING(U) table_references(C)
        opt_where_clause(D). {
    A = mylite_sql_parser_make_multi_delete_statement(
        state, T, MYLITE_SQL_AST_DELETE_FROM_TARGETS_USING, B,
        mylite_sql_parser_make_from_table_references(state, U, C), D);
}

single_delete_target(A) ::= delete_table_name(B) opt_table_alias(C) opt_index_hint_list. {
    A = mylite_sql_parser_make_delete_target(state, B, C);
}

delete_target_list(A) ::= delete_target_name(B). {
    A = mylite_sql_parser_make_delete_target_list(state, B);
}
delete_target_list(A) ::= delete_target_list(B) COMMA delete_target_name(C). {
    A = mylite_sql_parser_append_delete_target_name(state, B, C);
}

delete_target_name(A) ::= delete_target_identifier(B). {
    A = mylite_sql_parser_make_delete_target_name(state, B, (struct mylite_sql_token){0});
}
delete_target_name(A) ::= delete_target_identifier(B) DOT STAR(C). {
    A = mylite_sql_parser_make_delete_target_name(state, B, C);
}
delete_target_name(A) ::= delete_target_identifier(B) DOT delete_target_identifier(C). {
    A = mylite_sql_parser_make_delete_target_name(
        state, mylite_sql_parser_make_qualified_identifier(state, B, C),
        (struct mylite_sql_token){0});
}
delete_target_name(A) ::= delete_target_identifier(B) DOT delete_target_identifier(C) DOT STAR(D). {
    A = mylite_sql_parser_make_delete_target_name(
        state, mylite_sql_parser_make_qualified_identifier(state, B, C), D);
}

delete_target_identifier(A) ::= IDENTIFIER(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
delete_target_identifier(A) ::= QUOTED_IDENTIFIER(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}

delete_using_target_list(A) ::= delete_using_target_name(B). {
    A = mylite_sql_parser_make_delete_target_list(state, B);
}
delete_using_target_list(A) ::= delete_using_target_list(B) COMMA delete_using_target_name(C). {
    A = mylite_sql_parser_append_delete_target_name(state, B, C);
}

delete_using_target_name(A) ::= identifier(B). {
    A = mylite_sql_parser_make_delete_target_name(state, B, (struct mylite_sql_token){0});
}
delete_using_target_name(A) ::= identifier(B) DOT STAR(C). {
    A = mylite_sql_parser_make_delete_target_name(state, B, C);
}
delete_using_target_name(A) ::= identifier(B) DOT identifier(C). {
    A = mylite_sql_parser_make_delete_target_name(
        state, mylite_sql_parser_make_qualified_identifier(state, B, C),
        (struct mylite_sql_token){0});
}
delete_using_target_name(A) ::= identifier(B) DOT identifier(C) DOT STAR(D). {
    A = mylite_sql_parser_make_delete_target_name(
        state, mylite_sql_parser_make_qualified_identifier(state, B, C), D);
}

delete_table_name(A) ::= identifier(B). {
    A = B;
}
delete_table_name(A) ::= identifier(B) DOT identifier(C). {
    A = mylite_sql_parser_make_qualified_identifier(state, B, C);
}

opt_delete_limit_clause(A) ::= . {
    A = NULL;
}
opt_delete_limit_clause(A) ::= LIMIT(T) limit_bound(B). {
    A = mylite_sql_parser_make_delete_limit_clause(state, T, B);
}

start_transaction_statement(A) ::= START(T) TRANSACTION opt_transaction_characteristics(B). {
    A = mylite_sql_parser_make_start_transaction_statement(state, T, B);
}

opt_transaction_characteristics(A) ::= . {
    A = NULL;
}
opt_transaction_characteristics(A) ::= transaction_characteristic_list(B). {
    A = B;
}

transaction_characteristic_list(A) ::= transaction_characteristic(B). {
    A = mylite_sql_parser_make_transaction_characteristic_list(state, B);
}
transaction_characteristic_list(A) ::= transaction_characteristic_list(B) COMMA transaction_characteristic(C). {
    A = mylite_sql_parser_append_transaction_characteristic(state, B, C);
}

transaction_characteristic(A) ::= READ(T) WRITE(W). {
    A = mylite_sql_parser_make_transaction_access_mode(
        state, T, W, MYLITE_SQL_AST_TRANSACTION_ACCESS_READ_WRITE);
}
transaction_characteristic(A) ::= READ(T) ONLY(O). {
    A = mylite_sql_parser_make_transaction_access_mode(
        state, T, O, MYLITE_SQL_AST_TRANSACTION_ACCESS_READ_ONLY);
}
transaction_characteristic(A) ::= WITH(T) CONSISTENT SNAPSHOT(S). {
    A = mylite_sql_parser_make_transaction_consistent_snapshot(state, T, S);
}

begin_transaction_statement(A) ::= BEGIN(T) opt_work(W). {
    A = mylite_sql_parser_make_begin_transaction_statement(
        state, (struct mylite_sql_parser_statement_tokens){.start = T, .end = W});
}

commit_statement(A) ::= COMMIT(T) opt_work(W) opt_transaction_completion(C). {
    A = mylite_sql_parser_make_commit_statement(
        state, (struct mylite_sql_parser_statement_tokens){.start = T, .end = W}, C);
}

rollback_statement(A) ::= ROLLBACK(T) opt_work(W) opt_transaction_completion(C). {
    A = mylite_sql_parser_make_rollback_statement(
        state, (struct mylite_sql_parser_statement_tokens){.start = T, .end = W}, C);
}

savepoint_statement(A) ::= SAVEPOINT(T) identifier(B). {
    A = mylite_sql_parser_make_savepoint_statement(state, T, B);
}

rollback_to_savepoint_statement(A) ::= ROLLBACK(T) opt_work TO opt_savepoint_keyword identifier(B). {
    A = mylite_sql_parser_make_rollback_to_savepoint_statement(state, T, B);
}

release_savepoint_statement(A) ::= RELEASE(T) SAVEPOINT identifier(B). {
    A = mylite_sql_parser_make_release_savepoint_statement(state, T, B);
}

opt_work(A) ::= . {
    A = (struct mylite_sql_token){0};
}
opt_work(A) ::= WORK(T). {
    A = T;
}

opt_savepoint_keyword(A) ::= . {
    A = (struct mylite_sql_token){0};
}
opt_savepoint_keyword(A) ::= SAVEPOINT(T). {
    A = T;
}

opt_transaction_completion(A) ::= . {
    A = NULL;
}
opt_transaction_completion(A) ::= RELEASE(T). {
    A = mylite_sql_parser_make_transaction_completion(
        state, (struct mylite_sql_parser_completion_tokens){.start = T, .end = T},
        MYLITE_SQL_AST_TRANSACTION_CHAIN_DEFAULT, MYLITE_SQL_AST_TRANSACTION_RELEASE_YES);
}
opt_transaction_completion(A) ::= NO(T) RELEASE(R). {
    A = mylite_sql_parser_make_transaction_completion(
        state, (struct mylite_sql_parser_completion_tokens){.start = T, .end = R},
        MYLITE_SQL_AST_TRANSACTION_CHAIN_DEFAULT, MYLITE_SQL_AST_TRANSACTION_RELEASE_NO);
}
opt_transaction_completion(A) ::= AND(T) CHAIN(C). {
    A = mylite_sql_parser_make_transaction_completion(
        state, (struct mylite_sql_parser_completion_tokens){.start = T, .end = C},
        MYLITE_SQL_AST_TRANSACTION_CHAIN_YES, MYLITE_SQL_AST_TRANSACTION_RELEASE_DEFAULT);
}
opt_transaction_completion(A) ::= AND(T) CHAIN NO RELEASE(R). {
    A = mylite_sql_parser_make_transaction_completion(
        state, (struct mylite_sql_parser_completion_tokens){.start = T, .end = R},
        MYLITE_SQL_AST_TRANSACTION_CHAIN_YES, MYLITE_SQL_AST_TRANSACTION_RELEASE_NO);
}
opt_transaction_completion(A) ::= AND(T) NO CHAIN(C). {
    A = mylite_sql_parser_make_transaction_completion(
        state, (struct mylite_sql_parser_completion_tokens){.start = T, .end = C},
        MYLITE_SQL_AST_TRANSACTION_CHAIN_NO, MYLITE_SQL_AST_TRANSACTION_RELEASE_DEFAULT);
}
opt_transaction_completion(A) ::= AND(T) NO CHAIN RELEASE(R). {
    A = mylite_sql_parser_make_transaction_completion(
        state, (struct mylite_sql_parser_completion_tokens){.start = T, .end = R},
        MYLITE_SQL_AST_TRANSACTION_CHAIN_NO, MYLITE_SQL_AST_TRANSACTION_RELEASE_YES);
}
opt_transaction_completion(A) ::= AND(T) NO CHAIN NO RELEASE(R). {
    A = mylite_sql_parser_make_transaction_completion(
        state, (struct mylite_sql_parser_completion_tokens){.start = T, .end = R},
        MYLITE_SQL_AST_TRANSACTION_CHAIN_NO, MYLITE_SQL_AST_TRANSACTION_RELEASE_NO);
}

show_schemas_statement(A) ::= SHOW(T) DATABASES(D). {
    A = mylite_sql_parser_make_show_schemas_statement(state, T, D);
}
show_schemas_statement(A) ::= SHOW(T) SCHEMAS(D). {
    A = mylite_sql_parser_make_show_schemas_statement(state, T, D);
}

show_variables_statement(A) ::= SHOW(T) opt_show_variables_scope(S) VARIABLES(V)
        opt_show_variables_filter(F). {
    A = mylite_sql_parser_make_show_variables_statement(state, T, S, V, F);
}

opt_show_variables_scope(A) ::= . {
    A = mylite_sql_parser_make_show_variables_scope(
        (struct mylite_sql_token){0}, MYLITE_SQL_AST_SHOW_VARIABLES_SESSION);
}
opt_show_variables_scope(A) ::= GLOBAL(T). {
    A = mylite_sql_parser_make_show_variables_scope(
        T, MYLITE_SQL_AST_SHOW_VARIABLES_GLOBAL);
}
opt_show_variables_scope(A) ::= SESSION(T). {
    A = mylite_sql_parser_make_show_variables_scope(
        T, MYLITE_SQL_AST_SHOW_VARIABLES_SESSION);
}
opt_show_variables_scope(A) ::= LOCAL(T). {
    A = mylite_sql_parser_make_show_variables_scope(
        T, MYLITE_SQL_AST_SHOW_VARIABLES_SESSION);
}

opt_show_variables_filter(A) ::= . {
    A = NULL;
}
opt_show_variables_filter(A) ::= LIKE STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
opt_show_variables_filter(A) ::= where_clause(B). {
    A = B;
}

show_status_statement(A) ::= SHOW(T) opt_show_status_scope(S) STATUS(V)
        opt_show_status_filter(F). {
    A = mylite_sql_parser_make_show_status_statement(state, T, S, V, F);
}

opt_show_status_scope(A) ::= . {
    A = mylite_sql_parser_make_show_status_scope(
        (struct mylite_sql_token){0}, MYLITE_SQL_AST_SHOW_STATUS_SESSION);
}
opt_show_status_scope(A) ::= GLOBAL(T). {
    A = mylite_sql_parser_make_show_status_scope(
        T, MYLITE_SQL_AST_SHOW_STATUS_GLOBAL);
}
opt_show_status_scope(A) ::= SESSION(T). {
    A = mylite_sql_parser_make_show_status_scope(
        T, MYLITE_SQL_AST_SHOW_STATUS_SESSION);
}
opt_show_status_scope(A) ::= LOCAL(T). {
    A = mylite_sql_parser_make_show_status_scope(
        T, MYLITE_SQL_AST_SHOW_STATUS_SESSION);
}

opt_show_status_filter(A) ::= . {
    A = NULL;
}
opt_show_status_filter(A) ::= LIKE STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
opt_show_status_filter(A) ::= where_clause(B). {
    A = B;
}

show_engines_statement(A) ::= SHOW(T) opt_show_storage(S) ENGINES(E). {
    A = mylite_sql_parser_make_show_engines_statement(
        state, (struct mylite_sql_parser_show_engines_tokens){
            .show = T,
            .storage = S,
            .engines = E,
        });
}

opt_show_storage(A) ::= . {
    A = (struct mylite_sql_token){0};
}
opt_show_storage(A) ::= STORAGE(T). {
    A = T;
}

show_character_set_statement(A) ::= SHOW(T) show_character_set_keyword(K)
        opt_show_character_set_filter(F). {
    A = mylite_sql_parser_make_show_character_set_statement(state, T, K, F);
}

show_character_set_keyword(A) ::= CHARACTER SET(T). {
    A = T;
}
show_character_set_keyword(A) ::= CHAR SET(T). {
    A = T;
}
show_character_set_keyword(A) ::= CHARSET(T). {
    A = T;
}

opt_show_character_set_filter(A) ::= . {
    A = NULL;
}
opt_show_character_set_filter(A) ::= LIKE STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
opt_show_character_set_filter(A) ::= where_clause(B). {
    A = B;
}

show_collation_statement(A) ::= SHOW(T) COLLATION(C) opt_show_collation_filter(F). {
    A = mylite_sql_parser_make_show_collation_statement(state, T, C, F);
}

opt_show_collation_filter(A) ::= . {
    A = NULL;
}
opt_show_collation_filter(A) ::= LIKE STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
opt_show_collation_filter(A) ::= where_clause(B). {
    A = B;
}

show_tables_statement(A) ::= SHOW(T) opt_extended(E) opt_full(F) TABLES(S)
        opt_show_tables_schema(D) opt_show_tables_filter(L). {
    A = mylite_sql_parser_make_show_tables_statement(
        state, (struct mylite_sql_parser_show_tables_tokens){
                   .show = T, .extended = E, .full = F, .tables = S},
        D, L);
}

show_table_status_statement(A) ::= SHOW(T) TABLE(S) show_table_status_keyword(K)
        opt_show_tables_schema(D) opt_show_tables_filter(L). {
    A = mylite_sql_parser_make_show_table_status_statement(
        state, (struct mylite_sql_parser_show_table_status_tokens){
                   .show = T, .table = S, .status = K},
        D, L);
}

show_table_status_keyword(A) ::= STATUS(T). {
    A = T;
}

opt_extended(A) ::= . {
    A = (struct mylite_sql_token){0};
}
opt_extended(A) ::= EXTENDED(T). {
    A = T;
}

opt_full(A) ::= . {
    A = (struct mylite_sql_token){0};
}
opt_full(A) ::= FULL(T). {
    A = T;
}

opt_show_tables_schema(A) ::= . {
    A = NULL;
}
opt_show_tables_schema(A) ::= FROM identifier(B). {
    A = B;
}
opt_show_tables_schema(A) ::= IN identifier(B). {
    A = B;
}

opt_show_tables_filter(A) ::= . {
    A = NULL;
}
opt_show_tables_filter(A) ::= LIKE STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
opt_show_tables_filter(A) ::= where_clause(B). {
    A = B;
}

show_columns_statement(A) ::= SHOW(T) opt_extended(E) opt_full(F) show_columns_keyword(C)
        FROM table_name(B) opt_show_columns_schema(D) opt_show_columns_filter(L). {
    A = mylite_sql_parser_make_show_columns_statement(
        state, (struct mylite_sql_parser_show_columns_tokens){
                   .show = T, .extended = E, .full = F, .columns = C},
        B, D, L);
}
show_columns_statement(A) ::= SHOW(T) opt_extended(E) opt_full(F) show_columns_keyword(C)
        IN table_name(B) opt_show_columns_schema(D) opt_show_columns_filter(L). {
    A = mylite_sql_parser_make_show_columns_statement(
        state, (struct mylite_sql_parser_show_columns_tokens){
                   .show = T, .extended = E, .full = F, .columns = C},
        B, D, L);
}

show_columns_keyword(A) ::= COLUMNS(T). {
    A = T;
}
show_columns_keyword(A) ::= FIELDS(T). {
    A = T;
}

opt_show_columns_schema(A) ::= . {
    A = NULL;
}
opt_show_columns_schema(A) ::= FROM identifier(B). {
    A = B;
}
opt_show_columns_schema(A) ::= IN identifier(B). {
    A = B;
}

opt_show_columns_filter(A) ::= . {
    A = NULL;
}
opt_show_columns_filter(A) ::= LIKE STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
opt_show_columns_filter(A) ::= where_clause(B). {
    A = B;
}

show_index_statement(A) ::= SHOW(T) opt_extended(E) show_index_keyword(K)
        FROM table_name(B) opt_show_index_schema(D) opt_show_index_filter(W). {
    A = mylite_sql_parser_make_show_index_statement(
        state, (struct mylite_sql_parser_show_index_tokens){
                   .show = T, .extended = E, .index = K},
        B, D, W);
}
show_index_statement(A) ::= SHOW(T) opt_extended(E) show_index_keyword(K)
        IN table_name(B) opt_show_index_schema(D) opt_show_index_filter(W). {
    A = mylite_sql_parser_make_show_index_statement(
        state, (struct mylite_sql_parser_show_index_tokens){
                   .show = T, .extended = E, .index = K},
        B, D, W);
}

describe_table_statement(A) ::= describe_table_keyword(T) table_name(B)
        opt_describe_column_filter(F). {
    A = mylite_sql_parser_make_describe_table_statement(
        state, (struct mylite_sql_parser_describe_table_tokens){.keyword = T}, B, F);
}

describe_table_keyword(A) ::= DESCRIBE(T). {
    A = T;
}
describe_table_keyword(A) ::= DESC(T). {
    A = T;
}
describe_table_keyword(A) ::= EXPLAIN(T). {
    A = T;
}

opt_describe_column_filter(A) ::= . {
    A = NULL;
}
opt_describe_column_filter(A) ::= identifier(B). {
    A = B;
}
opt_describe_column_filter(A) ::= STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}

explain_statement(A) ::= EXPLAIN(T) explain_statement_tail(B). {
    A = mylite_sql_parser_make_placeholder_statement(
        state, MYLITE_SQL_AST_PLACEHOLDER_EXPLAIN, T, B);
}
explain_statement(A) ::= DESCRIBE(T) explain_statement_tail(B). {
    A = mylite_sql_parser_make_placeholder_statement(
        state, MYLITE_SQL_AST_PLACEHOLDER_EXPLAIN, T, B);
}
explain_statement(A) ::= DESC(T) explain_statement_tail(B). {
    A = mylite_sql_parser_make_placeholder_statement(
        state, MYLITE_SQL_AST_PLACEHOLDER_EXPLAIN, T, B);
}

explain_statement_tail(A) ::= opt_explain_type opt_explain_into opt_explain_schema
        explainable_statement(B). {
    A = B;
}
explain_statement_tail(A) ::= opt_explain_type opt_explain_into FOR CONNECTION INTEGER(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER);
}
explain_statement_tail(A) ::= ANALYZE opt_explain_analyze_format opt_explain_schema
        select_statement(B). {
    A = B;
}

opt_explain_type(A) ::= . {
    A = NULL;
}
opt_explain_type(A) ::= FORMAT(T) opt_equal identifier(B). {
    (void)T;
    A = B;
}

opt_explain_into(A) ::= . {
    A = NULL;
}
opt_explain_into(A) ::= INTO USER_VARIABLE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}

opt_explain_schema(A) ::= . {
    A = NULL;
}
opt_explain_schema(A) ::= FOR SCHEMA identifier(B). {
    A = B;
}
opt_explain_schema(A) ::= FOR DATABASE identifier(B). {
    A = B;
}

opt_explain_analyze_format(A) ::= . {
    A = NULL;
}
opt_explain_analyze_format(A) ::= FORMAT(T) opt_equal identifier(B). {
    (void)T;
    A = B;
}

explainable_statement(A) ::= select_statement(B). {
    A = B;
}
explainable_statement(A) ::= union_query_expression(B). {
    A = B;
}
explainable_statement(A) ::= table_query_statement(B). {
    A = B;
}
explainable_statement(A) ::= insert_values_statement(B). {
    A = B;
}
explainable_statement(A) ::= insert_set_statement(B). {
    A = B;
}
explainable_statement(A) ::= replace_values_statement(B). {
    A = B;
}
explainable_statement(A) ::= replace_set_statement(B). {
    A = B;
}
explainable_statement(A) ::= update_statement(B). {
    A = B;
}
explainable_statement(A) ::= delete_statement(B). {
    A = B;
}

table_query_statement(A) ::= TABLE(T) table_name(B) opt_order_by_clause(C) opt_limit_clause(D). {
    A = mylite_sql_parser_make_table_query_statement(state, T, B, C, D);
}

show_index_keyword(A) ::= INDEX(T). {
    A = T;
}
show_index_keyword(A) ::= INDEXES(T). {
    A = T;
}
show_index_keyword(A) ::= KEYS(T). {
    A = T;
}

opt_show_index_schema(A) ::= . {
    A = NULL;
}
opt_show_index_schema(A) ::= FROM identifier(B). {
    A = B;
}
opt_show_index_schema(A) ::= IN identifier(B). {
    A = B;
}

opt_show_index_filter(A) ::= . {
    A = NULL;
}
opt_show_index_filter(A) ::= where_clause(B). {
    A = B;
}

show_create_table_statement(A) ::= SHOW(S) CREATE TABLE(T) table_name(B). {
    A = mylite_sql_parser_make_show_create_table_statement(
        state, (struct mylite_sql_parser_show_create_table_tokens){.show = S, .table = T}, B);
}

show_create_schema_statement(A) ::= SHOW(S) CREATE show_create_schema_keyword(K)
        opt_if_not_exists(I) identifier(B). {
    A = mylite_sql_parser_make_show_create_schema_statement(
        state, (struct mylite_sql_parser_show_create_schema_tokens){.show = S, .schema = K}, I, B);
}

show_create_schema_keyword(A) ::= DATABASE(T). {
    A = T;
}
show_create_schema_keyword(A) ::= SCHEMA(T). {
    A = T;
}

show_diagnostics_statement(A) ::= SHOW(T) show_diagnostics_kind(K)
        opt_show_diagnostics_limit(L). {
    A = mylite_sql_parser_make_show_diagnostics_statement(state, T, K, L);
}
show_diagnostics_statement(A) ::= SHOW(T) COUNT(C) LPAREN STAR RPAREN(R)
        show_diagnostics_kind(K). {
    A = mylite_sql_parser_make_show_diagnostics_count_statement(
        state,
        (struct mylite_sql_parser_show_diagnostics_count_tokens){
            .show = T, .count = C, .right_paren = R},
        K);
}

show_diagnostics_kind(A) ::= WARNINGS(T). {
    A = mylite_sql_parser_make_show_diagnostics_kind(
        T, MYLITE_SQL_AST_SHOW_DIAGNOSTICS_WARNINGS);
}
show_diagnostics_kind(A) ::= ERRORS(T). {
    A = mylite_sql_parser_make_show_diagnostics_kind(
        T, MYLITE_SQL_AST_SHOW_DIAGNOSTICS_ERRORS);
}

opt_show_diagnostics_limit(A) ::= . {
    A = NULL;
}
opt_show_diagnostics_limit(A) ::= limit_clause(B). {
    A = B;
}

set_names_statement(A) ::= SET(T) NAMES charset_value(B) opt_set_names_collation(C). {
    A = mylite_sql_parser_make_set_names_statement(state, T, B, C);
}
set_names_statement(A) ::= SET(T) NAMES DEFAULT(D). {
    A = mylite_sql_parser_make_set_names_statement(
        state, T, mylite_sql_parser_make_default(state, D), NULL);
}

opt_set_names_collation(A) ::= . {
    A = NULL;
}
opt_set_names_collation(A) ::= COLLATE charset_value(B). {
    A = B;
}

set_character_set_statement(A) ::= SET(T) CHARACTER SET charset_value(B). {
    A = mylite_sql_parser_make_set_character_set_statement(state, T, B);
}
set_character_set_statement(A) ::= SET(T) CHARACTER SET DEFAULT(D). {
    A = mylite_sql_parser_make_set_character_set_statement(
        state, T, mylite_sql_parser_make_default(state, D));
}
set_character_set_statement(A) ::= SET(T) CHARSET charset_value(B). {
    A = mylite_sql_parser_make_set_character_set_statement(state, T, B);
}
set_character_set_statement(A) ::= SET(T) CHARSET DEFAULT(D). {
    A = mylite_sql_parser_make_set_character_set_statement(
        state, T, mylite_sql_parser_make_default(state, D));
}

set_system_variable_statement(A) ::= SET(T) opt_set_system_variable_scope(S)
        set_system_variable_name(B) EQ set_system_variable_value(C). {
    A = mylite_sql_parser_make_set_system_variable_statement(state, T, S, B, C);
}

opt_set_system_variable_scope(A) ::= . {
    A = MYLITE_SQL_AST_SET_SYSTEM_VARIABLE_SESSION;
}
opt_set_system_variable_scope(A) ::= SESSION. {
    A = MYLITE_SQL_AST_SET_SYSTEM_VARIABLE_SESSION;
}
opt_set_system_variable_scope(A) ::= LOCAL. {
    A = MYLITE_SQL_AST_SET_SYSTEM_VARIABLE_SESSION;
}
opt_set_system_variable_scope(A) ::= GLOBAL. {
    A = MYLITE_SQL_AST_SET_SYSTEM_VARIABLE_GLOBAL;
}

set_system_variable_name(A) ::= IDENTIFIER(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
set_system_variable_name(A) ::= QUOTED_IDENTIFIER(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
set_system_variable_name(A) ::= SYSTEM_VARIABLE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}

set_system_variable_value(A) ::= literal(B). {
    A = B;
}
set_system_variable_value(A) ::= PLUS(T) numeric_literal(V). {
    A = mylite_sql_parser_make_unary_expression(
        state, T, MYLITE_SQL_AST_OPERATOR_POSITIVE, V);
}
set_system_variable_value(A) ::= MINUS(T) numeric_literal(V). [UMINUS] {
    A = mylite_sql_parser_make_unary_expression(
        state, T, MYLITE_SQL_AST_OPERATOR_NEGATIVE, V);
}
set_system_variable_value(A) ::= DEFAULT(T). {
    A = mylite_sql_parser_make_default(state, T);
}
set_system_variable_value(A) ::= REPLACE(T) LPAREN(L) set_system_variable_name(B) COMMA STRING(C) COMMA STRING(D) RPAREN(R). {
    struct mylite_sql_ast_node *arguments = mylite_sql_parser_make_function_argument_list(state, B);
    arguments = mylite_sql_parser_append_function_argument(
        state, arguments, mylite_sql_parser_make_literal(state, C, MYLITE_SQL_AST_LITERAL_STRING));
    arguments = mylite_sql_parser_append_function_argument(
        state, arguments, mylite_sql_parser_make_literal(state, D, MYLITE_SQL_AST_LITERAL_STRING));
    A = mylite_sql_parser_make_function_call(
        state, mylite_sql_parser_make_identifier(state, T), L, arguments, R);
}

set_user_variable_statement(A) ::= SET(T) set_user_variable_assignment_list(B). {
    A = mylite_sql_parser_make_set_user_variable_statement(state, T, B);
}

set_user_variable_assignment_list(A) ::= set_user_variable_assignment(B). {
    A = mylite_sql_parser_make_user_variable_assignment_list(state, B);
}
set_user_variable_assignment_list(A) ::= set_user_variable_assignment_list(B) COMMA set_user_variable_assignment(C). {
    A = mylite_sql_parser_append_user_variable_assignment(state, B, C);
}

set_user_variable_assignment(A) ::= USER_VARIABLE(T) EQ expression(B). {
    A = mylite_sql_parser_make_user_variable_assignment(
        state, mylite_sql_parser_make_identifier(state, T), B);
}
set_user_variable_assignment(A) ::= USER_VARIABLE(T) ASSIGN expression(B). {
    A = mylite_sql_parser_make_user_variable_assignment(
        state, mylite_sql_parser_make_identifier(state, T), B);
}

prepare_statement(A) ::= PREPARE(T) identifier(B) FROM prepare_source(C). {
    A = mylite_sql_parser_make_prepare_statement(state, T, B, C);
}

prepare_source(A) ::= STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
prepare_source(A) ::= USER_VARIABLE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}

execute_statement(A) ::= EXECUTE(T) identifier(B) opt_execute_using_list(C). {
    A = mylite_sql_parser_make_execute_statement(state, T, B, C);
}

opt_execute_using_list(A) ::= . {
    A = NULL;
}
opt_execute_using_list(A) ::= USING execute_using_list(B). {
    A = B;
}

execute_using_list(A) ::= USER_VARIABLE(T). {
    A = mylite_sql_parser_make_execute_using_list(
        state, mylite_sql_parser_make_identifier(state, T));
}
execute_using_list(A) ::= execute_using_list(B) COMMA USER_VARIABLE(T). {
    A = mylite_sql_parser_append_execute_using_variable(
        state, B, mylite_sql_parser_make_identifier(state, T));
}

deallocate_prepare_statement(A) ::= DEALLOCATE(T) PREPARE identifier(B). {
    A = mylite_sql_parser_make_deallocate_prepare_statement(state, T, B);
}
deallocate_prepare_statement(A) ::= DROP(T) PREPARE identifier(B). {
    A = mylite_sql_parser_make_deallocate_prepare_statement(state, T, B);
}

call_statement(A) ::= CALL(T) table_name(B). {
    A = mylite_sql_parser_make_placeholder_statement(
        state, MYLITE_SQL_AST_PLACEHOLDER_CALL, T, B);
}
call_statement(A) ::= CALL(T) table_name LPAREN RPAREN(R). {
    A = mylite_sql_parser_make_placeholder_statement_with_end_token(
        state, MYLITE_SQL_AST_PLACEHOLDER_CALL, T, R);
}
call_statement(A) ::= CALL(T) table_name LPAREN expression_list RPAREN(R). {
    A = mylite_sql_parser_make_placeholder_statement_with_end_token(
        state, MYLITE_SQL_AST_PLACEHOLDER_CALL, T, R);
}

create_procedure_statement(A) ::= CREATE(T) opt_definer PROCEDURE opt_if_not_exists
        table_name LPAREN opt_proc_parameter_list RPAREN routine_characteristic_list
        routine_body(B). {
    A = mylite_sql_parser_make_placeholder_statement(
        state, MYLITE_SQL_AST_PLACEHOLDER_CREATE_PROCEDURE, T, B);
}

create_function_statement(A) ::= CREATE(T) FUNCTION opt_if_not_exists
        table_name create_function_tail(B). {
    A = mylite_sql_parser_make_placeholder_statement(
        state, MYLITE_SQL_AST_PLACEHOLDER_CREATE_FUNCTION, T, B);
}
create_function_statement(A) ::= CREATE(T) DEFINER EQ definer_user FUNCTION opt_if_not_exists
        table_name LPAREN opt_func_parameter_list RPAREN RETURNS column_type
        routine_characteristic_list routine_body(B). {
    A = mylite_sql_parser_make_placeholder_statement(
        state, MYLITE_SQL_AST_PLACEHOLDER_CREATE_FUNCTION, T, B);
}
create_function_statement(A) ::= CREATE(T) AGGREGATE FUNCTION opt_if_not_exists
        table_name RETURNS loadable_function_return_type SONAME STRING(S). {
    A = mylite_sql_parser_make_placeholder_statement(
        state,
        MYLITE_SQL_AST_PLACEHOLDER_CREATE_FUNCTION,
        T,
        mylite_sql_parser_make_literal(state, S, MYLITE_SQL_AST_LITERAL_STRING));
}

create_function_tail(A) ::= LPAREN opt_func_parameter_list RPAREN RETURNS column_type
        routine_characteristic_list routine_body(B). {
    A = B;
}
create_function_tail(A) ::= RETURNS loadable_function_return_type SONAME STRING(S). {
    A = mylite_sql_parser_make_literal(state, S, MYLITE_SQL_AST_LITERAL_STRING);
}

create_trigger_statement(A) ::= CREATE(T) opt_definer TRIGGER opt_if_not_exists table_name
        trigger_time trigger_event ON table_name FOR EACH ROW opt_trigger_order routine_body(B). {
    A = mylite_sql_parser_make_placeholder_statement(
        state, MYLITE_SQL_AST_PLACEHOLDER_CREATE_TRIGGER, T, B);
}

create_event_statement(A) ::= CREATE(T) opt_definer EVENT opt_if_not_exists table_name
        ON SCHEDULE event_schedule opt_event_completion opt_event_status opt_event_comment
        DO routine_body(B). {
    A = mylite_sql_parser_make_placeholder_statement(
        state, MYLITE_SQL_AST_PLACEHOLDER_CREATE_EVENT, T, B);
}

drop_procedure_statement(A) ::= DROP(T) PROCEDURE opt_if_exists table_name(B). {
    A = mylite_sql_parser_make_placeholder_statement(
        state, MYLITE_SQL_AST_PLACEHOLDER_DROP_PROCEDURE, T, B);
}

drop_function_statement(A) ::= DROP(T) FUNCTION opt_if_exists table_name(B). {
    A = mylite_sql_parser_make_placeholder_statement(
        state, MYLITE_SQL_AST_PLACEHOLDER_DROP_FUNCTION, T, B);
}

drop_trigger_statement(A) ::= DROP(T) TRIGGER opt_if_exists table_name(B). {
    A = mylite_sql_parser_make_placeholder_statement(
        state, MYLITE_SQL_AST_PLACEHOLDER_DROP_TRIGGER, T, B);
}

drop_event_statement(A) ::= DROP(T) EVENT opt_if_exists table_name(B). {
    A = mylite_sql_parser_make_placeholder_statement(
        state, MYLITE_SQL_AST_PLACEHOLDER_DROP_EVENT, T, B);
}

signal_statement(A) ::= SIGNAL(T) signal_condition_value(B) opt_signal_information_items(C). {
    A = mylite_sql_parser_make_placeholder_statement(
        state, MYLITE_SQL_AST_PLACEHOLDER_SIGNAL, T, C == NULL ? B : C);
}

opt_definer ::= .
opt_definer ::= DEFINER EQ definer_user.

definer_user ::= CURRENT_USER.
definer_user ::= CURRENT_USER LPAREN RPAREN.
definer_user ::= identifier.
definer_user ::= STRING.
definer_user ::= STRING USER_VARIABLE.
definer_user ::= identifier USER_VARIABLE.

opt_proc_parameter_list ::= .
opt_proc_parameter_list ::= proc_parameter_list.

proc_parameter_list ::= proc_parameter.
proc_parameter_list ::= proc_parameter_list COMMA proc_parameter.

proc_parameter ::= opt_proc_parameter_mode identifier column_type.

opt_proc_parameter_mode ::= .
opt_proc_parameter_mode ::= IN.
opt_proc_parameter_mode ::= OUT.
opt_proc_parameter_mode ::= INOUT.

opt_func_parameter_list ::= .
opt_func_parameter_list ::= func_parameter_list.

func_parameter_list ::= func_parameter.
func_parameter_list ::= func_parameter_list COMMA func_parameter.

func_parameter ::= identifier column_type.

routine_characteristic_list ::= .
routine_characteristic_list ::= routine_characteristic_list routine_characteristic.

routine_characteristic ::= COMMENT STRING.
routine_characteristic ::= LANGUAGE SQL.
routine_characteristic ::= DETERMINISTIC.
routine_characteristic ::= NOT DETERMINISTIC.
routine_characteristic ::= CONTAINS SQL.
routine_characteristic ::= NO SQL.
routine_characteristic ::= READS SQL DATA.
routine_characteristic ::= MODIFIES SQL DATA.
routine_characteristic ::= SQL SECURITY DEFINER.
routine_characteristic ::= SQL SECURITY INVOKER.

routine_body(A) ::= stored_program_statement(B). {
    A = B;
}
routine_body(A) ::= BEGIN(T) stored_statement_list END(E). {
    A = mylite_sql_parser_make_stored_program_body_with_end_token(state, T, E);
}

stored_statement_list ::= .
stored_statement_list ::= stored_statement_list stored_program_statement SEMICOLON.

stored_program_statement(A) ::= select_statement(B). {
    A = B;
}
stored_program_statement(A) ::= insert_values_statement(B). {
    A = B;
}
stored_program_statement(A) ::= insert_set_statement(B). {
    A = B;
}
stored_program_statement(A) ::= replace_values_statement(B). {
    A = B;
}
stored_program_statement(A) ::= replace_set_statement(B). {
    A = B;
}
stored_program_statement(A) ::= update_statement(B). {
    A = B;
}
stored_program_statement(A) ::= delete_statement(B). {
    A = B;
}
stored_program_statement(A) ::= call_statement(B). {
    A = B;
}
stored_program_statement(A) ::= signal_statement(B). {
    A = B;
}
stored_program_statement(A) ::= stored_return_statement(B). {
    A = B;
}
stored_program_statement(A) ::= stored_set_statement(B). {
    A = B;
}

stored_return_statement(A) ::= RETURN(T) expression(B). {
    A = mylite_sql_parser_make_stored_program_body(state, T, B);
}

stored_set_statement(A) ::= SET(T) stored_set_assignment_list(B). {
    A = mylite_sql_parser_make_stored_program_body(state, T, B);
}

stored_set_assignment_list(A) ::= stored_set_assignment(B). {
    A = B;
}
stored_set_assignment_list(A) ::= stored_set_assignment_list COMMA stored_set_assignment(B). {
    A = B;
}

stored_set_assignment(A) ::= stored_set_target EQ expression(B). {
    A = B;
}
stored_set_assignment(A) ::= stored_set_target ASSIGN expression(B). {
    A = B;
}

stored_set_target(A) ::= qualified_identifier(B). {
    A = B;
}
stored_set_target(A) ::= USER_VARIABLE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
stored_set_target(A) ::= SYSTEM_VARIABLE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}

loadable_function_return_type ::= STRINGKW.
loadable_function_return_type ::= INTEGERKW.
loadable_function_return_type ::= REAL.
loadable_function_return_type ::= DECIMALKW.

trigger_time ::= BEFORE.
trigger_time ::= AFTER.

trigger_event ::= INSERT.
trigger_event ::= UPDATE.
trigger_event ::= DELETE.

opt_trigger_order ::= .
opt_trigger_order ::= FOLLOWS table_name.
opt_trigger_order ::= PRECEDES table_name.

event_schedule ::= AT event_timestamp_expression.
event_schedule ::= EVERY event_interval opt_event_starts opt_event_ends.

event_timestamp_expression ::= expression.

event_interval ::= expression event_interval_unit.

event_interval_unit ::= YEAR.
event_interval_unit ::= QUARTER.
event_interval_unit ::= MONTH.
event_interval_unit ::= DAY.
event_interval_unit ::= HOUR.
event_interval_unit ::= MINUTE.
event_interval_unit ::= WEEK.
event_interval_unit ::= SECOND.
event_interval_unit ::= YEAR_MONTH.
event_interval_unit ::= DAY_HOUR.
event_interval_unit ::= DAY_MINUTE.
event_interval_unit ::= DAY_SECOND.
event_interval_unit ::= HOUR_MINUTE.
event_interval_unit ::= HOUR_SECOND.
event_interval_unit ::= MINUTE_SECOND.

opt_event_starts ::= .
opt_event_starts ::= STARTS event_timestamp_expression.

opt_event_ends ::= .
opt_event_ends ::= ENDS event_timestamp_expression.

opt_event_completion ::= .
opt_event_completion ::= ON COMPLETION PRESERVE.
opt_event_completion ::= ON COMPLETION NOT PRESERVE.

opt_event_status ::= .
opt_event_status ::= ENABLE.
opt_event_status ::= DISABLE.
opt_event_status ::= DISABLE ON SLAVE.
opt_event_status ::= DISABLE ON REPLICA.

opt_event_comment ::= .
opt_event_comment ::= COMMENT STRING.

signal_condition_value(A) ::= SQLSTATE STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
signal_condition_value(A) ::= SQLSTATE VALUE STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
signal_condition_value(A) ::= identifier(B). {
    A = B;
}

opt_signal_information_items(A) ::= . {
    A = NULL;
}
opt_signal_information_items(A) ::= SET signal_information_item_list(B). {
    A = B;
}

signal_information_item_list(A) ::= signal_information_item(B). {
    A = B;
}
signal_information_item_list(A) ::= signal_information_item_list COMMA signal_information_item(B). {
    A = B;
}

signal_information_item(A) ::= CLASS_ORIGIN EQ signal_simple_value(B). {
    A = B;
}
signal_information_item(A) ::= SUBCLASS_ORIGIN EQ signal_simple_value(B). {
    A = B;
}
signal_information_item(A) ::= MESSAGE_TEXT EQ signal_simple_value(B). {
    A = B;
}
signal_information_item(A) ::= MYSQL_ERRNO EQ signal_simple_value(B). {
    A = B;
}
signal_information_item(A) ::= CONSTRAINT_CATALOG EQ signal_simple_value(B). {
    A = B;
}
signal_information_item(A) ::= CONSTRAINT_SCHEMA EQ signal_simple_value(B). {
    A = B;
}
signal_information_item(A) ::= CONSTRAINT_NAME EQ signal_simple_value(B). {
    A = B;
}
signal_information_item(A) ::= CATALOG_NAME EQ signal_simple_value(B). {
    A = B;
}
signal_information_item(A) ::= SCHEMA_NAME EQ signal_simple_value(B). {
    A = B;
}
signal_information_item(A) ::= TABLE_NAME EQ signal_simple_value(B). {
    A = B;
}
signal_information_item(A) ::= COLUMN_NAME EQ signal_simple_value(B). {
    A = B;
}
signal_information_item(A) ::= CURSOR_NAME EQ signal_simple_value(B). {
    A = B;
}

signal_simple_value(A) ::= literal(B). {
    A = B;
}
signal_simple_value(A) ::= qualified_identifier(B). {
    A = B;
}
signal_simple_value(A) ::= USER_VARIABLE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
signal_simple_value(A) ::= SYSTEM_VARIABLE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}

alter_user_statement(A) ::= ALTER(T) USER parser_placeholder_tail(B). {
    A = mylite_sql_parser_make_placeholder_statement(
        state, MYLITE_SQL_AST_PLACEHOLDER_ALTER_USER, T, B);
}
alter_user_statement(A) ::= ALTER(T) USER IF EXISTS parser_placeholder_tail(B). {
    A = mylite_sql_parser_make_placeholder_statement(
        state, MYLITE_SQL_AST_PLACEHOLDER_ALTER_USER, T, B);
}

create_user_statement(A) ::= CREATE(T) USER parser_placeholder_tail(B). {
    A = mylite_sql_parser_make_placeholder_statement(
        state, MYLITE_SQL_AST_PLACEHOLDER_CREATE_USER, T, B);
}
create_user_statement(A) ::= CREATE(T) USER IF NOT EXISTS parser_placeholder_tail(B). {
    A = mylite_sql_parser_make_placeholder_statement(
        state, MYLITE_SQL_AST_PLACEHOLDER_CREATE_USER, T, B);
}

create_role_statement(A) ::= CREATE(T) ROLE parser_placeholder_tail(B). {
    A = mylite_sql_parser_make_placeholder_statement(
        state, MYLITE_SQL_AST_PLACEHOLDER_CREATE_ROLE, T, B);
}
create_role_statement(A) ::= CREATE(T) ROLE IF NOT EXISTS parser_placeholder_tail(B). {
    A = mylite_sql_parser_make_placeholder_statement(
        state, MYLITE_SQL_AST_PLACEHOLDER_CREATE_ROLE, T, B);
}

drop_user_statement(A) ::= DROP(T) USER parser_placeholder_tail(B). {
    A = mylite_sql_parser_make_placeholder_statement(
        state, MYLITE_SQL_AST_PLACEHOLDER_DROP_USER, T, B);
}
drop_user_statement(A) ::= DROP(T) USER IF EXISTS parser_placeholder_tail(B). {
    A = mylite_sql_parser_make_placeholder_statement(
        state, MYLITE_SQL_AST_PLACEHOLDER_DROP_USER, T, B);
}

drop_role_statement(A) ::= DROP(T) ROLE parser_placeholder_tail(B). {
    A = mylite_sql_parser_make_placeholder_statement(
        state, MYLITE_SQL_AST_PLACEHOLDER_DROP_ROLE, T, B);
}
drop_role_statement(A) ::= DROP(T) ROLE IF EXISTS parser_placeholder_tail(B). {
    A = mylite_sql_parser_make_placeholder_statement(
        state, MYLITE_SQL_AST_PLACEHOLDER_DROP_ROLE, T, B);
}

grant_statement(A) ::= GRANT(T) parser_placeholder_tail(B). {
    A = mylite_sql_parser_make_placeholder_statement(
        state, MYLITE_SQL_AST_PLACEHOLDER_GRANT, T, B);
}

rename_user_statement(A) ::= RENAME(T) USER parser_placeholder_tail(B). {
    A = mylite_sql_parser_make_placeholder_statement(
        state, MYLITE_SQL_AST_PLACEHOLDER_RENAME_USER, T, B);
}

revoke_statement(A) ::= REVOKE(T) parser_placeholder_tail(B). {
    A = mylite_sql_parser_make_placeholder_statement(
        state, MYLITE_SQL_AST_PLACEHOLDER_REVOKE, T, B);
}

set_default_role_statement(A) ::= SET(T) DEFAULT ROLE parser_placeholder_tail(B). {
    A = mylite_sql_parser_make_placeholder_statement(
        state, MYLITE_SQL_AST_PLACEHOLDER_SET_DEFAULT_ROLE, T, B);
}

set_password_statement(A) ::= SET(T) PASSWORD parser_placeholder_tail(B). {
    A = mylite_sql_parser_make_placeholder_statement(
        state, MYLITE_SQL_AST_PLACEHOLDER_SET_PASSWORD, T, B);
}

set_role_statement(A) ::= SET(T) ROLE parser_placeholder_tail(B). {
    A = mylite_sql_parser_make_placeholder_statement(
        state, MYLITE_SQL_AST_PLACEHOLDER_SET_ROLE, T, B);
}

show_grants_statement(A) ::= SHOW(T) GRANTS(G). {
    A = mylite_sql_parser_make_placeholder_statement_with_end_token(
        state, MYLITE_SQL_AST_PLACEHOLDER_SHOW_GRANTS, T, G);
}
show_grants_statement(A) ::= SHOW(T) GRANTS parser_placeholder_tail(B). {
    A = mylite_sql_parser_make_placeholder_statement(
        state, MYLITE_SQL_AST_PLACEHOLDER_SHOW_GRANTS, T, B);
}

show_privileges_statement(A) ::= SHOW(T) PRIVILEGES(P). {
    A = mylite_sql_parser_make_placeholder_statement_with_end_token(
        state, MYLITE_SQL_AST_PLACEHOLDER_SHOW_PRIVILEGES, T, P);
}

parser_placeholder_tail(A) ::= parser_placeholder_item(B). {
    A = B;
}
parser_placeholder_tail(A) ::= parser_placeholder_tail parser_placeholder_item(B). {
    A = B;
}

parser_placeholder_item(A) ::= identifier(B). {
    A = B;
}
parser_placeholder_item(A) ::= literal(B). {
    A = B;
}
parser_placeholder_item(A) ::= USER_VARIABLE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
parser_placeholder_item(A) ::= SYSTEM_VARIABLE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
parser_placeholder_item(A) ::= LPAREN(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
parser_placeholder_item(A) ::= RPAREN(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
parser_placeholder_item(A) ::= COMMA(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
parser_placeholder_item(A) ::= DOT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
parser_placeholder_item(A) ::= STAR(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
parser_placeholder_item(A) ::= EQ(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
parser_placeholder_item(A) ::= ASSIGN(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
parser_placeholder_item(A) ::= PLUS(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
parser_placeholder_item(A) ::= MINUS(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
parser_placeholder_item(A) ::= SLASH(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
parser_placeholder_item(A) ::= PERCENT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
parser_placeholder_item(A) ::= LT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
parser_placeholder_item(A) ::= LE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
parser_placeholder_item(A) ::= GT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
parser_placeholder_item(A) ::= GE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
parser_placeholder_item(A) ::= NE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
parser_placeholder_item(A) ::= parser_placeholder_keyword(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}

parser_placeholder_keyword(A) ::= ALL(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= ALTER(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= ANALYZE(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= AND(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= AS(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= BY(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= CHECK(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= CREATE(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= CURRENT_USER(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= DATABASE(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= DEFAULT(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= DELETE(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= DROP(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= EXCEPT(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= EXECUTE(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= FOR(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= FROM(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= FUNCTION(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= GRANT(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= IN(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= INDEX(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= INSERT(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= INTO(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= INTERVAL(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= KEY(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= LINEAR(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= LOCK(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= MAXVALUE(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= NOT(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= ON(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= OPTION(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= OPTIMIZE(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= OR(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= PARTITION(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= PROCEDURE(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= RANGE(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= REFERENCES(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= REPLACE(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= RECURSIVE(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= REQUIRE(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= REVOKE(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= SELECT(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= SET(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= SHOW(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= SSL(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= TABLE(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= TO(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= TRIGGER(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= UNION(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= UNLOCK(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= UPDATE(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= USAGE(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= USING(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= VALUES(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= WHERE(T). {
    A = T;
}
parser_placeholder_keyword(A) ::= WITH(T). {
    A = T;
}

alter_table_partition_statement(A) ::= ALTER(T) TABLE table_name partition_options(B). {
    A = mylite_sql_parser_make_placeholder_statement(
        state, MYLITE_SQL_AST_PLACEHOLDER_TABLE_PARTITIONING, T, B);
}
alter_table_partition_statement(A) ::= ALTER(T) TABLE table_name ADD PARTITION parser_placeholder_tail(B). {
    A = mylite_sql_parser_make_placeholder_statement(
        state, MYLITE_SQL_AST_PLACEHOLDER_TABLE_PARTITIONING, T, B);
}
alter_table_partition_statement(A) ::= ALTER(T) TABLE table_name DROP PARTITION parser_placeholder_tail(B). {
    A = mylite_sql_parser_make_placeholder_statement(
        state, MYLITE_SQL_AST_PLACEHOLDER_TABLE_PARTITIONING, T, B);
}
alter_table_partition_statement(A) ::= ALTER(T) TABLE table_name DISCARD PARTITION parser_placeholder_tail(B). {
    A = mylite_sql_parser_make_placeholder_statement(
        state, MYLITE_SQL_AST_PLACEHOLDER_TABLE_PARTITIONING, T, B);
}
alter_table_partition_statement(A) ::= ALTER(T) TABLE table_name IMPORT PARTITION parser_placeholder_tail(B). {
    A = mylite_sql_parser_make_placeholder_statement(
        state, MYLITE_SQL_AST_PLACEHOLDER_TABLE_PARTITIONING, T, B);
}
alter_table_partition_statement(A) ::= ALTER(T) TABLE table_name TRUNCATE PARTITION parser_placeholder_tail(B). {
    A = mylite_sql_parser_make_placeholder_statement(
        state, MYLITE_SQL_AST_PLACEHOLDER_TABLE_PARTITIONING, T, B);
}
alter_table_partition_statement(A) ::= ALTER(T) TABLE table_name COALESCE PARTITION parser_placeholder_tail(B). {
    A = mylite_sql_parser_make_placeholder_statement(
        state, MYLITE_SQL_AST_PLACEHOLDER_TABLE_PARTITIONING, T, B);
}
alter_table_partition_statement(A) ::= ALTER(T) TABLE table_name REORGANIZE PARTITION parser_placeholder_tail(B). {
    A = mylite_sql_parser_make_placeholder_statement(
        state, MYLITE_SQL_AST_PLACEHOLDER_TABLE_PARTITIONING, T, B);
}
alter_table_partition_statement(A) ::= ALTER(T) TABLE table_name EXCHANGE PARTITION parser_placeholder_tail(B). {
    A = mylite_sql_parser_make_placeholder_statement(
        state, MYLITE_SQL_AST_PLACEHOLDER_TABLE_PARTITIONING, T, B);
}
alter_table_partition_statement(A) ::= ALTER(T) TABLE table_name ANALYZE PARTITION parser_placeholder_tail(B). {
    A = mylite_sql_parser_make_placeholder_statement(
        state, MYLITE_SQL_AST_PLACEHOLDER_TABLE_PARTITIONING, T, B);
}
alter_table_partition_statement(A) ::= ALTER(T) TABLE table_name CHECK PARTITION parser_placeholder_tail(B). {
    A = mylite_sql_parser_make_placeholder_statement(
        state, MYLITE_SQL_AST_PLACEHOLDER_TABLE_PARTITIONING, T, B);
}
alter_table_partition_statement(A) ::= ALTER(T) TABLE table_name OPTIMIZE PARTITION parser_placeholder_tail(B). {
    A = mylite_sql_parser_make_placeholder_statement(
        state, MYLITE_SQL_AST_PLACEHOLDER_TABLE_PARTITIONING, T, B);
}
alter_table_partition_statement(A) ::= ALTER(T) TABLE table_name REBUILD PARTITION parser_placeholder_tail(B). {
    A = mylite_sql_parser_make_placeholder_statement(
        state, MYLITE_SQL_AST_PLACEHOLDER_TABLE_PARTITIONING, T, B);
}
alter_table_partition_statement(A) ::= ALTER(T) TABLE table_name REPAIR PARTITION parser_placeholder_tail(B). {
    A = mylite_sql_parser_make_placeholder_statement(
        state, MYLITE_SQL_AST_PLACEHOLDER_TABLE_PARTITIONING, T, B);
}
alter_table_partition_statement(A) ::= ALTER(T) TABLE table_name REMOVE PARTITIONING(P). {
    A = mylite_sql_parser_make_placeholder_statement_with_end_token(
        state, MYLITE_SQL_AST_PLACEHOLDER_TABLE_PARTITIONING, T, P);
}

partition_options(A) ::= PARTITION(P) BY parser_placeholder_tail(B). {
    A = mylite_sql_parser_make_placeholder_statement(
        state, MYLITE_SQL_AST_PLACEHOLDER_TABLE_PARTITIONING, P, B);
}

cte_query_expression_placeholder(A) ::= WITH(T) parser_placeholder_tail(B). {
    A = mylite_sql_parser_make_placeholder_statement(
        state, MYLITE_SQL_AST_PLACEHOLDER_CTE, T, B);
}

create_partitioned_table_statement(A) ::= CREATE(T) opt_temporary TABLE opt_if_not_exists table_name
        LPAREN table_element_list RPAREN table_option_list partition_options(B). {
    A = mylite_sql_parser_make_placeholder_statement(
        state, MYLITE_SQL_AST_PLACEHOLDER_TABLE_PARTITIONING, T, B);
}

create_table_statement(A) ::= CREATE(T) opt_temporary(U) TABLE opt_if_not_exists(B) table_name(C) LPAREN table_element_list(D) RPAREN table_option_list(E). {
    A = mylite_sql_parser_make_create_table_statement(state, T, U, B, C, D, E);
}

create_index_statement(A) ::= CREATE(T) index_class(C) INDEX identifier(B) opt_index_type(P)
        ON table_name(D) LPAREN key_part_list(E) RPAREN index_option_list(F)
        ddl_table_option_list(G). {
    A = mylite_sql_parser_make_create_index_statement(
        state,
        (struct mylite_sql_parser_create_index_tokens){
            .create = T,
            .class_token = (struct mylite_sql_token){0},
        },
        C, B, P, D, E, F, G);
}
create_index_statement(A) ::= CREATE(T) UNIQUE(U) INDEX identifier(B) opt_index_type(P)
        ON table_name(D) LPAREN key_part_list(E) RPAREN index_option_list(F)
        ddl_table_option_list(G). {
    A = mylite_sql_parser_make_create_index_statement(
        state,
        (struct mylite_sql_parser_create_index_tokens){.create = T, .class_token = U},
        MYLITE_SQL_AST_INDEX_CLASS_UNIQUE, B, P, D, E, F, G);
}
create_index_statement(A) ::= CREATE(T) FULLTEXT(F) INDEX identifier(B) opt_index_type(P)
        ON table_name(D) LPAREN key_part_list(E) RPAREN fulltext_index_option_list(O)
        ddl_table_option_list(G). {
    A = mylite_sql_parser_make_create_index_statement(
        state,
        (struct mylite_sql_parser_create_index_tokens){.create = T, .class_token = F},
        MYLITE_SQL_AST_INDEX_CLASS_FULLTEXT, B, P, D, E, O, G);
}
create_index_statement(A) ::= CREATE(T) SPATIAL(S) INDEX identifier(B) opt_index_type(P)
        ON table_name(D) LPAREN key_part_list(E) RPAREN index_option_list(F)
        ddl_table_option_list(G). {
    A = mylite_sql_parser_make_create_index_statement(
        state,
        (struct mylite_sql_parser_create_index_tokens){.create = T, .class_token = S},
        MYLITE_SQL_AST_INDEX_CLASS_SPATIAL, B, P, D, E, F, G);
}

index_class(A) ::= . {
    A = MYLITE_SQL_AST_INDEX_CLASS_ORDINARY;
}

drop_index_statement(A) ::= DROP(T) INDEX identifier(B) ON table_name(C) ddl_table_option_list(D). {
    A = mylite_sql_parser_make_drop_index_statement(state, T, B, C, D);
}

table_element_list(A) ::= table_element(B). {
    A = mylite_sql_parser_make_column_definition_list(state, B);
}
table_element_list(A) ::= table_element_list(B) COMMA table_element(C). {
    A = mylite_sql_parser_append_column_definition(state, B, C);
}

table_element(A) ::= column_definition(B). {
    A = B;
}
table_element(A) ::= table_primary_key_constraint(B). {
    A = B;
}
table_element(A) ::= table_secondary_index(B). {
    A = B;
}
table_element(A) ::= table_unique_index(B). {
    A = B;
}

column_definition(A) ::= identifier(B) column_type(C) column_attribute_list(D). {
    A = mylite_sql_parser_make_column_definition(state, B, C, D);
}

column_type(A) ::= integer_column_type(B). {
    A = B;
}
column_type(A) ::= boolean_column_type(B). {
    A = B;
}
column_type(A) ::= character_column_type(B). {
    A = B;
}
column_type(A) ::= text_column_type(B). {
    A = B;
}
column_type(A) ::= binary_column_type(B). {
    A = B;
}
column_type(A) ::= blob_column_type(B). {
    A = B;
}
column_type(A) ::= exact_numeric_column_type(B). {
    A = B;
}
column_type(A) ::= float_column_type(B). {
    A = B;
}
column_type(A) ::= double_column_type(B). {
    A = B;
}
column_type(A) ::= temporal_column_type(B). {
    A = B;
}

integer_column_type(A) ::= integer_type_name(B) opt_integer_display_width(C). {
    A = mylite_sql_parser_set_column_display_width(state, B, C);
}
integer_column_type(A) ::= integer_column_type(B) SIGNED(T). {
    A = mylite_sql_parser_set_column_type_signed(state, B, T);
}
integer_column_type(A) ::= integer_column_type(B) UNSIGNED(T). {
    A = mylite_sql_parser_set_column_type_unsigned(state, B, T);
}

integer_type_name(A) ::= TINYINT(T). {
    A = mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_TINYINT);
}
integer_type_name(A) ::= SMALLINT(T). {
    A = mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_SMALLINT);
}
integer_type_name(A) ::= MEDIUMINT(T). {
    A = mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMINT);
}
integer_type_name(A) ::= MIDDLEINT(T). {
    A = mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMINT);
}
integer_type_name(A) ::= INT(T). {
    A = mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_INT);
}
integer_type_name(A) ::= INTEGERKW(T). {
    A = mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_INT);
}
integer_type_name(A) ::= BIGINT(T). {
    A = mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_BIGINT);
}
integer_type_name(A) ::= INT1(T). {
    A = mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_TINYINT);
}
integer_type_name(A) ::= INT2(T). {
    A = mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_SMALLINT);
}
integer_type_name(A) ::= INT3(T). {
    A = mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMINT);
}
integer_type_name(A) ::= INT4(T). {
    A = mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_INT);
}
integer_type_name(A) ::= INT8(T). {
    A = mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_BIGINT);
}

opt_integer_display_width(A) ::= . {
    A = NULL;
}
opt_integer_display_width(A) ::= LPAREN(L) INTEGER(T) RPAREN(R). {
    A = mylite_sql_parser_make_integer_display_width(
        state, (struct mylite_sql_parser_display_width_tokens){
            .left_paren = L,
            .integer = T,
            .right_paren = R,
        });
}

boolean_column_type(A) ::= BOOL(T). {
    A = mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_BOOL);
}
boolean_column_type(A) ::= BOOLEAN(T). {
    A = mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_BOOLEAN);
}

character_column_type(A) ::= CHAR(T) opt_column_length(B) character_type_attribute_list(C). {
    A = mylite_sql_parser_apply_column_type_attributes(
        state,
        mylite_sql_parser_set_column_length(
            state, mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_CHAR),
            B),
        C);
}
character_column_type(A) ::= CHARACTER(T) opt_column_length(B) character_type_attribute_list(C). {
    A = mylite_sql_parser_apply_column_type_attributes(
        state,
        mylite_sql_parser_set_column_length(
            state, mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_CHAR),
            B),
        C);
}
character_column_type(A) ::= CHAR(T) VARYING column_length(B) character_type_attribute_list(C). {
    A = mylite_sql_parser_apply_column_type_attributes(
        state,
        mylite_sql_parser_set_column_length(
            state, mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_VARCHAR),
            B),
        C);
}
character_column_type(A) ::= CHARACTER(T) VARYING column_length(B) character_type_attribute_list(C). {
    A = mylite_sql_parser_apply_column_type_attributes(
        state,
        mylite_sql_parser_set_column_length(
            state, mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_VARCHAR),
            B),
        C);
}
character_column_type(A) ::= VARCHAR(T) column_length(B) character_type_attribute_list(C). {
    A = mylite_sql_parser_apply_column_type_attributes(
        state,
        mylite_sql_parser_set_column_length(
            state, mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_VARCHAR),
            B),
        C);
}
character_column_type(A) ::= NATIONAL(T) CHAR(C) opt_column_length(B). {
    A = mylite_sql_parser_set_column_type_national(
        state,
        mylite_sql_parser_set_column_length(
            state, mylite_sql_parser_make_column_type(state, C, MYLITE_SQL_AST_COLUMN_TYPE_CHAR),
            B),
        T);
}
character_column_type(A) ::= NCHAR(T) opt_column_length(B). {
    A = mylite_sql_parser_set_column_type_national(
        state,
        mylite_sql_parser_set_column_length(
            state, mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_CHAR),
            B),
        T);
}
character_column_type(A) ::= NATIONAL(T) VARCHAR(V) column_length(B). {
    A = mylite_sql_parser_set_column_type_national(
        state,
        mylite_sql_parser_set_column_length(
            state, mylite_sql_parser_make_column_type(state, V, MYLITE_SQL_AST_COLUMN_TYPE_VARCHAR),
            B),
        T);
}
character_column_type(A) ::= NVARCHAR(T) column_length(B). {
    A = mylite_sql_parser_set_column_type_national(
        state,
        mylite_sql_parser_set_column_length(
            state, mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_VARCHAR),
            B),
        T);
}

text_column_type(A) ::= TINYTEXT(T) character_type_attribute_list(B). {
    A = mylite_sql_parser_apply_column_type_attributes(
        state, mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_TINYTEXT),
        B);
}
text_column_type(A) ::= TEXT(T) opt_column_length(B) character_type_attribute_list(C). {
    A = mylite_sql_parser_apply_column_type_attributes(
        state,
        mylite_sql_parser_set_column_length(
            state, mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_TEXT),
            B),
        C);
}
text_column_type(A) ::= MEDIUMTEXT(T) character_type_attribute_list(B). {
    A = mylite_sql_parser_apply_column_type_attributes(
        state, mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMTEXT),
        B);
}
text_column_type(A) ::= LONGTEXT(T) character_type_attribute_list(B). {
    A = mylite_sql_parser_apply_column_type_attributes(
        state, mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_LONGTEXT),
        B);
}
text_column_type(A) ::= LONG(T) VARCHAR. {
    A = mylite_sql_parser_validate_column_type(
        state, mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMTEXT));
}

binary_column_type(A) ::= BINARY(T) opt_column_length(B). {
    A = mylite_sql_parser_validate_column_type(
        state,
        mylite_sql_parser_set_column_length(
            state, mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_BINARY),
            B));
}
binary_column_type(A) ::= VARBINARY(T) column_length(B). {
    A = mylite_sql_parser_validate_column_type(
        state,
        mylite_sql_parser_set_column_length(
            state, mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_VARBINARY),
            B));
}
binary_column_type(A) ::= CHAR(T) opt_column_length(B) BYTE(Y). {
    A = mylite_sql_parser_validate_column_type(
        state,
        mylite_sql_parser_set_column_type_byte_attribute(
            state,
            mylite_sql_parser_set_column_length(
                state,
                mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_CHAR),
                B),
            Y));
}
binary_column_type(A) ::= CHARACTER(T) opt_column_length(B) BYTE(Y). {
    A = mylite_sql_parser_validate_column_type(
        state,
        mylite_sql_parser_set_column_type_byte_attribute(
            state,
            mylite_sql_parser_set_column_length(
                state,
                mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_CHAR),
                B),
            Y));
}
binary_column_type(A) ::= VARCHAR(T) column_length(B) BYTE(Y). {
    A = mylite_sql_parser_validate_column_type(
        state,
        mylite_sql_parser_set_column_type_byte_attribute(
            state,
            mylite_sql_parser_set_column_length(
                state,
                mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_VARCHAR),
                B),
            Y));
}

blob_column_type(A) ::= TINYBLOB(T). {
    A = mylite_sql_parser_validate_column_type(
        state, mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_TINYBLOB));
}
blob_column_type(A) ::= BLOB(T) opt_column_length(B). {
    A = mylite_sql_parser_validate_column_type(
        state,
        mylite_sql_parser_set_column_length(
            state, mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_BLOB),
            B));
}
blob_column_type(A) ::= MEDIUMBLOB(T). {
    A = mylite_sql_parser_validate_column_type(
        state, mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMBLOB));
}
blob_column_type(A) ::= LONGBLOB(T). {
    A = mylite_sql_parser_validate_column_type(
        state, mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_LONGBLOB));
}
blob_column_type(A) ::= LONG(T) VARBINARY. {
    A = mylite_sql_parser_validate_column_type(
        state, mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMBLOB));
}

exact_numeric_column_type(A) ::= exact_numeric_type_name(B) opt_numeric_precision_scale(C) numeric_type_attribute_list(D). {
    A = mylite_sql_parser_apply_column_type_attributes(
        state, mylite_sql_parser_validate_column_type(
                   state, mylite_sql_parser_set_column_precision_scale(state, B, C)),
        D);
}

exact_numeric_type_name(A) ::= DECIMALKW(T). {
    A = mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_DECIMAL);
}
exact_numeric_type_name(A) ::= DEC(T). {
    A = mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_DECIMAL);
}
exact_numeric_type_name(A) ::= NUMERIC(T). {
    A = mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_DECIMAL);
}
exact_numeric_type_name(A) ::= FIXED(T). {
    A = mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_DECIMAL);
}

float_column_type(A) ::= FLOATKW(T) opt_float_precision_scale(B) numeric_type_attribute_list(C). {
    A = mylite_sql_parser_apply_column_type_attributes(
        state,
        mylite_sql_parser_validate_column_type(
            state, mylite_sql_parser_set_column_precision_scale(
                       state,
                       mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_FLOAT),
                       B)),
        C);
}
float_column_type(A) ::= FLOAT4(T) opt_float_precision_scale(B) numeric_type_attribute_list(C). {
    A = mylite_sql_parser_apply_column_type_attributes(
        state,
        mylite_sql_parser_validate_column_type(
            state, mylite_sql_parser_set_column_precision_scale(
                       state,
                       mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_FLOAT),
                       B)),
        C);
}

double_column_type(A) ::= DOUBLE(T) opt_precision_keyword opt_double_precision_scale(B) numeric_type_attribute_list(C). {
    A = mylite_sql_parser_apply_column_type_attributes(
        state,
        mylite_sql_parser_validate_column_type(
            state, mylite_sql_parser_set_column_precision_scale(
                       state,
                       mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_DOUBLE),
                       B)),
        C);
}
double_column_type(A) ::= REAL(T) opt_double_precision_scale(B) numeric_type_attribute_list(C). {
    A = mylite_sql_parser_apply_column_type_attributes(
        state,
        mylite_sql_parser_validate_column_type(
            state, mylite_sql_parser_set_column_precision_scale(
                       state,
                       mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_DOUBLE),
                       B)),
        C);
}
double_column_type(A) ::= FLOAT8(T) opt_double_precision_scale(B) numeric_type_attribute_list(C). {
    A = mylite_sql_parser_apply_column_type_attributes(
        state,
        mylite_sql_parser_validate_column_type(
            state, mylite_sql_parser_set_column_precision_scale(
                       state,
                       mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_DOUBLE),
                       B)),
        C);
}

temporal_column_type(A) ::= DATE(T). {
    A = mylite_sql_parser_validate_column_type(
        state, mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_DATE));
}
temporal_column_type(A) ::= TIME(T) opt_temporal_fsp(B). {
    A = mylite_sql_parser_validate_column_type(
        state,
        mylite_sql_parser_set_column_precision_scale(
            state, mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_TIME),
            B));
}
temporal_column_type(A) ::= DATETIME(T) opt_temporal_fsp(B). {
    A = mylite_sql_parser_validate_column_type(
        state,
        mylite_sql_parser_set_column_precision_scale(
            state, mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_DATETIME),
            B));
}
temporal_column_type(A) ::= TIMESTAMP(T) opt_temporal_fsp(B). {
    A = mylite_sql_parser_validate_column_type(
        state,
        mylite_sql_parser_set_column_precision_scale(
            state, mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_TIMESTAMP),
            B));
}
temporal_column_type(A) ::= YEAR(T) opt_year_width(B). {
    A = mylite_sql_parser_validate_column_type(
        state,
        mylite_sql_parser_set_column_precision_scale(
            state, mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_YEAR),
            B));
}

opt_temporal_fsp(A) ::= . {
    A = NULL;
}
opt_temporal_fsp(A) ::= column_precision(B). {
    A = B;
}

opt_year_width(A) ::= . {
    A = NULL;
}
opt_year_width(A) ::= column_precision(B). {
    A = B;
}

opt_precision_keyword ::= .
opt_precision_keyword ::= PRECISION.

opt_numeric_precision_scale(A) ::= . {
    A = NULL;
}
opt_numeric_precision_scale(A) ::= column_precision(B). {
    A = B;
}
opt_numeric_precision_scale(A) ::= column_precision_scale(B). {
    A = B;
}

opt_float_precision_scale(A) ::= . {
    A = NULL;
}
opt_float_precision_scale(A) ::= column_precision(B). {
    A = B;
}
opt_float_precision_scale(A) ::= column_precision_scale(B). {
    A = B;
}

opt_double_precision_scale(A) ::= . {
    A = NULL;
}
opt_double_precision_scale(A) ::= column_precision_scale(B). {
    A = B;
}

numeric_type_attribute_list(A) ::= . {
    A = mylite_sql_parser_make_column_type_attribute_list(state);
}
numeric_type_attribute_list(A) ::= numeric_type_attribute_list(B) numeric_type_attribute(C). {
    A = mylite_sql_parser_apply_column_type_attributes(state, B, C);
}

numeric_type_attribute(A) ::= SIGNED(T). {
    A = mylite_sql_parser_set_column_type_signed(
        state, mylite_sql_parser_make_column_type_attribute_list(state), T);
}
numeric_type_attribute(A) ::= UNSIGNED(T). {
    A = mylite_sql_parser_set_column_type_unsigned(
        state, mylite_sql_parser_make_column_type_attribute_list(state), T);
}
numeric_type_attribute(A) ::= ZEROFILL(T). {
    A = mylite_sql_parser_set_column_type_zerofill_attribute(
        state, mylite_sql_parser_make_column_type_attribute_list(state), T);
}

character_type_attribute_list(A) ::= . {
    A = mylite_sql_parser_make_column_type_attribute_list(state);
}
character_type_attribute_list(A) ::= character_type_attribute_list(B) character_type_attribute(C). {
    A = mylite_sql_parser_apply_column_type_attributes(state, B, C);
}

character_type_attribute(A) ::= CHARACTER SET charset_value(B). {
    A = mylite_sql_parser_set_column_type_character_set(
        state, mylite_sql_parser_make_column_type_attribute_list(state), B);
}
character_type_attribute(A) ::= CHARSET charset_value(B). {
    A = mylite_sql_parser_set_column_type_character_set(
        state, mylite_sql_parser_make_column_type_attribute_list(state), B);
}
character_type_attribute(A) ::= COLLATE charset_value(B). {
    A = mylite_sql_parser_set_column_type_collation(
        state, mylite_sql_parser_make_column_type_attribute_list(state), B);
}
character_type_attribute(A) ::= BINARY(T). {
    A = mylite_sql_parser_set_column_type_binary_attribute(
        state, mylite_sql_parser_make_column_type_attribute_list(state), T);
}

column_attribute_list(A) ::= . {
    A = mylite_sql_parser_make_column_attribute_list(state);
}
column_attribute_list(A) ::= column_attribute_list(B) column_attribute(C). {
    A = mylite_sql_parser_append_column_attribute(state, B, C);
}

column_attribute(A) ::= NULL(T). {
    A = mylite_sql_parser_make_column_null_attribute(state, T);
}
column_attribute(A) ::= NOT(N) NULL(T). {
    A = mylite_sql_parser_make_column_not_null_attribute(state, N, T);
}
column_attribute(A) ::= DEFAULT(T) column_default_value(B). {
    A = mylite_sql_parser_make_column_default_attribute(state, T, B);
}
column_attribute(A) ::= ON(O) UPDATE(U) current_timestamp_value(B). {
    A = mylite_sql_parser_make_column_on_update_attribute(state, O, U, B);
}
column_attribute(A) ::= COMMENT(T) STRING(S). {
    A = mylite_sql_parser_make_column_comment_attribute(
        state, T, mylite_sql_parser_make_literal(state, S, MYLITE_SQL_AST_LITERAL_STRING));
}
column_attribute(A) ::= VISIBLE(T). {
    A = mylite_sql_parser_make_column_visibility_attribute(
        state, T, MYLITE_SQL_AST_COLUMN_ATTRIBUTE_VISIBLE);
}
column_attribute(A) ::= INVISIBLE(T). {
    A = mylite_sql_parser_make_column_visibility_attribute(
        state, T, MYLITE_SQL_AST_COLUMN_ATTRIBUTE_INVISIBLE);
}
column_attribute(A) ::= COLUMN_FORMAT(C) DEFAULT(T). {
    A = mylite_sql_parser_make_column_format_attribute(
        state, C, T, MYLITE_SQL_AST_COLUMN_FORMAT_DEFAULT);
}
column_attribute(A) ::= COLUMN_FORMAT(C) FIXED(T). {
    A = mylite_sql_parser_make_column_format_attribute(
        state, C, T, MYLITE_SQL_AST_COLUMN_FORMAT_FIXED);
}
column_attribute(A) ::= COLUMN_FORMAT(C) DYNAMIC(T). {
    A = mylite_sql_parser_make_column_format_attribute(
        state, C, T, MYLITE_SQL_AST_COLUMN_FORMAT_DYNAMIC);
}
column_attribute(A) ::= STORAGE(S) DEFAULT(T). {
    A = mylite_sql_parser_make_column_storage_attribute(
        state, S, T, MYLITE_SQL_AST_COLUMN_STORAGE_DEFAULT);
}
column_attribute(A) ::= STORAGE(S) DISK(T). {
    A = mylite_sql_parser_make_column_storage_attribute(
        state, S, T, MYLITE_SQL_AST_COLUMN_STORAGE_DISK);
}
column_attribute(A) ::= STORAGE(S) MEMORY(T). {
    A = mylite_sql_parser_make_column_storage_attribute(
        state, S, T, MYLITE_SQL_AST_COLUMN_STORAGE_MEMORY);
}
column_attribute(A) ::= AUTO_INCREMENT(T). {
    A = mylite_sql_parser_make_column_auto_increment_attribute(state, T);
}
column_attribute(A) ::= PRIMARY(P) KEY(K). {
    A = mylite_sql_parser_make_column_primary_key_attribute(state, P, K);
}
column_attribute(A) ::= KEY(T). {
    A = mylite_sql_parser_make_column_primary_key_attribute(state, T, T);
}
column_attribute(A) ::= UNIQUE(U). [LOWEST] {
    A = mylite_sql_parser_make_column_unique_key_attribute(
        state, (struct mylite_sql_parser_column_unique_key_attribute_tokens){
            .unique_token = U,
            .key_token = (struct mylite_sql_token){0},
        });
}
column_attribute(A) ::= UNIQUE(U) KEY(K). {
    A = mylite_sql_parser_make_column_unique_key_attribute(
        state, (struct mylite_sql_parser_column_unique_key_attribute_tokens){
            .unique_token = U,
            .key_token = K,
        });
}

table_primary_key_constraint(A) ::= PRIMARY(P) KEY opt_primary_key_name(B) opt_index_type(C) LPAREN key_part_list(D) RPAREN index_option_list(E). {
    A = mylite_sql_parser_make_primary_key_constraint(state, P, NULL, B, C, D, E);
}
table_primary_key_constraint(A) ::= CONSTRAINT(C) opt_constraint_name(B) PRIMARY KEY opt_primary_key_name(D) opt_index_type(E) LPAREN key_part_list(F) RPAREN index_option_list(G). {
    A = mylite_sql_parser_make_primary_key_constraint(state, C, B, D, E, F, G);
}

table_secondary_index(A) ::= KEY(T) opt_index_name(B) opt_index_type(C) LPAREN key_part_list(D) RPAREN index_option_list(E). {
    A = mylite_sql_parser_make_secondary_index(state, T, B, C, D, E);
}
table_secondary_index(A) ::= INDEX(T) opt_index_name(B) opt_index_type(C) LPAREN key_part_list(D) RPAREN index_option_list(E). {
    A = mylite_sql_parser_make_secondary_index(state, T, B, C, D, E);
}

table_unique_index(A) ::= UNIQUE(T) opt_unique_index_keyword opt_index_name(B) opt_index_type(C) LPAREN key_part_list(D) RPAREN index_option_list(E). {
    A = mylite_sql_parser_make_unique_index(state, T, NULL, B, C, D, E);
}
table_unique_index(A) ::= CONSTRAINT(C) opt_constraint_name(B) UNIQUE opt_unique_index_keyword opt_index_name(D) opt_index_type(E) LPAREN key_part_list(F) RPAREN index_option_list(G). {
    A = mylite_sql_parser_make_unique_index(state, C, B, D, E, F, G);
}

opt_constraint_name(A) ::= . {
    A = NULL;
}
opt_constraint_name(A) ::= identifier(B). {
    A = B;
}

opt_primary_key_name(A) ::= . {
    A = NULL;
}
opt_primary_key_name(A) ::= identifier(B). {
    A = B;
}

opt_index_name(A) ::= . {
    A = NULL;
}
opt_index_name(A) ::= identifier(B). {
    A = B;
}

opt_unique_index_keyword ::= .
opt_unique_index_keyword ::= KEY.
opt_unique_index_keyword ::= INDEX.

key_part_list(A) ::= key_part(B). {
    A = mylite_sql_parser_make_key_part_list(state, B);
}
key_part_list(A) ::= key_part_list(B) COMMA key_part(C). {
    A = mylite_sql_parser_append_key_part(state, B, C);
}

key_part(A) ::= identifier(B) opt_key_part_prefix(C). {
    A = mylite_sql_parser_make_key_part(
        state, B, C, MYLITE_SQL_AST_KEY_PART_ORDER_NONE, (struct mylite_sql_token){0});
}
key_part(A) ::= identifier(B) opt_key_part_prefix(C) ASC(T). {
    A = mylite_sql_parser_make_key_part(state, B, C, MYLITE_SQL_AST_KEY_PART_ORDER_ASC, T);
}
key_part(A) ::= identifier(B) opt_key_part_prefix(C) DESC(T). {
    A = mylite_sql_parser_make_key_part(state, B, C, MYLITE_SQL_AST_KEY_PART_ORDER_DESC, T);
}

opt_key_part_prefix(A) ::= . {
    A = NULL;
}
opt_key_part_prefix(A) ::= LPAREN(L) INTEGER(T) RPAREN(R). {
    A = mylite_sql_parser_make_key_part_prefix(
        state, (struct mylite_sql_parser_key_part_prefix_tokens){
            .left_paren = L,
            .integer = T,
            .right_paren = R,
        });
}

opt_index_type(A) ::= . {
    A = NULL;
}
opt_index_type(A) ::= index_type(B). {
    A = B;
}

index_type(A) ::= USING(U) BTREE(T). {
    A = mylite_sql_parser_make_index_type(state, U, T, MYLITE_SQL_AST_INDEX_ALGORITHM_BTREE);
}
index_type(A) ::= USING(U) HASH(T). {
    A = mylite_sql_parser_make_index_type(state, U, T, MYLITE_SQL_AST_INDEX_ALGORITHM_HASH);
}
index_type(A) ::= TYPE(U) BTREE(T). {
    A = mylite_sql_parser_make_index_type(state, U, T, MYLITE_SQL_AST_INDEX_ALGORITHM_BTREE);
}
index_type(A) ::= TYPE(U) HASH(T). {
    A = mylite_sql_parser_make_index_type(state, U, T, MYLITE_SQL_AST_INDEX_ALGORITHM_HASH);
}

index_option_list(A) ::= . {
    A = mylite_sql_parser_make_index_option_list(state);
}
index_option_list(A) ::= index_option_list(B) index_option(C). {
    A = mylite_sql_parser_append_index_option(state, B, C);
}

index_option(A) ::= index_type(B). {
    A = mylite_sql_parser_make_index_using_option(state, B);
}
index_option(A) ::= KEY_BLOCK_SIZE(T) INTEGER(V). {
    A = mylite_sql_parser_make_index_key_block_size_option(
        state, (struct mylite_sql_parser_index_key_block_size_tokens){
            .key_block_size = T,
            .integer = V,
        });
}
index_option(A) ::= KEY_BLOCK_SIZE(T) EQ INTEGER(V). {
    A = mylite_sql_parser_make_index_key_block_size_option(
        state, (struct mylite_sql_parser_index_key_block_size_tokens){
            .key_block_size = T,
            .integer = V,
        });
}
index_option(A) ::= COMMENT(T) STRING(S). {
    A = mylite_sql_parser_make_index_comment_option(
        state, (struct mylite_sql_parser_index_string_option_tokens){
            .option = T,
            .string = S,
        });
}
index_option(A) ::= VISIBLE(T). {
    A = mylite_sql_parser_make_index_visibility_option(
        state, T, MYLITE_SQL_AST_INDEX_OPTION_VISIBLE);
}
index_option(A) ::= INVISIBLE(T). {
    A = mylite_sql_parser_make_index_visibility_option(
        state, T, MYLITE_SQL_AST_INDEX_OPTION_INVISIBLE);
}
index_option(A) ::= ENGINE_ATTRIBUTE(T) STRING(S). {
    A = mylite_sql_parser_make_index_attribute_option(
        state, (struct mylite_sql_parser_index_string_option_tokens){
            .option = T,
            .string = S,
        }, MYLITE_SQL_AST_INDEX_OPTION_ENGINE_ATTRIBUTE);
}
index_option(A) ::= ENGINE_ATTRIBUTE(T) EQ STRING(S). {
    A = mylite_sql_parser_make_index_attribute_option(
        state, (struct mylite_sql_parser_index_string_option_tokens){
            .option = T,
            .string = S,
        }, MYLITE_SQL_AST_INDEX_OPTION_ENGINE_ATTRIBUTE);
}
index_option(A) ::= SECONDARY_ENGINE_ATTRIBUTE(T) STRING(S). {
    A = mylite_sql_parser_make_index_attribute_option(
        state, (struct mylite_sql_parser_index_string_option_tokens){
            .option = T,
            .string = S,
        }, MYLITE_SQL_AST_INDEX_OPTION_SECONDARY_ENGINE_ATTRIBUTE);
}
index_option(A) ::= SECONDARY_ENGINE_ATTRIBUTE(T) EQ STRING(S). {
    A = mylite_sql_parser_make_index_attribute_option(
        state, (struct mylite_sql_parser_index_string_option_tokens){
            .option = T,
            .string = S,
        }, MYLITE_SQL_AST_INDEX_OPTION_SECONDARY_ENGINE_ATTRIBUTE);
}

fulltext_index_option_list(A) ::= . {
    A = mylite_sql_parser_make_index_option_list(state);
}
fulltext_index_option_list(A) ::= fulltext_index_option_list(B) index_option(C). {
    A = mylite_sql_parser_append_index_option(state, B, C);
}
fulltext_index_option_list(A) ::= fulltext_index_option_list(B) fulltext_index_option(C). {
    A = mylite_sql_parser_append_index_option(state, B, C);
}

fulltext_index_option(A) ::= WITH(T) PARSER identifier(B). {
    A = mylite_sql_parser_make_index_with_parser_option(state, T, B);
}

ddl_table_option_list(A) ::= . {
    A = mylite_sql_parser_make_ddl_table_option_list(state);
}
ddl_table_option_list(A) ::= ddl_table_option_list(B) ddl_table_option(C). {
    A = mylite_sql_parser_append_ddl_table_option(state, B, C);
}

ddl_table_option(A) ::= ALGORITHM(T) opt_equal ddl_algorithm(V). {
    A = mylite_sql_parser_make_ddl_table_option(
        state, (struct mylite_sql_parser_ddl_table_option_tokens){.option = T, .value = V},
        MYLITE_SQL_AST_DDL_TABLE_OPTION_ALGORITHM);
}
ddl_table_option(A) ::= LOCK(T) opt_equal ddl_lock(V). {
    A = mylite_sql_parser_make_ddl_table_option(
        state, (struct mylite_sql_parser_ddl_table_option_tokens){.option = T, .value = V},
        MYLITE_SQL_AST_DDL_TABLE_OPTION_LOCK);
}

ddl_algorithm(A) ::= DEFAULT(T). {
    A = T;
}
ddl_algorithm(A) ::= INSTANT(T). {
    A = T;
}
ddl_algorithm(A) ::= INPLACE(T). {
    A = T;
}
ddl_algorithm(A) ::= COPY(T). {
    A = T;
}

ddl_lock(A) ::= DEFAULT(T). {
    A = T;
}
ddl_lock(A) ::= NONE(T). {
    A = T;
}
ddl_lock(A) ::= SHARED(T). {
    A = T;
}
ddl_lock(A) ::= EXCLUSIVE(T). {
    A = T;
}

table_option_list(A) ::= . {
    A = mylite_sql_parser_make_table_option_list(state);
}
table_option_list(A) ::= table_option_list(B) table_option(C). {
    A = mylite_sql_parser_append_table_option(state, B, C);
}

table_option(A) ::= ENGINE(T) opt_equal identifier(B). {
    A = mylite_sql_parser_make_table_option(
        state, T, MYLITE_SQL_AST_TABLE_OPTION_ENGINE, B);
}
table_option(A) ::= opt_default CHARACTER(T) SET opt_equal table_option_value(B). {
    A = mylite_sql_parser_make_table_option(
        state, T, MYLITE_SQL_AST_TABLE_OPTION_CHARACTER_SET, B);
}
table_option(A) ::= opt_default CHARSET(T) opt_equal table_option_value(B). {
    A = mylite_sql_parser_make_table_option(
        state, T, MYLITE_SQL_AST_TABLE_OPTION_CHARACTER_SET, B);
}
table_option(A) ::= opt_default COLLATE(T) opt_equal table_option_value(B). {
    A = mylite_sql_parser_make_table_option(
        state, T, MYLITE_SQL_AST_TABLE_OPTION_COLLATE, B);
}
table_option(A) ::= COMMENT(T) opt_equal STRING(S). {
    A = mylite_sql_parser_make_table_comment_option(
        state, (struct mylite_sql_parser_table_string_option_tokens){
            .option = T,
            .string = S,
        });
}
table_option(A) ::= AUTO_INCREMENT(T) opt_equal INTEGER(V). {
    A = mylite_sql_parser_make_table_auto_increment_option(
        state, (struct mylite_sql_parser_table_integer_option_tokens){
            .option = T,
            .integer = V,
        });
}

table_option_value(A) ::= identifier(B). {
    A = B;
}
table_option_value(A) ::= STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
table_option_value(A) ::= BINARY(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
table_option_value(A) ::= DEFAULT(T). {
    A = mylite_sql_parser_make_default(state, T);
}

column_default_value(A) ::= literal(B). {
    A = B;
}
column_default_value(A) ::= PLUS(T) numeric_literal(B). {
    A = mylite_sql_parser_make_unary_expression(
        state, T, MYLITE_SQL_AST_OPERATOR_POSITIVE, B);
}
column_default_value(A) ::= MINUS(T) numeric_literal(B). [UMINUS] {
    A = mylite_sql_parser_make_unary_expression(
        state, T, MYLITE_SQL_AST_OPERATOR_NEGATIVE, B);
}
column_default_value(A) ::= current_timestamp_value(B). {
    A = B;
}
column_default_value(A) ::= LPAREN(L) expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_parenthesized_column_default_expression(state, L, B, R);
}

current_timestamp_value(A) ::= CURRENT_TIMESTAMP(T). {
    A = mylite_sql_parser_make_current_timestamp(state, T, NULL);
}
current_timestamp_value(A) ::= CURRENT_TIMESTAMP(T) LPAREN(L) RPAREN(R). {
    A = mylite_sql_parser_make_current_timestamp_empty_parens(state, T, L, R);
}
current_timestamp_value(A) ::= CURRENT_TIMESTAMP(T) column_precision(B). {
    A = mylite_sql_parser_make_current_timestamp(state, T, B);
}

opt_column_length(A) ::= . {
    A = NULL;
}
opt_column_length(A) ::= column_length(B). {
    A = B;
}
column_length(A) ::= LPAREN(L) INTEGER(T) RPAREN(R). {
    A = mylite_sql_parser_make_column_length(
        state, (struct mylite_sql_parser_column_length_tokens){
            .left_paren = L,
            .integer = T,
            .right_paren = R,
        });
}

column_precision(A) ::= LPAREN(L) INTEGER(T) RPAREN(R). {
    A = mylite_sql_parser_make_column_precision(
        state, (struct mylite_sql_parser_precision_tokens){
            .left_paren = L,
            .precision = T,
            .right_paren = R,
        });
}
column_precision_scale(A) ::= LPAREN(L) INTEGER(P) COMMA INTEGER(S) RPAREN(R). {
    A = mylite_sql_parser_make_column_precision_scale(
        state, (struct mylite_sql_parser_precision_scale_tokens){
            .left_paren = L,
            .precision = P,
            .scale = S,
            .right_paren = R,
        });
}

opt_if_not_exists(A) ::= . {
    A = NULL;
}
opt_if_not_exists(A) ::= IF(I) NOT EXISTS(E). {
    A = mylite_sql_parser_make_if_not_exists(state, I, E);
}

opt_if_exists(A) ::= . {
    A = NULL;
}
opt_if_exists(A) ::= IF(I) EXISTS(E). {
    A = mylite_sql_parser_make_if_exists(state, I, E);
}

schema_create_option_list(A) ::= . {
    A = mylite_sql_parser_make_schema_option_list(state);
}
schema_create_option_list(A) ::= schema_create_option_list(B) schema_create_option(C). {
    A = mylite_sql_parser_append_schema_option(state, B, C);
}

schema_alter_option_list(A) ::= schema_alter_option(B). {
    A = mylite_sql_parser_append_schema_option(
        state, mylite_sql_parser_make_schema_option_list(state), B);
}
schema_alter_option_list(A) ::= schema_alter_option_list(B) schema_alter_option(C). {
    A = mylite_sql_parser_append_schema_option(state, B, C);
}

schema_create_option(A) ::= schema_common_option(B). {
    A = B;
}

schema_alter_option(A) ::= schema_common_option(B). {
    A = B;
}
schema_alter_option(A) ::= READ(R) ONLY opt_equal read_only_value(B). {
    A = mylite_sql_parser_make_schema_option(
        state, R, MYLITE_SQL_AST_SCHEMA_OPTION_READ_ONLY, B);
}

schema_common_option(A) ::= opt_default CHARACTER(T) SET opt_equal schema_option_value(B). {
    A = mylite_sql_parser_make_schema_option(
        state, T, MYLITE_SQL_AST_SCHEMA_OPTION_CHARACTER_SET, B);
}
schema_common_option(A) ::= opt_default CHARSET(T) opt_equal schema_option_value(B). {
    A = mylite_sql_parser_make_schema_option(
        state, T, MYLITE_SQL_AST_SCHEMA_OPTION_CHARACTER_SET, B);
}
schema_common_option(A) ::= opt_default COLLATE(T) opt_equal schema_option_value(B). {
    A = mylite_sql_parser_make_schema_option(
        state, T, MYLITE_SQL_AST_SCHEMA_OPTION_COLLATE, B);
}
schema_common_option(A) ::= opt_default ENCRYPTION(T) opt_equal STRING(B). {
    A = mylite_sql_parser_make_schema_option(
        state, T, MYLITE_SQL_AST_SCHEMA_OPTION_ENCRYPTION,
        mylite_sql_parser_make_literal(state, B, MYLITE_SQL_AST_LITERAL_STRING));
}

schema_option_value(A) ::= identifier(B). {
    A = B;
}
schema_option_value(A) ::= STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
schema_option_value(A) ::= BINARY(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}

charset_value(A) ::= identifier(B). {
    A = B;
}
charset_value(A) ::= STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}
charset_value(A) ::= BINARY(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}

read_only_value(A) ::= DEFAULT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
read_only_value(A) ::= INTEGER(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER);
}

opt_default ::= .
opt_default ::= DEFAULT.

opt_equal ::= .
opt_equal ::= EQ.

select_statement(A) ::= SELECT(T) select_modifiers(M) select_item_list(B) opt_window_clause(W). {
    A = mylite_sql_parser_make_select_statement(
        state, T, M, B, NULL, NULL, NULL, NULL, W, NULL, NULL);
}
select_statement(A) ::= SELECT(T) select_modifiers(M) select_item_list(B) FROM(F) DUAL(D)
        opt_window_clause(W). {
    A = mylite_sql_parser_make_select_statement(
        state, T, M, B, mylite_sql_parser_make_from_dual(state, F, D), NULL, NULL, NULL, W, NULL,
        NULL);
}
select_statement(A) ::= SELECT(T) select_modifiers(M) select_item_list(B) FROM(F)
        table_references(C)
        opt_where_clause(E) opt_group_by_clause(G) opt_having_clause(H) opt_window_clause(W)
        opt_order_by_clause(I) opt_limit_clause(J). {
    A = mylite_sql_parser_make_select_statement(
        state, T, M, B, mylite_sql_parser_make_from_table_references(state, F, C), E, G, H, W, I,
        J);
}
select_statement(A) ::= SELECT(T) select_modifiers(M) STAR(S) opt_window_clause(W). {
    A = mylite_sql_parser_make_select_statement(
        state, T, M, mylite_sql_parser_make_wildcard_select_list(state, S), NULL, NULL, NULL, NULL,
        W, NULL, NULL);
}
select_statement(A) ::= SELECT(T) select_modifiers(M) STAR(S) FROM(F) DUAL(D)
        opt_window_clause(W). {
    A = mylite_sql_parser_make_select_statement(
        state, T, M, mylite_sql_parser_make_wildcard_select_list(state, S),
        mylite_sql_parser_make_from_dual(state, F, D), NULL, NULL, NULL, W, NULL, NULL);
}
select_statement(A) ::= SELECT(T) select_modifiers(M) STAR(S) FROM(F) table_references(C)
        opt_where_clause(E) opt_group_by_clause(G) opt_having_clause(H) opt_window_clause(W)
        opt_order_by_clause(I) opt_limit_clause(J). {
    A = mylite_sql_parser_make_select_statement(
        state, T, M, mylite_sql_parser_make_wildcard_select_list(state, S),
        mylite_sql_parser_make_from_table_references(state, F, C), E, G, H, W, I, J);
}

union_query_expression(A) ::= query_expression_with_set_op(B) opt_order_by_clause(C) opt_limit_clause(D). {
    A = mylite_sql_parser_make_query_expression(state, B, C, D);
}

parenthesized_query_expression(A) ::= parenthesized_query_primary(B) opt_order_by_clause(C) opt_limit_clause(D). {
    A = mylite_sql_parser_make_query_expression(state, B, C, D);
}

query_expression_body(A) ::= query_term(B). {
    A = B;
}
query_expression_body(A) ::= query_expression_body(B) union_or_except_operator(C) query_term(D). {
    A = mylite_sql_parser_make_union_expression(state, B, C, D);
}

query_expression_with_set_op(A) ::= query_term_with_set_op(B). {
    A = B;
}
query_expression_with_set_op(A) ::= query_term(B) union_or_except_operator(C) query_term(D). {
    A = mylite_sql_parser_make_union_expression(state, B, C, D);
}
query_expression_with_set_op(A) ::= query_expression_with_set_op(B) union_or_except_operator(C) query_term(D). {
    A = mylite_sql_parser_make_union_expression(state, B, C, D);
}

query_term(A) ::= query_primary(B). [LOWEST] {
    A = B;
}
query_term(A) ::= query_term(B) intersect_operator(C) query_primary(D). {
    A = mylite_sql_parser_make_union_expression(state, B, C, D);
}

query_term_with_set_op(A) ::= query_primary(B) intersect_operator(C) query_primary(D). {
    A = mylite_sql_parser_make_union_expression(state, B, C, D);
}
query_term_with_set_op(A) ::= query_term_with_set_op(B) intersect_operator(C) query_primary(D). {
    A = mylite_sql_parser_make_union_expression(state, B, C, D);
}

query_primary(A) ::= select_union_operand(B). {
    A = B;
}
query_primary(A) ::= table_query_operand(B). {
    A = B;
}
query_primary(A) ::= values_query_operand(B). {
    A = B;
}
query_primary(A) ::= parenthesized_query_primary(B). {
    A = B;
}

parenthesized_query_primary(A) ::= LPAREN(L) query_expression_body(B) opt_order_by_clause(C) opt_limit_clause(D) RPAREN(R). {
    A = mylite_sql_parser_make_query_primary(
        state, L, mylite_sql_parser_make_query_expression(state, B, C, D), R);
}

union_or_except_operator(A) ::= UNION(T). {
    A = mylite_sql_parser_make_default_set_operator(T, MYLITE_SQL_AST_SET_OPERATION_UNION);
}
union_or_except_operator(A) ::= UNION(T) ALL(M). {
    A = mylite_sql_parser_make_all_set_operator(T, M, MYLITE_SQL_AST_SET_OPERATION_UNION);
}
union_or_except_operator(A) ::= UNION(T) DISTINCT(M). {
    A = mylite_sql_parser_make_distinct_set_operator(T, M, MYLITE_SQL_AST_SET_OPERATION_UNION);
}
union_or_except_operator(A) ::= EXCEPT(T). {
    A = mylite_sql_parser_make_default_set_operator(T, MYLITE_SQL_AST_SET_OPERATION_EXCEPT);
}
union_or_except_operator(A) ::= EXCEPT(T) ALL(M). {
    A = mylite_sql_parser_make_all_set_operator(T, M, MYLITE_SQL_AST_SET_OPERATION_EXCEPT);
}
union_or_except_operator(A) ::= EXCEPT(T) DISTINCT(M). {
    A = mylite_sql_parser_make_distinct_set_operator(T, M, MYLITE_SQL_AST_SET_OPERATION_EXCEPT);
}

intersect_operator(A) ::= INTERSECT(T). {
    A = mylite_sql_parser_make_default_set_operator(T, MYLITE_SQL_AST_SET_OPERATION_INTERSECT);
}
intersect_operator(A) ::= INTERSECT(T) ALL(M). {
    A = mylite_sql_parser_make_all_set_operator(T, M, MYLITE_SQL_AST_SET_OPERATION_INTERSECT);
}
intersect_operator(A) ::= INTERSECT(T) DISTINCT(M). {
    A = mylite_sql_parser_make_distinct_set_operator(T, M, MYLITE_SQL_AST_SET_OPERATION_INTERSECT);
}

table_query_operand(A) ::= TABLE(T) table_name(B). {
    A = mylite_sql_parser_make_table_query_statement(state, T, B, NULL, NULL);
}

values_query_statement(A) ::= VALUES(T) values_query_row_list(B) opt_order_by_clause(C) opt_limit_clause(D). {
    A = mylite_sql_parser_make_values_statement(state, T, B, C, D);
}

values_query_operand(A) ::= VALUES(T) values_query_row_list(B). {
    A = mylite_sql_parser_make_values_statement(state, T, B, NULL, NULL);
}

values_query_row_list(A) ::= values_query_row(B). {
    A = mylite_sql_parser_make_insert_row_list(state, B);
}
values_query_row_list(A) ::= values_query_row_list(B) COMMA values_query_row(C). {
    A = mylite_sql_parser_append_insert_row(state, B, C);
}

values_query_row(A) ::= ROW(T) LPAREN expression_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_insert_row(state, T, B, R);
}

select_union_operand(A) ::= SELECT(T) select_modifiers(M) select_item_list(B) opt_window_clause(W). {
    A = mylite_sql_parser_make_select_statement(
        state, T, M, B, NULL, NULL, NULL, NULL, W, NULL, NULL);
}
select_union_operand(A) ::= SELECT(T) select_modifiers(M) select_item_list(B)
        FROM(F) DUAL(D) opt_window_clause(W). {
    A = mylite_sql_parser_make_select_statement(
        state, T, M, B, mylite_sql_parser_make_from_dual(state, F, D), NULL, NULL, NULL, W, NULL,
        NULL);
}
select_union_operand(A) ::= SELECT(T) select_modifiers(M) select_item_list(B)
        FROM(F) table_references(C) opt_where_clause(E) opt_group_by_clause(G)
        opt_having_clause(H) opt_window_clause(W). {
    A = mylite_sql_parser_make_select_statement(
        state, T, M, B, mylite_sql_parser_make_from_table_references(state, F, C), E, G, H, W,
        NULL, NULL);
}
select_union_operand(A) ::= SELECT(T) select_modifiers(M) STAR(S) opt_window_clause(W). {
    A = mylite_sql_parser_make_select_statement(
        state, T, M, mylite_sql_parser_make_wildcard_select_list(state, S), NULL, NULL, NULL, NULL,
        W, NULL, NULL);
}
select_union_operand(A) ::= SELECT(T) select_modifiers(M) STAR(S) FROM(F) DUAL(D)
        opt_window_clause(W). {
    A = mylite_sql_parser_make_select_statement(
        state, T, M, mylite_sql_parser_make_wildcard_select_list(state, S),
        mylite_sql_parser_make_from_dual(state, F, D), NULL, NULL, NULL, W, NULL, NULL);
}
select_union_operand(A) ::= SELECT(T) select_modifiers(M) STAR(S) FROM(F)
        table_references(C) opt_where_clause(E) opt_group_by_clause(G) opt_having_clause(H)
        opt_window_clause(W). {
    A = mylite_sql_parser_make_select_statement(
        state, T, M, mylite_sql_parser_make_wildcard_select_list(state, S),
        mylite_sql_parser_make_from_table_references(state, F, C), E, G, H, W, NULL, NULL);
}

select_modifiers(A) ::= select_duplicate_mode(B). {
    A = B;
}

select_duplicate_mode(A) ::= . {
    A = mylite_sql_parser_make_implicit_select_duplicate_mode();
}
select_duplicate_mode(A) ::= select_duplicate_mode_list(B). {
    A = B;
}

select_duplicate_mode_list(A) ::= select_duplicate_mode_item(B). {
    A = B;
}
select_duplicate_mode_list(A) ::= select_duplicate_mode_list(B) select_duplicate_mode_item(C). {
    A = mylite_sql_parser_append_select_duplicate_mode(state, B, C);
}

select_duplicate_mode_item(A) ::= ALL(T). {
    A = mylite_sql_parser_make_all_select_duplicate_mode(T);
}
select_duplicate_mode_item(A) ::= DISTINCT(T). {
    A = mylite_sql_parser_make_distinct_select_duplicate_mode(T);
}
select_duplicate_mode_item(A) ::= DISTINCTROW(T). {
    A = mylite_sql_parser_make_distinct_select_duplicate_mode(T);
}
select_duplicate_mode_item(A) ::= SQL_CALC_FOUND_ROWS(T). {
    A = mylite_sql_parser_make_sql_calc_found_rows_select_duplicate_mode(T);
}

opt_where_clause(A) ::= . {
    A = NULL;
}
opt_where_clause(A) ::= where_clause(B). {
    A = B;
}

where_clause(A) ::= WHERE(T) expression(B). {
    A = mylite_sql_parser_make_where_clause(state, T, B);
}

opt_group_by_clause(A) ::= . {
    A = NULL;
}
opt_group_by_clause(A) ::= group_by_clause(B). {
    A = B;
}

group_by_clause(A) ::= GROUP(G) BY(B) group_item_list(C). {
    A = mylite_sql_parser_make_group_by_clause(state, G, B, C);
}

group_item_list(A) ::= group_item(B). {
    A = mylite_sql_parser_make_group_item_list(state, B);
}
group_item_list(A) ::= group_item_list(B) COMMA group_item(C). {
    A = mylite_sql_parser_append_group_item(state, B, C);
}

group_item(A) ::= expression(B) opt_order_direction(C). {
    A = mylite_sql_parser_make_group_item(state, B, C);
}

opt_having_clause(A) ::= . {
    A = NULL;
}
opt_having_clause(A) ::= having_clause(B). {
    A = B;
}

having_clause(A) ::= HAVING(T) expression(B). {
    A = mylite_sql_parser_make_having_clause(state, T, B);
}

opt_window_clause(A) ::= . {
    A = NULL;
}
opt_window_clause(A) ::= window_clause(B). {
    A = B;
}

window_clause(A) ::= WINDOW(T) window_definition_list(B). {
    A = mylite_sql_parser_make_window_clause(state, T, B);
}

window_definition_list(A) ::= window_definition(B). {
    A = mylite_sql_parser_make_window_definition_list(state, B);
}
window_definition_list(A) ::= window_definition_list(B) COMMA window_definition(C). {
    A = mylite_sql_parser_append_window_definition(state, B, C);
}

window_definition(A) ::= identifier(B) AS(T) LPAREN window_specification(C) RPAREN(R). {
    A = mylite_sql_parser_make_window_definition(state, B, T, C, R);
}

over_clause(A) ::= OVER(T) identifier(B). {
    A = mylite_sql_parser_make_over_clause_with_name(state, T, B);
}
over_clause(A) ::= OVER(T) LPAREN window_specification(B) RPAREN(R). {
    A = mylite_sql_parser_make_over_clause_with_specification(state, T, B, R);
}

window_specification(A) ::= opt_window_name(B) opt_window_partition_clause(C)
        opt_window_order_clause(D) opt_window_frame_clause(E). {
    A = mylite_sql_parser_make_window_specification(state, B, C, D, E);
}

opt_window_name(A) ::= . {
    A = NULL;
}
opt_window_name(A) ::= identifier(B). {
    A = B;
}

opt_window_partition_clause(A) ::= . {
    A = NULL;
}
opt_window_partition_clause(A) ::= PARTITION(P) BY(B) expression_list(C). {
    A = mylite_sql_parser_make_window_partition_clause(state, P, B, C);
}

opt_window_order_clause(A) ::= . {
    A = NULL;
}
opt_window_order_clause(A) ::= order_by_clause(B). {
    A = B;
}

opt_window_frame_clause(A) ::= . {
    A = NULL;
}
opt_window_frame_clause(A) ::= window_frame_clause(B). {
    A = B;
}

window_frame_clause(A) ::= window_frame_unit(U) window_frame_bound(B). {
    A = mylite_sql_parser_make_window_frame_clause(state, U, B, NULL);
}
window_frame_clause(A) ::= window_frame_unit(U) BETWEEN window_frame_bound(B) AND window_frame_bound(C). {
    A = mylite_sql_parser_make_window_frame_clause(state, U, B, C);
}

window_frame_unit(A) ::= ROWS(T). {
    A = mylite_sql_parser_make_window_frame_unit_token(
        T, MYLITE_SQL_AST_WINDOW_FRAME_UNIT_ROWS);
}
window_frame_unit(A) ::= RANGE(T). {
    A = mylite_sql_parser_make_window_frame_unit_token(
        T, MYLITE_SQL_AST_WINDOW_FRAME_UNIT_RANGE);
}

window_frame_bound(A) ::= CURRENT(T) ROW(R). {
    A = mylite_sql_parser_make_window_frame_bound(
        state, T, MYLITE_SQL_AST_WINDOW_FRAME_BOUND_CURRENT_ROW, NULL, R);
}
window_frame_bound(A) ::= UNBOUNDED(T) PRECEDING(P). {
    A = mylite_sql_parser_make_window_frame_bound(
        state, T, MYLITE_SQL_AST_WINDOW_FRAME_BOUND_UNBOUNDED_PRECEDING, NULL, P);
}
window_frame_bound(A) ::= UNBOUNDED(T) FOLLOWING(F). {
    A = mylite_sql_parser_make_window_frame_bound(
        state, T, MYLITE_SQL_AST_WINDOW_FRAME_BOUND_UNBOUNDED_FOLLOWING, NULL, F);
}
window_frame_bound(A) ::= expression(B) PRECEDING(P). {
    A = mylite_sql_parser_make_window_frame_bound(
        state, (struct mylite_sql_token){0}, MYLITE_SQL_AST_WINDOW_FRAME_BOUND_EXPRESSION_PRECEDING,
        B, P);
}
window_frame_bound(A) ::= expression(B) FOLLOWING(F). {
    A = mylite_sql_parser_make_window_frame_bound(
        state, (struct mylite_sql_token){0}, MYLITE_SQL_AST_WINDOW_FRAME_BOUND_EXPRESSION_FOLLOWING,
        B, F);
}

opt_order_by_clause(A) ::= . {
    A = NULL;
}
opt_order_by_clause(A) ::= order_by_clause(B). {
    A = B;
}

order_by_clause(A) ::= ORDER(O) BY(B) order_item_list(C). {
    A = mylite_sql_parser_make_order_by_clause(state, O, B, C);
}

order_item_list(A) ::= order_item(B). {
    A = mylite_sql_parser_make_order_item_list(state, B);
}
order_item_list(A) ::= order_item_list(B) COMMA order_item(C). {
    A = mylite_sql_parser_append_order_item(state, B, C);
}

order_item(A) ::= expression(B) opt_order_direction(C). {
    A = mylite_sql_parser_make_order_item(state, B, C);
}

opt_order_direction(A) ::= . {
    A = (struct mylite_sql_token){0};
}
opt_order_direction(A) ::= ASC(T). {
    A = T;
}
opt_order_direction(A) ::= DESC(T). {
    A = T;
}

opt_limit_clause(A) ::= . {
    A = NULL;
}
opt_limit_clause(A) ::= limit_clause(B). {
    A = B;
}

limit_clause(A) ::= LIMIT(T) limit_bound(B). {
    A = mylite_sql_parser_make_limit_clause(state, T, NULL, B);
}
limit_clause(A) ::= LIMIT(T) limit_bound(B) COMMA limit_bound(C). {
    A = mylite_sql_parser_make_limit_clause(state, T, B, C);
}
limit_clause(A) ::= LIMIT(T) limit_bound(B) OFFSET limit_bound(C). {
    A = mylite_sql_parser_make_limit_clause(state, T, C, B);
}

limit_bound(A) ::= INTEGER(T). {
    A = mylite_sql_parser_make_limit_bound(state, T);
}

table_references(A) ::= table_reference_list(B). {
    A = B;
}

table_reference_list(A) ::= joined_table_reference(B). {
    A = mylite_sql_parser_make_table_reference_list(state, B);
}
table_reference_list(A) ::= table_reference_list(B) COMMA joined_table_reference(C). {
    A = mylite_sql_parser_append_table_reference(state, B, C);
}

joined_table_reference(A) ::= table_factor(B). {
    A = B;
}
joined_table_reference(A) ::= joined_table_reference(B) inner_join_operator(C) table_factor(D)
        opt_inner_join_condition(E). {
    A = mylite_sql_parser_make_join_expression(state, B, C, D, E);
}
joined_table_reference(A) ::= joined_table_reference(B) outer_join_operator(C) table_factor(D)
        outer_join_condition(E). {
    A = mylite_sql_parser_make_join_expression(state, B, C, D, E);
}

inner_join_operator(A) ::= JOIN(T). {
    A = mylite_sql_parser_make_join_operator(state, T, MYLITE_SQL_AST_JOIN_INNER);
}
inner_join_operator(A) ::= INNER(T) JOIN. {
    A = mylite_sql_parser_make_join_operator(state, T, MYLITE_SQL_AST_JOIN_INNER);
}
inner_join_operator(A) ::= CROSS(T) JOIN. {
    A = mylite_sql_parser_make_join_operator(state, T, MYLITE_SQL_AST_JOIN_CROSS);
}

outer_join_operator(A) ::= LEFT(T) JOIN. {
    A = mylite_sql_parser_make_join_operator(state, T, MYLITE_SQL_AST_JOIN_LEFT);
}
outer_join_operator(A) ::= LEFT(T) OUTER JOIN. {
    A = mylite_sql_parser_make_join_operator(state, T, MYLITE_SQL_AST_JOIN_LEFT);
}
outer_join_operator(A) ::= RIGHT(T) JOIN. {
    A = mylite_sql_parser_make_join_operator(state, T, MYLITE_SQL_AST_JOIN_RIGHT);
}
outer_join_operator(A) ::= RIGHT(T) OUTER JOIN. {
    A = mylite_sql_parser_make_join_operator(state, T, MYLITE_SQL_AST_JOIN_RIGHT);
}

opt_inner_join_condition(A) ::= . {
    A = NULL;
}
opt_inner_join_condition(A) ::= ON(T) expression(B). {
    A = mylite_sql_parser_make_join_on_condition(state, T, B);
}
opt_inner_join_condition(A) ::= USING(T) LPAREN using_column_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_join_using_condition(state, T, B, R);
}

outer_join_condition(A) ::= ON(T) expression(B). {
    A = mylite_sql_parser_make_join_on_condition(state, T, B);
}
outer_join_condition(A) ::= USING(T) LPAREN using_column_list(B) RPAREN(R). {
    A = mylite_sql_parser_make_join_using_condition(state, T, B, R);
}

using_column_list(A) ::= identifier(B). {
    A = mylite_sql_parser_make_using_column_list(state, B);
}
using_column_list(A) ::= using_column_list(B) COMMA identifier(C). {
    A = mylite_sql_parser_append_using_column(
        state, (struct mylite_sql_parser_using_column_append){
            .list = B,
            .column = C,
        });
}

table_factor(A) ::= table_name(B) opt_table_alias(C) opt_index_hint_list. {
    A = mylite_sql_parser_make_table_factor(state, B, C);
}

opt_index_hint_list(A) ::= . {
    A = (struct mylite_sql_token){0};
}
opt_index_hint_list(A) ::= index_hint_list(B). {
    A = B;
}

index_hint_list(A) ::= index_hint(B). {
    A = B;
}
index_hint_list(A) ::= index_hint_list(B) index_hint. {
    A = B;
}

index_hint(A) ::= USE(T) index_hint_index_or_key opt_index_hint_scope
        LPAREN opt_index_hint_name_list RPAREN. {
    A = T;
}
index_hint(A) ::= IGNORE(T) index_hint_index_or_key opt_index_hint_scope
        LPAREN opt_index_hint_name_list RPAREN. {
    A = T;
}
index_hint(A) ::= FORCE(T) index_hint_index_or_key opt_index_hint_scope
        LPAREN opt_index_hint_name_list RPAREN. {
    A = T;
}

index_hint_index_or_key(A) ::= INDEX(T). {
    A = T;
}
index_hint_index_or_key(A) ::= KEY(T). {
    A = T;
}

opt_index_hint_scope(A) ::= . {
    A = (struct mylite_sql_token){0};
}
opt_index_hint_scope(A) ::= FOR(T) JOIN. {
    A = T;
}
opt_index_hint_scope(A) ::= FOR(T) ORDER BY. {
    A = T;
}
opt_index_hint_scope(A) ::= FOR(T) GROUP BY. {
    A = T;
}

opt_index_hint_name_list(A) ::= . {
    A = NULL;
}
opt_index_hint_name_list(A) ::= index_hint_name_list(B). {
    A = B;
}

index_hint_name_list(A) ::= index_hint_name(B). {
    A = B;
}
index_hint_name_list(A) ::= index_hint_name_list(B) COMMA index_hint_name. {
    A = B;
}

index_hint_name(A) ::= identifier(B). {
    A = B;
}
index_hint_name(A) ::= PRIMARY(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}

table_name(A) ::= qualified_identifier(B). {
    A = B;
}

opt_table_alias(A) ::= . {
    A = NULL;
}
opt_table_alias(A) ::= table_alias(B). {
    A = B;
}
opt_table_alias(A) ::= AS table_alias(B). {
    A = B;
}

table_alias(A) ::= identifier(B). {
    A = B;
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
select_item(A) ::= expression(B) AS projection_alias(C). {
    A = mylite_sql_parser_make_aliased_select_item(state, B, C);
}
select_item(A) ::= expression(B) projection_alias(C). {
    A = mylite_sql_parser_make_aliased_select_item(state, B, C);
}
select_item(A) ::= qualified_wildcard(B). {
    A = mylite_sql_parser_make_select_item(state, B);
}

projection_alias(A) ::= identifier(B). {
    A = B;
}
projection_alias(A) ::= STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}

qualified_wildcard(A) ::= qualified_identifier(B) DOT STAR(T). {
    A = mylite_sql_parser_make_qualified_wildcard(state, B, NULL, T);
}

expression(A) ::= logical_or_expression(B). [LOWEST] {
    A = B;
}

logical_or_expression(A) ::= logical_xor_expression(B). [OR] {
    A = B;
}
logical_or_expression(A) ::= logical_or_expression(B) OR(T) logical_xor_expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_LOGICAL_OR, C);
}
logical_or_expression(A) ::= logical_or_expression(B) LOGICAL_OR(T) logical_xor_expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_LOGICAL_OR, C);
}

logical_xor_expression(A) ::= logical_and_expression(B). [XOR] {
    A = B;
}
logical_xor_expression(A) ::= logical_xor_expression(B) XOR(T) logical_and_expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR, C);
}

logical_and_expression(A) ::= logical_not_expression(B). [AND] {
    A = B;
}
logical_and_expression(A) ::= logical_and_expression(B) AND(T) logical_not_expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_LOGICAL_AND, C);
}
logical_and_expression(A) ::= logical_and_expression(B) LOGICAL_AND(T) logical_not_expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_LOGICAL_AND, C);
}

logical_not_expression(A) ::= between_expression(B). [NOT] {
    A = B;
}
logical_not_expression(A) ::= EXISTS(T) subquery(B). {
    A = mylite_sql_parser_make_exists_expression(state, T, B, false);
}
logical_not_expression(A) ::= NOT(T) logical_not_expression(B). {
    A = mylite_sql_parser_make_unary_expression(
        state, T, MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT, B);
}

between_expression(A) ::= comparison_expression(B). [NOT] {
    A = B;
}
between_expression(A) ::= comparison_expression(B) BETWEEN(T) comparison_expression(C) AND comparison_expression(D). {
    A = mylite_sql_parser_make_ternary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_BETWEEN, C, D);
}
between_expression(A) ::= comparison_expression(B) NOT(T) BETWEEN comparison_expression(C) AND comparison_expression(D). {
    A = mylite_sql_parser_make_ternary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_NOT_BETWEEN, C, D);
}

comparison_expression(A) ::= bit_or_expression(B). [LOWEST] {
    A = B;
}
comparison_expression(A) ::= comparison_expression(B) EQ(T) bit_or_expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_EQUAL, C);
}
comparison_expression(A) ::= comparison_expression(B) NULL_SAFE_EQ(T) bit_or_expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL, C);
}
comparison_expression(A) ::= comparison_expression(B) NE(T) bit_or_expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_NOT_EQUAL, C);
}
comparison_expression(A) ::= comparison_expression(B) LT(T) bit_or_expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_LESS, C);
}
comparison_expression(A) ::= comparison_expression(B) LE(T) bit_or_expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_LESS_EQUAL, C);
}
comparison_expression(A) ::= comparison_expression(B) GT(T) bit_or_expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_GREATER, C);
}
comparison_expression(A) ::= comparison_expression(B) GE(T) bit_or_expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL, C);
}
comparison_expression(A) ::= comparison_expression(B) IN(T) subquery(C). {
    A = mylite_sql_parser_make_in_subquery_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_IN, C);
}
comparison_expression(A) ::= comparison_expression(B) NOT(T) IN subquery(C). {
    A = mylite_sql_parser_make_in_subquery_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_NOT_IN, C);
}
comparison_expression(A) ::= comparison_expression(B) quantified_comparison_operator(C)
        subquery_quantifier(D) subquery(E). {
    A = mylite_sql_parser_make_quantified_comparison(state, B, C.token, C.operator_kind, D, E);
}
comparison_expression(A) ::= comparison_expression(B) IS(T) NULL. {
    A = mylite_sql_parser_make_unary_expression(
        state, T, MYLITE_SQL_AST_OPERATOR_IS_NULL, B);
}
comparison_expression(A) ::= comparison_expression(B) IS(T) NOT NULL. {
    A = mylite_sql_parser_make_unary_expression(
        state, T, MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL, B);
}
comparison_expression(A) ::= comparison_expression(B) IS(T) TRUE. {
    A = mylite_sql_parser_make_unary_expression(
        state, T, MYLITE_SQL_AST_OPERATOR_IS_TRUE, B);
}
comparison_expression(A) ::= comparison_expression(B) IS(T) NOT TRUE. {
    A = mylite_sql_parser_make_unary_expression(
        state, T, MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE, B);
}
comparison_expression(A) ::= comparison_expression(B) IS(T) FALSE. {
    A = mylite_sql_parser_make_unary_expression(
        state, T, MYLITE_SQL_AST_OPERATOR_IS_FALSE, B);
}
comparison_expression(A) ::= comparison_expression(B) IS(T) NOT FALSE. {
    A = mylite_sql_parser_make_unary_expression(
        state, T, MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE, B);
}
comparison_expression(A) ::= comparison_expression(B) IS(T) UNKNOWN. {
    A = mylite_sql_parser_make_unary_expression(
        state, T, MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN, B);
}
comparison_expression(A) ::= comparison_expression(B) IS(T) NOT UNKNOWN. {
    A = mylite_sql_parser_make_unary_expression(
        state, T, MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN, B);
}
comparison_expression(A) ::= comparison_expression(B) LIKE(T) bit_or_expression(C) opt_like_escape(D). {
    A = D == NULL ? mylite_sql_parser_make_binary_expression(
                        state, B, T, MYLITE_SQL_AST_OPERATOR_LIKE, C)
                  : mylite_sql_parser_make_ternary_expression(
                        state, B, T, MYLITE_SQL_AST_OPERATOR_LIKE, C, D);
}
comparison_expression(A) ::= comparison_expression(B) NOT(T) LIKE bit_or_expression(C) opt_like_escape(D). {
    A = D == NULL ? mylite_sql_parser_make_binary_expression(
                        state, B, T, MYLITE_SQL_AST_OPERATOR_NOT_LIKE, C)
                  : mylite_sql_parser_make_ternary_expression(
                        state, B, T, MYLITE_SQL_AST_OPERATOR_NOT_LIKE, C, D);
}
comparison_expression(A) ::= comparison_expression(B) REGEXP(T) bit_or_expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_REGEXP, C);
}
comparison_expression(A) ::= comparison_expression(B) RLIKE(T) bit_or_expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_REGEXP, C);
}
comparison_expression(A) ::= comparison_expression(B) NOT(T) REGEXP bit_or_expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_NOT_REGEXP, C);
}
comparison_expression(A) ::= comparison_expression(B) NOT(T) RLIKE bit_or_expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_NOT_REGEXP, C);
}
comparison_expression(A) ::= comparison_expression(B) IN(T) LPAREN expression_list(C) RPAREN. {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_IN, C);
}
comparison_expression(A) ::= comparison_expression(B) NOT(T) IN LPAREN expression_list(C) RPAREN. {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_NOT_IN, C);
}

quantified_comparison_operator(A) ::= EQ(T). {
    A = (struct mylite_sql_parser_comparison_operator){
        .token = T,
        .operator_kind = MYLITE_SQL_AST_OPERATOR_EQUAL,
    };
}
quantified_comparison_operator(A) ::= NE(T). {
    A = (struct mylite_sql_parser_comparison_operator){
        .token = T,
        .operator_kind = MYLITE_SQL_AST_OPERATOR_NOT_EQUAL,
    };
}
quantified_comparison_operator(A) ::= LT(T). {
    A = (struct mylite_sql_parser_comparison_operator){
        .token = T,
        .operator_kind = MYLITE_SQL_AST_OPERATOR_LESS,
    };
}
quantified_comparison_operator(A) ::= LE(T). {
    A = (struct mylite_sql_parser_comparison_operator){
        .token = T,
        .operator_kind = MYLITE_SQL_AST_OPERATOR_LESS_EQUAL,
    };
}
quantified_comparison_operator(A) ::= GT(T). {
    A = (struct mylite_sql_parser_comparison_operator){
        .token = T,
        .operator_kind = MYLITE_SQL_AST_OPERATOR_GREATER,
    };
}
quantified_comparison_operator(A) ::= GE(T). {
    A = (struct mylite_sql_parser_comparison_operator){
        .token = T,
        .operator_kind = MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL,
    };
}

subquery_quantifier(A) ::= ANY. {
    A = MYLITE_SQL_AST_SUBQUERY_QUANTIFIER_ANY;
}
subquery_quantifier(A) ::= SOME. {
    A = MYLITE_SQL_AST_SUBQUERY_QUANTIFIER_SOME;
}
subquery_quantifier(A) ::= ALL. {
    A = MYLITE_SQL_AST_SUBQUERY_QUANTIFIER_ALL;
}

opt_like_escape(A) ::= . {
    A = NULL;
}
opt_like_escape(A) ::= ESCAPE bit_or_expression(B). {
    A = B;
}

expression_list(A) ::= expression(B). {
    A = mylite_sql_parser_make_expression_list(state, B);
}
expression_list(A) ::= expression_list(B) COMMA expression(C). {
    A = mylite_sql_parser_append_expression(state, B, C);
}

bit_or_expression(A) ::= bit_and_expression(B). {
    A = B;
}
bit_or_expression(A) ::= bit_or_expression(B) BIT_OR(T) bit_and_expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_BITWISE_OR, C);
}

bit_and_expression(A) ::= bit_shift_expression(B). {
    A = B;
}
bit_and_expression(A) ::= bit_and_expression(B) BIT_AND(T) bit_shift_expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_BITWISE_AND, C);
}

bit_shift_expression(A) ::= additive_expression(B). {
    A = B;
}
bit_shift_expression(A) ::= bit_shift_expression(B) SHIFT_LEFT(T) additive_expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_SHIFT_LEFT, C);
}
bit_shift_expression(A) ::= bit_shift_expression(B) SHIFT_RIGHT(T) additive_expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_SHIFT_RIGHT, C);
}

additive_expression(A) ::= multiplicative_expression(B). {
    A = B;
}
additive_expression(A) ::= additive_expression(B) PLUS(T) multiplicative_expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_ADD, C);
}
additive_expression(A) ::= additive_expression(B) MINUS(T) multiplicative_expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_SUBTRACT, C);
}

multiplicative_expression(A) ::= bit_xor_expression(B). {
    A = B;
}
multiplicative_expression(A) ::= multiplicative_expression(B) STAR(T) bit_xor_expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_MULTIPLY, C);
}
multiplicative_expression(A) ::= multiplicative_expression(B) SLASH(T) bit_xor_expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_DIVIDE, C);
}
multiplicative_expression(A) ::= multiplicative_expression(B) DIV(T) bit_xor_expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE, C);
}
multiplicative_expression(A) ::= multiplicative_expression(B) PERCENT(T) bit_xor_expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_MODULO, C);
}
multiplicative_expression(A) ::= multiplicative_expression(B) MOD(T) bit_xor_expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_MODULO, C);
}

bit_xor_expression(A) ::= unary_expression(B). {
    A = B;
}
bit_xor_expression(A) ::= bit_xor_expression(B) BIT_XOR(T) unary_expression(C). {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_BITWISE_XOR, C);
}

unary_expression(A) ::= primary_expression(B). {
    A = B;
}
unary_expression(A) ::= PLUS(T) unary_expression(B). [UPLUS] {
    A = mylite_sql_parser_make_unary_expression(
        state, T, MYLITE_SQL_AST_OPERATOR_POSITIVE, B);
}
unary_expression(A) ::= MINUS(T) unary_expression(B). [UMINUS] {
    A = mylite_sql_parser_make_unary_expression(
        state, T, MYLITE_SQL_AST_OPERATOR_NEGATIVE, B);
}
unary_expression(A) ::= BIT_NOT(T) unary_expression(B). {
    A = mylite_sql_parser_make_unary_expression(
        state, T, MYLITE_SQL_AST_OPERATOR_BITWISE_NOT, B);
}
unary_expression(A) ::= LOGICAL_NOT(T) unary_expression(B). {
    A = mylite_sql_parser_make_unary_expression(
        state, T, MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT, B);
}
unary_expression(A) ::= BINARY(T) unary_expression(B). {
    A = mylite_sql_parser_make_unary_expression(
        state, T, MYLITE_SQL_AST_OPERATOR_BINARY_CAST, B);
}

primary_expression(A) ::= literal(B). {
    A = B;
}
primary_expression(A) ::= cast_expression(B). {
    A = B;
}
primary_expression(A) ::= convert_expression(B). {
    A = B;
}
primary_expression(A) ::= case_expression(B). {
    A = B;
}
primary_expression(A) ::= window_function_call(B). {
    A = B;
}
primary_expression(A) ::= group_concat_call(B). {
    A = B;
}
primary_expression(A) ::= aggregate_distinct_call(B) over_clause(C). {
    A = mylite_sql_parser_make_aggregate_window_function_call(state, B, C);
}
primary_expression(A) ::= aggregate_distinct_call(B). {
    A = B;
}
primary_expression(A) ::= aggregate_star_call(B) over_clause(C). {
    A = mylite_sql_parser_make_aggregate_window_function_call(state, B, C);
}
primary_expression(A) ::= aggregate_star_call(B). {
    A = B;
}
primary_expression(A) ::= scalar_function_call(B) over_clause(C). {
    A = mylite_sql_parser_make_aggregate_window_function_call(state, B, C);
}
primary_expression(A) ::= scalar_function_call(B). {
    A = B;
}
primary_expression(A) ::= json_extract_expression(B). {
    A = B;
}
primary_expression(A) ::= SYSTEM_VARIABLE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
primary_expression(A) ::= USER_VARIABLE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
primary_expression(A) ::= qualified_identifier(B). {
    A = B;
}
primary_expression(A) ::= current_timestamp_value(B). {
    A = B;
}
primary_expression(A) ::= bare_temporal_function(B). {
    A = B;
}
primary_expression(A) ::= bare_current_user_function(B). {
    A = B;
}
primary_expression(A) ::= subquery(B). {
    A = mylite_sql_parser_make_scalar_subquery_expression(state, B);
}
primary_expression(A) ::= row_constructor(B). {
    A = B;
}
primary_expression(A) ::= LPAREN(L) expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_parenthesized_expression(state, L, B, R);
}

json_extract_expression(A) ::= qualified_identifier(B) JSON_EXTRACT(T) STRING(P). {
    struct mylite_sql_ast_node *path = mylite_sql_parser_make_literal(
        state, P, MYLITE_SQL_AST_LITERAL_STRING);
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_JSON_EXTRACT, path);
}
json_extract_expression(A) ::= qualified_identifier(B) JSON_UNQUOTE_EXTRACT(T) STRING(P). {
    struct mylite_sql_ast_node *path = mylite_sql_parser_make_literal(
        state, P, MYLITE_SQL_AST_LITERAL_STRING);
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_JSON_UNQUOTE_EXTRACT, path);
}

subquery(A) ::= LPAREN(L) select_statement(B) RPAREN(R). {
    A = (struct mylite_sql_parser_subquery){
        .left_paren = L,
        .select_statement = B,
        .right_paren = R,
    };
}

row_constructor(A) ::= LPAREN(L) expression(B) COMMA expression_list(C) RPAREN(R). {
    A = mylite_sql_parser_make_row_constructor(
        state, L,
        (struct mylite_sql_parser_row_constructor_elements){
            .first_expression = B,
            .remaining_expressions = C,
        },
        R);
}
row_constructor(A) ::= ROW(T) LPAREN expression(B) COMMA expression_list(C) RPAREN(R). {
    A = mylite_sql_parser_make_row_constructor(
        state, T,
        (struct mylite_sql_parser_row_constructor_elements){
            .first_expression = B,
            .remaining_expressions = C,
        },
        R);
}

cast_expression(A) ::= CAST(T) LPAREN expression(B) AS cast_target_type(C) RPAREN(R). {
    A = mylite_sql_parser_make_cast_expression(state, T, B, C, R);
}

convert_expression(A) ::= CONVERT(T) LPAREN expression(B) COMMA cast_target_type(C) RPAREN(R). {
    A = mylite_sql_parser_make_cast_expression(state, T, B, C, R);
}
convert_expression(A) ::= CONVERT(T) LPAREN expression(B) USING charset_value(C) RPAREN(R). {
    A = mylite_sql_parser_make_convert_using_expression(
        state,
        (struct mylite_sql_parser_convert_using_expression_parts){
            .convert_token = T,
            .expression = B,
            .charset = C,
            .right_paren = R,
        });
}

cast_target_type(A) ::= SIGNED(T) opt_integer_keyword. {
    A = mylite_sql_parser_set_column_type_signed(
        state, mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_BIGINT), T);
}
cast_target_type(A) ::= UNSIGNED(T) opt_integer_keyword. {
    A = mylite_sql_parser_set_column_type_unsigned(
        state, mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_BIGINT), T);
}
cast_target_type(A) ::= DECIMALKW(T) opt_numeric_precision_scale(B). {
    A = mylite_sql_parser_validate_column_type(
        state, mylite_sql_parser_set_column_precision_scale(
                   state, mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_DECIMAL),
                   B));
}
cast_target_type(A) ::= DEC(T) opt_numeric_precision_scale(B). {
    A = mylite_sql_parser_validate_column_type(
        state, mylite_sql_parser_set_column_precision_scale(
                   state, mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_DECIMAL),
                   B));
}
cast_target_type(A) ::= CHAR(T) opt_column_length(B) opt_cast_character_set(C). {
    A = mylite_sql_parser_make_cast_character_target(
        state, T,
        (struct mylite_sql_parser_cast_character_target_parts){
            .length = B,
            .attributes = C,
        });
}
cast_target_type(A) ::= NCHAR(T) opt_column_length(B). {
    A = mylite_sql_parser_set_column_type_national(
        state,
        mylite_sql_parser_validate_column_type(
            state, mylite_sql_parser_set_column_length(
                       state,
                       mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_CHAR),
                       B)),
        T);
}
cast_target_type(A) ::= BINARY(T). {
    A = mylite_sql_parser_make_column_type(state, T, MYLITE_SQL_AST_COLUMN_TYPE_BINARY);
}

opt_integer_keyword ::= .
opt_integer_keyword ::= INTEGERKW.

opt_cast_character_set(A) ::= . {
    A = mylite_sql_parser_make_column_type_attribute_list(state);
}
opt_cast_character_set(A) ::= CHARACTER SET charset_value(B). {
    A = mylite_sql_parser_set_column_type_character_set(
        state, mylite_sql_parser_make_column_type_attribute_list(state), B);
}
opt_cast_character_set(A) ::= CHARSET charset_value(B). {
    A = mylite_sql_parser_set_column_type_character_set(
        state, mylite_sql_parser_make_column_type_attribute_list(state), B);
}

case_expression(A) ::= CASE(T) expression(B) case_when_list(C) opt_case_else(D) END(E). {
    A = mylite_sql_parser_make_simple_case_expression(state, T, B, C, D, E);
}
case_expression(A) ::= CASE(T) case_when_list(B) opt_case_else(C) END(E). {
    A = mylite_sql_parser_make_searched_case_expression(state, T, B, C, E);
}

case_when_list(A) ::= case_when(B). {
    A = mylite_sql_parser_make_case_when_list(state, B);
}
case_when_list(A) ::= case_when_list(B) case_when(C). {
    A = mylite_sql_parser_append_case_when(state, B, C);
}

case_when(A) ::= WHEN(T) expression(B) THEN expression(C). {
    A = mylite_sql_parser_make_case_when(state, T, B, C);
}

opt_case_else(A) ::= . {
    A = NULL;
}
opt_case_else(A) ::= ELSE expression(B). {
    A = B;
}

bare_temporal_function(A) ::= CURRENT_DATE(T). {
    A = mylite_sql_parser_make_bare_function_call(state, T);
}
bare_temporal_function(A) ::= CURRENT_TIME(T). {
    A = mylite_sql_parser_make_bare_function_call(state, T);
}
bare_temporal_function(A) ::= LOCALTIME(T). {
    A = mylite_sql_parser_make_bare_function_call(state, T);
}
bare_temporal_function(A) ::= LOCALTIMESTAMP(T). {
    A = mylite_sql_parser_make_bare_function_call(state, T);
}
bare_temporal_function(A) ::= UTC_DATE(T). {
    A = mylite_sql_parser_make_bare_function_call(state, T);
}
bare_temporal_function(A) ::= UTC_TIME(T). {
    A = mylite_sql_parser_make_bare_function_call(state, T);
}
bare_temporal_function(A) ::= UTC_TIMESTAMP(T). {
    A = mylite_sql_parser_make_bare_function_call(state, T);
}

bare_current_user_function(A) ::= CURRENT_USER(T). {
    A = mylite_sql_parser_make_bare_function_call(state, T);
}

window_function_call(A) ::= window_function_name(B) LPAREN(L) RPAREN(R)
        opt_window_null_treatment(N) over_clause(O). {
    A = mylite_sql_parser_make_window_function_call(
        state, B, L, mylite_sql_parser_make_empty_function_argument_list(state, L, R), R, N, O);
}
window_function_call(A) ::= window_function_name(B) LPAREN(L) function_argument_list(C) RPAREN(R)
        opt_window_null_treatment(N) over_clause(O). {
    A = mylite_sql_parser_make_window_function_call(state, B, L, C, R, N, O);
}

window_function_name(A) ::= CUME_DIST(T). {
    A = mylite_sql_parser_make_window_function_token(
        T, MYLITE_SQL_AST_WINDOW_FUNCTION_CUME_DIST);
}
window_function_name(A) ::= DENSE_RANK(T). {
    A = mylite_sql_parser_make_window_function_token(
        T, MYLITE_SQL_AST_WINDOW_FUNCTION_DENSE_RANK);
}
window_function_name(A) ::= FIRST_VALUE(T). {
    A = mylite_sql_parser_make_window_function_token(
        T, MYLITE_SQL_AST_WINDOW_FUNCTION_FIRST_VALUE);
}
window_function_name(A) ::= LAG(T). {
    A = mylite_sql_parser_make_window_function_token(T, MYLITE_SQL_AST_WINDOW_FUNCTION_LAG);
}
window_function_name(A) ::= LAST_VALUE(T). {
    A = mylite_sql_parser_make_window_function_token(
        T, MYLITE_SQL_AST_WINDOW_FUNCTION_LAST_VALUE);
}
window_function_name(A) ::= LEAD(T). {
    A = mylite_sql_parser_make_window_function_token(T, MYLITE_SQL_AST_WINDOW_FUNCTION_LEAD);
}
window_function_name(A) ::= NTH_VALUE(T). {
    A = mylite_sql_parser_make_window_function_token(
        T, MYLITE_SQL_AST_WINDOW_FUNCTION_NTH_VALUE);
}
window_function_name(A) ::= NTILE(T). {
    A = mylite_sql_parser_make_window_function_token(T, MYLITE_SQL_AST_WINDOW_FUNCTION_NTILE);
}
window_function_name(A) ::= PERCENT_RANK(T). {
    A = mylite_sql_parser_make_window_function_token(
        T, MYLITE_SQL_AST_WINDOW_FUNCTION_PERCENT_RANK);
}
window_function_name(A) ::= RANK(T). {
    A = mylite_sql_parser_make_window_function_token(T, MYLITE_SQL_AST_WINDOW_FUNCTION_RANK);
}
window_function_name(A) ::= ROW_NUMBER(T). {
    A = mylite_sql_parser_make_window_function_token(
        T, MYLITE_SQL_AST_WINDOW_FUNCTION_ROW_NUMBER);
}

opt_window_null_treatment(A) ::= . {
    A = NULL;
}
opt_window_null_treatment(A) ::= window_null_treatment(T) NULLS(N). {
    A = mylite_sql_parser_make_window_null_treatment(state, T, N);
}

window_null_treatment(A) ::= RESPECT(T). {
    A = mylite_sql_parser_make_window_null_treatment_token(
        T, MYLITE_SQL_AST_WINDOW_NULL_TREATMENT_RESPECT);
}
window_null_treatment(A) ::= IGNORE(T). {
    A = mylite_sql_parser_make_window_null_treatment_token(
        T, MYLITE_SQL_AST_WINDOW_NULL_TREATMENT_IGNORE);
}

aggregate_star_call(A) ::= function_name(B) LPAREN(L) STAR(S) RPAREN(R). {
    A = mylite_sql_parser_make_aggregate_star_call(
        state, B,
        (struct mylite_sql_parser_aggregate_star_tokens){
            .left_paren = L,
            .star = S,
            .right_paren = R,
        });
}

group_concat_call(A) ::= function_name(B) LPAREN(L) function_argument_list(C) order_by_clause(O) opt_group_concat_separator(S) RPAREN(R). {
    A = mylite_sql_parser_make_group_concat_call(
        state,
        (struct mylite_sql_parser_group_concat_call_parts){
            .name = B,
            .left_paren = L,
            .distinct = (struct mylite_sql_token){0},
            .arguments = C,
            .order_by = O,
            .separator = S,
            .right_paren = R,
        });
}
group_concat_call(A) ::= function_name(B) LPAREN(L) function_argument_list(C) group_concat_separator(S) RPAREN(R). {
    A = mylite_sql_parser_make_group_concat_call(
        state,
        (struct mylite_sql_parser_group_concat_call_parts){
            .name = B,
            .left_paren = L,
            .distinct = (struct mylite_sql_token){0},
            .arguments = C,
            .order_by = NULL,
            .separator = S,
            .right_paren = R,
        });
}
group_concat_call(A) ::= function_name(B) LPAREN(L) DISTINCT(D) expression_list(C) order_by_clause(O) opt_group_concat_separator(S) RPAREN(R). {
    A = mylite_sql_parser_make_group_concat_call(
        state,
        (struct mylite_sql_parser_group_concat_call_parts){
            .name = B,
            .left_paren = L,
            .distinct = D,
            .arguments = C,
            .order_by = O,
            .separator = S,
            .right_paren = R,
        });
}
group_concat_call(A) ::= function_name(B) LPAREN(L) DISTINCT(D) expression_list(C) group_concat_separator(S) RPAREN(R). {
    A = mylite_sql_parser_make_group_concat_call(
        state,
        (struct mylite_sql_parser_group_concat_call_parts){
            .name = B,
            .left_paren = L,
            .distinct = D,
            .arguments = C,
            .order_by = NULL,
            .separator = S,
            .right_paren = R,
        });
}

group_concat_separator(A) ::= SEPARATOR STRING(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_STRING);
}

opt_group_concat_separator(A) ::= . {
    A = NULL;
}
opt_group_concat_separator(A) ::= group_concat_separator(B). {
    A = B;
}

aggregate_distinct_call(A) ::= function_name(B) LPAREN(L) DISTINCT expression_list(C) RPAREN(R). {
    A = mylite_sql_parser_make_distinct_aggregate_call(state, B, L, C, R);
}

scalar_function_call(A) ::= function_name(B) LPAREN(L) RPAREN(R). {
    A = mylite_sql_parser_make_function_call(
        state, B, L, mylite_sql_parser_make_empty_function_argument_list(state, L, R), R);
}
scalar_function_call(A) ::= function_name(B) LPAREN(L) function_argument_list(C) RPAREN(R). {
    A = mylite_sql_parser_make_function_call(state, B, L, C, R);
}
scalar_function_call(A) ::= DEFAULT(T) LPAREN(L) qualified_identifier(B) RPAREN(R). {
    A = mylite_sql_parser_make_function_call(
        state, mylite_sql_parser_make_identifier(state, T), L,
        mylite_sql_parser_make_function_argument_list(state, B), R);
}
scalar_function_call(A) ::= date_interval_function_name(B) LPAREN(L) expression(C) COMMA INTERVAL expression(D) interval_unit(U) RPAREN(R). {
    A = mylite_sql_parser_make_interval_function_call(
        state, B, (struct mylite_sql_parser_interval_function_call_parts){
                      .left_paren = L,
                      .temporal = C,
                      .amount = D,
                      .unit = U,
                      .right_paren = R,
                  });
}
scalar_function_call(A) ::= CHAR(T) LPAREN(L) function_argument_list(C) RPAREN(R). {
    A = mylite_sql_parser_make_char_function_call(
        state, (struct mylite_sql_parser_char_function_call_parts){
                   .char_token = T,
                   .left_paren = L,
                   .arguments = C,
                   .charset = NULL,
                   .right_paren = R,
               });
}
scalar_function_call(A) ::= CHAR(T) LPAREN(L) function_argument_list(C) USING charset_value(U) RPAREN(R). {
    A = mylite_sql_parser_make_char_function_call(
        state, (struct mylite_sql_parser_char_function_call_parts){
                   .char_token = T,
                   .left_paren = L,
                   .arguments = C,
                   .charset = U,
                   .right_paren = R,
               });
}
scalar_function_call(A) ::= IF(T) LPAREN(L) expression(B) COMMA expression(C) COMMA expression(D) RPAREN(R). {
    struct mylite_sql_ast_node *arguments =
        mylite_sql_parser_make_function_argument_list(state, B);
    arguments = mylite_sql_parser_append_function_argument(state, arguments, C);
    arguments = mylite_sql_parser_append_function_argument(state, arguments, D);
    A = mylite_sql_parser_make_function_call(
        state, mylite_sql_parser_make_identifier(state, T), L, arguments, R);
}
scalar_function_call(A) ::= function_name(B) LPAREN(L) expression(C) FROM expression(D) RPAREN(R). {
    A = mylite_sql_parser_make_from_function_call(state, B, L, C, D, R);
}
scalar_function_call(A) ::= POSITION(T) LPAREN(L) bit_or_expression(C) IN expression(D) RPAREN(R). {
    A = mylite_sql_parser_make_position_function_call(
        state, mylite_sql_parser_make_identifier(state, T), L,
        (struct mylite_sql_parser_position_operands){
            .substring = C,
            .source = D,
        },
        R);
}
scalar_function_call(A) ::= function_name(B) LPAREN(L) expression(C) FROM expression(D) FOR expression(E) RPAREN(R). {
    A = mylite_sql_parser_make_substring_for_function_call(
        state, B, L,
        (struct mylite_sql_parser_substring_operands){
            .text = C,
            .position = D,
            .length = E,
        },
        R);
}
scalar_function_call(A) ::= function_name(B) LPAREN(L) trim_direction(D) expression(C) FROM expression(E) RPAREN(R). {
    A = mylite_sql_parser_make_trim_direction_function_call(
        state, B, L, D,
        (struct mylite_sql_parser_trim_operands){
            .remove = C,
            .source = E,
        },
        R);
}
scalar_function_call(A) ::= function_name(B) LPAREN(L) trim_direction(D) FROM expression(E) RPAREN(R). {
    A = mylite_sql_parser_make_trim_direction_function_call(
        state, B, L, D,
        (struct mylite_sql_parser_trim_operands){
            .remove = NULL,
            .source = E,
        },
        R);
}

trim_direction(A) ::= BOTH. {
    A = MYLITE_SQL_AST_TRIM_DIRECTION_BOTH;
}
trim_direction(A) ::= LEADING. {
    A = MYLITE_SQL_AST_TRIM_DIRECTION_LEADING;
}
trim_direction(A) ::= TRAILING. {
    A = MYLITE_SQL_AST_TRIM_DIRECTION_TRAILING;
}

interval_unit(A) ::= DAY. {
    A = MYLITE_SQL_AST_INTERVAL_UNIT_DAY;
}
interval_unit(A) ::= WEEK. {
    A = MYLITE_SQL_AST_INTERVAL_UNIT_WEEK;
}
interval_unit(A) ::= MONTH. {
    A = MYLITE_SQL_AST_INTERVAL_UNIT_MONTH;
}
interval_unit(A) ::= YEAR. {
    A = MYLITE_SQL_AST_INTERVAL_UNIT_YEAR;
}
interval_unit(A) ::= HOUR. {
    A = MYLITE_SQL_AST_INTERVAL_UNIT_HOUR;
}
interval_unit(A) ::= MINUTE. {
    A = MYLITE_SQL_AST_INTERVAL_UNIT_MINUTE;
}
interval_unit(A) ::= SECOND. {
    A = MYLITE_SQL_AST_INTERVAL_UNIT_SECOND;
}

date_interval_function_name(A) ::= DATE_ADD(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
date_interval_function_name(A) ::= DATE_SUB(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
date_interval_function_name(A) ::= ADDDATE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
date_interval_function_name(A) ::= SUBDATE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}

function_name(A) ::= identifier(B). {
    A = B;
}
function_name(A) ::= CURRENT_DATE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
function_name(A) ::= CURRENT_TIME(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
function_name(A) ::= CURRENT_USER(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
function_name(A) ::= DATE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
function_name(A) ::= DATABASE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
function_name(A) ::= FORMAT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
function_name(A) ::= INSERT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
function_name(A) ::= LEFT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
function_name(A) ::= LOCALTIME(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
function_name(A) ::= LOCALTIMESTAMP(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
function_name(A) ::= MOD(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
function_name(A) ::= REPEAT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
function_name(A) ::= REPLACE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
function_name(A) ::= RIGHT(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
function_name(A) ::= SCHEMA(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
function_name(A) ::= UTC_DATE(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
function_name(A) ::= UTC_TIME(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
function_name(A) ::= UTC_TIMESTAMP(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
function_name(A) ::= VALUES(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}

function_argument_list(A) ::= expression(B). {
    A = mylite_sql_parser_make_function_argument_list(state, B);
}
function_argument_list(A) ::= function_argument_list(B) COMMA expression(C). {
    A = mylite_sql_parser_append_function_argument(state, B, C);
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

numeric_literal(A) ::= INTEGER(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_INTEGER);
}
numeric_literal(A) ::= DECIMAL(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_DECIMAL);
}
numeric_literal(A) ::= FLOAT(T). {
    A = mylite_sql_parser_make_literal(state, T, MYLITE_SQL_AST_LITERAL_FLOAT);
}

qualified_identifier(A) ::= identifier(B). {
    A = B;
}
qualified_identifier(A) ::= qualified_identifier(B) DOT identifier(C). {
    A = mylite_sql_parser_make_qualified_identifier(state, B, C);
}

identifier(A) ::= IDENTIFIER(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= QUOTED_IDENTIFIER(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}
identifier(A) ::= nonreserved_identifier_keyword(T). {
    A = mylite_sql_parser_make_identifier(state, T);
}

nonreserved_identifier_keyword(A) ::= ALGORITHM(T). {
    A = T;
}
nonreserved_identifier_keyword(A) ::= AFTER(T). {
    A = T;
}
nonreserved_identifier_keyword(A) ::= COPY(T). {
    A = T;
}
nonreserved_identifier_keyword(A) ::= COLUMNS(T). {
    A = T;
}
nonreserved_identifier_keyword(A) ::= COALESCE(T). {
    A = T;
}
nonreserved_identifier_keyword(A) ::= CHANGED(T). {
    A = T;
}
nonreserved_identifier_keyword(A) ::= CONNECTION(T). {
    A = T;
}
nonreserved_identifier_keyword(A) ::= DISCARD(T). {
    A = T;
}
nonreserved_identifier_keyword(A) ::= EXCHANGE(T). {
    A = T;
}
nonreserved_identifier_keyword(A) ::= EXCLUSIVE(T). {
    A = T;
}
nonreserved_identifier_keyword(A) ::= EXTENDED(T). {
    A = T;
}
nonreserved_identifier_keyword(A) ::= FAST(T). {
    A = T;
}
nonreserved_identifier_keyword(A) ::= FIRST(T). {
    A = T;
}
nonreserved_identifier_keyword(A) ::= FIELDS(T). {
    A = T;
}
nonreserved_identifier_keyword(A) ::= FULL(T). {
    A = T;
}
nonreserved_identifier_keyword(A) ::= GLOBAL(T). {
    A = T;
}
nonreserved_identifier_keyword(A) ::= GRANTS(T). {
    A = T;
}
nonreserved_identifier_keyword(A) ::= IMPORT(T). {
    A = T;
}
nonreserved_identifier_keyword(A) ::= INPLACE(T). {
    A = T;
}
nonreserved_identifier_keyword(A) ::= INSTANT(T). {
    A = T;
}
nonreserved_identifier_keyword(A) ::= INDEXES(T). {
    A = T;
}
nonreserved_identifier_keyword(A) ::= LOCAL(T). {
    A = T;
}
nonreserved_identifier_keyword(A) ::= MEDIUM(T). {
    A = T;
}
nonreserved_identifier_keyword(A) ::= MODIFY(T). {
    A = T;
}
nonreserved_identifier_keyword(A) ::= NONE(T). {
    A = T;
}
nonreserved_identifier_keyword(A) ::= PARSER(T). {
    A = T;
}
nonreserved_identifier_keyword(A) ::= PARTITIONING(T). {
    A = T;
}
nonreserved_identifier_keyword(A) ::= PASSWORD(T). {
    A = T;
}
nonreserved_identifier_keyword(A) ::= PRIVILEGES(T). {
    A = T;
}
nonreserved_identifier_keyword(A) ::= QUICK(T). {
    A = T;
}
nonreserved_identifier_keyword(A) ::= REBUILD(T). {
    A = T;
}
nonreserved_identifier_keyword(A) ::= REORGANIZE(T). {
    A = T;
}
nonreserved_identifier_keyword(A) ::= REPAIR(T). {
    A = T;
}
nonreserved_identifier_keyword(A) ::= REMOVE(T). {
    A = T;
}
nonreserved_identifier_keyword(A) ::= ROLE(T). {
    A = T;
}
nonreserved_identifier_keyword(A) ::= SHARED(T). {
    A = T;
}
nonreserved_identifier_keyword(A) ::= SESSION(T). {
    A = T;
}
nonreserved_identifier_keyword(A) ::= STATUS(T). {
    A = T;
}
nonreserved_identifier_keyword(A) ::= TABLES(T). {
    A = T;
}
nonreserved_identifier_keyword(A) ::= TIME(T). {
    A = T;
}
nonreserved_identifier_keyword(A) ::= TRUNCATE(T). {
    A = T;
}
nonreserved_identifier_keyword(A) ::= UPGRADE(T). {
    A = T;
}
nonreserved_identifier_keyword(A) ::= USER(T). {
    A = T;
}
nonreserved_identifier_keyword(A) ::= USE_FRM(T). {
    A = T;
}
nonreserved_identifier_keyword(A) ::= VALUE(T). {
    A = T;
}
nonreserved_identifier_keyword(A) ::= VARIABLES(T). {
    A = T;
}
