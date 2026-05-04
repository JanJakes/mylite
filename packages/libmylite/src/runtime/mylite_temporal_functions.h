#ifndef MYLITE_RUNTIME_MYLITE_TEMPORAL_FUNCTIONS_H
#define MYLITE_RUNTIME_MYLITE_TEMPORAL_FUNCTIONS_H

#include <mylite/mylite.h>

#include <stdbool.h>

struct mylite_expression_value;
struct mylite_sql_ast_node;

int mylite_temporal_evaluate_current_function(mylite_stmt *stmt,
                                              const struct mylite_sql_ast_node *function_call,
                                              struct mylite_expression_value *out_value);
bool mylite_temporal_current_function_fsp(const struct mylite_sql_ast_node *function_call,
                                          unsigned int *out_fsp);
bool mylite_temporal_function_name_is_current(const struct mylite_sql_ast_node *name);
bool mylite_temporal_function_name_is_current_datetime(const struct mylite_sql_ast_node *name);
bool mylite_temporal_function_name_is_current_date(const struct mylite_sql_ast_node *name);
bool mylite_temporal_function_name_is_current_time(const struct mylite_sql_ast_node *name);

#endif
