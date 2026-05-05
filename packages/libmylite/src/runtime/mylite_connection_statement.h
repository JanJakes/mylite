#ifndef MYLITE_RUNTIME_MYLITE_CONNECTION_STATEMENT_H
#define MYLITE_RUNTIME_MYLITE_CONNECTION_STATEMENT_H

#include <mylite/mylite.h>

#include "mylite_connection_statement_types.h"

struct mylite_sql_ast_node;

int mylite_connection_prepare_charset_statement(mylite_db *database,
                                                const struct mylite_sql_ast_node *statement,
                                                mylite_stmt **out_stmt);
int mylite_connection_prepare_sql_mode_statement(mylite_db *database,
                                                 const struct mylite_sql_ast_node *statement,
                                                 mylite_stmt **out_stmt);
int mylite_connection_execute_set_names_statement(mylite_stmt *stmt);
int mylite_connection_execute_set_character_set_statement(mylite_stmt *stmt);
int mylite_connection_execute_set_sql_mode_statement(mylite_stmt *stmt);
void mylite_connection_charset_plan_deinit(struct mylite_connection_charset_plan *plan);
void mylite_connection_sql_mode_plan_deinit(struct mylite_connection_sql_mode_plan *plan);

#endif
