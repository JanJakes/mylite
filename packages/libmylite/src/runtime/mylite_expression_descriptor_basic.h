#ifndef MYLITE_RUNTIME_MYLITE_EXPRESSION_DESCRIPTOR_BASIC_H
#define MYLITE_RUNTIME_MYLITE_EXPRESSION_DESCRIPTOR_BASIC_H

#include <mylite/mylite.h>

#include "mylite_field_descriptor.h"

struct mylite_expression_value;
struct mylite_select_plan;
struct mylite_sql_ast_node;

int mylite_expression_descriptor_infer_literal(mylite_db *database,
                                               const struct mylite_sql_ast_node *expression,
                                               const struct mylite_expression_value *value,
                                               struct mylite_field_descriptor *out_descriptor);
int mylite_expression_descriptor_infer_identifier(mylite_db *database,
                                                  const struct mylite_select_plan *plan,
                                                  const struct mylite_sql_ast_node *expression,
                                                  struct mylite_field_descriptor *out_descriptor);

#endif
