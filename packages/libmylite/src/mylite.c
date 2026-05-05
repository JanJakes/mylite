#include <mylite/mylite.h>

#include "mylite_expression.h"
#include "mylite_parser.h"
#include "mylite_sqlite_translator.h"
#include "runtime/mylite_catalog.h"
#include "runtime/mylite_connection.h"
#include "runtime/mylite_connection_statement.h"
#include "runtime/mylite_diagnostics.h"
#include "runtime/mylite_dml.h"
#include "runtime/mylite_dml_statement.h"
#include "runtime/mylite_dml_types.h"
#include "runtime/mylite_error_codes.h"
#include "runtime/mylite_expression_collation.h"
#include "runtime/mylite_expression_descriptor.h"
#include "runtime/mylite_expression_descriptor_aggregate.h"
#include "runtime/mylite_expression_descriptor_basic.h"
#include "runtime/mylite_expression_descriptor_case.h"
#include "runtime/mylite_expression_descriptor_cast.h"
#include "runtime/mylite_expression_descriptor_function.h"
#include "runtime/mylite_expression_descriptor_operator.h"
#include "runtime/mylite_expression_descriptor_subquery.h"
#include "runtime/mylite_expression_descriptor_temporal.h"
#include "runtime/mylite_field_descriptor.h"
#include "runtime/mylite_information_schema.h"
#include "runtime/mylite_metadata.h"
#include "runtime/mylite_metadata_types.h"
#include "runtime/mylite_runtime.h"
#include "runtime/mylite_schema.h"
#include "runtime/mylite_schema_types.h"
#include "runtime/mylite_select.h"
#include "runtime/mylite_select_aggregate.h"
#include "runtime/mylite_select_aggregate_bind.h"
#include "runtime/mylite_select_diagnostics.h"
#include "runtime/mylite_select_distinct_validate.h"
#include "runtime/mylite_select_eval.h"
#include "runtime/mylite_select_from.h"
#include "runtime/mylite_select_group.h"
#include "runtime/mylite_select_group_bind.h"
#include "runtime/mylite_select_group_validate.h"
#include "runtime/mylite_select_join_cache.h"
#include "runtime/mylite_select_materialize.h"
#include "runtime/mylite_select_metadata.h"
#include "runtime/mylite_select_order_bind.h"
#include "runtime/mylite_select_predicate_bind.h"
#include "runtime/mylite_select_prepare.h"
#include "runtime/mylite_select_projection.h"
#include "runtime/mylite_select_resolve.h"
#include "runtime/mylite_select_row_loader.h"
#include "runtime/mylite_select_rowset.h"
#include "runtime/mylite_select_scalar.h"
#include "runtime/mylite_select_sql.h"
#include "runtime/mylite_select_statement.h"
#include "runtime/mylite_select_subquery.h"
#include "runtime/mylite_select_types.h"
#include "runtime/mylite_select_union.h"
#include "runtime/mylite_show.h"
#include "runtime/mylite_show_types.h"
#include "runtime/mylite_span.h"
#include "runtime/mylite_sqlite_value.h"
#include "runtime/mylite_statement.h"
#include "runtime/mylite_statement_custom.h"
#include "runtime/mylite_statement_execute.h"
#include "runtime/mylite_statement_functions.h"
#include "runtime/mylite_statement_prepare.h"
#include "runtime/mylite_table_ddl.h"
#include "runtime/mylite_table_ddl_statement.h"
#include "runtime/mylite_table_ddl_types.h"
#include "runtime/mylite_temporal_functions.h"
#include "runtime/mylite_transaction_types.h"
#include "runtime/mylite_transactions.h"
#include "sql/mylite_lexer.h"
#include "sqlite3.h"
#include "types/mylite_column_type.h"

#include <stdbool.h>

static int prepare_select_subquery_statement(mylite_db *database,
                                             const struct mylite_sql_ast_node *statement,
                                             mylite_stmt **out_stmt);
static int infer_select_expression_descriptor(mylite_db *database,
                                              const struct mylite_select_plan *plan,
                                              const struct mylite_sql_ast_node *expression,
                                              struct mylite_field_descriptor *out_descriptor);
static int infer_scalar_expression_descriptor(mylite_db *database,
                                              const struct mylite_sql_ast_node *expression,
                                              const struct mylite_expression_value *value,
                                              struct mylite_field_descriptor *out_descriptor);
static int infer_expression_descriptor(mylite_db *database, const struct mylite_select_plan *plan,
                                       const struct mylite_sql_ast_node *expression,
                                       const struct mylite_expression_value *value,
                                       struct mylite_field_descriptor *out_descriptor);
static int infer_collation_expression_descriptor(mylite_db *database,
                                                 const struct mylite_select_plan *plan,
                                                 const struct mylite_sql_ast_node *expression,
                                                 struct mylite_field_descriptor *out_descriptor);
static int infer_aggregate_expression_descriptor(mylite_db *database,
                                                 const struct mylite_select_plan *plan,
                                                 const struct mylite_sql_ast_node *expression,
                                                 struct mylite_field_descriptor *out_descriptor);
static int bind_select_projection_expression(mylite_db *database,
                                             const struct mylite_sql_ast_node *expression,
                                             struct mylite_select_plan *plan);
static int bind_select_aggregate_aware_expression(mylite_db *database,
                                                  const struct mylite_sql_ast_node *expression,
                                                  struct mylite_select_plan *plan,
                                                  const char *clause_context);
static int execute_scalar_select_statement(mylite_stmt *stmt);
static int evaluate_scalar_select_session_function(
    mylite_stmt *stmt, const struct mylite_sql_ast_node *function_call,
    const struct mylite_expression_eval_context *expression_context,
    struct mylite_expression_warnings *warnings, struct mylite_expression_value *out_value);
static int evaluate_dml_materialize_session_function(
    void *user_data, const struct mylite_select_table *table,
    const struct mylite_sql_ast_node *function_call,
    const struct mylite_expression_eval_context *expression_context,
    struct mylite_expression_warnings *warnings, struct mylite_expression_value *out_value);
static int set_dml_materialize_where_predicate_eval_error(void *user_data);
static int evaluate_statement_session_function(
    mylite_stmt *stmt, const struct mylite_sql_ast_node *function_call,
    const struct mylite_expression_eval_context *expression_context,
    struct mylite_expression_warnings *warnings, const struct mylite_select_table *table,
    struct mylite_expression_value *out_value);
static int execute_table_select_statement(mylite_stmt *stmt);
static int evaluate_select_subquery_expression(mylite_stmt *stmt,
                                               const struct mylite_sql_ast_node *subquery,
                                               struct mylite_expression_warnings *warnings,
                                               struct mylite_expression_value *out_value);
static int evaluate_in_subquery_expression(mylite_stmt *stmt,
                                           const struct mylite_sql_ast_node *expression,
                                           const struct mylite_expression_value *left,
                                           struct mylite_expression_warnings *warnings,
                                           struct mylite_expression_value *out_value);
static int evaluate_quantified_subquery_expression(mylite_stmt *stmt,
                                                   const struct mylite_sql_ast_node *expression,
                                                   const struct mylite_expression_value *left,
                                                   struct mylite_expression_warnings *warnings,
                                                   struct mylite_expression_value *out_value);
static int
evaluate_row_subquery_expression(mylite_stmt *stmt, const struct mylite_sql_ast_node *expression,
                                 const struct mylite_expression_eval_context *expression_context,
                                 struct mylite_expression_warnings *warnings,
                                 struct mylite_expression_value *out_value);

static const struct mylite_select_eval_callbacks table_select_eval_callbacks = {
    .resolve_order_reference = mylite_select_resolve_order_reference,
    .resolve_having_reference = mylite_select_resolve_having_reference_internal,
    .eval_session_function = evaluate_statement_session_function,
    .eval_subquery = evaluate_select_subquery_expression,
    .eval_in_subquery = evaluate_in_subquery_expression,
    .eval_quantified_subquery = evaluate_quantified_subquery_expression,
    .eval_row_subquery = evaluate_row_subquery_expression,
    .copy_column_value = mylite_select_copy_current_sqlite_column_value,
    .set_expression_eval_error = mylite_select_set_where_predicate_eval_error,
};

static const struct mylite_select_subquery_eval_callbacks select_subquery_eval_callbacks = {
    .prepare_select_subquery = prepare_select_subquery_statement,
    .table_select_eval_callbacks = &table_select_eval_callbacks,
};

static const struct mylite_select_subquery_bind_callbacks select_subquery_bind_callbacks = {
    .prepare_select_subquery = prepare_select_subquery_statement,
    .set_unsupported_where_error = mylite_select_set_unsupported_where_error,
};

static const struct mylite_select_predicate_bind_callbacks select_predicate_bind_callbacks = {
    .subquery_callbacks = &select_subquery_bind_callbacks,
    .set_invalid_group_function_error = mylite_select_set_invalid_group_function_error,
    .set_unsupported_where_error = mylite_select_set_unsupported_where_error,
};

static const struct mylite_select_aggregate_bind_callbacks select_aggregate_bind_callbacks = {
    .predicate_callbacks = &select_predicate_bind_callbacks,
    .subquery_callbacks = &select_subquery_bind_callbacks,
    .infer_aggregate_expression_descriptor = infer_aggregate_expression_descriptor,
    .infer_expression_descriptor = infer_expression_descriptor,
    .set_invalid_group_function_error = mylite_select_set_invalid_group_function_error,
    .set_unsupported_projection_error = mylite_select_set_unsupported_projection_error,
};

static const struct mylite_select_group_bind_callbacks select_group_bind_callbacks = {
    .aggregate_callbacks = &select_aggregate_bind_callbacks,
    .predicate_callbacks = &select_predicate_bind_callbacks,
    .set_invalid_group_function_error = mylite_select_set_invalid_group_function_error,
    .set_unsupported_where_error = mylite_select_set_unsupported_where_error,
};

static const struct mylite_select_order_bind_callbacks select_order_bind_callbacks = {
    .aggregate_callbacks = &select_aggregate_bind_callbacks,
    .subquery_callbacks = &select_subquery_bind_callbacks,
    .set_unsupported_order_error = mylite_select_set_unsupported_order_error,
};

static const struct mylite_select_metadata_callbacks select_metadata_callbacks = {
    .infer_expression_descriptor = infer_select_expression_descriptor,
};

static const struct mylite_select_projection_callbacks select_projection_callbacks = {
    .bind_expression = bind_select_projection_expression,
    .set_unsupported_projection_error = mylite_select_set_unsupported_projection_error,
};

static const struct mylite_select_statement_callbacks select_statement_callbacks = {
    .aggregate_callbacks = &select_aggregate_bind_callbacks,
    .metadata_callbacks = &select_metadata_callbacks,
};

static const struct mylite_expression_collation_callbacks expression_collation_callbacks = {
    .infer_expression_descriptor = infer_collation_expression_descriptor,
};

static const struct mylite_expression_descriptor_aggregate_callbacks
    aggregate_descriptor_callbacks = {
        .infer_expression_descriptor = infer_expression_descriptor,
};

static const struct mylite_expression_descriptor_case_callbacks case_descriptor_callbacks = {
    .infer_expression_descriptor = infer_expression_descriptor,
};

static const struct mylite_expression_descriptor_cast_callbacks cast_descriptor_callbacks = {
    .infer_expression_descriptor = infer_expression_descriptor,
};

static const struct mylite_expression_descriptor_subquery_callbacks subquery_descriptor_callbacks =
    {
        .infer_expression_descriptor = infer_expression_descriptor,
        .prepare_select_subquery = prepare_select_subquery_statement,
        .bind_callbacks = &select_subquery_bind_callbacks,
};

static const struct mylite_expression_descriptor_operator_callbacks operator_descriptor_callbacks =
    {
        .infer_expression_descriptor = infer_expression_descriptor,
        .subquery_callbacks = &subquery_descriptor_callbacks,
};

static const struct mylite_expression_descriptor_function_callbacks function_descriptor_callbacks =
    {
        .infer_expression_descriptor = infer_expression_descriptor,
};

static const struct mylite_select_scalar_eval_callbacks select_scalar_eval_callbacks = {
    .infer_expression_descriptor = infer_scalar_expression_descriptor,
    .eval_session_function = evaluate_scalar_select_session_function,
    .eval_subquery = evaluate_select_subquery_expression,
    .eval_in_subquery = evaluate_in_subquery_expression,
    .eval_quantified_subquery = evaluate_quantified_subquery_expression,
    .eval_row_subquery = evaluate_row_subquery_expression,
    .set_unsupported_order_error = mylite_select_set_unsupported_order_error,
    .set_ambiguous_order_column_error = mylite_select_set_ambiguous_order_column_error,
};

static const struct mylite_select_prepare_callbacks select_prepare_callbacks = {
    .projection_callbacks = &select_projection_callbacks,
    .statement_callbacks = &select_statement_callbacks,
    .metadata_callbacks = &select_metadata_callbacks,
    .predicate_callbacks = &select_predicate_bind_callbacks,
    .group_callbacks = &select_group_bind_callbacks,
    .order_callbacks = &select_order_bind_callbacks,
    .scalar_callbacks = &select_scalar_eval_callbacks,
};

static const struct mylite_select_union_prepare_callbacks union_query_prepare_callbacks = {
    .prepare_select_subquery = prepare_select_subquery_statement,
    .clone_order_expressions = mylite_select_clone_order_expressions,
    .set_ambiguous_order_column_error = mylite_select_set_ambiguous_order_column_error,
    .set_unsupported_order_error = mylite_select_set_unsupported_order_error,
};

static const struct mylite_statement_prepare_callbacks statement_prepare_callbacks = {
    .select_callbacks = &select_prepare_callbacks,
    .scalar_callbacks = &select_scalar_eval_callbacks,
    .union_callbacks = &union_query_prepare_callbacks,
};

static const struct mylite_select_union_callbacks union_query_callbacks = {
    .select_eval_callbacks = &table_select_eval_callbacks,
    .execute_scalar_select = execute_scalar_select_statement,
    .execute_table_select = execute_table_select_statement,
    .copy_operand_row_value = mylite_select_subquery_copy_row_value,
    .append_warnings = mylite_select_subquery_append_warnings,
    .set_unsupported_order_error = mylite_select_set_unsupported_order_error,
};

static const struct mylite_statement_execute_callbacks statement_execute_callbacks = {
    .execute_scalar_select = execute_scalar_select_statement,
    .execute_table_select = execute_table_select_statement,
    .union_callbacks = &union_query_callbacks,
    .eval_dml_materialize_session_function = evaluate_dml_materialize_session_function,
    .set_dml_materialize_where_predicate_eval_error =
        set_dml_materialize_where_predicate_eval_error,
};

int mylite_prepare(mylite_db *database, const char *sql, size_t length, mylite_stmt **out_stmt)
{
    return mylite_statement_prepare_with_callbacks(database, sql, length, out_stmt,
                                                   &statement_prepare_callbacks);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int prepare_select_subquery_statement(mylite_db *database,
                                             const struct mylite_sql_ast_node *statement,
                                             mylite_stmt **out_stmt)
{
    return mylite_select_prepare_subquery(database, statement, out_stmt, &select_prepare_callbacks);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_select_expression_descriptor(mylite_db *database,
                                              const struct mylite_select_plan *plan,
                                              const struct mylite_sql_ast_node *expression,
                                              struct mylite_field_descriptor *out_descriptor)
{
    return infer_expression_descriptor(database, plan, expression, NULL, out_descriptor);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_scalar_expression_descriptor(mylite_db *database,
                                              const struct mylite_sql_ast_node *expression,
                                              const struct mylite_expression_value *value,
                                              struct mylite_field_descriptor *out_descriptor)
{
    return infer_expression_descriptor(database, NULL, expression, value, out_descriptor);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_collation_expression_descriptor(mylite_db *database,
                                                 const struct mylite_select_plan *plan,
                                                 const struct mylite_sql_ast_node *expression,
                                                 struct mylite_field_descriptor *out_descriptor)
{
    return infer_expression_descriptor(database, plan, expression, NULL, out_descriptor);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_expression_descriptor(mylite_db *database, const struct mylite_select_plan *plan,
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
        return infer_aggregate_expression_descriptor(database, plan, node, out_descriptor);
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
static int infer_aggregate_expression_descriptor(mylite_db *database,
                                                 const struct mylite_select_plan *plan,
                                                 const struct mylite_sql_ast_node *expression,
                                                 struct mylite_field_descriptor *out_descriptor)
{
    return mylite_expression_descriptor_infer_aggregate_expression(
        database, plan, expression, out_descriptor, &aggregate_descriptor_callbacks);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_select_projection_expression(mylite_db *database,
                                             const struct mylite_sql_ast_node *expression,
                                             struct mylite_select_plan *plan)
{
    int status = bind_select_aggregate_aware_expression(database, expression, plan, "field list");

    if (status == MYLITE_UNSUPPORTED) {
        return mylite_select_set_unsupported_projection_error(database);
    }
    return status;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_select_aggregate_aware_expression(mylite_db *database,
                                                  const struct mylite_sql_ast_node *expression,
                                                  struct mylite_select_plan *plan,
                                                  const char *clause_context)
{
    return mylite_select_bind_aggregate_aware_expression(database, expression, plan, clause_context,
                                                         &select_aggregate_bind_callbacks);
}

int mylite_statement_execute_custom(mylite_stmt *stmt)
{
    return mylite_statement_execute_custom_with_callbacks(stmt, &statement_execute_callbacks);
}

static int execute_scalar_select_statement(mylite_stmt *stmt)
{
    return mylite_select_scalar_execute_statement(stmt, &select_scalar_eval_callbacks);
}

static int evaluate_scalar_select_session_function(
    mylite_stmt *stmt, const struct mylite_sql_ast_node *function_call,
    const struct mylite_expression_eval_context *expression_context,
    struct mylite_expression_warnings *warnings, struct mylite_expression_value *out_value)
{
    return evaluate_statement_session_function(stmt, function_call, expression_context, warnings,
                                               NULL, out_value);
}

static int evaluate_dml_materialize_session_function(
    void *user_data, const struct mylite_select_table *table,
    const struct mylite_sql_ast_node *function_call,
    const struct mylite_expression_eval_context *expression_context,
    struct mylite_expression_warnings *warnings, struct mylite_expression_value *out_value)
{
    return evaluate_statement_session_function((mylite_stmt *)user_data, function_call,
                                               expression_context, warnings, table, out_value);
}

static int set_dml_materialize_where_predicate_eval_error(void *user_data)
{
    return mylite_select_set_where_predicate_eval_error((mylite_stmt *)user_data);
}

static int evaluate_statement_session_function(
    mylite_stmt *stmt, const struct mylite_sql_ast_node *function_call,
    const struct mylite_expression_eval_context *expression_context,
    struct mylite_expression_warnings *warnings, const struct mylite_select_table *table,
    struct mylite_expression_value *out_value)
{
    return mylite_statement_evaluate_session_function(stmt, function_call, expression_context,
                                                      warnings, table,
                                                      &expression_collation_callbacks, out_value);
}

static int execute_table_select_statement(mylite_stmt *stmt)
{
    return mylite_select_execute_table_statement(stmt, &table_select_eval_callbacks);
}

static int evaluate_select_subquery_expression(mylite_stmt *stmt,
                                               const struct mylite_sql_ast_node *subquery,
                                               struct mylite_expression_warnings *warnings,
                                               struct mylite_expression_value *out_value)
{
    return mylite_select_subquery_eval(stmt, subquery, warnings, out_value,
                                       &select_subquery_eval_callbacks);
}

static int evaluate_in_subquery_expression(mylite_stmt *stmt,
                                           const struct mylite_sql_ast_node *expression,
                                           const struct mylite_expression_value *left,
                                           struct mylite_expression_warnings *warnings,
                                           struct mylite_expression_value *out_value)
{
    return mylite_select_subquery_eval_in(stmt, expression, left, warnings, out_value,
                                          &select_subquery_eval_callbacks);
}

static int evaluate_quantified_subquery_expression(mylite_stmt *stmt,
                                                   const struct mylite_sql_ast_node *expression,
                                                   const struct mylite_expression_value *left,
                                                   struct mylite_expression_warnings *warnings,
                                                   struct mylite_expression_value *out_value)
{
    return mylite_select_subquery_eval_quantified(stmt, expression, left, warnings, out_value,
                                                  &select_subquery_eval_callbacks);
}

static int
evaluate_row_subquery_expression(mylite_stmt *stmt, const struct mylite_sql_ast_node *expression,
                                 const struct mylite_expression_eval_context *expression_context,
                                 struct mylite_expression_warnings *warnings,
                                 struct mylite_expression_value *out_value)
{
    return mylite_select_subquery_eval_row(stmt, expression, expression_context, warnings,
                                           out_value, &select_subquery_eval_callbacks);
}
