#ifndef MYLITE_RUNTIME_MYLITE_SELECT_SUBQUERY_DIAGNOSTICS_H
#define MYLITE_RUNTIME_MYLITE_SELECT_SUBQUERY_DIAGNOSTICS_H

#include <mylite/mylite.h>

#include <stddef.h>

struct mylite_sql_ast_node;

int mylite_select_subquery_set_operand_column_count_error(
    mylite_db *database,
    size_t expected_width
);
int mylite_select_subquery_set_operand_columns_error(mylite_db *database);
int mylite_select_subquery_set_in_limit_error(mylite_db *database);
int mylite_select_subquery_set_row_quantified_non_alias_error(
    mylite_db *database,
    const struct mylite_sql_ast_node *expression
);
int mylite_select_subquery_set_scalar_cardinality_error(mylite_db *database);

#endif
