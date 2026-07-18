static int init_select_source_context(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    const struct table_name_resolution *source,
    struct select_source_context *out_context
);

static int resolve_table_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct table_name_resolution *out_resolution
);
static int resolve_writable_table_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct table_name_resolution *out_resolution
);
static int resolve_visible_table_reference(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct table_name_resolution *out_resolution,
    struct mylite_catalog_table_descriptor *out_table
);
static int resolve_visible_writable_table_reference(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct table_name_resolution *out_resolution,
    struct mylite_catalog_table_descriptor *out_table
);
static int resolve_table_name_allow_missing_schema(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct table_name_resolution *out_resolution,
    bool *out_missing_schema
);
static int resolve_writable_table_name_allow_missing_schema(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct table_name_resolution *out_resolution,
    bool *out_missing_schema
);
static int resolve_readable_table(
    struct mylite_db *database,
    const struct table_name_resolution *resolution,
    bool missing_schema,
    struct mylite_catalog_table_descriptor *out_table
);
static int resolve_metadata_table_reference(
    struct mylite_db *database,
    const struct table_name_resolution *resolution,
    bool missing_schema,
    struct mylite_catalog_table_descriptor *out_table
);
static int resolve_persistent_metadata_table_reference(
    struct mylite_db *database,
    const struct table_name_resolution *resolution,
    bool missing_schema,
    struct mylite_catalog_table_descriptor *out_table
);
static int resolve_show_columns_table_name(
    struct mylite_db *database,
    struct show_columns_target_nodes nodes,
    struct table_name_resolution *out_resolution,
    bool *out_missing_schema
);
static int copy_show_columns_explicit_table_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node,
    char *destination,
    size_t destination_size
);
static int resolve_truncate_table_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct table_name_resolution *out_resolution
);
static int resolve_writable_truncate_table_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct table_name_resolution *out_resolution
);
static int require_selected_schema_for_unqualified_table_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node
);
static int resolve_schema_name(
    struct mylite_db *database,
    const char *schema_name,
    struct mylite_catalog_schema_descriptor *out_schema
);
static int resolve_selected_schema(
    struct mylite_db *database,
    struct mylite_catalog_schema_descriptor *out_schema
);
static int collect_identifier_parts(
    const struct mylite_sql_ast_node *node,
    char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t part_capacity,
    size_t *part_count,
    struct mylite_db *database
);
static int reject_builtin_schema_write_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node
);
static int reject_builtin_schema_schema_write_name(
    struct mylite_db *database,
    const char *schema_name
);
static const struct mylite_execution_catalog_builtin_schema *find_builtin_schema_descriptor(
    const char *schema_name
);
static bool selected_schema_is_builtin_schema(const struct mylite_db *database);
static const char *builtin_schema_error_name(const char *schema_name);
static void set_builtin_schema_write_error(struct mylite_db *database, const char *schema_name);
static bool selected_schema_is_information_schema(const struct mylite_db *database);
static bool schema_name_is_information_schema(const char *schema_name);
static int plan_create_table_items(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *item_list,
    struct planned_create_table *out_plan
);
static int validate_create_table_visible_columns(
    struct mylite_db *database,
    const struct planned_create_table *plan
);
static int plan_create_table_item(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *item,
    struct planned_create_table *out_plan,
    size_t *column_index,
    const struct mylite_sql_ast_node **primary_key
);
static int apply_create_table_default_binary_charset_to_column(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct mylite_sql_ast_node *column_node,
    struct planned_column *column
);
static int normalize_text_length_argument_for_default_charset(
    struct mylite_db *database,
    struct planned_column *column,
    const char *default_charset
);
static int apply_default_binary_charset_to_column(
    struct mylite_db *database,
    const char *default_charset,
    struct planned_column *column
);
static int apply_column_binary_collation_shorthand(
    struct mylite_db *database,
    const char *default_charset,
    struct planned_column *column
);
static bool create_table_column_has_explicit_charset_or_collation(
    const struct mylite_sql_ast_node *column_node
);
static bool planned_create_table_default_charset_is_binary(const struct planned_create_table *plan);
static int count_create_table_column_items(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *item_list,
    size_t *out_column_count
);
static int validate_create_table_item_list(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *item_list
);
static int apply_create_table_inline_primary_key(
    struct mylite_db *database,
    struct planned_create_table *plan,
    size_t column_index,
    const struct mylite_sql_ast_node *column_node
);
static int apply_create_table_primary_key_definition(
    struct mylite_db *database,
    struct planned_create_table *plan,
    const struct mylite_sql_ast_node *primary_key
);
static int extract_primary_key_definition_nodes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *primary_key,
    struct primary_key_definition_nodes *out_nodes
);
static int apply_create_table_secondary_index_definitions(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *item_list,
    struct planned_create_table *plan
);
static int apply_create_table_secondary_index_definition(
    struct mylite_db *database,
    struct planned_create_table *plan,
    const struct mylite_sql_ast_node *secondary_index,
    enum mylite_catalog_index_kind index_kind,
    bool is_unique
);
static int apply_create_table_foreign_key_definitions(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *item_list,
    struct planned_create_table *plan
);
static int apply_create_table_foreign_key_definition(
    struct mylite_db *database,
    struct planned_create_table *plan,
    const struct mylite_sql_ast_node *foreign_key
);
static int apply_create_table_check_constraint_definitions(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *item_list,
    struct planned_create_table *plan
);
static int apply_create_table_column_check_constraint_definitions(
    struct mylite_db *database,
    struct planned_create_table *plan,
    const struct mylite_sql_ast_node *column_definition,
    size_t column_index
);
static int apply_create_table_check_constraint_definition(
    struct mylite_db *database,
    struct planned_create_table *plan,
    const struct create_table_check_constraint_definition *definition
);
static int64_t next_planned_check_constraint_generated_ordinal(
    const struct planned_create_table *plan
);
static int plan_create_table_check_constraint_name(
    struct mylite_db *database,
    struct planned_create_table *plan,
    const struct mylite_sql_ast_node *name_node,
    struct planned_check_constraint *check_constraint
);
static int generate_create_table_check_constraint_name(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    int64_t generated_ordinal,
    char *destination,
    size_t destination_size
);
static bool planned_check_constraint_name_is_used(
    const struct planned_create_table *plan,
    const char *check_constraint_name
);
static int reject_duplicate_schema_check_constraint_name(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const char *check_constraint_name
);
static int reject_duplicate_schema_check_constraint_name_callback(
    const struct mylite_catalog_check_constraint_descriptor *check_constraint,
    void *user_data
);
static int render_check_constraint_expression(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct planned_column *inline_column,
    size_t inline_column_index,
    const struct mylite_sql_ast_node *expression,
    char *check_clause,
    size_t check_clause_size,
    char *sqlite_expression,
    size_t sqlite_expression_size
);
static int finalize_planned_generated_columns(
    struct mylite_db *database,
    struct planned_create_table *plan
);
static int finalize_planned_generated_column(
    struct mylite_db *database,
    struct planned_create_table *plan,
    size_t column_index
);
static int render_generated_column_expression(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    size_t generated_column_index,
    const struct mylite_sql_ast_node *expression,
    char *generation_expression,
    size_t generation_expression_size,
    char *sqlite_expression,
    size_t sqlite_expression_size
);
static int render_generated_expression_node(
    struct generated_expression_render_context *context,
    const struct mylite_sql_ast_node *node
);
static int render_generated_expression_work_item(
    struct generated_expression_render_context *context,
    struct generated_expression_render_work_item item,
    struct generated_expression_render_work_item **items,
    size_t *item_count
);
static int render_generated_expression_column(
    struct generated_expression_render_context *context,
    const struct mylite_sql_ast_node *node
);
static int render_generated_expression_literal(
    struct generated_expression_render_context *context,
    const struct mylite_sql_ast_node *node
);
static int render_generated_expression_unary(
    struct generated_expression_render_context *context,
    const struct mylite_sql_ast_node *node,
    struct generated_expression_render_work_item **items,
    size_t *item_count
);
static int render_generated_expression_binary(
    struct generated_expression_render_context *context,
    const struct mylite_sql_ast_node *node,
    struct generated_expression_render_work_item **items,
    size_t *item_count
);
static const char *generated_expression_binary_operator_text(
    enum mylite_sql_ast_operator operator_kind
);
static int append_generated_expression_render_node(
    struct mylite_db *database,
    struct generated_expression_render_work_item **items,
    size_t *item_count,
    const struct mylite_sql_ast_node *node
);
static int append_generated_expression_render_text(
    struct mylite_db *database,
    struct generated_expression_render_work_item **items,
    size_t *item_count,
    const char *text
);
static int append_generated_expression_render_char(
    struct mylite_db *database,
    struct generated_expression_render_work_item **items,
    size_t *item_count,
    char character
);
static int append_generated_expression_render_work_item(
    struct mylite_db *database,
    struct generated_expression_render_work_item **items,
    size_t *item_count,
    struct generated_expression_render_work_item item
);
static int render_check_expression_node(
    struct check_expression_render_context *context,
    const struct mylite_sql_ast_node *node,
    bool require_boolean,
    bool *out_is_boolean
);
static int render_check_expression_work_item(
    struct check_expression_render_context *context,
    struct check_expression_render_work_item item,
    struct check_expression_render_work_item **items,
    size_t *item_count,
    bool *out_is_boolean
);
static int render_check_expression_column(
    struct check_expression_render_context *context,
    const struct mylite_sql_ast_node *node
);
static int render_check_expression_literal(
    struct check_expression_render_context *context,
    const struct mylite_sql_ast_node *node,
    bool require_boolean,
    bool *out_is_boolean
);
static int render_check_expression_function(
    struct check_expression_render_context *context,
    const struct mylite_sql_ast_node *node,
    bool *out_is_boolean
);
static int render_check_expression_identifier_argument_function(
    struct check_expression_render_context *context,
    const struct mylite_sql_ast_node *argument,
    const char *check_clause_function,
    const char *sqlite_function,
    bool is_boolean,
    bool *out_is_boolean
);
static int append_check_expression_mysql_string_literal(
    struct mylite_dynamic_string *string,
    const char *text,
    size_t text_length
);
static int append_check_expression_sqlite_string_literal(
    struct mylite_dynamic_string *string,
    const char *text,
    size_t text_length
);
static int render_check_expression_unary(
    struct check_expression_render_context *context,
    const struct mylite_sql_ast_node *node,
    struct check_expression_render_work_item **items,
    size_t *item_count,
    bool *out_is_boolean
);
static int render_check_expression_binary(
    struct check_expression_render_context *context,
    const struct mylite_sql_ast_node *node,
    struct check_expression_render_work_item **items,
    size_t *item_count,
    bool *out_is_boolean
);
static bool check_expression_binary_operator_is_boolean(enum mylite_sql_ast_operator operator_kind);
static bool check_expression_binary_operator_is_arithmetic(
    enum mylite_sql_ast_operator operator_kind
);
static bool check_expression_binary_operator_requires_boolean_children(
    enum mylite_sql_ast_operator operator_kind
);
static const char *check_expression_check_clause_operator_sql(
    enum mylite_sql_ast_operator operator_kind
);
static const char *check_expression_sqlite_operator_sql(enum mylite_sql_ast_operator operator_kind);
static int append_check_expression_span_text(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct mylite_sql_ast_node *node
);
static int append_check_expression_render_node(
    struct mylite_db *database,
    struct check_expression_render_work_item **items,
    size_t *item_count,
    const struct mylite_sql_ast_node *node,
    bool require_boolean
);
static int append_check_expression_render_text(
    struct mylite_db *database,
    struct check_expression_render_work_item **items,
    size_t *item_count,
    const char *text
);
static int append_check_expression_render_char(
    struct mylite_db *database,
    struct check_expression_render_work_item **items,
    size_t *item_count,
    char character
);
static int append_check_expression_render_operator(
    struct mylite_db *database,
    struct check_expression_render_work_item **items,
    size_t *item_count,
    enum mylite_sql_ast_operator operator_kind
);
static int append_check_expression_render_work_item(
    struct mylite_db *database,
    struct check_expression_render_work_item **items,
    size_t *item_count,
    struct check_expression_render_work_item item
);
static bool check_constraint_node_is_not_enforced(const struct mylite_sql_ast_node *node);
static bool check_constraint_node_is_enforcement(const struct mylite_sql_ast_node *node);
static int reserve_planned_check_constraints(
    struct mylite_db *database,
    struct planned_create_table *plan,
    size_t requested_capacity
);
static int extract_foreign_key_definition_nodes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *foreign_key,
    struct foreign_key_definition_nodes *out_nodes
);
static int record_foreign_key_definition_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct foreign_key_definition_nodes *out_nodes
);
static int record_foreign_key_identifier_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct foreign_key_definition_nodes *out_nodes
);
static int record_foreign_key_qualified_identifier_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct foreign_key_definition_nodes *out_nodes
);
static int record_foreign_key_index_name_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct foreign_key_definition_nodes *out_nodes
);
static int record_foreign_key_part_list_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct foreign_key_definition_nodes *out_nodes
);
static int record_foreign_key_action_list_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct foreign_key_definition_nodes *out_nodes
);
static int copy_foreign_key_index_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *index_name_node,
    char *destination,
    size_t destination_size
);
static int parse_foreign_key_actions(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *action_list_node,
    char *update_rule,
    size_t update_rule_size,
    char *delete_rule,
    size_t delete_rule_size
);
static int copy_foreign_key_rule_text(
    struct mylite_db *database,
    char *destination,
    size_t destination_size,
    const char *rule
);
static bool foreign_key_rule_equals(const char *rule, const char *expected_rule);
static bool foreign_key_rule_is_cascade(const char *rule);
static bool foreign_key_rule_is_set_null(const char *rule);
static int validate_planned_foreign_key_set_null_columns(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct planned_foreign_key *foreign_key
);
static void foreign_key_column_names_deinit(struct foreign_key_column_names *names);
static int copy_foreign_key_part_names(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *part_list,
    struct foreign_key_part_name **out_parts,
    size_t *out_part_count
);
static int validate_foreign_key_part_counts(
    struct mylite_db *database,
    size_t child_part_count,
    size_t parent_part_count
);
static int reject_duplicate_foreign_key_child_parts(
    struct mylite_db *database,
    const struct foreign_key_column_names *names
);
static int plan_create_table_foreign_key_name(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct mylite_sql_ast_node *constraint_name_node,
    char *destination,
    size_t destination_size
);
static int generate_create_table_foreign_key_name(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    char *destination,
    size_t destination_size
);
static bool planned_foreign_key_name_is_used(
    const struct planned_create_table *plan,
    const char *foreign_key_name
);
static int validate_foreign_key_integer_columns(
    struct mylite_db *database,
    const char *child_logical_type,
    const char *parent_logical_type
);
static void planned_foreign_key_deinit(struct planned_foreign_key *foreign_key);
static int append_planned_foreign_key_part(
    struct mylite_db *database,
    struct planned_foreign_key *foreign_key,
    size_t child_column_index,
    size_t parent_column_index,
    const struct mylite_catalog_column_descriptor *parent_column
);
static int reserve_planned_foreign_key_parts(
    struct mylite_db *database,
    struct planned_foreign_key *foreign_key,
    size_t required_capacity
);
static int plan_create_table_foreign_key_parent(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct mylite_sql_ast_node *parent_table_node,
    const struct foreign_key_column_names *names,
    struct planned_foreign_key *foreign_key
);
static bool create_table_foreign_key_parent_is_self_reference(
    const struct planned_create_table *plan,
    const struct table_name_resolution *parent_resolution
);
static int plan_create_table_self_referencing_foreign_key_parent(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct table_name_resolution *parent_resolution,
    const struct foreign_key_column_names *names,
    struct planned_foreign_key *foreign_key
);
static int copy_planned_column_to_self_foreign_key_parent(
    struct mylite_db *database,
    const struct planned_column *planned,
    size_t column_index,
    struct mylite_catalog_column_descriptor *out_column
);
static int find_create_table_self_foreign_key_parent_index(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    struct planned_foreign_key *foreign_key
);
static int find_foreign_key_parent_index(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *parent_table,
    const struct planned_foreign_key *foreign_key,
    struct mylite_catalog_index_descriptor *out_index
);
static int find_alter_table_foreign_key_parent_index(
    struct mylite_db *database,
    const struct planned_alter_table_add_foreign_key *plan,
    struct mylite_catalog_index_descriptor *out_index
);
static int choose_create_table_foreign_key_child_index(
    struct mylite_db *database,
    struct planned_create_table *plan,
    struct planned_foreign_key *foreign_key
);
static int add_create_table_foreign_key_child_index(
    struct mylite_db *database,
    struct planned_create_table *plan,
    struct planned_foreign_key *foreign_key
);
static int plan_create_table_foreign_key_child_index_name(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct planned_foreign_key *foreign_key,
    const char *default_name,
    char *destination,
    size_t destination_size
);
static bool planned_primary_key_starts_with_column(
    const struct planned_create_table *plan,
    const struct planned_foreign_key *foreign_key
);
static int reserve_planned_foreign_keys(
    struct mylite_db *database,
    struct planned_create_table *plan,
    size_t required_capacity
);
static int append_planned_secondary_index_part(
    struct mylite_db *database,
    struct planned_create_table *plan,
    struct planned_secondary_index *index,
    const struct mylite_sql_ast_node *part
);
static int validate_planned_unique_index_part_list(
    struct mylite_db *database,
    const struct planned_secondary_index *index
);
static int reserve_planned_secondary_index_parts(
    struct mylite_db *database,
    struct planned_secondary_index *index,
    size_t required_capacity
);
static int apply_create_table_inline_unique_indexes(
    struct mylite_db *database,
    struct planned_create_table *plan
);
static int reserve_planned_secondary_indexes(
    struct mylite_db *database,
    struct planned_create_table *plan,
    size_t required_capacity
);
static int plan_secondary_index_name(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct mylite_sql_ast_node *index_name_node,
    const char *column_name,
    char *destination,
    size_t destination_size
);
static int generate_secondary_index_name(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const char *base_name,
    char *destination,
    size_t destination_size
);
static bool planned_index_name_is_used(
    const struct planned_create_table *plan,
    const char *index_name
);
static int validate_secondary_index_column(
    struct mylite_db *database,
    const struct planned_column *column
);
static int validate_fulltext_index_column(
    struct mylite_db *database,
    const struct planned_column *column
);
static int validate_spatial_index_column(
    struct mylite_db *database,
    const struct planned_column *column
);
static int validate_fulltext_string_key_column(
    struct mylite_db *database,
    const struct planned_column *column
);
static bool planned_secondary_index_is_fulltext(const struct planned_secondary_index *index);
static bool planned_secondary_index_is_spatial(const struct planned_secondary_index *index);
static const char *planned_secondary_index_unqualified_column_message(
    const struct planned_secondary_index *index
);
static int validate_planned_secondary_index_part(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct planned_secondary_index *index,
    size_t column_index,
    const char *column_name,
    struct secondary_index_part_nodes nodes,
    int64_t *out_prefix_length
);
static int validate_secondary_index_prefix_for_planned_column(
    struct mylite_db *database,
    const struct planned_column *column,
    int64_t prefix_length,
    uint64_t *out_key_bytes
);
static int text_family_prefix_key_bytes(
    struct mylite_db *database,
    const char *logical_type,
    uint64_t prefix,
    uint64_t *out_key_bytes
);
static int binary_string_prefix_key_bytes(
    struct mylite_db *database,
    const char *logical_type,
    uint64_t prefix,
    uint64_t *out_key_bytes
);
static int binary_string_full_key_bytes(
    struct mylite_db *database,
    const struct binary_string_type_info *info,
    const char *column_name,
    uint64_t *out_key_bytes
);
static int planned_index_part_key_bytes(
    struct mylite_db *database,
    const struct planned_column *column,
    uint64_t *out_key_bytes
);
static int validate_unique_index_column(
    struct mylite_db *database,
    const struct planned_column *column
);
static int validate_planned_secondary_index_key_length(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct planned_secondary_index *index
);
static int validate_create_table_primary_key_key_length(
    struct mylite_db *database,
    const struct planned_create_table *plan
);
static int planned_primary_key_part_key_bytes(
    struct mylite_db *database,
    const struct planned_column *column,
    uint64_t *out_key_bytes
);
static int column_descriptor_primary_key_part_key_bytes(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    uint64_t *out_key_bytes
);
static int parse_secondary_index_part_prefix_length(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *prefix_node,
    const char *column_name,
    int64_t *out_prefix_length
);
static const char *index_part_unsupported_message(
    const struct mylite_sql_ast_node *part,
    const char *default_message
);
static bool secondary_index_part_is_multi_valued(const struct mylite_sql_ast_node *part);
static bool secondary_index_part_is_functional(const struct mylite_sql_ast_node *part);
static const struct mylite_sql_ast_node *secondary_index_part_column_node(
    const struct mylite_sql_ast_node *part
);
static const struct mylite_sql_ast_node *secondary_index_part_prefix_node(
    const struct mylite_sql_ast_node *part
);
static const struct mylite_sql_ast_node *secondary_index_part_direction_node(
    const struct mylite_sql_ast_node *part
);
static enum mylite_catalog_index_sort_direction index_part_sort_direction(
    const struct mylite_sql_ast_node *part
);
static int mark_primary_key_column(
    struct mylite_db *database,
    struct planned_create_table *plan,
    size_t column_index,
    enum mylite_catalog_index_sort_direction sort_direction
);
static int reserve_planned_primary_key_parts(
    struct mylite_db *database,
    struct planned_create_table *plan,
    size_t required_capacity
);
static bool planned_primary_key_contains_column(
    const struct planned_create_table *plan,
    size_t column_index
);
static int validate_primary_key_column(struct mylite_db *database, struct planned_column *column);
static int validate_create_table_auto_increment_columns(
    struct mylite_db *database,
    struct planned_create_table *plan,
    bool allow_secondary_key
);
static int validate_auto_increment_column(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    size_t column_index,
    bool allow_secondary_key
);
static bool planned_primary_key_starts_with_column_index(
    const struct planned_create_table *plan,
    size_t column_index
);
static bool planned_secondary_index_starts_with_column(
    const struct planned_create_table *plan,
    size_t column_index
);
static int find_planned_column_index(
    const struct planned_column *columns,
    size_t column_count,
    const char *name,
    size_t *out_index
);
static int plan_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    struct planned_column *out_column
);
static const struct mylite_sql_ast_node *last_column_default_node(
    const struct mylite_sql_ast_node *column_node
);
static const struct mylite_sql_ast_node *last_column_nullability_node(
    const struct mylite_sql_ast_node *column_node
);
static int validate_column_attributes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct planned_column *column
);
static bool column_attribute_counts_have_duplicates(
    size_t primary_key_count,
    size_t auto_increment_count,
    size_t on_update_count,
    size_t charset_count,
    size_t collation_count,
    size_t generated_count
);
static int validate_generated_column_attributes(
    struct mylite_db *database,
    const struct planned_column *column
);
static int validate_non_generated_column_attributes(
    struct mylite_db *database,
    const struct planned_column *column
);
static int apply_column_charset_collation_attributes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    struct planned_column *column
);
static int apply_column_comment_attributes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    struct planned_column *column
);
static int apply_column_generated_attribute(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    struct planned_column *column
);
static enum mylite_catalog_generated_column_kind generated_column_storage_kind(
    const struct mylite_sql_ast_node *generated_clause
);
static bool column_charset_collation_requests_binary(
    bool has_charset,
    const char *charset_name,
    bool has_collation,
    const char *collation_name
);
static bool column_collation_attribute_is_binary_shorthand(
    const struct mylite_sql_ast_node *collation_attribute
);
static int apply_column_binary_charset_collation_attributes(
    struct mylite_db *database,
    struct planned_column *column,
    bool has_charset,
    const char *charset_name,
    bool has_collation,
    const char *collation_name
);
static int validate_column_binary_charset_collation_attributes(
    struct mylite_db *database,
    bool has_charset,
    const char *charset_name,
    bool has_collation,
    const char *collation_name
);
static int normalize_column_to_binary_string_descriptor(
    struct mylite_db *database,
    struct planned_column *column
);
static int normalize_column_to_binary_text_descriptor(
    struct mylite_db *database,
    struct planned_column *column
);
static int copy_column_charset_attribute_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *charset_attribute,
    char *charset_name,
    size_t charset_name_size
);
static int copy_column_collation_attribute_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *collation_attribute,
    char *collation_name,
    size_t collation_name_size
);
static bool planned_column_allows_charset_collation_attributes(const struct planned_column *column);
static bool planned_column_is_national_char_or_varchar(const struct planned_column *column);
static int validate_column_default(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *default_node,
    const struct planned_column *column
);
static int validate_column_default_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *default_node,
    const struct planned_column *column
);
static int validate_binary_column_default_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *default_node,
    const struct planned_column *column
);
static int finalize_planned_column_defaults(
    struct mylite_db *database,
    struct planned_column *columns,
    size_t column_count
);
static int finalize_planned_column_default(
    struct mylite_db *database,
    struct planned_column *column
);
static int finalize_planned_column_parenthesized_default(
    struct mylite_db *database,
    struct planned_column *column,
    const struct mylite_sql_ast_node *value_node
);
static int finalize_planned_column_now_scalar_expression_default(
    struct mylite_db *database,
    struct planned_column *column,
    const struct mylite_sql_ast_node *value_node
);
static int finalize_planned_column_temporal_scalar_expression_default(
    struct mylite_db *database,
    struct planned_column *column,
    const struct mylite_sql_ast_node *value_node
);
static int finalize_planned_column_type_default(
    struct mylite_db *database,
    struct planned_column *column
);
static int finalize_planned_column_string_default(
    struct mylite_db *database,
    struct planned_column *column
);
static int finalize_planned_column_string_default_value(
    struct mylite_db *database,
    struct planned_column *column,
    const struct mylite_sql_ast_node *value_node
);
static int finalize_planned_column_numeric_character_default(
    struct mylite_db *database,
    struct planned_column *column,
    const struct mylite_sql_ast_node *value_node
);
static bool column_default_value_is_numeric_character_literal(
    const struct mylite_sql_ast_node *value_node
);
static int allocate_numeric_character_default_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    char **out_text,
    size_t *out_text_length
);
static int copy_numeric_character_default_span(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    char **out_text,
    size_t *out_text_length
);
static int copy_numeric_character_default_bytes(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    char **out_text,
    size_t *out_text_length
);
static int finalize_planned_column_decimal_default(
    struct mylite_db *database,
    struct planned_column *column
);
static int finalize_planned_column_approximate_default(
    struct mylite_db *database,
    struct planned_column *column
);
static int finalize_planned_column_bit_default(
    struct mylite_db *database,
    struct planned_column *column
);
static int finalize_planned_column_binary_default(
    struct mylite_db *database,
    struct planned_column *column
);
static int finalize_planned_column_binary_blob_expression_default(
    struct mylite_db *database,
    struct planned_column *column,
    const struct mylite_sql_ast_node *value_node
);
static int finalize_planned_column_year_default(
    struct mylite_db *database,
    struct planned_column *column
);
static int finalize_planned_column_date_default(
    struct mylite_db *database,
    struct planned_column *column
);
static int finalize_planned_column_time_default(
    struct mylite_db *database,
    struct planned_column *column
);
static int finalize_planned_column_datetime_default(
    struct mylite_db *database,
    struct planned_column *column
);
static int finalize_planned_column_timestamp_default(
    struct mylite_db *database,
    struct planned_column *column
);
static int finalize_planned_column_enum_default(
    struct mylite_db *database,
    struct planned_column *column
);
static int finalize_planned_column_set_default(
    struct mylite_db *database,
    struct planned_column *column
);
static int finalize_planned_column_integer_expression_default(
    struct mylite_db *database,
    struct planned_column *column,
    const struct mylite_sql_ast_node *value_node
);
static int finalize_planned_column_bit_expression_default(
    struct mylite_db *database,
    struct planned_column *column,
    const struct mylite_sql_ast_node *value_node
);
static int finalize_planned_column_year_expression_default(
    struct mylite_db *database,
    struct planned_column *column,
    const struct mylite_sql_ast_node *value_node
);
static int finalize_planned_column_text_expression_default(
    struct mylite_db *database,
    struct planned_column *column,
    const struct mylite_sql_ast_node *value_node
);
static int finalize_planned_column_text_literal_default(
    struct mylite_db *database,
    struct planned_column *column
);
static int finalize_planned_column_character_expression_default(
    struct mylite_db *database,
    struct planned_column *column,
    const struct mylite_sql_ast_node *value_node
);
static int finalize_planned_column_concat_scalar_expression_default(
    struct mylite_db *database,
    struct planned_column *column,
    const struct mylite_sql_ast_node *expression
);
static int finalize_planned_column_parenthesized_string_literal_default(
    struct mylite_db *database,
    struct planned_column *column
);
static bool column_default_value_is_current_date_expression(
    const struct mylite_sql_ast_node *value_node
);
static bool column_default_value_is_current_time_expression(
    const struct mylite_sql_ast_node *value_node
);
static bool column_default_value_is_current_timestamp_expression(
    const struct mylite_sql_ast_node *value_node
);
static bool current_timestamp_default_should_preserve_now_text(
    const struct mylite_sql_ast_node *value_node
);
static bool column_default_value_is_parenthesized_text_expression(
    const struct mylite_sql_ast_node *default_node
);
static bool column_default_value_is_parenthesized_binary_blob_expression(
    const struct mylite_sql_ast_node *default_node
);
static void planned_column_descriptor_for_default(
    const struct planned_column *column,
    struct mylite_catalog_column_descriptor *out_descriptor
);
static int copy_planned_default_text(
    struct mylite_db *database,
    struct planned_column *column,
    struct planned_value *value
);
static int copy_planned_character_expression_default_text(
    struct mylite_db *database,
    struct planned_column *column,
    const char *text,
    size_t text_length
);
static int copy_planned_binary_default_text(
    struct mylite_db *database,
    struct planned_column *column,
    const struct planned_value *value
);
static int copy_planned_default_approximate_text(
    struct mylite_db *database,
    struct planned_column *column,
    const struct planned_value *value
);
static int copy_planned_integer_expression_default_text(
    struct mylite_db *database,
    struct planned_column *column,
    const struct mylite_sql_ast_node *expression
);
static int copy_planned_date_add_current_timestamp_default_text(
    struct mylite_db *database,
    struct planned_column *column,
    const struct mylite_sql_ast_node *expression
);
static int copy_planned_concat_scalar_expression_default_text(
    struct mylite_db *database,
    struct planned_column *column,
    const struct mylite_sql_ast_node *expression
);
static int append_concat_scalar_expression_default_text(
    struct mylite_db *database,
    struct planned_column *column,
    struct mylite_dynamic_string *string,
    const struct mylite_sql_ast_node *expression
);
static int append_concat_scalar_expression_argument_text(
    struct mylite_db *database,
    struct planned_column *column,
    struct mylite_dynamic_string *string,
    const struct mylite_sql_ast_node *argument
);
static int append_integer_expression_default_text(
    struct mylite_dynamic_string *string,
    const struct mylite_sql_ast_node *expression
);
static int append_integer_expression_default_text_item(
    struct mylite_dynamic_string *string,
    struct integer_expression_default_text_stack *stack,
    const struct integer_expression_default_text_work_item *item
);
static int append_integer_expression_default_node_text(
    struct mylite_dynamic_string *string,
    struct integer_expression_default_text_stack *stack,
    const struct mylite_sql_ast_node *expression
);
static int append_integer_expression_default_parenthesized_text(
    struct integer_expression_default_text_stack *stack,
    const struct mylite_sql_ast_node *expression
);
static int append_integer_expression_default_unary_text(
    struct integer_expression_default_text_stack *stack,
    const struct mylite_sql_ast_node *expression
);
static int append_integer_expression_default_binary_text(
    struct integer_expression_default_text_stack *stack,
    const struct mylite_sql_ast_node *expression
);
static int append_integer_expression_default_mod_text(
    struct integer_expression_default_text_stack *stack,
    const struct mylite_sql_ast_node *expression
);
static const char *integer_expression_default_binary_operator_text(
    enum mylite_sql_ast_operator operator_kind
);
static int push_integer_expression_default_text_item(
    struct integer_expression_default_text_stack *stack,
    enum integer_expression_default_text_work_item_kind kind,
    const struct mylite_sql_ast_node *expression,
    const char *text
);
static int append_integer_expression_default_literal_text(
    struct mylite_dynamic_string *string,
    const struct mylite_sql_ast_node *expression
);
static int convert_integer_expression_default_result(
    struct mylite_db *database,
    const struct planned_column *column,
    int64_t value,
    int64_t *out_value
);
static int convert_bit_expression_default_result(
    struct mylite_db *database,
    const struct planned_column *column,
    int64_t value,
    int64_t *out_value
);
static int convert_year_expression_default_result(int64_t value, int64_t *out_value);
static int convert_column_default_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct planned_column *column,
    int64_t *out_value
);
static int parse_column_default_integer_magnitude(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct planned_column *column,
    uint64_t *out_magnitude,
    bool *out_is_negative
);
static int finish_column_default_integer_value(
    struct mylite_db *database,
    const struct planned_column *column,
    const struct integer_column_range *range,
    uint64_t magnitude,
    bool is_negative,
    int64_t *out_value
);
static int parse_column_default_integer_string(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    uint64_t *out_magnitude,
    bool *out_is_negative
);
static int evaluate_integer_default_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_arithmetic_value *out_value
);
static int evaluate_integer_default_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_eval_stack *expression_stack,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct scalar_arithmetic_eval_frame *frame
);
static int evaluate_integer_default_apply_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack,
    enum mylite_sql_ast_operator operator_kind
);
static int evaluate_integer_default_apply_unary_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack,
    enum mylite_sql_ast_operator operator_kind
);
static int evaluate_integer_default_enter_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_eval_stack *expression_stack,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct mylite_sql_ast_node *expression
);
static int evaluate_integer_default_literal_enter_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct mylite_sql_ast_node *expression
);
static int evaluate_integer_default_unary_enter_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_eval_stack *expression_stack,
    struct scalar_arithmetic_value_stack *value_stack,
    const struct mylite_sql_ast_node *expression
);
static int evaluate_integer_default_mod_enter_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_eval_stack *expression_stack,
    const struct mylite_sql_ast_node *expression
);
static int evaluate_integer_default_binary_enter_frame(
    struct mylite_db *database,
    struct scalar_arithmetic_eval_stack *expression_stack,
    const struct mylite_sql_ast_node *expression
);
static bool integer_default_binary_operator_is_supported(enum mylite_sql_ast_operator operator_kind
);
static int parse_integer_default_literal_value(
    const struct mylite_sql_ast_node *literal,
    bool is_negative,
    struct scalar_arithmetic_value *out_value
);
static int finish_integer_default_result(
    struct scalar_arithmetic_value_stack *value_stack,
    struct scalar_arithmetic_value *out_value
);
static bool column_default_value_is_parenthesized_expression(
    const struct mylite_sql_ast_node *default_node
);
static int check_duplicate_column_names(
    struct mylite_db *database,
    const struct planned_column *columns,
    size_t column_count
);
static bool text_equals_ascii_case_insensitive(const char *left, const char *right);
static bool text_has_ascii_case_insensitive_prefix(const char *text, const char *prefix);
static char ascii_lower(unsigned char byte);
static bool enum_label_equals_ascii_case_insensitive(
    const char *left,
    size_t left_length,
    const char *right,
    size_t right_length
);
static int map_integer_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    const char **out_logical_type,
    const char **out_physical_type
);
static int map_column_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct planned_column *out_column
);
static bool ast_node_is_temporal_column_type(const struct mylite_sql_ast_node *type_node);
static int map_temporal_column_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct planned_column *out_column
);
static int map_varchar_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct planned_column *out_column
);
static int map_char_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    struct planned_column *out_column
);
static int append_national_character_set_warning(struct mylite_db *database);
static int map_text_family_type(
    struct mylite_db *database,
    const char *column_name,
    const struct mylite_sql_ast_node *type_node,
    struct planned_column *out_column
);
static int assign_text_family_descriptor_type(
    struct mylite_db *database,
    struct planned_column *out_column,
    const char *logical_type
);
static const char *text_family_logical_type_for_length(
    uint64_t character_length,
    uint64_t max_bytes_per_character
);
static uint64_t saturating_text_byte_requirement(
    uint64_t character_length,
    uint64_t max_bytes_per_character
);
static uint64_t max_bytes_per_character_for_charset(const char *charset_name);
static int map_json_type(struct planned_column *out_column);
static int map_spatial_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    struct planned_column *out_column
);
static int map_enum_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct planned_column *out_column
);
static int map_enum_label(
    struct mylite_db *database,
    const char *column_name,
    const struct mylite_sql_ast_node *label_node,
    size_t label_index,
    struct enum_type_info *info,
    struct mylite_dynamic_string *descriptor
);
static int decode_enum_label_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *label_node,
    char **out_text,
    size_t *out_text_length
);
static int check_enum_label_unique(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    const struct enum_type_info *info,
    const char *column_name
);
static int store_enum_label(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    size_t character_length,
    struct enum_type_info *info
);
static int append_enum_descriptor_label(
    struct mylite_dynamic_string *string,
    const char *text,
    size_t text_length,
    bool needs_comma
);
static int append_enum_quoted_label(
    struct mylite_dynamic_string *string,
    const char *text,
    size_t text_length
);
static int map_set_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct planned_column *out_column
);
static int map_set_member(
    struct mylite_db *database,
    const char *column_name,
    const struct mylite_sql_ast_node *member_node,
    size_t member_index,
    struct set_type_info *info,
    struct mylite_dynamic_string *descriptor
);
static int decode_set_member_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *member_node,
    char **out_text,
    size_t *out_text_length
);
static int check_set_member_unique(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    const struct set_type_info *info,
    const char *column_name
);
static bool set_member_contains_comma(const char *text, size_t text_length);
static int store_set_member(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    size_t character_length,
    struct set_type_info *info
);
static int map_binary_string_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct planned_column *out_column
);
static int map_bit_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct planned_column *out_column
);
static int map_year_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    struct planned_column *out_column
);
static int map_binary_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct planned_column *out_column
);
static int map_varbinary_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct planned_column *out_column
);
static int map_blob_family_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct planned_column *out_column
);
static int assign_binary_descriptor_type(
    struct mylite_db *database,
    struct planned_column *out_column,
    const char *format,
    uint64_t length
);
static int assign_bit_descriptor_type(
    struct mylite_db *database,
    struct planned_column *out_column,
    uint64_t length
);
static int map_decimal_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct planned_column *out_column
);
static int map_approximate_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct planned_column *out_column
);
static enum approximate_type_class approximate_type_class_for_type_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node
);
static int apply_approximate_type_precision(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct approximate_type_mapping_state *state
);
static int apply_approximate_type_scaled_precision(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    struct approximate_type_mapping_state *state
);
static int apply_approximate_type_float_precision(
    struct mylite_db *database,
    const char *column_name,
    struct approximate_type_mapping_state *state
);
static int append_approximate_unsigned_warning_if_needed(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node
);
static int assign_approximate_type_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const struct approximate_type_mapping_state *state,
    struct planned_column *out_column
);
static int map_integer_display_width(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    bool *out_has_display_width,
    uint64_t *out_display_width
);
static int append_integer_display_width_warning(struct mylite_db *database);
static int append_approximate_display_width_warning(struct mylite_db *database);
static int append_year_display_width_warning(struct mylite_db *database);
static const char *logical_type_for_mapped_integer(struct mapped_integer_type integer_type);
static bool logical_type_is_varchar(const char *logical_type);
static bool logical_type_is_char(const char *logical_type);
static bool logical_type_is_national_varchar(const char *logical_type);
static bool logical_type_is_national_char(const char *logical_type);
static bool logical_type_is_national_char_or_varchar(const char *logical_type);
static size_t char_descriptor_prefix_length(const char *logical_type);
static size_t char_descriptor_syntax_overhead(const char *logical_type);
static size_t varchar_descriptor_prefix_length(const char *logical_type);
static size_t varchar_descriptor_syntax_overhead(const char *logical_type);
static uint64_t logical_type_max_bytes_per_character(const char *logical_type);
static bool planned_column_is_varchar(const struct planned_column *column);
static bool planned_column_is_char(const struct planned_column *column);
static bool planned_column_is_char_or_varchar(const struct planned_column *column);
static bool planned_column_is_text_family(const struct planned_column *column);
static bool planned_column_is_string_family(const struct planned_column *column);
static bool planned_column_is_json(const struct planned_column *column);
static bool planned_column_is_spatial(const struct planned_column *column);
static bool planned_column_is_enum(const struct planned_column *column);
static bool planned_column_is_set(const struct planned_column *column);
static bool planned_column_is_binary_string_family(const struct planned_column *column);
static bool planned_column_is_binary_blob_family(const struct planned_column *column);
static bool planned_column_is_bit(const struct planned_column *column);
static bool planned_column_is_year(const struct planned_column *column);
static bool planned_column_is_decimal(const struct planned_column *column);
static bool planned_column_is_approximate(const struct planned_column *column);
static bool planned_column_is_date(const struct planned_column *column);
static bool planned_column_is_time(const struct planned_column *column);
static bool planned_column_is_datetime(const struct planned_column *column);
static bool planned_column_is_timestamp(const struct planned_column *column);
static bool column_default_kind_has_integer_value(
    enum mylite_catalog_column_default_kind default_kind
);
static bool column_default_kind_has_text_value(enum mylite_catalog_column_default_kind default_kind
);
static bool column_default_kind_is_expression(enum mylite_catalog_column_default_kind default_kind);
static bool column_default_kind_materializes_value(
    enum mylite_catalog_column_default_kind default_kind
);
static bool column_descriptor_physical_type_equals(
    const struct mylite_catalog_column_descriptor *column,
    const char *physical_type
);
static bool column_descriptor_is_varchar(const struct mylite_catalog_column_descriptor *column);
static bool column_descriptor_is_char(const struct mylite_catalog_column_descriptor *column);
static bool column_descriptor_is_national_char_or_varchar(
    const struct mylite_catalog_column_descriptor *column
);
static bool column_descriptor_is_char_or_varchar(
    const struct mylite_catalog_column_descriptor *column
);
static bool column_descriptor_is_text_family(const struct mylite_catalog_column_descriptor *column);
static bool column_descriptor_is_string_family(const struct mylite_catalog_column_descriptor *column
);
static bool column_descriptor_is_json(const struct mylite_catalog_column_descriptor *column);
static bool column_descriptor_is_spatial(const struct mylite_catalog_column_descriptor *column);
static bool spatial_logical_type_display_text(const char *logical_type, const char **out_type_text);
static bool column_descriptor_is_enum(const struct mylite_catalog_column_descriptor *column);
static bool column_descriptor_uses_enum_implicit_missing_default(
    const struct mylite_catalog_column_descriptor *column
);
static bool column_descriptor_is_set(const struct mylite_catalog_column_descriptor *column);
static bool column_descriptor_is_binary_string_family(
    const struct mylite_catalog_column_descriptor *column
);
static bool column_descriptor_is_binary_blob_family(
    const struct mylite_catalog_column_descriptor *column
);
static bool column_descriptor_is_bit(const struct mylite_catalog_column_descriptor *column);
static bool column_descriptor_is_year(const struct mylite_catalog_column_descriptor *column);
static bool column_descriptor_is_decimal(const struct mylite_catalog_column_descriptor *column);
static bool column_descriptor_is_approximate(const struct mylite_catalog_column_descriptor *column);
static bool column_descriptor_is_date(const struct mylite_catalog_column_descriptor *column);
static bool column_descriptor_is_time(const struct mylite_catalog_column_descriptor *column);
static bool column_descriptor_is_datetime(const struct mylite_catalog_column_descriptor *column);
static bool column_descriptor_is_timestamp(const struct mylite_catalog_column_descriptor *column);
static int decimal_type_info_for_logical_type(
    const char *logical_type,
    struct decimal_type_info *out_info
);
static int approximate_type_info_for_logical_type(
    const char *logical_type,
    struct approximate_type_info *out_info
);
static int approximate_display_type_info_for_logical_type(
    const char *logical_type,
    const char *prefix,
    enum approximate_type_class type_class,
    struct approximate_type_info *out_info
);
static int enum_type_info_for_logical_type(
    struct mylite_db *database,
    const char *logical_type,
    const char *unsupported_message,
    struct enum_type_info *out_info
);
static int parse_enum_descriptor_label(
    struct mylite_db *database,
    const char *logical_type,
    size_t end_index,
    size_t *index,
    struct enum_type_info *out_info
);
static int finish_parse_enum_descriptor_label(
    struct mylite_db *database,
    size_t label_start,
    struct enum_type_info *out_info
);
static int read_enum_descriptor_escaped_byte(
    struct mylite_db *database,
    const char *logical_type,
    size_t end_index,
    size_t *index,
    char *out_byte
);
static int enum_type_info_append_byte(
    struct mylite_db *database,
    struct enum_type_info *info,
    char byte
);
static bool enum_find_label(
    const struct enum_type_info *info,
    const char *text,
    size_t text_length,
    const struct enum_label_descriptor **out_label
);
static bool enum_find_ordinal(
    const struct enum_type_info *info,
    uint64_t ordinal,
    const struct enum_label_descriptor **out_label
);
static bool enum_text_is_unsigned_integer(
    const char *text,
    size_t text_length,
    uint64_t *out_value
);
static int set_type_info_for_logical_type(
    struct mylite_db *database,
    const char *logical_type,
    const char *unsupported_message,
    struct set_type_info *out_info
);
static int parse_set_descriptor_member(
    struct mylite_db *database,
    const char *logical_type,
    size_t end_index,
    size_t *index,
    struct set_type_info *out_info
);
static int finish_parse_set_descriptor_member(
    struct mylite_db *database,
    size_t member_start,
    struct set_type_info *out_info
);
static int read_set_descriptor_escaped_byte(
    struct mylite_db *database,
    const char *logical_type,
    size_t end_index,
    size_t *index,
    char *out_byte
);
static int set_type_info_append_byte(
    struct mylite_db *database,
    struct set_type_info *info,
    char byte
);
static bool set_find_member(
    const struct set_type_info *info,
    const char *text,
    size_t text_length,
    size_t *out_index
);
static bool set_bitmap_is_valid(const struct set_type_info *info, uint64_t bitmap);
static const struct text_family_type_info *text_family_type_info_for_logical_type(
    const char *logical_type
);
static const struct binary_string_type_info *binary_string_type_info_for_logical_type(
    const char *logical_type,
    struct binary_string_type_info *storage
);
static int parse_binary_descriptor_length(
    struct mylite_db *database,
    const char *logical_type,
    const char *unsupported_message,
    size_t *out_length
);
static int parse_varbinary_descriptor_length(
    struct mylite_db *database,
    const char *logical_type,
    const char *unsupported_message,
    size_t *out_length
);
static int bit_width_for_logical_type(const char *logical_type, uint64_t *out_width);
static size_t bit_byte_width(uint64_t width);
static uint64_t bit_width_max_value(uint64_t width);
static bool column_descriptor_is_auto_increment(
    const struct mylite_catalog_column_descriptor *column
);
static int validate_planned_table_row_size(
    struct mylite_db *database,
    const struct planned_column *columns,
    size_t column_count
);
static int validate_added_column_row_size(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const struct planned_column *added_column
);
static int add_planned_column_row_size(
    struct mylite_db *database,
    const struct planned_column *column,
    uint64_t *total
);
static int add_catalog_column_row_size(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    uint64_t *total
);
static int row_size_for_column_descriptor(
    struct mylite_db *database,
    const char *logical_type,
    const char *physical_type,
    const char *unsupported_message,
    uint64_t *out_size
);
static int binary_string_row_size_bytes(
    struct mylite_db *database,
    const char *logical_type,
    uint64_t *out_size,
    const char *unsupported_message
);
static int text_backed_row_size_bytes(
    struct mylite_db *database,
    const char *logical_type,
    uint64_t *out_size,
    const char *unsupported_message
);
static int set_row_size_bytes(
    struct mylite_db *database,
    const char *logical_type,
    uint64_t *out_size,
    const char *unsupported_message
);
static int integer_row_size_bytes(const char *logical_type, uint64_t *out_size);
static int decimal_row_size_bytes(const struct decimal_type_info *info, uint64_t *out_size);
static int approximate_row_size_bytes(const struct approximate_type_info *info, uint64_t *out_size);
static uint64_t decimal_digit_group_size(uint64_t digits);
static int add_row_size_bytes(struct mylite_db *database, uint64_t addition, uint64_t *total);
static int validate_planned_string_key_column(
    struct mylite_db *database,
    const struct planned_column *column
);
static int validate_descriptor_string_key_column(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
);
static int validate_string_key_value(struct mylite_db *database, const struct planned_value *value);
static bool loaded_index_part_requires_string_key_validation(const struct loaded_index_part *part);
static bool text_value_is_valid_string_key(const char *text, size_t text_length);
static bool text_value_is_supported_string_key(const char *text, size_t text_length);
static bool modify_column_integer_value_domain_matches(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
);
static bool modify_column_char_varchar_replacement_supported(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
);
static bool modify_column_text_family_replacement_supported(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
);
static bool modify_column_binary_string_replacement_supported(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
);
static bool column_is_nullable(const struct mylite_sql_ast_node *nullability_node);

static int reject_auto_increment_column_definition(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_definition,
    const char *message
);
static int plan_alter_table_rename_column_from_columns(
    struct mylite_db *database,
    struct planned_alter_table_rename_column *out_plan,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const char *old_column_name
);
static int find_column_index(
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const char *name,
    size_t *out_index
);
static int resolve_descriptor_column_reference(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct select_source_context *source_context,
    enum column_reference_diagnostic_context diagnostic_context,
    const char *unsupported_message,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column
);
static int resolve_descriptor_column_reference_with_source_index(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct select_source_context *source_context,
    enum column_reference_diagnostic_context diagnostic_context,
    const char *unsupported_message,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column,
    size_t *out_source_index
);
static int resolve_descriptor_column_reference_pointer_with_source_index(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct select_source_context *source_context,
    enum column_reference_diagnostic_context diagnostic_context,
    const char *unsupported_message,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct mylite_catalog_column_descriptor **out_column,
    size_t *out_source_index
);
static int resolve_joined_descriptor_column_reference_pointer(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct select_source_context *source_context,
    enum column_reference_diagnostic_context diagnostic_context,
    const char *unsupported_message,
    const struct mylite_catalog_column_descriptor **out_column,
    size_t *out_source_index
);
static bool column_reference_source_matches_parts(
    const struct planned_select_source *source,
    char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t part_count
);
static int collect_column_reference_parts(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t *out_part_count
);
static int format_column_reference_name(
    struct mylite_db *database,
    char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t part_count,
    char *destination,
    size_t destination_size
);
static bool column_reference_qualifier_matches_source(
    char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t part_count,
    const struct select_source_context *source_context
);
static void set_unknown_column_reference_error(
    struct mylite_db *database,
    enum column_reference_diagnostic_context context,
    const char *column_name
);
static size_t count_visible_columns(
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count
);
