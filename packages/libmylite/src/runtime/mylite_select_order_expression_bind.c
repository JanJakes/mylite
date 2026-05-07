#include "mylite_select_order_expression_bind.h"

#include "mylite_expression_validation.h"
#include "mylite_select.h"
#include "mylite_select_aggregate_bind.h"
#include "mylite_select_order_bind.h"
#include "mylite_select_order_resolve.h"
#include "mylite_select_subquery.h"
#include "mylite_span.h"
#include "mylite_system_variables.h"
#include "mylite_user_variables.h"
#include "sql/mylite_ast.h"
#include "sql/mylite_expression.h"

static int bind_order_row_constructor(
    mylite_db *database,
    const struct mylite_sql_ast_node *row,
    struct mylite_select_plan *plan,
    const struct mylite_select_order_bind_callbacks *callbacks
);

static int bind_order_binary_expression(
    mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_select_plan *plan,
    const struct mylite_select_order_bind_callbacks *callbacks
);

static int bind_order_identifier_expression(
    mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_select_plan *plan
);

static int bind_order_in_subquery_expression(
    mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_select_plan *plan,
    const struct mylite_select_order_bind_callbacks *callbacks
);

static int bind_order_quantified_subquery_expression(
    mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_select_plan *plan,
    const struct mylite_select_order_bind_callbacks *callbacks
);

int mylite_select_bind_order_expression( // NOLINT(misc-no-recursion)
    mylite_db *database, const struct mylite_sql_ast_node *expression,
    struct mylite_select_plan *plan, const struct mylite_select_order_bind_callbacks *callbacks)
{
    if (expression == NULL) {
        return callbacks->set_unsupported_order_error(database);
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_LITERAL:
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP:
        return MYLITE_OK;
    case MYLITE_SQL_AST_IDENTIFIER:
    case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER:
        return bind_order_identifier_expression(database, expression, plan);
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
    case MYLITE_SQL_AST_TERNARY_EXPRESSION:
    case MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION:
    case MYLITE_SQL_AST_EXPRESSION_LIST:
    case MYLITE_SQL_AST_CASE_EXPRESSION:
    case MYLITE_SQL_AST_CASE_WHEN_LIST:
    case MYLITE_SQL_AST_CASE_WHEN:
        for (const struct mylite_sql_ast_node *child = expression->first_child; child != NULL;
             child = child->next_sibling) {
            int status = mylite_select_bind_order_expression(database, child, plan, callbacks);

            if (status != MYLITE_OK) {
                return status;
            }
        }
        return MYLITE_OK;
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
        return bind_order_binary_expression(database, expression, plan, callbacks);
    case MYLITE_SQL_AST_SUBQUERY_EXPRESSION:
    case MYLITE_SQL_AST_EXISTS_EXPRESSION:
        return mylite_select_subquery_bind_select_expression(
            database,
            expression,
            expression->kind == MYLITE_SQL_AST_SUBQUERY_EXPRESSION,
            callbacks->subquery_callbacks
        );
    case MYLITE_SQL_AST_QUANTIFIED_COMPARISON:
        return bind_order_quantified_subquery_expression(database, expression, plan, callbacks);
    case MYLITE_SQL_AST_CREATE_INDEX_STATEMENT:
    case MYLITE_SQL_AST_DROP_INDEX_STATEMENT:
    case MYLITE_SQL_AST_DDL_TABLE_OPTION_LIST:
    case MYLITE_SQL_AST_DDL_TABLE_OPTION:
    case MYLITE_SQL_AST_ALTER_TABLE_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_ITEM_LIST:
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION:
    case MYLITE_SQL_AST_ALTER_TABLE_COLUMN_POSITION:
    case MYLITE_SQL_AST_RENAME_TABLE_STATEMENT:
    case MYLITE_SQL_AST_TRUNCATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_SHOW_VARIABLES_STATEMENT:
    case MYLITE_SQL_AST_SHOW_STATUS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_ENGINES_STATEMENT:
    case MYLITE_SQL_AST_SHOW_CHARACTER_SET_STATEMENT:
    case MYLITE_SQL_AST_SHOW_COLLATION_STATEMENT:
    case MYLITE_SQL_AST_SHOW_TABLES_STATEMENT:
    case MYLITE_SQL_AST_SHOW_TABLE_STATUS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_INDEX_STATEMENT:
    case MYLITE_SQL_AST_SHOW_CREATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_SHOW_CREATE_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_SHOW_DIAGNOSTICS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_DIAGNOSTICS_COUNT_STATEMENT:
    case MYLITE_SQL_AST_DESCRIBE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_RENAME_TABLE_PAIR_LIST:
    case MYLITE_SQL_AST_RENAME_TABLE_PAIR:
    case MYLITE_SQL_AST_PLACEHOLDER_STATEMENT:
    case MYLITE_SQL_AST_ROW_CONSTRUCTOR:
    case MYLITE_SQL_AST_QUERY_EXPRESSION:
    case MYLITE_SQL_AST_UNION_EXPRESSION:
    case MYLITE_SQL_AST_QUERY_PRIMARY:
    case MYLITE_SQL_AST_VALUES_STATEMENT:
    case MYLITE_SQL_AST_WINDOW_FUNCTION_CALL:
    case MYLITE_SQL_AST_OVER_CLAUSE:
    case MYLITE_SQL_AST_WINDOW_SPECIFICATION:
    case MYLITE_SQL_AST_WINDOW_CLAUSE:
    case MYLITE_SQL_AST_WINDOW_DEFINITION_LIST:
    case MYLITE_SQL_AST_WINDOW_DEFINITION:
    case MYLITE_SQL_AST_WINDOW_PARTITION_CLAUSE:
    case MYLITE_SQL_AST_WINDOW_FRAME_CLAUSE:
    case MYLITE_SQL_AST_WINDOW_FRAME_BOUND:
    case MYLITE_SQL_AST_WINDOW_NULL_TREATMENT:
    case MYLITE_SQL_AST_INSERT_DUPLICATE_UPDATE_CLAUSE:
    case MYLITE_SQL_AST_INSERT_UPDATE_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_INSERT_UPDATE_ASSIGNMENT:
    case MYLITE_SQL_AST_INSERT_ROW_ALIAS:
    case MYLITE_SQL_AST_INSERT_ALIAS_COLUMN_LIST:
        return callbacks->set_unsupported_order_error(database);
    case MYLITE_SQL_AST_CAST_EXPRESSION: {
        int status = mylite_expression_validate_cast_target_charset(database, expression);

        if (status != MYLITE_OK) {
            return status;
        }
        return mylite_select_bind_order_expression(
            database,
            mylite_ast_child_at(expression, 0U),
            plan,
            callbacks
        );
    }
    case MYLITE_SQL_AST_FUNCTION_CALL: {
        const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);

        if (!mylite_expression_is_supported_function_call(expression)) {
            return callbacks->set_unsupported_order_error(database);
        }
        {
            int status = mylite_expression_validate_char_function_charset(database, expression);

            if (status != MYLITE_OK) {
                return status;
            }
        }
        for (const struct mylite_sql_ast_node *child = arguments == NULL ? NULL
                                                                         : arguments->first_child;
             child != NULL;
             child = child->next_sibling) {
            int status = mylite_select_bind_order_expression(database, child, plan, callbacks);

            if (status != MYLITE_OK) {
                return status;
            }
        }
        return MYLITE_OK;
    }
    case MYLITE_SQL_AST_AGGREGATE_CALL:
        return mylite_select_bind_aggregate_aware_expression(
            database,
            expression,
            plan,
            "order clause",
            callbacks->aggregate_callbacks
        );
    case MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST:
    case MYLITE_SQL_AST_SCRIPT:
    case MYLITE_SQL_AST_SELECT_STATEMENT:
    case MYLITE_SQL_AST_USE_STATEMENT:
    case MYLITE_SQL_AST_SELECT_LIST:
    case MYLITE_SQL_AST_SELECT_ITEM:
    case MYLITE_SQL_AST_FROM_DUAL:
    case MYLITE_SQL_AST_FROM_TABLE:
    case MYLITE_SQL_AST_FROM_TABLE_REFERENCES:
    case MYLITE_SQL_AST_TABLE_REFERENCE_LIST:
    case MYLITE_SQL_AST_JOIN_EXPRESSION:
    case MYLITE_SQL_AST_JOIN_CONDITION:
    case MYLITE_SQL_AST_USING_COLUMN_LIST:
    case MYLITE_SQL_AST_USING_COLUMN:
    case MYLITE_SQL_AST_WILDCARD:
    case MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_ALTER_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_SHOW_SCHEMAS_STATEMENT:
    case MYLITE_SQL_AST_IF_EXISTS:
    case MYLITE_SQL_AST_IF_NOT_EXISTS:
    case MYLITE_SQL_AST_SCHEMA_OPTION_LIST:
    case MYLITE_SQL_AST_SCHEMA_OPTION:
    case MYLITE_SQL_AST_SET_NAMES_STATEMENT:
    case MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT:
    case MYLITE_SQL_AST_SET_SYSTEM_VARIABLE_STATEMENT:
    case MYLITE_SQL_AST_SET_USER_VARIABLE_STATEMENT:
    case MYLITE_SQL_AST_USER_VARIABLE_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_USER_VARIABLE_ASSIGNMENT:
    case MYLITE_SQL_AST_PREPARE_STATEMENT:
    case MYLITE_SQL_AST_EXECUTE_STATEMENT:
    case MYLITE_SQL_AST_EXECUTE_USING_LIST:
    case MYLITE_SQL_AST_DEALLOCATE_PREPARE_STATEMENT:
    case MYLITE_SQL_AST_DEFAULT:
    case MYLITE_SQL_AST_CREATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_COLUMN_DEFINITION_LIST:
    case MYLITE_SQL_AST_COLUMN_DEFINITION:
    case MYLITE_SQL_AST_COLUMN_TYPE:
    case MYLITE_SQL_AST_COLUMN_TYPE_ATTRIBUTE_LIST:
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_LIST:
    case MYLITE_SQL_AST_COLUMN_ATTRIBUTE:
    case MYLITE_SQL_AST_PRIMARY_KEY_CONSTRAINT:
    case MYLITE_SQL_AST_KEY_PART_LIST:
    case MYLITE_SQL_AST_KEY_PART:
    case MYLITE_SQL_AST_INDEX_TYPE:
    case MYLITE_SQL_AST_INDEX_OPTION_LIST:
    case MYLITE_SQL_AST_INDEX_OPTION:
    case MYLITE_SQL_AST_SECONDARY_INDEX:
    case MYLITE_SQL_AST_UNIQUE_INDEX:
    case MYLITE_SQL_AST_TABLE_OPTION_LIST:
    case MYLITE_SQL_AST_TABLE_OPTION:
    case MYLITE_SQL_AST_DROP_TABLE_STATEMENT:
    case MYLITE_SQL_AST_TABLE_NAME_LIST:
    case MYLITE_SQL_AST_INSERT_VALUES_STATEMENT:
    case MYLITE_SQL_AST_INSERT_SELECT_STATEMENT:
    case MYLITE_SQL_AST_INSERT_COLUMN_LIST:
    case MYLITE_SQL_AST_INSERT_ROW_LIST:
    case MYLITE_SQL_AST_INSERT_ROW:
    case MYLITE_SQL_AST_INSERT_VALUE_LIST:
    case MYLITE_SQL_AST_INSERT_SET_STATEMENT:
    case MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT:
    case MYLITE_SQL_AST_REPLACE_VALUES_STATEMENT:
    case MYLITE_SQL_AST_REPLACE_SET_STATEMENT:
    case MYLITE_SQL_AST_WHERE_CLAUSE:
    case MYLITE_SQL_AST_GROUP_BY_CLAUSE:
    case MYLITE_SQL_AST_GROUP_ITEM_LIST:
    case MYLITE_SQL_AST_GROUP_ITEM:
    case MYLITE_SQL_AST_HAVING_CLAUSE:
    case MYLITE_SQL_AST_ORDER_BY_CLAUSE:
    case MYLITE_SQL_AST_ORDER_ITEM_LIST:
    case MYLITE_SQL_AST_ORDER_ITEM:
    case MYLITE_SQL_AST_LIMIT_CLAUSE:
    case MYLITE_SQL_AST_LIMIT_BOUND:
    case MYLITE_SQL_AST_UPDATE_STATEMENT:
    case MYLITE_SQL_AST_UPDATE_TARGET:
    case MYLITE_SQL_AST_UPDATE_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_UPDATE_ASSIGNMENT:
    case MYLITE_SQL_AST_UPDATE_LIMIT_CLAUSE:
    case MYLITE_SQL_AST_DELETE_STATEMENT:
    case MYLITE_SQL_AST_DELETE_TARGET:
    case MYLITE_SQL_AST_DELETE_LIMIT_CLAUSE:
    case MYLITE_SQL_AST_DELETE_TARGET_LIST:
    case MYLITE_SQL_AST_DELETE_TARGET_NAME:
    case MYLITE_SQL_AST_START_TRANSACTION_STATEMENT:
    case MYLITE_SQL_AST_BEGIN_TRANSACTION_STATEMENT:
    case MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC_LIST:
    case MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC:
    case MYLITE_SQL_AST_COMMIT_STATEMENT:
    case MYLITE_SQL_AST_ROLLBACK_STATEMENT:
    case MYLITE_SQL_AST_TRANSACTION_COMPLETION:
    case MYLITE_SQL_AST_SAVEPOINT_STATEMENT:
    case MYLITE_SQL_AST_ROLLBACK_TO_SAVEPOINT_STATEMENT:
    case MYLITE_SQL_AST_RELEASE_SAVEPOINT_STATEMENT:
        return callbacks->set_unsupported_order_error(database);
    }

    return callbacks->set_unsupported_order_error(database);
}

static int bind_order_identifier_expression(
    mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_select_plan *plan
) {
    enum mylite_select_order_key_kind kind = MYLITE_SELECT_ORDER_KEY_EXPRESSION;
    size_t index = 0U;
    int status = MYLITE_OK;

    if (mylite_system_variable_identifier_is_system_variable(expression)) {
        return MYLITE_OK;
    }
    if (mylite_user_variable_identifier_is_user_variable(expression)) {
        return MYLITE_OK;
    }

    status = mylite_select_resolve_order_reference(database, plan, expression, &kind, &index);
    if (status == MYLITE_OK && kind == MYLITE_SELECT_ORDER_KEY_OUTPUT) {
        mylite_select_plan_mark_output_order_reference(plan, index);
    }
    return status;
}

static int bind_order_row_constructor( // NOLINT(misc-no-recursion)
    mylite_db *database, const struct mylite_sql_ast_node *row, struct mylite_select_plan *plan,
    const struct mylite_select_order_bind_callbacks *callbacks)
{
    if (row == NULL || row->kind != MYLITE_SQL_AST_ROW_CONSTRUCTOR) {
        return callbacks->set_unsupported_order_error(database);
    }
    for (const struct mylite_sql_ast_node *child = row->first_child; child != NULL;
         child = child->next_sibling) {
        int status = mylite_select_bind_order_expression(database, child, plan, callbacks);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int bind_order_binary_expression( // NOLINT(misc-no-recursion)
    mylite_db *database, const struct mylite_sql_ast_node *expression,
    struct mylite_select_plan *plan, const struct mylite_select_order_bind_callbacks *callbacks)
{
    if (mylite_select_subquery_binary_expression_is_row(expression)) {
        const struct mylite_sql_ast_node *left =
            mylite_sql_ast_unwrap_parenthesized_expression(mylite_ast_child_at(expression, 0U));
        int status = MYLITE_OK;

        if (left == NULL || left->kind != MYLITE_SQL_AST_ROW_CONSTRUCTOR) {
            return callbacks->set_unsupported_order_error(database);
        }
        status = bind_order_row_constructor(database, left, plan, callbacks);
        if (status != MYLITE_OK) {
            return status;
        }
        return mylite_select_subquery_bind_row_expression(
            database,
            expression,
            plan,
            callbacks->subquery_callbacks
        );
    }
    if (mylite_select_subquery_binary_expression_is_in(expression)) {
        return bind_order_in_subquery_expression(database, expression, plan, callbacks);
    }
    for (const struct mylite_sql_ast_node *child = expression->first_child; child != NULL;
         child = child->next_sibling) {
        int status = mylite_select_bind_order_expression(database, child, plan, callbacks);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    return MYLITE_OK;
}

static int bind_order_in_subquery_expression( // NOLINT(misc-no-recursion)
    mylite_db *database, const struct mylite_sql_ast_node *expression,
    struct mylite_select_plan *plan, const struct mylite_select_order_bind_callbacks *callbacks)
{
    const struct mylite_sql_ast_node *left = mylite_ast_child_at(expression, 0U);
    int status = MYLITE_OK;

    if (left == NULL || left->kind == MYLITE_SQL_AST_ROW_CONSTRUCTOR) {
        return callbacks->set_unsupported_order_error(database);
    }
    status = mylite_select_bind_order_expression(database, left, plan, callbacks);
    if (status != MYLITE_OK) {
        return status;
    }
    return mylite_select_subquery_bind_in_expression(
        database,
        expression,
        plan,
        callbacks->subquery_callbacks
    );
}

static int bind_order_quantified_subquery_expression( // NOLINT(misc-no-recursion)
    mylite_db *database, const struct mylite_sql_ast_node *expression,
    struct mylite_select_plan *plan, const struct mylite_select_order_bind_callbacks *callbacks)
{
    const struct mylite_sql_ast_node *left = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *unwrapped_left =
        mylite_sql_ast_unwrap_parenthesized_expression(left);
    int status = MYLITE_OK;

    if (mylite_select_subquery_quantified_comparison_is_row_alias(expression)) {
        status = bind_order_row_constructor(database, unwrapped_left, plan, callbacks);
        if (status != MYLITE_OK) {
            return status;
        }
        return mylite_select_subquery_bind_row_expression(
            database,
            expression,
            plan,
            callbacks->subquery_callbacks
        );
    }
    if (unwrapped_left == NULL) {
        return callbacks->set_unsupported_order_error(database);
    }
    if (unwrapped_left->kind == MYLITE_SQL_AST_ROW_CONSTRUCTOR) {
        return mylite_select_subquery_set_row_quantified_non_alias_error(database, expression);
    }
    status = mylite_select_bind_order_expression(database, left, plan, callbacks);
    if (status != MYLITE_OK) {
        return status;
    }
    return mylite_select_subquery_bind_quantified_expression(
        database,
        expression,
        plan,
        callbacks->subquery_callbacks
    );
}
