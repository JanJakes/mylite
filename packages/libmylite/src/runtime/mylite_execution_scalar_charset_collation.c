#include "mylite_execution_scalar_charset_collation.h"
#include "mylite_execution_scalar.h"

#include "mylite_connection.h"
#include "mylite_execution_catalog.h"

#include <mylite/mylite.h>

#include <stdbool.h>
#include <string.h>

/* Static helper prototypes. */
static int charset_collation_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int charset_collation_scalar_result(
    struct mylite_db *database,
    enum planned_charset_collation_function_kind function_kind,
    const struct mylite_sql_ast_node *expression,
    const char **out_result
);
static int coercibility_scalar_result(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char **out_result
);
static int coercibility_non_concat_scalar_result(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char **out_result
);
static int coercibility_concat_scalar_result(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char **out_result
);
static int apply_coercibility_concat_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *argument,
    const char **inout_result,
    bool *inout_has_non_null_argument,
    const struct mylite_execution_catalog_scalar_collation **inout_explicit_collation
);
static int coercibility_binary_wrapper_result(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char **out_result
);
static int coercibility_validate_binary_wrapper_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *argument
);
static int coercibility_literal_result(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char **out_result
);
static const char *coercibility_concat_argument_result(const char *argument_result);
static bool coercibility_binary_wrapper_column_reference(
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node **out_column_reference
);
static int set_unknown_column_for_reference(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
);
static int charset_collation_concat_scalar_result(
    struct mylite_db *database,
    enum planned_charset_collation_function_kind function_kind,
    const struct mylite_sql_ast_node *expression,
    const char **out_result
);
static int validate_charset_collation_concat_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool *out_is_binary_string_argument,
    const struct mylite_execution_catalog_scalar_collation **out_explicit_collation
);
static int merge_concat_explicit_collation(
    struct mylite_db *database,
    const struct mylite_execution_catalog_scalar_collation **inout_explicit_collation,
    const struct mylite_execution_catalog_scalar_collation *argument_collation
);
static int charset_collation_convert_using_charset_result(
    struct mylite_db *database,
    enum planned_charset_collation_function_kind function_kind,
    const struct mylite_sql_ast_node *expression,
    const char **out_result
);
static int charset_collation_collate_expression_result(
    struct mylite_db *database,
    enum planned_charset_collation_function_kind function_kind,
    const struct mylite_sql_ast_node *expression,
    const char **out_result
);
static int charset_collation_rand_result(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char **out_charset,
    const char **out_collation
);
static int scalar_expression_charset_collation_metadata(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char **out_charset,
    const char **out_collation
);
static int scalar_collation_info_for_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_execution_catalog_scalar_collation **out_info
);
static int scalar_expression_base_charset_collation_metadata(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char **out_charset,
    const char **out_collation
);
static const struct mylite_execution_catalog_scalar_collation *scalar_collation_info_by_name(
    const char *collation_name
);
static enum planned_charset_collation_function_kind charset_collation_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
);
static bool is_charset_collation_function_kind(enum mylite_sql_ast_node_kind ast_kind);
static int charset_collation_select_result(
    enum planned_charset_collation_function_kind function_kind,
    const char *charset,
    const char *collation,
    const char **out_result
);

int mylite_execution_scalar_charset_collation_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return charset_collation_function_value(database, expression, out_cell);
}

int mylite_execution_scalar_expression_charset_collation_metadata(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char **out_charset,
    const char **out_collation
) {
    return scalar_expression_charset_collation_metadata(
        database,
        expression,
        out_charset,
        out_collation
    );
}

int mylite_execution_charset_collation_scalar_result(
    struct mylite_db *database,
    enum planned_charset_collation_function_kind function_kind,
    const struct mylite_sql_ast_node *expression,
    const char **out_result
) {
    return charset_collation_scalar_result(database, function_kind, expression, out_result);
}

int mylite_execution_charset_collation_select_result(
    enum planned_charset_collation_function_kind function_kind,
    const char *charset,
    const char *collation,
    const char **out_result
) {
    return charset_collation_select_result(function_kind, charset, collation, out_result);
}

int mylite_execution_scalar_collation_info_for_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_execution_catalog_scalar_collation **out_info
) {
    return scalar_collation_info_for_expression(database, expression, out_info);
}

const struct mylite_execution_catalog_scalar_collation *mylite_execution_scalar_collation_info_by_name(
    const char *collation_name
) {
    return scalar_collation_info_by_name(collation_name);
}

bool mylite_execution_coercibility_binary_wrapper_column_reference(
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node **out_column_reference
) {
    return coercibility_binary_wrapper_column_reference(expression, out_column_reference);
}

enum planned_charset_collation_function_kind mylite_execution_charset_collation_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
) {
    return charset_collation_function_kind(ast_kind);
}

bool mylite_execution_is_charset_collation_function_kind(enum mylite_sql_ast_node_kind ast_kind) {
    return is_charset_collation_function_kind(ast_kind);
}

static int charset_collation_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    enum planned_charset_collation_function_kind function_kind =
        PLANNED_CHARSET_COLLATION_FUNCTION_NONE;
    const char *result = NULL;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    function_kind = expression == NULL ? PLANNED_CHARSET_COLLATION_FUNCTION_NONE
                                       : charset_collation_function_kind(expression->kind);
    if (function_kind == PLANNED_CHARSET_COLLATION_FUNCTION_NONE ||
        mylite_sql_ast_node_child_count(expression) != 1U) {
        mylite_execution_set_unsupported_error(
            database,
            "CHARSET(), COLLATION(), and COERCIBILITY() support exactly one argument"
        );
        return MYLITE_ERROR;
    }

    rc = charset_collation_scalar_result(
        database,
        function_kind,
        mylite_execution_child_at(expression, 0U),
        &result
    );
    if (rc == MYLITE_OK) {
        out_cell->value = result;
    }
    return rc;
}

static int charset_collation_scalar_result(
    struct mylite_db *database,
    enum planned_charset_collation_function_kind function_kind,
    const struct mylite_sql_ast_node *expression,
    const char **out_result
) {
    const char *charset = "binary";
    const char *collation = "binary";

    if (out_result == NULL) {
        return MYLITE_MISUSE;
    }
    *out_result = NULL;
    if (function_kind == PLANNED_CHARSET_COLLATION_FUNCTION_COERCIBILITY) {
        return coercibility_scalar_result(database, expression, out_result);
    }

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        mylite_execution_set_unsupported_error(
            database,
            "CHARSET(), COLLATION(), and COERCIBILITY() support only scalar metadata arguments"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return set_unknown_column_for_reference(database, expression);
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_LITERAL:
        if (mylite_sql_ast_node_literal_kind(expression) == MYLITE_SQL_AST_LITERAL_STRING) {
            charset = database->session.character_set_connection;
            collation = database->session.collation_connection;
        }
        return charset_collation_select_result(function_kind, charset, collation, out_result);
    case MYLITE_SQL_AST_UNARY_EXPRESSION: {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);
        const struct mylite_sql_ast_node *literal =
            mylite_execution_unwrap_parenthesized_expression(
                mylite_execution_child_at(expression, 0U)
            );

        if ((operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE ||
             operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) &&
            literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL &&
            mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_INTEGER) {
            return charset_collation_select_result(function_kind, charset, collation, out_result);
        }
        break;
    }
    case MYLITE_SQL_AST_UUID_FUNCTION:
    case MYLITE_SQL_AST_DATABASE_FUNCTION:
    case MYLITE_SQL_AST_SCHEMA_FUNCTION:
        return charset_collation_select_result(
            function_kind,
            mylite_execution_national_character_set_name(),
            mylite_execution_national_collation_name(),
            out_result
        );
    case MYLITE_SQL_AST_RAND_FUNCTION:
    case MYLITE_SQL_AST_RAND_SEED_FUNCTION: {
        int rc = charset_collation_rand_result(database, expression, &charset, &collation);

        if (rc != MYLITE_OK) {
            return rc;
        }
        return charset_collation_select_result(function_kind, charset, collation, out_result);
    }
    case MYLITE_SQL_AST_CAST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION:
        return charset_collation_select_result(function_kind, charset, collation, out_result);
    case MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION:
        return charset_collation_convert_using_charset_result(
            database,
            function_kind,
            expression,
            out_result
        );
    case MYLITE_SQL_AST_CHAR_FUNCTION: {
        struct scalar_convert_charset_info info = {0};
        int rc =
            mylite_execution_scalar_char_charset_info_for_expression(database, expression, &info);

        if (rc != MYLITE_OK) {
            return rc;
        }
        return charset_collation_select_result(
            function_kind,
            info.charset,
            info.collation,
            out_result
        );
    }
    case MYLITE_SQL_AST_COLLATE_EXPRESSION:
        return charset_collation_collate_expression_result(
            database,
            function_kind,
            expression,
            out_result
        );
    case MYLITE_SQL_AST_CONCAT_FUNCTION:
        return charset_collation_concat_scalar_result(
            database,
            function_kind,
            expression,
            out_result
        );
    default:
        break;
    }

    mylite_execution_set_unsupported_error(
        database,
        "CHARSET(), COLLATION(), and COERCIBILITY() support only scalar metadata arguments"
    );
    return MYLITE_ERROR;
}

static int coercibility_scalar_result(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char **out_result
) {
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression != NULL && expression->kind == MYLITE_SQL_AST_CONCAT_FUNCTION) {
        return coercibility_concat_scalar_result(database, expression, out_result);
    }
    return coercibility_non_concat_scalar_result(database, expression, out_result);
}

static int coercibility_non_concat_scalar_result(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char **out_result
) {
    if (out_result == NULL) {
        return MYLITE_MISUSE;
    }
    *out_result = NULL;
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        mylite_execution_set_unsupported_error(
            database,
            "COERCIBILITY() supports only scalar metadata arguments"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_IDENTIFIER ||
        expression->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return mylite_execution_set_unknown_column_reference_error(database, expression);
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_LITERAL:
        return coercibility_literal_result(database, expression, out_result);
    case MYLITE_SQL_AST_UNARY_EXPRESSION: {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);
        const struct mylite_sql_ast_node *literal =
            mylite_execution_unwrap_parenthesized_expression(
                mylite_execution_child_at(expression, 0U)
            );

        if ((operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE ||
             operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) &&
            literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL &&
            mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_INTEGER) {
            *out_result = "5";
            return MYLITE_OK;
        }
        break;
    }
    case MYLITE_SQL_AST_DATABASE_FUNCTION:
    case MYLITE_SQL_AST_SCHEMA_FUNCTION:
    case MYLITE_SQL_AST_VERSION_FUNCTION:
        *out_result = "3";
        return MYLITE_OK;
    case MYLITE_SQL_AST_UUID_FUNCTION:
        *out_result = "4";
        return MYLITE_OK;
    case MYLITE_SQL_AST_RAND_FUNCTION:
    case MYLITE_SQL_AST_RAND_SEED_FUNCTION: {
        const char *charset = NULL;
        const char *collation = NULL;
        int rc = charset_collation_rand_result(database, expression, &charset, &collation);

        if (rc != MYLITE_OK) {
            return rc;
        }
        *out_result = "5";
        return MYLITE_OK;
    }
    case MYLITE_SQL_AST_CAST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION:
        return coercibility_binary_wrapper_result(database, expression, out_result);
    case MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION: {
        struct session_scalar_cell cell = {0};
        int rc = mylite_execution_convert_using_charset_value(database, expression, &cell);

        mylite_execution_session_scalar_cell_deinit(&cell);
        if (rc != MYLITE_OK) {
            return rc;
        }
        *out_result = "2";
        return MYLITE_OK;
    }
    case MYLITE_SQL_AST_CHAR_FUNCTION: {
        struct scalar_convert_charset_info info = {0};
        int rc =
            mylite_execution_scalar_char_charset_info_for_expression(database, expression, &info);

        if (rc != MYLITE_OK) {
            return rc;
        }
        *out_result = "4";
        return MYLITE_OK;
    }
    case MYLITE_SQL_AST_COLLATE_EXPRESSION: {
        struct session_scalar_cell cell = {0};
        int rc = mylite_execution_collate_expression_value(database, expression, &cell);

        mylite_execution_session_scalar_cell_deinit(&cell);
        if (rc != MYLITE_OK) {
            return rc;
        }
        *out_result = "0";
        return MYLITE_OK;
    }
    default:
        break;
    }

    mylite_execution_set_unsupported_error(
        database,
        "COERCIBILITY() supports only scalar metadata arguments"
    );
    return MYLITE_ERROR;
}

static int coercibility_concat_scalar_result(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char **out_result
) {
    const struct mylite_sql_ast_node *arguments = mylite_execution_child_at(expression, 0U);
    const struct mylite_sql_ast_node *argument = NULL;
    size_t argument_count = 0U;
    const char *result = "6";
    bool has_non_null_argument = false;
    const struct mylite_execution_catalog_scalar_collation *explicit_collation = NULL;

    if (out_result == NULL) {
        return MYLITE_MISUSE;
    }
    *out_result = NULL;

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_CONCAT_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U || arguments == NULL ||
        arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) {
        mylite_execution_set_unsupported_error(database, "CONCAT() requires one or more arguments");
        return MYLITE_ERROR;
    }

    argument_count = mylite_sql_ast_node_child_count(arguments);
    if (argument_count == 0U) {
        mylite_execution_set_unsupported_error(database, "CONCAT() requires one or more arguments");
        return MYLITE_ERROR;
    }

    argument = mylite_execution_child_at(arguments, 0U);
    for (size_t argument_index = 0U; argument_index < argument_count && argument != NULL;
         ++argument_index) {
        int rc = apply_coercibility_concat_argument(
            database,
            argument,
            &result,
            &has_non_null_argument,
            &explicit_collation
        );

        if (rc != MYLITE_OK) {
            return rc;
        }
        argument = argument->next_sibling;
    }
    if (argument != NULL) {
        mylite_execution_set_parse_error(database);
        return MYLITE_ERROR;
    }

    *out_result = result;
    return MYLITE_OK;
}

static int apply_coercibility_concat_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *argument,
    const char **inout_result,
    bool *inout_has_non_null_argument,
    const struct mylite_execution_catalog_scalar_collation **inout_explicit_collation
) {
    const struct mylite_sql_ast_node *unwrapped_argument =
        mylite_execution_unwrap_parenthesized_expression(argument);
    const char *argument_result = NULL;
    int rc = MYLITE_OK;

    if (inout_result == NULL || inout_has_non_null_argument == NULL ||
        inout_explicit_collation == NULL) {
        return MYLITE_MISUSE;
    }

    if (unwrapped_argument != NULL &&
        (unwrapped_argument->kind == MYLITE_SQL_AST_IDENTIFIER ||
         unwrapped_argument->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) {
        mylite_execution_set_unsupported_error(
            database,
            "COERCIBILITY() supports only scalar metadata arguments"
        );
        return MYLITE_ERROR;
    }

    rc = coercibility_non_concat_scalar_result(database, argument, &argument_result);
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (unwrapped_argument != NULL &&
        unwrapped_argument->kind == MYLITE_SQL_AST_COLLATE_EXPRESSION) {
        const struct mylite_execution_catalog_scalar_collation *argument_explicit_collation = NULL;

        rc = scalar_collation_info_for_expression(
            database,
            unwrapped_argument,
            &argument_explicit_collation
        );
        if (rc == MYLITE_OK) {
            rc = merge_concat_explicit_collation(
                database,
                inout_explicit_collation,
                argument_explicit_collation
            );
        }
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    if (argument_result == NULL || strcmp(argument_result, "6") == 0) {
        return MYLITE_OK;
    }
    argument_result = coercibility_concat_argument_result(argument_result);
    if (!*inout_has_non_null_argument || argument_result[0] < (*inout_result)[0]) {
        *inout_result = argument_result;
    }
    *inout_has_non_null_argument = true;
    return MYLITE_OK;
}

static int coercibility_binary_wrapper_result(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char **out_result
) {
    const struct mylite_sql_ast_node *argument = NULL;
    int rc = MYLITE_OK;

    if (out_result == NULL) {
        return MYLITE_MISUSE;
    }
    *out_result = NULL;
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || mylite_sql_ast_node_child_count(expression) != 1U) {
        mylite_execution_set_unsupported_error(
            database,
            "COERCIBILITY() supports only scalar metadata arguments"
        );
        return MYLITE_ERROR;
    }

    argument =
        mylite_execution_unwrap_parenthesized_expression(mylite_execution_child_at(expression, 0U));
    if (argument != NULL && (argument->kind == MYLITE_SQL_AST_IDENTIFIER ||
                             argument->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) {
        return set_unknown_column_for_reference(database, argument);
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_CAST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION:
        rc = coercibility_validate_binary_wrapper_argument(database, argument);
        break;
    default:
        mylite_execution_set_unsupported_error(
            database,
            "COERCIBILITY() supports only scalar metadata arguments"
        );
        return MYLITE_ERROR;
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    *out_result = "2";
    return MYLITE_OK;
}

static int coercibility_validate_binary_wrapper_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *argument
) {
    const char *ignored_result = NULL;

    argument = mylite_execution_unwrap_parenthesized_expression(argument);
    if (argument == NULL) {
        mylite_execution_set_unsupported_error(
            database,
            "COERCIBILITY() supports only scalar metadata arguments"
        );
        return MYLITE_ERROR;
    }
    if (argument->kind == MYLITE_SQL_AST_LITERAL) {
        return coercibility_literal_result(database, argument, &ignored_result);
    }
    if (argument->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(argument);
        const struct mylite_sql_ast_node *literal =
            mylite_execution_unwrap_parenthesized_expression(mylite_execution_child_at(argument, 0U)
            );

        if ((operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE ||
             operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) &&
            literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL &&
            mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_INTEGER) {
            return MYLITE_OK;
        }
        mylite_execution_set_unsupported_error(
            database,
            "COERCIBILITY() supports only scalar metadata arguments"
        );
        return MYLITE_ERROR;
    }
    if (argument->kind == MYLITE_SQL_AST_DATABASE_FUNCTION ||
        argument->kind == MYLITE_SQL_AST_SCHEMA_FUNCTION ||
        argument->kind == MYLITE_SQL_AST_VERSION_FUNCTION) {
        return MYLITE_OK;
    }
    if (argument->kind == MYLITE_SQL_AST_RAND_FUNCTION ||
        argument->kind == MYLITE_SQL_AST_RAND_SEED_FUNCTION) {
        const char *charset = NULL;
        const char *collation = NULL;

        return charset_collation_rand_result(database, argument, &charset, &collation);
    }
    if (argument->kind == MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION) {
        struct session_scalar_cell cell = {0};
        int rc = mylite_execution_convert_using_charset_value(database, argument, &cell);

        mylite_execution_session_scalar_cell_deinit(&cell);
        return rc;
    }
    if (argument->kind == MYLITE_SQL_AST_CHAR_FUNCTION) {
        struct scalar_convert_charset_info info = {0};

        return mylite_execution_scalar_char_charset_info_for_expression(database, argument, &info);
    }

    mylite_execution_set_unsupported_error(
        database,
        "COERCIBILITY() supports only scalar metadata arguments"
    );
    return MYLITE_ERROR;
}

static int coercibility_literal_result(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char **out_result
) {
    if (out_result == NULL) {
        return MYLITE_MISUSE;
    }
    switch (mylite_sql_ast_node_literal_kind(expression)) {
    case MYLITE_SQL_AST_LITERAL_STRING:
    case MYLITE_SQL_AST_LITERAL_NATIONAL_STRING:
    case MYLITE_SQL_AST_LITERAL_HEX:
    case MYLITE_SQL_AST_LITERAL_BIT:
        *out_result = "4";
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_INTEGER:
    case MYLITE_SQL_AST_LITERAL_TRUE:
    case MYLITE_SQL_AST_LITERAL_FALSE:
        *out_result = "5";
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_NULL:
        *out_result = "6";
        return MYLITE_OK;
    case MYLITE_SQL_AST_LITERAL_NONE:
    case MYLITE_SQL_AST_LITERAL_DECIMAL:
    case MYLITE_SQL_AST_LITERAL_FLOAT:
        break;
    }
    mylite_execution_set_unsupported_error(
        database,
        "COERCIBILITY() supports only scalar metadata arguments"
    );
    return MYLITE_ERROR;
}

static const char *coercibility_concat_argument_result(const char *argument_result) {
    if (argument_result != NULL && strcmp(argument_result, "5") == 0) {
        return "4";
    }
    return argument_result;
}

static bool coercibility_binary_wrapper_column_reference(
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node **out_column_reference
) {
    const struct mylite_sql_ast_node *argument = NULL;

    if (out_column_reference == NULL) {
        return false;
    }
    *out_column_reference = NULL;
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || mylite_sql_ast_node_child_count(expression) != 1U) {
        return false;
    }
    switch (expression->kind) {
    case MYLITE_SQL_AST_CAST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION:
        break;
    default:
        return false;
    }

    argument =
        mylite_execution_unwrap_parenthesized_expression(mylite_execution_child_at(expression, 0U));
    if (argument == NULL || (argument->kind != MYLITE_SQL_AST_IDENTIFIER &&
                             argument->kind != MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) {
        return false;
    }
    *out_column_reference = argument;
    return true;
}

static int set_unknown_column_for_reference(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
) {
    return mylite_execution_set_unknown_column_reference_error(database, expression);
}

static int charset_collation_concat_scalar_result(
    struct mylite_db *database,
    enum planned_charset_collation_function_kind function_kind,
    const struct mylite_sql_ast_node *expression,
    const char **out_result
) {
    const struct mylite_sql_ast_node *arguments = mylite_execution_child_at(expression, 0U);
    const struct mylite_sql_ast_node *argument = NULL;
    size_t argument_count = 0U;
    bool has_binary_argument = false;
    const struct mylite_execution_catalog_scalar_collation *explicit_collation = NULL;

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_CONCAT_FUNCTION ||
        mylite_sql_ast_node_child_count(expression) != 1U || arguments == NULL ||
        arguments->kind != MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST) {
        mylite_execution_set_unsupported_error(database, "CONCAT() requires one or more arguments");
        return MYLITE_ERROR;
    }

    argument_count = mylite_sql_ast_node_child_count(arguments);
    if (argument_count == 0U) {
        mylite_execution_set_unsupported_error(database, "CONCAT() requires one or more arguments");
        return MYLITE_ERROR;
    }

    argument = mylite_execution_child_at(arguments, 0U);
    for (size_t argument_index = 0U; argument_index < argument_count && argument != NULL;
         ++argument_index) {
        bool is_binary_argument = false;
        const struct mylite_execution_catalog_scalar_collation *argument_explicit_collation = NULL;
        int rc = MYLITE_OK;

        rc = validate_charset_collation_concat_argument(
            database,
            argument,
            &is_binary_argument,
            &argument_explicit_collation
        );
        if (rc == MYLITE_OK) {
            rc = merge_concat_explicit_collation(
                database,
                &explicit_collation,
                argument_explicit_collation
            );
        }
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (is_binary_argument) {
            has_binary_argument = true;
        }
        argument = argument->next_sibling;
    }
    if (argument != NULL) {
        mylite_execution_set_parse_error(database);
        return MYLITE_ERROR;
    }

    if (explicit_collation != NULL) {
        return charset_collation_select_result(
            function_kind,
            explicit_collation->charset,
            explicit_collation->collation,
            out_result
        );
    }
    if (has_binary_argument) {
        return charset_collation_select_result(function_kind, "binary", "binary", out_result);
    }
    return charset_collation_select_result(
        function_kind,
        database->session.character_set_connection,
        database->session.collation_connection,
        out_result
    );
}

static int validate_charset_collation_concat_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool *out_is_binary_string_argument,
    const struct mylite_execution_catalog_scalar_collation **out_explicit_collation
) {
    const struct mylite_sql_ast_node *literal = NULL;

    if (out_is_binary_string_argument == NULL || out_explicit_collation == NULL) {
        return MYLITE_MISUSE;
    }
    *out_is_binary_string_argument = false;
    *out_explicit_collation = NULL;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        mylite_execution_set_unsupported_error(
            database,
            "CHARSET() and COLLATION() support only scalar metadata arguments"
        );
        return MYLITE_ERROR;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_LITERAL:
        *out_is_binary_string_argument =
            mylite_sql_ast_node_literal_kind(expression) == MYLITE_SQL_AST_LITERAL_HEX;
        return MYLITE_OK;
    case MYLITE_SQL_AST_UNARY_EXPRESSION: {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);

        literal = mylite_execution_unwrap_parenthesized_expression(
            mylite_execution_child_at(expression, 0U)
        );
        if ((operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE ||
             operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) &&
            literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL &&
            mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_INTEGER) {
            return MYLITE_OK;
        }
        break;
    }
    case MYLITE_SQL_AST_DATABASE_FUNCTION:
    case MYLITE_SQL_AST_SCHEMA_FUNCTION:
    case MYLITE_SQL_AST_UUID_FUNCTION:
        return MYLITE_OK;
    case MYLITE_SQL_AST_RAND_FUNCTION:
    case MYLITE_SQL_AST_RAND_SEED_FUNCTION: {
        const char *charset = NULL;
        const char *collation = NULL;

        return charset_collation_rand_result(database, expression, &charset, &collation);
    }
    case MYLITE_SQL_AST_CAST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION:
        *out_is_binary_string_argument = true;
        return MYLITE_OK;
    case MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION: {
        struct session_scalar_cell cell = {0};
        int rc = mylite_execution_convert_using_charset_value(database, expression, &cell);

        mylite_execution_session_scalar_cell_deinit(&cell);
        return rc;
    }
    case MYLITE_SQL_AST_CHAR_FUNCTION: {
        struct scalar_convert_charset_info info = {0};
        int rc =
            mylite_execution_scalar_char_charset_info_for_expression(database, expression, &info);

        if (rc != MYLITE_OK) {
            return rc;
        }
        *out_is_binary_string_argument =
            mylite_execution_text_equals_ascii_case_insensitive(info.charset, "binary");
        return MYLITE_OK;
    }
    case MYLITE_SQL_AST_COLLATE_EXPRESSION: {
        const struct mylite_execution_catalog_scalar_collation *collation_info = NULL;
        struct session_scalar_cell cell = {0};
        int rc = scalar_collation_info_for_expression(database, expression, &collation_info);

        if (rc == MYLITE_OK) {
            rc = mylite_execution_collate_expression_value(database, expression, &cell);
        }
        mylite_execution_session_scalar_cell_deinit(&cell);
        if (rc != MYLITE_OK) {
            return rc;
        }
        *out_explicit_collation = collation_info;
        return MYLITE_OK;
    }
    default:
        break;
    }

    mylite_execution_set_unsupported_error(
        database,
        "CHARSET() and COLLATION() support only scalar metadata arguments"
    );
    return MYLITE_ERROR;
}

static int merge_concat_explicit_collation(
    struct mylite_db *database,
    const struct mylite_execution_catalog_scalar_collation **inout_explicit_collation,
    const struct mylite_execution_catalog_scalar_collation *argument_collation
) {
    if (inout_explicit_collation == NULL) {
        return MYLITE_MISUSE;
    }
    if (argument_collation == NULL) {
        return MYLITE_OK;
    }
    if (*inout_explicit_collation == NULL) {
        *inout_explicit_collation = argument_collation;
        return MYLITE_OK;
    }
    if (mylite_execution_text_equals_ascii_case_insensitive(
            (*inout_explicit_collation)->collation,
            argument_collation->collation
        )) {
        return MYLITE_OK;
    }

    mylite_execution_set_illegal_mix_of_collations_error(
        database,
        (*inout_explicit_collation)->collation,
        argument_collation->collation,
        "concat"
    );
    return MYLITE_ERROR;
}

static int charset_collation_convert_using_charset_result(
    struct mylite_db *database,
    enum planned_charset_collation_function_kind function_kind,
    const struct mylite_sql_ast_node *expression,
    const char **out_result
) {
    struct session_scalar_cell cell = {0};
    struct scalar_convert_charset_info info = {0};
    int rc =
        mylite_execution_scalar_convert_charset_info_for_expression(database, expression, &info);

    if (rc == MYLITE_OK) {
        rc = mylite_execution_convert_using_charset_value(database, expression, &cell);
    }
    mylite_execution_session_scalar_cell_deinit(&cell);
    if (rc != MYLITE_OK) {
        return rc;
    }
    return charset_collation_select_result(function_kind, info.charset, info.collation, out_result);
}

static int charset_collation_collate_expression_result(
    struct mylite_db *database,
    enum planned_charset_collation_function_kind function_kind,
    const struct mylite_sql_ast_node *expression,
    const char **out_result
) {
    const char *charset = NULL;
    const char *collation = NULL;
    struct session_scalar_cell cell = {0};
    int rc =
        scalar_expression_charset_collation_metadata(database, expression, &charset, &collation);

    if (rc == MYLITE_OK) {
        rc = mylite_execution_collate_expression_value(database, expression, &cell);
    }
    mylite_execution_session_scalar_cell_deinit(&cell);
    if (rc != MYLITE_OK) {
        return rc;
    }
    return charset_collation_select_result(function_kind, charset, collation, out_result);
}

static int charset_collation_rand_result(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char **out_charset,
    const char **out_collation
) {
    if (out_charset == NULL || out_collation == NULL) {
        return MYLITE_MISUSE;
    }
    *out_charset = "binary";
    *out_collation = "binary";
    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        mylite_execution_set_unsupported_error(
            database,
            "RAND() supports only RAND() and RAND(seed)"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_RAND_FUNCTION &&
        mylite_sql_ast_node_child_count(expression) == 0U) {
        return MYLITE_OK;
    }
    if (expression->kind == MYLITE_SQL_AST_RAND_SEED_FUNCTION &&
        mylite_sql_ast_node_child_count(expression) == 1U) {
        uint32_t seed = 0U;

        return mylite_execution_rand_seed_value(
            database,
            mylite_execution_child_at(expression, 0U),
            &seed
        );
    }

    mylite_execution_set_unsupported_error(database, "RAND() supports only RAND() and RAND(seed)");
    return MYLITE_ERROR;
}

static int scalar_expression_charset_collation_metadata(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char **out_charset,
    const char **out_collation
) {
    const struct mylite_execution_catalog_scalar_collation *collation_info = NULL;
    int rc = MYLITE_OK;

    if (out_charset == NULL || out_collation == NULL) {
        return MYLITE_MISUSE;
    }
    *out_charset = "binary";
    *out_collation = "binary";

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        mylite_execution_set_unsupported_error(
            database,
            "COLLATE supports only scalar values with known character set metadata"
        );
        return MYLITE_ERROR;
    }

    if (expression->kind != MYLITE_SQL_AST_COLLATE_EXPRESSION) {
        return scalar_expression_base_charset_collation_metadata(
            database,
            expression,
            out_charset,
            out_collation
        );
    }

    rc = scalar_collation_info_for_expression(database, expression, &collation_info);
    if (rc != MYLITE_OK) {
        return rc;
    }
    *out_charset = collation_info->charset;
    *out_collation = collation_info->collation;
    return MYLITE_OK;
}

static int scalar_collation_info_for_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_execution_catalog_scalar_collation **out_info
) {
    const struct mylite_sql_ast_node *collation = NULL;
    const struct mylite_execution_catalog_scalar_collation *collation_info = NULL;
    const char *charset = NULL;
    const char *current_collation = NULL;
    char collation_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = MYLITE_OK;

    if (out_info == NULL) {
        return MYLITE_MISUSE;
    }
    *out_info = NULL;

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_COLLATE_EXPRESSION ||
        mylite_sql_ast_node_child_count(expression) != 2U) {
        mylite_execution_set_unsupported_error(
            database,
            "COLLATE supports only scalar postfix collation"
        );
        return MYLITE_ERROR;
    }

    rc = scalar_expression_base_charset_collation_metadata(
        database,
        mylite_execution_child_at(expression, 0U),
        &charset,
        &current_collation
    );
    (void)current_collation;
    if (rc != MYLITE_OK) {
        return rc;
    }

    collation = mylite_execution_child_at(expression, 1U);
    rc = mylite_execution_copy_identifier_name_text(
        database,
        collation,
        collation_name,
        sizeof(collation_name),
        "collation",
        "COLLATE names do not support NUL bytes"
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    collation_info = scalar_collation_info_by_name(collation_name);
    if (collation_info == NULL) {
        mylite_execution_set_unknown_collation_error(database, collation_name);
        return MYLITE_ERROR;
    }
    if (!mylite_execution_text_equals_ascii_case_insensitive(charset, collation_info->charset)) {
        mylite_execution_set_collation_not_valid_for_charset_error(
            database,
            collation_info->collation,
            charset
        );
        return MYLITE_ERROR;
    }

    *out_info = collation_info;
    return MYLITE_OK;
}

static int scalar_expression_base_charset_collation_metadata(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char **out_charset,
    const char **out_collation
) {
    if (out_charset == NULL || out_collation == NULL) {
        return MYLITE_MISUSE;
    }
    *out_charset = "binary";
    *out_collation = "binary";

    expression = mylite_execution_unwrap_parenthesized_expression(expression);
    if (expression == NULL) {
        mylite_execution_set_unsupported_error(
            database,
            "COLLATE supports only scalar values with known character set metadata"
        );
        return MYLITE_ERROR;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_LITERAL:
        switch (mylite_sql_ast_node_literal_kind(expression)) {
        case MYLITE_SQL_AST_LITERAL_STRING:
        case MYLITE_SQL_AST_LITERAL_NATIONAL_STRING:
        case MYLITE_SQL_AST_LITERAL_INTEGER:
        case MYLITE_SQL_AST_LITERAL_TRUE:
        case MYLITE_SQL_AST_LITERAL_FALSE:
            *out_charset = database->session.character_set_connection;
            *out_collation = database->session.collation_connection;
            break;
        case MYLITE_SQL_AST_LITERAL_NULL:
        case MYLITE_SQL_AST_LITERAL_HEX:
        case MYLITE_SQL_AST_LITERAL_BIT:
            *out_charset = "binary";
            *out_collation = "binary";
            break;
        case MYLITE_SQL_AST_LITERAL_NONE:
        case MYLITE_SQL_AST_LITERAL_DECIMAL:
        case MYLITE_SQL_AST_LITERAL_FLOAT:
            mylite_execution_set_unsupported_error(
                database,
                "COLLATE supports only scalar values with known character set metadata"
            );
            return MYLITE_ERROR;
        }
        return MYLITE_OK;
    case MYLITE_SQL_AST_UNARY_EXPRESSION: {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);
        const struct mylite_sql_ast_node *literal =
            mylite_execution_unwrap_parenthesized_expression(
                mylite_execution_child_at(expression, 0U)
            );

        if ((operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE ||
             operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) &&
            literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL &&
            mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_INTEGER) {
            *out_charset = database->session.character_set_connection;
            *out_collation = database->session.collation_connection;
            return MYLITE_OK;
        }
        break;
    }
    case MYLITE_SQL_AST_CAST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION:
        *out_charset = "binary";
        *out_collation = "binary";
        return MYLITE_OK;
    case MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION: {
        struct scalar_convert_charset_info info = {0};
        int rc = mylite_execution_scalar_convert_charset_info_for_expression(
            database,
            expression,
            &info
        );

        if (rc != MYLITE_OK) {
            return rc;
        }
        *out_charset = info.charset;
        *out_collation = info.collation;
        return MYLITE_OK;
    }
    case MYLITE_SQL_AST_CHAR_FUNCTION: {
        struct scalar_convert_charset_info info = {0};
        int rc =
            mylite_execution_scalar_char_charset_info_for_expression(database, expression, &info);

        if (rc != MYLITE_OK) {
            return rc;
        }
        *out_charset = info.charset;
        *out_collation = info.collation;
        return MYLITE_OK;
    }
    case MYLITE_SQL_AST_DATABASE_FUNCTION:
    case MYLITE_SQL_AST_SCHEMA_FUNCTION:
    case MYLITE_SQL_AST_UUID_FUNCTION:
        *out_charset = mylite_execution_national_character_set_name();
        *out_collation = mylite_execution_national_collation_name();
        return MYLITE_OK;
    case MYLITE_SQL_AST_MD5_FUNCTION:
    case MYLITE_SQL_AST_SHA_FUNCTION:
    case MYLITE_SQL_AST_SHA1_FUNCTION:
    case MYLITE_SQL_AST_SHA2_FUNCTION:
        *out_charset = database->session.character_set_connection;
        *out_collation = database->session.collation_connection;
        return MYLITE_OK;
    default:
        break;
    }

    mylite_execution_set_unsupported_error(
        database,
        "COLLATE supports only scalar values with known character set metadata"
    );
    return MYLITE_ERROR;
}

static const struct mylite_execution_catalog_scalar_collation *scalar_collation_info_by_name(
    const char *collation_name
) {
    return mylite_execution_catalog_scalar_collation_info_by_name(collation_name);
}

static enum planned_charset_collation_function_kind charset_collation_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
) {
    switch (ast_kind) {
    case MYLITE_SQL_AST_CHARSET_FUNCTION:
        return PLANNED_CHARSET_COLLATION_FUNCTION_CHARSET;
    case MYLITE_SQL_AST_COLLATION_FUNCTION:
        return PLANNED_CHARSET_COLLATION_FUNCTION_COLLATION;
    case MYLITE_SQL_AST_COERCIBILITY_FUNCTION:
        return PLANNED_CHARSET_COLLATION_FUNCTION_COERCIBILITY;
    default:
        return PLANNED_CHARSET_COLLATION_FUNCTION_NONE;
    }
}

static bool is_charset_collation_function_kind(enum mylite_sql_ast_node_kind ast_kind) {
    return charset_collation_function_kind(ast_kind) != PLANNED_CHARSET_COLLATION_FUNCTION_NONE;
}

static int charset_collation_select_result(
    enum planned_charset_collation_function_kind function_kind,
    const char *charset,
    const char *collation,
    const char **out_result
) {
    if (out_result == NULL || charset == NULL || collation == NULL) {
        return MYLITE_MISUSE;
    }
    switch (function_kind) {
    case PLANNED_CHARSET_COLLATION_FUNCTION_CHARSET:
        *out_result = charset;
        return MYLITE_OK;
    case PLANNED_CHARSET_COLLATION_FUNCTION_COLLATION:
        *out_result = collation;
        return MYLITE_OK;
    case PLANNED_CHARSET_COLLATION_FUNCTION_NONE:
    case PLANNED_CHARSET_COLLATION_FUNCTION_COERCIBILITY:
        break;
    }
    return MYLITE_ERROR;
}
