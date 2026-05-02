%name mylite_sql_lemon
%token_prefix MYLITE_SQL_PARSE_
%token_type { struct mylite_sql_token }
%default_type { struct mylite_sql_ast_node * }
%type opt_into { struct mylite_sql_token }
%type opt_temporary { struct mylite_sql_token }
%type opt_drop_table_mode { struct mylite_sql_token }
%type insert_values_keyword { struct mylite_sql_token }
%type opt_order_direction { struct mylite_sql_token }
%type opt_work { struct mylite_sql_token }
%type opt_like_escape { struct mylite_sql_ast_node * }
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

%left LOWEST.
%left OR LOGICAL_OR.
%left XOR.
%left AND LOGICAL_AND.
%right NOT.
%left BETWEEN.
%left EQ NULL_SAFE_EQ NE LT LE GT GE IS LIKE IN.
%left BIT_OR.
%left BIT_AND.
%left SHIFT_LEFT SHIFT_RIGHT.
%left PLUS MINUS.
%left STAR SLASH DIV PERCENT MOD.
%left BIT_XOR.
%right UPLUS UMINUS BIT_NOT.
%right LOGICAL_NOT.
%right KEY.
%fallback IDENTIFIER AUTO_INCREMENT BEGIN BOOL BOOLEAN BTREE CHAIN CHARSET COLUMN_FORMAT COMMENT
    COMMIT CONSISTENT DATE DATETIME DISK DYNAMIC ENGINE ENGINE_ATTRIBUTE ENCRYPTION FIXED HASH
    INVISIBLE KEY_BLOCK_SIZE MEMORY NCHAR NO NVARCHAR OFFSET ONLY ROLLBACK
    SECONDARY_ENGINE_ATTRIBUTE SIGNED SNAPSHOT START STORAGE TEMPORARY TEXT TIME TIMESTAMP
    TRANSACTION VISIBLE VALUE WORK YEAR.

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
statement(A) ::= create_schema_statement(B). {
    A = B;
}
statement(A) ::= alter_schema_statement(B). {
    A = B;
}
statement(A) ::= drop_schema_statement(B). {
    A = B;
}
statement(A) ::= drop_table_statement(B). {
    A = B;
}
statement(A) ::= insert_values_statement(B). {
    A = B;
}
statement(A) ::= insert_set_statement(B). {
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
statement(A) ::= show_schemas_statement(B). {
    A = B;
}
statement(A) ::= set_names_statement(B). {
    A = B;
}
statement(A) ::= set_character_set_statement(B). {
    A = B;
}
statement(A) ::= create_table_statement(B). {
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

opt_temporary(A) ::= . {
    A = (struct mylite_sql_token){0};
}
opt_temporary(A) ::= TEMPORARY(T). {
    A = T;
}

drop_table_name_list(A) ::= table_name(B). {
    A = mylite_sql_parser_make_table_name_list(state, B);
}
drop_table_name_list(A) ::= drop_table_name_list(B) COMMA table_name(C). {
    A = mylite_sql_parser_append_table_name(state, B, C);
}

opt_drop_table_mode(A) ::= . {
    A = (struct mylite_sql_token){0};
}
opt_drop_table_mode(A) ::= RESTRICT(T). {
    A = T;
}
opt_drop_table_mode(A) ::= CASCADE(T). {
    A = T;
}

insert_values_statement(A) ::= INSERT(T) opt_into table_name(B) opt_insert_column_list(C) insert_values_keyword insert_row_list(D). {
    A = mylite_sql_parser_make_insert_values_statement(state, T, B, C, D);
}
insert_set_statement(A) ::= INSERT(T) opt_into table_name(B) SET insert_set_assignment_list(C). {
    A = mylite_sql_parser_make_insert_set_statement(state, T, B, C);
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

single_update_target(A) ::= update_table_name(B) opt_table_alias(C). {
    A = mylite_sql_parser_make_update_target(state, B, C);
}

update_table_name(A) ::= identifier(B). {
    A = B;
}
update_table_name(A) ::= identifier(B) DOT identifier(C). {
    A = mylite_sql_parser_make_qualified_identifier(state, B, C);
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

delete_statement(A) ::= DELETE(T) FROM single_delete_target(B) opt_where_clause(C)
        opt_order_by_clause(D) opt_delete_limit_clause(E). {
    A = mylite_sql_parser_make_delete_statement(state, T, B, C, D, E);
}

single_delete_target(A) ::= delete_table_name(B) opt_table_alias(C). {
    A = mylite_sql_parser_make_delete_target(state, B, C);
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

opt_work(A) ::= . {
    A = (struct mylite_sql_token){0};
}
opt_work(A) ::= WORK(T). {
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

create_table_statement(A) ::= CREATE(T) TABLE opt_if_not_exists(B) table_name(C) LPAREN table_element_list(D) RPAREN table_option_list(E). {
    A = mylite_sql_parser_make_create_table_statement(state, T, B, C, D, E);
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
    A = mylite_sql_parser_make_parenthesized_expression(state, L, B, R);
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

select_statement(A) ::= SELECT(T) select_item_list(B). {
    A = mylite_sql_parser_make_select_statement(state, T, B, NULL, NULL, NULL, NULL);
}
select_statement(A) ::= SELECT(T) select_item_list(B) FROM(F) DUAL(D). {
    A = mylite_sql_parser_make_select_statement(
        state, T, B, mylite_sql_parser_make_from_dual(state, F, D), NULL, NULL, NULL);
}
select_statement(A) ::= SELECT(T) select_item_list(B) FROM(F) table_name(C) opt_table_alias(D)
        opt_where_clause(E) opt_order_by_clause(G) opt_limit_clause(H). {
    A = mylite_sql_parser_make_select_statement(
        state, T, B, mylite_sql_parser_make_from_table(state, F, C, D), E, G, H);
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
select_statement(A) ::= SELECT(T) STAR(S) FROM(F) table_name(C) opt_table_alias(D)
        opt_where_clause(E) opt_order_by_clause(G) opt_limit_clause(H). {
    A = mylite_sql_parser_make_select_statement(
        state, T, mylite_sql_parser_make_wildcard_select_list(state, S),
        mylite_sql_parser_make_from_table(state, F, C, D), E, G, H);
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

comparison_expression(A) ::= bit_or_expression(B). {
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
comparison_expression(A) ::= comparison_expression(B) IN(T) LPAREN expression_list(C) RPAREN. {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_IN, C);
}
comparison_expression(A) ::= comparison_expression(B) NOT(T) IN LPAREN expression_list(C) RPAREN. {
    A = mylite_sql_parser_make_binary_expression(
        state, B, T, MYLITE_SQL_AST_OPERATOR_NOT_IN, C);
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

primary_expression(A) ::= literal(B). {
    A = B;
}
primary_expression(A) ::= qualified_identifier(B). {
    A = B;
}
primary_expression(A) ::= current_timestamp_value(B). {
    A = B;
}
primary_expression(A) ::= LPAREN(L) expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_parenthesized_expression(state, L, B, R);
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
