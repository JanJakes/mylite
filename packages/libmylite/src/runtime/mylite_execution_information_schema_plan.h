#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_INFORMATION_SCHEMA_PLAN_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_INFORMATION_SCHEMA_PLAN_H

#include "mylite_execution_plan_types.h"

#include <stddef.h>

struct information_schema_query;
struct information_schema_projection_expression;
struct information_schema_predicate_plan;
struct mylite_db;
struct mylite_execution_catalog_table_definition;
struct mylite_sql_ast_node;

int mylite_execution_information_schema_plan_projection(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    struct information_schema_query *out_query
);
int mylite_execution_information_schema_plan_group(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *group_clause,
    struct information_schema_query *out_query
);
int mylite_execution_information_schema_plan_order(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_clause,
    struct information_schema_query *out_query
);
int mylite_execution_information_schema_plan_limit(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *limit_clause,
    struct information_schema_query *out_query
);
int mylite_execution_information_schema_plan_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    struct information_schema_query *out_query
);
int mylite_execution_information_schema_resolve_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct information_schema_query *out_query
);
int mylite_execution_information_schema_resolve_column_reference(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *column_node,
    enum column_reference_diagnostic_context diagnostic_context,
    size_t *out_column_index
);
int mylite_execution_information_schema_table_definition_index(
    const struct mylite_execution_catalog_table_definition *definition,
    const char *column_name,
    size_t *out_index
);
const struct mylite_execution_catalog_table_definition *mylite_execution_find_information_schema_table_definition(
    const char *table_name
);
void mylite_execution_information_schema_projection_expression_deinit(
    struct information_schema_projection_expression *expression
);
void mylite_execution_information_schema_predicate_plan_deinit(
    struct information_schema_predicate_plan *plan
);

#endif
