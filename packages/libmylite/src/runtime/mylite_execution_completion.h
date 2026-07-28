#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_COMPLETION_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_COMPLETION_H

#include <mylite/mylite.h>

#include "mylite_execution_result_capability.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct mylite_db;
struct mylite_sql_ast_node;

int64_t row_count_for_completed_statement(
    const struct mylite_sql_ast_node *statement,
    const mylite_result *result
);
bool statement_preserves_diagnostics_snapshot(const struct mylite_sql_ast_node *statement);
int finish_successful_result(
    struct mylite_db *database,
    mylite_result *result,
    mylite_result **out_result
);
int finish_successful_result_with_warning_count(
    mylite_result *result,
    size_t warning_count,
    mylite_result **out_result
);

#endif
