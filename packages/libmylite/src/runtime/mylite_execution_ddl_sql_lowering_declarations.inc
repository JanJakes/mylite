/* Private declarations for the DDL SQL-lowering implementation fragments. */

static int append_create_table_definition_sql(
    struct mylite_dynamic_string *string,
    const struct planned_create_table *plan,
    const char *physical_name,
    bool temporary
);
static int append_create_table_columns_sql(
    struct mylite_dynamic_string *string,
    const struct planned_create_table *plan
);
static int append_create_table_check_constraints_sql(
    struct mylite_dynamic_string *string,
    const struct planned_create_table *plan
);
static int append_create_table_generated_column_constraints_sql(
    struct mylite_dynamic_string *string,
    const struct planned_create_table *plan
);
static int append_generated_column_range_check_constraint_sql(
    struct mylite_dynamic_string *string,
    const struct planned_column *column,
    size_t column_index
);
static int append_generated_column_range_check_constraint_name(
    struct mylite_dynamic_string *string,
    size_t column_index
);
static int append_create_table_primary_key_sql(
    struct mylite_dynamic_string *string,
    const struct planned_create_table *plan,
    const char *physical_name
);
static int append_create_table_secondary_indexes_sql(
    struct mylite_dynamic_string *string,
    const struct planned_create_table *plan,
    const char *physical_name
);
static int append_create_table_index_sql(
    struct mylite_dynamic_string *string,
    bool is_unique,
    const char *index_physical_name,
    const char *table_physical_name
);
static int append_create_table_index_sql_close(struct mylite_dynamic_string *string);
static int append_planned_key_part_sql(
    struct mylite_dynamic_string *string,
    const struct planned_create_table *plan,
    const struct planned_column *column
);
static int append_planned_secondary_key_part_sql(
    struct mylite_dynamic_string *string,
    const struct planned_create_table *plan,
    const struct planned_secondary_index_part *part
);
static int append_loaded_prefix_key_part_sql(
    struct mylite_dynamic_string *string,
    const struct loaded_index_part *part,
    const char *qualifier
);
static bool planned_column_uses_string_key_collation(
    const struct planned_create_table *plan,
    const struct planned_column *column,
    bool include_text_family
);
static bool string_key_collation_name_is_binary(const char *name);
static int append_named_string_key_collation_sql(
    struct mylite_dynamic_string *string,
    const char *collation_name
);
static int append_create_unique_index_string_validation_select_list_sql(
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_index *plan
);
static int append_create_unique_index_string_validation_where_sql(
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_index *plan
);
static int append_loaded_key_part_list_sql(
    struct mylite_dynamic_string *string,
    const struct loaded_index_part *parts,
    size_t part_count
);
static int append_loaded_key_part_not_null_filter_sql(
    struct mylite_dynamic_string *string,
    const struct loaded_index_part *parts,
    size_t part_count
);
static int append_alter_table_primary_key_expression_list_sql(
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_primary_key *plan
);
static int append_alter_table_order_column_list(
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_order_by *plan
);
static int append_alter_table_order_order_list(
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_order_by *plan
);
static int append_alter_table_force_column_list(
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_force *plan
);
