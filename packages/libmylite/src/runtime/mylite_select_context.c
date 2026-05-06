#include "mylite_select_context.h"

#include "mylite_expression_collation.h"
#include "mylite_expression_descriptor_dispatch.h"
#include "mylite_select_aggregate_bind.h"
#include "mylite_select_diagnostics.h"
#include "mylite_select_eval.h"
#include "mylite_select_group_bind.h"
#include "mylite_select_metadata.h"
#include "mylite_select_order_bind.h"
#include "mylite_select_order_resolve.h"
#include "mylite_select_predicate_bind.h"
#include "mylite_select_prepare.h"
#include "mylite_select_projection.h"
#include "mylite_select_resolve.h"
#include "mylite_select_row_loader.h"
#include "mylite_select_scalar.h"
#include "mylite_select_statement.h"
#include "mylite_select_subquery.h"
#include "mylite_select_union.h"
#include "mylite_statement_execute.h"
#include "mylite_statement_functions.h"
#include "mylite_statement_prepare.h"

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
static int evaluate_dml_materialize_subquery(void *user_data,
                                             const struct mylite_sql_ast_node *subquery,
                                             struct mylite_expression_warnings *warnings,
                                             struct mylite_expression_value *out_value);
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

static const struct mylite_expression_collation_callbacks expression_collation_callbacks = {
    .infer_expression_descriptor = mylite_expression_descriptor_infer_collation,
};

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
    .prepare_select_subquery = mylite_select_context_prepare_subquery,
    .table_select_eval_callbacks = &table_select_eval_callbacks,
};

const struct mylite_select_subquery_bind_callbacks mylite_select_context_subquery_bind_callbacks = {
    .prepare_select_subquery = mylite_select_context_prepare_subquery,
    .set_unsupported_where_error = mylite_select_set_unsupported_where_error,
};

static const struct mylite_select_predicate_bind_callbacks select_predicate_bind_callbacks = {
    .subquery_callbacks = &mylite_select_context_subquery_bind_callbacks,
    .set_invalid_group_function_error = mylite_select_set_invalid_group_function_error,
    .set_unsupported_where_error = mylite_select_set_unsupported_where_error,
};

static const struct mylite_select_aggregate_bind_callbacks select_aggregate_bind_callbacks = {
    .predicate_callbacks = &select_predicate_bind_callbacks,
    .subquery_callbacks = &mylite_select_context_subquery_bind_callbacks,
    .infer_aggregate_expression_descriptor = mylite_expression_descriptor_infer_aggregate,
    .infer_expression_descriptor = mylite_expression_descriptor_infer,
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
    .subquery_callbacks = &mylite_select_context_subquery_bind_callbacks,
    .set_unsupported_order_error = mylite_select_set_unsupported_order_error,
};

static const struct mylite_select_metadata_callbacks select_metadata_callbacks = {
    .infer_expression_descriptor = mylite_expression_descriptor_infer_select,
};

static const struct mylite_select_projection_callbacks select_projection_callbacks = {
    .bind_expression = bind_select_projection_expression,
    .set_unsupported_projection_error = mylite_select_set_unsupported_projection_error,
};

static const struct mylite_select_statement_callbacks select_statement_callbacks = {
    .aggregate_callbacks = &select_aggregate_bind_callbacks,
    .metadata_callbacks = &select_metadata_callbacks,
};

static const struct mylite_select_scalar_eval_callbacks select_scalar_eval_callbacks = {
    .infer_expression_descriptor = mylite_expression_descriptor_infer_scalar,
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
    .prepare_select_subquery = mylite_select_context_prepare_subquery,
    .scalar_callbacks = &select_scalar_eval_callbacks,
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
    .scalar_callbacks = &select_scalar_eval_callbacks,
    .execute_scalar_select = execute_scalar_select_statement,
    .execute_table_select = execute_table_select_statement,
    .copy_operand_row_value = mylite_select_subquery_copy_row_value,
    .append_warnings = mylite_select_subquery_append_warnings,
    .set_unsupported_order_error = mylite_select_set_unsupported_order_error,
};

static const struct mylite_statement_execute_callbacks statement_execute_callbacks = {
    .execute_scalar_select = execute_scalar_select_statement,
    .execute_table_select = execute_table_select_statement,
    .scalar_callbacks = &select_scalar_eval_callbacks,
    .union_callbacks = &union_query_callbacks,
    .eval_dml_materialize_session_function = evaluate_dml_materialize_session_function,
    .eval_dml_materialize_subquery = evaluate_dml_materialize_subquery,
    .set_dml_materialize_where_predicate_eval_error =
        set_dml_materialize_where_predicate_eval_error,
};

const struct mylite_statement_prepare_callbacks *
mylite_select_context_statement_prepare_callbacks(void)
{
    return &statement_prepare_callbacks;
}

const struct mylite_statement_execute_callbacks *
mylite_select_context_statement_execute_callbacks(void)
{
    return &statement_execute_callbacks;
}

const struct mylite_select_eval_callbacks *mylite_select_context_table_select_eval_callbacks(void)
{
    return &table_select_eval_callbacks;
}

const struct mylite_select_predicate_bind_callbacks *
mylite_select_context_predicate_bind_callbacks(void)
{
    return &select_predicate_bind_callbacks;
}

// NOLINTNEXTLINE(misc-no-recursion)
int mylite_select_context_prepare_subquery(mylite_db *database,
                                           const struct mylite_sql_ast_node *statement,
                                           mylite_stmt **out_stmt)
{
    return mylite_select_prepare_subquery(database, statement, out_stmt, &select_prepare_callbacks);
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

static int evaluate_dml_materialize_subquery(void *user_data,
                                             const struct mylite_sql_ast_node *subquery,
                                             struct mylite_expression_warnings *warnings,
                                             struct mylite_expression_value *out_value)
{
    return evaluate_select_subquery_expression((mylite_stmt *)user_data, subquery, warnings,
                                               out_value);
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
