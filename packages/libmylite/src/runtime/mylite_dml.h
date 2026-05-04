#ifndef MYLITE_RUNTIME_MYLITE_DML_H
#define MYLITE_RUNTIME_MYLITE_DML_H

#include "mylite_dml_types.h"

int mylite_dml_copy_insert_values_statement(
    const struct mylite_sql_ast_node *statement, struct mylite_insert_values_plan *values_plan,
    struct mylite_insert_duplicate_update_plan *update_plan);
int mylite_dml_copy_insert_set_statement(const struct mylite_sql_ast_node *statement,
                                         struct mylite_insert_values_plan *values_plan,
                                         struct mylite_insert_set_plan *set_plan,
                                         struct mylite_insert_duplicate_update_plan *update_plan);
int mylite_dml_copy_replace_values_statement(const struct mylite_sql_ast_node *statement,
                                             struct mylite_insert_values_plan *values_plan);
int mylite_dml_copy_replace_set_statement(const struct mylite_sql_ast_node *statement,
                                          struct mylite_insert_values_plan *values_plan,
                                          struct mylite_insert_set_plan *set_plan);
void mylite_dml_insert_values_plan_deinit(struct mylite_insert_values_plan *plan);
void mylite_dml_insert_set_plan_deinit(struct mylite_insert_set_plan *plan);
void mylite_dml_insert_set_assignment_deinit(struct mylite_insert_set_assignment *assignment);
void mylite_dml_insert_duplicate_update_plan_deinit(
    struct mylite_insert_duplicate_update_plan *plan);
void mylite_dml_insert_update_assignment_deinit(struct mylite_insert_update_assignment *assignment);
void mylite_dml_update_plan_deinit(struct mylite_update_plan *plan);
void mylite_dml_update_assignment_deinit(struct mylite_update_assignment *assignment);
void mylite_dml_update_order_plan_deinit(struct mylite_update_order_plan *plan);
void mylite_dml_update_rowset_deinit(struct mylite_update_rowset *rowset);
void mylite_dml_update_row_deinit(struct mylite_update_row *row);
void mylite_dml_delete_plan_deinit(struct mylite_delete_plan *plan);
void mylite_dml_insert_row_deinit(struct mylite_insert_row *row);
void mylite_dml_insert_value_deinit(struct mylite_insert_value *value);
void mylite_dml_insert_table_deinit(struct mylite_insert_table *table);
void mylite_dml_insert_table_column_deinit(struct mylite_insert_table_column *column);
void mylite_dml_insert_bound_values_deinit(struct mylite_insert_bound_value *values,
                                           size_t value_count);
void mylite_dml_insert_bound_value_deinit(struct mylite_insert_bound_value *value);

#endif
