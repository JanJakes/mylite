static int execute_create_table_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_create_temporary_table_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_create_table_like_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_create_temporary_table_like_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_create_table_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_create_temporary_table_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_create_view_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_view_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_create_index_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_create_schema_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_schema_default_charset_collation_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int maybe_finish_create_schema_if_not_exists_noop(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result *result,
    bool *out_finished
);
static bool create_schema_has_if_not_exists(const struct mylite_sql_ast_node *statement);
static int execute_drop_table_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_drop_temporary_table_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_drop_view_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int plan_drop_table(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool temporary_only,
    struct planned_drop_table *out_plan
);
static int plan_drop_view(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_drop_table *out_plan
);
static int execute_drop_view_from_plan(
    struct mylite_db *database,
    const struct planned_drop_table *plan
);
static int plan_drop_table_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *target_node,
    struct planned_drop_table *out_plan,
    size_t target_index
);
static int check_drop_table_duplicate_targets(
    struct mylite_db *database,
    const struct planned_drop_table *plan,
    size_t target_index
);
static bool drop_table_targets_match(
    const struct planned_drop_table_target *left,
    const struct planned_drop_table_target *right
);
static int finish_drop_table_missing_targets(
    struct mylite_db *database,
    const struct planned_drop_table *plan
);
static bool drop_table_plan_requires_implicit_commit(const struct planned_drop_table *plan);
static int append_drop_table_missing_notes(
    struct mylite_db *database,
    const struct planned_drop_table *plan
);
static void planned_drop_table_deinit(struct planned_drop_table *plan);
static int execute_drop_table_from_plan(
    struct mylite_db *database,
    const struct planned_drop_table *plan
);
static bool planned_drop_table_has_persistent_targets(const struct planned_drop_table *plan);
static int delete_drop_table_persistent_catalog_rows(
    struct mylite_db *database,
    const struct planned_drop_table *plan,
    struct mylite_catalog_mutation *mutation
);
static int drop_existing_physical_tables(
    struct mylite_db *database,
    const struct planned_drop_table *plan
);
static int remove_drop_table_temporary_descriptors(
    struct mylite_db *database,
    const struct planned_drop_table *plan
);
static int execute_drop_schema_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int maybe_finish_drop_schema_if_exists_noop(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool *out_finished
);
static bool drop_schema_has_if_exists(const struct mylite_sql_ast_node *statement);
static int execute_truncate_table_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_rename_table_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_rename_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_add_column_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_multi_action_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_multi_action(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node,
    const struct mylite_sql_ast_node *action,
    const struct mylite_catalog_mutation *mutation,
    struct alter_table_multi_action_state *state,
    bool *out_physical_schema_changed
);
static int execute_alter_table_multi_action_add_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node,
    const struct mylite_sql_ast_node *action,
    const struct mylite_catalog_mutation *mutation,
    struct alter_table_multi_action_state *state,
    bool *out_physical_schema_changed
);
static int execute_alter_table_multi_action_add_index(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node,
    const struct mylite_sql_ast_node *action,
    const struct mylite_catalog_mutation *mutation,
    bool *out_physical_schema_changed
);
static int execute_alter_table_multi_action_add_primary_key(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node,
    const struct mylite_sql_ast_node *action,
    const struct mylite_catalog_mutation *mutation,
    bool *out_physical_schema_changed
);
static int execute_alter_table_multi_action_drop_index(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node,
    const struct mylite_sql_ast_node *action,
    const struct mylite_catalog_mutation *mutation,
    bool *out_physical_schema_changed
);
static int execute_alter_table_multi_action_drop_primary_key(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node,
    const struct mylite_sql_ast_node *action,
    const struct mylite_catalog_mutation *mutation,
    bool *out_physical_schema_changed
);
static int execute_alter_table_multi_action_modify_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node,
    const struct mylite_sql_ast_node *action,
    const struct mylite_catalog_mutation *mutation,
    bool *out_physical_schema_changed
);
static int execute_alter_table_multi_action_change_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node,
    const struct mylite_sql_ast_node *action,
    const struct mylite_catalog_mutation *mutation,
    bool *out_physical_schema_changed
);
static int execute_alter_table_multi_action_set_default(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node,
    const struct mylite_sql_ast_node *action,
    const struct mylite_catalog_mutation *mutation,
    const struct alter_table_multi_action_state *state
);
static int execute_alter_table_multi_action_drop_default(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node,
    const struct mylite_sql_ast_node *action,
    const struct mylite_catalog_mutation *mutation,
    const struct alter_table_multi_action_state *state
);
static int validate_alter_table_multi_action_final_state(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node
);
static int validate_alter_table_multi_action_auto_increment_keys(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table
);
static bool table_index_infos_have_leading_column(
    const struct alter_table_multi_auto_increment_key_lookup *lookup
);
static void make_alter_table_action_statement_view(
    const struct mylite_sql_ast_node *table_node,
    struct alter_table_action_statement_view *out_view,
    const struct mylite_sql_ast_node *action
);
static int reject_warning_producing_multi_action_add_column(
    struct mylite_db *database,
    const struct planned_alter_table_add_column *plan
);
static int reject_unsupported_multi_action_add_index(
    struct mylite_db *database,
    const struct planned_alter_table_add_index *plan
);
static int reject_unsupported_multi_action_add_primary_key(
    struct mylite_db *database,
    const struct planned_alter_table_add_primary_key *plan
);
static int reject_multi_action_default_target_added_in_statement(
    struct mylite_db *database,
    const struct alter_table_multi_default_target_view *target,
    const struct alter_table_multi_action_state *state
);
static int reject_alter_table_multi_action_temporary_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node
);
static bool multi_action_state_has_added_column(
    const struct alter_table_multi_action_state *state,
    const char *column_name
);
static int copy_multi_action_default_target_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *action,
    char *out_name,
    size_t out_size
);
static int copy_multi_action_table_leaf_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node,
    char *out_name,
    size_t out_size
);
static int append_multi_action_added_column(
    struct mylite_db *database,
    struct alter_table_multi_action_state *state,
    const char *column_name
);
static int reserve_multi_action_added_columns(
    struct mylite_db *database,
    struct alter_table_multi_action_state *state,
    size_t required_capacity
);
static void alter_table_multi_action_state_deinit(struct alter_table_multi_action_state *state);
static int execute_alter_table_add_primary_key_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_add_index_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_add_foreign_key_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_drop_foreign_key_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_drop_constraint_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_drop_index_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_rename_index_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_add_check_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_drop_check_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_alter_check_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_drop_index_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_drop_primary_key_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_auto_increment_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_drop_column_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_rename_column_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_modify_column_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_change_column_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_set_default_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_drop_default_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_column_visibility_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_index_visibility_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_default_charset_collation_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_convert_character_set_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_comment_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_storage_statistics_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_order_by_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_force_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_disable_keys_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_enable_keys_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_alter_table_key_maintenance_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int validate_alter_table_algorithm_lock_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
);
static int validate_alter_table_algorithm_lock_option_values(
    struct mylite_db *database,
    enum mylite_sql_ast_alter_algorithm algorithm,
    enum mylite_sql_ast_alter_lock lock
);
static int validate_alter_table_column_algorithm_options(
    struct mylite_db *database,
    enum mylite_sql_ast_alter_algorithm algorithm
);
static int validate_alter_table_add_index_algorithm_lock_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    enum mylite_sql_ast_alter_algorithm algorithm,
    enum mylite_sql_ast_alter_lock lock
);
static int validate_alter_table_online_metadata_algorithm_options(
    struct mylite_db *database,
    enum mylite_sql_ast_alter_algorithm algorithm
);
static int validate_alter_table_add_foreign_key_algorithm_lock_options(
    struct mylite_db *database,
    enum mylite_sql_ast_alter_algorithm algorithm,
    enum mylite_sql_ast_alter_lock lock
);
static int validate_alter_table_rebuild_algorithm_options(
    struct mylite_db *database,
    enum mylite_sql_ast_alter_algorithm algorithm
);
static int validate_alter_table_key_maintenance_algorithm_lock_options(
    struct mylite_db *database,
    enum mylite_sql_ast_alter_algorithm algorithm,
    enum mylite_sql_ast_alter_lock lock
);
static bool alter_table_statement_accepts_algorithm_lock_options(enum mylite_sql_ast_node_kind kind
);
static bool alter_table_add_index_statement_is_fulltext(const struct mylite_sql_ast_node *statement
);
static int execute_insert_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_replace_values_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int append_insert_delayed_warning_if_needed(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
);
static int append_replace_delayed_warning_if_needed(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
);
static int execute_planned_insert_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_insert_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_replace_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_planned_insert_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_insert_set_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_replace_set_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_planned_insert_set_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_load_data_infile_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_delete_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_update_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_prepared_insert_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_prepared_delete_statement(
    struct mylite_db *database,
    mylite_result **out_result
);
static int execute_prepared_update_statement(
    struct mylite_db *database,
    mylite_result **out_result
);
static int append_update_assignment_warnings(
    struct mylite_db *database,
    const struct planned_update *plan
);
static int execute_do_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_explain_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
