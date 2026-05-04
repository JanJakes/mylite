#ifndef MYLITE_RUNTIME_MYLITE_SELECT_RESOLVE_H
#define MYLITE_RUNTIME_MYLITE_SELECT_RESOLVE_H

#include <mylite/mylite.h>

#include "mylite_select_types.h"

#include <stdbool.h>
#include <stddef.h>

int mylite_select_resolve_plan_wildcard(mylite_db *database, const struct mylite_select_plan *plan,
                                        const struct mylite_sql_ast_node *wildcard,
                                        size_t *out_table_index, bool *out_all);
int mylite_select_resolve_plan_column_reference(mylite_db *database,
                                                const struct mylite_select_plan *plan,
                                                const struct mylite_sql_ast_node *expression,
                                                const char *clause_context, size_t *out_index);
int mylite_select_resolve_plan_column_reference_in_scope(
    mylite_db *database, const struct mylite_select_plan *plan,
    const struct mylite_sql_ast_node *expression, const char *clause_context, size_t first_table,
    size_t table_count, size_t *out_index);
int mylite_select_resolve_plan_column_parts(mylite_db *database,
                                            const struct mylite_select_plan *plan, char **parts,
                                            size_t part_count, const char *clause_context,
                                            size_t *out_index);
size_t mylite_select_count_plan_column_parts_matches(const struct mylite_select_plan *plan,
                                                     char **parts, size_t part_count,
                                                     size_t first_table, size_t table_count,
                                                     size_t *match_index);
char *mylite_select_copy_wildcard_qualifier_name(const struct mylite_sql_ast_node *wildcard);
int mylite_select_set_unknown_where_column_error(mylite_db *database, const char *column_name);
int mylite_select_set_unknown_order_column_error(mylite_db *database, const char *column_name);

#endif
