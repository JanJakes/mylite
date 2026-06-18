static int plan_grouped_aggregate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_grouped_aggregate *out_plan
);
static void planned_grouped_aggregate_deinit(struct planned_grouped_aggregate *plan);
static int collect_grouped_aggregate_clauses(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct grouped_aggregate_clauses *out_clauses
);
static int plan_grouped_aggregate_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct planned_grouped_aggregate *out_plan,
    struct select_source_context *out_source_context,
    struct mylite_catalog_column_descriptor **out_columns,
    size_t *out_column_count
);
static int plan_grouped_aggregate_join_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct planned_grouped_aggregate *out_plan,
    struct select_source_context *out_source_context
);
static int plan_grouped_aggregate_derived_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct planned_grouped_aggregate *out_plan,
    struct select_source_context *out_source_context
);
static int validate_grouped_projection_references(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count
);
static int validate_grouped_qualified_wildcard_reference(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *qualified_wildcard,
    const struct select_source_context *source_context
);
static int plan_grouped_aggregate_group_columns(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct mylite_sql_ast_node *group_clause,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
);
static int resolve_grouped_aggregate_group_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct mylite_sql_ast_node *group_key,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_key *out_key
);
static int resolve_grouped_aggregate_group_temporal_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *group_key,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_key *out_key
);
static int resolve_grouped_select_alias_group_column(
    struct mylite_db *database,
    const struct grouped_alias_group_resolution *resolution,
    struct planned_grouped_key *out_key,
    bool *out_matched
);
static int resolve_grouped_select_alias_group_temporal_expression(
    struct mylite_db *database,
    const struct grouped_alias_group_resolution *resolution,
    const struct mylite_sql_ast_node *matched_expression,
    const struct mylite_sql_ast_node *matched_alias,
    struct planned_grouped_key *out_key,
    bool *out_matched
);
static int count_grouped_source_columns_by_unqualified_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *group_key,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    size_t *out_match_count
);
static size_t grouped_aggregate_group_key_count(const struct mylite_sql_ast_node *group_clause);
static bool grouped_aggregate_group_clause_has_rollup(const struct mylite_sql_ast_node *group_clause
);
static int validate_grouped_aggregate_group_column(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
);
static bool grouped_expression_is_temporal_group_key(const struct mylite_sql_ast_node *expression);
static int plan_grouped_temporal_row_scalar_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_expression *out_expression
);
static int plan_grouped_aggregate_projection_columns(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan,
    size_t *out_first_aggregate_index
);
static int append_grouped_projection_column(
    struct mylite_db *database,
    struct planned_grouped_aggregate *plan,
    const struct mylite_catalog_column_descriptor *column,
    size_t source_index,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node *alias
);
static int append_grouped_projection_row_scalar_expression(
    struct mylite_db *database,
    struct planned_grouped_aggregate *plan,
    struct planned_row_scalar_expression *row_scalar_expression,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node *alias
);
static int plan_grouped_aggregate_temporal_projection(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node *alias,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
);
static int plan_grouped_aggregate_literal_projection(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node *alias,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
);
static int plan_grouped_aggregate_integer_arithmetic_projection(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node *alias,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
);
static int validate_grouped_projection_row_scalar_expression(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct planned_row_scalar_expression *expression
);
static int validate_grouped_projection_row_scalar_column(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct planned_row_scalar_expression *expression
);
static int plan_grouped_aggregate_relaxed_row_scalar_projection(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node *alias,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
);
static bool grouped_expression_is_relaxed_row_scalar_projection(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
);
static bool grouped_relaxed_row_scalar_projection_is_allowed(
    const struct planned_grouped_aggregate *plan,
    const struct planned_row_scalar_expression *expression
);
static int plan_grouped_aggregate_wildcard_projection_columns(
    struct mylite_db *database,
    size_t select_expression_index,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
);
static int plan_grouped_aggregate_qualified_wildcard_projection_columns(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *qualified_wildcard,
    size_t select_expression_index,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
);
static int append_visible_grouped_projection_columns_from_source(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    size_t source_index,
    struct planned_grouped_aggregate *out_plan
);
static int validate_grouped_projection_column(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct mylite_catalog_column_descriptor *column,
    size_t source_index,
    size_t select_expression_index
);
static int grouped_source_primary_key_is_covered(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    size_t source_index,
    bool *out_covered
);
static bool grouped_source_columns(
    const struct planned_grouped_aggregate *plan,
    size_t source_index,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct mylite_catalog_column_descriptor **out_columns,
    size_t *out_column_count,
    int64_t *out_table_id
);
static bool grouped_projection_column_is_allowed(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct mylite_catalog_column_descriptor *column,
    size_t source_index,
    int *out_rc
);
static bool grouped_row_scalar_projection_is_allowed(
    const struct planned_grouped_aggregate *plan,
    const struct planned_row_scalar_expression *expression
);
static bool planned_grouped_row_scalar_expressions_match(
    const struct planned_row_scalar_expression *left,
    const struct planned_row_scalar_expression *right
);
static bool grouped_plan_has_temporal_key_on_column(
    const struct planned_grouped_aggregate *plan,
    enum mylite_temporal_extract_kind temporal_extract_kind,
    const struct planned_row_scalar_expression *column_argument
);
static bool planned_grouped_temporal_key_uses_week_mode_one(
    const struct planned_row_scalar_expression *expression
);
static bool planned_grouped_row_scalar_columns_match(
    const struct planned_row_scalar_expression *left,
    const struct planned_row_scalar_expression *right
);
static bool planned_grouped_row_scalar_value_arguments_match(
    const struct planned_row_scalar_expression *left,
    const struct planned_row_scalar_expression *right
);
static int plan_grouped_aggregate_functions(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    size_t first_aggregate_index,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
);
static int plan_grouped_aggregate_item(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *aggregate_item,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan,
    struct planned_grouped_aggregate_item *out_item
);
static int plan_grouped_count_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column,
    size_t *out_source_index
);
static int plan_grouped_any_value_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column,
    size_t *out_source_index
);
static int plan_grouped_column_aggregate_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    enum planned_column_aggregate_function aggregate_function,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column,
    size_t *out_source_index
);
static int plan_grouped_aggregate_row_scalar_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate_item *out_item
);
static int plan_grouped_group_concat_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate_item *out_item
);
static int plan_group_concat_value_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column,
    size_t *out_source_index
);
static int plan_group_concat_order_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    bool qualify_column_reference,
    struct planned_select_order *out_order
);
static int plan_group_concat_separator(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    bool *out_has_separator,
    struct planned_value *out_separator
);
static const struct mylite_sql_ast_node *group_concat_value_node(
    const struct mylite_sql_ast_node *function
);
static const struct mylite_sql_ast_node *group_concat_order_node(
    const struct mylite_sql_ast_node *function
);
static const struct mylite_sql_ast_node *group_concat_separator_node(
    const struct mylite_sql_ast_node *function
);
static enum planned_grouped_aggregate_function grouped_aggregate_function_from_expression(
    const struct mylite_sql_ast_node *expression
);
static enum planned_column_aggregate_function grouped_column_aggregate_function(
    enum planned_grouped_aggregate_function function
);
static bool grouped_aggregate_function_has_column(enum planned_grouped_aggregate_function function);
static bool grouped_aggregate_function_is_bitwise(enum planned_grouped_aggregate_function function);
static bool planned_grouped_aggregate_columns_match(
    const struct mylite_catalog_column_descriptor *left_column,
    size_t left_source_index,
    const struct mylite_catalog_column_descriptor *right_column,
    size_t right_source_index
);
static bool find_matching_grouped_key(
    const struct planned_grouped_aggregate *plan,
    const struct mylite_catalog_column_descriptor *column,
    size_t source_index,
    size_t *out_group_index
);
static bool find_grouped_key_by_unqualified_name(
    const struct planned_grouped_aggregate *plan,
    const char *name,
    size_t *out_group_index,
    bool *out_ambiguous
);
static bool find_grouped_projection_by_unqualified_name(
    const struct planned_grouped_aggregate *plan,
    const char *name,
    size_t *out_projection_index,
    bool *out_ambiguous
);
static const struct table_name_resolution *planned_grouped_aggregate_source_resolution(
    const struct planned_grouped_aggregate *plan,
    size_t source_index
);
static int plan_grouped_aggregate_having(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *having_clause,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
);
static int plan_grouped_having_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *having_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
);
static int plan_grouped_having_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *operand_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan,
    enum planned_grouped_having_operand *out_operand
);
static int plan_grouped_having_identifier_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *operand_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan,
    enum planned_grouped_having_operand *out_operand
);
static int plan_grouped_having_row_scalar_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *operand_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan,
    enum planned_grouped_having_operand *out_operand
);
static int validate_grouped_having_row_scalar_expression(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct planned_row_scalar_expression *expression
);
static int validate_grouped_having_row_scalar_column(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct planned_row_scalar_expression *expression
);
static int find_grouped_having_projection_alias_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *operand_node,
    const char *column_name,
    const struct planned_grouped_aggregate *plan,
    bool *out_matched,
    size_t *out_group_index
);
static int find_grouped_having_group_alias_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *operand_node,
    const struct planned_grouped_aggregate *plan,
    bool *out_matched,
    size_t *out_group_index
);
static int plan_grouped_having_aggregate_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *operand_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
);
static int find_grouped_having_aggregate_alias_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *operand_node,
    struct planned_grouped_aggregate *out_plan,
    bool *out_matched
);
static int convert_grouped_having_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct planned_grouped_aggregate *plan,
    enum planned_grouped_having_operand operand,
    struct planned_value *out_value
);
static int parse_grouped_having_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const char *operand_name,
    bool *out_is_negative,
    uint64_t *out_magnitude
);
static int convert_grouped_having_group_value(
    struct mylite_db *database,
    uint64_t magnitude,
    bool is_negative,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);
static int convert_grouped_having_aggregate_value(
    struct mylite_db *database,
    uint64_t magnitude,
    bool is_negative,
    const char *operand_name,
    struct planned_value *out_value
);
static int plan_grouped_aggregate_order(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_clause,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
);
static int resolve_grouped_order_key(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan,
    struct planned_select_order_item *out_item,
    bool *out_uses_aggregate,
    size_t *out_aggregate_index
);
static int plan_grouped_aggregate_order_ast_item_and_append(
    struct mylite_db *database,
    struct select_order_ast_item_nodes item_nodes,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
);
static int validate_grouped_order_row_scalar_expression(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct planned_row_scalar_expression *expression
);
static int validate_grouped_order_row_scalar_column(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct planned_row_scalar_expression *expression
);
static int append_planned_grouped_order_item(
    struct planned_grouped_aggregate *plan,
    struct planned_select_order_item item,
    bool uses_aggregate,
    size_t aggregate_index
);
static int find_grouped_order_group_alias(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    const struct planned_grouped_aggregate *plan,
    bool *out_matched,
    size_t *out_group_index
);
static int find_grouped_order_projection_alias(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    const struct planned_grouped_aggregate *plan,
    bool *out_matched,
    size_t *out_projection_index
);
static int order_identifier_matches_alias(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct mylite_sql_ast_node *alias,
    bool *out_matches
);
static int order_identifier_matches_unaliased_group_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct planned_grouped_aggregate *plan,
    bool *out_matches,
    size_t *out_group_index
);
static int order_identifier_matches_unaliased_projection_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct planned_grouped_aggregate *plan,
    bool *out_matches,
    size_t *out_projection_index
);
static int find_grouped_order_aggregate_alias(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    const struct planned_grouped_aggregate *plan,
    bool *out_matched,
    size_t *out_aggregate_index
);
static int execute_grouped_aggregate_from_plan(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *plan,
    mylite_result **out_result
);
static int append_grouped_aggregate_result_columns(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_grouped_aggregate *plan
);
static bool select_statement_has_count_aggregate(const struct mylite_sql_ast_node *statement);
static int plan_count(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_count *out_plan
);
static void planned_count_deinit(struct planned_count *plan);
static int plan_count_without_source(
    struct mylite_db *database,
    const struct planned_count_source_nodes *nodes,
    struct planned_count *out_plan
);
static int plan_count_table_source(
    struct mylite_db *database,
    const struct planned_count_source_nodes *nodes,
    struct planned_count *out_plan
);
static int plan_count_derived_table_source(
    struct mylite_db *database,
    const struct planned_count_source_nodes *nodes,
    struct planned_count *out_plan
);
static int plan_count_derived_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_count *out_plan
);
static int plan_count_derived_compound_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_count *out_plan
);
static int plan_count_derived_compound_branch(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *branch,
    struct planned_count_derived_compound_branch *out_branch
);
static void planned_count_derived_compound_deinit(struct planned_count_derived_compound *compound);
static void planned_count_derived_compound_branch_deinit(
    struct planned_count_derived_compound_branch *branch
);
static int validate_count_order_clause(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_clause,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count
);
static int validate_count_order_item(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count
);
static enum planned_count_function count_function_from_expression(
    const struct mylite_sql_ast_node *expression
);
static const char *count_exactly_one_message(enum planned_count_function function);
static const char *count_supported_clauses_message(enum planned_count_function function);
static const char *count_descriptor_table_message(enum planned_count_function function);
static int plan_count_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    struct planned_value *out_literal
);
static int plan_count_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column
);
static int execute_count_from_plan(
    struct mylite_db *database,
    const struct planned_count *plan,
    bool apply_sql_select_limit,
    mylite_result **out_result
);
static void apply_count_limit_to_result(mylite_result *result, const struct planned_count *plan);
static int plan_count_expression_aggregate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_count_expression_aggregate *out_plan
);
static void planned_count_expression_aggregate_deinit(
    struct planned_count_expression_aggregate *plan
);
static int execute_count_expression_aggregate_from_plan(
    struct mylite_db *database,
    const struct planned_count_expression_aggregate *plan,
    bool apply_sql_select_limit,
    mylite_result **out_result
);
static bool select_statement_has_column_aggregate(const struct mylite_sql_ast_node *statement);
static int plan_column_aggregate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_column_aggregate *out_plan
);
static int plan_column_aggregate_without_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *aggregate_expression,
    enum planned_column_aggregate_function function,
    const struct mylite_sql_ast_node *where_clause,
    struct planned_column_aggregate *out_plan
);
static void planned_column_aggregate_deinit(struct planned_column_aggregate *plan);
static enum planned_column_aggregate_function column_aggregate_function_from_expression(
    const struct mylite_sql_ast_node *expression
);
static enum planned_column_aggregate_function column_aggregate_function_from_projection_expression(
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node **out_aggregate_expression,
    enum mylite_sql_ast_operator *out_operator,
    const struct mylite_sql_ast_node **out_rhs_expression
);
static enum planned_column_aggregate_function select_list_column_aggregate_function(
    const struct mylite_sql_ast_node *select_list
);
static bool column_aggregate_argument_is_row_scalar_expression(
    const struct mylite_sql_ast_node *function
);
static bool column_aggregate_function_is_bitwise(enum planned_column_aggregate_function function);
static bool column_aggregate_function_is_min_max(enum planned_column_aggregate_function function);
static const char *column_aggregate_single_item_error(
    enum planned_column_aggregate_function function
);
static const char *column_aggregate_optional_clause_error(
    enum planned_column_aggregate_function function
);
static const char *column_aggregate_source_error(enum planned_column_aggregate_function function);
static const char *column_aggregate_column_error(enum planned_column_aggregate_function function);
static const char *column_aggregate_integer_error(enum planned_column_aggregate_function function);
static int validate_column_aggregate_descriptor_column(
    struct mylite_db *database,
    enum planned_column_aggregate_function aggregate_function,
    const struct mylite_catalog_column_descriptor *column
);
static int plan_column_aggregate_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    enum planned_column_aggregate_function aggregate_function,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column
);
static int plan_column_aggregate_row_scalar_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    const struct select_source_context *source_context,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_column_aggregate *out_plan
);
static int plan_column_group_concat_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    const struct select_source_context *source_context,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_column_aggregate *out_plan
);
static int execute_column_aggregate_from_plan(
    struct mylite_db *database,
    const struct planned_column_aggregate *plan,
    bool apply_sql_select_limit,
    mylite_result **out_result
);
static void apply_column_aggregate_limit_to_result(
    mylite_result *result,
    const struct planned_column_aggregate *plan
);
static int append_count_result_column(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_count *plan
);
static int append_column_aggregate_result_column(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_column_aggregate *plan
);
static int copy_aggregate_result_column_name(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    char **out_text
);
static size_t aggregate_label_extra_spaces_after_block_comments(
    const struct mylite_sql_source_span *span
);
static void copy_aggregate_label_with_spacing(
    const struct mylite_sql_source_span *span,
    char *destination
);
static bool aggregate_label_needs_space_after_block_comment(char next_byte);
static int read_count_from_source(
    struct mylite_db *database,
    const struct planned_count *plan,
    int64_t *out_count
);
static int read_column_aggregate_from_source(
    struct mylite_db *database,
    const struct planned_column_aggregate *plan,
    mylite_result *result
);
static int read_grouped_aggregate_from_source(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *plan,
    mylite_result *result
);
static int set_grouped_aggregate_found_row_count(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *plan,
    mylite_result *result
);
static int read_grouped_aggregate_found_row_count(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *plan,
    int64_t *out_count
);
static int step_count_statement(sqlite3_stmt *statement, int64_t *out_count);
static int step_column_aggregate_statement(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_column_aggregate *plan,
    mylite_result *result
);
static int append_grouped_aggregate_sqlite_row(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_grouped_aggregate *plan,
    mylite_result *result
);
static int append_grouped_aggregate_projection_cell(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_grouped_aggregate *plan,
    size_t projection_index,
    struct mylite_result_cell *out_cell,
    char *buffer,
    size_t buffer_size
);
static int append_grouped_aggregate_sqlite_cell(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_grouped_aggregate_item *item,
    int *sqlite_column_index,
    struct mylite_result_cell *out_cell,
    char *buffer,
    size_t buffer_size
);
static int append_grouped_group_concat_sqlite_cell(
    sqlite3_stmt *statement,
    int sqlite_column_index,
    struct mylite_result_cell *out_cell
);
static int sqlite_integer_result_text(
    sqlite3_stmt *statement,
    int column_index,
    char *buffer,
    size_t buffer_size,
    const char **out_value
);
static int append_avg_sqlite_row(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    mylite_result *result
);
static int append_bitwise_aggregate_sqlite_row(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    mylite_result *result
);
static int format_avg_value(
    struct mylite_db *database,
    struct avg_accumulator accumulator,
    char *buffer,
    size_t buffer_size
);
static uint64_t absolute_int64_magnitude(int64_t value);
static int next_decimal_digit(uint64_t *remainder, uint64_t denominator);
static struct uint128_parts multiply_u64_by_decimal_radix(uint64_t value);
static bool uint128_ge_u64(const struct uint128_parts *left, uint64_t right);
static void uint128_subtract_u64(struct uint128_parts *left, uint64_t right);
static int column_aggregate_step_error(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_column_aggregate *plan,
    int sqlite_rc
);
static int grouped_aggregate_step_error(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_grouped_aggregate *plan,
    int sqlite_rc
);
static int format_count_value(
    struct mylite_db *database,
    int64_t count_value,
    char *buffer,
    size_t buffer_size
);
static int append_count_value_row(
    struct mylite_db *database,
    mylite_result *result,
    const char *count_text
);
static int count_execution_error(struct mylite_db *database, int rc);
static int column_aggregate_execution_error(struct mylite_db *database, int rc);
static int grouped_aggregate_execution_error(struct mylite_db *database, int rc);
