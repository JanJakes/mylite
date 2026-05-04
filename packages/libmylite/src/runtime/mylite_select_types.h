#ifndef MYLITE_RUNTIME_MYLITE_SELECT_TYPES_H
#define MYLITE_RUNTIME_MYLITE_SELECT_TYPES_H

#include <mylite/mylite.h>

#include "mylite_expression.h"
#include "mylite_field_descriptor.h"
#include "sql/mylite_ast.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct mylite_select_column {
    char *name;
    struct mylite_field_descriptor descriptor;
    bool visible;
};

struct mylite_select_table {
    char *schema_name;
    char *table_name;
    char *alias;
    char *physical_name;
    struct mylite_select_column *columns;
    size_t first_column_index;
    size_t column_count;
};

struct mylite_select_join_predicate {
    const struct mylite_sql_ast_node *expression;
    size_t first_table;
    size_t table_count;
};

struct mylite_select_table_range {
    size_t first_table;
    size_t table_count;
};

struct mylite_select_join_step {
    enum mylite_sql_ast_join_type join_type;
    struct mylite_select_table_range left_range;
    struct mylite_select_table_range right_range;
    struct mylite_select_table_range joined_range;
};

struct mylite_select_join_stack_entry {
    const struct mylite_sql_ast_node *right;
    const struct mylite_sql_ast_node *condition;
    enum mylite_sql_ast_join_type join_type;
};

struct mylite_select_join_using_column {
    char *name;
    size_t left_column_index;
    size_t right_column_index;
    size_t coalesced_column_index;
    size_t first_table;
    size_t table_count;
};

struct mylite_select_join_using_request {
    char **names;
    size_t name_count;
    size_t left_first_table;
    size_t left_table_count;
    size_t right_first_table;
    size_t right_table_count;
    enum mylite_sql_ast_join_type join_type;
};

struct mylite_select_column_sequence {
    size_t *column_indexes;
    size_t column_count;
};

enum mylite_select_output_kind {
    MYLITE_SELECT_OUTPUT_COLUMN = 0,
    MYLITE_SELECT_OUTPUT_EXPRESSION = 1,
};

struct mylite_select_output_column {
    enum mylite_select_output_kind kind;
    size_t column_index;
    const struct mylite_sql_ast_node *expression;
    char *label;
    bool referenced_by_order;
};

enum mylite_select_order_key_kind {
    MYLITE_SELECT_ORDER_KEY_EXPRESSION = 0,
    MYLITE_SELECT_ORDER_KEY_OUTPUT = 1,
};

struct mylite_select_order_key {
    enum mylite_select_order_key_kind kind;
    enum mylite_sql_ast_key_part_order direction;
    size_t output_index;
    const struct mylite_sql_ast_node *expression;
};

enum mylite_select_group_key_kind {
    MYLITE_SELECT_GROUP_KEY_EXPRESSION = 0,
    MYLITE_SELECT_GROUP_KEY_OUTPUT = 1,
};

enum mylite_select_grouping_reference_policy {
    MYLITE_SELECT_GROUPING_REFERENCE_SELECT = 0,
    MYLITE_SELECT_GROUPING_REFERENCE_HAVING = 1,
    MYLITE_SELECT_GROUPING_REFERENCE_ORDER = 2,
};

struct mylite_select_group_key {
    enum mylite_select_group_key_kind kind;
    enum mylite_sql_ast_key_part_order direction;
    size_t output_index;
    const struct mylite_sql_ast_node *expression;
};

struct mylite_select_aggregate_binding {
    const struct mylite_sql_ast_node *call;
    const struct mylite_sql_ast_node *argument;
    enum mylite_sql_ast_aggregate_kind kind;
    enum mylite_sql_ast_aggregate_argument argument_kind;
    struct mylite_field_descriptor descriptor;
    struct mylite_field_descriptor *argument_descriptors;
    size_t argument_descriptor_count;
};

struct mylite_select_limit {
    uint64_t offset;
    uint64_t row_count;
    bool has_limit;
};

struct mylite_select_limit_position {
    uint64_t matched_row;
    size_t kept_count;
};

struct mylite_select_distinct_order_column_error_context {
    size_t order_position;
    size_t column_index;
};

struct mylite_select_distinct_order_validation_frame {
    const struct mylite_sql_ast_node *expression;
    bool alias_first;
};

struct mylite_select_distinct_order_validation_stack {
    struct mylite_select_distinct_order_validation_frame *frames;
    size_t count;
    size_t capacity;
};

struct mylite_select_plan {
    struct mylite_select_table table;
    struct mylite_select_table *tables;
    size_t table_count;
    size_t column_count;
    struct mylite_select_table_range *from_ranges;
    size_t from_range_count;
    struct mylite_select_join_step *join_steps;
    size_t join_step_count;
    struct mylite_select_output_column *outputs;
    size_t output_count;
    struct mylite_select_order_key *order_keys;
    size_t order_key_count;
    struct mylite_select_group_key *group_keys;
    size_t group_key_count;
    struct mylite_select_aggregate_binding *aggregate_bindings;
    size_t aggregate_binding_count;
    struct mylite_select_join_predicate *join_predicates;
    size_t join_predicate_count;
    struct mylite_select_join_using_column *using_columns;
    size_t using_column_count;
    struct mylite_select_join_using_request *using_requests;
    size_t using_request_count;
    const struct mylite_sql_ast_node *having_expression;
    struct mylite_select_limit limit;
    enum mylite_sql_ast_select_duplicate_mode duplicate_mode;
    bool has_group_by;
    bool has_having;
    bool has_aggregate;
};

struct mylite_select_clause_nodes {
    const struct mylite_sql_ast_node *where;
    const struct mylite_sql_ast_node *group_by;
    const struct mylite_sql_ast_node *having;
    const struct mylite_sql_ast_node *order_by;
    const struct mylite_sql_ast_node *limit;
};

struct mylite_case_descriptor_aggregate {
    struct mylite_field_descriptor descriptor;
    bool has_result;
    bool has_non_null_result;
    bool has_text_result;
    bool has_decimal_result;
    bool has_double_result;
    bool nullable;
};

struct mylite_scalar_result {
    struct mylite_expression_value *values;
    char **texts;
    const struct mylite_sql_ast_node **expressions;
    struct mylite_expression_warnings warnings;
    size_t value_count;
    bool has_row;
    bool row_available;
};

struct mylite_cached_expression_value {
    const struct mylite_sql_ast_node *expression;
    struct mylite_expression_value value;
    bool evaluated;
    int status;
};

struct mylite_table_select_row {
    struct mylite_expression_value *values;
    struct mylite_expression_value *output_values;
    struct mylite_expression_value *order_values;
    struct mylite_expression_value *aggregate_values;
    size_t *source_row_indexes;
    size_t value_count;
    size_t output_value_count;
    size_t order_value_count;
    size_t aggregate_value_count;
    size_t source_row_index_count;
};

struct mylite_unordered_table_select_append_state {
    uint64_t matched_row;
    bool stop;
};

struct mylite_table_select_table_rowset {
    struct mylite_table_select_row *rows;
    size_t row_count;
};

struct mylite_select_join_match_tracking {
    bool left_matched;
    bool *right_matched;
};

struct mylite_select_join_row_pair {
    const struct mylite_table_select_row *left;
    const struct mylite_table_select_row *right;
    size_t right_index;
};

struct mylite_table_select_join_scan_frame {
    size_t row_index;
    bool copied;
};

struct mylite_table_select_join_scan_state {
    struct mylite_table_select_join_scan_frame *frames;
    struct mylite_table_select_row *row;
    size_t table_count;
    size_t table_index;
};

struct mylite_table_select_join_condition_cache_entry {
    size_t *row_indexes;
    size_t first_table;
    size_t table_count;
    bool matches;
};

struct mylite_table_select_join_condition_cache {
    struct mylite_table_select_join_condition_cache_entry *entries;
    size_t entry_count;
};

struct mylite_table_select_join_condition_cache_lookup {
    bool found;
    bool matches;
};

struct mylite_table_select_join_materialize_state {
    struct mylite_table_select_table_rowset *rowsets;
    struct mylite_table_select_join_condition_cache condition_cache;
    struct mylite_table_select_group *groups;
    size_t group_count;
    uint64_t matched_row;
    bool stop;
};

struct mylite_select_aggregate_state {
    struct mylite_expression_value value;
    struct mylite_count_distinct_tuple *distinct_tuples;
    uint64_t count;
    uint64_t non_null_count;
    double sum;
    size_t distinct_tuple_count;
    bool integral_sum;
    bool unsigned_sum;
    bool has_value;
};

struct mylite_count_distinct_tuple {
    struct mylite_expression_value *values;
    size_t value_count;
};

struct mylite_aggregate_numeric_value {
    double value;
    bool integral;
    bool unsigned_value;
};

struct mylite_table_select_group {
    struct mylite_table_select_row representative;
    struct mylite_expression_value *group_values;
    struct mylite_select_aggregate_state *aggregate_states;
    size_t group_value_count;
    size_t aggregate_state_count;
    bool has_representative;
};

struct mylite_table_select_result {
    struct mylite_table_select_row *rows;
    struct mylite_expression_value *current_values;
    char **current_texts;
    size_t row_count;
    size_t next_row;
    size_t current_value_count;
    bool materialized;
    bool has_current_row;
};

struct mylite_table_select_expression_context {
    mylite_stmt *stmt;
    const struct mylite_table_select_row *row;
    bool order_resolution;
    bool having_resolution;
};

struct mylite_union_plan {
    mylite_stmt **operands;
    enum mylite_sql_ast_set_duplicate_mode *operators;
    size_t operand_count;
};

struct mylite_union_expression_context {
    mylite_stmt *stmt;
    const struct mylite_table_select_row *row;
};

struct mylite_in_subquery_scan_context {
    mylite_stmt *outer_stmt;
    mylite_stmt *subquery_stmt;
    const struct mylite_expression_value *left;
    struct mylite_expression_warnings *warnings;
};

struct mylite_in_subquery_scan_state {
    bool has_row;
    bool matched;
    bool saw_unknown;
};

struct mylite_row_expression_values {
    struct mylite_expression_value *items;
    size_t count;
};

struct mylite_row_in_subquery_scan_context {
    mylite_stmt *outer_stmt;
    mylite_stmt *subquery_stmt;
    const struct mylite_row_expression_values *left;
    struct mylite_expression_warnings *warnings;
    bool left_has_null;
};

struct mylite_row_in_subquery_scan_state {
    bool has_row;
    bool matched;
    bool saw_unknown;
};

struct mylite_row_order_comparison {
    enum mylite_sql_ast_operator operator_kind;
    int comparison;
};

struct mylite_quantified_subquery_scan_context {
    mylite_stmt *outer_stmt;
    mylite_stmt *subquery_stmt;
    const struct mylite_expression_value *left;
    struct mylite_expression_warnings *warnings;
    enum mylite_sql_ast_operator operator_kind;
    enum mylite_sql_ast_subquery_quantifier quantifier;
};

struct mylite_quantified_subquery_scan_state {
    bool has_row;
    bool decided;
    bool result;
    bool saw_unknown;
};

struct mylite_expression_collation_context {
    const struct mylite_select_plan *plan;
    const struct mylite_select_table *table;
};

#endif
