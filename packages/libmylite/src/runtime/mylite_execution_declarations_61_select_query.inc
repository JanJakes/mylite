static int plan_select(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool allow_order_by_field,
    struct planned_select *out_plan
);
static const struct mylite_sql_ast_node *from_table_alias_node(
    const struct mylite_sql_ast_node *from_table
);
static const struct mylite_sql_ast_node *from_table_index_hint_list_node(
    const struct mylite_sql_ast_node *from_table
);
static int validate_select_index_hints(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_table,
    const struct mylite_catalog_table_descriptor *table
);
static int validate_select_index_hint(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *hint,
    const struct mylite_catalog_table_descriptor *table,
    bool *has_use,
    bool *has_force
);
static const struct mylite_sql_ast_node *index_hint_name_list_node(
    const struct mylite_sql_ast_node *hint
);
static int validate_select_index_hint_names(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *name_list,
    const struct mylite_catalog_table_descriptor *table
);
static int select_index_hint_name_exists(
    struct mylite_db *database,
    int64_t table_id,
    const char *index_name,
    bool *out_exists
);
static int match_select_index_hint_name(
    const struct mylite_catalog_index_descriptor *index,
    void *user_data
);
static int collect_select_optional_clauses(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct select_optional_clauses *out_clauses
);
static int plan_select_projection(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select *out_plan
);
static int validate_select_distinct_order(
    struct mylite_db *database,
    const struct planned_select *plan
);
static bool select_distinct_order_item_is_selected(
    const struct planned_select *plan,
    const struct planned_select_order_item *item
);
static int validate_select_distinct_columns(
    struct mylite_db *database,
    const struct planned_select *plan
);
static bool select_distinct_column_is_supported(
    const struct mylite_catalog_column_descriptor *column
);
static void copy_plan_source_alias_if_present(
    struct planned_select *plan,
    const struct select_source_context *context
);
static int plan_joined_select(
    struct mylite_db *database,
    const struct joined_select_ast *joined_select,
    struct planned_select *out_plan
);
static int reject_joined_select_order_ambiguity_for_literal_projection(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct mylite_sql_ast_node *order_clause,
    const struct select_source_context *source_context
);
static int reject_ambiguous_joined_order_references(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_clause,
    const struct select_source_context *source_context
);
static int reject_ambiguous_joined_order_reference(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    const struct select_source_context *source_context
);
static int reject_unsupported_joined_select_having(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct mylite_sql_ast_node *having_clause
);
static int reject_joined_select_having_reference_error(
    struct mylite_db *database,
    struct joined_select_having_reference_request request
);
static const struct mylite_sql_ast_node *joined_select_having_reference_node(
    const struct mylite_sql_ast_node *having_node
);
static int count_select_list_output_name_matches(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const char *name,
    size_t *out_match_count
);
static int select_item_output_name_matches(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_item,
    const char *name,
    bool *out_matches
);
static size_t joined_source_unqualified_column_match_count(
    const struct select_source_context *source_context,
    const char *column_name
);
static bool select_list_has_only_literal_projection_items(
    const struct mylite_sql_ast_node *select_list
);
static bool select_item_is_literal_projection(const struct mylite_sql_ast_node *select_item);
static int prepare_joined_select_plan(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct planned_select *out_plan,
    struct joined_select_temp_nodes *temp_nodes
);
static void joined_select_temp_nodes_deinit(struct joined_select_temp_nodes *temp_nodes);
static size_t joined_select_source_count(const struct mylite_sql_ast_node *from_clause);
static int collect_joined_select_parts(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct planned_select *out_plan,
    const struct joined_select_temp_nodes *temp_nodes
);
static int plan_joined_select_sources(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *const *source_nodes,
    struct planned_select *out_plan
);
static int reject_unsupported_multi_source_join_edges(
    struct mylite_db *database,
    const struct planned_select *plan
);
static int plan_joined_select_conditions(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *const *join_condition_nodes,
    size_t join_condition_node_count,
    struct planned_select *out_plan
);
static int plan_joined_select_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *source_node,
    struct planned_select_source *out_source
);
static int plan_joined_select_table_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *source_node,
    struct planned_select_source *out_source
);
static int plan_joined_select_derived_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *source_node,
    struct planned_select_source *out_source
);
static int plan_derived_select_source_child(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_statement,
    struct planned_select_source *out_source
);
static int plan_derived_compound_select_source_child(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *compound_statement,
    struct planned_select_source *out_source
);
static int validate_derived_compound_select_terms(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *terms
);
static int plan_derived_compound_select_branch(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *branch,
    struct planned_select *out_branch
);
static int validate_derived_compound_select_branch_shape(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *branch
);
static int copy_derived_select_source_columns(
    struct mylite_db *database,
    const struct planned_select *derived_plan,
    struct planned_select_source *out_source
);
static int copy_derived_row_scalar_select_source_columns(
    struct mylite_db *database,
    const struct planned_row_scalar_select *derived_plan,
    struct planned_select_source *out_source
);
static int make_derived_row_scalar_source_column(
    struct mylite_db *database,
    const struct planned_row_scalar_select_item *item,
    size_t output_index,
    struct mylite_catalog_column_descriptor *out_column
);
static int copy_derived_compound_select_source_columns(
    struct mylite_db *database,
    const struct planned_compound_select *compound_plan,
    struct planned_select_source *out_source
);
static int validate_derived_grouped_aggregate_output_columns(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *derived_plan
);
static int copy_derived_grouped_aggregate_source_columns(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *derived_plan,
    struct planned_select_source *out_source
);
static int copy_derived_grouped_projection_source_column(
    struct mylite_db *database,
    const struct planned_grouped_projection *projection,
    size_t output_index,
    struct mylite_catalog_column_descriptor *out_column
);
static int make_derived_grouped_aggregate_source_column(
    struct mylite_db *database,
    const struct planned_grouped_aggregate_item *item,
    size_t output_index,
    struct mylite_catalog_column_descriptor *out_column
);
static int copy_derived_grouped_output_alias(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *alias,
    char *destination,
    size_t destination_size
);
static int validate_derived_select_output_columns(
    struct mylite_db *database,
    const struct planned_select *derived_plan
);
static int init_join_select_source_context(
    const struct planned_select_source *sources,
    size_t source_count,
    struct select_source_context *out_context
);
static int reject_duplicate_select_source_reference(
    struct mylite_db *database,
    const struct planned_select_source *sources,
    size_t source_count
);
static const char *planned_select_source_reference_name(const struct planned_select_source *source);
static int plan_joined_select_condition(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *condition_node,
    enum mylite_sql_ast_join_kind join_kind,
    const struct select_source_context *source_context,
    struct planned_select_join_condition *out_condition
);
static int plan_joined_select_condition_with_extra_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *condition_node,
    const struct select_source_context *source_context,
    struct planned_select_join_condition *out_condition
);
static int plan_joined_select_primary_condition(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *condition_node,
    const struct select_source_context *source_context,
    struct planned_select_join_condition *out_condition
);
static int plan_joined_select_extra_on_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    struct planned_select_join_condition *out_condition
);
static bool joined_select_on_node_is_and(const struct mylite_sql_ast_node *node);
static bool joined_select_on_node_is_descriptor_equality(const struct mylite_sql_ast_node *node);
static bool joined_select_on_node_is_primary_condition(const struct mylite_sql_ast_node *node);
static bool join_condition_columns_are_compatible(
    const struct mylite_catalog_column_descriptor *left,
    const struct mylite_catalog_column_descriptor *right
);
static bool join_condition_column_is_integer_family(
    const struct mylite_catalog_column_descriptor *column
);
static int append_select_column_from_source(
    struct mylite_db *database,
    struct planned_select *plan,
    const struct mylite_catalog_column_descriptor *column,
    size_t source_index,
    const struct mylite_sql_ast_node *alias
);
static void planned_select_order_deinit(struct planned_select_order *order);
static void planned_compound_select_deinit(struct planned_compound_select *plan);
static void planned_select_source_deinit(struct planned_select_source *source);
static void planned_select_source_deinit_shallow(struct planned_select_source *source);
static void planned_select_join_condition_deinit(struct planned_select_join_condition *condition);
static void planned_select_deinit(struct planned_select *plan);
static void planned_select_deinit_shallow(struct planned_select *plan);
static struct planned_select_source *planned_select_find_deepest_derived_source(
    struct planned_select *plan
);
static int execute_select_from_plan(
    struct mylite_db *database,
    const struct planned_select *plan,
    mylite_result **out_result
);
static int set_select_found_row_count(
    struct mylite_db *database,
    const struct planned_select *plan,
    mylite_result *result
);
static int found_row_count_for_select_limit_envelope(
    struct mylite_db *database,
    const struct planned_select *plan,
    size_t visible_row_count,
    uint64_t *out_found_row_count
);
static int read_select_found_row_count(
    struct mylite_db *database,
    const struct planned_select *plan,
    int64_t *out_count
);
static bool select_statement_is_row_scalar_projection_attempt(
    const struct mylite_sql_ast_node *statement
);
static bool select_statement_optional_clauses_contain_row_scalar_function(
    const struct mylite_sql_ast_node *statement
);
static bool select_optional_clause_contains_row_scalar_function(
    const struct mylite_sql_ast_node *clause
);
static bool select_where_clause_contains_row_scalar_context_attempt(
    const struct mylite_sql_ast_node *predicate
);
static bool select_order_key_is_supported_row_scalar_expression(
    const struct mylite_sql_ast_node *order_key
);
static int execute_row_scalar_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result
);
static int plan_row_scalar_select(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool allow_order_by_field,
    struct planned_row_scalar_select *out_plan
);
static int plan_count_having_select(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_count_having_select *out_plan
);
static int collect_count_having_select_clauses(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct select_optional_clauses *out_clauses
);
static int plan_count_having_select_projection(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct select_optional_clauses *clauses,
    const struct select_source_context *source_context,
    struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_count_having_select *out_plan
);
static bool select_list_is_descriptor_projection(const struct mylite_sql_ast_node *select_list);
static int plan_count_having_descriptor_projection(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct select_optional_clauses *clauses,
    const struct select_source_context *source_context,
    struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_count_having_select *out_plan
);
static int plan_count_having_row_scalar_projection(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct select_optional_clauses *clauses,
    const struct select_source_context *source_context,
    struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_count_having_select *out_plan
);
static int plan_count_having_clause(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *having_clause,
    enum mylite_sql_ast_operator *out_operator_kind,
    struct planned_value *out_value
);
static const struct mylite_sql_ast_node *count_having_comparison_node(
    const struct mylite_sql_ast_node *having_clause
);
static bool count_having_comparison_uses_count_star(const struct mylite_sql_ast_node *comparison);
static void planned_count_having_select_deinit(struct planned_count_having_select *plan);
static int collect_row_scalar_select_clauses(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *optional_clause,
    struct row_scalar_select_clauses *out_clauses
);
static int plan_row_scalar_select_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct planned_row_scalar_select *out_plan,
    struct select_source_context *out_source_context,
    struct mylite_catalog_column_descriptor **out_table_columns,
    size_t *out_table_column_count
);
static int plan_row_scalar_select_join_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct planned_row_scalar_select *out_plan,
    struct select_source_context *out_source_context
);
static int plan_row_scalar_select_derived_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct planned_row_scalar_select *out_plan,
    struct select_source_context *out_source_context
);
static int plan_row_scalar_select_row_envelope(
    struct mylite_db *database,
    const struct row_scalar_select_clauses *clauses,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    bool allow_order_by_field,
    struct planned_row_scalar_select *out_plan
);
static int plan_row_scalar_select_order(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_clause,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    bool allow_order_by_field,
    struct planned_row_scalar_select *out_plan
);
static int plan_row_scalar_select_order_ordinals(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_clause,
    size_t selected_item_count,
    struct planned_select_order *out_order,
    bool *out_handled
);
static int plan_row_scalar_select_order_ordinal_item_and_append(
    struct mylite_db *database,
    struct select_order_ast_item_nodes item_nodes,
    size_t selected_item_count,
    struct planned_select_order *out_order
);
static int parse_select_order_ordinal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    size_t selected_item_count,
    size_t *out_ordinal
);
static int plan_row_scalar_select_order_alias(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_clause,
    const struct planned_row_scalar_select *select_plan,
    struct planned_select_order *out_order,
    bool *out_handled
);
static int plan_row_scalar_select_order_alias_item(
    struct mylite_db *database,
    struct select_order_ast_item_nodes item_nodes,
    const struct planned_row_scalar_select *select_plan,
    struct planned_select_order *out_order,
    bool *out_handled
);
static int plan_row_scalar_select_tableless_clauses(
    struct mylite_db *database,
    const struct row_scalar_select_clauses *clauses,
    struct planned_row_scalar_select *out_plan
);
static int plan_row_scalar_select_tableless_order(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_clause,
    struct planned_row_scalar_select *out_plan
);
static int plan_row_scalar_select_tableless_order_item(
    struct mylite_db *database,
    struct select_order_ast_item_nodes item_nodes,
    struct planned_row_scalar_select *out_plan
);
static int plan_row_scalar_select_tableless_order_alias_item(
    struct mylite_db *database,
    struct select_order_ast_item_nodes item_nodes,
    const struct planned_row_scalar_select *select_plan,
    struct planned_select_order *out_order,
    bool *out_handled
);
static int plan_row_scalar_select_tableless_order_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key
);
static int plan_row_scalar_select_tableless_filter(
    struct mylite_db *database,
    const struct row_scalar_select_clauses *clauses,
    struct planned_row_scalar_select *out_plan
);
static int plan_row_scalar_select_exists_filter(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    struct planned_row_scalar_select *out_plan
);
static bool row_scalar_select_where_is_exists_filter(const struct mylite_sql_ast_node *where_clause
);
static int plan_row_scalar_select_scalar_filter(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    struct planned_row_scalar_select *out_plan
);
static bool planned_row_scalar_tableless_filter_is_supported(
    const struct planned_select_predicate *predicate
);
static bool planned_row_scalar_tableless_filter_node_is_supported(
    const struct planned_select_predicate_node *node
);
static bool planned_row_scalar_tableless_filter_expression_is_supported(
    const struct planned_row_scalar_expression *expression
);
static bool planned_row_scalar_tableless_filter_expression_kind_is_supported(
    enum planned_row_scalar_expression_kind kind
);
static void planned_row_scalar_select_deinit(struct planned_row_scalar_select *plan);
static int execute_row_scalar_select_from_plan(
    struct mylite_db *database,
    const struct planned_row_scalar_select *plan,
    mylite_result **out_result
);
static int execute_count_having_select_from_plan(
    struct mylite_db *database,
    const struct planned_count_having_select *plan,
    bool apply_sql_select_limit,
    mylite_result **out_result
);
static int append_count_having_select_result_columns(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_count_having_select *plan,
    const struct result_column_metadata_context *metadata_context
);
static int append_count_having_row_scalar_result_columns(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_count_having_select *plan,
    const struct result_column_metadata_context *metadata_context
);
static const char *count_having_row_scalar_literal_label(
    const struct planned_row_scalar_select_item *item
);
static int row_scalar_select_step_error(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int sqlite_rc
);
static bool map_numeric_row_scalar_step_error(struct mylite_db *database, const char *message);
static bool map_json_extract_row_scalar_step_error(struct mylite_db *database, const char *message);
static bool map_json_mutation_row_scalar_step_error(
    struct mylite_db *database,
    const char *message
);
static bool map_json_search_row_scalar_step_error(struct mylite_db *database, const char *message);
static bool map_json_type_row_scalar_step_error(struct mylite_db *database, const char *message);
static bool map_json_constructor_row_scalar_step_error(
    struct mylite_db *database,
    const char *message
);
static int append_row_scalar_result_columns(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_row_scalar_select *plan,
    const struct result_column_metadata_context *metadata_context
);
static int append_row_scalar_result_column(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_row_scalar_select *plan,
    const struct result_column_metadata_context *metadata_context,
    const struct planned_row_scalar_select_item *item
);
static int make_row_scalar_result_column_descriptor(
    struct mylite_db *database,
    const struct planned_row_scalar_select *plan,
    const struct result_column_metadata_context *metadata_context,
    const struct planned_row_scalar_select_item *item,
    const char *label,
    struct mylite_result_column_descriptor *out_descriptor
);
static int make_row_scalar_column_result_column_descriptor(
    struct mylite_db *database,
    const struct planned_row_scalar_select *plan,
    const struct result_column_metadata_context *metadata_context,
    const struct planned_row_scalar_select_item *item,
    const char *label,
    struct mylite_result_column_descriptor *out_descriptor
);
static int populate_row_scalar_expression_result_column_descriptor(
    struct mylite_db *database,
    const struct planned_row_scalar_expression *expression,
    struct mylite_result_column_descriptor *descriptor
);
static int populate_row_scalar_source_result_column_descriptor(
    struct mylite_db *database,
    const struct planned_row_scalar_select_item *item,
    struct mylite_result_column_descriptor *descriptor
);
static int populate_row_scalar_special_source_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_result_column_descriptor *descriptor
);
static int populate_row_scalar_integer_expression_result_column_descriptor(
    const struct mylite_sql_ast_node *expression,
    struct mylite_result_column_descriptor *descriptor
);
static int populate_row_scalar_abs_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_result_column_descriptor *descriptor
);
static int populate_row_scalar_scalar_subquery_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_result_column_descriptor *descriptor
);
static int populate_row_scalar_concat_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_result_column_descriptor *descriptor
);
static void populate_row_scalar_year_result_column_descriptor(
    struct mylite_result_column_descriptor *descriptor
);
static int populate_row_scalar_conversion_source_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_result_column_descriptor *descriptor
);
static int populate_row_scalar_coalesce_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_result_column_descriptor *descriptor
);
static int populate_row_scalar_searched_case_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_result_column_descriptor *descriptor
);
static int row_scalar_source_string_display_length(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool *out_matched,
    uint64_t *out_display_length
);
static int row_scalar_source_binary_display_length(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    bool *out_matched,
    uint64_t *out_display_length
);
static int scalar_binary_literal_byte_count(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    size_t *out_byte_count
);
static bool row_scalar_source_expression_is_string_literal(
    const struct mylite_sql_ast_node *expression
);
static int row_scalar_integer_expression_token_width(
    const struct mylite_sql_ast_node *expression,
    bool *out_matched,
    uint64_t *out_width
);
static int row_scalar_integer_width_enter_frame(
    struct row_scalar_integer_width_stack *stack,
    const struct mylite_sql_ast_node *expression
);
static int row_scalar_integer_width_finish_unary_frame(struct row_scalar_integer_width_stack *stack
);
static int row_scalar_integer_width_finish_binary_frame(struct row_scalar_integer_width_stack *stack
);
static int row_scalar_integer_width_push_frame(
    struct row_scalar_integer_width_stack *stack,
    struct row_scalar_integer_width_frame frame
);
static int row_scalar_integer_width_push_result(
    struct row_scalar_integer_width_stack *stack,
    struct row_scalar_integer_width_result result
);
static int row_scalar_integer_width_pop_result(
    struct row_scalar_integer_width_stack *stack,
    struct row_scalar_integer_width_result *out_result
);
static void row_scalar_integer_width_stack_deinit(struct row_scalar_integer_width_stack *stack);
static bool row_scalar_expression_is_date_target_conversion(
    const struct mylite_sql_ast_node *expression
);
static bool row_scalar_metadata_byte_is_identifier(char byte);
static bool row_scalar_metadata_text_is_integer(const char *text, size_t text_length);
static bool row_scalar_descriptor_has_known_type(
    const struct mylite_result_column_descriptor *descriptor
);
static int populate_window_function_result_column_descriptor(
    struct mylite_db *database,
    const struct planned_row_scalar_expression *expression,
    struct mylite_result_column_descriptor *descriptor
);
static int populate_window_value_function_result_column_descriptor(
    struct mylite_db *database,
    const struct planned_row_scalar_expression *expression,
    struct mylite_result_column_descriptor *descriptor
);
static int populate_window_value_column_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct mylite_result_column_descriptor *descriptor
);
static int populate_window_value_literal_result_column_descriptor(
    struct mylite_db *database,
    const struct planned_value *value,
    struct mylite_result_column_descriptor *descriptor
);
static int populate_row_scalar_conversion_result_column_descriptor(
    const struct mylite_db *database,
    const struct planned_row_scalar_expression *expression,
    struct mylite_result_column_descriptor *descriptor
);
static void populate_row_scalar_digest_result_column_descriptor(
    const struct mylite_db *database,
    const struct planned_row_scalar_expression *expression,
    struct mylite_result_column_descriptor *descriptor
);
static int populate_row_scalar_compress_result_column_descriptor(
    struct mylite_db *database,
    const struct planned_row_scalar_expression *expression,
    struct mylite_result_column_descriptor *descriptor
);
static int populate_row_scalar_date_interval_second_result_column_descriptor(
    struct mylite_db *database,
    const struct planned_row_scalar_expression *expression,
    struct mylite_result_column_descriptor *descriptor
);
static int copy_row_scalar_result_column_name(
    struct mylite_db *database,
    const struct planned_row_scalar_select_item *item,
    char **out_name
);
static int set_row_scalar_select_found_row_count(
    struct mylite_db *database,
    const struct planned_row_scalar_select *plan,
    mylite_result *result
);
static bool select_statement_has_group_by_clause(const struct mylite_sql_ast_node *statement);
