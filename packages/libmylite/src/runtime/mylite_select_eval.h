#ifndef MYLITE_RUNTIME_MYLITE_SELECT_EVAL_H
#define MYLITE_RUNTIME_MYLITE_SELECT_EVAL_H

#include <mylite/mylite.h>

#include "mylite_select_types.h"

#include <stdbool.h>
#include <stddef.h>

struct mylite_expression_eval_context;
struct mylite_expression_value;
struct mylite_expression_warnings;

struct mylite_select_eval_callbacks {
    int (*resolve_order_reference)(
        mylite_db *database,
        const struct mylite_select_plan *plan,
        const struct mylite_sql_ast_node *expression,
        enum mylite_select_order_key_kind *out_kind,
        size_t *out_index
    );
    int (*resolve_having_reference)(
        mylite_db *database,
        const struct mylite_select_plan *plan,
        const struct mylite_sql_ast_node *expression,
        enum mylite_select_order_key_kind *out_kind,
        size_t *out_index,
        bool emit_warnings
    );
    int (*eval_session_function)(
        mylite_stmt *stmt,
        const struct mylite_sql_ast_node *function_call,
        const struct mylite_expression_eval_context *expression_context,
        struct mylite_expression_warnings *warnings,
        const struct mylite_select_table *table,
        struct mylite_expression_value *out_value
    );
    int (*eval_subquery)(
        mylite_stmt *stmt,
        const struct mylite_sql_ast_node *subquery,
        struct mylite_expression_warnings *warnings,
        struct mylite_expression_value *out_value
    );
    int (*eval_in_subquery)(
        mylite_stmt *stmt,
        const struct mylite_sql_ast_node *expression,
        const struct mylite_expression_value *left,
        struct mylite_expression_warnings *warnings,
        struct mylite_expression_value *out_value
    );
    int (*eval_quantified_subquery)(
        mylite_stmt *stmt,
        const struct mylite_sql_ast_node *expression,
        const struct mylite_expression_value *left,
        struct mylite_expression_warnings *warnings,
        struct mylite_expression_value *out_value
    );
    int (*eval_row_subquery)(
        mylite_stmt *stmt,
        const struct mylite_sql_ast_node *expression,
        const struct mylite_expression_eval_context *expression_context,
        struct mylite_expression_warnings *warnings,
        struct mylite_expression_value *out_value
    );
    int (*copy_column_value)(
        mylite_stmt *stmt,
        size_t column_index,
        struct mylite_expression_value *out_value
    );
    int (*set_expression_eval_error)(mylite_stmt *stmt);
};

int mylite_select_eval_group_key(
    mylite_stmt *stmt,
    const struct mylite_table_select_row *row,
    const struct mylite_select_group_key *group_key,
    const struct mylite_select_eval_callbacks *callbacks,
    struct mylite_expression_value *out_value
);
int mylite_select_eval_having(
    mylite_stmt *stmt,
    const struct mylite_table_select_row *row,
    const struct mylite_select_eval_callbacks *callbacks,
    bool *out_matches
);
int mylite_select_eval_aggregate_argument(
    mylite_stmt *stmt,
    const struct mylite_select_aggregate_binding *binding,
    const struct mylite_table_select_row *row,
    const struct mylite_select_eval_callbacks *callbacks,
    struct mylite_expression_value *out_value
);
int mylite_select_eval_order_values(
    mylite_stmt *stmt,
    struct mylite_table_select_row *row,
    const struct mylite_select_eval_callbacks *callbacks
);
int mylite_select_eval_materialize_output_values(
    mylite_stmt *stmt,
    struct mylite_table_select_row *row,
    const struct mylite_select_eval_callbacks *callbacks
);
int mylite_select_eval_set_current_row(
    mylite_stmt *stmt,
    const struct mylite_table_select_row *row,
    const struct mylite_select_eval_callbacks *callbacks
);
int mylite_select_eval_constant_predicate(
    mylite_stmt *stmt,
    const struct mylite_select_eval_callbacks *callbacks
);
int mylite_select_eval_expression_predicate(
    mylite_stmt *stmt,
    const struct mylite_table_select_row *row,
    const struct mylite_sql_ast_node *predicate,
    const struct mylite_select_eval_callbacks *callbacks,
    bool *out_matches
);
int mylite_select_eval_row_predicate(
    mylite_stmt *stmt,
    const struct mylite_table_select_row *row,
    const struct mylite_select_eval_callbacks *callbacks,
    bool *out_matches
);

#endif
