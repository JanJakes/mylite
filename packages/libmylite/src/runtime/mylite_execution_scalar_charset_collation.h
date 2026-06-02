#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_SCALAR_CHARSET_COLLATION_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_SCALAR_CHARSET_COLLATION_H

#include "mylite_ast.h"

#include <stdbool.h>

struct mylite_db;
struct mylite_execution_catalog_scalar_collation;
struct mylite_sql_ast_node;
struct session_scalar_cell;

enum planned_charset_collation_function_kind {
    PLANNED_CHARSET_COLLATION_FUNCTION_NONE = 0,
    PLANNED_CHARSET_COLLATION_FUNCTION_CHARSET = 1,
    PLANNED_CHARSET_COLLATION_FUNCTION_COLLATION = 2,
    PLANNED_CHARSET_COLLATION_FUNCTION_COERCIBILITY = 3,
};

int mylite_execution_scalar_charset_collation_function_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_scalar_expression_charset_collation_metadata(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char **out_charset,
    const char **out_collation
);
int mylite_execution_charset_collation_scalar_result(
    struct mylite_db *database,
    enum planned_charset_collation_function_kind function_kind,
    const struct mylite_sql_ast_node *expression,
    const char **out_result
);
int mylite_execution_charset_collation_select_result(
    enum planned_charset_collation_function_kind function_kind,
    const char *charset,
    const char *collation,
    const char **out_result
);
int mylite_execution_scalar_collation_info_for_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_execution_catalog_scalar_collation **out_info
);
const struct mylite_execution_catalog_scalar_collation *mylite_execution_scalar_collation_info_by_name(
    const char *collation_name
);
bool mylite_execution_coercibility_binary_wrapper_column_reference(
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node **out_column_reference
);
enum planned_charset_collation_function_kind mylite_execution_charset_collation_function_kind(
    enum mylite_sql_ast_node_kind ast_kind
);
bool mylite_execution_is_charset_collation_function_kind(enum mylite_sql_ast_node_kind ast_kind);

#endif
