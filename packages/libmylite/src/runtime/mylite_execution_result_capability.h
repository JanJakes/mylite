#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_RESULT_CAPABILITY_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_RESULT_CAPABILITY_H

#include <stdbool.h>

struct mylite_sql_ast_node;

enum mylite_statement_result_capability {
    MYLITE_STATEMENT_RESULT_NONE,
    MYLITE_STATEMENT_RESULT_QUERY_NO_ROWS,
    MYLITE_STATEMENT_RESULT_STREAMING_QUERY,
    MYLITE_STATEMENT_RESULT_MATERIALIZED_QUERY,
    MYLITE_STATEMENT_RESULT_MATERIALIZED_UTILITY,
    MYLITE_STATEMENT_RESULT_DYNAMIC_QUERY,
    MYLITE_STATEMENT_RESULT_DYNAMIC_UTILITY,
};

enum mylite_statement_result_capability mylite_statement_result_capability(
    const struct mylite_sql_ast_node *statement
);
bool mylite_statement_result_may_have_rows(enum mylite_statement_result_capability capability);
bool mylite_statement_result_is_query(enum mylite_statement_result_capability capability);
bool mylite_statement_result_is_dynamic(enum mylite_statement_result_capability capability);

#endif
