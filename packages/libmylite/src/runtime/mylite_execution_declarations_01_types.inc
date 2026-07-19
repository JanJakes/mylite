enum table_maintenance_operation {
    TABLE_MAINTENANCE_ANALYZE = 0,
    TABLE_MAINTENANCE_CHECK = 1,
    TABLE_MAINTENANCE_OPTIMIZE = 2,
    TABLE_MAINTENANCE_REPAIR = 3,
};

enum checksum_table_option {
    CHECKSUM_TABLE_OPTION_DEFAULT = 0,
    CHECKSUM_TABLE_OPTION_QUICK = 1,
    CHECKSUM_TABLE_OPTION_EXTENDED = 2,
};

struct show_binlog_events_options {
    bool include_placeholder_row;
};

struct planned_lock_tables {
    struct mylite_session_table_lock *locks;
    size_t lock_count;
};

struct table_maintenance_target {
    char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char table_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char display_name[(MYLITE_CATALOG_IDENTIFIER_CAPACITY * 2) + 2];
    struct mylite_catalog_table_descriptor table;
    bool missing_schema;
    bool missing_table;
    bool unsupported_kind;
    bool has_table;
};

struct transaction_characteristics {
    bool has_isolation;
    bool has_access_mode;
    bool has_consistent_snapshot;
    enum mylite_transaction_isolation isolation;
    enum mylite_transaction_access_mode access_mode;
};

struct savepoint_control_sql_request {
    const char *prefix;
    const char *sqlite_name;
};

struct user_savepoint_values {
    const char *name;
    const char *sqlite_name;
};

struct planned_compound_select;
struct planned_select_source;
struct planned_exists_subquery;
struct planned_in_subquery;
struct planned_scalar_aggregate_subquery;

struct select_source_context {
    const struct table_name_resolution *source;
    char alias[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    bool has_alias;
    bool allow_insert_values_reference;
    const struct planned_select_source *sources;
    size_t source_count;
};

struct select_index_hint_lookup_context {
    const char *name;
    bool exact_match;
    size_t prefix_match_count;
};

enum temporal_text_kind {
    TEMPORAL_TEXT_NORMAL = 0,
    TEMPORAL_TEXT_FULL_ZERO = 1,
    TEMPORAL_TEXT_PARTIAL_ZERO = 2,
    TEMPORAL_TEXT_ALLOW_INVALID_CANDIDATE = 3,
    TEMPORAL_TEXT_INVALID = 4,
    TEMPORAL_TEXT_DEFERRED = 5,
};

enum temporal_storage_truncation {
    TEMPORAL_STORAGE_TRUNCATION_NONE = 0,
    TEMPORAL_STORAGE_TRUNCATION_NOTE = 1,
    TEMPORAL_STORAGE_TRUNCATION_WARNING = 2,
};

struct temporal_predicate_normalization_input;

struct year_string_trailing_hyphen_request {
    size_t index;
    unsigned char byte;
    uint64_t magnitude;
};

struct year_string_magnitude_conversion {
    uint64_t magnitude;
    size_t text_length;
};

enum planned_index_type_option {
    PLANNED_INDEX_TYPE_DEFAULT = 0,
    PLANNED_INDEX_TYPE_BTREE = 1,
    PLANNED_INDEX_TYPE_HASH = 2,
    PLANNED_INDEX_TYPE_RTREE = 3,
};

struct planned_column {
    char name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char logical_type_storage[MYLITE_CATALOG_TYPE_NAME_CAPACITY];
    char physical_type_storage[MYLITE_CATALOG_TYPE_NAME_CAPACITY];
    const char *logical_type;
    const char *physical_type;
    enum mylite_sql_ast_nullability nullability;
    bool is_nullable;
    bool is_visible;
    bool is_primary_key;
    bool is_unique_key;
    bool is_auto_increment;
    bool is_serial_alias;
    bool on_update_current_timestamp;
    bool has_text_length_argument;
    uint64_t text_length_argument;
    const struct mylite_sql_ast_node *default_node;
    enum mylite_catalog_column_default_kind default_kind;
    int64_t default_integer;
    char default_text[MYLITE_CATALOG_DEFAULT_TEXT_CAPACITY];
    char character_set_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char collation_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    bool has_binary_collation_attribute;
    char comment[MYLITE_CATALOG_COLUMN_COMMENT_CAPACITY];
    bool is_generated;
    enum mylite_catalog_generated_column_kind generated_kind;
    const struct mylite_sql_ast_node *generation_expression_node;
    char generation_expression[MYLITE_CATALOG_GENERATION_EXPRESSION_CAPACITY];
    char sqlite_generation_expression[MYLITE_CATALOG_GENERATION_EXPRESSION_CAPACITY];
};

struct planned_secondary_index_part {
    size_t column_index;
    bool has_prefix_length;
    int64_t prefix_length;
    enum mylite_catalog_index_sort_direction sort_direction;
};

struct planned_secondary_index {
    char name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    struct planned_secondary_index_part *parts;
    size_t part_count;
    size_t part_capacity;
    int64_t index_id;
    char physical_name[MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY];
    enum mylite_catalog_index_kind kind;
    bool is_unique;
    bool is_visible;
    char comment[MYLITE_CATALOG_INDEX_COMMENT_CAPACITY];
    bool show_create_explicit_btree;
    bool uses_hash_index_type;
};

struct secondary_index_part_nodes {
    const struct mylite_sql_ast_node *prefix_node;
    const struct mylite_sql_ast_node *direction_node;
};

struct planned_primary_key_part {
    size_t column_index;
    enum mylite_catalog_index_sort_direction sort_direction;
};

struct planned_foreign_key_part {
    size_t child_column_index;
    size_t parent_column_index;
    struct mylite_catalog_column_descriptor parent_column;
};

struct planned_foreign_key {
    char name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char child_index_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char update_rule[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char delete_rule[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    struct planned_foreign_key_part *parts;
    size_t part_count;
    size_t part_capacity;
    int64_t foreign_key_id;
    struct mylite_catalog_table_descriptor parent_table;
    struct mylite_catalog_index_descriptor parent_index;
    int64_t child_index_id;
    size_t child_secondary_index_index;
    size_t parent_secondary_index_index;
    bool child_index_is_primary;
    bool parent_is_self_reference;
    bool parent_index_is_primary;
    bool has_explicit_name;
};

struct planned_check_constraint {
    char name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char physical_name[MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY];
    char check_clause[MYLITE_CATALOG_CHECK_CLAUSE_CAPACITY];
    char sqlite_expression[MYLITE_CATALOG_CHECK_CLAUSE_CAPACITY];
    int64_t check_constraint_id;
    int64_t generated_ordinal;
    int64_t ordinal_position;
    bool is_enforced;
    bool name_is_generated;
};

struct create_table_check_constraint_definition {
    const struct mylite_sql_ast_node *check_constraint;
    const struct mylite_sql_ast_node *inline_enforcement;
    const struct planned_column *inline_column;
    size_t inline_column_index;
};

enum check_expression_render_work_item_kind {
    CHECK_EXPRESSION_RENDER_NODE = 0,
    CHECK_EXPRESSION_RENDER_TEXT = 1,
    CHECK_EXPRESSION_RENDER_CHAR = 2,
    CHECK_EXPRESSION_RENDER_OPERATOR = 3,
};

struct check_expression_render_work_item {
    enum check_expression_render_work_item_kind kind;
    const struct mylite_sql_ast_node *node;
    bool require_boolean;
    const char *text;
    char character;
    enum mylite_sql_ast_operator operator_kind;
};

enum generated_expression_render_work_item_kind {
    GENERATED_EXPRESSION_RENDER_NODE = 0,
    GENERATED_EXPRESSION_RENDER_TEXT = 1,
    GENERATED_EXPRESSION_RENDER_CHAR = 2,
};

struct generated_expression_render_work_item {
    enum generated_expression_render_work_item_kind kind;
    const struct mylite_sql_ast_node *node;
    const char *text;
    char character;
};

struct check_expression_render_context {
    struct mylite_db *database;
    const struct planned_create_table *plan;
    const struct planned_column *inline_column;
    size_t inline_column_index;
    const char *alter_check_constraint_name;
    struct mylite_dynamic_string *check_clause;
    struct mylite_dynamic_string *sqlite_expression;
};

struct generated_expression_render_context {
    struct mylite_db *database;
    const struct planned_create_table *plan;
    size_t generated_column_index;
    struct mylite_dynamic_string *generation_expression;
    struct mylite_dynamic_string *sqlite_expression;
};

struct planned_create_table {
    struct table_name_resolution target;
    struct planned_column *columns;
    size_t column_count;
    bool has_primary_key;
    struct planned_primary_key_part *primary_key_parts;
    size_t primary_key_part_count;
    size_t primary_key_part_capacity;
    int64_t primary_key_index_id;
    char primary_key_physical_name[MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY];
    char primary_key_comment[MYLITE_CATALOG_INDEX_COMMENT_CAPACITY];
    bool primary_key_show_create_explicit_btree;
    bool primary_key_uses_hash_index_type;
    struct planned_secondary_index *secondary_indexes;
    size_t secondary_index_count;
    size_t secondary_index_capacity;
    struct planned_foreign_key *foreign_keys;
    size_t foreign_key_count;
    size_t foreign_key_capacity;
    struct planned_check_constraint *check_constraints;
    size_t check_constraint_count;
    size_t check_constraint_capacity;
    int64_t auto_increment_next;
    char default_charset[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char default_collation[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char comment[MYLITE_CATALOG_TABLE_COMMENT_CAPACITY];
    char row_format_option[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int64_t key_block_size;
    int64_t pack_keys;
    int64_t checksum;
    int64_t stats_persistent;
    int64_t stats_auto_recalc;
    int64_t stats_sample_pages;
    int64_t min_rows;
    int64_t max_rows;
    int64_t avg_row_length;
    int64_t delay_key_write;
    bool suppress_spatial_index_warnings;
};

struct schema_default_option_validation {
    char charset_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char collation_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char first_charset_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char unsupported_charset_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char unsupported_collation_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    bool has_charset;
    bool has_collation;
};

struct temporary_index_descriptor_positions {
    size_t index;
    size_t index_column;
};

struct planned_create_table_like {
    struct planned_create_table create_table;
    struct table_name_resolution source;
    struct mylite_catalog_table_descriptor source_table;
};

struct planned_rename_table {
    struct table_name_resolution source;
    struct table_name_resolution target;
};

struct planned_rename_table_statement {
    struct planned_rename_table *pairs;
    size_t pair_count;
};

struct zero_temporal_default_warning {
    char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
};

struct zero_temporal_default_warnings {
    struct zero_temporal_default_warning *items;
    size_t count;
    size_t capacity;
};

struct zero_temporal_default_warning_filter {
    int64_t skipped_column_id;
};

struct planned_alter_table_add_column {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct planned_column column;
    struct mylite_catalog_column_descriptor *columns;
    struct zero_temporal_default_warnings zero_temporal_default_warnings;
    size_t column_count;
    size_t target_column_index;
    bool changes_position;
    bool adds_inline_primary_key;
    bool adds_inline_unique_key;
    bool defer_auto_increment_key_validation;
    char after_column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
};

struct add_inline_key_descriptor_request {
    const struct mylite_catalog_column_descriptor *source_columns;
    size_t source_column_count;
    size_t target_column_index;
    const struct mylite_catalog_column_descriptor *target_column;
};

struct alter_table_action_statement_view {
    struct mylite_sql_ast_node table_proxy;
    struct mylite_sql_ast_node statement;
};

struct alter_table_multi_added_column {
    char name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
};

struct alter_table_multi_action_state {
    struct alter_table_multi_added_column *added_columns;
    size_t added_column_count;
    size_t added_column_capacity;
};

struct alter_table_multi_default_target_view {
    const struct mylite_sql_ast_node *table_node;
    const struct mylite_sql_ast_node *action;
};

struct alter_table_multi_auto_increment_key_lookup {
    const struct loaded_index_info *indexes;
    size_t index_count;
    int64_t column_id;
};

struct planned_alter_table_add_primary_key {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct loaded_index_part *parts;
    size_t part_count;
    size_t part_capacity;
    const char *rowid_alias;
    char comment[MYLITE_CATALOG_INDEX_COMMENT_CAPACITY];
    bool show_create_explicit_btree;
    bool uses_hash_index_type;
};

struct planned_alter_table_add_index {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct loaded_index_part *parts;
    size_t part_count;
    size_t part_capacity;
    char index_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    const char *rowid_alias;
    enum mylite_catalog_index_kind kind;
    bool is_unique;
    bool is_visible;
    char comment[MYLITE_CATALOG_INDEX_COMMENT_CAPACITY];
    bool show_create_explicit_btree;
    bool uses_hash_index_type;
    int64_t affected_rows;
};

struct planned_alter_table_add_foreign_key {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    char name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char child_index_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char update_rule[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char delete_rule[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    struct mylite_catalog_table_descriptor parent_table;
    struct mylite_catalog_index_descriptor parent_index;
    struct mylite_catalog_index_descriptor child_index;
    struct loaded_foreign_key_part *parts;
    size_t part_count;
    size_t part_capacity;
    struct planned_alter_table_add_index child_index_plan;
    bool create_child_index;
    bool has_explicit_name;
};

enum planned_alter_table_check_constraint_action {
    PLANNED_ALTER_TABLE_CHECK_ADD = 0,
    PLANNED_ALTER_TABLE_CHECK_DROP = 1,
    PLANNED_ALTER_TABLE_CHECK_ALTER = 2,
};

struct planned_alter_table_check_constraint {
    enum planned_alter_table_check_constraint_action action;
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct planned_create_table rebuild;
    int64_t target_check_constraint_id;
    size_t check_constraint_index;
    bool has_check_constraint_index;
    bool target_enforced;
    bool catalog_change_required;
    bool physical_rebuild_required;
    bool validating_rebuild;
    int64_t affected_rows;
};

struct planned_alter_table_drop_foreign_key {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct loaded_foreign_key_info foreign_key;
};

enum planned_alter_table_drop_constraint_kind {
    PLANNED_ALTER_TABLE_DROP_CONSTRAINT_NONE = 0,
    PLANNED_ALTER_TABLE_DROP_CONSTRAINT_PRIMARY_KEY = 1,
    PLANNED_ALTER_TABLE_DROP_CONSTRAINT_UNIQUE_INDEX = 2,
    PLANNED_ALTER_TABLE_DROP_CONSTRAINT_FOREIGN_KEY = 3,
    PLANNED_ALTER_TABLE_DROP_CONSTRAINT_CHECK = 4,
};

struct planned_drop_index {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct loaded_index_info index;
    int64_t affected_rows;
};

struct planned_rename_index {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct loaded_index_info index;
    char new_index_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
};

struct index_option_nodes {
    const struct mylite_sql_ast_node *index_type_node;
    const struct mylite_sql_ast_node *option_list_node;
};

struct alter_table_add_index_nodes {
    const struct mylite_sql_ast_node *index_name_node;
    struct index_option_nodes options;
    const struct mylite_sql_ast_node *part_list;
    enum mylite_catalog_index_kind kind;
    bool is_unique;
};

struct create_index_nodes {
    const struct mylite_sql_ast_node *index_name_node;
    const struct mylite_sql_ast_node *table_name_node;
    struct index_option_nodes options;
    const struct mylite_sql_ast_node *part_list_node;
    enum mylite_catalog_index_kind kind;
};

struct planned_index_options {
    enum planned_index_type_option type;
    bool is_visible;
    char comment[MYLITE_CATALOG_INDEX_COMMENT_CAPACITY];
};

struct primary_key_definition_nodes {
    struct index_option_nodes options;
    const struct mylite_sql_ast_node *part_list;
};

struct create_table_secondary_index_definition_nodes {
    const struct mylite_sql_ast_node *index_name_node;
    struct index_option_nodes options;
    const struct mylite_sql_ast_node *part_list;
};

struct create_table_secondary_index_first_part {
    size_t column_index;
    size_t part_count;
};

struct foreign_key_definition_nodes {
    const struct mylite_sql_ast_node *constraint_name_node;
    const struct mylite_sql_ast_node *index_name_node;
    const struct mylite_sql_ast_node *child_parts_node;
    const struct mylite_sql_ast_node *parent_table_node;
    const struct mylite_sql_ast_node *parent_parts_node;
    const struct mylite_sql_ast_node *action_list_node;
};

struct foreign_key_part_name {
    char name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
};

struct foreign_key_column_names {
    struct foreign_key_part_name *child_parts;
    struct foreign_key_part_name *parent_parts;
    size_t child_part_count;
    size_t parent_part_count;
};

struct alter_table_add_foreign_key_child_metadata {
    struct mylite_catalog_column_descriptor *columns;
    struct loaded_index_info *indexes;
    struct loaded_foreign_key_info *foreign_keys;
    size_t column_count;
    size_t index_count;
    size_t foreign_key_count;
};

struct drop_index_nodes {
    const struct mylite_sql_ast_node *table_name_node;
    const struct mylite_sql_ast_node *index_name_node;
};

struct planned_alter_table_drop_primary_key {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct mylite_catalog_index_descriptor primary_key;
    struct loaded_index_part *parts;
    size_t part_count;
    int64_t affected_rows;
};

struct planned_alter_table_drop_constraint {
    enum planned_alter_table_drop_constraint_kind kind;
    struct planned_alter_table_drop_primary_key primary_key;
    struct planned_drop_index unique_index;
    struct planned_alter_table_drop_foreign_key foreign_key;
    struct planned_alter_table_check_constraint check_constraint;
};

struct alter_table_drop_constraint_context {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct mylite_catalog_column_descriptor *columns;
    struct loaded_index_info *indexes;
    struct loaded_foreign_key_info *foreign_keys;
    struct loaded_check_constraint_info *check_constraints;
    size_t column_count;
    size_t index_count;
    size_t foreign_key_count;
    size_t check_constraint_count;
};

struct alter_table_drop_constraint_resolution {
    enum planned_alter_table_drop_constraint_kind kind;
    size_t match_count;
    size_t unique_index;
};

struct planned_alter_table_auto_increment {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct mylite_catalog_column_descriptor column;
    int64_t requested_next;
    int64_t effective_next;
    bool has_auto_increment_column;
    bool is_temporary;
};

struct planned_drop_column_index_update {
    struct mylite_catalog_index_descriptor index;
    struct loaded_index_part *remaining_parts;
    size_t remaining_part_count;
    int64_t removed_ordinal_position;
    bool drops_index;
};

struct planned_alter_table_drop_column {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct mylite_catalog_column_descriptor column;
    struct planned_drop_column_index_update *index_updates;
    size_t index_update_count;
    size_t index_update_capacity;
    const char *rowid_alias;
    int64_t affected_rows;
    size_t column_count;
    bool removes_one_part_primary_key;
};

struct planned_alter_table_rename_column {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct mylite_catalog_column_descriptor column;
    char new_column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    bool is_noop;
};

struct planned_alter_table_modify_column {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct mylite_catalog_column_descriptor original_column;
    struct planned_column column;
    char lookup_column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    struct mylite_catalog_column_descriptor *columns;
    size_t column_count;
    size_t column_index;
    size_t target_column_index;
    bool has_position;
    bool position_first;
    bool changes_position;
    char after_column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    bool is_noop;
    bool is_metadata_only;
    bool is_temporary;
    bool checks_duplicate_replacement;
    bool reports_rebuild_row_count;
    bool adds_inline_primary_key;
    bool adds_inline_unique_key;
    bool defer_auto_increment_key_validation;
    const char *unsupported_object_message;
    const char *rowid_alias_message;
    const char *integer_support_message;
    const char *row_count_overflow_message;
    const char *failure_message;
    const char *rowid_alias;
    struct loaded_index_info *indexes;
    struct zero_temporal_default_warnings zero_temporal_default_warnings;
    size_t index_count;
    int64_t affected_rows;
};

struct planned_alter_table_set_default {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct mylite_catalog_column_descriptor original_column;
    struct planned_column column;
    struct zero_temporal_default_warnings zero_temporal_default_warnings;
};

struct planned_alter_table_drop_default {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct mylite_catalog_column_descriptor column;
};

struct planned_alter_table_column_visibility {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct mylite_catalog_column_descriptor column;
    bool is_visible;
};

struct planned_alter_table_index_visibility {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct loaded_index_info index;
    bool is_visible;
    int64_t affected_rows;
};

struct planned_alter_table_default_charset_collation {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    char default_charset[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char default_collation[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    bool changes_descriptor;
};

struct planned_alter_table_convert_character_set {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct mylite_catalog_column_descriptor *columns;
    size_t column_count;
    char default_charset[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char default_collation[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    bool changes_descriptor;
};

struct planned_alter_table_comment {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    char comment[MYLITE_CATALOG_TABLE_COMMENT_CAPACITY];
};

struct planned_alter_table_storage_statistics {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor original_table;
    struct mylite_catalog_table_descriptor table;
    char comment[MYLITE_CATALOG_TABLE_COMMENT_CAPACITY];
    bool changes_descriptor;
    bool changes_comment;
};

struct planned_alter_schema_default_charset_collation {
    struct mylite_catalog_schema_descriptor schema;
    char default_charset[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char default_collation[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    bool changes_descriptor;
};

enum planned_select_order_direction {
    PLANNED_SELECT_ORDER_DEFAULT = 0,
    PLANNED_SELECT_ORDER_ASC = 1,
    PLANNED_SELECT_ORDER_DESC = 2,
};

struct planned_alter_table_order_by_item {
    struct mylite_catalog_column_descriptor column;
    enum planned_select_order_direction direction;
};

struct planned_alter_table_order_by {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct mylite_catalog_column_descriptor *columns;
    size_t column_count;
    struct planned_alter_table_order_by_item *items;
    size_t item_count;
    int64_t affected_rows;
};

struct planned_alter_table_force {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct mylite_catalog_column_descriptor *columns;
    size_t column_count;
};

struct planned_alter_table_key_maintenance {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    int64_t affected_rows;
};

struct planned_truncate_table {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
};

struct planned_drop_schema_table {
    char physical_name[MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY];
};

struct planned_drop_schema {
    struct mylite_catalog_schema_descriptor schema;
    struct planned_drop_schema_table *tables;
    size_t table_count;
    size_t table_capacity;
    size_t object_count;
};

struct wall_clock_timestamp {
    int64_t epoch;
    long microsecond;
};

enum integer_expression_default_text_work_item_kind {
    INTEGER_EXPRESSION_DEFAULT_TEXT_NODE = 0,
    INTEGER_EXPRESSION_DEFAULT_TEXT_BYTES = 1,
};

struct integer_expression_default_text_work_item {
    enum integer_expression_default_text_work_item_kind kind;
    const struct mylite_sql_ast_node *expression;
    const char *text;
};

struct integer_expression_default_text_stack {
    struct integer_expression_default_text_work_item *items;
    size_t count;
    size_t capacity;
};

struct bit_blob_value {
    uint64_t width;
    uint64_t magnitude;
};

struct varchar_text_validation {
    const char *text;
    size_t text_length;
    size_t row_number;
};

struct char_text_conversion {
    char *text;
    size_t text_length;
    size_t row_number;
};

struct utf8_prefix_request {
    const char *text;
    size_t text_length;
    size_t character_limit;
};

struct utf8_byte_prefix_request {
    const char *text;
    size_t text_length;
    size_t byte_limit;
};

struct insert_select_string_validation {
    const struct mylite_catalog_column_descriptor *source_column;
    const struct mylite_catalog_column_descriptor *target_column;
    size_t row_number;
    bool adjust_value;
};

struct text_family_type_info {
    const char *logical_type;
    const char *display_type;
    uint64_t maximum_length;
};

struct binary_string_type_info {
    const char *logical_type;
    const char *display_type;
    uint64_t maximum_length;
    bool fixed_length;
    bool blob_family;
};

struct decimal_type_info {
    uint64_t precision;
    uint64_t scale;
    bool is_unsigned;
};

enum approximate_type_class {
    APPROXIMATE_TYPE_FLOAT = 1,
    APPROXIMATE_TYPE_DOUBLE = 2,
};

struct approximate_type_info {
    enum approximate_type_class type_class;
    uint64_t precision;
    uint64_t scale;
    bool has_scale;
    bool is_unsigned;
};

struct approximate_type_mapping_state {
    enum approximate_type_class type_class;
    uint64_t precision;
    uint64_t scale;
};

struct decimal_literal_parts {
    size_t integer_end;
    size_t fraction_start;
    size_t fraction_end;
    size_t integer_digit_start;
};

struct decimal_digit_buffer {
    char *digits;
    size_t digit_count;
    size_t digit_capacity;
    bool discarded_nonzero;
};

struct integer_column_range {
    uint64_t positive_max;
    uint64_t negative_abs_max;
};

struct integer_logical_type_range_request {
    const char *logical_type;
    const char *unsupported_message;
};

struct generated_constraint_columns {
    const struct mylite_catalog_column_descriptor *columns;
    size_t column_count;
};

enum planned_update_assignment_value_kind {
    PLANNED_UPDATE_ASSIGNMENT_VALUE = 0,
    PLANNED_UPDATE_ASSIGNMENT_SAME_COLUMN_ARITHMETIC = 1,
    PLANNED_UPDATE_ASSIGNMENT_DATE_INTERVAL = 2,
    PLANNED_UPDATE_ASSIGNMENT_COLUMN_COPY = 3,
    PLANNED_UPDATE_ASSIGNMENT_ROW_SCALAR = 4,
};

enum {
    update_date_interval_parameter_count = 4,
};

enum update_arithmetic_range_condition {
    UPDATE_ARITHMETIC_RANGE_ANY_NON_NULL = 0,
    UPDATE_ARITHMETIC_RANGE_GREATER_THAN = 1,
    UPDATE_ARITHMETIC_RANGE_LESS_THAN = 2,
};

enum update_arithmetic_range_error_kind {
    UPDATE_ARITHMETIC_RANGE_ERROR_COLUMN = 0,
    UPDATE_ARITHMETIC_RANGE_ERROR_SIGNED_BIGINT = 1,
    UPDATE_ARITHMETIC_RANGE_ERROR_UNSIGNED_BIGINT = 2,
};

struct update_arithmetic_range_check {
    enum update_arithmetic_range_condition condition;
    int64_t threshold;
    enum update_arithmetic_range_error_kind error_kind;
};

struct mapped_integer_type {
    enum mylite_sql_ast_integer_type type;
    int is_unsigned;
    bool has_display_width;
    bool is_bool_alias;
    uint64_t display_width;
};

struct planned_insert_row {
    struct planned_value *values;
    bool generated_auto_increment;
};

struct insert_auto_increment_mode_counts {
    size_t generated_count;
    size_t explicit_count;
};

struct insert_execution_counters {
    int64_t affected_rows;
    int64_t auto_increment_next_after_rows;
    int64_t last_inserted_explicit_auto_increment;
    int64_t first_inserted_generated_auto_increment;
    bool inserted_explicit_auto_increment;
    bool inserted_generated_auto_increment;
};

enum planned_insert_duplicate_update_value_kind {
    PLANNED_INSERT_DUPLICATE_UPDATE_VALUE_LITERAL = 0,
    PLANNED_INSERT_DUPLICATE_UPDATE_VALUE_VALUES_REFERENCE = 1,
    PLANNED_INSERT_DUPLICATE_UPDATE_VALUE_SAME_COLUMN_ARITHMETIC = 2,
    PLANNED_INSERT_DUPLICATE_UPDATE_VALUE_ROW_SCALAR = 3,
};

struct planned_insert_duplicate_update_assignment {
    size_t assignment_column_index;
    const struct mylite_sql_ast_node *assignment_value_node;
    enum planned_insert_duplicate_update_value_kind value_kind;
    size_t values_column_index;
    size_t values_reference_count;
    enum mylite_sql_ast_operator arithmetic_operator;
    uint64_t arithmetic_delta;
    struct planned_row_scalar_expression *row_scalar_expression;
    bool arithmetic_delta_is_valid;
    bool generated_default_noop;
};

struct planned_insert_duplicate_update {
    bool has_clause;
    struct planned_insert_duplicate_update_assignment *assignments;
    size_t assignment_count;
    size_t key_index;
    bool has_key;
};

struct insert_duplicate_projected_row_inputs {
    const struct planned_value *current_values;
    const struct planned_value *assignment_values;
};

struct insert_duplicate_assignment_values {
    struct planned_value *values;
    const struct planned_insert_duplicate_update_assignment *assignments;
    size_t count;
};

struct insert_duplicate_arithmetic_result_request {
    const struct mylite_catalog_column_descriptor *column;
    size_t row_number;
    enum mylite_sql_ast_operator operator_kind;
    int64_t current_value;
    uint64_t delta;
};

struct insert_duplicate_arithmetic_range_error {
    const struct mylite_catalog_column_descriptor *column;
    size_t row_number;
    enum update_arithmetic_range_error_kind error_kind;
};

struct planned_insert {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct mylite_catalog_column_descriptor *columns;
    size_t column_count;
    struct loaded_table_columns_cache_entry *columns_cache_entry;
    struct loaded_index_info *indexes;
    size_t index_count;
    struct loaded_foreign_key_info *foreign_keys;
    size_t foreign_key_count;
    struct planned_insert_row *rows;
    size_t row_count;
    bool ignore_errors;
    bool replace_existing_rows;
    bool has_primary_key;
    size_t primary_key_column_index;
    bool has_auto_increment;
    bool no_auto_value_on_zero;
    size_t auto_increment_column_index;
    struct integer_column_range auto_increment_range;
    int64_t auto_increment_next;
    int64_t auto_increment_next_after_statement;
    int64_t first_generated_auto_increment;
    bool generated_auto_increment;
    struct planned_insert_duplicate_update duplicate_update;
};

struct planned_load_data_infile {
    struct planned_insert insert;
    char *file_path;
    size_t file_path_length;
    size_t *target_indexes;
    size_t target_count;
    uint64_t ignore_line_count;
};

struct load_data_field {
    char *text;
    size_t text_length;
    bool is_null;
};

struct load_data_row {
    struct load_data_field *fields;
    size_t field_count;
    size_t field_capacity;
};

enum planned_select_predicate_kind {
    PLANNED_SELECT_PREDICATE_NONE = 0,
    PLANNED_SELECT_PREDICATE_COMPARISON = 1,
    PLANNED_SELECT_PREDICATE_IS_NULL = 2,
    PLANNED_SELECT_PREDICATE_AND = 3,
    PLANNED_SELECT_PREDICATE_OR = 4,
    PLANNED_SELECT_PREDICATE_NOT = 5,
    PLANNED_SELECT_PREDICATE_BETWEEN = 6,
    PLANNED_SELECT_PREDICATE_IN = 7,
    PLANNED_SELECT_PREDICATE_IS_BOOLEAN = 8,
    PLANNED_SELECT_PREDICATE_XOR = 9,
    PLANNED_SELECT_PREDICATE_EXISTS = 10,
    PLANNED_SELECT_PREDICATE_ROW_SCALAR_TRUTH = 11,
    PLANNED_SELECT_PREDICATE_ROW_SCALAR_COMPARISON = 12,
    PLANNED_SELECT_PREDICATE_ROW_SCALAR_IS_NULL = 13,
    PLANNED_SELECT_PREDICATE_ROW_SCALAR_BETWEEN = 14,
    PLANNED_SELECT_PREDICATE_SCALAR_SUBQUERY_IN = 15,
    PLANNED_SELECT_PREDICATE_SCALAR_COUNT_SUBQUERY_COMPARISON = 16,
    PLANNED_SELECT_PREDICATE_SCALAR_AGGREGATE_SUBQUERY_COMPARISON = 17,
    PLANNED_SELECT_PREDICATE_SCALAR_AGGREGATE_SUBQUERY_BETWEEN = 18,
    PLANNED_SELECT_PREDICATE_ROW_SCALAR_IN = 19,
    PLANNED_SELECT_PREDICATE_ROW_SCALAR_IS_BOOLEAN = 20,
    PLANNED_SELECT_PREDICATE_QUANTIFIED_SUBQUERY = 21,
};

struct planned_select_predicate_node {
    size_t column_source_index;
    size_t value_column_source_index;
    struct planned_value *values;
    size_t value_count;
    size_t row_scalar_decimal_scale;
    size_t left_index;
    size_t right_index;
    struct planned_exists_subquery *exists_subquery;
    struct planned_in_subquery *in_subquery;
    struct planned_scalar_aggregate_subquery *subject_scalar_subquery;
    struct planned_scalar_aggregate_subquery *value_scalar_subquery;
    struct planned_scalar_aggregate_subquery *upper_scalar_subquery;
    struct planned_row_scalar_expression *row_scalar_expression;
    struct planned_row_scalar_expression *row_scalar_value_expression;
    struct planned_row_scalar_expression *row_scalar_upper_value_expression;
    struct planned_row_scalar_expression *row_scalar_value_expressions;
    struct planned_value value;
    struct planned_value upper_value;
    const struct mylite_catalog_column_descriptor *column;
    const struct mylite_catalog_column_descriptor *value_column;
    enum planned_select_predicate_kind kind;
    enum mylite_sql_ast_operator operator_kind;
    enum mylite_sql_ast_node_kind quantified_subquery_kind;
    enum mylite_sql_ast_operator comparison_result_is_operator_kind;
    bool value_is_column_reference;
    bool compare_date_as_datetime;
    bool compare_string_as_integer;
    bool row_scalar_compare_as_real;
    bool row_scalar_regexp_case_sensitive;
    bool regexp_cast_subject_as_text;
    char like_escape_character;
};

_Static_assert(
    sizeof(struct planned_select_predicate_node) <= 512U,
    "planned predicate nodes must retain compact column references"
);

struct planned_select_predicate {
    struct planned_select_predicate_node *nodes;
    size_t node_count;
    size_t node_capacity;
    struct mylite_catalog_column_descriptor **owned_columns;
    size_t owned_column_count;
    size_t owned_column_capacity;
    size_t root_index;
    bool has_root;
    bool qualify_column_references;
};

struct planned_row_scalar_expression;
struct planned_row_scalar_select;

enum planned_select_order_item_kind {
    PLANNED_SELECT_ORDER_ITEM_COLUMN = 0,
    PLANNED_SELECT_ORDER_ITEM_FIELD = 1,
    PLANNED_SELECT_ORDER_ITEM_RAND = 2,
    PLANNED_SELECT_ORDER_ITEM_ROW_SCALAR = 3,
    PLANNED_SELECT_ORDER_ITEM_ALIAS = 4,
    PLANNED_SELECT_ORDER_ITEM_ORDINAL = 5,
};

struct select_predicate_plan_options {
    bool allow_exists;
    bool allow_in_subquery;
    bool allow_quantified_subquery;
    bool allow_scalar_subquery_in_literal;
    bool allow_scalar_count_subquery_comparison;
    bool allow_column_reference_rhs;
    bool allow_same_scope_column_reference_rhs;
    bool allow_date_format_numeric_predicate;
    bool allow_sql_auto_is_null_lookup;
    size_t exists_inner_source_index;
    size_t exists_inner_source_count;
    const struct select_source_context *outer_source_context;
    const struct mylite_catalog_column_descriptor *outer_columns;
    size_t outer_column_count;
};

enum predicate_work_item_kind {
    PREDICATE_WORK_NODE = 0,
    PREDICATE_WORK_DEPRECATED_AND_WARNING = 1,
    PREDICATE_WORK_DEPRECATED_OR_WARNING = 2,
    PREDICATE_WORK_FINISH_LOGICAL = 3,
    PREDICATE_WORK_FINISH_NOT = 4,
};

struct predicate_work_item {
    enum predicate_work_item_kind kind;
    const struct mylite_sql_ast_node *node;
    enum mylite_sql_ast_operator operator_kind;
};

struct predicate_work_stack {
    struct predicate_work_item *items;
    size_t count;
    size_t capacity;
};

struct predicate_result_stack {
    size_t *indexes;
    size_t count;
    size_t capacity;
};

enum predicate_sql_work_item_kind {
    PREDICATE_SQL_WORK_NODE = 0,
    PREDICATE_SQL_WORK_OPERATOR = 1,
    PREDICATE_SQL_WORK_CLOSE = 2,
    PREDICATE_SQL_WORK_TEXT = 3,
};

struct predicate_sql_work_item {
    enum predicate_sql_work_item_kind kind;
    size_t node_index;
    enum mylite_sql_ast_operator operator_kind;
    const char *text;
};

enum select_sql_work_item_kind {
    SELECT_SQL_WORK_QUERY = 0,
    SELECT_SQL_WORK_FROM = 1,
    SELECT_SQL_WORK_SOURCE = 2,
    SELECT_SQL_WORK_JOIN_CONDITION = 3,
    SELECT_SQL_WORK_PREDICATE = 4,
    SELECT_SQL_WORK_ORDER = 5,
    SELECT_SQL_WORK_LIMIT = 6,
    SELECT_SQL_WORK_TEXT = 7,
    SELECT_SQL_WORK_SOURCE_ALIAS = 8,
    SELECT_SQL_WORK_GROUPED_QUERY = 9,
    SELECT_SQL_WORK_COMPOUND_QUERY = 10,
    SELECT_SQL_WORK_ROW_SCALAR_QUERY = 11,
};

struct select_sql_work_item {
    enum select_sql_work_item_kind kind;
    const struct planned_select *plan;
    const struct planned_grouped_aggregate *grouped_plan;
    const struct planned_compound_select *compound_plan;
    const struct planned_row_scalar_select *row_scalar_plan;
    const struct planned_select_source *source;
    const struct planned_select_join_condition *join_condition;
    const struct planned_select_predicate *predicate;
    const struct planned_select_order *order;
    const struct planned_select_limit *limit;
    const char *text;
    size_t source_index;
};

struct select_sql_work_stack {
    struct select_sql_work_item *items;
    size_t count;
    size_t capacity;
};

enum select_parameter_bind_item_kind {
    SELECT_PARAMETER_BIND_PLAN = 0,
    SELECT_PARAMETER_BIND_SOURCE_LIST = 1,
    SELECT_PARAMETER_BIND_SOURCE = 2,
    SELECT_PARAMETER_BIND_JOIN_CONDITION = 3,
    SELECT_PARAMETER_BIND_PREDICATE = 4,
    SELECT_PARAMETER_BIND_ORDER = 5,
    SELECT_PARAMETER_BIND_GROUPED_AGGREGATE = 6,
    SELECT_PARAMETER_BIND_COMPOUND_SELECT = 7,
    SELECT_PARAMETER_BIND_ROW_SCALAR_SELECT = 8,
};

struct select_parameter_bind_item {
    enum select_parameter_bind_item_kind kind;
    const struct planned_select *plan;
    const struct planned_grouped_aggregate *grouped_plan;
    const struct planned_compound_select *compound_plan;
    const struct planned_row_scalar_select *row_scalar_plan;
    const struct planned_select_source *sources;
    const struct planned_select_source *source;
    const struct planned_select_join_condition *join_condition;
    const struct planned_select_predicate *predicate;
    const struct planned_select_order *order;
    size_t source_count;
};

struct select_parameter_bind_stack {
    struct select_parameter_bind_item *items;
    size_t count;
    size_t capacity;
};

enum row_scalar_integer_width_frame_kind {
    ROW_SCALAR_INTEGER_WIDTH_ENTER = 0,
    ROW_SCALAR_INTEGER_WIDTH_FINISH_UNARY = 1,
    ROW_SCALAR_INTEGER_WIDTH_FINISH_BINARY = 2,
};

struct row_scalar_integer_width_frame {
    enum row_scalar_integer_width_frame_kind kind;
    const struct mylite_sql_ast_node *expression;
};

struct row_scalar_integer_width_result {
    bool matched;
    uint64_t width;
};

struct row_scalar_integer_width_stack {
    struct row_scalar_integer_width_frame *frames;
    size_t frame_count;
    size_t frame_capacity;
    struct row_scalar_integer_width_result *results;
    size_t result_count;
    size_t result_capacity;
};

struct planned_select_order_item {
    enum planned_select_order_item_kind kind;
    enum planned_select_order_direction direction;
    struct mylite_catalog_column_descriptor *column;
    size_t column_source_index;
    struct planned_row_scalar_expression *expression;
    char alias[select_item_alias_capacity];
    size_t ordinal;
};

struct planned_select_order {
    bool has_order;
    enum planned_select_order_direction direction;
    struct mylite_catalog_column_descriptor *column;
    size_t column_source_index;
    bool qualify_column_reference;
    struct planned_select_order_item *items;
    size_t item_count;
    size_t item_capacity;
};

_Static_assert(
    sizeof(struct planned_select_order_item) <= 512U,
    "planned ORDER BY items must retain compact column references"
);

_Static_assert(
    sizeof(struct planned_select_order) <= 128U,
    "planned ORDER BY state must retain compact column references"
);

struct select_order_ast_item_nodes {
    const struct mylite_sql_ast_node *order_key;
    const struct mylite_sql_ast_node *direction;
};

struct joined_select_having_reference_request {
    const struct mylite_sql_ast_node *select_list;
    const struct mylite_sql_ast_node *having_node;
};

struct planned_select_limit {
    bool has_limit;
    int64_t row_count;
    bool has_offset;
    int64_t offset;
};

enum planned_grouped_having_kind {
    PLANNED_GROUPED_HAVING_NONE = 0,
    PLANNED_GROUPED_HAVING_COMPARISON = 1,
    PLANNED_GROUPED_HAVING_IS_NULL = 2,
    PLANNED_GROUPED_HAVING_IN = 3,
};

enum planned_grouped_having_operand {
    PLANNED_GROUPED_HAVING_OPERAND_NONE = 0,
    PLANNED_GROUPED_HAVING_OPERAND_GROUP_COLUMN = 1,
    PLANNED_GROUPED_HAVING_OPERAND_AGGREGATE = 2,
    PLANNED_GROUPED_HAVING_OPERAND_ROW_SCALAR = 3,
};

struct planned_grouped_having {
    enum planned_grouped_having_kind kind;
    enum planned_grouped_having_operand operand;
    enum mylite_sql_ast_operator operator_kind;
    struct planned_value value;
    struct planned_value *values;
    size_t value_count;
    bool value_uses_uint64_order_key;
    char uint64_order_key[uint64_decimal_digit_capacity + 1U];
    size_t group_index;
    size_t aggregate_index;
    struct planned_row_scalar_expression *row_scalar_expression;
};

struct planned_diagnostics_show_limit {
    bool has_limit;
    uint64_t row_count;
    bool has_offset;
    uint64_t offset;
};

struct planned_json_table_source {
    struct planned_value *values;
    size_t row_count;
    size_t column_count;
};

struct planned_select_source {
    struct table_name_resolution source;
    struct mylite_catalog_table_descriptor table;
    struct mylite_catalog_column_descriptor *columns;
    size_t column_count;
    struct loaded_table_columns_cache_entry *columns_cache_entry;
    char alias[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    bool has_alias;
    bool is_derived;
    bool is_json_table;
    struct planned_json_table_source json_table;
    struct planned_select *derived_select;
    struct planned_row_scalar_select *derived_row_scalar_select;
    struct planned_grouped_aggregate *derived_grouped_aggregate;
    struct planned_compound_select *derived_compound_select;
};

struct planned_exists_subquery {
    bool has_table_source;
    size_t inner_source_index;
    struct planned_select_source source;
    struct planned_select_source *sources;
    size_t source_count;
    enum mylite_sql_ast_join_kind *join_kinds;
    struct planned_select_join_condition *join_conditions;
    size_t join_count;
    struct planned_select_predicate predicate;
    struct planned_select_limit limit;
};

struct exists_subquery_outer_context {
    const struct select_source_context *source_context;
    const struct mylite_catalog_column_descriptor *columns;
    size_t column_count;
    size_t source_count;
};

struct planned_in_subquery {
    struct planned_select_source source;
    struct planned_select_source *sources;
    size_t source_count;
    enum mylite_sql_ast_join_kind *join_kinds;
    struct planned_select_join_condition *join_conditions;
    size_t join_count;
    struct mylite_catalog_column_descriptor column;
    size_t column_source_index;
    struct planned_select_predicate predicate;
    size_t inner_source_index;
};

struct planned_select_join_condition {
    bool has_condition;
    bool right_value_is_row_scalar_expression;
    struct mylite_catalog_column_descriptor left_column;
    struct mylite_catalog_column_descriptor right_column;
    size_t left_source_index;
    size_t right_source_index;
    struct planned_row_scalar_expression *right_row_scalar_expression;
    struct planned_select_predicate extra_predicate;
};

struct planned_select {
    struct table_name_resolution source;
    struct mylite_catalog_table_descriptor table;
    char source_alias[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    bool source_has_alias;
    struct planned_select_source *sources;
    size_t source_count;
    struct mylite_catalog_column_descriptor *source_columns;
    size_t source_column_count;
    struct loaded_table_columns_cache_entry *source_columns_cache_entry;
    const struct mylite_catalog_column_descriptor **columns;
    char **column_aliases;
    size_t *column_source_indexes;
    size_t column_count;
    size_t column_capacity;
    bool is_distinct;
    bool calc_found_rows;
    bool requires_source_alias;
    enum mylite_sql_ast_join_kind join_kind;
    struct planned_select_join_condition join_condition;
    enum mylite_sql_ast_join_kind *join_kinds;
    struct planned_select_join_condition *join_conditions;
    size_t join_count;
    struct planned_select_predicate predicate;
    struct planned_select_order order;
    struct planned_select_limit limit;
};

struct select_optional_clauses {
    const struct mylite_sql_ast_node *where_clause;
    const struct mylite_sql_ast_node *having_clause;
    const struct mylite_sql_ast_node *order_clause;
    const struct mylite_sql_ast_node *limit_clause;
};

struct joined_select_ast {
    const struct mylite_sql_ast_node *statement;
    const struct mylite_sql_ast_node *from_clause;
};

struct joined_select_temp_nodes {
    const struct mylite_sql_ast_node **source_nodes;
    size_t source_node_count;
    const struct mylite_sql_ast_node **join_condition_nodes;
    size_t join_condition_node_count;
};

enum planned_row_scalar_expression_kind {
    PLANNED_ROW_SCALAR_EXPRESSION_NONE = 0,
    PLANNED_ROW_SCALAR_EXPRESSION_VALUE = 1,
    PLANNED_ROW_SCALAR_EXPRESSION_COLUMN = 2,
    PLANNED_ROW_SCALAR_EXPRESSION_CONCAT = 3,
    PLANNED_ROW_SCALAR_EXPRESSION_FIELD = 4,
    PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT = 5,
    PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_COMPARISON = 6,
    PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH = 7,
    PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE = 8,
    PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE = 9,
    PLANNED_ROW_SCALAR_EXPRESSION_HEX = 10,
    PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM = 11,
    PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT = 12,
    PLANNED_ROW_SCALAR_EXPRESSION_STRING_SEARCH = 13,
    PLANNED_ROW_SCALAR_EXPRESSION_FIND_IN_SET = 14,
    PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS = 15,
    PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALID = 16,
    PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT = 17,
    PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE = 18,
    PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT = 19,
    PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY = 20,
    PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT = 21,
    PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE = 22,
    PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH = 23,
    PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS = 24,
    PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH = 25,
    PLANNED_ROW_SCALAR_EXPRESSION_STRING_REPLACE = 26,
    PLANNED_ROW_SCALAR_EXPRESSION_IF = 27,
    PLANNED_ROW_SCALAR_EXPRESSION_IFNULL = 28,
    PLANNED_ROW_SCALAR_EXPRESSION_COALESCE = 29,
    PLANNED_ROW_SCALAR_EXPRESSION_NULLIF = 30,
    PLANNED_ROW_SCALAR_EXPRESSION_ISNULL = 31,
    PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP = 32,
    PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_LIKE = 33,
    PLANNED_ROW_SCALAR_EXPRESSION_UNHEX = 34,
    PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT = 35,
    PLANNED_ROW_SCALAR_EXPRESSION_GREATEST = 36,
    PLANNED_ROW_SCALAR_EXPRESSION_LEAST = 37,
    PLANNED_ROW_SCALAR_EXPRESSION_SUBSTRING_INDEX = 38,
    PLANNED_ROW_SCALAR_EXPRESSION_STRING_PADDING = 39,
    PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF = 40,
    PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND = 41,
    PLANNED_ROW_SCALAR_EXPRESSION_STRING_REVERSE = 42,
    PLANNED_ROW_SCALAR_EXPRESSION_CHAR = 43,
    PLANNED_ROW_SCALAR_EXPRESSION_STRCMP = 44,
    PLANNED_ROW_SCALAR_EXPRESSION_STRING_QUOTE = 45,
    PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME = 46,
    PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME = 47,
    PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS = 48,
    PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE = 49,
    PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME = 50,
    PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC = 51,
    PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT = 52,
    PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF = 53,
    PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID = 54,
    PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN = 55,
    PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID = 56,
    PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE = 57,
    PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF = 58,
    PLANNED_ROW_SCALAR_EXPRESSION_UUID = 59,
    PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64 = 60,
    PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64 = 61,
    PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE = 62,
    PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS = 63,
    PLANNED_ROW_SCALAR_EXPRESSION_STRING_INSERT = 64,
    PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET = 65,
    PLANNED_ROW_SCALAR_EXPRESSION_RAND = 66,
    PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL = 67,
    PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE = 68,
    PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP = 69,
    PLANNED_ROW_SCALAR_EXPRESSION_STRING_BITMASK = 70,
    PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION = 71,
    PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR = 72,
    PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR = 73,
    PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE = 74,
    PLANNED_ROW_SCALAR_EXPRESSION_SOUNDEX = 75,
    PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION = 76,
    PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD = 77,
    PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF = 78,
    PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ = 79,
    PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING = 80,
    PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY = 81,
    PLANNED_ROW_SCALAR_EXPRESSION_SEARCHED_CASE = 82,
    PLANNED_ROW_SCALAR_EXPRESSION_LIKE_PREDICATE = 83,
    PLANNED_ROW_SCALAR_EXPRESSION_LOGICAL_CONDITION = 84,
    PLANNED_ROW_SCALAR_EXPRESSION_COUNT_STAR = 85,
    PLANNED_ROW_SCALAR_EXPRESSION_SUM_COLUMN = 86,
    PLANNED_ROW_SCALAR_EXPRESSION_COLLATE = 87,
    PLANNED_ROW_SCALAR_EXPRESSION_DIGEST = 88,
    PLANNED_ROW_SCALAR_EXPRESSION_COMPRESS = 89,
    PLANNED_ROW_SCALAR_EXPRESSION_UNCOMPRESS = 90,
    PLANNED_ROW_SCALAR_EXPRESSION_UNCOMPRESSED_LENGTH = 91,
    PLANNED_ROW_SCALAR_EXPRESSION_RANDOM_BYTES = 92,
    PLANNED_ROW_SCALAR_EXPRESSION_NUMERIC_FUNCTION = 93,
    PLANNED_ROW_SCALAR_EXPRESSION_ELT = 94,
    PLANNED_ROW_SCALAR_EXPRESSION_BASE_CONVERSION = 95,
    PLANNED_ROW_SCALAR_EXPRESSION_NUMERIC_EXTRA = 96,
    PLANNED_ROW_SCALAR_EXPRESSION_ADDTIME = 97,
    PLANNED_ROW_SCALAR_EXPRESSION_SUBTIME = 98,
    PLANNED_ROW_SCALAR_EXPRESSION_BITWISE = 99,
    PLANNED_ROW_SCALAR_EXPRESSION_INET_ATON = 100,
    PLANNED_ROW_SCALAR_EXPRESSION_INET_NTOA = 101,
    PLANNED_ROW_SCALAR_EXPRESSION_LAST_INSERT_ID_SET = 102,
    PLANNED_ROW_SCALAR_EXPRESSION_LAST_INSERT_ID_GET = 103,
    PLANNED_ROW_SCALAR_EXPRESSION_GET_LOCK = 104,
    PLANNED_ROW_SCALAR_EXPRESSION_IS_FREE_LOCK = 105,
    PLANNED_ROW_SCALAR_EXPRESSION_IS_USED_LOCK = 106,
    PLANNED_ROW_SCALAR_EXPRESSION_RELEASE_LOCK = 107,
    PLANNED_ROW_SCALAR_EXPRESSION_RELEASE_ALL_LOCKS = 108,
    PLANNED_ROW_SCALAR_EXPRESSION_ICU_VERSION = 109,
    PLANNED_ROW_SCALAR_EXPRESSION_BENCHMARK = 110,
    PLANNED_ROW_SCALAR_EXPRESSION_SLEEP = 111,
    PLANNED_ROW_SCALAR_EXPRESSION_LOAD_FILE = 112,
    PLANNED_ROW_SCALAR_EXPRESSION_EXTRACT_VALUE = 113,
    PLANNED_ROW_SCALAR_EXPRESSION_UPDATE_XML = 114,
    PLANNED_ROW_SCALAR_EXPRESSION_SYS_FUNCTION = 115,
    PLANNED_ROW_SCALAR_EXPRESSION_JSON_DEPTH = 116,
    PLANNED_ROW_SCALAR_EXPRESSION_JSON_PRETTY = 117,
    PLANNED_ROW_SCALAR_EXPRESSION_JSON_OVERLAPS = 118,
    PLANNED_ROW_SCALAR_EXPRESSION_JSON_MEMBER_OF = 119,
    PLANNED_ROW_SCALAR_EXPRESSION_UUID_SHORT = 120,
    PLANNED_ROW_SCALAR_EXPRESSION_JSON_STORAGE_SIZE = 121,
    PLANNED_ROW_SCALAR_EXPRESSION_JSON_STORAGE_FREE = 122,
    PLANNED_ROW_SCALAR_EXPRESSION_JSON_SEARCH = 123,
    PLANNED_ROW_SCALAR_EXPRESSION_SPATIAL_FUNCTION = 124,
    PLANNED_ROW_SCALAR_EXPRESSION_AES_ENCRYPT = 125,
    PLANNED_ROW_SCALAR_EXPRESSION_AES_DECRYPT = 126,
    PLANNED_ROW_SCALAR_EXPRESSION_VALUES_OUTSIDE_ODKU = 127,
    PLANNED_ROW_SCALAR_EXPRESSION_JSON_SCHEMA_VALID = 128,
    PLANNED_ROW_SCALAR_EXPRESSION_JSON_SCHEMA_VALIDATION_REPORT = 129,
};

enum row_scalar_control_flow_bind_mode {
    ROW_SCALAR_CONTROL_FLOW_BIND_ARGUMENT = 0,
    ROW_SCALAR_CONTROL_FLOW_BIND_LEAF_ARGUMENT = 1,
};

struct row_scalar_control_flow_bind_frame {
    const struct planned_row_scalar_expression *expression;
    enum row_scalar_control_flow_bind_mode mode;
};

struct row_scalar_control_flow_bind_stack {
    struct row_scalar_control_flow_bind_frame *items;
    size_t count;
    size_t capacity;
};

enum {
    planned_row_scalar_timestampdiff_argument_count = 5,
};

enum planned_window_function_kind {
    PLANNED_WINDOW_FUNCTION_NONE = 0,
    PLANNED_WINDOW_FUNCTION_ROW_NUMBER = 1,
    PLANNED_WINDOW_FUNCTION_RANK = 2,
    PLANNED_WINDOW_FUNCTION_DENSE_RANK = 3,
    PLANNED_WINDOW_FUNCTION_PERCENT_RANK = 4,
    PLANNED_WINDOW_FUNCTION_CUME_DIST = 5,
    PLANNED_WINDOW_FUNCTION_NTILE = 6,
    PLANNED_WINDOW_FUNCTION_LAG = 7,
    PLANNED_WINDOW_FUNCTION_LEAD = 8,
    PLANNED_WINDOW_FUNCTION_FIRST_VALUE = 9,
    PLANNED_WINDOW_FUNCTION_LAST_VALUE = 10,
    PLANNED_WINDOW_FUNCTION_NTH_VALUE = 11,
    PLANNED_WINDOW_FUNCTION_COUNT_STAR = 12,
    PLANNED_WINDOW_FUNCTION_COUNT_VALUE = 13,
    PLANNED_WINDOW_FUNCTION_SUM = 14,
    PLANNED_WINDOW_FUNCTION_AVG = 15,
    PLANNED_WINDOW_FUNCTION_MIN = 16,
    PLANNED_WINDOW_FUNCTION_MAX = 17,
    PLANNED_WINDOW_FUNCTION_BIT_AND = 18,
    PLANNED_WINDOW_FUNCTION_BIT_OR = 19,
    PLANNED_WINDOW_FUNCTION_BIT_XOR = 20,
    PLANNED_WINDOW_FUNCTION_STD = 21,
    PLANNED_WINDOW_FUNCTION_STDDEV = 22,
    PLANNED_WINDOW_FUNCTION_STDDEV_POP = 23,
    PLANNED_WINDOW_FUNCTION_STDDEV_SAMP = 24,
    PLANNED_WINDOW_FUNCTION_VAR_POP = 25,
    PLANNED_WINDOW_FUNCTION_VAR_SAMP = 26,
    PLANNED_WINDOW_FUNCTION_VARIANCE = 27,
    PLANNED_WINDOW_FUNCTION_GROUP_CONCAT = 28,
    PLANNED_WINDOW_FUNCTION_JSON_ARRAYAGG = 29,
    PLANNED_WINDOW_FUNCTION_JSON_OBJECTAGG = 30,
};

enum planned_window_frame_unit {
    PLANNED_WINDOW_FRAME_UNIT_NONE = 0,
    PLANNED_WINDOW_FRAME_UNIT_ROWS = 1,
    PLANNED_WINDOW_FRAME_UNIT_RANGE = 2,
};

enum planned_window_frame_bound_kind {
    PLANNED_WINDOW_FRAME_BOUND_NONE = 0,
    PLANNED_WINDOW_FRAME_BOUND_UNBOUNDED_PRECEDING = 1,
    PLANNED_WINDOW_FRAME_BOUND_UNBOUNDED_FOLLOWING = 2,
    PLANNED_WINDOW_FRAME_BOUND_CURRENT_ROW = 3,
    PLANNED_WINDOW_FRAME_BOUND_VALUE_PRECEDING = 4,
    PLANNED_WINDOW_FRAME_BOUND_VALUE_FOLLOWING = 5,
};

struct planned_window_frame_bound {
    enum planned_window_frame_bound_kind kind;
    uint64_t value;
};

struct window_function_argument_count_request {
    enum planned_window_function_kind kind;
    size_t argument_count;
};

struct window_function_argument_position {
    enum planned_window_function_kind kind;
    size_t argument_index;
};

enum planned_row_scalar_field_domain {
    PLANNED_ROW_SCALAR_FIELD_DOMAIN_NONE = 0,
    PLANNED_ROW_SCALAR_FIELD_DOMAIN_STRING = 1,
    PLANNED_ROW_SCALAR_FIELD_DOMAIN_INTEGER = 2,
};

enum planned_row_scalar_conversion_kind {
    PLANNED_ROW_SCALAR_CONVERSION_NONE = 0,
    PLANNED_ROW_SCALAR_CONVERSION_BINARY = 1,
    PLANNED_ROW_SCALAR_CONVERSION_CHAR = 2,
    PLANNED_ROW_SCALAR_CONVERSION_SIGNED = 3,
    PLANNED_ROW_SCALAR_CONVERSION_UNSIGNED = 4,
    PLANNED_ROW_SCALAR_CONVERSION_USING_BINARY = 5,
    PLANNED_ROW_SCALAR_CONVERSION_USING_CHARSET = 6,
};

enum planned_row_scalar_digest_kind {
    PLANNED_ROW_SCALAR_DIGEST_NONE = 0,
    PLANNED_ROW_SCALAR_DIGEST_MD5 = 1,
    PLANNED_ROW_SCALAR_DIGEST_SHA = 2,
    PLANNED_ROW_SCALAR_DIGEST_SHA1 = 3,
    PLANNED_ROW_SCALAR_DIGEST_SHA2 = 4,
};

enum planned_row_scalar_numeric_function_kind {
    PLANNED_ROW_SCALAR_NUMERIC_NONE = 0,
    PLANNED_ROW_SCALAR_NUMERIC_ABS = 1,
    PLANNED_ROW_SCALAR_NUMERIC_SIGN = 2,
    PLANNED_ROW_SCALAR_NUMERIC_CEIL = 3,
    PLANNED_ROW_SCALAR_NUMERIC_CEILING = 4,
    PLANNED_ROW_SCALAR_NUMERIC_FLOOR = 5,
    PLANNED_ROW_SCALAR_NUMERIC_ROUND = 6,
    PLANNED_ROW_SCALAR_NUMERIC_SQRT = 7,
    PLANNED_ROW_SCALAR_NUMERIC_DEGREES = 8,
    PLANNED_ROW_SCALAR_NUMERIC_RADIANS = 9,
    PLANNED_ROW_SCALAR_NUMERIC_ACOS = 10,
    PLANNED_ROW_SCALAR_NUMERIC_ASIN = 11,
    PLANNED_ROW_SCALAR_NUMERIC_ATAN = 12,
    PLANNED_ROW_SCALAR_NUMERIC_ATAN2 = 13,
    PLANNED_ROW_SCALAR_NUMERIC_SIN = 14,
    PLANNED_ROW_SCALAR_NUMERIC_COS = 15,
    PLANNED_ROW_SCALAR_NUMERIC_TAN = 16,
    PLANNED_ROW_SCALAR_NUMERIC_COT = 17,
    PLANNED_ROW_SCALAR_NUMERIC_EXP = 18,
    PLANNED_ROW_SCALAR_NUMERIC_LN = 19,
    PLANNED_ROW_SCALAR_NUMERIC_LOG = 20,
    PLANNED_ROW_SCALAR_NUMERIC_LOG10 = 21,
    PLANNED_ROW_SCALAR_NUMERIC_LOG2 = 22,
    PLANNED_ROW_SCALAR_NUMERIC_POW = 23,
    PLANNED_ROW_SCALAR_NUMERIC_POWER = 24,
    PLANNED_ROW_SCALAR_NUMERIC_BIT_COUNT = 25,
};

enum planned_row_scalar_base_conversion_kind {
    PLANNED_ROW_SCALAR_BASE_CONVERSION_NONE = 0,
    PLANNED_ROW_SCALAR_BASE_CONVERSION_BIN = 1,
    PLANNED_ROW_SCALAR_BASE_CONVERSION_OCT = 2,
    PLANNED_ROW_SCALAR_BASE_CONVERSION_CONV = 3,
};

enum planned_row_scalar_numeric_extra_kind {
    PLANNED_ROW_SCALAR_NUMERIC_EXTRA_NONE = 0,
    PLANNED_ROW_SCALAR_NUMERIC_EXTRA_CRC32 = 1,
    PLANNED_ROW_SCALAR_NUMERIC_EXTRA_FORMAT = 2,
    PLANNED_ROW_SCALAR_NUMERIC_EXTRA_TRUNCATE = 3,
    PLANNED_ROW_SCALAR_NUMERIC_EXTRA_PI = 4,
};

struct row_scalar_digest_ast_arguments {
    const struct mylite_sql_ast_node *function_expression;
    const struct mylite_sql_ast_node *argument;
    const struct mylite_sql_ast_node *length_argument;
};

enum {
    planned_row_scalar_conversion_step_capacity = 8,
};

struct planned_row_scalar_conversion_step {
    enum planned_row_scalar_conversion_kind kind;
    bool ascii_only;
    const char *charset;
    const char *collation;
};

struct string_slice_right_bounds {
    size_t text_length;
    uint64_t requested_length;
    size_t character_count;
};

struct substring_text_bounds {
    const char *text;
    size_t text_length;
    int64_t position;
    bool has_length;
    int64_t requested_length;
};

struct row_scalar_named_lock_function {
    const char *function_name;
    size_t argument_count;
    enum planned_row_scalar_expression_kind expression_kind;
};

struct planned_row_scalar_expression {
    enum planned_row_scalar_expression_kind kind;
    enum planned_row_scalar_field_domain field_domain;
    enum planned_string_length_function_kind string_length_kind;
    enum planned_string_case_function_kind string_case_kind;
    enum planned_string_codepoint_function_kind string_codepoint_kind;
    enum planned_string_trim_function_kind string_trim_kind;
    enum planned_string_slice_function_kind string_slice_kind;
    enum planned_string_search_function_kind string_search_kind;
    enum planned_string_padding_function_kind string_padding_kind;
    enum planned_string_bitmask_function_kind string_bitmask_kind;
    enum planned_regexp_string_function_kind regexp_string_kind;
    enum planned_row_scalar_conversion_kind conversion_kind;
    enum planned_row_scalar_digest_kind digest_kind;
    enum planned_row_scalar_numeric_function_kind numeric_function_kind;
    enum planned_row_scalar_base_conversion_kind base_conversion_kind;
    enum planned_row_scalar_numeric_extra_kind numeric_extra_kind;
    enum mylite_sys_function_kind sys_function_kind;
    enum mylite_spatial_function_kind spatial_function_kind;
    bool numeric_extra_has_decimal_scale;
    size_t numeric_extra_decimal_scale;
    enum planned_window_function_kind window_function_kind;
    enum mylite_temporal_extract_kind temporal_extract_kind;
    enum mylite_sql_ast_operator operator_kind;
    enum mylite_sql_ast_operator arithmetic_operator;
    struct planned_value value;
    bool value_is_insert_values_reference;
    size_t value_source_index;
    enum mylite_json_sql_value_kind json_value_kind;
    enum planned_json_mutation_kind json_mutation_kind;
    bool regexp_case_sensitive;
    bool like_uses_escape;
    bool case_has_else;
    bool has_rand_seed;
    bool rand_seed_is_expression;
    uint32_t rand_seed;
    bool window_has_partition;
    bool window_has_order;
    bool window_has_frame;
    bool conversion_ascii_only;
    const char *conversion_charset;
    const char *conversion_collation;
    const char *collation_name;
    struct planned_row_scalar_conversion_step
        conversion_steps[planned_row_scalar_conversion_step_capacity];
    size_t conversion_step_count;
    enum planned_select_order_direction window_order_direction;
    enum planned_window_frame_unit window_frame_unit;
    struct planned_window_frame_bound window_frame_start;
    struct planned_window_frame_bound window_frame_end;
    struct mylite_catalog_column_descriptor *window_partition_column;
    struct mylite_catalog_column_descriptor *window_order_column;
    struct mylite_catalog_column_descriptor *column;
    char column_effective_charset[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char column_effective_collation[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    bool column_cast_as_integer;
    bool column_has_source_index;
    size_t column_source_index;
    struct planned_row_scalar_expression *arguments;
    size_t argument_count;
};

_Static_assert(
    sizeof(struct planned_row_scalar_expression) <= 1024U,
    "planned row-scalar expressions must retain compact column references"
);

struct row_scalar_uuid_column_expression_request {
    const struct mylite_sql_ast_node *function_expression;
    const struct mylite_sql_ast_node *value_argument;
    const struct mylite_sql_ast_node *swap_argument;
};

struct row_scalar_integer_arithmetic_plan_frame {
    const struct mylite_sql_ast_node *expression;
    struct planned_row_scalar_expression *out_expression;
};

struct row_scalar_integer_arithmetic_plan_stack {
    struct row_scalar_integer_arithmetic_plan_frame *items;
    size_t count;
    size_t capacity;
};

enum row_scalar_integer_arithmetic_append_phase {
    ROW_SCALAR_INTEGER_ARITHMETIC_APPEND_ENTER = 0,
    ROW_SCALAR_INTEGER_ARITHMETIC_APPEND_COMMA = 1,
    ROW_SCALAR_INTEGER_ARITHMETIC_APPEND_CLOSE = 2,
};

struct row_scalar_integer_arithmetic_append_frame {
    const struct planned_row_scalar_expression *expression;
    enum row_scalar_integer_arithmetic_append_phase phase;
};

struct row_scalar_integer_arithmetic_append_stack {
    struct row_scalar_integer_arithmetic_append_frame *items;
    size_t count;
    size_t capacity;
};

struct row_scalar_integer_arithmetic_bind_stack {
    const struct planned_row_scalar_expression **items;
    size_t count;
    size_t capacity;
};

struct planned_result_column_shape {
    enum mylite_result_logical_type logical_type;
    uint32_t flags;
    uint32_t charset_id;
    uint32_t collation_id;
    uint64_t display_length;
    uint16_t decimals;
    bool nullable;
};

struct planned_row_scalar_select_item {
    struct planned_row_scalar_expression expression;
    char *result_label;
    struct planned_result_column_shape source_result_shape;
    bool has_alias;
    bool alias_is_identifier;
    bool has_source_result_shape;
    bool is_exists_expression;
    struct planned_exists_subquery exists_subquery;
};

struct planned_row_scalar_exists_filter {
    bool has_filter;
    bool has_scalar_predicate;
    bool negate;
    struct planned_exists_subquery subquery;
    struct planned_select_predicate predicate;
};

struct planned_row_scalar_select {
    bool has_source;
    bool has_distinct;
    bool calc_found_rows;
    struct table_name_resolution source;
    char source_alias[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    struct mylite_catalog_table_descriptor table;
    struct planned_select_source *sources;
    size_t source_count;
    enum mylite_sql_ast_join_kind *join_kinds;
    struct planned_select_join_condition *join_conditions;
    size_t join_count;
    struct planned_row_scalar_select_item *items;
    size_t item_count;
    size_t item_capacity;
    bool source_has_alias;
    struct planned_row_scalar_exists_filter tableless_filter;
    struct planned_select_predicate predicate;
    struct planned_select_order order;
    struct planned_select_limit limit;
};

enum planned_count_having_select_projection_kind {
    PLANNED_COUNT_HAVING_SELECT_DESCRIPTOR = 0,
    PLANNED_COUNT_HAVING_SELECT_ROW_SCALAR = 1,
};

struct planned_count_having_select {
    enum planned_count_having_select_projection_kind projection_kind;
    enum mylite_sql_ast_operator operator_kind;
    struct planned_value value;
    struct planned_select descriptor_projection;
    struct planned_row_scalar_select row_projection;
};

struct row_scalar_select_clauses {
    const struct mylite_sql_ast_node *where_clause;
    const struct mylite_sql_ast_node *order_clause;
    const struct mylite_sql_ast_node *limit_clause;
    const struct mylite_sql_ast_node *window_definitions;
};

enum row_scalar_window_definition_state {
    ROW_SCALAR_WINDOW_DEFINITION_UNRESOLVED = 0,
    ROW_SCALAR_WINDOW_DEFINITION_RESOLVING = 1,
    ROW_SCALAR_WINDOW_DEFINITION_RESOLVED = 2,
};

struct row_scalar_window_spec_clauses {
    const struct mylite_sql_ast_node *partition_clause;
    const struct mylite_sql_ast_node *order_clause;
    const struct mylite_sql_ast_node *frame_clause;
    char partition_source_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char order_source_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char frame_source_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
};

struct row_scalar_window_definition_entry {
    char name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    const struct mylite_sql_ast_node *spec;
    enum row_scalar_window_definition_state state;
    bool is_used;
    struct row_scalar_window_spec_clauses clauses;
};

struct row_scalar_window_definitions {
    struct row_scalar_window_definition_entry *entries;
    size_t count;
};

struct row_scalar_window_inheritance_error {
    const char *owner_name;
    const char *inherited_name;
    const char *property;
};

enum planned_insert_select_source_kind {
    PLANNED_INSERT_SELECT_SOURCE_TABLE = 0,
    PLANNED_INSERT_SELECT_SOURCE_ROW_SCALAR = 1,
    PLANNED_INSERT_SELECT_SOURCE_COMPOUND = 2,
};

enum planned_insert_select_compound_branch_kind {
    PLANNED_INSERT_SELECT_COMPOUND_BRANCH_TABLE = 0,
    PLANNED_INSERT_SELECT_COMPOUND_BRANCH_ROW_SCALAR = 1,
};

struct planned_insert_select_compound_branch {
    enum planned_insert_select_compound_branch_kind kind;
    enum mylite_sql_ast_union_modifier modifier;
    struct planned_select source;
    struct planned_row_scalar_select row_source;
};

struct planned_insert_select_compound_source {
    struct planned_insert_select_compound_branch *branches;
    size_t branch_count;
};

struct planned_insert_select {
    struct planned_insert target;
    enum planned_insert_select_source_kind source_kind;
    struct planned_select source;
    struct planned_row_scalar_select row_source;
    struct planned_insert_select_compound_source compound_source;
    size_t *target_indexes;
    size_t target_count;
};

struct insert_select_execution_statements {
    sqlite3_stmt *select_statement;
    sqlite3_stmt *insert_statement;
};

struct insert_select_table_execution {
    char temporary_table_name[MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY];
    char *materialize_sql;
    char *validation_sql;
    char *insert_sql;
    char *drop_sql;
    struct mylite_statement_transaction transaction;
    struct insert_execution_counters counters;
    bool temporary_table_created;
};

struct alter_table_modify_copy_statements {
    sqlite3_stmt *select_statement;
    sqlite3_stmt *insert_statement;
};

struct planned_create_table_select {
    struct planned_create_table create_table;
    struct planned_select source;
    enum mylite_sql_ast_select_locking_clause source_locking_clause;
};

enum planned_count_function {
    PLANNED_COUNT_NONE = 0,
    PLANNED_COUNT_STAR = 1,
    PLANNED_COUNT_COLUMN = 2,
    PLANNED_COUNT_LITERAL = 3,
    PLANNED_COUNT_DISTINCT_COLUMN = 4,
};

enum planned_count_source_kind {
    PLANNED_COUNT_SOURCE_NONE = 0,
    PLANNED_COUNT_SOURCE_TABLE = 1,
    PLANNED_COUNT_SOURCE_DERIVED_SELECT = 2,
    PLANNED_COUNT_SOURCE_DERIVED_ROW_SCALAR = 3,
    PLANNED_COUNT_SOURCE_DERIVED_GROUPED_AGGREGATE = 4,
    PLANNED_COUNT_SOURCE_DERIVED_COMPOUND = 5,
};

struct planned_count_derived_compound_branch {
    enum mylite_sql_ast_union_modifier modifier;
    enum planned_count_source_kind kind;
    struct planned_select select;
    struct planned_row_scalar_select row_scalar_select;
    struct planned_grouped_aggregate *grouped_aggregate;
};

struct planned_count_derived_compound {
    struct planned_count_derived_compound_branch *branches;
    size_t branch_count;
};

struct planned_count {
    bool has_source;
    bool calc_found_rows;
    enum planned_count_source_kind source_kind;
    char *result_label;
    enum planned_count_function function;
    bool count_is_distinct;
    struct table_name_resolution source;
    struct mylite_catalog_table_descriptor table;
    struct mylite_catalog_column_descriptor count_column;
    struct planned_value count_literal;
    struct planned_select_predicate predicate;
    struct planned_select_limit limit;
    struct planned_select derived_select;
    struct planned_row_scalar_select derived_row_scalar_select;
    struct planned_grouped_aggregate *derived_grouped_aggregate;
    struct planned_count_derived_compound derived_compound;
};

enum planned_count_expression_aggregate_item_kind {
    PLANNED_COUNT_EXPRESSION_AGGREGATE_STAR = 0,
    PLANNED_COUNT_EXPRESSION_AGGREGATE_NULLIF_PREDICATE = 1,
    PLANNED_COUNT_EXPRESSION_AGGREGATE_ROW_SCALAR = 2,
};

struct planned_count_expression_aggregate_item {
    enum planned_count_expression_aggregate_item_kind kind;
    char *result_label;
    bool is_distinct;
    struct planned_select_predicate predicate;
    struct planned_row_scalar_expression row_scalar_expression;
};

struct planned_count_expression_aggregate {
    struct planned_select source;
    struct planned_count_expression_aggregate_item *items;
    size_t item_count;
    bool is_distinct;
    bool calc_found_rows;
    struct planned_select_limit limit;
};

struct planned_count_source_nodes {
    const struct mylite_sql_ast_node *from_clause;
    const struct mylite_sql_ast_node *where_clause;
    const struct mylite_sql_ast_node *order_clause;
    const struct mylite_sql_ast_node *count_expression;
};

enum planned_column_aggregate_function {
    PLANNED_COLUMN_AGGREGATE_NONE = 0,
    PLANNED_COLUMN_AGGREGATE_MIN = 1,
    PLANNED_COLUMN_AGGREGATE_MAX = 2,
    PLANNED_COLUMN_AGGREGATE_SUM = 3,
    PLANNED_COLUMN_AGGREGATE_AVG = 4,
    PLANNED_COLUMN_AGGREGATE_BIT_AND = 5,
    PLANNED_COLUMN_AGGREGATE_BIT_OR = 6,
    PLANNED_COLUMN_AGGREGATE_BIT_XOR = 7,
    PLANNED_COLUMN_AGGREGATE_GROUP_CONCAT = 8,
    PLANNED_COLUMN_AGGREGATE_JSON_ARRAYAGG = 9,
    PLANNED_COLUMN_AGGREGATE_JSON_OBJECTAGG = 10,
    PLANNED_COLUMN_AGGREGATE_STDDEV_POP = 11,
    PLANNED_COLUMN_AGGREGATE_STDDEV_SAMP = 12,
    PLANNED_COLUMN_AGGREGATE_VAR_POP = 13,
    PLANNED_COLUMN_AGGREGATE_VAR_SAMP = 14,
    PLANNED_COLUMN_AGGREGATE_COUNT_STAR = 15,
    PLANNED_COLUMN_AGGREGATE_COUNT_COLUMN = 16,
    PLANNED_COLUMN_AGGREGATE_COUNT_LITERAL = 17,
    PLANNED_COLUMN_AGGREGATE_COUNT_DISTINCT_COLUMN = 18,
    PLANNED_COLUMN_AGGREGATE_ANY_VALUE = 19,
    PLANNED_COLUMN_AGGREGATE_ST_COLLECT = 20,
};

struct planned_scalar_aggregate_subquery {
    enum planned_column_aggregate_function function;
    struct mylite_catalog_column_descriptor column;
    size_t column_source_index;
    struct planned_exists_subquery subquery;
};

struct planned_column_aggregate {
    char *result_label;
    enum planned_column_aggregate_function function;
    bool has_source;
    struct table_name_resolution source;
    struct mylite_catalog_table_descriptor table;
    struct mylite_catalog_column_descriptor *aggregate_column;
    bool aggregate_uses_row_scalar_expression;
    struct planned_row_scalar_expression aggregate_key_row_scalar_expression;
    struct planned_row_scalar_expression aggregate_row_scalar_expression;
    bool aggregate_has_projection_rhs;
    enum mylite_sql_ast_operator aggregate_projection_operator;
    struct planned_row_scalar_expression aggregate_projection_rhs;
    struct planned_select_order aggregate_order;
    bool aggregate_is_distinct;
    char aggregate_collation_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    bool has_spatial_wrapper;
    enum mylite_spatial_function_kind spatial_wrapper_kind;
    bool has_separator;
    struct planned_value separator;
    struct planned_value count_literal;
    struct planned_select_predicate predicate;
    struct planned_select_limit limit;
};

struct planned_column_aggregate_select {
    bool has_source;
    struct table_name_resolution source;
    struct mylite_catalog_table_descriptor table;
    struct planned_column_aggregate *items;
    size_t item_count;
    struct planned_select_predicate predicate;
    struct planned_select_limit limit;
};

struct column_aggregate_select_item_plan_input {
    const struct mylite_sql_ast_node *select_item;
    const struct mylite_sql_ast_node *from_clause;
    const struct mylite_sql_ast_node *where_clause;
    const struct select_source_context *source_context;
    const struct mylite_catalog_table_descriptor *table;
    const struct mylite_catalog_column_descriptor *table_columns;
    size_t table_column_count;
};

enum planned_grouped_aggregate_function {
    PLANNED_GROUPED_AGGREGATE_NONE = 0,
    PLANNED_GROUPED_AGGREGATE_COUNT_STAR = 1,
    PLANNED_GROUPED_AGGREGATE_COUNT_COLUMN = 2,
    PLANNED_GROUPED_AGGREGATE_COUNT_DISTINCT_COLUMN = 3,
    PLANNED_GROUPED_AGGREGATE_MIN = 4,
    PLANNED_GROUPED_AGGREGATE_MAX = 5,
    PLANNED_GROUPED_AGGREGATE_SUM = 6,
    PLANNED_GROUPED_AGGREGATE_AVG = 7,
    PLANNED_GROUPED_AGGREGATE_BIT_AND = 8,
    PLANNED_GROUPED_AGGREGATE_BIT_OR = 9,
    PLANNED_GROUPED_AGGREGATE_BIT_XOR = 10,
    PLANNED_GROUPED_AGGREGATE_GROUP_CONCAT = 11,
    PLANNED_GROUPED_AGGREGATE_JSON_ARRAYAGG = 12,
    PLANNED_GROUPED_AGGREGATE_JSON_OBJECTAGG = 13,
    PLANNED_GROUPED_AGGREGATE_ANY_VALUE = 14,
    PLANNED_GROUPED_AGGREGATE_STDDEV_POP = 15,
    PLANNED_GROUPED_AGGREGATE_STDDEV_SAMP = 16,
    PLANNED_GROUPED_AGGREGATE_VAR_POP = 17,
    PLANNED_GROUPED_AGGREGATE_VAR_SAMP = 18,
    PLANNED_GROUPED_AGGREGATE_ST_COLLECT = 19,
};

enum { grouped_aggregate_max_results = 16 };

enum { grouped_aggregate_max_group_keys = 4 };

struct planned_grouped_aggregate_item {
    char *result_label;
    enum planned_grouped_aggregate_function function;
    bool has_alias;
    bool alias_is_identifier;
    bool is_hidden_order_key;
    struct mylite_catalog_column_descriptor *aggregate_column;
    size_t aggregate_column_source_index;
    bool aggregate_uses_row_scalar_expression;
    struct planned_row_scalar_expression aggregate_key_row_scalar_expression;
    struct planned_row_scalar_expression aggregate_row_scalar_expression;
    struct planned_select_order aggregate_order;
    bool aggregate_is_distinct;
    char aggregate_collation_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    bool has_spatial_wrapper;
    enum mylite_spatial_function_kind spatial_wrapper_kind;
    bool has_separator;
    struct planned_value separator;
};

struct planned_grouped_key {
    char *alias;
    bool uses_row_scalar_expression;
    struct planned_row_scalar_expression row_scalar_expression;
    struct mylite_catalog_column_descriptor *column;
    size_t column_source_index;
};

struct grouped_alias_group_resolution {
    const struct mylite_sql_ast_node *select_list;
    const struct mylite_sql_ast_node *group_key;
    const struct select_source_context *source_context;
    const struct mylite_catalog_column_descriptor *table_columns;
    size_t table_column_count;
};

struct planned_compound_select {
    struct planned_select *branches;
    size_t branch_count;
};

struct planned_grouped_projection {
    char *result_label;
    bool has_alias;
    bool alias_is_identifier;
    bool is_grouping_marker;
    bool uses_row_scalar_expression;
    struct planned_row_scalar_expression row_scalar_expression;
    struct mylite_catalog_column_descriptor *column;
    size_t column_source_index;
};

struct planned_grouped_aggregate {
    struct planned_grouped_key *groups;
    size_t group_count;
    struct planned_grouped_projection *projections;
    size_t projection_count;
    size_t projection_capacity;
    struct planned_grouped_aggregate_item *aggregates;
    size_t aggregate_count;
    struct table_name_resolution source;
    struct mylite_catalog_table_descriptor table;
    struct planned_select_source *sources;
    size_t source_count;
    enum mylite_sql_ast_join_kind join_kind;
    struct planned_select_join_condition join_condition;
    enum mylite_sql_ast_join_kind *join_kinds;
    struct planned_select_join_condition *join_conditions;
    size_t join_count;
    struct planned_select_predicate predicate;
    struct planned_grouped_having having;
    struct planned_select_order order;
    bool *order_item_uses_aggregate;
    size_t *order_item_aggregate_indexes;
    bool order_uses_aggregate;
    size_t order_aggregate_index;
    bool with_rollup;
    bool is_distinct;
    bool calc_found_rows;
    struct planned_select_limit limit;
};

_Static_assert(
    sizeof(struct planned_grouped_key) <= 1024U,
    "planned grouped keys must retain compact column references"
);

_Static_assert(
    sizeof(struct planned_grouped_projection) <= 1024U,
    "planned grouped projections must retain compact column references"
);

_Static_assert(
    sizeof(struct planned_grouped_aggregate_item) <= 2048U,
    "planned grouped aggregate items must retain compact column references"
);

_Static_assert(
    sizeof(struct planned_column_aggregate) <= 16384U,
    "planned column aggregates must retain compact column references"
);

enum planned_view_statement_kind {
    PLANNED_VIEW_CREATE = 0,
    PLANNED_VIEW_CREATE_OR_REPLACE = 1,
    PLANNED_VIEW_ALTER = 2,
};

struct planned_create_view {
    enum planned_view_statement_kind statement_kind;
    struct table_name_resolution target;
    struct planned_select source;
    char **column_names;
    char **select_column_names;
    size_t column_count;
    bool has_explicit_column_names;
    char algorithm[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char check_option[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char is_updatable[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char definer[MYLITE_CATALOG_DEFINER_CAPACITY];
    char security_type[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char *view_definition;
    char *show_create_sql;
};

struct grouped_aggregate_clauses {
    const struct mylite_sql_ast_node *where_clause;
    const struct mylite_sql_ast_node *group_clause;
    const struct mylite_sql_ast_node *having_clause;
    const struct mylite_sql_ast_node *order_clause;
    const struct mylite_sql_ast_node *limit_clause;
};

struct avg_accumulator {
    int64_t sum;
    int64_t count;
};

struct uint128_parts {
    uint64_t high;
    uint64_t low;
};

struct planned_show_create_table {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct mylite_catalog_view_descriptor view;
    struct mylite_catalog_column_descriptor *columns;
    size_t column_count;
    struct loaded_table_columns_cache_entry *columns_cache_entry;
    struct loaded_table_key_metadata_reference key_metadata;
    const struct loaded_index_info *indexes;
    size_t index_count;
    struct loaded_foreign_key_info *foreign_keys;
    size_t foreign_key_count;
    struct loaded_check_constraint_info *check_constraints;
    size_t check_constraint_count;
    const struct mylite_execution_catalog_builtin_sys_view *builtin_sys_view;
    bool is_view;
    bool target_was_schema_qualified;
};

struct planned_delete {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct planned_select_source *sources;
    size_t source_count;
    enum mylite_sql_ast_join_kind join_kind;
    struct planned_select_join_condition join_condition;
    size_t target_source_index;
    size_t target_source_indexes[2];
    size_t target_source_count;
    struct planned_select_predicate predicate;
    struct planned_select_order order;
    struct planned_select_limit limit;
    const char *rowid_alias;
    bool is_joined;
};

struct planned_update_assignment {
    struct mylite_catalog_column_descriptor column;
    const struct mylite_sql_ast_node *value_node;
    enum planned_update_assignment_value_kind value_kind;
    struct planned_row_scalar_expression row_scalar_expression;
    struct planned_value value;
    bool generated_default_noop;
};

struct planned_update {
    struct planned_select_source *sources;
    size_t source_count;
    size_t target_source_index;
    struct planned_update_assignment *assignments;
    size_t assignment_count;
    struct loaded_index_info *indexes;
    size_t index_count;
    const struct mylite_sql_ast_node *assignment_value_node;
    uint64_t arithmetic_delta_magnitude;
    int64_t arithmetic_delta;
    int64_t date_interval_value;
    struct mylite_catalog_column_descriptor *columns;
    size_t column_count;
    struct loaded_table_columns_cache_entry *columns_cache_entry;
    const char *rowid_alias;
    size_t primary_key_column_index;
    int64_t primary_key_column_id;
    struct planned_select_predicate predicate;
    struct planned_select_limit limit;
    struct planned_value assignment_value;
    struct planned_value auto_update_value;
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct mylite_catalog_column_descriptor assignment_column;
    struct mylite_catalog_column_descriptor assignment_source_column;
    struct planned_select_order order;
    struct planned_select_join_condition join_condition;
    struct planned_row_scalar_expression assignment_row_scalar_expression;
    struct planned_row_scalar_expression assignment_scalar_aggregate_rhs;
    struct planned_select assignment_subquery;
    struct planned_column_aggregate assignment_scalar_aggregate;
    enum mylite_sql_ast_join_kind join_kind;
    enum planned_update_assignment_value_kind assignment_value_kind;
    enum mylite_sql_ast_operator arithmetic_operator;
    enum mylite_date_interval_second_input_kind date_interval_input_kind;
    enum mylite_date_interval_unit date_interval_unit;
    enum mylite_sql_ast_operator assignment_scalar_aggregate_operator;
    bool arithmetic_delta_is_valid;
    bool date_interval_is_null;
    bool date_interval_subtract;
    bool assignment_value_is_scalar_subquery;
    bool assignment_value_is_scalar_aggregate_expression_subquery;
    bool assignment_scalar_aggregate_has_rhs;
    bool has_primary_key;
    bool is_joined;
    bool low_priority;
    bool ignore_errors;
    bool generated_default_noop;
};

struct update_optional_clauses {
    const struct mylite_sql_ast_node *where_clause;
    const struct mylite_sql_ast_node *order_clause;
    const struct mylite_sql_ast_node *limit_clause;
};

struct information_schema_joined_update_clauses {
    const struct mylite_sql_ast_node *from_clause;
    const struct mylite_sql_ast_node *where_clause;
};

struct update_unique_key_conflict_bind_request {
    const struct planned_update *executable_plan;
    const struct planned_update *plan;
    const struct loaded_index_info *index;
};

struct table_option_name_policy {
    const char *identifier_kind;
    const char *nul_message;
};

enum show_variables_where_truth {
    SHOW_VARIABLES_WHERE_FALSE = 0,
    SHOW_VARIABLES_WHERE_TRUE = 1,
    SHOW_VARIABLES_WHERE_UNKNOWN = 2,
};

enum show_variables_where_column {
    SHOW_VARIABLES_WHERE_COLUMN_NONE = 0,
    SHOW_VARIABLES_WHERE_COLUMN_VARIABLE_NAME = 1,
    SHOW_VARIABLES_WHERE_COLUMN_VALUE = 2,
};

enum show_variables_where_eval_action {
    SHOW_VARIABLES_WHERE_VISIT = 1,
    SHOW_VARIABLES_WHERE_EVALUATE = 2,
};

struct show_variables_where_row {
    const char *variable_name;
    const char *value;
};

struct show_catalog_where_row {
    const char *const *columns;
    const char *const *values;
    size_t column_count;
};

struct show_catalog_where_comparison {
    enum mylite_sql_ast_operator operator_kind;
    int comparison;
};

struct show_variables_where_eval_frame {
    const struct mylite_sql_ast_node *node;
    enum show_variables_where_eval_action action;
};

struct show_variables_where_frame_stack {
    struct show_variables_where_eval_frame *items;
    size_t count;
    size_t capacity;
};

struct show_variables_where_truth_stack {
    enum show_variables_where_truth *items;
    size_t count;
    size_t capacity;
};

struct result_column_metadata_context {
    struct loaded_table_key_metadata_reference key_metadata;
    bool has_single_source_metadata;
};

enum cursor_plan_attempt_result {
    CURSOR_PLAN_ATTEMPT_READY = 0,
    CURSOR_PLAN_ATTEMPT_UNSUPPORTED,
    CURSOR_PLAN_ATTEMPT_FAILED,
};

struct mylite_select_analysis_session_key {
    uint64_t sql_mode;
    uint64_t last_insert_id;
    int time_zone_offset_minutes;
    bool sql_auto_is_null;
};

struct mylite_select_parameter_context_frame {
    const struct mylite_sql_ast_node *node;
    enum mylite_sql_ast_node_kind parent_kind;
    enum mylite_sql_ast_node_kind grandparent_kind;
    size_t child_index;
};

struct mylite_select_parameter_context_stack {
    struct mylite_select_parameter_context_frame *items;
    size_t count;
    size_t capacity;
};

struct mylite_analyzed_select_plan {
    struct planned_select plan;
    char *lowered_sql;
    enum mylite_stmt_binding_type *binding_types;
    struct mylite_select_analysis_session_key session_key;
    uint64_t catalog_generation;
    uint64_t sqlite_schema_generation;
    size_t binding_type_count;
    bool parameter_values_are_reusable;
    bool valid;
};

struct mylite_stmt {
    struct mylite_db *database;
    struct mylite_stmt *next_live_statement;
    struct mylite_statement_context context;
    struct mylite_statement_completion completion;
    struct mylite_sql_parse_result parse_result;
    struct mylite_execution_normalized_sql normalized_sql;
    const struct mylite_sql_ast_node *statement;
    struct mylite_analyzed_select_plan analyzed_select;
    struct result_column_metadata_context metadata_context;
    struct mylite_execution_result_row_storage row_storage;
    struct mylite_statement_transaction read_transaction;
    sqlite3_stmt *sqlite_statement;
    mylite_result *metadata_result;
    mylite_result *completed_result;
    struct mylite_stmt_binding *bindings;
    uint64_t statement_id;
    unsigned int parse_modes;
    size_t parameter_count;
    size_t row_count;
    size_t materialized_row_index;
    size_t current_materialized_row;
    bool has_context;
    bool has_parse_result;
    bool has_normalized_sql;
    bool has_materialized_rows;
    bool buffered_results;
    bool returns_rows;
    bool analyzing_unbound_parameters;
    bool parameter_plan_reusable;
    bool sqlite_statement_is_cached;
    bool current_row_available;
    bool done;
};

struct find_check_constraint_name_context {
    const char *name;
    bool found;
};

struct check_constraint_name_collision_context {
    const char *name;
    int64_t excluded_table_id;
    bool found;
};

struct validate_parent_foreign_keys_context {
    struct mylite_db *database;
    int rc;
};

struct foreign_key_exists_context {
    bool found;
};

struct foreign_key_index_exists_context {
    int64_t index_id;
    bool found;
};

struct loaded_index_column_lookup {
    const struct loaded_index_info *indexes;
    size_t index_count;
    int64_t skipped_index_id;
    int64_t column_id;
};

struct nonprimary_index_presence_context {
    bool has_nonprimary_index;
};

struct nonprimary_index_count_context {
    int64_t count;
};

struct show_tables_context {
    struct mylite_db *database;
    mylite_result *result;
    const struct show_like_filter *filter;
    const struct mylite_sql_ast_node *where_clause;
    const char *table_name_column;
    bool is_full;
};

struct show_tables_filter_nodes {
    const struct mylite_sql_ast_node *schema;
    const struct mylite_sql_ast_node *like;
    const struct mylite_sql_ast_node *where;
};

struct show_schema_resolution {
    struct mylite_catalog_schema_descriptor schema;
    const struct mylite_execution_catalog_builtin_schema_table_directory *builtin_directory;
};

struct table_status_values {
    int64_t row_count;
    char row_count_text[integer_text_capacity];
    char average_row_length_text[integer_text_capacity];
    char index_length_text[integer_text_capacity];
    char auto_increment_text[integer_text_capacity];
    char create_time_text[datetime_text_length + 1U];
    char update_time_text[datetime_text_length + 1U];
    char row_format_text[sizeof("Compressed")];
    char create_options_text[table_status_create_options_capacity];
    const char *index_length;
    const char *auto_increment;
    const char *create_time;
    const char *update_time;
    const char *row_format;
    const char *create_options;
};

struct show_table_status_context {
    struct mylite_db *database;
    mylite_result *result;
    const struct show_like_filter *filter;
    const struct mylite_sql_ast_node *where_clause;
};

struct show_table_status_filter_nodes {
    const struct mylite_sql_ast_node *schema;
    const struct mylite_sql_ast_node *like;
    const struct mylite_sql_ast_node *where;
};

struct show_table_status_where_comparison {
    int value;
};

struct show_table_status_where_operand {
    const char *value;
    bool case_sensitive;
    bool numeric;
    struct session_scalar_cell cell;
};

struct show_databases_where_comparison {
    int value;
};

struct show_metadata_regexp_messages {
    const char *literal;
    const char *nul;
    const char *ascii;
    const char *unsupported;
};

struct show_index_filter_nodes {
    const struct mylite_sql_ast_node *table;
    const struct mylite_sql_ast_node *schema;
    const struct mylite_sql_ast_node *where;
};

struct show_columns_context {
    struct mylite_db *database;
    mylite_result *result;
    const struct show_like_filter *filter;
    const struct mylite_sql_ast_node *where_clause;
    const struct mylite_catalog_table_descriptor *table;
    const struct primary_key_info *primary_key;
    const struct loaded_index_info *indexes;
    size_t index_count;
    bool full;
};

struct show_columns_target_nodes {
    const struct mylite_sql_ast_node *table;
    const struct mylite_sql_ast_node *schema;
};

struct show_columns_filter_nodes {
    const struct mylite_sql_ast_node *table;
    const struct mylite_sql_ast_node *schema;
    const struct mylite_sql_ast_node *like;
    const struct mylite_sql_ast_node *where;
};

struct information_schema_character_metadata {
    const char *character_maximum_length;
    const char *character_octet_length;
    const char *character_set_name;
    const char *collation_name;
};

struct information_schema_innodb_column_type_info {
    int64_t mtype;
    int64_t prtype;
    int64_t length;
};

struct information_schema_innodb_character_type_request {
    int64_t mtype;
    int64_t mysql_type_code;
};

struct information_schema_row_set {
    const struct mylite_execution_catalog_table_definition *definition;
    const struct information_schema_query *query;
    const struct mylite_sql_ast_node *predicate;
    const bool *required_columns;
    mylite_result *stream_result;
    struct information_schema_result_projection_buffers *stream_buffers;
    char ***rows;
    size_t row_count;
    size_t row_capacity;
    size_t row_limit;
    size_t row_offset;
    size_t skipped_row_count;
    bool has_row_limit;
    bool offset_applied;
    bool count_only;
    bool predicate_applied;
    bool exclude_connection_control_failed_login_attempts;
};

struct information_schema_row_index_set {
    size_t *items;
    size_t count;
    size_t capacity;
    size_t stack_items[information_schema_row_index_stack_capacity];
    bool uses_heap;
};

struct information_schema_columns_numeric_metadata_buffers {
    char *numeric_precision_text;
    size_t numeric_precision_text_size;
    char *numeric_scale_text;
    size_t numeric_scale_text_size;
    const char **values;
};

struct information_schema_row_order_pair {
    size_t left_row;
    size_t right_row;
};

enum information_schema_row_index_sort_kind {
    INFORMATION_SCHEMA_ROW_INDEX_SORT_ORDER = 0,
    INFORMATION_SCHEMA_ROW_INDEX_SORT_TABLES_DEFAULT = 1,
    INFORMATION_SCHEMA_ROW_INDEX_SORT_GROUPED_STATISTICS = 2,
};

struct information_schema_row_index_sort {
    const struct information_schema_query *query;
    const struct information_schema_row_set *rows;
    enum information_schema_row_index_sort_kind kind;
};

struct information_schema_merge_range {
    size_t left;
    size_t middle;
    size_t right;
};

struct information_schema_group_concat_order {
    size_t start_index;
    size_t end_index;
    size_t column_index;
};

struct information_schema_row_order_column {
    size_t column_index;
    enum planned_select_order_direction direction;
};

struct information_schema_predicate_value {
    char *text;
    bool is_null;
    bool is_numeric;
    char like_escape_character;
};

struct enum_label_descriptor {
    const char *text;
    size_t text_length;
    size_t character_length;
};

struct enum_type_info {
    char decoded_storage[MYLITE_CATALOG_TYPE_NAME_CAPACITY];
    size_t decoded_length;
    struct enum_label_descriptor labels[enum_label_count_capacity];
    size_t label_count;
    size_t max_label_character_length;
};

struct set_type_info {
    char decoded_storage[MYLITE_CATALOG_TYPE_NAME_CAPACITY];
    size_t decoded_length;
    struct enum_label_descriptor members[set_member_count_capacity];
    size_t member_count;
    size_t max_display_character_length;
};

struct set_predicate_member_text_request {
    struct mylite_dynamic_string *display;
    const struct set_type_info *info;
    char *text;
    size_t token_start;
    size_t token_end;
    size_t member_index;
    bool found_member;
};

enum enum_string_trailing_space_policy {
    ENUM_STRING_PRESERVE_TRAILING_SPACES,
    ENUM_STRING_TRIM_TRAILING_SPACES,
};

enum information_schema_predicate_eval_action {
    INFORMATION_SCHEMA_PREDICATE_VISIT = 0,
    INFORMATION_SCHEMA_PREDICATE_EVALUATE = 1,
};

enum information_schema_truth_value {
    INFORMATION_SCHEMA_TRUTH_FALSE = 0,
    INFORMATION_SCHEMA_TRUTH_TRUE = 1,
    INFORMATION_SCHEMA_TRUTH_UNKNOWN = 2,
};

struct information_schema_predicate_eval_frame {
    const struct mylite_sql_ast_node *node;
    enum information_schema_predicate_eval_action action;
};

struct information_schema_predicate_frame_stack {
    struct information_schema_predicate_eval_frame stack_items[if_stack_initial_capacity];
    struct information_schema_predicate_eval_frame *items;
    size_t count;
    size_t capacity;
    bool uses_heap;
};

struct information_schema_truth_stack {
    enum information_schema_truth_value stack_items[if_stack_initial_capacity];
    enum information_schema_truth_value *items;
    size_t count;
    size_t capacity;
    bool uses_heap;
};

enum information_schema_projection_kind {
    INFORMATION_SCHEMA_PROJECTION_COLUMN = 0,
    INFORMATION_SCHEMA_PROJECTION_UNSIGNED_INTEGER_EXPRESSION = 1,
    INFORMATION_SCHEMA_PROJECTION_INTEGER_LITERAL = 2,
    INFORMATION_SCHEMA_PROJECTION_GROUP_CONCAT_ORDERED_COLUMN = 3,
    INFORMATION_SCHEMA_PROJECTION_LOGICAL_NOT_COLUMN = 4,
};

struct information_schema_query {
    const struct mylite_execution_catalog_table_definition *definition;
    const struct mylite_execution_catalog_mysql_system_table *system_table_definition;
    const char *source_schema_name;
    bool *required_columns;
    char alias[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    bool has_alias;
    bool is_count_star;
    const struct mylite_sql_ast_node *count_expression;
    const struct mylite_sql_ast_node *count_alias;
    size_t *projection_indexes;
    enum information_schema_projection_kind *projection_kinds;
    const struct mylite_sql_ast_node **projection_expressions;
    char **projection_names;
    size_t projection_count;
    bool has_group;
    size_t group_indexes[3];
    size_t group_index_count;
    bool has_order;
    size_t order_index;
    enum planned_select_order_direction order_direction;
    size_t order_indexes[information_schema_order_key_capacity];
    enum planned_select_order_direction order_directions[information_schema_order_key_capacity];
    size_t order_count;
    struct planned_select_limit limit;
};

struct information_schema_catalog_filter {
    char *schema_name;
    char *table_name;
    char *table_type;
    size_t schema_column_index;
    size_t table_column_index;
    size_t table_type_column_index;
    bool has_schema_name;
    bool has_table_name;
    bool has_table_type;
};

struct information_schema_projection_slot_request {
    size_t column_index;
    enum information_schema_projection_kind kind;
    const struct mylite_sql_ast_node *expression;
    const struct mylite_sql_ast_node *alias;
};

struct information_schema_result_projection_buffers {
    const char **values;
    const char *stack_values[information_schema_projection_stack_capacity];
    char *computed_values;
    char stack_computed_values[information_schema_projection_stack_capacity][integer_text_capacity];
    bool values_use_heap;
    bool computed_values_use_heap;
};

enum information_schema_numeric_eval_action {
    INFORMATION_SCHEMA_NUMERIC_EVAL_ENTER = 1,
    INFORMATION_SCHEMA_NUMERIC_EVAL_APPLY_DIVIDE = 2,
};

struct information_schema_numeric_eval_frame {
    enum information_schema_numeric_eval_action action;
    const struct mylite_sql_ast_node *expression;
};

struct information_schema_numeric_eval_frame_stack {
    struct information_schema_numeric_eval_frame *items;
    size_t count;
    size_t capacity;
};

struct information_schema_numeric_eval_value {
    double value;
    bool is_null;
};

struct information_schema_numeric_eval_value_stack {
    struct information_schema_numeric_eval_value *items;
    size_t count;
    size_t capacity;
};

struct information_schema_catalog_context {
    struct mylite_db *database;
    struct information_schema_row_set *rows;
    const struct mylite_catalog_schema_descriptor *schema;
    const struct information_schema_catalog_filter *filter;
};

struct mysql_system_table_catalog_context {
    struct mylite_db *database;
    struct information_schema_row_set *rows;
    const struct mylite_execution_catalog_mysql_system_table *definition;
    const struct mylite_catalog_schema_descriptor *schema;
};

struct sys_schema_auto_increment_columns_context {
    struct mylite_db *database;
    struct information_schema_row_set *rows;
    const struct mylite_catalog_schema_descriptor *schema;
};

struct sys_schema_object_overview_group {
    char db[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char object_type[sizeof("INDEX (FULLTEXT)")];
    uint64_t count;
};

struct sys_schema_object_overview_context {
    struct mylite_db *database;
    struct information_schema_row_set *rows;
    struct sys_schema_object_overview_group *groups;
    size_t group_count;
    size_t group_capacity;
    const struct mylite_catalog_schema_descriptor *schema;
};

struct sys_schema_index_statistics_context {
    struct mylite_db *database;
    struct information_schema_row_set *rows;
    bool formatted_latency;
    const struct mylite_catalog_schema_descriptor *schema;
};

struct sys_schema_unused_indexes_context {
    struct mylite_db *database;
    struct information_schema_row_set *rows;
    const struct mylite_catalog_schema_descriptor *schema;
};

struct sys_schema_table_statistics_context {
    struct mylite_db *database;
    struct information_schema_row_set *rows;
    bool formatted_metrics;
    bool include_buffer_metrics;
    const struct mylite_catalog_schema_descriptor *schema;
};

struct sys_schema_redundant_indexes_context {
    struct mylite_db *database;
    struct information_schema_row_set *rows;
    const struct mylite_catalog_schema_descriptor *schema;
};

struct sys_flattened_key_row {
    char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char table_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char index_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    bool non_unique;
    bool subpart_exists;
    char *index_columns;
};

struct sys_flattened_key_row_set {
    struct sys_flattened_key_row *rows;
    size_t count;
    size_t capacity;
};

struct information_schema_innodb_foreign_action_type_flags {
    int64_t cascade;
    int64_t set_null;
    int64_t no_action;
};

struct show_database_name {
    char name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
};

struct show_databases_context {
    struct mylite_db *database;
    struct show_database_name *names;
    size_t name_count;
    size_t name_capacity;
};

struct collect_drop_schema_tables_context {
    struct mylite_db *database;
    struct planned_drop_schema *plan;
};

struct string_bitmask_scalar_text_argument {
    struct mylite_string_bitmask_slice slice;
    struct session_scalar_cell cell;
    char *owned_text;
};

enum scalar_integer_cast_target {
    SCALAR_INTEGER_CAST_SIGNED,
    SCALAR_INTEGER_CAST_UNSIGNED,
};

enum scalar_placeholder_cast_target {
    SCALAR_PLACEHOLDER_CAST_TEXT,
    SCALAR_PLACEHOLDER_CAST_BINARY,
    SCALAR_PLACEHOLDER_CAST_DATE,
    SCALAR_PLACEHOLDER_CAST_TIME,
    SCALAR_PLACEHOLDER_CAST_DATETIME,
    SCALAR_PLACEHOLDER_CAST_DECIMAL,
    SCALAR_PLACEHOLDER_CAST_JSON,
};

struct scalar_integer_cast_parse {
    bool is_negative;
    bool saw_digits;
    bool overflowed;
    bool has_truncated_integer_warning;
    uint64_t magnitude;
};

struct scalar_integer_cast_digit_source {
    const char *text;
    size_t offset;
    uint64_t limit;
};

struct scalar_integer_cast_messages {
    const char *unsupported;
    const char *signed_value;
    const char *embedded_nul;
};

struct scalar_text_conversion_messages {
    const char *unsupported;
    const char *signed_value;
    const char *string_unsupported;
    const char *embedded_nul;
};

struct scalar_placeholder_cast_info {
    enum scalar_placeholder_cast_target target;
    bool has_length;
    bool has_decimal_scale;
    size_t length;
    size_t decimal_scale;
};

struct field_scalar_argument {
    enum planned_row_scalar_field_domain domain;
    bool is_null;
    int64_t integer;
    bool has_numeric_real;
    double numeric_real;
    size_t numeric_scale;
    char *text;
    size_t text_length;
};

struct field_scalar_argument_list {
    const struct field_scalar_argument *values;
    size_t count;
};

struct greatest_least_scalar_selection {
    size_t argument_count;
    enum planned_row_scalar_field_domain domain;
    bool is_greatest;
};

struct scalar_arithmetic_operation {
    enum mylite_sql_ast_operator operator_kind;
    int64_t left;
    int64_t right;
};

struct scalar_comparison_operation {
    enum mylite_sql_ast_operator operator_kind;
    int64_t left;
    int64_t right;
};

enum row_constructor_scalar_truth {
    ROW_CONSTRUCTOR_SCALAR_FALSE = 0,
    ROW_CONSTRUCTOR_SCALAR_TRUE = 1,
    ROW_CONSTRUCTOR_SCALAR_UNKNOWN = 2,
};

struct row_constructor_scalar_item {
    const struct mylite_sql_ast_node *expression;
    struct session_scalar_cell text_cell;
    struct scalar_arithmetic_value numeric_value;
    bool uses_text_comparison;
    bool has_numeric_value;
};

struct row_constructor_scalar_values {
    struct row_constructor_scalar_item *items;
    size_t count;
};

struct row_constructor_scalar_result {
    enum row_constructor_scalar_truth truth;
    size_t warning_count;
};

enum scalar_arithmetic_eval_frame_kind {
    SCALAR_ARITHMETIC_EVAL_ENTER = 1,
    SCALAR_ARITHMETIC_EVAL_APPLY = 2,
    SCALAR_ARITHMETIC_EVAL_APPLY_UNARY = 3,
};

struct scalar_arithmetic_eval_frame {
    enum scalar_arithmetic_eval_frame_kind kind;
    const struct mylite_sql_ast_node *expression;
    enum mylite_sql_ast_operator operator_kind;
};

struct scalar_arithmetic_eval_stack {
    struct scalar_arithmetic_eval_frame *items;
    size_t count;
    size_t capacity;
};

struct scalar_arithmetic_value_stack {
    struct scalar_arithmetic_value *items;
    size_t count;
    size_t capacity;
};

enum scalar_bitwise_eval_frame_kind {
    SCALAR_BITWISE_EVAL_ENTER = 1,
    SCALAR_BITWISE_EVAL_APPLY = 2,
    SCALAR_BITWISE_EVAL_APPLY_UNARY = 3,
    SCALAR_BITWISE_EVAL_SHORT_CIRCUIT_OR_ENTER_RIGHT = 4,
};

struct scalar_bitwise_eval_frame {
    enum scalar_bitwise_eval_frame_kind kind;
    const struct mylite_sql_ast_node *expression;
    enum mylite_sql_ast_operator operator_kind;
};

struct scalar_bitwise_eval_stack {
    struct scalar_bitwise_eval_frame *items;
    size_t count;
    size_t capacity;
};

struct scalar_bitwise_value_stack {
    struct scalar_bitwise_value *items;
    size_t count;
    size_t capacity;
};

struct scalar_arithmetic_node_stack {
    const struct mylite_sql_ast_node **items;
    size_t count;
    size_t capacity;
};

enum scalar_comparison_eval_frame_kind {
    SCALAR_COMPARISON_EVAL_ENTER = 1,
    SCALAR_COMPARISON_EVAL_APPLY = 2,
    SCALAR_COMPARISON_EVAL_SHORT_CIRCUIT_OR_ENTER_RIGHT = 3,
    SCALAR_COMPARISON_EVAL_APPLY_NOT = 4,
};

struct scalar_comparison_eval_frame {
    enum scalar_comparison_eval_frame_kind kind;
    const struct mylite_sql_ast_node *expression;
    enum mylite_sql_ast_operator operator_kind;
};

struct scalar_comparison_eval_stack {
    struct scalar_comparison_eval_frame *items;
    size_t count;
    size_t capacity;
};

enum scalar_comparison_numeric_eval_frame_kind {
    SCALAR_COMPARISON_NUMERIC_EVAL_ENTER = 1,
    SCALAR_COMPARISON_NUMERIC_EVAL_APPLY = 2,
    SCALAR_COMPARISON_NUMERIC_EVAL_APPLY_UNARY = 3,
};

struct scalar_comparison_numeric_eval_frame {
    enum scalar_comparison_numeric_eval_frame_kind kind;
    const struct mylite_sql_ast_node *expression;
    enum mylite_sql_ast_operator operator_kind;
};

struct scalar_comparison_numeric_eval_stack {
    struct scalar_comparison_numeric_eval_frame *items;
    size_t count;
    size_t capacity;
};

enum scalar_logical_eval_frame_kind {
    SCALAR_LOGICAL_EVAL_ENTER = 1,
    SCALAR_LOGICAL_EVAL_APPLY_NOT = 2,
    SCALAR_LOGICAL_EVAL_APPLY_COMPARISON = 3,
    SCALAR_LOGICAL_EVAL_COMPARISON_SHORT_CIRCUIT_OR_ENTER_RIGHT = 4,
    SCALAR_LOGICAL_EVAL_APPLY_LOGICAL = 5,
    SCALAR_LOGICAL_EVAL_LOGICAL_SHORT_CIRCUIT_OR_ENTER_RIGHT = 6,
    SCALAR_LOGICAL_EVAL_APPLY_IS = 7,
};

struct scalar_logical_eval_frame {
    enum scalar_logical_eval_frame_kind kind;
    const struct mylite_sql_ast_node *expression;
    enum mylite_sql_ast_operator operator_kind;
};

struct scalar_logical_eval_stack {
    struct scalar_logical_eval_frame *items;
    size_t count;
    size_t capacity;
};

enum if_eval_frame_kind {
    IF_EVAL_FRAME_IF = 1,
    IF_EVAL_FRAME_IFNULL = 2,
    IF_EVAL_FRAME_COALESCE = 3,
    IF_EVAL_FRAME_NULLIF = 4,
    IF_EVAL_FRAME_ISNULL = 5,
};

struct if_eval_frame {
    enum if_eval_frame_kind kind;
    const struct mylite_sql_ast_node *first_value;
    const struct mylite_sql_ast_node *second_value;
    struct session_scalar_cell first_cell;
};

struct if_eval_stack {
    struct if_eval_frame *items;
    size_t count;
    size_t capacity;
};

struct if_validation_stack {
    const struct mylite_sql_ast_node **items;
    size_t count;
    size_t capacity;
};

struct system_variable_component {
    char text[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    bool quoted;
};

enum set_system_variable_scope {
    SET_SYSTEM_VARIABLE_SCOPE_NONE = 0,
    SET_SYSTEM_VARIABLE_SCOPE_SESSION = 1,
    SET_SYSTEM_VARIABLE_SCOPE_LOCAL = 2,
    SET_SYSTEM_VARIABLE_SCOPE_GLOBAL = 3,
};

struct resolved_set_system_variable_target {
    enum mylite_execution_system_variable_kind kind;
    enum set_system_variable_scope scope;
    bool is_system_variable_target;
    char name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
};

struct set_session_snapshot {
    uint64_t sql_mode;
    uint64_t insert_id;
    uint64_t auto_increment_increment;
    uint64_t auto_increment_offset;
    uint64_t temptable_max_ram;
    uint64_t sql_select_limit;
    uint64_t group_concat_max_len;
    uint64_t information_schema_stats_expiry;
    uint64_t max_error_count;
    uint64_t wait_timeout;
    uint64_t interactive_timeout;
    uint64_t long_query_time_microseconds;
    uint64_t lock_wait_timeout;
    uint64_t innodb_lock_wait_timeout;
    uint64_t net_read_timeout;
    uint64_t net_retry_count;
    uint64_t net_write_timeout;
    uint64_t connection_memory_chunk_size;
    uint64_t connection_memory_limit;
    uint64_t join_buffer_size;
    uint64_t myisam_sort_buffer_size;
    uint64_t sort_buffer_size;
    int64_t timestamp_override;
    int time_zone_offset_minutes;
    enum mylite_transaction_isolation session_transaction_isolation;
    enum mylite_transaction_access_mode session_transaction_access_mode;
    enum mylite_transaction_isolation next_transaction_isolation;
    enum mylite_transaction_access_mode next_transaction_access_mode;
    enum mylite_session_internal_tmp_mem_storage_engine internal_tmp_mem_storage_engine;
    enum mylite_session_myisam_stats_method myisam_stats_method;
    bool sql_mode_is_placeholder;
    bool time_zone_is_placeholder;
    bool character_set_state_is_placeholder;
    bool system_variables_are_placeholder;
    bool temptable_max_ram_has_override;
    bool rand_seed_state_initialized;
    bool big_tables;
    bool foreign_key_checks_enabled;
    bool sql_require_primary_key;
    bool low_priority_updates;
    bool autocommit_enabled;
    bool has_next_transaction_isolation;
    bool has_next_transaction_access_mode;
    bool next_transaction_isolation_from_system_variable;
    bool next_transaction_access_mode_from_system_variable;
    bool has_timestamp_override;
    char time_zone[MYLITE_SESSION_TIME_ZONE_CAPACITY];
    char character_set_client[MYLITE_SESSION_CHARSET_NAME_CAPACITY];
    char character_set_connection[MYLITE_SESSION_CHARSET_NAME_CAPACITY];
    char character_set_results[MYLITE_SESSION_CHARSET_NAME_CAPACITY];
    char collation_connection[MYLITE_SESSION_CHARSET_NAME_CAPACITY];
    char sql_mode_text[MYLITE_SESSION_SQL_MODE_TEXT_CAPACITY];
    uint32_t rand_seed1;
    uint32_t rand_seed2;
    struct mylite_session_user_variable *user_variables;
    size_t user_variable_count;
    size_t user_variable_capacity;
    struct mylite_session_system_variable_override *system_variable_overrides;
    size_t system_variable_override_count;
    size_t system_variable_override_capacity;
};
