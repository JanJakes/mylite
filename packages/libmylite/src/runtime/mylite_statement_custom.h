#ifndef MYLITE_RUNTIME_MYLITE_STATEMENT_CUSTOM_H
#define MYLITE_RUNTIME_MYLITE_STATEMENT_CUSTOM_H

#include <mylite/mylite.h>

#include "mylite_statement_types.h"

struct mylite_select_scalar_eval_callbacks;
struct mylite_sql_ast_node;

int mylite_statement_prepare_custom(
    mylite_db *database,
    enum mylite_stmt_kind kind,
    const struct mylite_sql_ast_node *statement,
    const struct mylite_select_scalar_eval_callbacks *scalar_select_callbacks,
    mylite_stmt **out_stmt
);

#endif
