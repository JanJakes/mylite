#ifndef MYLITE_RUNTIME_MYLITE_STATEMENT_AST_H
#define MYLITE_RUNTIME_MYLITE_STATEMENT_AST_H

#include "sql/mylite_ast.h"

#include <stddef.h>

int mylite_statement_ast_clone_subtree(struct mylite_sql_ast *ast,
                                       const struct mylite_sql_ast_node *node,
                                       const char *source_sql, const char *sql_copy,
                                       size_t sql_length, struct mylite_sql_ast_node **out_node);

#endif
