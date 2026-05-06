#ifndef MYLITE_RUNTIME_MYLITE_PREPARED_STATEMENTS_H
#define MYLITE_RUNTIME_MYLITE_PREPARED_STATEMENTS_H

#include <mylite/mylite.h>

#include "mylite_prepared_statements_types.h"
#include "sql/mylite_ast.h"

int mylite_prepared_statement_prepare_prepare_statement(
    mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_stmt **out_stmt
);
int mylite_prepared_statement_prepare_execute_statement(
    mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_stmt **out_stmt
);
int mylite_prepared_statement_prepare_deallocate_statement(
    mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_stmt **out_stmt
);
int mylite_prepared_statement_execute_prepare(mylite_stmt *stmt);
int mylite_prepared_statement_execute_execute(mylite_stmt *stmt);
int mylite_prepared_statement_execute_deallocate(mylite_stmt *stmt);
void mylite_prepared_statement_store_deinit(struct mylite_prepared_statement_store *store);
void mylite_prepared_statement_prepare_plan_deinit(struct mylite_prepare_statement_plan *plan);
void mylite_prepared_statement_execute_plan_deinit(struct mylite_execute_prepared_plan *plan);
void mylite_prepared_statement_deallocate_plan_deinit(struct mylite_deallocate_prepare_plan *plan);

#endif
