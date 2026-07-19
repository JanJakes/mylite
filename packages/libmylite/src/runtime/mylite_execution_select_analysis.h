#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_SELECT_ANALYSIS_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_SELECT_ANALYSIS_H

#include <stdbool.h>

struct mylite_sql_ast_node;

int mylite_execution_select_parameters_are_plan_reusable(
    const struct mylite_sql_ast_node *statement,
    bool *out_reusable
);

#endif
