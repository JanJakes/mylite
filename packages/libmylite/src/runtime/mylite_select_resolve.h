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
int mylite_select_set_unknown_table_error(mylite_db *database, const char *table_name);
int mylite_select_set_unknown_where_column_error(mylite_db *database, const char *column_name);
int mylite_select_set_unknown_group_column_error(mylite_db *database, const char *column_name);
int mylite_select_resolve_group_reference(mylite_db *database,
                                          const struct mylite_select_plan *plan,
                                          const struct mylite_sql_ast_node *expression,
                                          enum mylite_select_group_key_kind *out_kind,
                                          size_t *out_index);
int mylite_select_resolve_having_reference(mylite_db *database,
                                           const struct mylite_select_plan *plan,
                                           const struct mylite_sql_ast_node *expression,
                                           enum mylite_select_order_key_kind *out_kind,
                                           size_t *out_index);
int mylite_select_resolve_having_reference_internal(mylite_db *database,
                                                    const struct mylite_select_plan *plan,
                                                    const struct mylite_sql_ast_node *expression,
                                                    enum mylite_select_order_key_kind *out_kind,
                                                    size_t *out_index, bool emit_warnings);
bool mylite_select_column_index_is_grouped(const struct mylite_select_plan *plan,
                                           size_t column_index);

#endif
