#ifndef MYLITE_RUNTIME_MYLITE_EXPRESSION_COLLATION_LEAF_H
#define MYLITE_RUNTIME_MYLITE_EXPRESSION_COLLATION_LEAF_H

#include <mylite/mylite.h>

#include "mylite_expression_collation_types.h"

struct mylite_expression_collation_context;
struct mylite_expression_collation_callbacks;
struct mylite_sql_ast_node;

int mylite_expression_infer_literal_collation_info(
    mylite_db *database, const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_collation_callbacks *callbacks,
    struct mylite_charset_collation_info *out_info);
int mylite_expression_infer_identifier_collation_info(
    mylite_db *database, const struct mylite_expression_collation_context *context,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_collation_callbacks *callbacks,
    struct mylite_charset_collation_info *out_info);
int mylite_expression_infer_cast_collation_info(
    mylite_db *database, const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_collation_callbacks *callbacks,
    struct mylite_charset_collation_info *out_info);

#endif
