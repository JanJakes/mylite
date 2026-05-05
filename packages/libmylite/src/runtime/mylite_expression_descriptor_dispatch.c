#include "mylite_expression_descriptor_dispatch.h"

#include "mylite_expression_descriptor.h"
#include "mylite_expression_descriptor_aggregate.h"
#include "mylite_expression_descriptor_basic.h"
#include "mylite_expression_descriptor_case.h"
#include "mylite_expression_descriptor_cast.h"
#include "mylite_expression_descriptor_function.h"
#include "mylite_expression_descriptor_operator.h"
#include "mylite_expression_descriptor_subquery.h"
#include "mylite_expression_descriptor_temporal.h"
#include "mylite_select_context.h"
#include "mylite_span.h"

static const struct mylite_expression_descriptor_aggregate_callbacks
    aggregate_descriptor_callbacks = {
        .infer_expression_descriptor = mylite_expression_descriptor_infer,
};

static const struct mylite_expression_descriptor_case_callbacks case_descriptor_callbacks = {
    .infer_expression_descriptor = mylite_expression_descriptor_infer,
};

static const struct mylite_expression_descriptor_cast_callbacks cast_descriptor_callbacks = {
    .infer_expression_descriptor = mylite_expression_descriptor_infer,
};

static const struct mylite_expression_descriptor_subquery_callbacks subquery_descriptor_callbacks =
    {
        .infer_expression_descriptor = mylite_expression_descriptor_infer,
        .prepare_select_subquery = mylite_select_context_prepare_subquery,
        .bind_callbacks = &mylite_select_context_subquery_bind_callbacks,
};

static const struct mylite_expression_descriptor_operator_callbacks operator_descriptor_callbacks =
    {
        .infer_expression_descriptor = mylite_expression_descriptor_infer,
        .subquery_callbacks = &subquery_descriptor_callbacks,
};

static const struct mylite_expression_descriptor_function_callbacks function_descriptor_callbacks =
    {
        .infer_expression_descriptor = mylite_expression_descriptor_infer,
};

// NOLINTNEXTLINE(misc-no-recursion)
int mylite_expression_descriptor_infer_select(mylite_db *database,
                                              const struct mylite_select_plan *plan,
                                              const struct mylite_sql_ast_node *expression,
                                              struct mylite_field_descriptor *out_descriptor)
{
    return mylite_expression_descriptor_infer(database, plan, expression, NULL, out_descriptor);
}

// NOLINTNEXTLINE(misc-no-recursion)
int mylite_expression_descriptor_infer_scalar(mylite_db *database,
                                              const struct mylite_sql_ast_node *expression,
                                              const struct mylite_expression_value *value,
                                              struct mylite_field_descriptor *out_descriptor)
{
    return mylite_expression_descriptor_infer(database, NULL, expression, value, out_descriptor);
}

// NOLINTNEXTLINE(misc-no-recursion)
int mylite_expression_descriptor_infer_collation(mylite_db *database,
                                                 const struct mylite_select_plan *plan,
                                                 const struct mylite_sql_ast_node *expression,
                                                 struct mylite_field_descriptor *out_descriptor)
{
    return mylite_expression_descriptor_infer(database, plan, expression, NULL, out_descriptor);
}

// NOLINTNEXTLINE(misc-no-recursion)
int mylite_expression_descriptor_infer(mylite_db *database, const struct mylite_select_plan *plan,
                                       const struct mylite_sql_ast_node *expression,
                                       const struct mylite_expression_value *value,
                                       struct mylite_field_descriptor *out_descriptor)
{
    const struct mylite_sql_ast_node *node = expression;

    if (out_descriptor == NULL) {
        return MYLITE_MISUSE;
    }
    while (node != NULL && node->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
        node = mylite_ast_child_at(node, 0U);
    }
    if (node == NULL) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return MYLITE_OK;
    }

    switch (node->kind) {
    case MYLITE_SQL_AST_LITERAL:
        return mylite_expression_descriptor_infer_literal(database, node, value, out_descriptor);
    case MYLITE_SQL_AST_IDENTIFIER:
    case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER:
        return mylite_expression_descriptor_infer_identifier(database, plan, node, out_descriptor);
    case MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION:
        break;
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
        return mylite_expression_descriptor_infer_unary_expression(
            database, plan, node, value, out_descriptor, &operator_descriptor_callbacks);
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
        return mylite_expression_descriptor_infer_binary_expression(
            database, plan, node, value, out_descriptor, &operator_descriptor_callbacks);
    case MYLITE_SQL_AST_TERNARY_EXPRESSION:
        return mylite_expression_descriptor_infer_ternary_expression(
            database, plan, node, value, out_descriptor, &operator_descriptor_callbacks);
    case MYLITE_SQL_AST_CASE_EXPRESSION:
        return mylite_expression_descriptor_infer_case_expression(
            database, plan, node, out_descriptor, &case_descriptor_callbacks);
    case MYLITE_SQL_AST_FUNCTION_CALL:
        return mylite_expression_descriptor_infer_function_expression(
            database, plan, node, value, out_descriptor, &function_descriptor_callbacks);
    case MYLITE_SQL_AST_AGGREGATE_CALL:
        return mylite_expression_descriptor_infer_aggregate(database, plan, node, out_descriptor);
    case MYLITE_SQL_AST_CAST_EXPRESSION:
        return mylite_expression_descriptor_infer_cast_expression(
            database, plan, node, value, out_descriptor, &cast_descriptor_callbacks);
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP: {
        unsigned int fsp = 0U;

        if (node->has_column_precision) {
            fsp = (unsigned int)node->column_precision;
        }
        *out_descriptor = mylite_expression_descriptor_current_datetime_function(fsp);
        return MYLITE_OK;
    }
    case MYLITE_SQL_AST_SUBQUERY_EXPRESSION:
    case MYLITE_SQL_AST_EXISTS_EXPRESSION:
    case MYLITE_SQL_AST_QUANTIFIED_COMPARISON:
        return mylite_expression_descriptor_infer_subquery_expression(
            database, plan, node, out_descriptor, &subquery_descriptor_callbacks);
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
    case MYLITE_SQL_AST_ROW_CONSTRUCTOR:
    case MYLITE_SQL_AST_QUERY_EXPRESSION:
    case MYLITE_SQL_AST_UNION_EXPRESSION:
    case MYLITE_SQL_AST_QUERY_PRIMARY:
    case MYLITE_SQL_AST_INSERT_DUPLICATE_UPDATE_CLAUSE:
    case MYLITE_SQL_AST_INSERT_UPDATE_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_INSERT_UPDATE_ASSIGNMENT:
    case MYLITE_SQL_AST_INSERT_ROW_ALIAS:
    case MYLITE_SQL_AST_INSERT_ALIAS_COLUMN_LIST:
        return MYLITE_UNSUPPORTED;
    case MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST:
    case MYLITE_SQL_AST_CASE_WHEN_LIST:
    case MYLITE_SQL_AST_CASE_WHEN:
    case MYLITE_SQL_AST_EXPRESSION_LIST:
    case MYLITE_SQL_AST_GROUP_BY_CLAUSE:
    case MYLITE_SQL_AST_GROUP_ITEM_LIST:
    case MYLITE_SQL_AST_GROUP_ITEM:
    case MYLITE_SQL_AST_HAVING_CLAUSE:
    case MYLITE_SQL_AST_SCRIPT:
    case MYLITE_SQL_AST_SELECT_STATEMENT:
    case MYLITE_SQL_AST_USE_STATEMENT:
    case MYLITE_SQL_AST_SELECT_LIST:
    case MYLITE_SQL_AST_SELECT_ITEM:
    case MYLITE_SQL_AST_WILDCARD:
    case MYLITE_SQL_AST_FROM_TABLE:
    case MYLITE_SQL_AST_FROM_TABLE_REFERENCES:
    case MYLITE_SQL_AST_TABLE_REFERENCE_LIST:
    case MYLITE_SQL_AST_JOIN_EXPRESSION:
    case MYLITE_SQL_AST_JOIN_CONDITION:
    case MYLITE_SQL_AST_USING_COLUMN_LIST:
    case MYLITE_SQL_AST_USING_COLUMN:
    case MYLITE_SQL_AST_FROM_DUAL:
    case MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_ALTER_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_SHOW_SCHEMAS_STATEMENT:
    case MYLITE_SQL_AST_IF_EXISTS:
    case MYLITE_SQL_AST_IF_NOT_EXISTS:
    case MYLITE_SQL_AST_SCHEMA_OPTION_LIST:
    case MYLITE_SQL_AST_SCHEMA_OPTION:
    case MYLITE_SQL_AST_DEFAULT:
    case MYLITE_SQL_AST_SET_NAMES_STATEMENT:
    case MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT:
    case MYLITE_SQL_AST_SET_SQL_MODE_STATEMENT:
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
        break;
    }

    *out_descriptor = mylite_expression_descriptor_from_value(value);
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
int mylite_expression_descriptor_infer_aggregate(mylite_db *database,
                                                 const struct mylite_select_plan *plan,
                                                 const struct mylite_sql_ast_node *expression,
                                                 struct mylite_field_descriptor *out_descriptor)
{
    return mylite_expression_descriptor_infer_aggregate_expression(
        database, plan, expression, out_descriptor, &aggregate_descriptor_callbacks);
}
