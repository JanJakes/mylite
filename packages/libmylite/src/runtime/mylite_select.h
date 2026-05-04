#ifndef MYLITE_RUNTIME_MYLITE_SELECT_H
#define MYLITE_RUNTIME_MYLITE_SELECT_H

#include "mylite_select_types.h"

void mylite_select_plan_deinit(struct mylite_select_plan *plan);
void mylite_select_table_deinit(struct mylite_select_table *table);
void mylite_select_column_deinit(struct mylite_select_column *column);
void mylite_select_output_column_deinit(struct mylite_select_output_column *column);
void mylite_select_aggregate_binding_deinit(struct mylite_select_aggregate_binding *binding);
void mylite_select_column_sequence_deinit(struct mylite_select_column_sequence *sequence);
int mylite_select_plan_add_output_column(struct mylite_select_plan *plan,
                                         const struct mylite_select_output_column *output);
int mylite_select_plan_add_order_key(struct mylite_select_plan *plan,
                                     const struct mylite_select_order_key *order_key);
int mylite_select_plan_add_group_key(struct mylite_select_plan *plan,
                                     const struct mylite_select_group_key *group_key);
int mylite_select_plan_add_aggregate_binding(struct mylite_select_plan *plan,
                                             const struct mylite_select_aggregate_binding *binding);
void mylite_select_plan_clear_aggregate_bindings(struct mylite_select_plan *plan);
void mylite_select_plan_mark_output_order_reference(struct mylite_select_plan *plan,
                                                    size_t output_index);
size_t mylite_select_plan_table_count(const struct mylite_select_plan *plan);
struct mylite_select_table *mylite_select_plan_table(struct mylite_select_plan *plan,
                                                     size_t table_index);
const struct mylite_select_table *
mylite_select_plan_table_const(const struct mylite_select_plan *plan, size_t table_index);
size_t mylite_select_plan_column_count(const struct mylite_select_plan *plan);
const struct mylite_select_column *
mylite_select_plan_column_const(const struct mylite_select_plan *plan, size_t column_index,
                                const struct mylite_select_table **out_table);
size_t mylite_select_count_column_parts_using_matches(const struct mylite_select_plan *plan,
                                                      const char *column_name,
                                                      struct mylite_select_table_range range,
                                                      size_t *match_index);
int mylite_select_resolve_column_in_table(const struct mylite_select_plan *plan,
                                          const struct mylite_select_table *table,
                                          const char *column_name, size_t *out_index);
int mylite_select_set_ambiguous_column_error(mylite_db *database, const char *column_name,
                                             const char *clause_context);
bool mylite_select_column_index_is_using_column_in_range(const struct mylite_select_plan *plan,
                                                         size_t column_index,
                                                         struct mylite_select_table_range range);
int mylite_select_bind_limit_clause(const struct mylite_sql_ast_node *limit_clause,
                                    struct mylite_select_plan *plan);
bool mylite_select_limit_row_is_kept(const struct mylite_select_limit *limit,
                                     struct mylite_select_limit_position position);
bool mylite_select_limit_is_full(const struct mylite_select_limit *limit, size_t kept_count);
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
