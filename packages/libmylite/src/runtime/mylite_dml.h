#ifndef MYLITE_RUNTIME_MYLITE_DML_H
#define MYLITE_RUNTIME_MYLITE_DML_H

#include "mylite_dml_types.h"

int mylite_dml_copy_insert_values_statement(
    const struct mylite_sql_ast_node *statement, struct mylite_insert_values_plan *values_plan,
    struct mylite_insert_duplicate_update_plan *update_plan);
int mylite_dml_copy_insert_set_statement(const struct mylite_sql_ast_node *statement,
                                         struct mylite_insert_values_plan *values_plan,
                                         struct mylite_insert_set_plan *set_plan,
                                         struct mylite_insert_duplicate_update_plan *update_plan);
int mylite_dml_copy_replace_values_statement(const struct mylite_sql_ast_node *statement,
                                             struct mylite_insert_values_plan *values_plan);
int mylite_dml_copy_replace_set_statement(const struct mylite_sql_ast_node *statement,
                                          struct mylite_insert_values_plan *values_plan,
                                          struct mylite_insert_set_plan *set_plan);
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
char *mylite_dml_build_insert_physical_sql(mylite_db *database,
                                           const struct mylite_insert_table *table);
char *mylite_dml_build_replace_delete_sql(mylite_db *database,
                                          const struct mylite_insert_table *table);
int mylite_dml_execute_insert_values_transaction(
    mylite_db *database, const char *selected_schema, const char *schema_name,
    const struct mylite_insert_values_plan *values_plan,
    const struct mylite_insert_duplicate_update_plan *update_plan,
    const struct mylite_insert_table *table, const size_t *column_indexes,
    const size_t *update_column_indexes, struct mylite_insert_transaction_result *out_result);
int mylite_dml_execute_insert_set_transaction(
    mylite_db *database, const char *selected_schema, const char *schema_name,
    const struct mylite_insert_values_plan *values_plan,
    const struct mylite_insert_set_plan *set_plan,
    const struct mylite_insert_duplicate_update_plan *update_plan,
    const struct mylite_insert_table *table, const size_t *column_indexes,
    size_t column_index_count, const size_t *update_column_indexes,
    struct mylite_insert_transaction_result *out_result);
int mylite_dml_execute_replace_values_transaction(
    mylite_db *database, const char *schema_name,
    const struct mylite_insert_values_plan *values_plan, const struct mylite_insert_table *table,
    const size_t *column_indexes, struct mylite_insert_transaction_result *out_result);
int mylite_dml_execute_replace_set_transaction(mylite_db *database, const char *schema_name,
                                               const struct mylite_insert_values_plan *values_plan,
                                               const struct mylite_insert_set_plan *set_plan,
                                               const struct mylite_insert_table *table,
                                               const size_t *column_indexes,
                                               size_t column_index_count,
                                               struct mylite_insert_transaction_result *out_result);
int mylite_dml_write_insert_candidate_row(mylite_db *database, sqlite3_stmt *insert,
                                          const struct mylite_insert_table *table,
                                          const struct mylite_insert_bound_value *values,
                                          struct mylite_insert_execution_state *state);
int mylite_dml_execute_insert_row(mylite_db *database, const struct mylite_insert_values_plan *plan,
                                  sqlite3_stmt *insert, const struct mylite_insert_table *table,
                                  const struct mylite_insert_row_column_indexes *column_indexes,
                                  struct mylite_insert_execution_state *state, size_t row_index);
int mylite_dml_execute_insert_set_row(mylite_db *database, const char *schema_name,
                                      const struct mylite_insert_values_plan *values_plan,
                                      const struct mylite_insert_set_plan *set_plan,
                                      sqlite3_stmt *insert, const struct mylite_insert_table *table,
                                      const size_t *column_indexes, size_t column_index_count,
                                      struct mylite_insert_execution_state *state,
                                      struct mylite_insert_bound_value *values,
                                      struct mylite_insert_set_row_state *row_state);
int mylite_dml_write_replace_candidate_row(mylite_db *database, sqlite3_stmt *insert,
                                           sqlite3_stmt *delete_stmt,
                                           const struct mylite_insert_table *table,
                                           struct mylite_insert_execution_state *state,
                                           const struct mylite_insert_bound_value *values);
int mylite_dml_execute_replace_row(mylite_db *database,
                                   const struct mylite_insert_values_plan *plan,
                                   sqlite3_stmt *insert, sqlite3_stmt *delete_stmt,
                                   const struct mylite_insert_table *table,
                                   const struct mylite_insert_row_column_indexes *column_indexes,
                                   struct mylite_insert_execution_state *state, size_t row_index);
int mylite_dml_execute_replace_set_row(mylite_db *database, const char *schema_name,
                                       const struct mylite_insert_values_plan *values_plan,
                                       const struct mylite_insert_set_plan *set_plan,
                                       sqlite3_stmt *insert, sqlite3_stmt *delete_stmt,
                                       const struct mylite_insert_table *table,
                                       const size_t *column_indexes, size_t column_index_count,
                                       struct mylite_insert_execution_state *state,
                                       struct mylite_insert_bound_value *values,
                                       struct mylite_insert_set_row_state *row_state);
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
    struct mylite_insert_execution_state *state, size_t row_index);
int mylite_dml_execute_insert_update_set_row(
    mylite_db *database, const char *selected_schema, const char *schema_name,
    const struct mylite_insert_values_plan *values_plan,
    const struct mylite_insert_set_plan *set_plan,
    const struct mylite_insert_duplicate_update_plan *update_plan, sqlite3_stmt *insert,
    const struct mylite_insert_table *table, const size_t *column_indexes,
    size_t column_index_count, const struct mylite_insert_row_column_indexes *row_column_indexes,
    struct mylite_insert_execution_state *state, struct mylite_insert_bound_value *values,
    struct mylite_insert_set_row_state *row_state);
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
int mylite_dml_validate_insert_unique_indexes(mylite_db *database, const char *table_name,
                                              bool ignore, const struct mylite_insert_table *table,
                                              const struct mylite_insert_bound_value *values,
                                              struct mylite_insert_execution_state *state,
                                              bool *out_ignored);
int mylite_dml_find_insert_unique_conflict(mylite_db *database,
                                           const struct mylite_insert_table *table,
                                           const struct mylite_insert_bound_value *values,
                                           struct mylite_insert_unique_conflict *out_conflict);
int mylite_dml_validate_insert_update_unique_indexes(mylite_db *database, const char *table_name,
                                                     bool ignore,
                                                     const struct mylite_insert_table *table,
                                                     const struct mylite_insert_bound_value *values,
                                                     sqlite3_int64 rowid, bool *out_conflicts);
int mylite_dml_load_insert_conflict_row(mylite_db *database,
                                        const struct mylite_insert_table *table,
                                        sqlite3_int64 rowid,
                                        struct mylite_insert_bound_value *values);
int mylite_dml_resolve_insert_row_values(mylite_db *database,
                                         const struct mylite_insert_values_plan *plan,
                                         const struct mylite_insert_table *table,
                                         const size_t *column_indexes, uint64_t statement_row_count,
                                         struct mylite_insert_execution_state *state,
                                         size_t row_index,
                                         struct mylite_insert_bound_value *out_values);
int mylite_dml_resolve_insert_set_row_values(
    mylite_db *database, const char *schema_name,
    const struct mylite_insert_values_plan *values_plan,
    const struct mylite_insert_set_plan *set_plan, const struct mylite_insert_table *table,
    const size_t *column_indexes, size_t column_index_count, uint64_t statement_row_count,
    struct mylite_insert_execution_state *state, struct mylite_insert_bound_value *values,
    struct mylite_insert_set_row_state *row_state);
int mylite_dml_resolve_insert_default_bound_value(mylite_db *database,
                                                  const struct mylite_insert_table_column *column,
                                                  uint64_t statement_row_count,
                                                  struct mylite_insert_execution_state *state,
                                                  struct mylite_insert_bound_value *out_value);
int mylite_dml_resolve_insert_implicit_expression_default(
    mylite_db *database, const struct mylite_insert_table_column *column,
    struct mylite_insert_bound_value *out_value);
int mylite_dml_resolve_insert_current_timestamp_bound_value(
    mylite_db *database, struct mylite_insert_bound_value *out_value);
uint64_t
mylite_dml_insert_auto_increment_next_value(const struct mylite_insert_execution_state *state);
int mylite_dml_bind_insert_row_values(mylite_db *database, sqlite3_stmt *insert,
                                      const struct mylite_insert_bound_value *values,
                                      size_t value_count);
int mylite_dml_bind_insert_bound_value(sqlite3_stmt *stmt, int index,
                                       const struct mylite_insert_bound_value *value);
int mylite_dml_copy_insert_sqlite_column_value(sqlite3_stmt *scan, int column,
                                               struct mylite_insert_bound_value *out_value);
int mylite_dml_copy_insert_bound_value(const struct mylite_insert_bound_value *value,
                                       struct mylite_insert_bound_value *out_value);
int mylite_dml_copy_insert_bound_values(mylite_db *database,
                                        const struct mylite_insert_bound_value *values,
                                        size_t value_count,
                                        struct mylite_insert_bound_value **out_values);
bool mylite_dml_insert_bound_value_is_numeric(const struct mylite_insert_bound_value *value,
                                              double *out_value, bool *out_is_integer);
bool mylite_dml_parse_insert_integer_text(const char *text, int64_t *out_value);
bool mylite_dml_parse_insert_real_text(const char *text, double *out_value);
int mylite_dml_copy_update_statement(const struct mylite_sql_ast_node *statement,
                                     struct mylite_update_plan *plan);
int mylite_dml_copy_delete_statement(const struct mylite_sql_ast_node *statement,
                                     struct mylite_delete_plan *plan);
int mylite_dml_copy_update_target_to_select_table(mylite_db *database,
                                                  const struct mylite_update_plan *plan,
                                                  struct mylite_select_table *table);
int mylite_dml_bind_update_assignment_targets(mylite_db *database,
                                              const struct mylite_update_plan *plan,
                                              const struct mylite_select_table *table,
                                              struct mylite_update_bound_assignment *assignments,
                                              size_t assignment_count);
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
void mylite_dml_insert_bound_values_deinit(struct mylite_insert_bound_value *values,
                                           size_t value_count);
void mylite_dml_insert_bound_value_deinit(struct mylite_insert_bound_value *value);

#endif
