#ifndef MYLITE_RUNTIME_MYLITE_EXPRESSION_VALIDATION_H
#define MYLITE_RUNTIME_MYLITE_EXPRESSION_VALIDATION_H

#include <mylite/mylite.h>

#include "sql/mylite_ast.h"

#include <stdbool.h>

int mylite_expression_validate_char_function_charset(
    mylite_db *database,
    const struct mylite_sql_ast_node *expression
);
int mylite_expression_validate_cast_target_charset(
    mylite_db *database,
    const struct mylite_sql_ast_node *expression
);
int mylite_expression_validate_collate_operator(
    mylite_db *database,
    const struct mylite_sql_ast_node *expression
);
bool mylite_expression_char_function_charset_name_is_supported(const char *name);
bool mylite_expression_literal_is_supported(const struct mylite_sql_ast_node *expression);

#endif
