#ifndef MYLITE_RUNTIME_MYLITE_SELECT_H
#define MYLITE_RUNTIME_MYLITE_SELECT_H

#include "mylite_select_types.h"

void mylite_select_plan_deinit(struct mylite_select_plan *plan);
void mylite_select_table_deinit(struct mylite_select_table *table);
void mylite_select_column_deinit(struct mylite_select_column *column);
void mylite_select_output_column_deinit(struct mylite_select_output_column *column);
void mylite_select_aggregate_binding_deinit(struct mylite_select_aggregate_binding *binding);
int mylite_select_resolve_table_target(mylite_db *database, struct mylite_select_table *table);
bool mylite_select_schema_name_is_system(const char *schema_name);
int mylite_select_resolve_column_reference(const struct mylite_select_table *table,
                                           const struct mylite_sql_ast_node *expression,
                                           size_t *out_index);
bool mylite_select_reference_qualifiers_match(const struct mylite_select_table *table, char **parts,
                                              size_t part_count);
size_t mylite_select_column_index(const struct mylite_select_table *table, const char *column_name);
char *mylite_select_copy_reference_name(const struct mylite_sql_ast_node *identifier);
int mylite_select_compare_values(const struct mylite_expression_value *left,
                                 const struct mylite_expression_value *right);
int mylite_select_compare_binary_text_values(const char *left, size_t left_length,
                                             const char *right, size_t right_length);

#endif
