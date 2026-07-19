#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_PARAMETER_BINDING_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_PARAMETER_BINDING_H

#include "sqlite3.h"

struct planned_column_aggregate_select;
struct planned_count;
struct planned_count_having_select;
struct planned_delete;
struct planned_grouped_aggregate;
struct planned_insert_select;
struct planned_row_scalar_expression;
struct planned_row_scalar_select;
struct planned_select;
struct planned_select_join_condition;
struct planned_select_predicate;
struct planned_update;

int bind_select_parameters(sqlite3_stmt *statement, const struct planned_select *plan);
int bind_select_parameters_at(
    sqlite3_stmt *statement,
    const struct planned_select *plan,
    int *parameter_index
);
int bind_select_join_condition_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_join_condition *condition,
    int *parameter_index
);
int bind_row_scalar_select_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_select *plan
);
int bind_insert_select_materialize_parameters(
    sqlite3_stmt *statement,
    const struct planned_insert_select *plan
);
int bind_row_scalar_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
);
int bind_select_predicate_parameters(
    sqlite3_stmt *statement,
    const struct planned_select_predicate *predicate,
    int *parameter_index
);
int bind_select_predicate_parameters_without_exists(
    sqlite3_stmt *statement,
    const struct planned_select_predicate *predicate,
    int *parameter_index
);
int bind_count_parameters(sqlite3_stmt *statement, const struct planned_count *plan);
int bind_count_having_select_parameters(
    sqlite3_stmt *statement,
    const struct planned_count_having_select *plan
);
int bind_column_aggregate_parameters(
    sqlite3_stmt *statement,
    const struct planned_column_aggregate_select *plan
);
int bind_grouped_aggregate_parameters(
    sqlite3_stmt *statement,
    const struct planned_grouped_aggregate *plan
);
int bind_grouped_aggregate_count_parameters(
    sqlite3_stmt *statement,
    const struct planned_grouped_aggregate *plan
);
int bind_grouped_aggregate_rollup_parameters(
    sqlite3_stmt *statement,
    const struct planned_grouped_aggregate *plan
);
int bind_delete_parameters(sqlite3_stmt *statement, const struct planned_delete *plan);
int bind_update_parameters(sqlite3_stmt *statement, const struct planned_update *plan);
int bind_update_assignment_parameter(
    sqlite3_stmt *statement,
    int parameter_index,
    const struct planned_update *plan
);
int bind_joined_update_source_parameters(
    sqlite3_stmt *statement,
    int *parameter_index,
    const struct planned_update *plan
);
int bind_update_multiple_changed_condition_parameters(
    sqlite3_stmt *statement,
    int *parameter_index,
    const struct planned_update *plan
);
int bind_update_row_scalar_changed_condition_parameters(
    sqlite3_stmt *statement,
    int *parameter_index,
    const struct planned_row_scalar_expression *expression
);
int bind_update_changed_condition_parameter(
    sqlite3_stmt *statement,
    int parameter_index,
    const struct planned_update *plan
);
int bind_update_matched_count_parameters(
    sqlite3_stmt *statement,
    const struct planned_update *plan
);

#endif
