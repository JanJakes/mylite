#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_INFORMATION_SCHEMA_PLAN_SUPPORT_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_INFORMATION_SCHEMA_PLAN_SUPPORT_H

struct mylite_db;
struct mylite_sql_ast_node;

int mylite_execution_information_schema_copy_select_item_alias(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *alias,
    char **out_text
);

#endif
