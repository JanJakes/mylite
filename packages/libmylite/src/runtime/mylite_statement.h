#ifndef MYLITE_RUNTIME_MYLITE_STATEMENT_H
#define MYLITE_RUNTIME_MYLITE_STATEMENT_H

#include "mylite_runtime.h"

const struct mylite_expression_value *
mylite_statement_table_select_current_output_value(const mylite_stmt *stmt, int column);
const char *mylite_statement_table_select_current_output_text(const mylite_stmt *stmt, int column);
int mylite_statement_execute_custom(mylite_stmt *stmt);
int mylite_statement_prepare_sqlite(mylite_db *database, const char *sqlite_sql,
                                    mylite_stmt **out_stmt);
bool mylite_statement_kind_writes(enum mylite_stmt_kind kind);
bool mylite_statement_ast_preserves_diagnostics(const struct mylite_sql_ast_node *statement);
int mylite_statement_clone_sql_ast_subtree(struct mylite_sql_ast *ast,
                                           const struct mylite_sql_ast_node *node,
                                           const char *source_sql, const char *sql_copy,
                                           size_t sql_length,
                                           struct mylite_sql_ast_node **out_node);
void mylite_statement_record_row_count(mylite_stmt *stmt);
void mylite_statement_scalar_result_deinit(struct mylite_scalar_result *result);
void mylite_statement_select_constant_values_deinit(mylite_stmt *stmt);
void mylite_statement_union_plan_deinit(struct mylite_union_plan *plan);

#endif
