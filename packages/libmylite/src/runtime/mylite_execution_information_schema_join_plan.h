#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_INFORMATION_SCHEMA_JOIN_PLAN_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_INFORMATION_SCHEMA_JOIN_PLAN_H

struct mylite_db;
struct mylite_sql_ast_node;

enum information_schema_join_compat_kind {
    INFORMATION_SCHEMA_JOIN_COMPAT_NONE = 0,
    INFORMATION_SCHEMA_JOIN_COMPAT_COLUMNS_STATISTICS,
    INFORMATION_SCHEMA_JOIN_COMPAT_COLUMNS_STATISTICS_UNION,
    INFORMATION_SCHEMA_JOIN_COMPAT_COLUMNS_TABLES,
    INFORMATION_SCHEMA_JOIN_COMPAT_TABLES_GROUPED_SIZE,
    INFORMATION_SCHEMA_JOIN_COMPAT_DYNAMIC_COLUMNS,
    INFORMATION_SCHEMA_JOIN_COMPAT_SCHEMATA_TABLES,
};

struct information_schema_join_compat_plan {
    enum information_schema_join_compat_kind kind;
    char *schema_name;
    char *table_name;
};

int mylite_execution_information_schema_join_plan_analyze(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct information_schema_join_compat_plan *out_plan
);
void mylite_execution_information_schema_join_plan_deinit(
    struct information_schema_join_compat_plan *plan
);

#endif
