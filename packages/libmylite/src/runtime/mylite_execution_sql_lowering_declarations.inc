int append_select_projection_column_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select *plan,
    size_t column_index
);

static int append_select_sql_query(
    struct mylite_dynamic_string *string,
    const struct planned_select *plan,
    size_t *next_parameter
);

static int append_select_sql_work_item(
    struct mylite_dynamic_string *string,
    struct select_sql_work_stack *stack,
    const struct select_sql_work_item *item,
    size_t *next_parameter
);

static int append_select_sql_work_query(
    struct mylite_dynamic_string *string,
    struct select_sql_work_stack *stack,
    const struct planned_select *plan
);

static int append_select_sql_work_from(
    struct mylite_dynamic_string *string,
    struct select_sql_work_stack *stack,
    const struct planned_select *plan
);

static int append_select_sql_work_source(
    struct mylite_dynamic_string *string,
    struct select_sql_work_stack *stack,
    const struct planned_select_source *source,
    size_t source_index,
    size_t *next_parameter
);

static int select_sql_work_stack_push(
    struct select_sql_work_stack *stack,
    struct select_sql_work_item item
);

static void select_sql_work_stack_deinit(struct select_sql_work_stack *stack);

int build_insert_sql(const struct planned_insert *plan, char **out_sql);
int build_insert_rows_sql(const struct planned_insert *plan, char **out_sql);

int append_insert_column_names(
    struct mylite_dynamic_string *string,
    const struct planned_insert *plan
);

static size_t count_physical_insert_columns(const struct planned_insert *plan);

int append_insert_parameters(struct mylite_dynamic_string *string, size_t column_count);

int append_numbered_parameter(struct mylite_dynamic_string *string, size_t parameter_index);

int append_size_literal(struct mylite_dynamic_string *string, size_t value);

int append_uint64_literal(struct mylite_dynamic_string *string, uint64_t value);

int build_insert_select_temp_table_name(
    const struct mylite_db *database,
    char *destination,
    size_t destination_size
);

int build_insert_select_materialize_sql(
    const struct planned_insert_select *plan,
    const char *temporary_table_name,
    char **out_sql
);

static int build_insert_select_table_materialize_sql(
    const struct planned_insert_select *plan,
    const char *temporary_table_name,
    char **out_sql
);

static int build_insert_select_compound_materialize_sql(
    const struct planned_insert_select *plan,
    const char *temporary_table_name,
    char **out_sql
);

static int append_insert_select_compound_branch_sql(
    struct mylite_dynamic_string *string,
    const struct planned_insert_select_compound_branch *branch,
    size_t branch_index,
    bool alias_columns,
    size_t *next_parameter
);

static int append_insert_select_compound_table_branch_sql(
    struct mylite_dynamic_string *string,
    const struct planned_insert_select_compound_branch *branch,
    size_t branch_index,
    bool alias_columns,
    size_t *next_parameter
);

static int append_insert_select_compound_branch_marker_sql(
    struct mylite_dynamic_string *string,
    size_t branch_index,
    bool alias_columns
);

static int append_insert_select_compound_column_projection_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select *source,
    size_t column_index
);

static int append_insert_select_compound_row_scalar_branch_sql(
    struct mylite_dynamic_string *string,
    const struct planned_insert_select_compound_branch *branch,
    size_t branch_index,
    bool alias_columns,
    size_t *next_parameter
);

static int append_insert_select_compound_row_scalar_projection_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);

bool row_scalar_expression_uses_string_collation(
    const struct planned_row_scalar_expression *expression
);

int build_insert_select_validation_sql(
    const struct planned_insert_select *plan,
    const char *temporary_table_name,
    char **out_sql
);

static int build_insert_select_table_validation_sql(
    const struct planned_insert_select *plan,
    const char *temporary_table_name,
    char **out_sql
);

static int build_insert_select_compound_validation_sql(
    const struct planned_insert_select *plan,
    const char *temporary_table_name,
    char **out_sql
);

static bool insert_select_compound_last_distinct_branch(
    const struct planned_insert_select *plan,
    size_t *out_branch_index
);

static int append_insert_select_compound_distinct_filter_sql(
    struct mylite_dynamic_string *string,
    const struct planned_insert_select *plan,
    const char *temporary_table_name,
    size_t last_distinct_branch
);

static int append_insert_select_compound_distinct_equality_sql(
    struct mylite_dynamic_string *string,
    const struct planned_insert_select *plan,
    size_t column_index
);

static int append_insert_select_compound_qualified_temp_column_name(
    struct mylite_dynamic_string *string,
    const char *alias,
    size_t column_index
);

static int append_insert_select_compound_qualified_branch_column_name(
    struct mylite_dynamic_string *string,
    const char *alias
);

static bool insert_select_compound_output_uses_string_collation(
    const struct planned_insert_select *plan,
    size_t column_index
);

int build_drop_temp_table_sql(const char *temporary_table_name, char **out_sql);

static int append_insert_select_source_projection(
    struct mylite_dynamic_string *string,
    const struct planned_insert_select *plan
);

static int append_insert_select_temp_column_name(
    struct mylite_dynamic_string *string,
    size_t column_index
);

static int append_insert_select_temp_table_name(
    struct mylite_dynamic_string *string,
    const char *temporary_table_name
);

bool find_insert_select_target_position(
    const struct planned_insert_select *plan,
    size_t column_index,
    size_t *out_target_position
);

int build_create_table_select_insert_sql(
    const struct planned_create_table_select *plan,
    const char *physical_name,
    char **out_sql
);

static int append_create_table_select_target_column_names(
    struct mylite_dynamic_string *string,
    const struct planned_create_table_select *plan
);

static int append_create_table_select_source_projection(
    struct mylite_dynamic_string *string,
    const struct planned_create_table_select *plan
);

int build_select_sql(const struct planned_select *plan, char **out_sql);

int build_row_scalar_select_sql(const struct planned_row_scalar_select *plan, char **out_sql);

static int append_row_scalar_select_query_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_select *plan,
    size_t *next_parameter
);

static int append_row_scalar_tableless_filter_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_select *plan,
    size_t *next_parameter
);

int build_select_found_rows_sql(const struct planned_select *plan, char **out_sql);

static int append_select_distinct_found_rows_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select *plan,
    size_t *next_parameter
);

int build_count_sql(const struct planned_count *plan, char **out_sql);

static int append_count_function_sql(
    struct mylite_dynamic_string *string,
    const struct planned_count *plan
);

static int append_count_source_sql(
    struct mylite_dynamic_string *string,
    const struct planned_count *plan,
    size_t *next_parameter
);

static int append_count_derived_source_query_sql(
    struct mylite_dynamic_string *string,
    enum planned_count_source_kind kind,
    const struct planned_select *select_plan,
    const struct planned_row_scalar_select *row_scalar_plan,
    const struct planned_grouped_aggregate *grouped_plan,
    const struct planned_count_derived_compound *compound_plan,
    size_t *next_parameter
);

static int append_count_derived_compound_query_sql(
    struct mylite_dynamic_string *string,
    const struct planned_count_derived_compound *compound_plan,
    size_t *next_parameter
);

static int append_count_derived_compound_branch_sql(
    struct mylite_dynamic_string *string,
    const struct planned_count_derived_compound_branch *branch,
    size_t *next_parameter
);

int build_column_aggregate_sql(const struct planned_column_aggregate_select *plan, char **out_sql);

static int append_column_aggregate_select_list_sql(
    struct mylite_dynamic_string *string,
    const struct planned_column_aggregate_select *plan,
    size_t *next_parameter
);

int append_column_aggregate_select_item_sql(
    struct mylite_dynamic_string *string,
    const struct planned_column_aggregate *item,
    size_t *next_parameter
);

static int append_column_aggregate_count_sql(
    struct mylite_dynamic_string *string,
    const struct planned_column_aggregate *item,
    size_t *next_parameter
);

static int append_aggregate_distinct_sql(struct mylite_dynamic_string *string, bool is_distinct);

static int append_json_aggregate_call_sql(
    struct mylite_dynamic_string *string,
    bool is_object,
    const struct planned_row_scalar_expression *key_expression,
    const struct planned_row_scalar_expression *value_expression,
    size_t *next_parameter
);

static int append_group_concat_call_sql(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *value_column,
    size_t value_source_index,
    const struct planned_select_order *order,
    bool qualify_column_references,
    bool is_distinct,
    bool has_separator,
    const char *collation_name,
    size_t *next_parameter
);

static int append_group_concat_expression_call_sql(
    struct mylite_dynamic_string *string,
    const struct planned_row_scalar_expression *value_expression,
    const struct planned_select_order *order,
    bool is_distinct,
    bool has_separator,
    const char *collation_name,
    size_t *next_parameter
);

static int append_spatial_collect_projection_sql(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column,
    size_t source_index,
    bool qualify_column_references,
    bool is_distinct,
    bool has_wrapper,
    enum mylite_spatial_function_kind wrapper_kind
);

static int append_spatial_collect_aggregate_call_sql(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column,
    size_t source_index,
    bool qualify_column_references,
    bool is_distinct
);

int build_grouped_aggregate_sql(const struct planned_grouped_aggregate *plan, char **out_sql);

int build_grouped_aggregate_rollup_sql(
    const struct planned_grouped_aggregate *plan,
    char **out_sql
);

int build_grouped_aggregate_found_rows_sql(
    const struct planned_grouped_aggregate *plan,
    char **out_sql
);

static int append_grouped_aggregate_query_sql(
    struct mylite_dynamic_string *string,
    const struct planned_grouped_aggregate *plan,
    size_t *next_parameter,
    bool include_order,
    bool include_limit,
    bool include_aliases
);

static int append_grouped_aggregate_rollup_query_sql(
    struct mylite_dynamic_string *string,
    const struct planned_grouped_aggregate *plan,
    size_t *next_parameter
);

static int append_grouped_aggregate_from_sql(
    struct mylite_dynamic_string *string,
    const struct planned_grouped_aggregate *plan,
    size_t *next_parameter
);

static int append_grouped_aggregate_select_list_sql(
    struct mylite_dynamic_string *string,
    const struct planned_grouped_aggregate *plan,
    size_t *next_parameter,
    bool include_aliases
);

static int append_grouped_projection_sql(
    struct mylite_dynamic_string *string,
    const struct planned_grouped_projection *projection,
    bool qualify,
    size_t *next_parameter,
    bool include_aliases
);

static int append_grouped_aggregate_select_item_sql(
    struct mylite_dynamic_string *string,
    const struct planned_grouped_aggregate *plan,
    const struct planned_grouped_aggregate_item *item,
    size_t *next_parameter,
    bool include_aliases
);

static int append_grouped_identifier_alias_sql(
    struct mylite_dynamic_string *string,
    const char *alias
);

static int append_grouped_aggregate_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_grouped_aggregate *plan,
    const struct planned_grouped_aggregate_item *item,
    size_t *next_parameter
);

static int append_grouped_group_key_sql(
    struct mylite_dynamic_string *string,
    const struct planned_grouped_aggregate *plan,
    size_t group_index,
    size_t *next_parameter
);

static int append_grouped_having_sql(
    struct mylite_dynamic_string *string,
    const struct planned_grouped_aggregate *plan,
    size_t *next_parameter
);

static int append_grouped_having_operand_sql(
    struct mylite_dynamic_string *string,
    const struct planned_grouped_aggregate *plan,
    size_t *next_parameter
);

static int append_grouped_having_aggregate_sql(
    struct mylite_dynamic_string *string,
    const struct planned_grouped_aggregate *plan,
    size_t *next_parameter
);

static int append_grouped_bitwise_having_operand_sql(
    struct mylite_dynamic_string *string,
    const struct planned_grouped_aggregate *plan,
    const struct planned_grouped_aggregate_item *item,
    size_t *next_parameter
);

static int append_grouped_avg_having_operand_sql(
    struct mylite_dynamic_string *string,
    const struct planned_grouped_aggregate *plan,
    const struct planned_grouped_aggregate_item *item,
    size_t *next_parameter
);

static int append_grouped_order_sql(
    struct mylite_dynamic_string *string,
    const struct planned_grouped_aggregate *plan,
    size_t *next_parameter
);

static int append_grouped_order_aggregate_sql(
    struct mylite_dynamic_string *string,
    const struct planned_grouped_aggregate *plan,
    size_t aggregate_index,
    size_t *next_parameter
);

static int append_grouped_group_concat_order_column_sql(
    struct mylite_dynamic_string *string,
    const struct planned_grouped_aggregate *plan,
    size_t aggregate_index
);

static int append_grouped_avg_order_key_sql(
    struct mylite_dynamic_string *string,
    const struct planned_grouped_aggregate *plan,
    const struct planned_grouped_aggregate_item *item,
    size_t *next_parameter
);

static int append_grouped_avg_order_argument_sql(
    struct mylite_dynamic_string *string,
    const struct planned_grouped_aggregate *plan,
    const struct planned_grouped_aggregate_item *item,
    size_t *next_parameter
);

int append_select_predicate_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    size_t *next_parameter
);

static int append_select_predicate_sql_without_exists(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    size_t *next_parameter
);

static int append_select_predicate_expression_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    size_t *next_parameter
);

int append_select_predicate_expression_sql_without_exists(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    size_t *next_parameter
);

static int append_select_predicate_expression_work_item(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    struct predicate_sql_work_item item,
    struct predicate_sql_work_item **items,
    size_t *item_count,
    size_t *next_parameter
);

static int append_select_predicate_expression_work_item_without_exists(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    struct predicate_sql_work_item item,
    struct predicate_sql_work_item **items,
    size_t *item_count,
    size_t *next_parameter
);

static int append_select_predicate_logical_operator_sql(
    struct mylite_dynamic_string *string,
    enum mylite_sql_ast_operator operator_kind
);

static int append_select_predicate_expression_node_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    size_t node_index,
    size_t *next_parameter,
    struct predicate_sql_work_item **items,
    size_t *item_count
);

static int append_select_predicate_expression_node_sql_without_exists(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    size_t node_index,
    size_t *next_parameter,
    struct predicate_sql_work_item **items,
    size_t *item_count
);

static int append_select_predicate_logical_node_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate_node *node,
    struct predicate_sql_work_item **items,
    size_t *item_count
);

static int append_select_predicate_not_node_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate_node *node,
    struct predicate_sql_work_item **items,
    size_t *item_count
);

static int append_select_predicate_node_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
);

static int append_select_predicate_node_sql_without_exists(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
);

static int append_select_predicate_non_exists_node_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
);

static int append_select_predicate_non_subquery_node_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
);

static int append_select_predicate_column_term_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
);

static int append_select_predicate_column_term_sql_without_subqueries(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
);

static bool planned_select_predicate_comparison_has_postfix_is(
    const struct planned_select_predicate_node *node
);

static int append_select_comparison_result_is_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate_node *node
);

static int append_select_row_scalar_predicate_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
);

static int append_select_row_scalar_is_boolean_predicate_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
);

static int append_select_row_scalar_regexp_predicate_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
);

static int append_select_row_scalar_like_binary_predicate_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
);

static int append_select_row_scalar_decimal_text_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
);

static int append_select_row_scalar_comparison_rhs_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
);

static int append_select_row_scalar_between_predicate_term_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
);

static int append_row_scalar_decimal_parameter_sql(
    struct mylite_dynamic_string *string,
    size_t *next_parameter
);

static int append_select_scalar_subquery_in_predicate_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
);

static int append_select_scalar_aggregate_subquery_comparison_predicate_sql(
    struct mylite_dynamic_string *string,
    bool qualify_column,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
);

static int append_select_scalar_aggregate_subquery_between_predicate_sql(
    struct mylite_dynamic_string *string,
    bool qualify_column,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
);

static int append_scalar_aggregate_subquery_operand_sql(
    struct mylite_dynamic_string *string,
    const struct planned_scalar_aggregate_subquery *subquery,
    size_t *next_parameter
);

static int append_scalar_aggregate_subquery_subject_sql(
    struct mylite_dynamic_string *string,
    bool qualify_column,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
);

static int append_scalar_aggregate_subquery_value_sql(
    struct mylite_dynamic_string *string,
    const struct planned_scalar_aggregate_subquery *subquery,
    const struct planned_value *value,
    size_t *next_parameter
);

static int append_select_scalar_count_subquery_comparison_predicate_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
);

static int append_select_quantified_subquery_predicate_sql(
    struct mylite_dynamic_string *string,
    bool qualify_column,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
);

static int append_select_quantified_subquery_exists_sql(
    struct mylite_dynamic_string *string,
    bool qualify_column,
    const struct planned_select_predicate_node *node,
    enum quantified_subquery_exists_kind exists_kind,
    size_t *next_parameter
);

static int append_select_quantified_subquery_where_sql(
    struct mylite_dynamic_string *string,
    bool qualify_column,
    const struct planned_select_predicate_node *node,
    enum quantified_subquery_exists_kind exists_kind,
    size_t *next_parameter
);

static int append_select_quantified_subquery_comparison_sql(
    struct mylite_dynamic_string *string,
    bool qualify_column,
    const struct planned_select_predicate_node *node,
    enum quantified_subquery_exists_kind exists_kind
);

static int append_scalar_count_subquery_from_sql(
    struct mylite_dynamic_string *string,
    const struct planned_exists_subquery *subquery,
    size_t *next_parameter
);

static int append_select_exists_predicate_sql(
    struct mylite_dynamic_string *string,
    const struct planned_exists_subquery *subquery,
    size_t *next_parameter
);

static int append_exists_subquery_from_sql(
    struct mylite_dynamic_string *string,
    const struct planned_exists_subquery *subquery
);

static int append_exists_subquery_join_from_sql(
    struct mylite_dynamic_string *string,
    const struct planned_exists_subquery *subquery,
    size_t *next_parameter
);

static int append_exists_subquery_join_condition_sql(
    struct mylite_dynamic_string *string,
    const struct planned_exists_subquery *subquery,
    size_t join_index,
    size_t *next_parameter
);

static int append_descriptor_value_sql_for_source(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column,
    size_t source_index,
    bool qualify
);

static bool planned_select_predicate_rhs_has_explicit_collate(
    const struct planned_select_predicate_node *node
);

static int append_select_predicate_subject_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate_node *node,
    bool qualify,
    bool suppress_default_string_collation
);

static int append_string_integer_value_sql_for_source(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column,
    size_t source_index,
    bool qualify
);

static int append_like_escape_character_argument_sql(
    struct mylite_dynamic_string *string,
    char escape_character
);

static int append_like_escape_clause_sql(
    struct mylite_dynamic_string *string,
    char escape_character
);

static int append_date_midnight_value_sql_for_source(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column,
    size_t source_index,
    bool qualify
);

static int append_descriptor_column_name_sql_for_source(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column,
    size_t source_index,
    bool qualify
);

static int append_time_seconds_value_sql_for_source(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column,
    size_t source_index,
    bool qualify
);

static int append_select_comparison_predicate_term_sql(
    struct mylite_dynamic_string *string,
    bool qualify_column,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
);

static int append_select_like_binary_predicate_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate *predicate,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
);

static int append_select_like_predicate_term_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
);

static int append_select_regexp_predicate_sql(
    struct mylite_dynamic_string *string,
    bool qualify_column,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
);

static int append_select_is_null_predicate_term_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate_node *node
);

static int append_select_is_boolean_predicate_term_sql(
    struct mylite_dynamic_string *string,
    bool qualify_column,
    const struct planned_select_predicate_node *node
);

static int append_is_boolean_rhs_term_sql(
    struct mylite_dynamic_string *string,
    bool qualify_column,
    const struct planned_select_predicate_node *node,
    const char *suffix
);

static int append_select_between_predicate_term_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
);

static int append_select_in_predicate_term_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
);

static int append_select_in_literal_predicate_term_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
);

static int append_select_in_value_list_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_predicate_node *node,
    size_t *next_parameter
);

static int append_select_in_subquery_sql(
    struct mylite_dynamic_string *string,
    const struct planned_in_subquery *subquery,
    size_t *next_parameter
);

static int append_in_subquery_from_sql(
    struct mylite_dynamic_string *string,
    const struct planned_in_subquery *subquery,
    size_t *next_parameter
);

int append_predicate_sql_work_node(
    struct predicate_sql_work_item **items,
    size_t *item_count,
    size_t node_index
);

static int append_predicate_sql_work_operator(
    struct predicate_sql_work_item **items,
    size_t *item_count,
    enum mylite_sql_ast_operator operator_kind
);

static int append_predicate_sql_work_close(
    struct predicate_sql_work_item **items,
    size_t *item_count
);

static int append_predicate_sql_work_text(
    struct predicate_sql_work_item **items,
    size_t *item_count,
    const char *text
);

int append_predicate_sql_work_item(
    struct predicate_sql_work_item **items,
    size_t *item_count,
    struct predicate_sql_work_item item
);

int append_select_order_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_order *order,
    size_t *next_parameter
);

static int append_select_order_legacy_column_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_order *order
);

static int append_select_order_item_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_order *order,
    size_t item_index,
    size_t *next_parameter
);

int append_select_order_direction_sql(
    struct mylite_dynamic_string *string,
    enum planned_select_order_direction direction
);

int append_select_source_alias(struct mylite_dynamic_string *string, size_t source_index);

static bool planned_select_qualifies_source_references(const struct planned_select *plan);

int append_select_from_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select *plan,
    size_t *next_parameter
);

static int append_select_source_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_source *source,
    size_t source_index,
    size_t *next_parameter
);

static int append_select_join_condition_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_join_condition *condition,
    size_t *next_parameter
);

int append_select_limit_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_limit *limit
);

static int append_limit_integer_sql(struct mylite_dynamic_string *string, int64_t value);

int build_delete_sql(const struct planned_delete *plan, char **out_sql);

static int append_delete_rowid_limited_sql(
    struct mylite_dynamic_string *string,
    const struct planned_delete *plan,
    size_t *next_parameter
);

static int append_joined_delete_rowid_filter_sql(
    struct mylite_dynamic_string *string,
    const struct planned_delete *plan,
    size_t *next_parameter
);

int append_joined_delete_rowid_subquery_sql(
    struct mylite_dynamic_string *string,
    const struct planned_delete *plan,
    size_t *next_parameter
);

static int append_joined_delete_rowid_select_sql(
    struct mylite_dynamic_string *string,
    const struct planned_delete *plan,
    size_t *next_parameter,
    size_t target_source_index
);

static int append_joined_delete_target_rowid_sql(
    struct mylite_dynamic_string *string,
    const struct planned_delete *plan,
    size_t target_source_index
);

static int append_joined_delete_from_sql(
    struct mylite_dynamic_string *string,
    const struct planned_delete *plan,
    size_t *next_parameter
);

int build_update_sql(const struct planned_update *plan, char **out_sql);

static int append_single_update_target_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan
);

static bool planned_update_needs_target_alias(const struct planned_update *plan);

static int append_update_assignment_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
);

static int append_update_date_interval_assignment_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
);

static int append_update_multiple_assignments_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
);

static int append_update_auto_update_columns_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
);

bool planned_update_column_has_auto_update(
    const struct planned_update *plan,
    const struct mylite_catalog_column_descriptor *column
);

size_t planned_update_auto_update_column_count(const struct planned_update *plan);

int append_single_update_matched_from_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan
);

int append_joined_update_matched_target_filter_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
);

bool planned_update_has_row_filter(const struct planned_update *plan);

int append_update_row_filter_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
);

int append_update_rowid_limited_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
);

static int append_single_update_rowid_source_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan
);

int append_joined_update_rowid_filter_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
);

int append_joined_update_rowid_subquery_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
);

int append_joined_update_target_rowid_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan
);

int append_joined_update_from_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
);

int append_update_changed_condition_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
);

static int append_update_multiple_changed_condition_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
);

static int append_update_column_copy_changed_condition_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan
);

static int append_update_row_scalar_changed_condition_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
);

int append_update_assignment_row_scalar_changed_condition_sql(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column,
    const struct planned_row_scalar_expression *expression,
    size_t *next_parameter
);

static int append_update_assignment_changed_condition_sql(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column,
    const struct planned_value *value,
    size_t *next_parameter
);

static int append_update_arithmetic_changed_condition_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
);

static int append_update_date_interval_changed_condition_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
);

static int append_update_date_interval_function_sql(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column,
    size_t first_parameter
);
