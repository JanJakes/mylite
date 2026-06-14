#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_AST_INTERNAL_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_AST_INTERNAL_H

#include "mylite_ast.h"
#include "mylite_parser.h"

#include <stddef.h>

struct mylite_db;

const struct mylite_sql_ast_node *mylite_execution_child_at(
    const struct mylite_sql_ast_node *node,
    size_t index
);
const struct mylite_sql_ast_node *mylite_execution_child_with_kind(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_node_kind kind
);
int mylite_execution_script_statement_count(
    const struct mylite_sql_ast_node *root,
    size_t *out_count
);
void mylite_execution_set_parse_result_error(
    struct mylite_db *database,
    const struct mylite_sql_parse_result *parse_result
);
void mylite_execution_set_multi_statement_parse_error(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *root
);

static inline const struct mylite_sql_ast_node *child_at(
    const struct mylite_sql_ast_node *node,
    size_t index
) {
    return mylite_execution_child_at(node, index);
}

static inline const struct mylite_sql_ast_node *child_with_kind(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_node_kind kind
) {
    return mylite_execution_child_with_kind(node, kind);
}

static inline int script_statement_count(
    const struct mylite_sql_ast_node *root,
    size_t *out_count
) {
    return mylite_execution_script_statement_count(root, out_count);
}

static inline void set_parse_error(
    struct mylite_db *database,
    const struct mylite_sql_parse_result *parse_result
) {
    mylite_execution_set_parse_result_error(database, parse_result);
}

static inline void set_multi_statement_parse_error(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *root
) {
    mylite_execution_set_multi_statement_parse_error(database, root);
}

#endif
