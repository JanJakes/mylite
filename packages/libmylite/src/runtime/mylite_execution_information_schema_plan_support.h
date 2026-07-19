#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_INFORMATION_SCHEMA_PLAN_SUPPORT_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_INFORMATION_SCHEMA_PLAN_SUPPORT_H

struct mylite_db;
struct mylite_result_column_descriptor;
struct mylite_sql_source_span;
struct mylite_sql_ast_node;

int mylite_execution_information_schema_copy_select_item_alias(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *alias,
    char **out_text
);
int mylite_execution_information_schema_copy_aggregate_label(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    char **out_text
);
int mylite_execution_information_schema_make_scalar_result_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *label,
    struct mylite_result_column_descriptor *out_descriptor
);

#endif
