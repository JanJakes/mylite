#include "mylite_expression_validation.h"

#include "mylite_charset.h"
#include "mylite_diagnostics.h"
#include "mylite_metadata_constants.h"
#include "mylite_span.h"

#include <stdlib.h>

static bool expression_function_name_is_char(const struct mylite_sql_ast_node *name);

static int append_utf8_alias_warning_if_needed(mylite_db *database, const char *charset_name);

int mylite_expression_validate_char_function_charset(
    mylite_db *database,
    const struct mylite_sql_ast_node *expression
) {
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *charset_node = mylite_ast_child_at(expression, 2U);
    char *charset_name = NULL;
    int status = MYLITE_OK;

    if (!expression_function_name_is_char(name) || charset_node == NULL) {
        return MYLITE_OK;
    }
    charset_name = mylite_copy_schema_text_span(charset_node);
    if (charset_name == NULL) {
        return MYLITE_NOMEM;
    }
    if (!mylite_expression_char_function_charset_name_is_supported(charset_name)) {
        status = mylite_diagnostics_set_unknown_charset_error(database, charset_name);
    }
    if (status == MYLITE_OK) {
        status = append_utf8_alias_warning_if_needed(database, charset_name);
    }
    free(charset_name);
    return status;
}

int mylite_expression_validate_cast_target_charset(
    mylite_db *database,
    const struct mylite_sql_ast_node *expression
) {
    const struct mylite_sql_ast_node *target = mylite_ast_child_at(expression, 1U);
    char *charset_name = NULL;
    int status = MYLITE_OK;

    if (target == NULL || target->kind != MYLITE_SQL_AST_COLUMN_TYPE ||
        target->column_type != MYLITE_SQL_AST_COLUMN_TYPE_CHAR) {
        return MYLITE_OK;
    }
    if (target->column_national_attribute) {
        return mylite_diagnostics_append_national_charset_warning(database, target->span);
    }
    if (!target->has_column_character_set) {
        return MYLITE_OK;
    }

    charset_name = mylite_copy_unquoted_span_text(target->column_character_set);
    if (charset_name == NULL) {
        return MYLITE_NOMEM;
    }
    if (!mylite_expression_char_function_charset_name_is_supported(charset_name)) {
        status = mylite_diagnostics_set_unknown_charset_error(database, charset_name);
    }
    if (status == MYLITE_OK) {
        status = append_utf8_alias_warning_if_needed(database, charset_name);
    }
    free(charset_name);
    return status;
}

int mylite_expression_validate_collate_operator(
    mylite_db *database,
    const struct mylite_sql_ast_node *expression
) {
    const struct mylite_sql_ast_node *collation_node = mylite_ast_child_at(expression, 1U);
    char *collation_name = NULL;
    int status = MYLITE_OK;

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_BINARY_EXPRESSION ||
        expression->operator_kind != MYLITE_SQL_AST_OPERATOR_COLLATE) {
        return MYLITE_OK;
    }

    collation_name = mylite_copy_schema_text_span(collation_node);
    if (collation_name == NULL) {
        return MYLITE_NOMEM;
    }
    if (mylite_collation_lookup(collation_name) == NULL) {
        status = mylite_diagnostics_set_unknown_collation_error(database, collation_name);
    }
    free(collation_name);
    return status;
}

bool mylite_expression_char_function_charset_name_is_supported(const char *name) {
    if (mylite_ascii_case_equal(name, mylite_mysql_binary_charset_name)) {
        return true;
    }
    if (mylite_ascii_case_equal(name, "latin1") || mylite_ascii_case_equal(name, "utf8mb4") ||
        mylite_ascii_case_equal(name, "utf8mb3") || mylite_ascii_case_equal(name, "utf8") ||
        mylite_ascii_case_equal(name, "ascii")) {
        return true;
    }
    return false;
}

bool mylite_expression_literal_is_supported(const struct mylite_sql_ast_node *expression) {
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_LITERAL) {
        return false;
    }
    switch (expression->literal_kind) {
    case MYLITE_SQL_AST_LITERAL_NULL:
    case MYLITE_SQL_AST_LITERAL_TRUE:
    case MYLITE_SQL_AST_LITERAL_FALSE:
    case MYLITE_SQL_AST_LITERAL_INTEGER:
    case MYLITE_SQL_AST_LITERAL_DECIMAL:
    case MYLITE_SQL_AST_LITERAL_FLOAT:
    case MYLITE_SQL_AST_LITERAL_STRING:
    case MYLITE_SQL_AST_LITERAL_NATIONAL_STRING:
    case MYLITE_SQL_AST_LITERAL_BINARY_STRING:
    case MYLITE_SQL_AST_LITERAL_HEX:
    case MYLITE_SQL_AST_LITERAL_BIT:
    case MYLITE_SQL_AST_LITERAL_DATE:
    case MYLITE_SQL_AST_LITERAL_TIME:
    case MYLITE_SQL_AST_LITERAL_TIMESTAMP:
        return true;
    case MYLITE_SQL_AST_LITERAL_NONE:
        return false;
    }
    return false;
}

static bool expression_function_name_is_char(const struct mylite_sql_ast_node *name) {
    if (name == NULL) {
        return false;
    }
    return mylite_span_equal_ci(name->span, "CHAR");
}

static int append_utf8_alias_warning_if_needed(mylite_db *database, const char *charset_name) {
    if (!mylite_charset_name_is_utf8_alias(charset_name)) {
        return MYLITE_OK;
    }
    return mylite_diagnostics_append_utf8_alias_warning(database);
}
