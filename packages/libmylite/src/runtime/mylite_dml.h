#ifndef MYLITE_RUNTIME_MYLITE_DML_H
#define MYLITE_RUNTIME_MYLITE_DML_H

#include "mylite_dml_insert_bound_value.h"
#include "mylite_dml_types.h"

struct mylite_select_plan;

int mylite_dml_prepare_update_statement(mylite_db *database,
                                        const struct mylite_sql_ast_node *statement,
                                        const char *sql, size_t sql_length, mylite_stmt **out_stmt);
int mylite_dml_prepare_delete_statement(mylite_db *database,
                                        const struct mylite_sql_ast_node *statement,
                                        const char *sql, size_t sql_length, mylite_stmt **out_stmt);
int mylite_dml_prepare_insert_values_statement(mylite_db *database,
                                               const struct mylite_sql_ast_node *statement,
                                               const char *sql, size_t sql_length,
                                               mylite_stmt **out_stmt);
int mylite_dml_prepare_insert_set_statement(mylite_db *database,
                                            const struct mylite_sql_ast_node *statement,
                                            const char *sql, size_t sql_length,
                                            mylite_stmt **out_stmt);
int mylite_dml_prepare_replace_values_statement(mylite_db *database,
                                                const struct mylite_sql_ast_node *statement,
                                                const char *sql, size_t sql_length,
                                                mylite_stmt **out_stmt);
int mylite_dml_prepare_replace_set_statement(mylite_db *database,
                                             const struct mylite_sql_ast_node *statement,
                                             const char *sql, size_t sql_length,
                                             mylite_stmt **out_stmt);
int mylite_dml_validate_insert_target(mylite_db *database, const char *selected_schema,
                                      const struct mylite_insert_values_plan *plan,
                                      const char **out_schema_name);
int mylite_dml_validate_insert_column_list(mylite_db *database,
                                           const struct mylite_insert_values_plan *plan,
                                           const struct mylite_insert_table *table,
                                           size_t **out_column_indexes);
int mylite_dml_validate_insert_row_alias(mylite_db *database,
                                         const struct mylite_insert_values_plan *plan,
                                         size_t source_column_count);
int mylite_dml_validate_insert_set_assignments(mylite_db *database,
                                               const struct mylite_insert_values_plan *values_plan,
                                               const struct mylite_insert_set_plan *set_plan,
                                               const struct mylite_insert_table *table,
                                               const char *schema_name, size_t **out_column_indexes,
                                               size_t *out_column_index_count);
int mylite_dml_load_write_table(mylite_db *database, const char *schema_name,
                                const char *table_name, struct mylite_insert_table *out_table);
int mylite_dml_initialize_insert_ignore_warning_state(mylite_db *database,
                                                      const struct mylite_insert_values_plan *plan,
                                                      const struct mylite_insert_table *table,
                                                      struct mylite_insert_execution_state *state);
void mylite_dml_insert_execution_state_deinit(struct mylite_insert_execution_state *state);
int mylite_dml_promote_expression_warnings(mylite_db *database, size_t warning_start);
int mylite_dml_set_expression_condition_error(mylite_db *database, size_t warning_start);
int mylite_dml_resolve_update_expression_identifier(void *user_data,
                                                    const struct mylite_sql_ast_node *identifier,
                                                    struct mylite_expression_value *out_value);
int mylite_dml_evaluate_session_function(
    void *user_data, const struct mylite_sql_ast_node *function_call,
    const struct mylite_expression_eval_context *expression_context,
    struct mylite_expression_warnings *warnings, struct mylite_expression_value *out_value);
int mylite_dml_evaluate_default_function(void *user_data,
                                         const struct mylite_sql_ast_node *function_call,
                                         struct mylite_expression_value *out_value);
char *mylite_dml_build_insert_physical_sql(mylite_db *database,
                                           const struct mylite_insert_table *table);
char *mylite_dml_build_replace_delete_sql(mylite_db *database,
                                          const struct mylite_insert_table *table);
int mylite_dml_execute_insert_values_transaction(
    mylite_db *database, const char *selected_schema, const char *schema_name,
    const struct mylite_insert_values_plan *values_plan,
    const struct mylite_insert_duplicate_update_plan *update_plan,
    const struct mylite_insert_table *table, const size_t *column_indexes,
    const size_t *update_column_indexes, const struct mylite_dml_expression_callbacks *callbacks,
    struct mylite_insert_transaction_result *out_result);
int mylite_dml_execute_insert_set_transaction(
    mylite_db *database, const char *selected_schema, const char *schema_name,
    const struct mylite_insert_values_plan *values_plan,
    const struct mylite_insert_set_plan *set_plan,
    const struct mylite_insert_duplicate_update_plan *update_plan,
    const struct mylite_insert_table *table, const size_t *column_indexes,
    size_t column_index_count, const size_t *update_column_indexes,
    const struct mylite_dml_expression_callbacks *callbacks,
    struct mylite_insert_transaction_result *out_result);
int mylite_dml_execute_replace_values_transaction(
    mylite_db *database, const char *schema_name,
    const struct mylite_insert_values_plan *values_plan, const struct mylite_insert_table *table,
    const size_t *column_indexes, const struct mylite_dml_expression_callbacks *callbacks,
    struct mylite_insert_transaction_result *out_result);
int mylite_dml_execute_replace_set_transaction(
    mylite_db *database, const char *schema_name,
    const struct mylite_insert_values_plan *values_plan,
    const struct mylite_insert_set_plan *set_plan, const struct mylite_insert_table *table,
    const size_t *column_indexes, size_t column_index_count,
    const struct mylite_dml_expression_callbacks *callbacks,
    struct mylite_insert_transaction_result *out_result);
int mylite_dml_write_insert_candidate_row(mylite_db *database, sqlite3_stmt *insert,
                                          const struct mylite_insert_table *table,
                                          const struct mylite_insert_bound_value *values,
                                          struct mylite_insert_execution_state *state);
int mylite_dml_execute_insert_row(mylite_db *database, const struct mylite_insert_values_plan *plan,
                                  const char *schema_name, sqlite3_stmt *insert,
                                  const struct mylite_insert_table *table,
                                  const struct mylite_insert_row_column_indexes *column_indexes,
                                  struct mylite_insert_execution_state *state, size_t row_index,
                                  const struct mylite_dml_expression_callbacks *callbacks);
int mylite_dml_execute_insert_set_row(mylite_db *database, const char *schema_name,
                                      const struct mylite_insert_values_plan *values_plan,
                                      const struct mylite_insert_set_plan *set_plan,
                                      sqlite3_stmt *insert, const struct mylite_insert_table *table,
                                      const size_t *column_indexes, size_t column_index_count,
                                      struct mylite_insert_execution_state *state,
                                      struct mylite_insert_bound_value *values,
                                      struct mylite_insert_set_row_state *row_state,
                                      const struct mylite_dml_expression_callbacks *callbacks);
int mylite_dml_write_insert_update_candidate(mylite_db *database,
                                             const struct mylite_insert_table *table,
                                             sqlite3_int64 rowid,
                                             const struct mylite_insert_bound_value *values,
                                             struct mylite_insert_execution_state *state);
bool mylite_dml_insert_update_row_changed(const struct mylite_insert_bound_value *stored,
                                          const struct mylite_insert_bound_value *candidate,
                                          size_t value_count);
int mylite_dml_apply_insert_update_assignments(
    mylite_db *database, const char *selected_schema,
    const struct mylite_insert_values_plan *values_plan,
    const struct mylite_insert_duplicate_update_plan *update_plan,
    const struct mylite_insert_table *table,
    const struct mylite_insert_row_column_indexes *column_indexes,
    const struct mylite_insert_bound_value *candidate_values,
    struct mylite_insert_bound_value *updated_values);
int mylite_dml_execute_insert_update_values_row(
    mylite_db *database, const char *selected_schema,
    const struct mylite_insert_values_plan *values_plan,
    const struct mylite_insert_duplicate_update_plan *update_plan, sqlite3_stmt *insert,
    const struct mylite_insert_table *table,
    const struct mylite_insert_row_column_indexes *column_indexes,
    struct mylite_insert_execution_state *state, size_t row_index,
    const struct mylite_dml_expression_callbacks *callbacks);
int mylite_dml_execute_insert_update_set_row(
    mylite_db *database, const char *selected_schema, const char *schema_name,
    const struct mylite_insert_values_plan *values_plan,
    const struct mylite_insert_set_plan *set_plan,
    const struct mylite_insert_duplicate_update_plan *update_plan, sqlite3_stmt *insert,
    const struct mylite_insert_table *table, const size_t *column_indexes,
    size_t column_index_count, const struct mylite_insert_row_column_indexes *row_column_indexes,
    struct mylite_insert_execution_state *state, struct mylite_insert_bound_value *values,
    struct mylite_insert_set_row_state *row_state,
    const struct mylite_dml_expression_callbacks *callbacks);
int mylite_dml_validate_insert_update_assignments(
    mylite_db *database, const struct mylite_insert_values_plan *values_plan,
    const struct mylite_insert_duplicate_update_plan *update_plan,
    const struct mylite_insert_table *table, const char *schema_name,
    const size_t *source_column_indexes, size_t source_column_count, size_t **out_column_indexes);
int mylite_dml_append_insert_update_deprecated_warnings(
    mylite_db *database, const struct mylite_insert_duplicate_update_plan *plan);
int mylite_dml_advance_insert_row_auto_increment(const struct mylite_insert_table *table,
                                                 const struct mylite_insert_bound_value *values,
                                                 struct mylite_insert_execution_state *state);
int mylite_dml_validate_update_unique_indexes(mylite_db *database,
                                              const struct mylite_select_table *table,
                                              const struct mylite_insert_table *write_table,
                                              const struct mylite_update_row *candidate);
int mylite_dml_resolve_insert_row_values(
    mylite_db *database, const struct mylite_insert_values_plan *plan, const char *schema_name,
    const struct mylite_insert_table *table, const size_t *column_indexes,
    uint64_t statement_row_count, struct mylite_insert_execution_state *state, size_t row_index,
    struct mylite_insert_bound_value *out_values,
    const struct mylite_dml_expression_callbacks *callbacks);
int mylite_dml_resolve_insert_expression_bound_value(
    mylite_db *database, const char *schema_name, const struct mylite_insert_values_plan *plan,
    const struct mylite_insert_table *table, const struct mylite_insert_bound_value *values,
    const struct mylite_insert_table_column *column, const struct mylite_sql_ast_node *expression,
    uint64_t statement_row_count, struct mylite_insert_execution_state *state,
    const struct mylite_dml_expression_callbacks *callbacks,
    struct mylite_insert_bound_value *out_value);
int mylite_dml_copy_update_statement(const struct mylite_sql_ast_node *statement,
                                     struct mylite_update_plan *plan);
int mylite_dml_copy_delete_statement(const struct mylite_sql_ast_node *statement,
                                     struct mylite_delete_plan *plan);
int mylite_dml_copy_update_target_to_select_table(mylite_db *database,
                                                  const struct mylite_update_plan *plan,
                                                  struct mylite_select_table *table);
int mylite_dml_copy_delete_target_to_select_table(mylite_db *database,
                                                  const struct mylite_delete_plan *plan,
                                                  struct mylite_select_table *table);
int mylite_dml_set_delete_unknown_column_error(mylite_db *database, const char *column_name,
                                               const char *clause_context);
int mylite_dml_set_delete_unsupported_clause_error(mylite_db *database);
int mylite_dml_bind_delete_subset(mylite_db *database, const struct mylite_delete_plan *plan,
                                  const struct mylite_select_table *table);
int mylite_dml_resolve_delete_target(mylite_db *database, struct mylite_select_table *table);
int mylite_dml_bind_delete_order_by_clause(mylite_db *database,
                                           const struct mylite_delete_plan *plan,
                                           const struct mylite_select_table *table,
                                           struct mylite_update_order_plan *order_plan);
int mylite_dml_bind_update_subset(mylite_db *database, const struct mylite_update_plan *plan,
                                  const struct mylite_select_table *table,
                                  struct mylite_update_bound_assignment **out_assignments);
int mylite_dml_bind_update_order_by_clause(mylite_db *database,
                                           const struct mylite_update_plan *plan,
                                           const struct mylite_select_table *table,
                                           struct mylite_update_order_plan *order_plan);
int mylite_dml_bind_update_assignment_targets(mylite_db *database,
                                              const struct mylite_update_plan *plan,
                                              const struct mylite_select_table *table,
                                              struct mylite_update_bound_assignment *assignments,
                                              size_t assignment_count);
int mylite_dml_copy_update_sqlite_row(mylite_db *database, const struct mylite_select_table *table,
                                      sqlite3_stmt *scan, struct mylite_update_row *out_row);
int mylite_dml_append_update_row(mylite_db *database, struct mylite_update_rowset *rowset,
                                 struct mylite_update_row *row);
int mylite_dml_materialize_update_rows(mylite_db *database, const struct mylite_update_plan *plan,
                                       const struct mylite_select_table *table,
                                       const struct mylite_update_order_plan *order_plan,
                                       const struct mylite_dml_expression_callbacks *callbacks,
                                       struct mylite_update_rowset *rowset);
int mylite_dml_materialize_delete_rows(mylite_db *database, const struct mylite_delete_plan *plan,
                                       const struct mylite_select_table *table,
                                       const struct mylite_update_order_plan *order_plan,
                                       const struct mylite_dml_expression_callbacks *callbacks,
                                       struct mylite_update_rowset *rowset);
int mylite_dml_add_update_order_key(struct mylite_update_order_plan *plan,
                                    const struct mylite_select_order_key *order_key);
int mylite_dml_copy_update_candidate_values(mylite_db *database,
                                            const struct mylite_update_row *row,
                                            struct mylite_update_row *candidate);
int mylite_dml_resolve_update_default_value(mylite_db *database,
                                            const struct mylite_insert_table_column *column,
                                            struct mylite_expression_value *out_value);
int mylite_dml_resolve_default_function_value(mylite_db *database,
                                              const struct mylite_insert_table_column *column,
                                              struct mylite_expression_value *out_value);
int mylite_dml_copy_insert_bound_value_to_expression(const struct mylite_insert_bound_value *value,
                                                     struct mylite_expression_value *out_value);
int mylite_dml_validate_update_assignment_value(mylite_db *database,
                                                const struct mylite_insert_table_column *column,
                                                struct mylite_expression_value *value);
int mylite_dml_advance_update_auto_increment(mylite_db *database,
                                             const struct mylite_insert_table *write_table,
                                             const struct mylite_update_row *candidate,
                                             uint64_t *next_auto_increment);
bool mylite_dml_update_expression_value_positive_uint64(const struct mylite_expression_value *value,
                                                        uint64_t *out_value);
bool mylite_dml_update_row_changed(const struct mylite_update_row *stored,
                                   const struct mylite_update_row *candidate);
int mylite_dml_set_update_unknown_column_error(mylite_db *database, const char *column_name,
                                               const char *clause_context);
int mylite_dml_set_update_unsupported_expression_error(mylite_db *database,
                                                       const char *clause_context);
int mylite_dml_set_update_unsupported_clause_error(mylite_db *database);
int mylite_dml_set_update_unsupported_assignment_error(mylite_db *database);
int mylite_dml_set_not_null_column_error(mylite_db *database, const char *column_name);
int mylite_dml_sort_update_rowset(struct mylite_update_rowset *rowset,
                                  const struct mylite_update_order_plan *order_plan);
void mylite_dml_apply_update_limit(const struct mylite_sql_ast_node *limit_clause,
                                   struct mylite_update_rowset *rowset);
int mylite_dml_execute_delete_rows_transaction(mylite_db *database,
                                               const struct mylite_select_table *table,
                                               const struct mylite_update_rowset *rowset,
                                               int64_t *out_affected_rows);
int mylite_dml_execute_multi_delete_rows_transaction(
    mylite_db *database, const struct mylite_select_plan *plan, const size_t *target_table_indexes,
    const struct mylite_update_rowset *rowsets, size_t target_count, int64_t *out_affected_rows);
int mylite_dml_execute_update_rows_transaction(
    mylite_db *database, const struct mylite_select_table *table,
    const struct mylite_insert_table *write_table,
    const struct mylite_update_bound_assignment *assignments, size_t assignment_count,
    const struct mylite_dml_expression_callbacks *callbacks,
    const struct mylite_update_rowset *rowset, int64_t *out_affected_rows);
char *mylite_dml_build_update_scan_sql(mylite_db *database,
                                       const struct mylite_select_table *table);
char *mylite_dml_build_update_physical_sql(mylite_db *database,
                                           const struct mylite_select_table *table);
char *mylite_dml_build_update_unique_check_sql(mylite_db *database,
                                               const struct mylite_select_table *table,
                                               const struct mylite_insert_table *write_table,
                                               const struct mylite_insert_unique_index *index);
int mylite_dml_bind_update_unique_check_values(mylite_db *database, sqlite3_stmt *check,
                                               const struct mylite_insert_unique_index *index,
                                               const struct mylite_update_row *candidate);
int mylite_dml_bind_update_row_values(mylite_db *database, sqlite3_stmt *update,
                                      const struct mylite_update_row *candidate);
void mylite_dml_insert_values_plan_deinit(struct mylite_insert_values_plan *plan);
void mylite_dml_insert_set_plan_deinit(struct mylite_insert_set_plan *plan);
void mylite_dml_insert_set_assignment_deinit(struct mylite_insert_set_assignment *assignment);
void mylite_dml_insert_duplicate_update_plan_deinit(
    struct mylite_insert_duplicate_update_plan *plan);
void mylite_dml_insert_update_assignment_deinit(struct mylite_insert_update_assignment *assignment);
void mylite_dml_update_plan_deinit(struct mylite_update_plan *plan);
void mylite_dml_update_assignment_deinit(struct mylite_update_assignment *assignment);
void mylite_dml_update_order_plan_deinit(struct mylite_update_order_plan *plan);
void mylite_dml_update_rowset_deinit(struct mylite_update_rowset *rowset);
void mylite_dml_update_row_deinit(struct mylite_update_row *row);
void mylite_dml_delete_plan_deinit(struct mylite_delete_plan *plan);
void mylite_dml_insert_row_deinit(struct mylite_insert_row *row);
void mylite_dml_insert_value_deinit(struct mylite_insert_value *value);
void mylite_dml_insert_table_deinit(struct mylite_insert_table *table);
void mylite_dml_insert_table_column_deinit(struct mylite_insert_table_column *column);

#endif
