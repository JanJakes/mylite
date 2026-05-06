#ifndef MYLITE_RUNTIME_MYLITE_SELECT_H
#define MYLITE_RUNTIME_MYLITE_SELECT_H

#include "mylite_select_compare.h"
#include "mylite_select_target.h"
#include "mylite_select_types.h"
#include "mylite_select_using_range.h"

void mylite_select_plan_deinit(struct mylite_select_plan *plan);
void mylite_select_table_deinit(struct mylite_select_table *table);
void mylite_select_column_deinit(struct mylite_select_column *column);
void mylite_select_output_column_deinit(struct mylite_select_output_column *column);
void mylite_select_aggregate_binding_deinit(struct mylite_select_aggregate_binding *binding);
void mylite_select_column_sequence_deinit(struct mylite_select_column_sequence *sequence);
int mylite_select_plan_add_output_column(
    struct mylite_select_plan *plan,
    const struct mylite_select_output_column *output
);
int mylite_select_plan_add_order_key(
    struct mylite_select_plan *plan,
    const struct mylite_select_order_key *order_key
);
int mylite_select_plan_add_group_key(
    struct mylite_select_plan *plan,
    const struct mylite_select_group_key *group_key
);
int mylite_select_plan_add_aggregate_binding(
    struct mylite_select_plan *plan,
    const struct mylite_select_aggregate_binding *binding
);
void mylite_select_plan_clear_aggregate_bindings(struct mylite_select_plan *plan);
void mylite_select_plan_mark_output_order_reference(
    struct mylite_select_plan *plan,
    size_t output_index
);
size_t mylite_select_output_label_count(
    const struct mylite_select_plan *plan,
    const char *label,
    size_t *out_index
);
size_t mylite_select_output_label_span_count(
    const struct mylite_select_plan *plan,
    struct mylite_sql_source_span label,
    size_t *out_index
);
bool mylite_select_parse_uint64_span(struct mylite_sql_source_span span, uint64_t *out_value);
size_t mylite_select_plan_table_count(const struct mylite_select_plan *plan);
struct mylite_select_table *mylite_select_plan_table(
    struct mylite_select_plan *plan,
    size_t table_index
);
const struct mylite_select_table *mylite_select_plan_table_const(
    const struct mylite_select_plan *plan,
    size_t table_index
);
size_t mylite_select_plan_column_count(const struct mylite_select_plan *plan);
const struct mylite_select_column *mylite_select_plan_column_const(
    const struct mylite_select_plan *plan,
    size_t column_index,
    const struct mylite_select_table **out_table
);
int mylite_select_resolve_column_in_table(
    const struct mylite_select_plan *plan,
    const struct mylite_select_table *table,
    const char *column_name,
    size_t *out_index
);
int mylite_select_set_ambiguous_column_error(
    mylite_db *database,
    const char *column_name,
    const char *clause_context
);

static inline bool mylite_select_join_step_is_in_range(
    const struct mylite_select_join_step *step,
    struct mylite_select_table_range range
) {
    size_t range_end = range.first_table + range.table_count;
    size_t step_end = step->joined_range.first_table + step->joined_range.table_count;

    return (step->joined_range.first_table >= range.first_table && step_end <= range_end) != 0;
}

bool mylite_select_plan_has_column_span(
    const struct mylite_select_plan *plan,
    struct mylite_sql_source_span name
);
bool mylite_select_plan_has_visible_table_span(
    const struct mylite_select_plan *plan,
    struct mylite_sql_source_span name
);
bool mylite_select_plan_has_outer_join(const struct mylite_select_plan *plan);
bool mylite_select_duplicate_mode_is_distinct(enum mylite_sql_ast_select_duplicate_mode mode);
bool mylite_select_plan_requires_custom_runtime(
    const struct mylite_select_plan *plan,
    const struct mylite_select_clause_nodes *clauses
);
int mylite_select_bind_limit_clause(
    const struct mylite_sql_ast_node *limit_clause,
    struct mylite_select_plan *plan
);
bool mylite_select_limit_row_is_kept(
    const struct mylite_select_limit *limit,
    struct mylite_select_limit_position position
);
bool mylite_select_limit_is_full(const struct mylite_select_limit *limit, size_t kept_count);
int mylite_select_resolve_column_reference(
    const struct mylite_select_table *table,
    const struct mylite_sql_ast_node *expression,
    size_t *out_index
);
bool mylite_select_reference_qualifiers_match(
    const struct mylite_select_table *table,
    char **parts,
    size_t part_count
);
size_t mylite_select_column_index(const struct mylite_select_table *table, const char *column_name);
char *mylite_select_copy_reference_name(const struct mylite_sql_ast_node *identifier);
char *mylite_select_copy_expression_label(const struct mylite_sql_ast_node *expression);
char *mylite_select_copy_alias(const struct mylite_sql_ast_node *alias);

#endif
