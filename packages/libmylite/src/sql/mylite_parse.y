%name mylite_sql_lemon
%token_prefix MYLITE_SQL_PARSE_
%token_type { struct mylite_sql_token }
%default_type { struct mylite_sql_ast_node * }
%type opt_column_unique_key { struct mylite_sql_token }
%type opt_into { struct mylite_sql_token }
%type opt_temporary { struct mylite_sql_token }
%type opt_drop_table_mode { struct mylite_sql_token }
%type insert_values_keyword { struct mylite_sql_token }
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
%right KEY.
%fallback IDENTIFIER AUTO_INCREMENT BOOL BOOLEAN BTREE CHARSET COLUMN_FORMAT COMMENT DATE DATETIME
    DISK DYNAMIC ENGINE ENGINE_ATTRIBUTE ENCRYPTION FIXED HASH INVISIBLE KEY_BLOCK_SIZE MEMORY NCHAR
    NVARCHAR ONLY SECONDARY_ENGINE_ATTRIBUTE SIGNED STORAGE TEMPORARY TEXT TIME TIMESTAMP VISIBLE
    VALUE YEAR.

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
column_attribute(A) ::= UNIQUE(U) opt_column_unique_key(K). {
    A = mylite_sql_parser_make_column_unique_key_attribute(
        state, (struct mylite_sql_parser_column_unique_key_attribute_tokens){
            .unique_token = U,
            .key_token = K,
        });
}

opt_column_unique_key(A) ::= . [KEY] {
    A = (struct mylite_sql_token){0};
}
opt_column_unique_key(A) ::= KEY(T). {
    A = T;
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
    A = mylite_sql_parser_make_select_statement(state, T, B, NULL);
}
select_statement(A) ::= SELECT(T) select_item_list(B) FROM(F) DUAL(D). {
    A = mylite_sql_parser_make_select_statement(
        state, T, B, mylite_sql_parser_make_from_dual(state, F, D));
}
select_statement(A) ::= SELECT(T) select_item_list(B) FROM(F) table_name(C) opt_table_alias(D). {
    A = mylite_sql_parser_make_select_statement(
        state, T, B, mylite_sql_parser_make_from_table(state, F, C, D));
}
select_statement(A) ::= SELECT(T) STAR(S). {
    A = mylite_sql_parser_make_select_statement(
        state, T, mylite_sql_parser_make_wildcard_select_list(state, S), NULL);
}
select_statement(A) ::= SELECT(T) STAR(S) FROM(F) DUAL(D). {
    A = mylite_sql_parser_make_select_statement(
        state, T, mylite_sql_parser_make_wildcard_select_list(state, S),
        mylite_sql_parser_make_from_dual(state, F, D));
}
select_statement(A) ::= SELECT(T) STAR(S) FROM(F) table_name(C) opt_table_alias(D). {
    A = mylite_sql_parser_make_select_statement(
        state, T, mylite_sql_parser_make_wildcard_select_list(state, S),
        mylite_sql_parser_make_from_table(state, F, C, D));
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

expression(A) ::= literal(B). {
    A = B;
}
expression(A) ::= qualified_identifier(B). {
    A = B;
}
expression(A) ::= current_timestamp_value(B). {
    A = B;
}
expression(A) ::= LPAREN(L) expression(B) RPAREN(R). {
    A = mylite_sql_parser_make_parenthesized_expression(state, L, B, R);
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
