#ifndef MYLITE_RUNTIME_MYLITE_EXPRESSION_DESCRIPTOR_STRING_QUOTE_H
#define MYLITE_RUNTIME_MYLITE_EXPRESSION_DESCRIPTOR_STRING_QUOTE_H

#include <mylite/mylite.h>

#include "mylite_expression_descriptor_string.h"

#include <stdint.h>

struct mylite_select_plan;
struct mylite_sql_ast_node;

uint64_t mylite_expression_descriptor_quote_function_result_length(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_expression_descriptor_string_callbacks *callbacks);

#endif
