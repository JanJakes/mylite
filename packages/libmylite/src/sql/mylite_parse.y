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
%fallback IDENTIFIER BOOL BOOLEAN CHARSET ENCRYPTION FIXED NCHAR NVARCHAR ONLY SIGNED TEXT.

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

create_table_statement(A) ::= CREATE(T) TABLE table_name(B) LPAREN column_definition_list(C) RPAREN. {
    A = mylite_sql_parser_make_create_table_statement(state, T, B, C);
}

column_definition_list(A) ::= column_definition(B). {
    A = mylite_sql_parser_make_column_definition_list(state, B);
}
column_definition_list(A) ::= column_definition_list(B) COMMA column_definition(C). {
    A = mylite_sql_parser_append_column_definition(state, B, C);
}

column_definition(A) ::= identifier(B) column_type(C). {
    A = mylite_sql_parser_make_column_definition(state, B, C);
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
select_statement(A) ::= SELECT(T) select_item_list(B) FROM(F) table_name(C). {
    A = mylite_sql_parser_make_select_statement(
        state, T, B, mylite_sql_parser_make_from_table(state, F, C));
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
select_statement(A) ::= SELECT(T) STAR(S) FROM(F) table_name(C). {
    A = mylite_sql_parser_make_select_statement(
        state, T, mylite_sql_parser_make_wildcard_select_list(state, S),
        mylite_sql_parser_make_from_table(state, F, C));
}

table_name(A) ::= qualified_identifier(B). {
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

expression(A) ::= literal(B). {
    A = B;
}
expression(A) ::= qualified_identifier(B). {
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
