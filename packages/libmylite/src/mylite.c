#include <mylite/mylite.h>

#include "mylite_charset.h"
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
#include "runtime/mylite_expression_descriptor_numeric.h"
#include "runtime/mylite_expression_descriptor_scalar.h"
#include "runtime/mylite_expression_descriptor_string.h"
#include "runtime/mylite_expression_descriptor_temporal.h"
#include "runtime/mylite_expression_validation.h"
#include "runtime/mylite_field_descriptor.h"
#include "runtime/mylite_function_names.h"
#include "runtime/mylite_information_schema.h"
#include "runtime/mylite_metadata.h"
#include "runtime/mylite_metadata_constants.h"
#include "runtime/mylite_metadata_types.h"
#include "runtime/mylite_runtime.h"
#include "runtime/mylite_schema.h"
#include "runtime/mylite_schema_types.h"
#include "runtime/mylite_select.h"
#include "runtime/mylite_select_aggregate.h"
#include "runtime/mylite_select_aggregate_bind.h"
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
#include "runtime/mylite_session_functions.h"
#include "runtime/mylite_show.h"
#include "runtime/mylite_show_types.h"
#include "runtime/mylite_span.h"
#include "runtime/mylite_sqlite_value.h"
#include "runtime/mylite_statement.h"
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

#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int prepare_parsed_statement(mylite_db *database, const struct mylite_sql_ast_node *root,
                                    const char *sql, size_t sql_length, mylite_stmt **out_stmt);
static int prepare_schema_lifecycle_statement(mylite_db *database,
                                              const struct mylite_sql_ast_node *statement,
                                              mylite_stmt **out_stmt);
static int prepare_create_table_statement(mylite_db *database,
                                          const struct mylite_sql_ast_node *statement,
                                          mylite_stmt **out_stmt);
static int prepare_drop_table_statement(mylite_db *database,
                                        const struct mylite_sql_ast_node *statement,
                                        mylite_stmt **out_stmt);
static int prepare_rename_table_statement(mylite_db *database,
                                          const struct mylite_sql_ast_node *statement,
                                          mylite_stmt **out_stmt);
static int prepare_truncate_table_statement(mylite_db *database,
                                            const struct mylite_sql_ast_node *statement,
                                            mylite_stmt **out_stmt);
static int prepare_alter_table_statement(mylite_db *database,
                                         const struct mylite_sql_ast_node *statement,
                                         mylite_stmt **out_stmt);
static int prepare_create_index_statement(mylite_db *database,
                                          const struct mylite_sql_ast_node *statement,
                                          mylite_stmt **out_stmt);
static int prepare_drop_index_statement(mylite_db *database,
                                        const struct mylite_sql_ast_node *statement,
                                        mylite_stmt **out_stmt);
static int prepare_insert_values_statement(mylite_db *database,
                                           const struct mylite_sql_ast_node *statement,
                                           mylite_stmt **out_stmt);
static int prepare_insert_set_statement(mylite_db *database,
                                        const struct mylite_sql_ast_node *statement,
                                        mylite_stmt **out_stmt);
static int prepare_replace_values_statement(mylite_db *database,
                                            const struct mylite_sql_ast_node *statement,
                                            mylite_stmt **out_stmt);
static int prepare_replace_set_statement(mylite_db *database,
                                         const struct mylite_sql_ast_node *statement,
                                         mylite_stmt **out_stmt);
static int prepare_transaction_statement(mylite_db *database,
                                         const struct mylite_sql_ast_node *statement,
                                         mylite_stmt **out_stmt);
static int validate_select_duplicate_mode(mylite_db *database,
                                          const struct mylite_sql_ast_node *statement);
static int prepare_table_select_statement(mylite_db *database,
                                          const struct mylite_sql_ast_node *statement,
                                          const char *sql, size_t sql_length,
                                          mylite_stmt **out_stmt);
static int prepare_table_select_sqlite_statement(mylite_db *database,
                                                 const struct mylite_select_plan *plan,
                                                 mylite_stmt **out_stmt);
static int prepare_scalar_select_statement(mylite_db *database,
                                           const struct mylite_sql_ast_node *statement,
                                           mylite_stmt **out_stmt);
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
static int infer_identifier_descriptor(mylite_db *database, const struct mylite_select_plan *plan,
                                       const struct mylite_sql_ast_node *expression,
                                       struct mylite_field_descriptor *out_descriptor);
static int infer_literal_descriptor(mylite_db *database,
                                    const struct mylite_sql_ast_node *expression,
                                    const struct mylite_expression_value *value,
                                    struct mylite_field_descriptor *out_descriptor);
static int infer_unary_expression_descriptor(mylite_db *database,
                                             const struct mylite_select_plan *plan,
                                             const struct mylite_sql_ast_node *expression,
                                             const struct mylite_expression_value *value,
                                             struct mylite_field_descriptor *out_descriptor);
static int infer_binary_expression_descriptor(mylite_db *database,
                                              const struct mylite_select_plan *plan,
                                              const struct mylite_sql_ast_node *expression,
                                              const struct mylite_expression_value *value,
                                              struct mylite_field_descriptor *out_descriptor);
static int infer_ternary_expression_descriptor(mylite_db *database,
                                               const struct mylite_select_plan *plan,
                                               const struct mylite_sql_ast_node *expression,
                                               const struct mylite_expression_value *value,
                                               struct mylite_field_descriptor *out_descriptor);
static int infer_function_expression_descriptor(mylite_db *database,
                                                const struct mylite_select_plan *plan,
                                                const struct mylite_sql_ast_node *expression,
                                                const struct mylite_expression_value *value,
                                                struct mylite_field_descriptor *out_descriptor);
static bool infer_common_scalar_function_descriptor(mylite_db *database,
                                                    const struct mylite_sql_ast_node *name,
                                                    bool arguments_nullable, bool result_nullable,
                                                    struct mylite_field_descriptor *out_descriptor);
static int infer_temporal_function_descriptor(mylite_db *database,
                                              const struct mylite_select_plan *plan,
                                              const struct mylite_sql_ast_node *expression,
                                              const struct mylite_expression_value *value,
                                              struct mylite_field_descriptor *out_descriptor);
static int infer_aggregate_expression_descriptor(mylite_db *database,
                                                 const struct mylite_select_plan *plan,
                                                 const struct mylite_sql_ast_node *expression,
                                                 struct mylite_field_descriptor *out_descriptor);
static int infer_case_expression_descriptor(mylite_db *database,
                                            const struct mylite_select_plan *plan,
                                            const struct mylite_sql_ast_node *expression,
                                            struct mylite_field_descriptor *out_descriptor);
static int infer_cast_expression_descriptor(mylite_db *database,
                                            const struct mylite_select_plan *plan,
                                            const struct mylite_sql_ast_node *expression,
                                            const struct mylite_expression_value *value,
                                            struct mylite_field_descriptor *out_descriptor);
static int
infer_scalar_subquery_expression_descriptor(mylite_db *database,
                                            const struct mylite_sql_ast_node *expression,
                                            struct mylite_field_descriptor *out_descriptor);
static int infer_in_subquery_expression_descriptor(mylite_db *database,
                                                   const struct mylite_select_plan *plan,
                                                   const struct mylite_sql_ast_node *expression,
                                                   struct mylite_field_descriptor *out_descriptor);
static int infer_row_subquery_expression_descriptor(mylite_db *database,
                                                    const struct mylite_select_plan *plan,
                                                    const struct mylite_sql_ast_node *expression,
                                                    struct mylite_field_descriptor *out_descriptor);
static int infer_quantified_subquery_expression_descriptor(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, struct mylite_field_descriptor *out_descriptor);
static int infer_case_result_descriptor(mylite_db *database, const struct mylite_select_plan *plan,
                                        const struct mylite_sql_ast_node *expression,
                                        struct mylite_case_descriptor_aggregate *aggregate);
static void aggregate_case_result_descriptor(const struct mylite_field_descriptor *descriptor,
                                             struct mylite_case_descriptor_aggregate *aggregate);
static struct mylite_field_descriptor
finalize_case_descriptor(mylite_db *database,
                         const struct mylite_case_descriptor_aggregate *aggregate);
static struct mylite_field_descriptor
cast_signed_descriptor(const struct mylite_field_descriptor *source);
static struct mylite_field_descriptor
cast_unsigned_descriptor(const struct mylite_field_descriptor *source);
static struct mylite_field_descriptor
cast_decimal_descriptor(const struct mylite_sql_ast_node *target,
                        const struct mylite_field_descriptor *source);
static struct mylite_field_descriptor
cast_character_descriptor(mylite_db *database, const struct mylite_sql_ast_node *target,
                          const struct mylite_expression_value *value,
                          const struct mylite_field_descriptor *source);
static unsigned int cast_decimal_precision(const struct mylite_sql_ast_node *target);
static unsigned int cast_decimal_scale(const struct mylite_sql_ast_node *target);
static uint64_t cast_character_length(mylite_db *database, const struct mylite_sql_ast_node *target,
                                      const struct mylite_expression_value *value,
                                      const struct mylite_field_descriptor *source);
static unsigned int cast_target_charset_id(mylite_db *database,
                                           const struct mylite_sql_ast_node *target);
static int cast_target_charset_max_length(mylite_db *database,
                                          const struct mylite_sql_ast_node *target);
static int infer_function_arguments_nullable(mylite_db *database,
                                             const struct mylite_select_plan *plan,
                                             const struct mylite_sql_ast_node *arguments,
                                             bool *out_nullable);
// NOLINTNEXTLINE(misc-no-recursion)
static int infer_variadic_scalar_function_descriptor(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, const struct mylite_expression_value *value,
    bool result_nullable, struct mylite_field_descriptor *out_descriptor);
static int infer_greatest_least_function_descriptor(mylite_db *database,
                                                    const struct mylite_select_plan *plan,
                                                    const struct mylite_sql_ast_node *expression,
                                                    bool result_nullable,
                                                    struct mylite_field_descriptor *out_descriptor);
static int greatest_least_function_uses_string_domain(mylite_db *database,
                                                      const struct mylite_select_plan *plan,
                                                      const struct mylite_sql_ast_node *arguments,
                                                      bool *out_string_domain);
static int infer_greatest_least_string_descriptor(mylite_db *database,
                                                  const struct mylite_select_plan *plan,
                                                  const struct mylite_sql_ast_node *arguments,
                                                  bool result_nullable,
                                                  struct mylite_field_descriptor *out_descriptor);
static uint64_t greatest_least_string_result_length(mylite_db *database,
                                                    const struct mylite_select_plan *plan,
                                                    const struct mylite_sql_ast_node *arguments);
static uint64_t
greatest_least_argument_string_length(mylite_db *database,
                                      const struct mylite_field_descriptor *descriptor);
static int infer_greatest_least_numeric_descriptor(mylite_db *database,
                                                   const struct mylite_select_plan *plan,
                                                   const struct mylite_sql_ast_node *arguments,
                                                   bool result_nullable,
                                                   struct mylite_field_descriptor *out_descriptor);
static void
aggregate_greatest_least_numeric_descriptor(const struct mylite_field_descriptor *argument,
                                            struct mylite_field_descriptor *aggregate,
                                            bool *out_saw_nonnull);
static int bind_select_join_predicates(mylite_db *database, const struct mylite_select_plan *plan);
static int bind_table_select_clauses(mylite_db *database,
                                     const struct mylite_select_clause_nodes *clauses,
                                     struct mylite_select_plan *plan);
static int bind_select_where_clause(mylite_db *database,
                                    const struct mylite_sql_ast_node *where_clause,
                                    const struct mylite_select_plan *plan);
static int bind_select_projection_expression(mylite_db *database,
                                             const struct mylite_sql_ast_node *expression,
                                             struct mylite_select_plan *plan);
static int bind_select_aggregate_aware_expression(mylite_db *database,
                                                  const struct mylite_sql_ast_node *expression,
                                                  struct mylite_select_plan *plan,
                                                  const char *clause_context);
static int bind_select_group_by_clause(mylite_db *database,
                                       const struct mylite_sql_ast_node *group_by_clause,
                                       struct mylite_select_plan *plan);
static int bind_select_having_clause(mylite_db *database,
                                     const struct mylite_sql_ast_node *having_clause,
                                     struct mylite_select_plan *plan);
static int bind_select_order_by_clause(mylite_db *database,
                                       const struct mylite_sql_ast_node *order_by_clause,
                                       struct mylite_select_plan *plan);
static int set_select_invalid_group_function_error(mylite_db *database);
static int set_select_duplicate_mode_error(mylite_db *database);
static int set_select_unsupported_projection_error(mylite_db *database);
static int set_select_unsupported_where_error(mylite_db *database);
static int set_select_unsupported_order_error(mylite_db *database);
static int set_select_unsupported_join_grouping_error(mylite_db *database);
static int prepare_custom_statement(mylite_db *database, enum mylite_stmt_kind kind,
                                    const struct mylite_sql_ast_node *statement,
                                    mylite_stmt **out_stmt);
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
static int evaluate_strcmp_function(mylite_stmt *stmt,
                                    const struct mylite_sql_ast_node *function_call,
                                    const struct mylite_expression_eval_context *expression_context,
                                    struct mylite_expression_warnings *warnings,
                                    const struct mylite_select_table *table,
                                    struct mylite_expression_value *out_value);
static int infer_strcmp_collation_info(mylite_stmt *stmt,
                                       const struct mylite_sql_ast_node *function_call,
                                       const struct mylite_select_table *table,
                                       struct mylite_charset_collation_info *out_info);
static int set_strcmp_function_result(mylite_db *database,
                                      const struct mylite_expression_value *left,
                                      const struct mylite_sql_ast_node *left_argument,
                                      const struct mylite_expression_value *right,
                                      const struct mylite_sql_ast_node *right_argument,
                                      const struct mylite_charset_collation_info *collation_info,
                                      struct mylite_expression_value *out_value);
static int strcmp_value_to_text(mylite_db *database, const struct mylite_expression_value *value,
                                const struct mylite_sql_ast_node *argument, char **out_text,
                                size_t *out_length);
static const struct mylite_sql_ast_node *
strcmp_decimal_literal_argument(const struct mylite_sql_ast_node *argument, bool *out_negative);
static int strcmp_decimal_literal_to_text(mylite_db *database,
                                          const struct mylite_sql_ast_node *literal, bool negative,
                                          char **out_text, size_t *out_length);
static bool decimal_literal_span_is_zero(const char *text, size_t length);
static int compare_strcmp_texts(const char *left, size_t left_length, const char *right,
                                size_t right_length, struct mylite_strcmp_compare_options options);
static void trim_strcmp_trailing_spaces(const char *text, size_t *length);
static unsigned char strcmp_compare_byte(unsigned char value,
                                         struct mylite_strcmp_compare_options options);
static struct mylite_strcmp_compare_options
strcmp_compare_options_for_collation(const struct mylite_charset_collation_info *info);
static bool strcmp_collation_ignores_trailing_spaces(const char *collation_name);
static bool strcmp_collation_is_case_sensitive(const struct mylite_charset_collation_info *info);
static int evaluate_charset_collation_function(
    mylite_stmt *stmt, const struct mylite_sql_ast_node *function_call,
    const struct mylite_expression_eval_context *expression_context,
    struct mylite_expression_warnings *warnings, const struct mylite_select_table *table,
    struct mylite_expression_value *out_value);
static int set_charset_collation_function_result(mylite_db *database,
                                                 const struct mylite_sql_ast_node *name,
                                                 const struct mylite_charset_collation_info *info,
                                                 struct mylite_expression_value *out_value);
static int execute_table_select_statement(mylite_stmt *stmt);
static int set_where_predicate_eval_error(mylite_stmt *stmt);
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
    .set_expression_eval_error = set_where_predicate_eval_error,
};

static const struct mylite_select_subquery_eval_callbacks select_subquery_eval_callbacks = {
    .prepare_select_subquery = prepare_select_subquery_statement,
    .table_select_eval_callbacks = &table_select_eval_callbacks,
};

static const struct mylite_select_subquery_bind_callbacks select_subquery_bind_callbacks = {
    .prepare_select_subquery = prepare_select_subquery_statement,
    .set_unsupported_where_error = set_select_unsupported_where_error,
};

static const struct mylite_select_predicate_bind_callbacks select_predicate_bind_callbacks = {
    .subquery_callbacks = &select_subquery_bind_callbacks,
    .set_invalid_group_function_error = set_select_invalid_group_function_error,
    .set_unsupported_where_error = set_select_unsupported_where_error,
};

static const struct mylite_select_aggregate_bind_callbacks select_aggregate_bind_callbacks = {
    .predicate_callbacks = &select_predicate_bind_callbacks,
    .subquery_callbacks = &select_subquery_bind_callbacks,
    .infer_aggregate_expression_descriptor = infer_aggregate_expression_descriptor,
    .infer_expression_descriptor = infer_expression_descriptor,
    .set_invalid_group_function_error = set_select_invalid_group_function_error,
    .set_unsupported_projection_error = set_select_unsupported_projection_error,
};

static const struct mylite_select_group_bind_callbacks select_group_bind_callbacks = {
    .aggregate_callbacks = &select_aggregate_bind_callbacks,
    .predicate_callbacks = &select_predicate_bind_callbacks,
    .set_invalid_group_function_error = set_select_invalid_group_function_error,
    .set_unsupported_where_error = set_select_unsupported_where_error,
};

static const struct mylite_select_order_bind_callbacks select_order_bind_callbacks = {
    .aggregate_callbacks = &select_aggregate_bind_callbacks,
    .subquery_callbacks = &select_subquery_bind_callbacks,
    .set_unsupported_order_error = set_select_unsupported_order_error,
};

static const struct mylite_select_metadata_callbacks select_metadata_callbacks = {
    .infer_expression_descriptor = infer_select_expression_descriptor,
};

static const struct mylite_select_projection_callbacks select_projection_callbacks = {
    .bind_expression = bind_select_projection_expression,
    .set_unsupported_projection_error = set_select_unsupported_projection_error,
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

static const struct mylite_expression_descriptor_temporal_callbacks temporal_descriptor_callbacks =
    {
        .infer_expression_descriptor = infer_expression_descriptor,
};

static const struct mylite_expression_descriptor_numeric_callbacks numeric_descriptor_callbacks = {
    .infer_expression_descriptor = infer_expression_descriptor,
};

static const struct mylite_expression_descriptor_string_callbacks string_descriptor_callbacks = {
    .infer_expression_descriptor = infer_expression_descriptor,
};

static const struct mylite_select_scalar_eval_callbacks select_scalar_eval_callbacks = {
    .infer_expression_descriptor = infer_scalar_expression_descriptor,
    .eval_session_function = evaluate_scalar_select_session_function,
    .eval_subquery = evaluate_select_subquery_expression,
    .eval_in_subquery = evaluate_in_subquery_expression,
    .eval_quantified_subquery = evaluate_quantified_subquery_expression,
    .eval_row_subquery = evaluate_row_subquery_expression,
    .set_unsupported_order_error = set_select_unsupported_order_error,
    .set_ambiguous_order_column_error = mylite_select_set_ambiguous_order_column_error,
};

static const struct mylite_select_union_prepare_callbacks union_query_prepare_callbacks = {
    .prepare_select_subquery = prepare_select_subquery_statement,
    .clone_order_expressions = mylite_select_clone_order_expressions,
    .set_ambiguous_order_column_error = mylite_select_set_ambiguous_order_column_error,
    .set_unsupported_order_error = set_select_unsupported_order_error,
};

static const struct mylite_select_union_callbacks union_query_callbacks = {
    .select_eval_callbacks = &table_select_eval_callbacks,
    .execute_scalar_select = execute_scalar_select_statement,
    .execute_table_select = execute_table_select_statement,
    .copy_operand_row_value = mylite_select_subquery_copy_row_value,
    .append_warnings = mylite_select_subquery_append_warnings,
    .set_unsupported_order_error = set_select_unsupported_order_error,
};

int mylite_prepare(mylite_db *database, const char *sql, size_t length, mylite_stmt **out_stmt)
{
    struct mylite_sql_parse_result parse_result;
    enum mylite_sql_parse_status parse_status = MYLITE_SQL_PARSE_OK;
    const struct mylite_sql_ast_node *statement = NULL;
    int status = MYLITE_OK;

    if (out_stmt == NULL) {
        return MYLITE_MISUSE;
    }
    *out_stmt = NULL;

    if (database == NULL || sql == NULL) {
        return MYLITE_MISUSE;
    }

    mylite_diagnostics_clear_error_message(database);
    if (database->transaction_released) {
        return mylite_connection_set_released_error(database);
    }
    parse_status = mylite_sql_parse(
        (struct mylite_sql_parse_config){
            .input = sql,
            .length = length,
            .modes = 0U,
        },
        &parse_result);
    if (parse_status != MYLITE_SQL_PARSE_OK) {
        mylite_diagnostics_clear_warnings(database);
        status = mylite_statement_map_parse_status(database, parse_status);
        if (status != MYLITE_NOMEM) {
            (void)mylite_diagnostics_append_current_error_condition(database,
                                                                    MYLITE_MYSQL_ER_PARSE_ERROR);
        }
        mylite_sql_parse_result_deinit(&parse_result);
        return status;
    }

    statement = mylite_ast_single_statement(parse_result.root);
    if (!mylite_statement_ast_preserves_diagnostics(statement)) {
        mylite_diagnostics_clear_warnings(database);
    }

    status = prepare_parsed_statement(database, parse_result.root, sql, length, out_stmt);
    if (status != MYLITE_OK && status != MYLITE_NOMEM) {
        (void)mylite_diagnostics_ensure_current_error_condition(database,
                                                                MYLITE_MYSQL_ER_UNKNOWN_ERROR);
    }
    mylite_sql_parse_result_deinit(&parse_result);
    return status;
}

static int prepare_parsed_statement(mylite_db *database, const struct mylite_sql_ast_node *root,
                                    const char *sql, size_t sql_length, mylite_stmt **out_stmt)
{
    struct mylite_sqlite_translate_result translate_result;
    enum mylite_sqlite_translate_status translate_status = MYLITE_SQLITE_TRANSLATE_OK;
    const struct mylite_sql_ast_node *statement = mylite_ast_single_statement(root);
    int status = MYLITE_OK;

    if (statement != NULL) {
        switch (statement->kind) {
        case MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT:
        case MYLITE_SQL_AST_ALTER_SCHEMA_STATEMENT:
        case MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT:
        case MYLITE_SQL_AST_USE_STATEMENT:
            return prepare_schema_lifecycle_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_SET_NAMES_STATEMENT:
        case MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT:
            return mylite_connection_prepare_charset_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_CREATE_TABLE_STATEMENT:
            return prepare_create_table_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_DROP_TABLE_STATEMENT:
            return prepare_drop_table_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_RENAME_TABLE_STATEMENT:
            return prepare_rename_table_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_TRUNCATE_TABLE_STATEMENT:
            return prepare_truncate_table_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_ALTER_TABLE_STATEMENT:
            return prepare_alter_table_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_CREATE_INDEX_STATEMENT:
            return prepare_create_index_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_DROP_INDEX_STATEMENT:
            return prepare_drop_index_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_INSERT_VALUES_STATEMENT:
            return prepare_insert_values_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_INSERT_SET_STATEMENT:
            return prepare_insert_set_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_REPLACE_VALUES_STATEMENT:
            return prepare_replace_values_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_REPLACE_SET_STATEMENT:
            return prepare_replace_set_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_UPDATE_STATEMENT:
            return mylite_dml_prepare_update_statement(database, statement, sql, sql_length,
                                                       out_stmt);
        case MYLITE_SQL_AST_DELETE_STATEMENT:
            return mylite_dml_prepare_delete_statement(database, statement, sql, sql_length,
                                                       out_stmt);
        case MYLITE_SQL_AST_START_TRANSACTION_STATEMENT:
        case MYLITE_SQL_AST_BEGIN_TRANSACTION_STATEMENT:
        case MYLITE_SQL_AST_COMMIT_STATEMENT:
        case MYLITE_SQL_AST_ROLLBACK_STATEMENT:
        case MYLITE_SQL_AST_SAVEPOINT_STATEMENT:
        case MYLITE_SQL_AST_ROLLBACK_TO_SAVEPOINT_STATEMENT:
        case MYLITE_SQL_AST_RELEASE_SAVEPOINT_STATEMENT:
            return prepare_transaction_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_SHOW_SCHEMAS_STATEMENT:
            return mylite_show_prepare_schemas_statement(database, out_stmt);
        case MYLITE_SQL_AST_SHOW_VARIABLES_STATEMENT:
            return mylite_show_prepare_variables_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_SHOW_STATUS_STATEMENT:
            return mylite_show_prepare_status_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_SHOW_ENGINES_STATEMENT:
            return mylite_show_prepare_engines_statement(database, out_stmt);
        case MYLITE_SQL_AST_SHOW_CHARACTER_SET_STATEMENT:
            return mylite_show_prepare_character_set_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_SHOW_COLLATION_STATEMENT:
            return mylite_show_prepare_collation_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_SHOW_TABLES_STATEMENT:
            return mylite_show_prepare_tables_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_SHOW_TABLE_STATUS_STATEMENT:
            return mylite_show_prepare_table_status_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT:
            return mylite_show_prepare_columns_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_SHOW_INDEX_STATEMENT:
            return mylite_show_prepare_index_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_SHOW_CREATE_TABLE_STATEMENT:
            return mylite_show_prepare_create_table_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_SHOW_CREATE_SCHEMA_STATEMENT:
            return mylite_show_prepare_create_schema_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_SHOW_DIAGNOSTICS_STATEMENT:
            return mylite_show_prepare_diagnostics_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_SHOW_DIAGNOSTICS_COUNT_STATEMENT:
            return mylite_show_prepare_diagnostics_count_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_DESCRIBE_TABLE_STATEMENT:
            return mylite_show_prepare_describe_table_statement(database, statement, out_stmt);
        case MYLITE_SQL_AST_QUERY_EXPRESSION:
            return mylite_select_union_prepare_query_expression(
                database, statement, sql, sql_length, out_stmt, &union_query_prepare_callbacks);
        case MYLITE_SQL_AST_SELECT_STATEMENT:
            status = validate_select_duplicate_mode(database, statement);
            if (status != MYLITE_OK) {
                return status;
            }
            status =
                mylite_information_schema_prepare_select_statement(database, statement, out_stmt);
            if (status != MYLITE_UNSUPPORTED) {
                return status;
            }
            status = prepare_table_select_statement(database, statement, sql, sql_length, out_stmt);
            if (status != MYLITE_UNSUPPORTED || database->error_message != NULL) {
                return status;
            }
            status = prepare_scalar_select_statement(database, statement, out_stmt);
            if (status != MYLITE_UNSUPPORTED || database->error_message != NULL) {
                return status;
            }
            break;
        case MYLITE_SQL_AST_SCRIPT:
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
        case MYLITE_SQL_AST_IDENTIFIER:
        case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER:
        case MYLITE_SQL_AST_WILDCARD:
        case MYLITE_SQL_AST_LITERAL:
        case MYLITE_SQL_AST_UNARY_EXPRESSION:
        case MYLITE_SQL_AST_BINARY_EXPRESSION:
        case MYLITE_SQL_AST_TERNARY_EXPRESSION:
        case MYLITE_SQL_AST_EXPRESSION_LIST:
        case MYLITE_SQL_AST_FUNCTION_CALL:
        case MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST:
        case MYLITE_SQL_AST_CASE_EXPRESSION:
        case MYLITE_SQL_AST_CASE_WHEN_LIST:
        case MYLITE_SQL_AST_CASE_WHEN:
        case MYLITE_SQL_AST_CAST_EXPRESSION:
        case MYLITE_SQL_AST_GROUP_BY_CLAUSE:
        case MYLITE_SQL_AST_GROUP_ITEM_LIST:
        case MYLITE_SQL_AST_GROUP_ITEM:
        case MYLITE_SQL_AST_HAVING_CLAUSE:
        case MYLITE_SQL_AST_AGGREGATE_CALL:
        case MYLITE_SQL_AST_WHERE_CLAUSE:
        case MYLITE_SQL_AST_ORDER_BY_CLAUSE:
        case MYLITE_SQL_AST_ORDER_ITEM_LIST:
        case MYLITE_SQL_AST_ORDER_ITEM:
        case MYLITE_SQL_AST_LIMIT_CLAUSE:
        case MYLITE_SQL_AST_LIMIT_BOUND:
        case MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION:
        case MYLITE_SQL_AST_IF_EXISTS:
        case MYLITE_SQL_AST_IF_NOT_EXISTS:
        case MYLITE_SQL_AST_SCHEMA_OPTION_LIST:
        case MYLITE_SQL_AST_SCHEMA_OPTION:
        case MYLITE_SQL_AST_DEFAULT:
        case MYLITE_SQL_AST_COLUMN_DEFINITION_LIST:
        case MYLITE_SQL_AST_COLUMN_DEFINITION:
        case MYLITE_SQL_AST_COLUMN_TYPE:
        case MYLITE_SQL_AST_COLUMN_TYPE_ATTRIBUTE_LIST:
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE_LIST:
        case MYLITE_SQL_AST_COLUMN_ATTRIBUTE:
        case MYLITE_SQL_AST_CURRENT_TIMESTAMP:
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
        case MYLITE_SQL_AST_TABLE_NAME_LIST:
        case MYLITE_SQL_AST_INSERT_COLUMN_LIST:
        case MYLITE_SQL_AST_INSERT_ROW:
        case MYLITE_SQL_AST_INSERT_ROW_LIST:
        case MYLITE_SQL_AST_INSERT_VALUE_LIST:
        case MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT_LIST:
        case MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT:
        case MYLITE_SQL_AST_UPDATE_TARGET:
        case MYLITE_SQL_AST_UPDATE_ASSIGNMENT_LIST:
        case MYLITE_SQL_AST_UPDATE_ASSIGNMENT:
        case MYLITE_SQL_AST_UPDATE_LIMIT_CLAUSE:
        case MYLITE_SQL_AST_DELETE_TARGET:
        case MYLITE_SQL_AST_DELETE_LIMIT_CLAUSE:
        case MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC_LIST:
        case MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC:
        case MYLITE_SQL_AST_TRANSACTION_COMPLETION:
        case MYLITE_SQL_AST_SUBQUERY_EXPRESSION:
        case MYLITE_SQL_AST_EXISTS_EXPRESSION:
        case MYLITE_SQL_AST_QUANTIFIED_COMPARISON:
        case MYLITE_SQL_AST_ROW_CONSTRUCTOR:
        case MYLITE_SQL_AST_UNION_EXPRESSION:
        case MYLITE_SQL_AST_QUERY_PRIMARY:
        case MYLITE_SQL_AST_INSERT_DUPLICATE_UPDATE_CLAUSE:
        case MYLITE_SQL_AST_INSERT_UPDATE_ASSIGNMENT_LIST:
        case MYLITE_SQL_AST_INSERT_UPDATE_ASSIGNMENT:
        case MYLITE_SQL_AST_INSERT_ROW_ALIAS:
        case MYLITE_SQL_AST_INSERT_ALIAS_COLUMN_LIST:
        case MYLITE_SQL_AST_DDL_TABLE_OPTION_LIST:
        case MYLITE_SQL_AST_DDL_TABLE_OPTION:
        case MYLITE_SQL_AST_ALTER_TABLE_ITEM_LIST:
        case MYLITE_SQL_AST_ALTER_TABLE_ACTION:
        case MYLITE_SQL_AST_ALTER_TABLE_COLUMN_POSITION:
        case MYLITE_SQL_AST_RENAME_TABLE_PAIR_LIST:
        case MYLITE_SQL_AST_RENAME_TABLE_PAIR:
            break;
        }
    }

    translate_status = mylite_sqlite_translate(root, &translate_result);
    if (translate_status != MYLITE_SQLITE_TRANSLATE_OK) {
        return mylite_statement_map_translate_status(database, translate_status);
    }

    status = mylite_statement_prepare_sqlite(database, translate_result.sql, out_stmt);
    mylite_sqlite_translate_result_deinit(&translate_result);
    return status;
}

static int prepare_schema_lifecycle_statement(mylite_db *database,
                                              const struct mylite_sql_ast_node *statement,
                                              mylite_stmt **out_stmt)
{
    enum mylite_stmt_kind kind = MYLITE_STMT_SQLITE;

    switch (statement->kind) {
    case MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT:
        kind = MYLITE_STMT_CREATE_SCHEMA;
        break;
    case MYLITE_SQL_AST_ALTER_SCHEMA_STATEMENT:
        kind = MYLITE_STMT_ALTER_SCHEMA;
        break;
    case MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT:
        kind = MYLITE_STMT_DROP_SCHEMA;
        break;
    case MYLITE_SQL_AST_USE_STATEMENT:
        kind = MYLITE_STMT_USE_SCHEMA;
        break;
    case MYLITE_SQL_AST_SCRIPT:
    case MYLITE_SQL_AST_SELECT_STATEMENT:
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
    case MYLITE_SQL_AST_IDENTIFIER:
    case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER:
    case MYLITE_SQL_AST_WILDCARD:
    case MYLITE_SQL_AST_LITERAL:
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_TERNARY_EXPRESSION:
    case MYLITE_SQL_AST_EXPRESSION_LIST:
    case MYLITE_SQL_AST_FUNCTION_CALL:
    case MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST:
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP:
    case MYLITE_SQL_AST_CASE_EXPRESSION:
    case MYLITE_SQL_AST_CASE_WHEN_LIST:
    case MYLITE_SQL_AST_CASE_WHEN:
    case MYLITE_SQL_AST_CAST_EXPRESSION:
    case MYLITE_SQL_AST_SUBQUERY_EXPRESSION:
    case MYLITE_SQL_AST_EXISTS_EXPRESSION:
    case MYLITE_SQL_AST_QUANTIFIED_COMPARISON:
    case MYLITE_SQL_AST_ROW_CONSTRUCTOR:
    case MYLITE_SQL_AST_QUERY_EXPRESSION:
    case MYLITE_SQL_AST_UNION_EXPRESSION:
    case MYLITE_SQL_AST_QUERY_PRIMARY:
    case MYLITE_SQL_AST_INSERT_DUPLICATE_UPDATE_CLAUSE:
    case MYLITE_SQL_AST_INSERT_UPDATE_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_INSERT_UPDATE_ASSIGNMENT:
    case MYLITE_SQL_AST_INSERT_ROW_ALIAS:
    case MYLITE_SQL_AST_INSERT_ALIAS_COLUMN_LIST:
    case MYLITE_SQL_AST_GROUP_BY_CLAUSE:
    case MYLITE_SQL_AST_GROUP_ITEM_LIST:
    case MYLITE_SQL_AST_GROUP_ITEM:
    case MYLITE_SQL_AST_HAVING_CLAUSE:
    case MYLITE_SQL_AST_AGGREGATE_CALL:
    case MYLITE_SQL_AST_WHERE_CLAUSE:
    case MYLITE_SQL_AST_ORDER_BY_CLAUSE:
    case MYLITE_SQL_AST_ORDER_ITEM_LIST:
    case MYLITE_SQL_AST_ORDER_ITEM:
    case MYLITE_SQL_AST_LIMIT_CLAUSE:
    case MYLITE_SQL_AST_LIMIT_BOUND:
    case MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION:
    case MYLITE_SQL_AST_SHOW_SCHEMAS_STATEMENT:
    case MYLITE_SQL_AST_SET_NAMES_STATEMENT:
    case MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT:
    case MYLITE_SQL_AST_CREATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_DROP_TABLE_STATEMENT:
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
    case MYLITE_SQL_AST_ALTER_TABLE_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_ITEM_LIST:
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION:
    case MYLITE_SQL_AST_ALTER_TABLE_COLUMN_POSITION:
    case MYLITE_SQL_AST_CREATE_INDEX_STATEMENT:
    case MYLITE_SQL_AST_DROP_INDEX_STATEMENT:
    case MYLITE_SQL_AST_INSERT_VALUES_STATEMENT:
    case MYLITE_SQL_AST_INSERT_SET_STATEMENT:
    case MYLITE_SQL_AST_UPDATE_STATEMENT:
    case MYLITE_SQL_AST_DEFAULT:
    case MYLITE_SQL_AST_IF_EXISTS:
    case MYLITE_SQL_AST_IF_NOT_EXISTS:
    case MYLITE_SQL_AST_SCHEMA_OPTION_LIST:
    case MYLITE_SQL_AST_SCHEMA_OPTION:
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
    case MYLITE_SQL_AST_DDL_TABLE_OPTION_LIST:
    case MYLITE_SQL_AST_DDL_TABLE_OPTION:
    case MYLITE_SQL_AST_TABLE_NAME_LIST:
    case MYLITE_SQL_AST_INSERT_COLUMN_LIST:
    case MYLITE_SQL_AST_INSERT_ROW:
    case MYLITE_SQL_AST_INSERT_ROW_LIST:
    case MYLITE_SQL_AST_INSERT_VALUE_LIST:
    case MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_INSERT_SET_ASSIGNMENT:
    case MYLITE_SQL_AST_REPLACE_VALUES_STATEMENT:
    case MYLITE_SQL_AST_REPLACE_SET_STATEMENT:
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
        return MYLITE_UNSUPPORTED;
    }

    return prepare_custom_statement(database, kind, statement, out_stmt);
}

static int prepare_create_table_statement(mylite_db *database,
                                          const struct mylite_sql_ast_node *statement,
                                          mylite_stmt **out_stmt)
{
    return prepare_custom_statement(database, MYLITE_STMT_CREATE_TABLE, statement, out_stmt);
}

static int prepare_drop_table_statement(mylite_db *database,
                                        const struct mylite_sql_ast_node *statement,
                                        mylite_stmt **out_stmt)
{
    return prepare_custom_statement(database, MYLITE_STMT_DROP_TABLE, statement, out_stmt);
}

static int prepare_rename_table_statement(mylite_db *database,
                                          const struct mylite_sql_ast_node *statement,
                                          mylite_stmt **out_stmt)
{
    return prepare_custom_statement(database, MYLITE_STMT_RENAME_TABLE, statement, out_stmt);
}

static int prepare_truncate_table_statement(mylite_db *database,
                                            const struct mylite_sql_ast_node *statement,
                                            mylite_stmt **out_stmt)
{
    return prepare_custom_statement(database, MYLITE_STMT_TRUNCATE_TABLE, statement, out_stmt);
}

static int prepare_alter_table_statement(mylite_db *database,
                                         const struct mylite_sql_ast_node *statement,
                                         mylite_stmt **out_stmt)
{
    return prepare_custom_statement(database, MYLITE_STMT_ALTER_TABLE, statement, out_stmt);
}

static int prepare_create_index_statement(mylite_db *database,
                                          const struct mylite_sql_ast_node *statement,
                                          mylite_stmt **out_stmt)
{
    return prepare_custom_statement(database, MYLITE_STMT_CREATE_INDEX, statement, out_stmt);
}

static int prepare_drop_index_statement(mylite_db *database,
                                        const struct mylite_sql_ast_node *statement,
                                        mylite_stmt **out_stmt)
{
    return prepare_custom_statement(database, MYLITE_STMT_DROP_INDEX, statement, out_stmt);
}

static int prepare_insert_values_statement(mylite_db *database,
                                           const struct mylite_sql_ast_node *statement,
                                           mylite_stmt **out_stmt)
{
    return prepare_custom_statement(database, MYLITE_STMT_INSERT_VALUES, statement, out_stmt);
}

static int prepare_insert_set_statement(mylite_db *database,
                                        const struct mylite_sql_ast_node *statement,
                                        mylite_stmt **out_stmt)
{
    return prepare_custom_statement(database, MYLITE_STMT_INSERT_SET, statement, out_stmt);
}

static int prepare_replace_values_statement(mylite_db *database,
                                            const struct mylite_sql_ast_node *statement,
                                            mylite_stmt **out_stmt)
{
    return prepare_custom_statement(database, MYLITE_STMT_REPLACE_VALUES, statement, out_stmt);
}

static int prepare_replace_set_statement(mylite_db *database,
                                         const struct mylite_sql_ast_node *statement,
                                         mylite_stmt **out_stmt)
{
    return prepare_custom_statement(database, MYLITE_STMT_REPLACE_SET, statement, out_stmt);
}

static int prepare_transaction_statement(mylite_db *database,
                                         const struct mylite_sql_ast_node *statement,
                                         mylite_stmt **out_stmt)
{
    enum mylite_stmt_kind kind = MYLITE_STMT_SQLITE;

    switch (statement->kind) {
    case MYLITE_SQL_AST_START_TRANSACTION_STATEMENT:
        kind = MYLITE_STMT_START_TRANSACTION;
        break;
    case MYLITE_SQL_AST_BEGIN_TRANSACTION_STATEMENT:
        kind = MYLITE_STMT_BEGIN_TRANSACTION;
        break;
    case MYLITE_SQL_AST_COMMIT_STATEMENT:
        kind = MYLITE_STMT_COMMIT;
        break;
    case MYLITE_SQL_AST_ROLLBACK_STATEMENT:
        kind = MYLITE_STMT_ROLLBACK;
        break;
    case MYLITE_SQL_AST_SAVEPOINT_STATEMENT:
        kind = MYLITE_STMT_SAVEPOINT;
        break;
    case MYLITE_SQL_AST_ROLLBACK_TO_SAVEPOINT_STATEMENT:
        kind = MYLITE_STMT_ROLLBACK_TO_SAVEPOINT;
        break;
    case MYLITE_SQL_AST_RELEASE_SAVEPOINT_STATEMENT:
        kind = MYLITE_STMT_RELEASE_SAVEPOINT;
        break;
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
    case MYLITE_SQL_AST_IDENTIFIER:
    case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER:
    case MYLITE_SQL_AST_WILDCARD:
    case MYLITE_SQL_AST_LITERAL:
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP:
    case MYLITE_SQL_AST_SUBQUERY_EXPRESSION:
    case MYLITE_SQL_AST_EXISTS_EXPRESSION:
    case MYLITE_SQL_AST_QUANTIFIED_COMPARISON:
    case MYLITE_SQL_AST_ROW_CONSTRUCTOR:
    case MYLITE_SQL_AST_QUERY_EXPRESSION:
    case MYLITE_SQL_AST_UNION_EXPRESSION:
    case MYLITE_SQL_AST_QUERY_PRIMARY:
    case MYLITE_SQL_AST_INSERT_DUPLICATE_UPDATE_CLAUSE:
    case MYLITE_SQL_AST_INSERT_UPDATE_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_INSERT_UPDATE_ASSIGNMENT:
    case MYLITE_SQL_AST_INSERT_ROW_ALIAS:
    case MYLITE_SQL_AST_INSERT_ALIAS_COLUMN_LIST:
    case MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION:
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
    case MYLITE_SQL_AST_CREATE_INDEX_STATEMENT:
    case MYLITE_SQL_AST_DROP_INDEX_STATEMENT:
    case MYLITE_SQL_AST_DDL_TABLE_OPTION_LIST:
    case MYLITE_SQL_AST_DDL_TABLE_OPTION:
    case MYLITE_SQL_AST_ALTER_TABLE_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_ITEM_LIST:
    case MYLITE_SQL_AST_ALTER_TABLE_ACTION:
    case MYLITE_SQL_AST_ALTER_TABLE_COLUMN_POSITION:
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
    case MYLITE_SQL_AST_TERNARY_EXPRESSION:
    case MYLITE_SQL_AST_EXPRESSION_LIST:
    case MYLITE_SQL_AST_FUNCTION_CALL:
    case MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST:
    case MYLITE_SQL_AST_CASE_EXPRESSION:
    case MYLITE_SQL_AST_CASE_WHEN_LIST:
    case MYLITE_SQL_AST_CASE_WHEN:
    case MYLITE_SQL_AST_CAST_EXPRESSION:
    case MYLITE_SQL_AST_GROUP_BY_CLAUSE:
    case MYLITE_SQL_AST_GROUP_ITEM_LIST:
    case MYLITE_SQL_AST_GROUP_ITEM:
    case MYLITE_SQL_AST_HAVING_CLAUSE:
    case MYLITE_SQL_AST_AGGREGATE_CALL:
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
    case MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC_LIST:
    case MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC:
    case MYLITE_SQL_AST_TRANSACTION_COMPLETION:
        return MYLITE_UNSUPPORTED;
    }

    return prepare_custom_statement(database, kind, statement, out_stmt);
}

static int validate_select_duplicate_mode(mylite_db *database,
                                          const struct mylite_sql_ast_node *statement)
{
    if (statement == NULL || statement->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
        return MYLITE_OK;
    }
    if (statement->select_duplicate_mode_conflict) {
        return set_select_duplicate_mode_error(database);
    }
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int prepare_table_select_statement(mylite_db *database,
                                          const struct mylite_sql_ast_node *statement,
                                          const char *sql, size_t sql_length,
                                          mylite_stmt **out_stmt)
{
    const struct mylite_sql_ast_node *select_list = mylite_ast_child_at(statement, 0U);
    const struct mylite_sql_ast_node *from_clause = mylite_ast_child_at(statement, 1U);
    const struct mylite_sql_ast_node *where_clause =
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_WHERE_CLAUSE);
    const struct mylite_sql_ast_node *group_by_clause =
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_GROUP_BY_CLAUSE);
    const struct mylite_sql_ast_node *having_clause =
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_HAVING_CLAUSE);
    const struct mylite_sql_ast_node *order_by_clause =
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    const struct mylite_sql_ast_node *limit_clause =
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE);
    const struct mylite_select_clause_nodes clauses = {
        .where = where_clause,
        .group_by = group_by_clause,
        .having = having_clause,
        .order_by = order_by_clause,
        .limit = limit_clause,
    };
    bool custom_runtime = false;
    struct mylite_select_plan plan = {0};
    int status = MYLITE_OK;

    if (from_clause == NULL || (from_clause->kind != MYLITE_SQL_AST_FROM_TABLE &&
                                from_clause->kind != MYLITE_SQL_AST_FROM_TABLE_REFERENCES)) {
        return MYLITE_UNSUPPORTED;
    }

    plan.duplicate_mode = statement->select_duplicate_mode;
    status = mylite_select_bind_from_clause(database, from_clause, &plan);
    if (status == MYLITE_OK) {
        status = bind_select_join_predicates(database, &plan);
    }
    if (status == MYLITE_OK) {
        status = mylite_select_build_outputs(database, select_list, true, &plan,
                                             &select_projection_callbacks);
    }
    if (status == MYLITE_OK && mylite_select_plan_table_count(&plan) > 1U &&
        (group_by_clause != NULL || having_clause != NULL)) {
        status = set_select_unsupported_join_grouping_error(database);
    }
    if (status == MYLITE_OK) {
        custom_runtime = mylite_select_plan_requires_custom_runtime(&plan, &clauses);
    }
    if (status == MYLITE_OK) {
        status = bind_table_select_clauses(database, &clauses, &plan);
    }
    if (status == MYLITE_OK && custom_runtime) {
        status = mylite_select_prepare_custom_table_statement(
            database, where_clause, sql, sql_length, &plan, out_stmt, &select_statement_callbacks);
    }
    if (status == MYLITE_OK && !custom_runtime) {
        status = prepare_table_select_sqlite_statement(database, &plan, out_stmt);
    }

    mylite_select_plan_deinit(&plan);
    return status;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int prepare_table_select_sqlite_statement(mylite_db *database,
                                                 const struct mylite_select_plan *plan,
                                                 mylite_stmt **out_stmt)
{
    char *sqlite_sql = mylite_select_build_physical_sql(database, plan);
    int status = MYLITE_OK;

    if (sqlite_sql == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    status = mylite_statement_prepare_sqlite(database, sqlite_sql, out_stmt);
    if (status == MYLITE_OK) {
        status = mylite_select_attach_result_metadata(*out_stmt, plan, &select_metadata_callbacks);
        if (status != MYLITE_OK) {
            mylite_finalize(*out_stmt);
            *out_stmt = NULL;
        }
    }
    sqlite3_free(sqlite_sql);
    return status;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int prepare_scalar_select_statement(mylite_db *database,
                                           const struct mylite_sql_ast_node *statement,
                                           mylite_stmt **out_stmt)
{
    const struct mylite_sql_ast_node *select_list = mylite_ast_child_at(statement, 0U);
    const struct mylite_sql_ast_node *from_clause = mylite_ast_child_at(statement, 1U);

    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST ||
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_FROM_TABLE) != NULL ||
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_FROM_TABLE_REFERENCES) != NULL ||
        (from_clause != NULL && from_clause->kind != MYLITE_SQL_AST_FROM_DUAL &&
         from_clause->kind != MYLITE_SQL_AST_ORDER_BY_CLAUSE &&
         from_clause->kind != MYLITE_SQL_AST_LIMIT_CLAUSE)) {
        return MYLITE_UNSUPPORTED;
    }
    for (const struct mylite_sql_ast_node *item = select_list->first_child; item != NULL;
         item = item->next_sibling) {
        if (item->kind != MYLITE_SQL_AST_SELECT_ITEM || mylite_ast_child_at(item, 0U) == NULL) {
            return MYLITE_UNSUPPORTED;
        }
    }
    return prepare_custom_statement(database, MYLITE_STMT_SCALAR_SELECT, statement, out_stmt);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int prepare_select_subquery_statement(mylite_db *database,
                                             const struct mylite_sql_ast_node *statement,
                                             mylite_stmt **out_stmt)
{
    const char *sql = statement == NULL ? NULL : statement->span.text;
    size_t sql_length = statement == NULL ? 0U : statement->span.length;
    int status = MYLITE_OK;

    if (statement == NULL || statement->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
        return MYLITE_UNSUPPORTED;
    }

    status = validate_select_duplicate_mode(database, statement);
    if (status != MYLITE_OK) {
        return status;
    }
    status = mylite_information_schema_prepare_select_statement(database, statement, out_stmt);
    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }
    status = prepare_table_select_statement(database, statement, sql, sql_length, out_stmt);
    if (status != MYLITE_UNSUPPORTED || database->error_message != NULL) {
        return status;
    }
    return prepare_scalar_select_statement(database, statement, out_stmt);
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
        return infer_literal_descriptor(database, node, value, out_descriptor);
    case MYLITE_SQL_AST_IDENTIFIER:
    case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER:
        return infer_identifier_descriptor(database, plan, node, out_descriptor);
    case MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION:
        break;
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
        return infer_unary_expression_descriptor(database, plan, node, value, out_descriptor);
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
        return infer_binary_expression_descriptor(database, plan, node, value, out_descriptor);
    case MYLITE_SQL_AST_TERNARY_EXPRESSION:
        return infer_ternary_expression_descriptor(database, plan, node, value, out_descriptor);
    case MYLITE_SQL_AST_CASE_EXPRESSION:
        return infer_case_expression_descriptor(database, plan, node, out_descriptor);
    case MYLITE_SQL_AST_FUNCTION_CALL:
        return infer_function_expression_descriptor(database, plan, node, value, out_descriptor);
    case MYLITE_SQL_AST_AGGREGATE_CALL:
        return infer_aggregate_expression_descriptor(database, plan, node, out_descriptor);
    case MYLITE_SQL_AST_CAST_EXPRESSION:
        return infer_cast_expression_descriptor(database, plan, node, value, out_descriptor);
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP: {
        unsigned int fsp = 0U;

        if (node->has_column_precision) {
            fsp = (unsigned int)node->column_precision;
        }
        *out_descriptor = mylite_expression_descriptor_current_datetime_function(fsp);
        return MYLITE_OK;
    }
    case MYLITE_SQL_AST_SUBQUERY_EXPRESSION:
        return infer_scalar_subquery_expression_descriptor(database, node, out_descriptor);
    case MYLITE_SQL_AST_EXISTS_EXPRESSION:
        *out_descriptor = mylite_expression_descriptor_boolean(false);
        return MYLITE_OK;
    case MYLITE_SQL_AST_QUANTIFIED_COMPARISON:
        return infer_quantified_subquery_expression_descriptor(database, plan, node,
                                                               out_descriptor);
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

static int infer_identifier_descriptor(mylite_db *database, const struct mylite_select_plan *plan,
                                       const struct mylite_sql_ast_node *expression,
                                       struct mylite_field_descriptor *out_descriptor)
{
    size_t column_index = plan == NULL ? 0U : mylite_select_plan_column_count(plan);
    int status = plan == NULL ? MYLITE_UNSUPPORTED
                              : mylite_select_resolve_plan_column_reference(
                                    database, plan, expression, "field list", &column_index);

    if (status != MYLITE_OK || column_index >= mylite_select_plan_column_count(plan)) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return status == MYLITE_OK ? MYLITE_UNSUPPORTED : status;
    }

    const struct mylite_select_column *column =
        mylite_select_plan_column_const(plan, column_index, NULL);

    if (column == NULL) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return MYLITE_UNSUPPORTED;
    }
    *out_descriptor = column->descriptor;
    return MYLITE_OK;
}

static int infer_literal_descriptor(mylite_db *database,
                                    const struct mylite_sql_ast_node *expression,
                                    const struct mylite_expression_value *value,
                                    struct mylite_field_descriptor *out_descriptor)
{
    struct mylite_field_descriptor descriptor = mylite_expression_descriptor_defaults();

    switch (expression->literal_kind) {
    case MYLITE_SQL_AST_LITERAL_NULL:
        descriptor.flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM;
        break;
    case MYLITE_SQL_AST_LITERAL_TRUE:
    case MYLITE_SQL_AST_LITERAL_FALSE:
    case MYLITE_SQL_AST_LITERAL_INTEGER:
        descriptor = mylite_expression_descriptor_signed_longlong(false);
        descriptor.length = mylite_expression_descriptor_literal_integer_length(expression, value);
        if (value != NULL && value->kind == MYLITE_EXPRESSION_VALUE_UINT64) {
            descriptor.flags |= MYLITE_FIELD_FLAG_UNSIGNED;
        }
        break;
    case MYLITE_SQL_AST_LITERAL_DECIMAL:
    case MYLITE_SQL_AST_LITERAL_FLOAT:
        descriptor = mylite_expression_descriptor_decimal(false);
        descriptor.decimals = mylite_expression_descriptor_literal_decimal_scale(expression);
        descriptor.length = expression->span.length + 1U;
        break;
    case MYLITE_SQL_AST_LITERAL_STRING:
    case MYLITE_SQL_AST_LITERAL_NATIONAL_STRING:
        descriptor.type = MYLITE_FIELD_TYPE_VAR_STRING;
        descriptor.length = mylite_expression_descriptor_string_length(database, value, expression);
        descriptor.decimals = mylite_mysql_not_fixed_decimals;
        descriptor.charset_id = mylite_expression_descriptor_connection_charset_id(database);
        mylite_field_descriptor_set_not_null(&descriptor, true);
        break;
    case MYLITE_SQL_AST_LITERAL_HEX:
    case MYLITE_SQL_AST_LITERAL_BIT:
    case MYLITE_SQL_AST_LITERAL_NONE:
        descriptor = mylite_expression_descriptor_from_value(value);
        break;
    }

    *out_descriptor = descriptor;
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_unary_expression_descriptor(mylite_db *database,
                                             const struct mylite_select_plan *plan,
                                             const struct mylite_sql_ast_node *expression,
                                             const struct mylite_expression_value *value,
                                             struct mylite_field_descriptor *out_descriptor)
{
    struct mylite_field_descriptor operand = mylite_expression_descriptor_defaults();
    bool nullable = true;
    int status = MYLITE_OK;

    switch (expression->operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN:
        *out_descriptor = mylite_expression_descriptor_boolean(false);
        return MYLITE_OK;
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT:
        status = infer_expression_descriptor(database, plan, mylite_ast_child_at(expression, 0U),
                                             NULL, &operand);
        if (status != MYLITE_OK) {
            return status;
        }
        *out_descriptor = mylite_expression_descriptor_boolean(
            mylite_expression_descriptor_is_nullable(&operand));
        return MYLITE_OK;
    case MYLITE_SQL_AST_OPERATOR_BITWISE_NOT:
        status = infer_expression_descriptor(database, plan, mylite_ast_child_at(expression, 0U),
                                             NULL, &operand);
        if (status != MYLITE_OK) {
            return status;
        }
        *out_descriptor = mylite_expression_descriptor_unsigned_longlong(
            mylite_expression_descriptor_is_nullable(&operand));
        return MYLITE_OK;
    case MYLITE_SQL_AST_OPERATOR_POSITIVE:
    case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
        status = infer_expression_descriptor(database, plan, mylite_ast_child_at(expression, 0U),
                                             value, &operand);
        if (status != MYLITE_OK) {
            return status;
        }
        nullable = mylite_expression_descriptor_is_nullable(&operand);
        operand.flags &= ~(unsigned int)MYLITE_FIELD_FLAG_UNSIGNED;
        operand.length = mylite_expression_descriptor_max_u64(operand.length, 2U);
        mylite_field_descriptor_set_nullable(&operand, nullable);
        *out_descriptor = operand;
        return MYLITE_OK;
    case MYLITE_SQL_AST_OPERATOR_NONE:
    case MYLITE_SQL_AST_OPERATOR_ADD:
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
    case MYLITE_SQL_AST_OPERATOR_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LESS:
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_GREATER:
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_AND:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_XOR:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_OR:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_LEFT:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_RIGHT:
    case MYLITE_SQL_AST_OPERATOR_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_NOT_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_LIKE:
    case MYLITE_SQL_AST_OPERATOR_NOT_LIKE:
    case MYLITE_SQL_AST_OPERATOR_IN:
    case MYLITE_SQL_AST_OPERATOR_NOT_IN:
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_MODULO:
        break;
    }

    *out_descriptor = mylite_expression_descriptor_from_value(value);
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_binary_expression_descriptor(mylite_db *database,
                                              const struct mylite_select_plan *plan,
                                              const struct mylite_sql_ast_node *expression,
                                              const struct mylite_expression_value *value,
                                              struct mylite_field_descriptor *out_descriptor)
{
    if (mylite_select_subquery_binary_expression_is_row(expression)) {
        return infer_row_subquery_expression_descriptor(database, plan, expression, out_descriptor);
    }
    if (mylite_select_subquery_binary_expression_is_in(expression)) {
        return infer_in_subquery_expression_descriptor(database, plan, expression, out_descriptor);
    }

    struct mylite_field_descriptor left = mylite_expression_descriptor_defaults();
    struct mylite_field_descriptor right = mylite_expression_descriptor_defaults();
    bool nullable = true;
    int status = infer_expression_descriptor(database, plan, mylite_ast_child_at(expression, 0U),
                                             NULL, &left);

    if (status == MYLITE_OK) {
        status = infer_expression_descriptor(database, plan, mylite_ast_child_at(expression, 1U),
                                             NULL, &right);
    }
    if (status != MYLITE_OK) {
        return status;
    }

    if (mylite_expression_descriptor_is_nullable(&left) ||
        mylite_expression_descriptor_is_nullable(&right)) {
        nullable = true;
    } else {
        nullable = false;
    }
    switch (expression->operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_ADD:
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
        *out_descriptor = mylite_expression_descriptor_signed_longlong(nullable);
        out_descriptor->length =
            mylite_expression_descriptor_max_u64(left.length, right.length) + 1U;
        if ((left.flags & MYLITE_FIELD_FLAG_UNSIGNED) != 0U ||
            (right.flags & MYLITE_FIELD_FLAG_UNSIGNED) != 0U) {
            out_descriptor->flags |= MYLITE_FIELD_FLAG_UNSIGNED;
        }
        return MYLITE_OK;
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_MODULO:
        *out_descriptor = mylite_expression_descriptor_signed_longlong(nullable);
        out_descriptor->length = mylite_expression_descriptor_max_u64(left.length, right.length);
        return MYLITE_OK;
    case MYLITE_SQL_AST_OPERATOR_DIVIDE:
        *out_descriptor = mylite_expression_descriptor_decimal(nullable);
        return MYLITE_OK;
    case MYLITE_SQL_AST_OPERATOR_BITWISE_AND:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_XOR:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_OR:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_LEFT:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_RIGHT:
        *out_descriptor = mylite_expression_descriptor_unsigned_longlong(nullable);
        return MYLITE_OK;
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LESS:
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_GREATER:
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
    case MYLITE_SQL_AST_OPERATOR_IN:
    case MYLITE_SQL_AST_OPERATOR_NOT_IN:
    case MYLITE_SQL_AST_OPERATOR_LIKE:
    case MYLITE_SQL_AST_OPERATOR_NOT_LIKE: {
        bool descriptor_nullable = false;

        if (nullable) {
            bool forces_not_null =
                mylite_expression_descriptor_operator_forces_not_null(expression->operator_kind);

            if (!forces_not_null) {
                descriptor_nullable = true;
            }
        }
        *out_descriptor = mylite_expression_descriptor_boolean(descriptor_nullable);
        return MYLITE_OK;
    }
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
        *out_descriptor = mylite_expression_descriptor_boolean(false);
        return MYLITE_OK;
    case MYLITE_SQL_AST_OPERATOR_NONE:
    case MYLITE_SQL_AST_OPERATOR_POSITIVE:
    case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_NOT:
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_NOT_BETWEEN:
        break;
    }

    *out_descriptor = mylite_expression_descriptor_from_value(value);
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_ternary_expression_descriptor(mylite_db *database,
                                               const struct mylite_select_plan *plan,
                                               const struct mylite_sql_ast_node *expression,
                                               const struct mylite_expression_value *value,
                                               struct mylite_field_descriptor *out_descriptor)
{
    bool nullable = false;

    (void)value;
    for (const struct mylite_sql_ast_node *child = expression->first_child; child != NULL;
         child = child->next_sibling) {
        struct mylite_field_descriptor child_descriptor = mylite_expression_descriptor_defaults();
        int status = infer_expression_descriptor(database, plan, child, NULL, &child_descriptor);

        if (status != MYLITE_OK) {
            return status;
        }
        if (mylite_expression_descriptor_is_nullable(&child_descriptor)) {
            nullable = true;
        }
    }

    switch (expression->operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_NOT_BETWEEN:
    case MYLITE_SQL_AST_OPERATOR_LIKE:
    case MYLITE_SQL_AST_OPERATOR_NOT_LIKE:
        *out_descriptor = mylite_expression_descriptor_boolean(nullable);
        return MYLITE_OK;
    case MYLITE_SQL_AST_OPERATOR_NONE:
    case MYLITE_SQL_AST_OPERATOR_POSITIVE:
    case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
    case MYLITE_SQL_AST_OPERATOR_ADD:
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
    case MYLITE_SQL_AST_OPERATOR_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LESS:
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_GREATER:
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_AND:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR:
    case MYLITE_SQL_AST_OPERATOR_LOGICAL_OR:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_NOT:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_AND:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_XOR:
    case MYLITE_SQL_AST_OPERATOR_BITWISE_OR:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_LEFT:
    case MYLITE_SQL_AST_OPERATOR_SHIFT_RIGHT:
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_TRUE:
    case MYLITE_SQL_AST_OPERATOR_IS_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_FALSE:
    case MYLITE_SQL_AST_OPERATOR_IS_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_UNKNOWN:
    case MYLITE_SQL_AST_OPERATOR_IN:
    case MYLITE_SQL_AST_OPERATOR_NOT_IN:
    case MYLITE_SQL_AST_OPERATOR_INTEGER_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_MODULO:
        break;
    }

    *out_descriptor = mylite_expression_descriptor_from_value(value);
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_function_expression_descriptor(mylite_db *database,
                                                const struct mylite_select_plan *plan,
                                                const struct mylite_sql_ast_node *expression,
                                                const struct mylite_expression_value *value,
                                                struct mylite_field_descriptor *out_descriptor)
{
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    bool nullable = false;
    bool result_nullable = false;
    bool matched_string_encoding = false;
    bool matched_slice_string = false;
    int status = MYLITE_OK;

    if (!mylite_expression_is_supported_function_call(expression)) {
        return MYLITE_UNSUPPORTED;
    }
    status = infer_function_arguments_nullable(database, plan, arguments, &nullable);
    if (status != MYLITE_OK) {
        return status;
    }
    result_nullable = mylite_expression_descriptor_function_result_nullable(nullable, value);

    if (infer_common_scalar_function_descriptor(database, name, nullable, result_nullable,
                                                out_descriptor)) {
        return MYLITE_OK;
    }
    status = infer_temporal_function_descriptor(database, plan, expression, value, out_descriptor);
    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }
    status = infer_variadic_scalar_function_descriptor(database, plan, expression, value,
                                                       result_nullable, out_descriptor);
    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }
    status = mylite_expression_descriptor_infer_string_encoding_function(
        database, plan, expression, out_descriptor, &string_descriptor_callbacks,
        &matched_string_encoding);
    if (status != MYLITE_OK || matched_string_encoding) {
        return status;
    }
    status = mylite_expression_descriptor_infer_slice_string_function(
        database, plan, expression, value, nullable, out_descriptor, &string_descriptor_callbacks,
        &matched_slice_string);
    if (status != MYLITE_OK || matched_slice_string) {
        return status;
    }
    if (mylite_expression_descriptor_infer_text_function(database, name, value, result_nullable,
                                                         out_descriptor)) {
        return MYLITE_OK;
    }
    if (mylite_expression_descriptor_infer_fixed_integer_function(name, result_nullable,
                                                                  out_descriptor)) {
        return MYLITE_OK;
    }
    if (mylite_expression_descriptor_infer_code_search_function(name, result_nullable,
                                                                out_descriptor)) {
        return MYLITE_OK;
    }
    if (mylite_expression_descriptor_infer_list_index_function(name, result_nullable,
                                                               out_descriptor)) {
        return MYLITE_OK;
    }
    if (mylite_expression_descriptor_infer_scalar_numeric_function(name, value, result_nullable,
                                                                   out_descriptor)) {
        return MYLITE_OK;
    }

    *out_descriptor = mylite_expression_descriptor_from_value(value);
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_temporal_function_descriptor(mylite_db *database,
                                              const struct mylite_select_plan *plan,
                                              const struct mylite_sql_ast_node *expression,
                                              const struct mylite_expression_value *value,
                                              struct mylite_field_descriptor *out_descriptor)
{
    return mylite_expression_descriptor_infer_temporal_function(
        database, plan, expression, value, out_descriptor, &temporal_descriptor_callbacks);
}

static bool infer_common_scalar_function_descriptor(mylite_db *database,
                                                    const struct mylite_sql_ast_node *name,
                                                    bool arguments_nullable, bool result_nullable,
                                                    struct mylite_field_descriptor *out_descriptor)
{
    if (mylite_expression_descriptor_infer_session_or_inet_function(database, name,
                                                                    out_descriptor)) {
        return true;
    }
    if (mylite_expression_descriptor_infer_strcmp_function(name, result_nullable, out_descriptor)) {
        return true;
    }
    if (mylite_expression_descriptor_infer_uuid_function(database, name, out_descriptor)) {
        return true;
    }
    if (mylite_expression_descriptor_infer_math_function(name, result_nullable, out_descriptor)) {
        return true;
    }
    if (mylite_expression_descriptor_infer_temporal_scalar_function(name, arguments_nullable,
                                                                    out_descriptor)) {
        return true;
    }
    return mylite_expression_descriptor_infer_base_conversion_function(database, name,
                                                                       out_descriptor);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_variadic_scalar_function_descriptor(mylite_db *database,
                                                     const struct mylite_select_plan *plan,
                                                     const struct mylite_sql_ast_node *expression,
                                                     const struct mylite_expression_value *value,
                                                     bool result_nullable,
                                                     struct mylite_field_descriptor *out_descriptor)
{
    int status = mylite_expression_descriptor_infer_numeric_variadic_function(
        database, plan, expression, value, result_nullable, out_descriptor,
        &numeric_descriptor_callbacks);

    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }
    status = infer_greatest_least_function_descriptor(database, plan, expression, result_nullable,
                                                      out_descriptor);
    if (status != MYLITE_UNSUPPORTED) {
        return status;
    }
    return mylite_expression_descriptor_infer_char_function(database, expression, out_descriptor);
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
static int infer_case_expression_descriptor(mylite_db *database,
                                            const struct mylite_select_plan *plan,
                                            const struct mylite_sql_ast_node *expression,
                                            struct mylite_field_descriptor *out_descriptor)
{
    size_t when_list_index = 0U;
    size_t else_expression_index = 1U;
    const struct mylite_sql_ast_node *when_list = NULL;
    const struct mylite_sql_ast_node *else_expression = NULL;
    struct mylite_case_descriptor_aggregate aggregate = {0};

    if (expression->case_expression_simple) {
        when_list_index = 1U;
        else_expression_index = 2U;
    }
    when_list = mylite_ast_child_at(expression, when_list_index);
    else_expression = mylite_ast_child_at(expression, else_expression_index);

    aggregate.descriptor = mylite_expression_descriptor_defaults();
    if (when_list == NULL || when_list->kind != MYLITE_SQL_AST_CASE_WHEN_LIST) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return MYLITE_OK;
    }

    for (const struct mylite_sql_ast_node *arm = when_list->first_child; arm != NULL;
         arm = arm->next_sibling) {
        int status =
            infer_case_result_descriptor(database, plan, mylite_ast_child_at(arm, 1U), &aggregate);

        if (status != MYLITE_OK) {
            return status;
        }
    }
    if (else_expression == NULL) {
        aggregate.has_result = true;
        aggregate.nullable = true;
    } else {
        int status = infer_case_result_descriptor(database, plan, else_expression, &aggregate);

        if (status != MYLITE_OK) {
            return status;
        }
    }

    *out_descriptor = finalize_case_descriptor(database, &aggregate);
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_case_result_descriptor(mylite_db *database, const struct mylite_select_plan *plan,
                                        const struct mylite_sql_ast_node *expression,
                                        struct mylite_case_descriptor_aggregate *aggregate)
{
    struct mylite_field_descriptor descriptor = mylite_expression_descriptor_defaults();
    int status = infer_expression_descriptor(database, plan, expression, NULL, &descriptor);

    if (status != MYLITE_OK) {
        return status;
    }
    aggregate_case_result_descriptor(&descriptor, aggregate);
    return MYLITE_OK;
}

static void aggregate_case_result_descriptor(const struct mylite_field_descriptor *descriptor,
                                             struct mylite_case_descriptor_aggregate *aggregate)
{
    aggregate->has_result = true;
    if (mylite_expression_descriptor_is_nullable(descriptor)) {
        aggregate->nullable = true;
    }
    if (descriptor->type == MYLITE_FIELD_TYPE_NULL) {
        aggregate->nullable = true;
        return;
    }

    if (!aggregate->has_non_null_result) {
        aggregate->descriptor = *descriptor;
        aggregate->has_non_null_result = true;
    } else {
        aggregate->descriptor.length =
            mylite_expression_descriptor_max_u64(aggregate->descriptor.length, descriptor->length);
        aggregate->descriptor.decimals = (unsigned int)mylite_expression_descriptor_max_u64(
            aggregate->descriptor.decimals, descriptor->decimals);
        aggregate->descriptor.flags |= descriptor->flags & MYLITE_FIELD_FLAG_UNSIGNED;
        aggregate->descriptor.flags &= ~(unsigned int)MYLITE_FIELD_FLAG_NOT_NULL;
    }

    if (mylite_expression_descriptor_has_text_result(descriptor)) {
        aggregate->has_text_result = true;
    }
    if (mylite_expression_descriptor_has_decimal_result(descriptor)) {
        aggregate->has_decimal_result = true;
    }
    if (mylite_expression_descriptor_has_double_result(descriptor)) {
        aggregate->has_double_result = true;
    }
}

static struct mylite_field_descriptor
finalize_case_descriptor(mylite_db *database,
                         const struct mylite_case_descriptor_aggregate *aggregate)
{
    struct mylite_field_descriptor descriptor = aggregate->descriptor;

    if (!aggregate->has_result || !aggregate->has_non_null_result) {
        return mylite_expression_descriptor_null();
    }
    if (aggregate->has_text_result) {
        descriptor = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_VAR_STRING,
            .flags = 0U,
            .length = aggregate->descriptor.length,
            .decimals = mylite_mysql_not_fixed_decimals,
            .charset_id = mylite_expression_descriptor_connection_charset_id(database),
            .nullable = aggregate->nullable,
        };
    } else if (aggregate->has_decimal_result) {
        descriptor = mylite_expression_descriptor_decimal(aggregate->nullable);
        descriptor.length = aggregate->descriptor.length;
        descriptor.decimals = aggregate->descriptor.decimals;
    } else if (aggregate->has_double_result) {
        descriptor = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_DOUBLE,
            .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
            .length = mylite_expression_descriptor_max_u64(aggregate->descriptor.length,
                                                           mylite_mysql_double_display_length),
            .decimals = mylite_mysql_not_fixed_decimals,
            .charset_id = mylite_mysql_binary_charset_id,
            .nullable = aggregate->nullable,
        };
    } else {
        descriptor.flags |= MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM;
        descriptor.charset_id = mylite_mysql_binary_charset_id;
    }
    mylite_field_descriptor_set_nullable(&descriptor, aggregate->nullable);
    return descriptor;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_cast_expression_descriptor(mylite_db *database,
                                            const struct mylite_select_plan *plan,
                                            const struct mylite_sql_ast_node *expression,
                                            const struct mylite_expression_value *value,
                                            struct mylite_field_descriptor *out_descriptor)
{
    const struct mylite_sql_ast_node *source = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *target = mylite_ast_child_at(expression, 1U);
    struct mylite_field_descriptor source_descriptor = mylite_expression_descriptor_defaults();
    int status = infer_expression_descriptor(database, plan, source, NULL, &source_descriptor);

    if (status != MYLITE_OK) {
        return status;
    }
    if (target == NULL || target->kind != MYLITE_SQL_AST_COLUMN_TYPE) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return MYLITE_OK;
    }
    status = mylite_expression_validate_cast_target_charset(database, expression);
    if (status != MYLITE_OK) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return status;
    }

    switch (target->column_type) {
    case MYLITE_SQL_AST_COLUMN_TYPE_BIGINT:
        if (target->column_type_unsigned) {
            *out_descriptor = cast_unsigned_descriptor(&source_descriptor);
        } else {
            *out_descriptor = cast_signed_descriptor(&source_descriptor);
        }
        return MYLITE_OK;
    case MYLITE_SQL_AST_COLUMN_TYPE_DECIMAL:
        *out_descriptor = cast_decimal_descriptor(target, &source_descriptor);
        return MYLITE_OK;
    case MYLITE_SQL_AST_COLUMN_TYPE_CHAR:
    case MYLITE_SQL_AST_COLUMN_TYPE_BINARY:
        *out_descriptor = cast_character_descriptor(database, target, value, &source_descriptor);
        return MYLITE_OK;
    case MYLITE_SQL_AST_COLUMN_TYPE_NONE:
    case MYLITE_SQL_AST_COLUMN_TYPE_TINYINT:
    case MYLITE_SQL_AST_COLUMN_TYPE_SMALLINT:
    case MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMINT:
    case MYLITE_SQL_AST_COLUMN_TYPE_INT:
    case MYLITE_SQL_AST_COLUMN_TYPE_BOOL:
    case MYLITE_SQL_AST_COLUMN_TYPE_BOOLEAN:
    case MYLITE_SQL_AST_COLUMN_TYPE_VARCHAR:
    case MYLITE_SQL_AST_COLUMN_TYPE_TINYTEXT:
    case MYLITE_SQL_AST_COLUMN_TYPE_TEXT:
    case MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMTEXT:
    case MYLITE_SQL_AST_COLUMN_TYPE_LONGTEXT:
    case MYLITE_SQL_AST_COLUMN_TYPE_VARBINARY:
    case MYLITE_SQL_AST_COLUMN_TYPE_TINYBLOB:
    case MYLITE_SQL_AST_COLUMN_TYPE_BLOB:
    case MYLITE_SQL_AST_COLUMN_TYPE_MEDIUMBLOB:
    case MYLITE_SQL_AST_COLUMN_TYPE_LONGBLOB:
    case MYLITE_SQL_AST_COLUMN_TYPE_FLOAT:
    case MYLITE_SQL_AST_COLUMN_TYPE_DOUBLE:
    case MYLITE_SQL_AST_COLUMN_TYPE_DATE:
    case MYLITE_SQL_AST_COLUMN_TYPE_TIME:
    case MYLITE_SQL_AST_COLUMN_TYPE_DATETIME:
    case MYLITE_SQL_AST_COLUMN_TYPE_TIMESTAMP:
    case MYLITE_SQL_AST_COLUMN_TYPE_YEAR:
        break;
    }

    *out_descriptor = mylite_expression_descriptor_defaults();
    return MYLITE_OK;
}

static int infer_scalar_subquery_expression_descriptor( // NOLINT(misc-no-recursion)
    mylite_db *database, const struct mylite_sql_ast_node *expression,
    struct mylite_field_descriptor *out_descriptor)
{
    const struct mylite_sql_ast_node *select_statement = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *select_list = mylite_ast_child_at(select_statement, 0U);
    const struct mylite_sql_ast_node *from_clause = mylite_ast_child_at(select_statement, 1U);
    const struct mylite_sql_ast_node *select_item = NULL;
    const struct mylite_sql_ast_node *select_expression = NULL;
    mylite_stmt *subquery_stmt = NULL;
    int status = mylite_select_subquery_validate_scalar_select_list(database, select_statement);

    if (status != MYLITE_OK) {
        return status;
    }

    select_item = select_list == NULL ? NULL : select_list->first_child;
    select_expression = mylite_ast_child_at(select_item, 0U);
    if (from_clause == NULL || from_clause->kind == MYLITE_SQL_AST_FROM_DUAL) {
        return infer_expression_descriptor(database, NULL, select_expression, NULL, out_descriptor);
    }

    status = prepare_select_subquery_statement(database, select_statement, &subquery_stmt);
    if (status != MYLITE_OK) {
        return status;
    }
    if (subquery_stmt == NULL) {
        return MYLITE_UNSUPPORTED;
    }
    if (subquery_stmt->result_metadata.column_count == 1U) {
        *out_descriptor = subquery_stmt->result_metadata.columns[0].descriptor;
    } else if (mylite_column_count(subquery_stmt) == 1) {
        *out_descriptor = mylite_expression_descriptor_defaults();
    } else {
        status = mylite_select_subquery_set_operand_columns_error(database);
    }
    mylite_finalize(subquery_stmt);
    if (status == MYLITE_OK) {
        mylite_expression_descriptor_set_scalar_subquery_nullable(out_descriptor);
    }
    return status;
}

static int infer_in_subquery_expression_descriptor( // NOLINT(misc-no-recursion)
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, struct mylite_field_descriptor *out_descriptor)
{
    struct mylite_field_descriptor left = mylite_expression_descriptor_defaults();
    const struct mylite_sql_ast_node *left_expression = mylite_ast_child_at(expression, 0U);
    int status = MYLITE_OK;

    if (!mylite_select_subquery_binary_expression_is_in(expression)) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return MYLITE_UNSUPPORTED;
    }
    if (left_expression == NULL || left_expression->kind == MYLITE_SQL_AST_ROW_CONSTRUCTOR) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return MYLITE_UNSUPPORTED;
    }

    status = infer_expression_descriptor(database, plan, left_expression, NULL, &left);
    if (status != MYLITE_OK) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return status;
    }
    status = mylite_select_subquery_bind_in_expression(database, expression, plan,
                                                       &select_subquery_bind_callbacks);
    if (status != MYLITE_OK) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return status;
    }

    *out_descriptor = mylite_expression_descriptor_boolean(true);
    return MYLITE_OK;
}

static int infer_row_subquery_expression_descriptor( // NOLINT(misc-no-recursion)
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, struct mylite_field_descriptor *out_descriptor)
{
    int status = MYLITE_OK;

    if (!mylite_select_subquery_binary_expression_is_row(expression)) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return MYLITE_UNSUPPORTED;
    }

    status = mylite_select_subquery_bind_row_expression(database, expression, plan,
                                                        &select_subquery_bind_callbacks);
    if (status != MYLITE_OK) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return status;
    }

    *out_descriptor = mylite_expression_descriptor_boolean(expression->operator_kind !=
                                                           MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL);
    return MYLITE_OK;
}

static int infer_quantified_subquery_expression_descriptor( // NOLINT(misc-no-recursion)
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, struct mylite_field_descriptor *out_descriptor)
{
    struct mylite_field_descriptor left = mylite_expression_descriptor_defaults();
    const struct mylite_sql_ast_node *left_expression = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *unwrapped_left =
        mylite_sql_ast_unwrap_parenthesized_expression(left_expression);
    int status = MYLITE_OK;

    if (mylite_select_subquery_quantified_comparison_is_row_alias(expression)) {
        status = mylite_select_subquery_bind_row_expression(database, expression, plan,
                                                            &select_subquery_bind_callbacks);
        if (status != MYLITE_OK) {
            *out_descriptor = mylite_expression_descriptor_defaults();
            return status;
        }

        *out_descriptor = mylite_expression_descriptor_boolean(true);
        return MYLITE_OK;
    }

    if (expression == NULL || expression->kind != MYLITE_SQL_AST_QUANTIFIED_COMPARISON ||
        !mylite_select_subquery_quantified_operator_is_supported(expression->operator_kind)) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return MYLITE_UNSUPPORTED;
    }
    if (unwrapped_left == NULL) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return MYLITE_UNSUPPORTED;
    }
    if (unwrapped_left->kind == MYLITE_SQL_AST_ROW_CONSTRUCTOR) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return mylite_select_subquery_set_row_quantified_non_alias_error(database, expression);
    }

    status = infer_expression_descriptor(database, plan, left_expression, NULL, &left);
    if (status != MYLITE_OK) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return status;
    }
    status = mylite_select_subquery_bind_quantified_expression(database, expression, plan,
                                                               &select_subquery_bind_callbacks);
    if (status != MYLITE_OK) {
        *out_descriptor = mylite_expression_descriptor_defaults();
        return status;
    }

    *out_descriptor = mylite_expression_descriptor_boolean(true);
    return MYLITE_OK;
}

static struct mylite_field_descriptor
cast_signed_descriptor(const struct mylite_field_descriptor *source)
{
    struct mylite_field_descriptor descriptor = {
        .type = MYLITE_FIELD_TYPE_LONGLONG,
        .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
        .length = mylite_mysql_signed_longlong_display_length,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = mylite_expression_descriptor_is_nullable(source),
    };

    mylite_field_descriptor_set_nullable(&descriptor, descriptor.nullable);
    return descriptor;
}

static struct mylite_field_descriptor
cast_unsigned_descriptor(const struct mylite_field_descriptor *source)
{
    struct mylite_field_descriptor descriptor = cast_signed_descriptor(source);

    descriptor.flags |= MYLITE_FIELD_FLAG_UNSIGNED;
    return descriptor;
}

static struct mylite_field_descriptor
cast_decimal_descriptor(const struct mylite_sql_ast_node *target,
                        const struct mylite_field_descriptor *source)
{
    unsigned int precision = cast_decimal_precision(target);
    unsigned int scale = cast_decimal_scale(target);
    struct mylite_field_descriptor descriptor = {
        .type = MYLITE_FIELD_TYPE_NEWDECIMAL,
        .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
        .length = (uint64_t)precision + (scale == 0U ? 1U : 2U),
        .decimals = scale,
        .charset_id = mylite_mysql_binary_charset_id,
        .nullable = mylite_expression_descriptor_is_nullable(source),
    };

    mylite_field_descriptor_set_nullable(&descriptor, descriptor.nullable);
    return descriptor;
}

static struct mylite_field_descriptor
cast_character_descriptor(mylite_db *database, const struct mylite_sql_ast_node *target,
                          const struct mylite_expression_value *value,
                          const struct mylite_field_descriptor *source)
{
    unsigned int charset_id = cast_target_charset_id(database, target);
    struct mylite_field_descriptor descriptor = {
        .type = MYLITE_FIELD_TYPE_VAR_STRING,
        .flags = 0U,
        .length = cast_character_length(database, target, value, source),
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = charset_id,
        .nullable = mylite_expression_descriptor_is_nullable(source),
    };

    if (charset_id == mylite_mysql_binary_charset_id) {
        descriptor.flags |= MYLITE_FIELD_FLAG_BINARY;
    }
    mylite_field_descriptor_set_nullable(&descriptor, descriptor.nullable);
    return descriptor;
}

static unsigned int cast_decimal_precision(const struct mylite_sql_ast_node *target)
{
    if (target != NULL && target->has_column_precision) {
        return (unsigned int)target->column_precision;
    }
    return mylite_mysql_cast_default_decimal_precision;
}

static unsigned int cast_decimal_scale(const struct mylite_sql_ast_node *target)
{
    if (target != NULL && target->has_column_scale) {
        return (unsigned int)target->column_scale;
    }
    return 0U;
}

static uint64_t cast_character_length(mylite_db *database, const struct mylite_sql_ast_node *target,
                                      const struct mylite_expression_value *value,
                                      const struct mylite_field_descriptor *source)
{
    int max_length = cast_target_charset_max_length(database, target);

    if (target != NULL && target->has_column_length) {
        return target->column_length * (uint64_t)max_length;
    }
    if (value != NULL && value->kind == MYLITE_EXPRESSION_VALUE_TEXT) {
        return (uint64_t)(value->text_value == NULL ? 0U : value->text_length) *
               (uint64_t)max_length;
    }
    if (source != NULL && source->length != 0U) {
        return source->length;
    }
    return 0U;
}

static unsigned int cast_target_charset_id(mylite_db *database,
                                           const struct mylite_sql_ast_node *target)
{
    char *name = NULL;
    const struct mylite_charset *charset = NULL;
    const struct mylite_collation *collation = NULL;

    if (target == NULL || !target->has_column_character_set) {
        if (target != NULL && target->column_type == MYLITE_SQL_AST_COLUMN_TYPE_BINARY) {
            return mylite_mysql_binary_charset_id;
        }
        return mylite_expression_descriptor_connection_charset_id(database);
    }

    name = mylite_copy_unquoted_span_text(target->column_character_set);
    if (name == NULL) {
        return mylite_expression_descriptor_connection_charset_id(database);
    }
    charset = mylite_charset_lookup(name);
    if (charset != NULL) {
        collation = mylite_collation_lookup(charset->default_collation);
    }
    free(name);
    return collation == NULL ? mylite_expression_descriptor_connection_charset_id(database)
                             : (unsigned int)collation->id;
}

static int cast_target_charset_max_length(mylite_db *database,
                                          const struct mylite_sql_ast_node *target)
{
    char *name = NULL;
    const struct mylite_charset *charset = NULL;

    if (target == NULL || !target->has_column_character_set) {
        charset =
            target != NULL && target->column_type == MYLITE_SQL_AST_COLUMN_TYPE_BINARY
                ? mylite_charset_lookup("binary")
                : mylite_charset_lookup(database == NULL ? NULL : database->character_set_results);
        return charset == NULL ? 1 : charset->max_length;
    }

    name = mylite_copy_unquoted_span_text(target->column_character_set);
    if (name == NULL) {
        return 1;
    }
    charset = mylite_charset_lookup(name);
    free(name);
    return charset == NULL ? 1 : charset->max_length;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_function_arguments_nullable(mylite_db *database,
                                             const struct mylite_select_plan *plan,
                                             const struct mylite_sql_ast_node *arguments,
                                             bool *out_nullable)
{
    bool nullable = false;

    for (const struct mylite_sql_ast_node *child = arguments == NULL ? NULL
                                                                     : arguments->first_child;
         child != NULL; child = child->next_sibling) {
        struct mylite_field_descriptor child_descriptor = mylite_expression_descriptor_defaults();
        int status = infer_expression_descriptor(database, plan, child, NULL, &child_descriptor);

        if (status != MYLITE_OK) {
            return status;
        }
        if (mylite_expression_descriptor_is_nullable(&child_descriptor)) {
            nullable = true;
        }
    }
    *out_nullable = nullable;
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_greatest_least_function_descriptor(mylite_db *database,
                                                    const struct mylite_select_plan *plan,
                                                    const struct mylite_sql_ast_node *expression,
                                                    bool result_nullable,
                                                    struct mylite_field_descriptor *out_descriptor)
{
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(expression, 0U);
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(expression, 1U);
    bool string_domain = false;
    int status = MYLITE_OK;

    if (!mylite_function_name_is_greatest_least(name)) {
        return MYLITE_UNSUPPORTED;
    }

    status = greatest_least_function_uses_string_domain(database, plan, arguments, &string_domain);
    if (status != MYLITE_OK) {
        return status;
    }
    if (string_domain) {
        return infer_greatest_least_string_descriptor(database, plan, arguments, result_nullable,
                                                      out_descriptor);
    }
    return infer_greatest_least_numeric_descriptor(database, plan, arguments, result_nullable,
                                                   out_descriptor);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int greatest_least_function_uses_string_domain(mylite_db *database,
                                                      const struct mylite_select_plan *plan,
                                                      const struct mylite_sql_ast_node *arguments,
                                                      bool *out_string_domain)
{
    *out_string_domain = false;
    for (const struct mylite_sql_ast_node *child = arguments == NULL ? NULL
                                                                     : arguments->first_child;
         child != NULL; child = child->next_sibling) {
        struct mylite_field_descriptor descriptor = mylite_expression_descriptor_defaults();
        int status = infer_expression_descriptor(database, plan, child, NULL, &descriptor);

        if (status != MYLITE_OK) {
            return status;
        }
        if (mylite_expression_descriptor_has_text_result(&descriptor) ||
            descriptor.type == MYLITE_FIELD_TYPE_BLOB) {
            *out_string_domain = true;
            return MYLITE_OK;
        }
    }
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_greatest_least_string_descriptor(mylite_db *database,
                                                  const struct mylite_select_plan *plan,
                                                  const struct mylite_sql_ast_node *arguments,
                                                  bool result_nullable,
                                                  struct mylite_field_descriptor *out_descriptor)
{
    *out_descriptor = (struct mylite_field_descriptor){
        .type = MYLITE_FIELD_TYPE_VAR_STRING,
        .flags = 0U,
        .length = greatest_least_string_result_length(database, plan, arguments),
        .decimals = mylite_mysql_not_fixed_decimals,
        .charset_id = mylite_expression_descriptor_connection_charset_id(database),
        .nullable = result_nullable,
    };
    mylite_field_descriptor_set_nullable(out_descriptor, result_nullable);
    return MYLITE_OK;
}

// NOLINTNEXTLINE(misc-no-recursion)
static uint64_t greatest_least_string_result_length(mylite_db *database,
                                                    const struct mylite_select_plan *plan,
                                                    const struct mylite_sql_ast_node *arguments)
{
    uint64_t length = 0U;

    for (const struct mylite_sql_ast_node *child = arguments == NULL ? NULL
                                                                     : arguments->first_child;
         child != NULL; child = child->next_sibling) {
        struct mylite_field_descriptor descriptor = mylite_expression_descriptor_defaults();

        if (infer_expression_descriptor(database, plan, child, NULL, &descriptor) != MYLITE_OK) {
            return mylite_mysql_long_text_length;
        }
        length = mylite_expression_descriptor_max_u64(
            length, greatest_least_argument_string_length(database, &descriptor));
    }
    return length;
}

static uint64_t
greatest_least_argument_string_length(mylite_db *database,
                                      const struct mylite_field_descriptor *descriptor)
{
    uint64_t max_bytes_per_character =
        mylite_expression_descriptor_connection_character_max_length(database);

    if (descriptor == NULL || descriptor->type == MYLITE_FIELD_TYPE_NULL) {
        return 0U;
    }
    if (mylite_expression_descriptor_has_text_result(descriptor) ||
        descriptor->type == MYLITE_FIELD_TYPE_BLOB) {
        return descriptor->length;
    }
    if (max_bytes_per_character != 0U &&
        descriptor->length > UINT64_MAX / max_bytes_per_character) {
        return mylite_mysql_long_text_length;
    }
    return descriptor->length * max_bytes_per_character;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int infer_greatest_least_numeric_descriptor(mylite_db *database,
                                                   const struct mylite_select_plan *plan,
                                                   const struct mylite_sql_ast_node *arguments,
                                                   bool result_nullable,
                                                   struct mylite_field_descriptor *out_descriptor)
{
    struct mylite_field_descriptor aggregate = mylite_expression_descriptor_defaults();
    bool saw_nonnull = false;

    for (const struct mylite_sql_ast_node *child = arguments == NULL ? NULL
                                                                     : arguments->first_child;
         child != NULL; child = child->next_sibling) {
        struct mylite_field_descriptor descriptor = mylite_expression_descriptor_defaults();
        int status = infer_expression_descriptor(database, plan, child, NULL, &descriptor);

        if (status != MYLITE_OK) {
            return status;
        }
        aggregate_greatest_least_numeric_descriptor(&descriptor, &aggregate, &saw_nonnull);
    }

    if (!saw_nonnull) {
        *out_descriptor = mylite_expression_descriptor_null();
    } else {
        *out_descriptor = aggregate;
    }
    mylite_field_descriptor_set_nullable(out_descriptor, result_nullable);
    return MYLITE_OK;
}

static void
aggregate_greatest_least_numeric_descriptor(const struct mylite_field_descriptor *argument,
                                            struct mylite_field_descriptor *aggregate,
                                            bool *out_saw_nonnull)
{
    if (argument == NULL || argument->type == MYLITE_FIELD_TYPE_NULL) {
        return;
    }

    if (!*out_saw_nonnull) {
        *aggregate = *argument;
        aggregate->flags |= MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM;
        aggregate->charset_id = mylite_mysql_binary_charset_id;
        *out_saw_nonnull = true;
        return;
    }

    if (mylite_expression_descriptor_has_double_result(argument) ||
        argument->type == MYLITE_FIELD_TYPE_FLOAT ||
        mylite_expression_descriptor_has_double_result(aggregate) ||
        aggregate->type == MYLITE_FIELD_TYPE_FLOAT) {
        *aggregate = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_DOUBLE,
            .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
            .length = mylite_expression_descriptor_max_u64(
                mylite_expression_descriptor_max_u64(aggregate->length, argument->length),
                mylite_mysql_double_display_length),
            .decimals = mylite_mysql_not_fixed_decimals,
            .charset_id = mylite_mysql_binary_charset_id,
            .nullable = true,
        };
        return;
    }

    if (mylite_expression_descriptor_has_decimal_result(argument) ||
        mylite_expression_descriptor_has_decimal_result(aggregate)) {
        *aggregate = (struct mylite_field_descriptor){
            .type = MYLITE_FIELD_TYPE_NEWDECIMAL,
            .flags = MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM,
            .length = mylite_expression_descriptor_max_u64(aggregate->length, argument->length),
            .decimals =
                aggregate->decimals > argument->decimals ? aggregate->decimals : argument->decimals,
            .charset_id = mylite_mysql_binary_charset_id,
            .nullable = true,
        };
        return;
    }

    aggregate->type = MYLITE_FIELD_TYPE_LONGLONG;
    aggregate->flags |= MYLITE_FIELD_FLAG_BINARY | MYLITE_FIELD_FLAG_NUM;
    if (((aggregate->flags & MYLITE_FIELD_FLAG_UNSIGNED) == 0U) ||
        ((argument->flags & MYLITE_FIELD_FLAG_UNSIGNED) == 0U)) {
        aggregate->flags &= ~(unsigned int)MYLITE_FIELD_FLAG_UNSIGNED;
    }
    aggregate->charset_id = mylite_mysql_binary_charset_id;
    aggregate->length = mylite_expression_descriptor_max_u64(aggregate->length, argument->length);
    aggregate->decimals = 0U;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_table_select_clauses(mylite_db *database,
                                     const struct mylite_select_clause_nodes *clauses,
                                     struct mylite_select_plan *plan)
{
    int status = MYLITE_OK;

    if (clauses->where != NULL) {
        status = bind_select_where_clause(database, clauses->where, plan);
    }
    if (status == MYLITE_OK && clauses->group_by != NULL) {
        status = bind_select_group_by_clause(database, clauses->group_by, plan);
    }
    if (status == MYLITE_OK && clauses->having != NULL) {
        status = bind_select_having_clause(database, clauses->having, plan);
    }
    if (status == MYLITE_OK && clauses->limit != NULL) {
        status = mylite_select_bind_limit_clause(clauses->limit, plan);
    }
    if (status == MYLITE_OK && clauses->order_by != NULL) {
        status = bind_select_order_by_clause(database, clauses->order_by, plan);
    }
    if (status == MYLITE_OK) {
        status = mylite_select_validate_grouping(database, plan);
    }
    return status;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_select_where_clause(mylite_db *database,
                                    const struct mylite_sql_ast_node *where_clause,
                                    const struct mylite_select_plan *plan)
{
    return mylite_select_bind_where_clause(database, where_clause, plan,
                                           &select_predicate_bind_callbacks);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_select_join_predicates(mylite_db *database, const struct mylite_select_plan *plan)
{
    return mylite_select_bind_join_predicates(database, plan, &select_predicate_bind_callbacks);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_select_projection_expression(mylite_db *database,
                                             const struct mylite_sql_ast_node *expression,
                                             struct mylite_select_plan *plan)
{
    int status = bind_select_aggregate_aware_expression(database, expression, plan, "field list");

    if (status == MYLITE_UNSUPPORTED) {
        return set_select_unsupported_projection_error(database);
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

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_select_group_by_clause(mylite_db *database,
                                       const struct mylite_sql_ast_node *group_by_clause,
                                       struct mylite_select_plan *plan)
{
    return mylite_select_bind_group_by_clause(database, group_by_clause, plan,
                                              &select_group_bind_callbacks);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_select_having_clause(mylite_db *database,
                                     const struct mylite_sql_ast_node *having_clause,
                                     struct mylite_select_plan *plan)
{
    return mylite_select_bind_having_clause(database, having_clause, plan,
                                            &select_group_bind_callbacks);
}

// NOLINTNEXTLINE(misc-no-recursion)
static int bind_select_order_by_clause(mylite_db *database,
                                       const struct mylite_sql_ast_node *order_by_clause,
                                       struct mylite_select_plan *plan)
{
    return mylite_select_bind_order_by_clause(database, order_by_clause, plan,
                                              &select_order_bind_callbacks);
}

static int set_select_invalid_group_function_error(mylite_db *database)
{
    int status = mylite_diagnostics_set_error_message(database, "Invalid use of group function");

    if (status == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_INVALID_GROUP_FUNC_USE,
                                             mylite_error_message(database));
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_select_duplicate_mode_error(mylite_db *database)
{
    int status =
        mylite_diagnostics_set_error_message(database, "Incorrect usage of ALL and DISTINCT");

    if (status == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    status = mylite_diagnostics_append_error(database, MYLITE_MYSQL_ER_WRONG_USAGE,
                                             mylite_error_message(database));
    return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
}

static int set_select_unsupported_projection_error(mylite_db *database)
{
    if (mylite_diagnostics_set_error_message(database, "Unsupported SELECT projection") ==
        MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    return MYLITE_UNSUPPORTED;
}

static int set_select_unsupported_where_error(mylite_db *database)
{
    if (mylite_diagnostics_set_error_message(database, "Unsupported WHERE predicate") ==
        MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    return MYLITE_UNSUPPORTED;
}

static int set_select_unsupported_order_error(mylite_db *database)
{
    if (mylite_diagnostics_set_error_message(database, "Unsupported ORDER BY expression") ==
        MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    return MYLITE_UNSUPPORTED;
}

static int set_select_unsupported_join_grouping_error(mylite_db *database)
{
    if (mylite_diagnostics_set_error_message(
            database, "Unsupported GROUP BY or HAVING over joined tables") == MYLITE_NOMEM) {
        return MYLITE_NOMEM;
    }
    return MYLITE_EXEC_ERROR;
}

// NOLINTNEXTLINE(misc-no-recursion)
static int prepare_custom_statement(mylite_db *database, enum mylite_stmt_kind kind,
                                    const struct mylite_sql_ast_node *statement,
                                    mylite_stmt **out_stmt)
{
    mylite_stmt *stmt = calloc(1U, sizeof(*stmt));
    int status = MYLITE_OK;

    if (stmt == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    *stmt = (mylite_stmt){
        .database = database,
        .kind = kind,
        .affected_rows = 0,
    };

    switch (kind) {
    case MYLITE_STMT_CREATE_SCHEMA:
    case MYLITE_STMT_ALTER_SCHEMA:
    case MYLITE_STMT_DROP_SCHEMA:
    case MYLITE_STMT_USE_SCHEMA:
        status = mylite_schema_copy_statement_name(statement, &stmt->schema_name);
        if (status == MYLITE_OK) {
            status = mylite_schema_copy_options(statement, &stmt->options);
        }
        break;
    case MYLITE_STMT_SET_NAMES:
    case MYLITE_STMT_SET_CHARACTER_SET:
        status = MYLITE_UNSUPPORTED;
        break;
    case MYLITE_STMT_CREATE_TABLE:
        status = mylite_table_ddl_copy_create_table_statement(statement, &stmt->create_table);
        break;
    case MYLITE_STMT_DROP_TABLE:
        status = mylite_table_ddl_copy_drop_table_statement(statement, &stmt->drop_table);
        break;
    case MYLITE_STMT_RENAME_TABLE:
        status = mylite_table_ddl_copy_rename_table_statement(statement, &stmt->rename_table);
        break;
    case MYLITE_STMT_TRUNCATE_TABLE:
        status = mylite_table_ddl_copy_truncate_table_statement(statement, &stmt->truncate_table);
        break;
    case MYLITE_STMT_ALTER_TABLE:
        status = mylite_table_ddl_copy_alter_table_statement(statement, &stmt->alter_table);
        break;
    case MYLITE_STMT_CREATE_INDEX:
        status = mylite_table_ddl_copy_create_index_statement(statement, &stmt->index_ddl);
        break;
    case MYLITE_STMT_DROP_INDEX:
        status = mylite_table_ddl_copy_drop_index_statement(statement, &stmt->index_ddl);
        break;
    case MYLITE_STMT_INSERT_VALUES:
        status = mylite_dml_copy_insert_values_statement(statement, &stmt->insert_values,
                                                         &stmt->insert_update);
        break;
    case MYLITE_STMT_INSERT_SET:
        status = mylite_dml_copy_insert_set_statement(statement, &stmt->insert_values,
                                                      &stmt->insert_set, &stmt->insert_update);
        break;
    case MYLITE_STMT_REPLACE_VALUES:
        status = mylite_dml_copy_replace_values_statement(statement, &stmt->insert_values);
        break;
    case MYLITE_STMT_REPLACE_SET:
        status = mylite_dml_copy_replace_set_statement(statement, &stmt->insert_values,
                                                       &stmt->insert_set);
        break;
    case MYLITE_STMT_UPDATE:
    case MYLITE_STMT_DELETE:
        status = MYLITE_UNSUPPORTED;
        break;
    case MYLITE_STMT_START_TRANSACTION:
    case MYLITE_STMT_BEGIN_TRANSACTION:
    case MYLITE_STMT_COMMIT:
    case MYLITE_STMT_ROLLBACK:
        status = mylite_transaction_copy_statement(statement, stmt);
        break;
    case MYLITE_STMT_SAVEPOINT:
    case MYLITE_STMT_ROLLBACK_TO_SAVEPOINT:
    case MYLITE_STMT_RELEASE_SAVEPOINT:
        status = mylite_transaction_copy_savepoint_statement(statement, stmt);
        break;
    case MYLITE_STMT_SCALAR_SELECT:
        status =
            mylite_select_scalar_copy_statement(statement, stmt, &select_scalar_eval_callbacks);
        break;
    case MYLITE_STMT_TABLE_SELECT:
    case MYLITE_STMT_UNION_QUERY:
    case MYLITE_STMT_SQLITE:
        status = MYLITE_UNSUPPORTED;
        break;
    }
    if (status != MYLITE_OK) {
        if (status == MYLITE_NOMEM) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
        }
        mylite_finalize(stmt);
        return status;
    }

    if (kind == MYLITE_STMT_CREATE_SCHEMA &&
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_IF_NOT_EXISTS) != NULL) {
        stmt->if_not_exists = true;
    }
    if (kind == MYLITE_STMT_CREATE_TABLE &&
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_IF_NOT_EXISTS) != NULL) {
        stmt->if_not_exists = true;
    }
    if (kind == MYLITE_STMT_DROP_SCHEMA &&
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_IF_EXISTS) != NULL) {
        stmt->if_exists = true;
    }
    if (kind == MYLITE_STMT_DROP_TABLE &&
        mylite_ast_find_child_kind(statement, MYLITE_SQL_AST_IF_EXISTS) != NULL) {
        stmt->if_exists = true;
    }

    *out_stmt = stmt;
    return MYLITE_OK;
}

int mylite_statement_execute_custom(mylite_stmt *stmt)
{
    const struct mylite_dml_expression_callbacks dml_expression_callbacks = {
        .user_data = stmt,
        .eval_session_function = evaluate_dml_materialize_session_function,
        .set_where_predicate_eval_error = set_dml_materialize_where_predicate_eval_error,
    };
    int status = MYLITE_OK;

    if (stmt->kind == MYLITE_STMT_SCALAR_SELECT) {
        return execute_scalar_select_statement(stmt);
    }
    if (stmt->kind == MYLITE_STMT_TABLE_SELECT) {
        return execute_table_select_statement(stmt);
    }
    if (stmt->kind == MYLITE_STMT_UNION_QUERY) {
        return mylite_select_union_execute_query(stmt, &union_query_callbacks);
    }
    if (stmt->executed) {
        return MYLITE_DONE;
    }
    if (stmt->kind == MYLITE_STMT_REPLACE_VALUES || stmt->kind == MYLITE_STMT_REPLACE_SET) {
        status = mylite_dml_append_replace_delayed_warning(stmt);
        if (status != MYLITE_OK) {
            stmt->affected_rows = -1;
            return status;
        }
    }
    if (mylite_statement_kind_writes(stmt->kind) && stmt->database->transaction_active &&
        stmt->database->transaction_access_mode == MYLITE_TRANSACTION_ACCESS_READ_ONLY) {
        stmt->affected_rows = -1;
        return mylite_transaction_set_read_only_error(stmt->database);
    }
    stmt->executed = true;

    switch (stmt->kind) {
    case MYLITE_STMT_CREATE_SCHEMA:
        status = mylite_schema_execute_create_statement(stmt);
        break;
    case MYLITE_STMT_ALTER_SCHEMA:
        status = mylite_schema_execute_alter_statement(stmt);
        break;
    case MYLITE_STMT_DROP_SCHEMA:
        status = mylite_schema_execute_drop_statement(stmt);
        break;
    case MYLITE_STMT_USE_SCHEMA:
        status = mylite_schema_execute_use_statement(stmt);
        break;
    case MYLITE_STMT_SET_NAMES:
        status = mylite_connection_execute_set_names_statement(stmt);
        break;
    case MYLITE_STMT_SET_CHARACTER_SET:
        status = mylite_connection_execute_set_character_set_statement(stmt);
        break;
    case MYLITE_STMT_CREATE_TABLE:
        status = mylite_table_ddl_execute_create_table_statement(
            stmt->database, stmt->database->selected_schema, &stmt->create_table,
            stmt->if_not_exists);
        break;
    case MYLITE_STMT_DROP_TABLE:
        status = mylite_table_ddl_execute_drop_table_statement(
            stmt->database, stmt->database->selected_schema, &stmt->drop_table, stmt->if_exists);
        break;
    case MYLITE_STMT_RENAME_TABLE:
        status = mylite_table_ddl_execute_rename_table_prepared_statement(stmt);
        break;
    case MYLITE_STMT_TRUNCATE_TABLE:
        status = mylite_table_ddl_execute_truncate_table_prepared_statement(stmt);
        break;
    case MYLITE_STMT_ALTER_TABLE:
        status = mylite_table_ddl_execute_alter_table_prepared_statement(stmt);
        break;
    case MYLITE_STMT_CREATE_INDEX:
        status = mylite_table_ddl_execute_create_index_prepared_statement(stmt);
        break;
    case MYLITE_STMT_DROP_INDEX:
        status = mylite_table_ddl_execute_drop_index_prepared_statement(stmt);
        break;
    case MYLITE_STMT_INSERT_VALUES:
        status = mylite_dml_execute_insert_values_statement(stmt);
        break;
    case MYLITE_STMT_INSERT_SET:
        status = mylite_dml_execute_insert_set_statement(stmt);
        break;
    case MYLITE_STMT_REPLACE_VALUES:
        status = mylite_dml_execute_replace_values_statement(stmt);
        break;
    case MYLITE_STMT_REPLACE_SET:
        status = mylite_dml_execute_replace_set_statement(stmt);
        break;
    case MYLITE_STMT_UPDATE:
        status = mylite_dml_execute_update_statement(stmt, &dml_expression_callbacks);
        break;
    case MYLITE_STMT_DELETE:
        status = mylite_dml_execute_delete_statement(stmt, &dml_expression_callbacks);
        break;
    case MYLITE_STMT_START_TRANSACTION:
    case MYLITE_STMT_BEGIN_TRANSACTION:
    case MYLITE_STMT_COMMIT:
    case MYLITE_STMT_ROLLBACK:
    case MYLITE_STMT_SAVEPOINT:
    case MYLITE_STMT_ROLLBACK_TO_SAVEPOINT:
    case MYLITE_STMT_RELEASE_SAVEPOINT:
        status = mylite_transaction_execute_statement(stmt);
        break;
    case MYLITE_STMT_SCALAR_SELECT:
        return execute_scalar_select_statement(stmt);
    case MYLITE_STMT_TABLE_SELECT:
        return execute_table_select_statement(stmt);
    case MYLITE_STMT_UNION_QUERY:
        return mylite_select_union_execute_query(stmt, &union_query_callbacks);
    case MYLITE_STMT_SQLITE:
        status = MYLITE_MISUSE;
        break;
    }

    return status == MYLITE_OK ? MYLITE_DONE : status;
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
    return set_where_predicate_eval_error((mylite_stmt *)user_data);
}

static int evaluate_statement_session_function(
    mylite_stmt *stmt, const struct mylite_sql_ast_node *function_call,
    const struct mylite_expression_eval_context *expression_context,
    struct mylite_expression_warnings *warnings, const struct mylite_select_table *table,
    struct mylite_expression_value *out_value)
{
    const struct mylite_sql_ast_node *name = NULL;
    int status = mylite_session_evaluate_core_function(stmt, function_call, expression_context,
                                                       warnings, out_value);

    if (status != -1) {
        return status;
    }
    name = mylite_ast_child_at(function_call, 0U);
    if (name == NULL || name->kind != MYLITE_SQL_AST_IDENTIFIER) {
        return -1;
    }
    if (mylite_function_name_is_strcmp(name)) {
        return evaluate_strcmp_function(stmt, function_call, expression_context, warnings, table,
                                        out_value);
    }
    if (mylite_function_name_is_charset_collation_introspection(name)) {
        return evaluate_charset_collation_function(stmt, function_call, expression_context,
                                                   warnings, table, out_value);
    }
    return -1;
}

static int evaluate_strcmp_function(mylite_stmt *stmt,
                                    const struct mylite_sql_ast_node *function_call,
                                    const struct mylite_expression_eval_context *expression_context,
                                    struct mylite_expression_warnings *warnings,
                                    const struct mylite_select_table *table,
                                    struct mylite_expression_value *out_value)
{
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(function_call, 1U);
    const struct mylite_sql_ast_node *left_argument = mylite_ast_child_at(arguments, 0U);
    const struct mylite_sql_ast_node *right_argument = mylite_ast_child_at(arguments, 1U);
    struct mylite_charset_collation_info collation_info =
        mylite_expression_binary_collation_info(mylite_mysql_coercibility_ignorable);
    struct mylite_expression_value left = {0};
    struct mylite_expression_value right = {0};
    int status = MYLITE_OK;

    if (stmt == NULL || stmt->database == NULL ||
        mylite_sql_ast_node_child_count(arguments) != 2U) {
        return -1;
    }

    status =
        mylite_expression_eval_with_context(left_argument, expression_context, warnings, &left);
    if (status != MYLITE_OK) {
        goto cleanup;
    }
    if (left.kind == MYLITE_EXPRESSION_VALUE_NULL) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }

    status =
        mylite_expression_eval_with_context(right_argument, expression_context, warnings, &right);
    if (status != MYLITE_OK) {
        goto cleanup;
    }
    if (right.kind == MYLITE_EXPRESSION_VALUE_NULL) {
        *out_value = (struct mylite_expression_value){.kind = MYLITE_EXPRESSION_VALUE_NULL};
        goto cleanup;
    }

    status = infer_strcmp_collation_info(stmt, function_call, table, &collation_info);
    if (status == MYLITE_OK) {
        status = set_strcmp_function_result(stmt->database, &left, left_argument, &right,
                                            right_argument, &collation_info, out_value);
    }

cleanup:
    mylite_expression_value_deinit(&right);
    mylite_expression_value_deinit(&left);
    return status;
}

static int infer_strcmp_collation_info(mylite_stmt *stmt,
                                       const struct mylite_sql_ast_node *function_call,
                                       const struct mylite_select_table *table,
                                       struct mylite_charset_collation_info *out_info)
{
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(function_call, 1U);
    struct mylite_expression_collation_context context = {
        .plan = stmt == NULL ? NULL : &stmt->select_plan,
        .table = table,
    };
    int status = MYLITE_OK;

    if (stmt == NULL || stmt->database == NULL) {
        return -1;
    }

    status = mylite_expression_infer_function_arguments_collation_info(
        stmt->database, &context, arguments, 0U, true, &expression_collation_callbacks, out_info);
    if (status != MYLITE_OK) {
        return status;
    }
    if (out_info->coercibility == mylite_mysql_coercibility_ignorable ||
        out_info->collation == NULL) {
        *out_info = mylite_expression_connection_collation_info(
            stmt->database, mylite_mysql_coercibility_coercible);
    }
    return MYLITE_OK;
}

static int set_strcmp_function_result(mylite_db *database,
                                      const struct mylite_expression_value *left,
                                      const struct mylite_sql_ast_node *left_argument,
                                      const struct mylite_expression_value *right,
                                      const struct mylite_sql_ast_node *right_argument,
                                      const struct mylite_charset_collation_info *collation_info,
                                      struct mylite_expression_value *out_value)
{
    struct mylite_strcmp_compare_options options =
        strcmp_compare_options_for_collation(collation_info);
    char *left_text = NULL;
    char *right_text = NULL;
    size_t left_length = 0U;
    size_t right_length = 0U;
    int status = strcmp_value_to_text(database, left, left_argument, &left_text, &left_length);

    if (status == MYLITE_OK) {
        status = strcmp_value_to_text(database, right, right_argument, &right_text, &right_length);
    }
    if (status == MYLITE_OK) {
        *out_value = (struct mylite_expression_value){
            .kind = MYLITE_EXPRESSION_VALUE_INT64,
            .int64_value =
                compare_strcmp_texts(left_text, left_length, right_text, right_length, options),
        };
    }

    free(right_text);
    free(left_text);
    return status;
}

static int strcmp_value_to_text(mylite_db *database, const struct mylite_expression_value *value,
                                const struct mylite_sql_ast_node *argument, char **out_text,
                                size_t *out_length)
{
    enum { strcmp_text_buffer_size = 64 };
    const struct mylite_sql_ast_node *decimal_literal = NULL;
    char buffer[strcmp_text_buffer_size];
    bool negative_decimal = false;
    int length = 0;

    if (value == NULL || value->kind == MYLITE_EXPRESSION_VALUE_NULL) {
        return -1;
    }
    decimal_literal = strcmp_decimal_literal_argument(argument, &negative_decimal);
    if (decimal_literal != NULL) {
        return strcmp_decimal_literal_to_text(database, decimal_literal, negative_decimal, out_text,
                                              out_length);
    }

    switch (value->kind) {
    case MYLITE_EXPRESSION_VALUE_INT64:
        length = snprintf(buffer, sizeof(buffer), "%lld", (long long)value->int64_value);
        break;
    case MYLITE_EXPRESSION_VALUE_UINT64:
        length = snprintf(buffer, sizeof(buffer), "%llu", (unsigned long long)value->uint64_value);
        break;
    case MYLITE_EXPRESSION_VALUE_REAL:
        length = snprintf(buffer, sizeof(buffer), "%.15g", value->real_value);
        break;
    case MYLITE_EXPRESSION_VALUE_TEXT:
        *out_length = value->text_value == NULL ? 0U : value->text_length;
        *out_text =
            mylite_copy_span_text(value->text_value == NULL ? "" : value->text_value, *out_length);
        if (*out_text != NULL) {
            return MYLITE_OK;
        }
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    case MYLITE_EXPRESSION_VALUE_NULL:
        return -1;
    }

    if (length <= 0 || (size_t)length >= sizeof(buffer)) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    *out_length = (size_t)length;
    *out_text = mylite_copy_span_text(buffer, *out_length);
    if (*out_text == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    return MYLITE_OK;
}

static const struct mylite_sql_ast_node *
strcmp_decimal_literal_argument(const struct mylite_sql_ast_node *argument, bool *out_negative)
{
    bool negative = false;

    argument = mylite_sql_ast_unwrap_parenthesized_expression(argument);
    if (argument != NULL && argument->kind == MYLITE_SQL_AST_UNARY_EXPRESSION &&
        (argument->operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE ||
         argument->operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE)) {
        negative = argument->operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE;
        argument =
            mylite_sql_ast_unwrap_parenthesized_expression(mylite_ast_child_at(argument, 0U));
    }
    if (out_negative != NULL) {
        *out_negative = negative;
    }
    if (argument == NULL || argument->kind != MYLITE_SQL_AST_LITERAL ||
        argument->literal_kind != MYLITE_SQL_AST_LITERAL_DECIMAL) {
        return NULL;
    }
    return argument;
}

static int strcmp_decimal_literal_to_text(mylite_db *database,
                                          const struct mylite_sql_ast_node *literal, bool negative,
                                          char **out_text, size_t *out_length)
{
    const char *text = literal == NULL ? NULL : literal->span.text;
    size_t length = literal == NULL ? 0U : literal->span.length;
    const char *dot = NULL;
    size_t integer_length = 0U;
    size_t fractional_length = 0U;
    size_t integer_offset = 0U;
    size_t sign_length = 0U;
    size_t normalized_integer_length = 0U;
    size_t fractional_output_length = 0U;
    size_t result_length = 0U;
    size_t output = 0U;
    char *result = NULL;
    bool zero = false;

    if (text == NULL) {
        return -1;
    }
    dot = memchr(text, '.', length);
    if (dot == NULL) {
        return -1;
    }
    integer_length = (size_t)(dot - text);
    fractional_length = length - integer_length - 1U;
    zero = decimal_literal_span_is_zero(text, length);

    while (integer_offset < integer_length && text[integer_offset] == '0') {
        ++integer_offset;
    }
    normalized_integer_length = integer_length - integer_offset;
    if (normalized_integer_length == 0U) {
        normalized_integer_length = 1U;
    }
    sign_length = negative && !zero ? 1U : 0U;
    if (fractional_length > 0U) {
        if (fractional_length == SIZE_MAX) {
            (void)mylite_diagnostics_set_error_message(database, "out of memory");
            return MYLITE_NOMEM;
        }
        fractional_output_length = 1U + fractional_length;
    }
    if (normalized_integer_length > SIZE_MAX - sign_length) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    result_length = sign_length + normalized_integer_length;
    if (result_length == SIZE_MAX || fractional_output_length >= SIZE_MAX - result_length) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }
    result_length += fractional_output_length;
    result = malloc(result_length + 1U);
    if (result == NULL) {
        (void)mylite_diagnostics_set_error_message(database, "out of memory");
        return MYLITE_NOMEM;
    }

    if (negative && !zero) {
        result[output++] = '-';
    }
    if (integer_length == integer_offset) {
        result[output++] = '0';
    } else {
        memcpy(result + output, text + integer_offset, normalized_integer_length);
        output += normalized_integer_length;
    }
    if (fractional_length > 0U) {
        result[output++] = '.';
        memcpy(result + output, dot + 1, fractional_length);
        output += fractional_length;
    }
    result[output] = '\0';
    *out_text = result;
    *out_length = output;
    return MYLITE_OK;
}

static bool decimal_literal_span_is_zero(const char *text, size_t length)
{
    for (size_t index = 0U; index < length; ++index) {
        if (text[index] >= '1' && text[index] <= '9') {
            return false;
        }
    }
    return true;
}

static int compare_strcmp_texts(const char *left, size_t left_length, const char *right,
                                size_t right_length, struct mylite_strcmp_compare_options options)
{
    size_t compare_length = 0U;

    if (left == NULL) {
        left = "";
        left_length = 0U;
    }
    if (right == NULL) {
        right = "";
        right_length = 0U;
    }
    if (options.ignore_trailing_spaces) {
        trim_strcmp_trailing_spaces(left, &left_length);
        trim_strcmp_trailing_spaces(right, &right_length);
    }

    compare_length = left_length < right_length ? left_length : right_length;
    for (size_t index = 0U; index < compare_length; ++index) {
        unsigned char left_byte = strcmp_compare_byte((unsigned char)left[index], options);
        unsigned char right_byte = strcmp_compare_byte((unsigned char)right[index], options);

        if (left_byte != right_byte) {
            return left_byte > right_byte ? 1 : -1;
        }
    }
    return (left_length > right_length) - (left_length < right_length);
}

static void trim_strcmp_trailing_spaces(const char *text, size_t *length)
{
    if (text == NULL || length == NULL) {
        return;
    }
    while (*length > 0U && text[*length - 1U] == ' ') {
        *length -= 1U;
    }
}

static unsigned char strcmp_compare_byte(unsigned char value,
                                         struct mylite_strcmp_compare_options options)
{
    if (!options.case_sensitive && value >= 'A' && value <= 'Z') {
        return (unsigned char)(value - 'A' + 'a');
    }
    return value;
}

static struct mylite_strcmp_compare_options
strcmp_compare_options_for_collation(const struct mylite_charset_collation_info *info)
{
    const char *collation_name = info == NULL || info->collation == NULL
                                     ? mylite_charset_default_collation_name()
                                     : info->collation;

    return (struct mylite_strcmp_compare_options){
        .ignore_trailing_spaces = strcmp_collation_ignores_trailing_spaces(collation_name),
        .case_sensitive = strcmp_collation_is_case_sensitive(info),
    };
}

static bool strcmp_collation_ignores_trailing_spaces(const char *collation_name)
{
    const struct mylite_collation *collation = mylite_collation_lookup(collation_name);

    if (collation == NULL) {
        return false;
    }
    return mylite_ascii_case_equal(collation->pad_attribute, "PAD SPACE");
}

static bool strcmp_collation_is_case_sensitive(const struct mylite_charset_collation_info *info)
{
    const char *collation_name = info == NULL || info->collation == NULL
                                     ? mylite_charset_default_collation_name()
                                     : info->collation;
    size_t collation_length = strlen(collation_name);

    if (info != NULL &&
        mylite_ascii_case_equal(info->character_set, mylite_mysql_binary_charset_name)) {
        return true;
    }
    if (mylite_ascii_case_equal(collation_name, mylite_mysql_binary_charset_name)) {
        return true;
    }
    if (collation_length < 4U) {
        return false;
    }
    return mylite_ascii_case_equal(collation_name + collation_length - 4U, "_bin");
}

static int evaluate_charset_collation_function(
    mylite_stmt *stmt, const struct mylite_sql_ast_node *function_call,
    const struct mylite_expression_eval_context *expression_context,
    struct mylite_expression_warnings *warnings, const struct mylite_select_table *table,
    struct mylite_expression_value *out_value)
{
    const struct mylite_sql_ast_node *name = mylite_ast_child_at(function_call, 0U);
    const struct mylite_sql_ast_node *arguments = mylite_ast_child_at(function_call, 1U);
    const struct mylite_sql_ast_node *argument = mylite_ast_child_at(arguments, 0U);
    struct mylite_expression_collation_context collation_context = {
        .plan = stmt == NULL ? NULL : &stmt->select_plan,
        .table = table,
    };
    struct mylite_charset_collation_info info =
        mylite_expression_binary_collation_info(mylite_mysql_coercibility_ignorable);
    struct mylite_expression_value value = {0};
    int status = MYLITE_OK;

    if (stmt == NULL || stmt->database == NULL || argument == NULL) {
        return -1;
    }

    status = mylite_expression_infer_collation_info(stmt->database, &collation_context, argument,
                                                    &expression_collation_callbacks, &info);
    if (status != MYLITE_OK) {
        return status;
    }

    status = mylite_expression_eval_with_context(argument, expression_context, warnings, &value);
    mylite_expression_value_deinit(&value);
    if (status != 0) {
        return status;
    }
    return set_charset_collation_function_result(stmt->database, name, &info, out_value);
}

static int set_charset_collation_function_result(mylite_db *database,
                                                 const struct mylite_sql_ast_node *name,
                                                 const struct mylite_charset_collation_info *info,
                                                 struct mylite_expression_value *out_value)
{
    if (mylite_function_name_is_charset(name)) {
        return mylite_session_set_text_function_value(database, info->character_set, out_value);
    }
    if (mylite_function_name_is_collation(name)) {
        return mylite_session_set_text_function_value(database, info->collation, out_value);
    }
    if (mylite_function_name_is_coercibility(name)) {
        *out_value = (struct mylite_expression_value){
            .kind = MYLITE_EXPRESSION_VALUE_INT64,
            .int64_value = info->coercibility,
        };
        return MYLITE_OK;
    }
    return MYLITE_UNSUPPORTED;
}

static int execute_table_select_statement(mylite_stmt *stmt)
{
    if (stmt->sqlite_stmt == NULL && mylite_select_plan_table_count(&stmt->select_plan) <= 1U) {
        return MYLITE_MISUSE;
    }
    stmt->executed = true;
    stmt->affected_rows = -1;

    int status = mylite_select_materialize_table_result(stmt, &table_select_eval_callbacks);

    if (status != MYLITE_OK) {
        return status;
    }
    if (stmt->select_result.next_row >= stmt->select_result.row_count) {
        mylite_select_result_current_values_deinit(&stmt->select_result);
        stmt->select_result.has_current_row = false;
        return MYLITE_DONE;
    }

    status = mylite_select_eval_set_current_row(
        stmt, &stmt->select_result.rows[stmt->select_result.next_row],
        &table_select_eval_callbacks);
    if (status != MYLITE_OK) {
        return status;
    }
    ++stmt->select_result.next_row;
    return MYLITE_ROW;
}

static int set_where_predicate_eval_error(mylite_stmt *stmt)
{
    mylite_db *database = stmt == NULL ? NULL : stmt->database;

    if (database == NULL) {
        return MYLITE_EXEC_ERROR;
    }
    if (database->warnings.count != 0U) {
        const struct mylite_expression_warning *warning =
            &database->warnings.items[database->warnings.count - 1U];

        if (warning->level == MYLITE_EXPRESSION_WARNING_LEVEL_ERROR) {
            int status = mylite_diagnostics_set_error_message(database, warning->message);

            return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
        }
        if (warning->code == MYLITE_MYSQL_ER_WRONG_ARGUMENTS) {
            int status = mylite_diagnostics_set_error_message(database, warning->message);

            return status == MYLITE_NOMEM ? MYLITE_NOMEM : MYLITE_EXEC_ERROR;
        }
    }
    return set_select_unsupported_where_error(database);
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
