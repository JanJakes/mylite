#ifndef MYLITE_RUNTIME_MYLITE_DML_UPDATE_DELETE_EXPRESSION_BIND_H
#define MYLITE_RUNTIME_MYLITE_DML_UPDATE_DELETE_EXPRESSION_BIND_H

#include <mylite/mylite.h>

struct mylite_select_table;
struct mylite_sql_ast_node;

typedef int (*mylite_dml_mutation_unknown_column_error_fn)(
    mylite_db *database,
    const char *column_name,
    const char *clause_context
);
typedef int (*mylite_dml_mutation_unsupported_clause_error_fn)(mylite_db *database);
typedef int (*mylite_dml_mutation_unsupported_expression_error_fn)(
    mylite_db *database,
    const char *clause_context
);

struct mylite_dml_mutation_expression_bind_diagnostics {
    mylite_dml_mutation_unknown_column_error_fn set_unknown_column_error;
    mylite_dml_mutation_unsupported_clause_error_fn set_unsupported_clause_error;
    mylite_dml_mutation_unsupported_expression_error_fn set_unsupported_expression_error;
};

int mylite_dml_bind_mutation_expression(
    mylite_db *database,
    const struct mylite_select_table *table,
    const struct mylite_sql_ast_node *expression,
    const char *clause_context,
    const struct mylite_dml_mutation_expression_bind_diagnostics *diagnostics
);

#endif
