#include <mylite/mylite.h>

#include "mylite_ast.h"
#include "mylite_catalog.h"
#include "mylite_connection.h"
#include "mylite_diagnostics.h"
#include "mylite_parser.h"
#include "mylite_result.h"
#include "mylite_sqlite_registration.h"
#include "mylite_statement_context.h"
#include "sqlite3.h"

#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    mysql_error_parse = 1064,
    mysql_error_no_database_selected = 1046,
    mysql_error_database_exists = 1007,
    mysql_error_cant_drop_database = 1008,
    mysql_error_unknown_database = 1049,
    mysql_error_table_exists = 1050,
    mysql_error_column_ambiguous = 1052,
    mysql_error_not_unique_table_alias = 1066,
    mysql_error_unknown_column = 1054,
    mysql_error_incorrect_parameter_count = 1582,
    mysql_error_unknown_table = 1051,
    mysql_error_identifier_too_long = 1059,
    mysql_error_duplicate_column = 1060,
    mysql_error_cant_remove_all_fields = 1090,
    mysql_error_cant_drop_field_or_key = 1091,
    mysql_error_incorrect_database_name = 1102,
    mysql_error_incorrect_table_name = 1103,
    mysql_error_unknown = 1105,
    mysql_error_column_specified_twice = 1110,
    mysql_error_unknown_character_set = 1115,
    mysql_error_column_count_mismatch = 1136,
    mysql_error_table_does_not_exist = 1146,
    mysql_error_incorrect_column_name = 1166,
    mysql_error_unknown_system_variable = 1193,
    mysql_error_session_variable_only = 1238,
    mysql_error_data_out_of_range = 1264,
    mysql_error_data_truncated = 1265,
    mysql_error_unknown_collation = 1273,
    mysql_warning_deprecated_system_variable = 1287,
    mysql_warning_information_schema_processlist_deprecated = 1287,
    mysql_error_invalid_default = 1067,
    mysql_error_field_no_default = 1364,
    mysql_error_bad_null = 1048,
    mysql_error_unknown_storage_engine = 1286,
    mysql_error_display_width_out_of_range = 1439,
    mysql_warning_integer_display_width_deprecated = 1681,
    mysql_error_must_have_visible_column = 4028,
    sqlite_use_nul_terminated_string = -1,
    decimal_base = 10,
    table_name_part_capacity = 3,
    integer_text_capacity = 32,
    literal_projection_max_significant_digits = 81,
    literal_projection_text_capacity = literal_projection_max_significant_digits + 2,
    show_create_integer_default_text_capacity = integer_text_capacity + sizeof(" DEFAULT ''"),
    system_variable_body_offset = 2,
    show_columns_result_column_count = 6,
    show_columns_extra_column = 5,
    show_index_result_column_count = 15,
    show_create_table_result_column_count = 2,
    show_create_database_result_column_count = 2,
    show_table_status_result_column_count = 18,
    show_table_status_data_length = 16384,
    show_character_set_result_column_count = 4,
    show_collation_result_column_count = 7,
    show_triggers_result_column_count = 11,
    show_events_result_column_count = 15,
    show_open_tables_result_column_count = 4,
    show_routine_status_result_column_count = 12,
    show_processlist_result_column_count = 8,
    show_warnings_result_column_count = 3,
    show_count_warnings_result_column_count = 1,
    show_errors_result_column_count = 3,
    show_count_errors_result_column_count = 1,
    show_processlist_info_truncation_length = 100,
    show_processlist_db_column = 3,
    show_processlist_info_column = 7,
    show_engines_result_column_count = 6,
    select_item_alias_max_length = 256,
    select_item_alias_capacity = select_item_alias_max_length + 1,
};

struct table_name_resolution {
    struct mylite_catalog_schema_descriptor schema;
    char table_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
};

struct select_source_context {
    const struct table_name_resolution *source;
    char alias[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    bool has_alias;
};

enum column_reference_diagnostic_context {
    COLUMN_REFERENCE_FIELD = 0,
    COLUMN_REFERENCE_WHERE = 1,
    COLUMN_REFERENCE_ORDER = 2,
};

struct planned_column {
    char name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    const char *logical_type;
    const char *physical_type;
    bool is_nullable;
    const struct mylite_sql_ast_node *default_node;
    enum mylite_catalog_column_default_kind default_kind;
    int64_t default_integer;
};

struct planned_create_table {
    struct table_name_resolution target;
    struct planned_column *columns;
    size_t column_count;
};

struct planned_drop_table_target {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    bool missing;
};

struct planned_drop_table {
    struct planned_drop_table_target *targets;
    size_t target_count;
    size_t missing_count;
    size_t existing_count;
};

struct planned_rename_table {
    struct table_name_resolution source;
    struct table_name_resolution target;
};

struct planned_rename_table_statement {
    struct planned_rename_table *pairs;
    size_t pair_count;
};

struct planned_alter_table_add_column {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct planned_column column;
    int64_t ordinal_position;
};

struct planned_alter_table_drop_column {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct mylite_catalog_column_descriptor column;
    size_t column_count;
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
    bool is_noop;
    bool is_metadata_only;
    bool checks_duplicate_replacement;
    bool reports_rebuild_row_count;
    const char *unsupported_object_message;
    const char *rowid_alias_message;
    const char *integer_support_message;
    const char *row_count_overflow_message;
    const char *failure_message;
    const char *rowid_alias;
    int64_t affected_rows;
};

struct planned_alter_table_set_default {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct mylite_catalog_column_descriptor original_column;
    struct planned_column column;
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
};

struct planned_value {
    bool is_null;
    int64_t integer;
};

struct integer_column_range {
    uint64_t positive_max;
    uint64_t negative_abs_max;
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
};

struct planned_insert {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct mylite_catalog_column_descriptor *columns;
    size_t column_count;
    struct planned_insert_row *rows;
    size_t row_count;
};

enum planned_select_predicate_kind {
    PLANNED_SELECT_PREDICATE_NONE = 0,
    PLANNED_SELECT_PREDICATE_COMPARISON = 1,
    PLANNED_SELECT_PREDICATE_IS_NULL = 2,
};

enum planned_select_order_direction {
    PLANNED_SELECT_ORDER_DEFAULT = 0,
    PLANNED_SELECT_ORDER_ASC = 1,
    PLANNED_SELECT_ORDER_DESC = 2,
};

struct planned_select_predicate {
    enum planned_select_predicate_kind kind;
    enum mylite_sql_ast_operator operator_kind;
    struct mylite_catalog_column_descriptor column;
    struct planned_value value;
};

struct planned_select_order {
    bool has_order;
    enum planned_select_order_direction direction;
    struct mylite_catalog_column_descriptor column;
};

struct planned_select_limit {
    bool has_limit;
    int64_t row_count;
    bool has_offset;
    int64_t offset;
};

struct planned_diagnostics_show_limit {
    bool has_limit;
    uint64_t row_count;
    bool has_offset;
    uint64_t offset;
};

struct planned_select {
    struct table_name_resolution source;
    struct mylite_catalog_table_descriptor table;
    struct mylite_catalog_column_descriptor *columns;
    const struct mylite_sql_ast_node **column_aliases;
    size_t column_count;
    bool is_distinct;
    struct planned_select_predicate predicate;
    struct planned_select_order order;
    struct planned_select_limit limit;
};

enum planned_count_function {
    PLANNED_COUNT_NONE = 0,
    PLANNED_COUNT_STAR = 1,
    PLANNED_COUNT_COLUMN = 2,
    PLANNED_COUNT_LITERAL = 3,
    PLANNED_COUNT_DISTINCT_COLUMN = 4,
};

struct planned_count {
    bool has_source;
    const struct mylite_sql_ast_node *expression;
    const struct mylite_sql_ast_node *alias;
    enum planned_count_function function;
    struct table_name_resolution source;
    struct mylite_catalog_table_descriptor table;
    struct mylite_catalog_column_descriptor count_column;
    struct planned_value count_literal;
    struct planned_select_predicate predicate;
};

struct planned_count_source_nodes {
    const struct mylite_sql_ast_node *from_clause;
    const struct mylite_sql_ast_node *where_clause;
    const struct mylite_sql_ast_node *count_expression;
};

enum planned_min_max_aggregate_function {
    PLANNED_MIN_MAX_AGGREGATE_NONE = 0,
    PLANNED_MIN_MAX_AGGREGATE_MIN = 1,
    PLANNED_MIN_MAX_AGGREGATE_MAX = 2,
};

struct planned_min_max_aggregate {
    const struct mylite_sql_ast_node *expression;
    const struct mylite_sql_ast_node *alias;
    enum planned_min_max_aggregate_function function;
    struct table_name_resolution source;
    struct mylite_catalog_table_descriptor table;
    struct mylite_catalog_column_descriptor aggregate_column;
    struct planned_select_predicate predicate;
};

struct planned_show_create_table {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct mylite_catalog_column_descriptor *columns;
    size_t column_count;
};

struct planned_delete {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct planned_select_predicate predicate;
    struct planned_select_order order;
    struct planned_select_limit limit;
    const char *rowid_alias;
};

struct planned_update {
    struct table_name_resolution target;
    struct mylite_catalog_table_descriptor table;
    struct mylite_catalog_column_descriptor assignment_column;
    const struct mylite_sql_ast_node *assignment_value_node;
    struct planned_value assignment_value;
    struct planned_select_predicate predicate;
    struct planned_select_order order;
    struct planned_select_limit limit;
    const char *rowid_alias;
};

struct dynamic_string {
    char *text;
    size_t length;
    size_t capacity;
};

struct table_option_name_policy {
    const char *identifier_kind;
    const char *nul_message;
};

struct show_like_filter {
    bool has_pattern;
    char *pattern;
    size_t pattern_length;
};

struct show_like_pattern_item_request {
    const char *pattern;
    size_t pattern_length;
    size_t pattern_index;
    char value_byte;
    bool case_sensitive;
};

struct load_columns_context {
    struct mylite_catalog_column_descriptor *columns;
    size_t count;
    size_t capacity;
};

struct show_tables_context {
    mylite_result *result;
    const struct show_like_filter *filter;
};

struct show_table_status_context {
    struct mylite_db *database;
    mylite_result *result;
    const struct show_like_filter *filter;
};

struct show_columns_context {
    struct mylite_db *database;
    mylite_result *result;
    const struct show_like_filter *filter;
};

struct show_columns_target_nodes {
    const struct mylite_sql_ast_node *table;
    const struct mylite_sql_ast_node *schema;
};

struct show_databases_context {
    mylite_result *result;
    const struct show_like_filter *filter;
};

struct collect_drop_schema_tables_context {
    struct mylite_db *database;
    struct planned_drop_schema *plan;
};

struct session_scalar_cell {
    const char *value;
    char integer_text[integer_text_capacity];
    char literal_text[literal_projection_text_capacity];
};

enum session_system_variable_kind {
    SESSION_SYSTEM_VARIABLE_NONE = 0,
    SESSION_SYSTEM_VARIABLE_WARNING_COUNT = 1,
    SESSION_SYSTEM_VARIABLE_ERROR_COUNT = 2,
    SESSION_SYSTEM_VARIABLE_CHARACTER_SET_CLIENT = 3,
    SESSION_SYSTEM_VARIABLE_CHARACTER_SET_CONNECTION = 4,
    SESSION_SYSTEM_VARIABLE_CHARACTER_SET_RESULTS = 5,
    SESSION_SYSTEM_VARIABLE_COLLATION_CONNECTION = 6,
    SESSION_SYSTEM_VARIABLE_VERSION = 7,
    SESSION_SYSTEM_VARIABLE_VERSION_COMMENT = 8,
    SESSION_SYSTEM_VARIABLE_CHARACTER_SET_SERVER = 9,
    SESSION_SYSTEM_VARIABLE_COLLATION_SERVER = 10,
    SESSION_SYSTEM_VARIABLE_CHARACTER_SET_DATABASE = 11,
    SESSION_SYSTEM_VARIABLE_COLLATION_DATABASE = 12,
    SESSION_SYSTEM_VARIABLE_DEFAULT_STORAGE_ENGINE = 13,
    SESSION_SYSTEM_VARIABLE_CHARACTER_SET_SYSTEM = 14,
    SESSION_SYSTEM_VARIABLE_CHARACTER_SET_FILESYSTEM = 15,
    SESSION_SYSTEM_VARIABLE_AUTOCOMMIT = 16,
    SESSION_SYSTEM_VARIABLE_SQL_QUOTE_SHOW_CREATE = 17,
    SESSION_SYSTEM_VARIABLE_FOREIGN_KEY_CHECKS = 18,
    SESSION_SYSTEM_VARIABLE_UNIQUE_CHECKS = 19,
    SESSION_SYSTEM_VARIABLE_UPDATABLE_VIEWS_WITH_LIMIT = 20,
    SESSION_SYSTEM_VARIABLE_SQL_AUTO_IS_NULL = 21,
    SESSION_SYSTEM_VARIABLE_SQL_BIG_SELECTS = 22,
    SESSION_SYSTEM_VARIABLE_SQL_GENERATE_INVISIBLE_PRIMARY_KEY = 23,
    SESSION_SYSTEM_VARIABLE_SQL_SAFE_UPDATES = 24,
    SESSION_SYSTEM_VARIABLE_SQL_WARNINGS = 25,
    SESSION_SYSTEM_VARIABLE_SQL_SELECT_LIMIT = 26,
    SESSION_SYSTEM_VARIABLE_SQL_NOTES = 27,
    SESSION_SYSTEM_VARIABLE_SQL_BUFFER_RESULT = 28,
    SESSION_SYSTEM_VARIABLE_SQL_LOG_BIN = 29,
    SESSION_SYSTEM_VARIABLE_SQL_LOG_OFF = 30,
    SESSION_SYSTEM_VARIABLE_SQL_MODE = 31,
    SESSION_SYSTEM_VARIABLE_SQL_REQUIRE_PRIMARY_KEY = 32,
    SESSION_SYSTEM_VARIABLE_SQL_REPLICA_SKIP_COUNTER = 33,
    SESSION_SYSTEM_VARIABLE_SQL_SLAVE_SKIP_COUNTER = 34,
};

struct system_variable_component {
    char text[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    bool quoted;
};

static int execute_parsed_statement(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_empty_statement(struct mylite_db *database, mylite_result **out_result);
static int execute_use_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_create_table_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_create_schema_statement(
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
static int plan_drop_table(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_drop_table *out_plan
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
static int append_drop_table_missing_notes(
    struct mylite_db *database,
    const struct planned_drop_table *plan
);
static void planned_drop_table_deinit(struct planned_drop_table *plan);
static int execute_drop_table_from_plan(
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
static int execute_insert_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_insert_set_statement(
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
static int execute_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_show_tables_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_show_table_status_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_show_character_set_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_show_collation_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_show_triggers_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_show_events_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_show_open_tables_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_show_routine_status_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_show_processlist_statement(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_show_warnings_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_show_count_warnings_statement(
    struct mylite_db *database,
    mylite_result **out_result
);
static int execute_show_errors_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_show_count_errors_statement(
    struct mylite_db *database,
    mylite_result **out_result
);
static int execute_show_columns_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_show_index_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_show_create_table_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_show_create_database_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int execute_show_engines_statement(struct mylite_db *database, mylite_result **out_result);
static int execute_show_databases_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int64_t row_count_for_completed_statement(
    const struct mylite_sql_ast_node *statement,
    const mylite_result *result
);
static int finish_parse_failure(
    struct mylite_db *database,
    const struct mylite_sql_parse_result *parse_result,
    int parse_rc
);
static int finish_failed_statement(struct mylite_db *database, int rc, mylite_result **out_result);
static int finish_completed_statement(
    struct mylite_db *database,
    int64_t completed_row_count,
    bool preserve_diagnostics_snapshot,
    mylite_result **out_result
);
static bool statement_preserves_diagnostics_snapshot(const struct mylite_sql_ast_node *statement);
static int snapshot_current_diagnostics(struct mylite_db *database);
static int finish_successful_result(
    struct mylite_db *database,
    mylite_result *result,
    mylite_result **out_result
);
static int finish_successful_result_with_warning_count(
    mylite_result *result,
    size_t warning_count,
    mylite_result **out_result
);

static int plan_create_table(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_create_table *out_plan
);
static int validate_create_table_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_options
);
static int validate_create_table_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_option
);
static int validate_create_table_engine_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *engine_option
);
static int validate_create_table_charset_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *charset_option
);
static int validate_create_table_collation_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *collation_option
);
static int copy_table_option_name_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *option_name_node,
    char *destination,
    size_t destination_size,
    struct table_option_name_policy policy
);
static int decode_table_option_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *option_name_node,
    char **out_name,
    struct table_option_name_policy policy
);
static int append_decoded_table_option_name_escape(
    struct mylite_db *database,
    struct dynamic_string *string,
    char escaped_byte,
    struct table_option_name_policy policy
);
static void planned_create_table_deinit(struct planned_create_table *plan);
static int create_table_from_plan(
    struct mylite_db *database,
    const struct planned_create_table *plan
);
static int insert_create_table_catalog_rows(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    const char *physical_name,
    struct mylite_catalog_table_descriptor *out_table
);
static int execute_physical_create_table(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const char *physical_name
);
static int execute_physical_drop_table(struct mylite_db *database, const char *physical_name);
static int execute_physical_alter_table_add_column(
    struct mylite_db *database,
    const struct planned_alter_table_add_column *plan
);
static int execute_physical_alter_table_drop_column(
    struct mylite_db *database,
    const struct planned_alter_table_drop_column *plan
);
static int execute_physical_alter_table_rename_column(
    struct mylite_db *database,
    const struct planned_alter_table_rename_column *plan
);
static int execute_physical_alter_table_modify_column(
    struct mylite_db *database,
    const struct planned_alter_table_modify_column *plan,
    const struct mylite_catalog_mutation *mutation
);

static int create_schema_from_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result *result
);
static int plan_drop_schema(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct mylite_catalog_mutation *mutation,
    struct planned_drop_schema *out_plan
);
static void planned_drop_schema_deinit(struct planned_drop_schema *plan);
static int collect_drop_schema_table(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
);
static int reserve_drop_schema_tables(struct planned_drop_schema *plan, size_t required_capacity);
static int drop_schema_from_plan(
    struct mylite_db *database,
    struct mylite_catalog_mutation *mutation,
    const struct planned_drop_schema *plan,
    mylite_result *result
);

static int plan_truncate_table(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_truncate_table *out_plan
);
static int execute_truncate_from_plan(
    struct mylite_db *database,
    const struct planned_truncate_table *plan,
    mylite_result *result
);

static int plan_rename_table_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_rename_table_statement *out_plan
);
static int plan_rename_table_pair(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *pair_node,
    struct planned_rename_table *out_pair
);
static void planned_rename_table_statement_deinit(struct planned_rename_table_statement *plan);
static int rename_table_statement_from_plan(
    struct mylite_db *database,
    const struct planned_rename_table_statement *plan
);
static int plan_alter_table_rename(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_rename_table *out_plan
);
static int alter_table_rename_from_plan(
    struct mylite_db *database,
    const struct planned_rename_table *plan
);
static int plan_alter_table_add_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_add_column *out_plan
);
static int alter_table_add_column_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_add_column *plan
);
static int plan_alter_table_drop_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_drop_column *out_plan
);
static int alter_table_drop_column_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_drop_column *plan
);
static int plan_alter_table_rename_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_rename_column *out_plan
);
static int alter_table_rename_column_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_rename_column *plan
);
static int plan_alter_table_modify_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_modify_column *out_plan
);
static int plan_alter_table_change_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_modify_column *out_plan
);
static int plan_alter_table_set_default(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_set_default *out_plan
);
static int alter_table_set_default_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_set_default *plan
);
static int plan_alter_table_drop_default(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_drop_default *out_plan
);
static int alter_table_drop_default_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_drop_default *plan
);
static int plan_alter_table_column_visibility(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_column_visibility *out_plan
);
static int alter_table_column_visibility_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_column_visibility *plan
);
static void planned_column_from_catalog_descriptor(
    const struct mylite_catalog_column_descriptor *descriptor,
    const struct mylite_sql_ast_node *default_node,
    struct planned_column *out_column
);
static int resolve_alter_table_column_replacement_plan(
    struct mylite_db *database,
    struct planned_alter_table_modify_column *out_plan
);
static int complete_alter_table_modify_column_plan(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    struct planned_alter_table_modify_column *out_plan
);
static bool modify_column_definition_matches(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
);
static bool modify_column_type_matches(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
);
static bool modify_column_metadata_only_replacement(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
);
static bool modify_column_name_matches(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
);
static void planned_alter_table_modify_column_deinit(
    struct planned_alter_table_modify_column *plan
);
static int alter_table_modify_column_from_plan(
    struct mylite_db *database,
    struct planned_alter_table_modify_column *plan
);
static int collect_modify_column_rebuild_columns(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    struct planned_alter_table_modify_column *out_plan
);
static int validate_modify_column_existing_rows(
    struct mylite_db *database,
    const struct planned_alter_table_modify_column *plan,
    int64_t *out_row_count
);
static int validate_existing_integer_for_column(
    struct mylite_db *database,
    int64_t value,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    const char *unsupported_message
);
static void make_modify_target_descriptor(
    const struct planned_alter_table_modify_column *plan,
    struct mylite_catalog_column_descriptor *out_column
);
static int rename_table_from_plan_with_policy(
    struct mylite_db *database,
    const struct planned_rename_table *plan,
    bool allow_same_object_noop,
    const char *unsupported_object_message
);
static int rename_table_pair_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    const struct planned_rename_table *plan,
    bool allow_same_object_noop,
    const char *unsupported_object_message
);

static int plan_insert(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_insert *out_plan
);
static int plan_insert_set(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_insert *out_plan
);
static void planned_insert_deinit(struct planned_insert *plan);
static int execute_insert_from_plan(
    struct mylite_db *database,
    const struct planned_insert *plan,
    mylite_result *result
);

static int plan_select(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_select *out_plan
);
static void planned_select_deinit(struct planned_select *plan);
static int execute_select_from_plan(
    struct mylite_db *database,
    const struct planned_select *plan,
    mylite_result **out_result
);
static bool select_statement_has_count_aggregate(const struct mylite_sql_ast_node *statement);
static int plan_count(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_count *out_plan
);
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
    mylite_result **out_result
);
static bool select_statement_has_min_max_aggregate(const struct mylite_sql_ast_node *statement);
static int plan_min_max_aggregate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_min_max_aggregate *out_plan
);
static enum planned_min_max_aggregate_function min_max_aggregate_function_from_expression(
    const struct mylite_sql_ast_node *expression
);
static int plan_min_max_aggregate_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column
);
static int execute_min_max_aggregate_from_plan(
    struct mylite_db *database,
    const struct planned_min_max_aggregate *plan,
    mylite_result **out_result
);
static int append_count_result_column(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_count *plan
);
static int append_min_max_aggregate_result_column(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_min_max_aggregate *plan
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
static int read_min_max_aggregate_from_source(
    struct mylite_db *database,
    const struct planned_min_max_aggregate *plan,
    mylite_result *result
);
static int step_count_statement(sqlite3_stmt *statement, int64_t *out_count);
static int step_min_max_aggregate_statement(sqlite3_stmt *statement, mylite_result *result);
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
static int min_max_aggregate_execution_error(struct mylite_db *database, int rc);
static bool select_statement_is_session_scalar_projection(
    const struct mylite_sql_ast_node *statement
);
static bool select_statement_is_literal_projection(const struct mylite_sql_ast_node *statement);
static bool select_statement_has_no_source_or_dual(const struct mylite_sql_ast_node *statement);
static bool select_statement_is_literal_projection_attempt(
    const struct mylite_sql_ast_node *statement
);
static int execute_scalar_projection_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
);
static int append_session_scalar_select_warnings(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list
);
static int copy_scalar_projection_column_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char **out_text
);
static const char *select_statement_argument_count_error_function(
    const struct mylite_sql_ast_node *statement
);
static const char *argument_count_error_function_name(const struct mylite_sql_ast_node *expression);
static int session_scalar_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int literal_projection_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static int normalize_decimal_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    bool is_negative,
    char *buffer,
    size_t buffer_size
);
static int system_variable_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
static const char *default_sql_mode_value(void);
static const struct mylite_diagnostics *system_variable_count_diagnostics(
    const struct mylite_db *database
);
static int resolve_session_system_variable(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum session_system_variable_kind *out_kind
);
static bool resolve_system_variable_kind(
    const struct system_variable_component *name,
    enum session_system_variable_kind *out_kind
);
static bool system_variable_kind_allows_global_scope(enum session_system_variable_kind kind);
static bool system_variable_kind_allows_session_scope(enum session_system_variable_kind kind);
static bool system_variable_kind_warns_on_scalar_read(enum session_system_variable_kind kind);
static bool is_literal_projection_expression(const struct mylite_sql_ast_node *expression);
static bool is_literal_projection_attempt_expression(const struct mylite_sql_ast_node *expression);
static bool is_literal_projection_attempt_operand(const struct mylite_sql_ast_node *expression);
static int append_system_variable_read_warning(
    struct mylite_db *database,
    enum session_system_variable_kind kind
);
static int parse_system_variable_component(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    size_t *offset,
    struct system_variable_component *out_component
);
static int append_quoted_system_variable_byte(
    struct mylite_db *database,
    struct system_variable_component *component,
    size_t *component_length,
    char value
);
static bool system_variable_component_equals(
    const struct system_variable_component *component,
    const char *expected
);
static bool system_variable_component_is_empty(const struct system_variable_component *component);
static void set_session_variable_only_error(struct mylite_db *database, const char *variable_name);
static void set_global_variable_only_error(struct mylite_db *database, const char *variable_name);
static void set_unknown_system_variable_error(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
);
static int copy_system_variable_name_for_error(
    const struct mylite_sql_source_span *span,
    char **out_name
);
static int copy_system_variable_component_name_for_error(
    const struct mylite_sql_source_span *span,
    size_t *offset,
    char **out_name
);
static int copy_system_variable_raw_body_for_error(
    const struct mylite_sql_source_span *span,
    char **out_name
);
static int append_system_variable_error_name_byte(
    char value,
    char **name,
    size_t *length,
    size_t capacity
);
static const struct mylite_sql_ast_node *unwrap_parenthesized_expression(
    const struct mylite_sql_ast_node *expression
);
static bool is_session_scalar_expression(const struct mylite_sql_ast_node *expression);
static int copy_source_span_text(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    char **out_text
);
static int append_show_processlist_row(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    mylite_result *result
);
static int format_show_processlist_user_host(
    struct mylite_db *database,
    char *user,
    size_t user_size,
    char *host,
    size_t host_size
);
static int copy_show_processlist_info(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    char **out_info
);
static size_t statement_info_length_without_terminator(const char *sql, size_t sql_size);
static int append_show_processlist_warning(struct mylite_db *database);
static int plan_diagnostics_show_limit(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *limit_clause,
    struct planned_diagnostics_show_limit *out_limit
);
static int convert_diagnostics_show_limit_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_value
);
static int append_show_diagnostics_rows(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_diagnostics_show_limit *limit
);
static int append_show_diagnostics_row(
    struct mylite_db *database,
    mylite_result *result,
    const char *level,
    const struct mylite_diagnostic_record *record
);
static int append_show_count_warnings_row(struct mylite_db *database, mylite_result *result);
static int append_show_errors_rows(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_diagnostics_show_limit *limit
);
static int append_show_count_errors_row(struct mylite_db *database, mylite_result *result);
static int previous_diagnostics_condition_count(
    const struct mylite_diagnostics *diagnostics,
    uint64_t *out_count
);
static bool diagnostics_has_error_condition(const struct mylite_diagnostics *diagnostics);
static int format_uint64(
    struct mylite_db *database,
    uint64_t value,
    char *buffer,
    size_t buffer_size
);

static int plan_show_create_table(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_show_create_table *out_plan
);
static void planned_show_create_table_deinit(struct planned_show_create_table *plan);
static int execute_show_create_table_from_plan(
    struct mylite_db *database,
    const struct planned_show_create_table *plan,
    mylite_result *result
);
static int build_show_create_table_sql(
    struct mylite_db *database,
    const struct planned_show_create_table *plan,
    char **out_sql
);
static int append_show_create_table_column_definition(
    struct mylite_db *database,
    struct dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column,
    bool is_last_column
);
static int append_show_create_table_column_default(
    struct mylite_db *database,
    struct dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column
);
static int append_show_create_table_column_suffix(
    struct dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column,
    bool is_last_column
);
static int show_create_table_type_text(
    struct mylite_db *database,
    const char *logical_type,
    const char **out_type_text
);
static int build_show_create_database_sql(const char *schema_name, char **out_sql);

static int maybe_finish_create_if_not_exists_noop(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct planned_create_table *plan,
    bool *out_finished
);
static bool create_table_has_if_not_exists(const struct mylite_sql_ast_node *statement);
static const struct mylite_sql_ast_node *create_table_options_node(
    const struct mylite_sql_ast_node *statement
);
static bool drop_table_has_if_exists(const struct mylite_sql_ast_node *statement);

static int plan_delete(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_delete *out_plan
);
static void planned_delete_deinit(struct planned_delete *plan);
static int execute_delete_from_plan(
    struct mylite_db *database,
    const struct planned_delete *plan,
    mylite_result *result
);

static int plan_update(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_update *out_plan
);
static void planned_update_deinit(struct planned_update *plan);
static int execute_update_from_plan(
    struct mylite_db *database,
    const struct planned_update *plan,
    mylite_result *result
);
static int update_matches_any_row(
    struct mylite_db *database,
    const struct planned_update *plan,
    bool *out_matches
);
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
static int resolve_drop_if_exists_table_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct table_name_resolution *out_resolution,
    bool *out_missing_schema
);
static int resolve_show_columns_table_name(
    struct mylite_db *database,
    struct show_columns_target_nodes nodes,
    struct table_name_resolution *out_resolution
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
static int copy_identifier_text(
    const struct mylite_sql_ast_node *node,
    char *destination,
    size_t destination_size,
    struct mylite_db *database
);
static int copy_quoted_identifier_text(
    const char *source,
    size_t source_size,
    char *destination,
    size_t destination_size
);
static int copy_unquoted_identifier_text(
    const char *source,
    size_t source_size,
    char *destination,
    size_t destination_size
);

static int plan_columns(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_list,
    struct planned_column *columns,
    size_t column_count
);
static int plan_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    struct planned_column *out_column
);
static int validate_column_default(
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
static int convert_column_default_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct planned_column *column,
    int64_t *out_value
);
static int check_duplicate_column_names(
    struct mylite_db *database,
    const struct planned_column *columns,
    size_t column_count
);
static bool text_equals_ascii_case_insensitive(const char *left, const char *right);
static char ascii_lower(unsigned char byte);
static int map_integer_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    const char **out_logical_type,
    const char **out_physical_type
);
static int map_integer_display_width(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    bool *out_has_display_width,
    uint64_t *out_display_width
);
static int append_integer_display_width_warning(struct mylite_db *database);
static const char *logical_type_for_mapped_integer(struct mapped_integer_type integer_type);
static bool modify_column_integer_value_domain_matches(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
);
static bool column_is_nullable(const struct mylite_sql_ast_node *nullability_node);

static int resolve_readable_base_table(
    struct mylite_db *database,
    const struct table_name_resolution *resolution,
    struct mylite_catalog_table_descriptor *out_table
);
static int load_table_columns(
    struct mylite_db *database,
    int64_t table_id,
    struct mylite_catalog_column_descriptor **out_columns,
    size_t *out_column_count
);
static int append_loaded_column(
    const struct mylite_catalog_column_descriptor *column,
    void *user_data
);
static int load_columns_reserve(struct load_columns_context *context, size_t required_capacity);
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
    const char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
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
static int collect_insert_target_indexes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_list,
    const struct planned_insert *plan,
    size_t **out_indexes,
    size_t *out_index_count
);
static size_t count_visible_insert_target_columns(const struct planned_insert *plan);
static void collect_visible_insert_target_indexes(
    const struct planned_insert *plan,
    size_t *indexes
);
static int collect_explicit_insert_target_indexes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_list,
    const struct planned_insert *plan,
    size_t *indexes,
    size_t column_count
);
static int collect_insert_set_target_indexes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment_list,
    const struct planned_insert *plan,
    size_t **out_indexes,
    size_t *out_index_count
);
static int check_insert_target_duplicate(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    const size_t *target_indexes,
    size_t target_count
);
static int check_insert_omitted_columns(
    struct mylite_db *database,
    const struct planned_insert *plan,
    const size_t *target_indexes,
    size_t target_count
);
static int plan_insert_rows(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *row_list,
    const size_t *target_indexes,
    size_t target_count,
    struct planned_insert *plan
);
static int plan_insert_row(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *row_node,
    size_t row_number,
    const size_t *target_indexes,
    size_t target_count,
    struct planned_insert *plan,
    struct planned_insert_row *out_row
);
static int plan_insert_set_row(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment_list,
    const size_t *target_indexes,
    size_t target_count,
    struct planned_insert *plan
);
static int allocate_insert_row_values(
    struct mylite_db *database,
    const struct planned_insert *plan,
    struct planned_insert_row *out_row
);
static int convert_insert_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    struct planned_value *out_value
);
static int convert_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    struct planned_value *out_value
);
static int parse_unsigned_integer_literal(
    const struct mylite_sql_source_span *span,
    uint64_t *out_value
);
static bool boolean_literal_magnitude(
    const struct mylite_sql_ast_node *literal,
    uint64_t *out_value
);
static int convert_integer_for_column(
    struct mylite_db *database,
    uint64_t magnitude,
    bool is_negative,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    int64_t *out_value
);
static int plan_select_columns(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select *out_plan
);
static int plan_select_distinct_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select *out_plan
);
static int select_item_column_reference(
    const struct mylite_sql_ast_node *item,
    const struct mylite_sql_ast_node **out_column
);
static int plan_select_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *out_predicate
);
static int plan_select_predicate_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *out_predicate
);
static int plan_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *out_predicate
);
static int plan_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *out_predicate
);
static int resolve_predicate_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column
);
static int convert_predicate_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);
static int convert_integer_for_predicate(
    struct mylite_db *database,
    uint64_t magnitude,
    bool is_negative,
    const struct mylite_catalog_column_descriptor *column,
    int64_t *out_value
);
static int plan_select_order(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_clause,
    const struct select_source_context *source_context,
    const struct planned_select *select_plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_order *out_order
);
static int resolve_order_alias(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct planned_select *select_plan,
    struct mylite_catalog_column_descriptor *out_column,
    bool *out_resolved
);
static int resolve_order_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column
);
static int plan_select_limit(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *limit_clause,
    struct planned_select_limit *out_limit
);
static int plan_delete_limit(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *limit_clause,
    struct planned_select_limit *out_limit
);
static int plan_update_assignment(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment_list,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_update *out_plan
);
static int convert_update_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);
static int convert_update_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);
static int plan_update_limit(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *limit_clause,
    struct planned_select_limit *out_limit
);
static int convert_limit_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    int64_t *out_value
);
static int integer_range_for_column(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    const char *unsupported_message,
    struct integer_column_range *out_range
);
static bool select_list_is_wildcard(const struct mylite_sql_ast_node *select_list);
static int append_select_column(
    struct mylite_db *database,
    struct planned_select *plan,
    const struct mylite_catalog_column_descriptor *column,
    const struct mylite_sql_ast_node *alias
);
static int append_select_result_column(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_select *plan,
    size_t column_index
);
static int copy_select_item_alias_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *alias,
    char **out_text
);
static int copy_select_item_identifier_alias_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *alias,
    char **out_text
);
static int validate_select_item_alias_text(struct mylite_db *database, char **text);
static int duplicate_text(struct mylite_db *database, const char *source, char **out_text);
static int append_show_table(const struct mylite_catalog_table_descriptor *table, void *user_data);
static int append_show_table_status(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
);
static int append_show_column(
    const struct mylite_catalog_column_descriptor *column,
    void *user_data
);
static int show_column_type_text(
    struct mylite_db *database,
    const char *logical_type,
    const char **out_type_text
);
static int append_show_database(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
);

static int build_physical_table_name(int64_t table_id, char *destination, size_t destination_size);
static int build_create_table_sql(
    const struct planned_create_table *plan,
    const char *physical_name,
    char **out_sql
);
static int build_drop_table_sql(const char *physical_name, char **out_sql);
static int build_alter_table_add_column_sql(
    const struct planned_alter_table_add_column *plan,
    char **out_sql
);
static int build_alter_table_drop_column_sql(
    const struct planned_alter_table_drop_column *plan,
    char **out_sql
);
static int build_alter_table_rename_column_sql(
    const struct planned_alter_table_rename_column *plan,
    char **out_sql
);
static int build_modify_temporary_physical_name(
    const struct planned_alter_table_modify_column *plan,
    const struct mylite_catalog_mutation *mutation,
    char *destination,
    size_t destination_size
);
static int build_alter_table_modify_validation_sql(
    const struct planned_alter_table_modify_column *plan,
    char **out_sql
);
static int build_alter_table_modify_create_sql(
    const struct planned_alter_table_modify_column *plan,
    const char *temporary_physical_name,
    char **out_sql
);
static int build_alter_table_modify_copy_sql(
    const struct planned_alter_table_modify_column *plan,
    const char *temporary_physical_name,
    char **out_sql
);
static int build_alter_table_rename_physical_table_sql(
    const char *source_physical_name,
    const char *target_physical_name,
    char **out_sql
);
static int build_truncate_table_sql(const struct planned_truncate_table *plan, char **out_sql);
static int build_insert_sql(const struct planned_insert *plan, char **out_sql);
static int append_insert_column_names(
    struct dynamic_string *string,
    const struct planned_insert *plan
);
static int append_insert_parameters(struct dynamic_string *string, size_t column_count);
static int append_numbered_parameter(struct dynamic_string *string, size_t parameter_index);
static int build_select_sql(const struct planned_select *plan, char **out_sql);
static int build_count_sql(const struct planned_count *plan, char **out_sql);
static int build_min_max_aggregate_sql(
    const struct planned_min_max_aggregate *plan,
    char **out_sql
);
static const char *min_max_aggregate_sql_function(enum planned_min_max_aggregate_function function);
static int append_select_predicate_sql(
    struct dynamic_string *string,
    const struct planned_select_predicate *predicate,
    size_t *next_parameter
);
static int append_select_order_sql(
    struct dynamic_string *string,
    const struct planned_select_order *order
);
static int append_select_limit_sql(
    struct dynamic_string *string,
    const struct planned_select_limit *limit,
    size_t *next_parameter
);
static int build_delete_sql(const struct planned_delete *plan, char **out_sql);
static int append_delete_rowid_limited_sql(
    struct dynamic_string *string,
    const struct planned_delete *plan,
    size_t *next_parameter
);
static int build_update_sql(const struct planned_update *plan, char **out_sql);
static int build_update_matched_sql(const struct planned_update *plan, char **out_sql);
static int append_update_rowid_limited_sql(
    struct dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
);
static int append_update_changed_condition_sql(
    struct dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
);
static const char *comparison_operator_sql(enum mylite_sql_ast_operator operator_kind);
static int execute_sqlite_schema_sql(struct mylite_db *database, const char *sql);
static int execute_sqlite_control_sql(const struct mylite_db *database, const char *sql);
static int prepare_sqlite_statement(
    const struct mylite_db *database,
    const char *sql,
    sqlite3_stmt **out_statement
);
static int finalize_sqlite_statement(sqlite3_stmt *statement, int rc);
static int bind_insert_row(sqlite3_stmt *statement, const struct planned_insert *plan, size_t row);
static int step_insert_row(sqlite3_stmt *statement);
static int bind_select_parameters(sqlite3_stmt *statement, const struct planned_select *plan);
static int bind_count_parameters(sqlite3_stmt *statement, const struct planned_count *plan);
static int bind_min_max_aggregate_parameters(
    sqlite3_stmt *statement,
    const struct planned_min_max_aggregate *plan
);
static int bind_delete_parameters(sqlite3_stmt *statement, const struct planned_delete *plan);
static int bind_update_parameters(sqlite3_stmt *statement, const struct planned_update *plan);
static int bind_update_matched_parameters(
    sqlite3_stmt *statement,
    const struct planned_update *plan
);
static int bind_planned_value_parameter(
    sqlite3_stmt *statement,
    int parameter_index,
    const struct planned_value *value
);
static int bind_int64_parameter(sqlite3_stmt *statement, int parameter_index, int64_t value);
static int append_selected_sqlite_row(sqlite3_stmt *statement, mylite_result *result);
static int choose_sqlite_rowid_alias(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const char *unsupported_message,
    const char **out_alias
);
static bool column_name_exists(
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const char *name
);
static int make_show_like_filter(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *pattern_node,
    struct show_like_filter *out_filter
);
static void show_like_filter_deinit(struct show_like_filter *filter);
static bool show_like_filter_matches(
    const struct show_like_filter *filter,
    const char *value,
    bool case_sensitive
);
static int build_show_databases_column_name(const struct show_like_filter *filter, char **out_name);
static int build_show_tables_column_name(
    const char *schema_name,
    const struct show_like_filter *filter,
    char **out_name
);
static int read_show_table_status_row_count(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    int64_t *out_count
);
static int build_show_table_status_count_sql(
    const struct mylite_catalog_table_descriptor *table,
    char **out_sql
);
static int format_show_table_status_integer(
    struct mylite_db *database,
    int64_t value,
    char *buffer,
    size_t buffer_size
);
static int decode_show_like_pattern(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *pattern_node,
    char **out_pattern
);
static int append_decoded_string_escape(
    struct mylite_db *database,
    struct dynamic_string *string,
    char escaped_byte
);
static bool show_like_pattern_matches(
    const char *pattern,
    size_t pattern_length,
    const char *value,
    size_t value_length,
    bool case_sensitive
);
static size_t show_like_skip_percent_run(
    const char *pattern,
    size_t pattern_length,
    size_t pattern_index
);
static bool show_like_pattern_item_matches(
    struct show_like_pattern_item_request request,
    size_t *out_next_pattern_index
);
static bool show_like_bytes_equal(char left, char right, bool case_sensitive);
static char show_like_ascii_lower(char byte);

static void dynamic_string_init(struct dynamic_string *string);
static void dynamic_string_deinit(struct dynamic_string *string);
static int dynamic_string_append(struct dynamic_string *string, const char *text);
static int dynamic_string_append_char(struct dynamic_string *string, char byte);
static int dynamic_string_append_quoted_identifier(struct dynamic_string *string, const char *text);
static int dynamic_string_append_mysql_quoted_identifier(
    struct dynamic_string *string,
    const char *text
);
static int dynamic_string_reserve(struct dynamic_string *string, size_t required_capacity);
static char *dynamic_string_take(struct dynamic_string *string);

static const struct mylite_sql_ast_node *child_at(
    const struct mylite_sql_ast_node *node,
    size_t index
);
static const struct mylite_sql_ast_node *child_with_kind(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_node_kind kind
);
static int script_statement_count(const struct mylite_sql_ast_node *root, size_t *out_count);
static void set_parse_error(
    struct mylite_db *database,
    const struct mylite_sql_parse_result *parse_result
);
static void set_unsupported_error(struct mylite_db *database, const char *message);
static void set_native_function_parameter_count_error(
    struct mylite_db *database,
    const char *function_name
);
static void set_no_database_error(struct mylite_db *database);
static void set_database_exists_error(struct mylite_db *database, const char *schema_name);
static int append_database_exists_note(struct mylite_db *database, const char *schema_name);
static void set_cant_drop_database_error(struct mylite_db *database, const char *schema_name);
static void set_unknown_database_error(struct mylite_db *database, const char *schema_name);
static void set_table_exists_error(struct mylite_db *database, const char *table_name);
static int append_table_exists_note(struct mylite_db *database, const char *table_name);
static void set_unknown_column_error(struct mylite_db *database, const char *column_name);
static void set_unknown_where_column_error(struct mylite_db *database, const char *column_name);
static void set_unknown_order_column_error(struct mylite_db *database, const char *column_name);
static void set_ambiguous_order_column_error(struct mylite_db *database, const char *column_name);
static void set_unknown_table_error(
    struct mylite_db *database,
    const char *schema_name,
    const char *table_name
);
static int set_unknown_drop_tables_error(
    struct mylite_db *database,
    const struct planned_drop_table *plan
);
static int append_unknown_table_note(
    struct mylite_db *database,
    const char *schema_name,
    const char *table_name
);
static void set_unknown_storage_engine_error(struct mylite_db *database, const char *engine_name);
static void set_unknown_character_set_error(struct mylite_db *database, const char *charset_name);
static void set_unknown_collation_error(struct mylite_db *database, const char *collation_name);
static void set_table_does_not_exist_error(
    struct mylite_db *database,
    const char *schema_name,
    const char *table_name
);
static void set_duplicate_column_error(struct mylite_db *database, const char *column_name);
static void set_duplicate_table_alias_error(struct mylite_db *database, const char *table_name);
static void set_cant_drop_field_or_key_error(struct mylite_db *database, const char *column_name);
static void set_cant_remove_all_fields_error(struct mylite_db *database);
static void set_must_have_visible_column_error(struct mylite_db *database);
static void set_unknown_column_in_table_error(
    struct mylite_db *database,
    const char *column_name,
    const char *table_name
);
static void set_column_specified_twice_error(struct mylite_db *database, const char *column_name);
static void set_column_count_mismatch_error(struct mylite_db *database, size_t row_number);
static void set_bad_null_error(struct mylite_db *database, const char *column_name);
static void set_data_truncated_error(
    struct mylite_db *database,
    const char *column_name,
    size_t row_number
);
static void set_invalid_default_error(struct mylite_db *database, const char *column_name);
static void set_no_default_error(struct mylite_db *database, const char *column_name);
static void set_out_of_range_error(
    struct mylite_db *database,
    const char *column_name,
    size_t row_number
);
static void set_display_width_out_of_range_error(
    struct mylite_db *database,
    const char *column_name
);
static void set_predicate_out_of_range_error(struct mylite_db *database, const char *column_name);
static void set_limit_out_of_range_error(struct mylite_db *database);
static void set_identifier_too_long_error(struct mylite_db *database, const char *kind);
static void set_reserved_name_error(struct mylite_db *database, const char *kind, const char *name);
static void set_nomem_error(struct mylite_db *database);
static void set_physical_sqlite_error(struct mylite_db *database);
static void set_physical_sqlite_row_error(struct mylite_db *database);
static void set_runtime_error(struct mylite_db *database, const char *message);
static void set_internal_error_if_clear(struct mylite_db *database, int rc, const char *message);
static int status_from_parse_status(enum mylite_sql_parse_status status);

int mylite_execute(
    mylite_db *database,
    const char *sql,
    size_t sql_size,
    mylite_result **out_result
) {
    struct mylite_statement_context context;
    struct mylite_sql_parse_result parse_result;
    const struct mylite_sql_ast_node *statement = NULL;
    int64_t completed_row_count = -1;
    size_t statement_count = 0U;
    bool preserve_diagnostics_snapshot = false;
    int rc = MYLITE_OK;

    if (out_result == NULL) {
        if (database != NULL) {
            mylite_diagnostics_set_error(
                mylite_connection_diagnostics(database),
                MYLITE_MISUSE,
                "HY000",
                mylite_diagnostics_misuse_message()
            );
        }
        return MYLITE_MISUSE;
    }
    *out_result = NULL;
    if (database == NULL || sql == NULL) {
        if (database != NULL) {
            mylite_diagnostics_set_error(
                mylite_connection_diagnostics(database),
                MYLITE_MISUSE,
                "HY000",
                mylite_diagnostics_misuse_message()
            );
        }
        return MYLITE_MISUSE;
    }

    mylite_statement_context_init(&context);
    rc = mylite_statement_context_begin(&context, database, sql, sql_size);
    if (rc != MYLITE_OK) {
        mylite_statement_context_deinit(&context);
        return rc;
    }
    mylite_statement_context_set_previous_row_count(&context, database->session.previous_row_count);

    rc = status_from_parse_status(mylite_sql_parse(
        (struct mylite_sql_parse_config){
            .input = sql,
            .length = sql_size,
            .modes = 0U,
        },
        &parse_result
    ));
    if (rc != MYLITE_OK) {
        rc = finish_parse_failure(database, &parse_result, rc);
        mylite_sql_parse_result_deinit(&parse_result);
        (void)mylite_statement_context_end(&context, rc);
        mylite_statement_context_deinit(&context);
        return rc;
    }

    rc = script_statement_count(parse_result.root, &statement_count);
    if (rc == MYLITE_OK && statement_count == 0U) {
        rc = execute_empty_statement(database, out_result);
    } else if (rc == MYLITE_OK && statement_count == 1U) {
        statement = child_at(parse_result.root, 0U);
        rc = execute_parsed_statement(database, &context, statement, out_result);
    } else if (rc == MYLITE_OK) {
        set_unsupported_error(database, "multiple statements are not supported");
        rc = MYLITE_ERROR;
    }

    if (rc == MYLITE_OK) {
        completed_row_count = row_count_for_completed_statement(statement, *out_result);
        preserve_diagnostics_snapshot = statement_preserves_diagnostics_snapshot(statement);
    }
    mylite_sql_parse_result_deinit(&parse_result);
    if (rc != MYLITE_OK) {
        rc = finish_failed_statement(database, rc, out_result);
    } else {
        rc = finish_completed_statement(
            database,
            completed_row_count,
            preserve_diagnostics_snapshot,
            out_result
        );
    }
    (void)mylite_statement_context_end(&context, rc);
    mylite_statement_context_deinit(&context);

    return rc;
}

static int finish_parse_failure(
    struct mylite_db *database,
    const struct mylite_sql_parse_result *parse_result,
    int parse_rc
) {
    int rc = parse_rc;
    int snapshot_rc = MYLITE_OK;

    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    } else {
        set_parse_error(database, parse_result);
    }
    database->session.previous_row_count = -1;

    snapshot_rc = snapshot_current_diagnostics(database);
    return snapshot_rc == MYLITE_OK ? rc : snapshot_rc;
}

static int finish_failed_statement(struct mylite_db *database, int rc, mylite_result **out_result) {
    int snapshot_rc = MYLITE_OK;

    database->session.previous_row_count = -1;
    set_internal_error_if_clear(database, rc, "statement execution failed");
    mylite_result_free(*out_result);
    *out_result = NULL;

    snapshot_rc = snapshot_current_diagnostics(database);
    return snapshot_rc == MYLITE_OK ? rc : snapshot_rc;
}

static int finish_completed_statement(
    struct mylite_db *database,
    int64_t completed_row_count,
    bool preserve_diagnostics_snapshot,
    mylite_result **out_result
) {
    int rc = MYLITE_OK;

    if (!preserve_diagnostics_snapshot) {
        rc = snapshot_current_diagnostics(database);
    }
    if (rc != MYLITE_OK) {
        database->session.previous_row_count = -1;
        mylite_result_free(*out_result);
        *out_result = NULL;
        return rc;
    }

    database->session.previous_row_count = completed_row_count;
    return MYLITE_OK;
}

static int execute_parsed_statement(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    if (statement == NULL) {
        set_unsupported_error(database, "empty statement is not supported");
        return MYLITE_ERROR;
    }

    switch (statement->kind) {
    case MYLITE_SQL_AST_USE_STATEMENT:
        return execute_use_statement(database, statement, out_result);
    case MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT:
        return execute_create_schema_statement(database, statement, out_result);
    case MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT:
        return execute_drop_schema_statement(database, statement, out_result);
    case MYLITE_SQL_AST_CREATE_TABLE_STATEMENT:
        return execute_create_table_statement(database, statement, out_result);
    case MYLITE_SQL_AST_DROP_TABLE_STATEMENT:
        return execute_drop_table_statement(database, statement, out_result);
    case MYLITE_SQL_AST_TRUNCATE_TABLE_STATEMENT:
        return execute_truncate_table_statement(database, statement, out_result);
    case MYLITE_SQL_AST_RENAME_TABLE_STATEMENT:
        return execute_rename_table_statement(database, statement, out_result);
    case MYLITE_SQL_AST_ALTER_TABLE_RENAME_STATEMENT:
        return execute_alter_table_rename_statement(database, statement, out_result);
    case MYLITE_SQL_AST_ALTER_TABLE_ADD_COLUMN_STATEMENT:
        return execute_alter_table_add_column_statement(database, statement, out_result);
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_COLUMN_STATEMENT:
        return execute_alter_table_drop_column_statement(database, statement, out_result);
    case MYLITE_SQL_AST_ALTER_TABLE_RENAME_COLUMN_STATEMENT:
        return execute_alter_table_rename_column_statement(database, statement, out_result);
    case MYLITE_SQL_AST_ALTER_TABLE_MODIFY_COLUMN_STATEMENT:
        return execute_alter_table_modify_column_statement(database, statement, out_result);
    case MYLITE_SQL_AST_ALTER_TABLE_CHANGE_COLUMN_STATEMENT:
        return execute_alter_table_change_column_statement(database, statement, out_result);
    case MYLITE_SQL_AST_ALTER_TABLE_SET_DEFAULT_STATEMENT:
        return execute_alter_table_set_default_statement(database, statement, out_result);
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_DEFAULT_STATEMENT:
        return execute_alter_table_drop_default_statement(database, statement, out_result);
    case MYLITE_SQL_AST_ALTER_TABLE_COLUMN_VISIBILITY_STATEMENT:
        return execute_alter_table_column_visibility_statement(database, statement, out_result);
    case MYLITE_SQL_AST_INSERT_STATEMENT:
        return execute_insert_statement(database, statement, out_result);
    case MYLITE_SQL_AST_INSERT_SET_STATEMENT:
        return execute_insert_set_statement(database, statement, out_result);
    case MYLITE_SQL_AST_DELETE_STATEMENT:
        return execute_delete_statement(database, statement, out_result);
    case MYLITE_SQL_AST_UPDATE_STATEMENT:
        return execute_update_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SELECT_STATEMENT:
        return execute_select_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SHOW_TABLES_STATEMENT:
        return execute_show_tables_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SHOW_TABLE_STATUS_STATEMENT:
        return execute_show_table_status_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SHOW_CHARACTER_SET_STATEMENT:
        return execute_show_character_set_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SHOW_COLLATION_STATEMENT:
        return execute_show_collation_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SHOW_TRIGGERS_STATEMENT:
        return execute_show_triggers_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SHOW_EVENTS_STATEMENT:
        return execute_show_events_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SHOW_OPEN_TABLES_STATEMENT:
        return execute_show_open_tables_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SHOW_PROCEDURE_STATUS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_FUNCTION_STATUS_STATEMENT:
        return execute_show_routine_status_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SHOW_PROCESSLIST_STATEMENT:
    case MYLITE_SQL_AST_SHOW_FULL_PROCESSLIST_STATEMENT:
        return execute_show_processlist_statement(database, context, statement, out_result);
    case MYLITE_SQL_AST_SHOW_WARNINGS_STATEMENT:
        return execute_show_warnings_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SHOW_COUNT_WARNINGS_STATEMENT:
        return execute_show_count_warnings_statement(database, out_result);
    case MYLITE_SQL_AST_SHOW_ERRORS_STATEMENT:
        return execute_show_errors_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SHOW_COUNT_ERRORS_STATEMENT:
        return execute_show_count_errors_statement(database, out_result);
    case MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT:
        return execute_show_columns_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SHOW_INDEX_STATEMENT:
        return execute_show_index_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SHOW_CREATE_TABLE_STATEMENT:
        return execute_show_create_table_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SHOW_CREATE_DATABASE_STATEMENT:
        return execute_show_create_database_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SHOW_ENGINES_STATEMENT:
        return execute_show_engines_statement(database, out_result);
    case MYLITE_SQL_AST_SHOW_DATABASES_STATEMENT:
        return execute_show_databases_statement(database, statement, out_result);
    case MYLITE_SQL_AST_SCRIPT:
    case MYLITE_SQL_AST_SELECT_LIST:
    case MYLITE_SQL_AST_SELECT_ITEM:
    case MYLITE_SQL_AST_FROM_DUAL:
    case MYLITE_SQL_AST_IDENTIFIER:
    case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER:
    case MYLITE_SQL_AST_WILDCARD:
    case MYLITE_SQL_AST_LITERAL:
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION:
    case MYLITE_SQL_AST_COLUMN_DEFINITION_LIST:
    case MYLITE_SQL_AST_COLUMN_DEFINITION:
    case MYLITE_SQL_AST_INTEGER_TYPE:
    case MYLITE_SQL_AST_NULLABILITY:
    case MYLITE_SQL_AST_COLUMN_DEFAULT_NULL:
    case MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE:
    case MYLITE_SQL_AST_IDENTIFIER_LIST:
    case MYLITE_SQL_AST_INSERT_ROW_LIST:
    case MYLITE_SQL_AST_INSERT_ROW:
    case MYLITE_SQL_AST_INSERT_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_INSERT_ASSIGNMENT:
    case MYLITE_SQL_AST_FROM_TABLE:
    case MYLITE_SQL_AST_WHERE_CLAUSE:
    case MYLITE_SQL_AST_COMPARISON_PREDICATE:
    case MYLITE_SQL_AST_IS_NULL_PREDICATE:
    case MYLITE_SQL_AST_ORDER_BY_CLAUSE:
    case MYLITE_SQL_AST_ORDER_DIRECTION:
    case MYLITE_SQL_AST_LIMIT_CLAUSE:
    case MYLITE_SQL_AST_UPDATE_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_UPDATE_ASSIGNMENT:
    case MYLITE_SQL_AST_TABLE_ENGINE_OPTION:
    case MYLITE_SQL_AST_TABLE_OPTION_LIST:
    case MYLITE_SQL_AST_TABLE_CHARSET_OPTION:
    case MYLITE_SQL_AST_TABLE_COLLATION_OPTION:
    case MYLITE_SQL_AST_CREATE_IF_NOT_EXISTS_CLAUSE:
    case MYLITE_SQL_AST_DROP_IF_EXISTS_CLAUSE:
    case MYLITE_SQL_AST_CREATE_SCHEMA_IF_NOT_EXISTS_CLAUSE:
    case MYLITE_SQL_AST_DROP_SCHEMA_IF_EXISTS_CLAUSE:
    case MYLITE_SQL_AST_TABLE_NAME_LIST:
    case MYLITE_SQL_AST_RENAME_TABLE_PAIR:
    case MYLITE_SQL_AST_RENAME_TABLE_PAIR_LIST:
    case MYLITE_SQL_AST_DATABASE_FUNCTION:
    case MYLITE_SQL_AST_SCHEMA_FUNCTION:
    case MYLITE_SQL_AST_USER_FUNCTION:
    case MYLITE_SQL_AST_SESSION_USER_FUNCTION:
    case MYLITE_SQL_AST_SYSTEM_USER_FUNCTION:
    case MYLITE_SQL_AST_CURRENT_USER_FUNCTION:
    case MYLITE_SQL_AST_CURRENT_ROLE_FUNCTION:
    case MYLITE_SQL_AST_CURRENT_ROLE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_CONNECTION_ID_FUNCTION:
    case MYLITE_SQL_AST_CONNECTION_ID_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_VERSION_FUNCTION:
    case MYLITE_SQL_AST_VERSION_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST:
    case MYLITE_SQL_AST_ROW_COUNT_FUNCTION:
    case MYLITE_SQL_AST_LAST_INSERT_ID_FUNCTION:
    case MYLITE_SQL_AST_COUNT_STAR_FUNCTION:
    case MYLITE_SQL_AST_COUNT_COLUMN_FUNCTION:
    case MYLITE_SQL_AST_COUNT_LITERAL_FUNCTION:
    case MYLITE_SQL_AST_COUNT_DISTINCT_COLUMN_FUNCTION:
    case MYLITE_SQL_AST_MIN_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_MAX_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_SYSTEM_VARIABLE:
        break;
    }

    set_unsupported_error(database, "statement is not supported by this MyLite build");

    return MYLITE_ERROR;
}

static int execute_empty_statement(struct mylite_db *database, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_use_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct mylite_catalog_schema_descriptor schema = {0};
    char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    mylite_result *result = NULL;
    int written = 0;
    int rc =
        copy_identifier_text(child_at(statement, 0U), schema_name, sizeof(schema_name), database);

    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(schema_name)) {
        set_reserved_name_error(database, "database", schema_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = resolve_schema_name(database, schema_name, &schema);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    written = snprintf(
        database->session.selected_schema,
        sizeof(database->session.selected_schema),
        "%s",
        schema.name
    );
    if (written < 0 || (size_t)written >= sizeof(database->session.selected_schema)) {
        mylite_result_free(result);
        set_identifier_too_long_error(database, "database");
        return MYLITE_ERROR;
    }
    database->session.has_selected_schema = true;

    return finish_successful_result(database, result, out_result);
}

static int execute_create_table_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_create_table plan = {0};
    mylite_result *result = NULL;
    bool finished = false;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_create_table(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = maybe_finish_create_if_not_exists_noop(database, statement, &plan, &finished);
    }
    if (rc == MYLITE_OK && !finished) {
        rc = finalize_planned_column_defaults(database, plan.columns, plan.column_count);
    }
    if (rc == MYLITE_OK && !finished) {
        rc = check_duplicate_column_names(database, plan.columns, plan.column_count);
    }
    if (rc == MYLITE_OK && !finished) {
        rc = create_table_from_plan(database, &plan);
    }
    planned_create_table_deinit(&plan);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_create_schema_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    mylite_result *result = NULL;
    bool finished = false;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = maybe_finish_create_schema_if_not_exists_noop(database, statement, result, &finished);
    if (rc == MYLITE_OK && finished) {
        return finish_successful_result(database, result, out_result);
    }
    if (rc == MYLITE_OK) {
        rc = create_schema_from_statement(database, statement, result);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int maybe_finish_create_schema_if_not_exists_noop(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result *result,
    bool *out_finished
) {
    char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    struct mylite_catalog_schema_descriptor schema = {0};
    bool found = false;
    int rc = MYLITE_OK;

    *out_finished = false;
    if (!create_schema_has_if_not_exists(statement)) {
        return MYLITE_OK;
    }

    rc = copy_identifier_text(child_at(statement, 0U), schema_name, sizeof(schema_name), database);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(schema_name)) {
        set_reserved_name_error(database, "database", schema_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_try_read_schema_by_name(database, schema_name, &schema, &found);
        if (rc != MYLITE_OK) {
            set_internal_error_if_clear(database, rc, "failed to read schema descriptor");
        }
    }
    if (rc != MYLITE_OK || !found) {
        return rc;
    }

    rc = append_database_exists_note(database, schema_name);
    if (rc == MYLITE_OK) {
        mylite_result_set_affected_rows(result, 1);
        *out_finished = true;
    }
    return rc;
}

static bool create_schema_has_if_not_exists(const struct mylite_sql_ast_node *statement) {
    return child_with_kind(statement, MYLITE_SQL_AST_CREATE_SCHEMA_IF_NOT_EXISTS_CLAUSE) != NULL;
}

static int execute_drop_table_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_drop_table plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_drop_table(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = append_drop_table_missing_notes(database, &plan);
    }
    if (rc == MYLITE_OK) {
        rc = execute_drop_table_from_plan(database, &plan);
    }
    planned_drop_table_deinit(&plan);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int plan_drop_table(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_drop_table *out_plan
) {
    const struct mylite_sql_ast_node *target_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *target_node = NULL;
    bool drop_if_exists = drop_table_has_if_exists(statement);
    int rc = MYLITE_OK;

    *out_plan = (struct planned_drop_table){0};
    if (target_list == NULL || target_list->kind != MYLITE_SQL_AST_TABLE_NAME_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    out_plan->target_count = mylite_sql_ast_node_child_count(target_list);
    if (out_plan->target_count == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (out_plan->target_count > SIZE_MAX / sizeof(*out_plan->targets)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_plan->targets = calloc(out_plan->target_count, sizeof(*out_plan->targets));
    if (out_plan->targets == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    target_node = child_at(target_list, 0U);
    for (size_t target_index = 0U; rc == MYLITE_OK && target_index < out_plan->target_count;
         ++target_index) {
        rc = plan_drop_table_target(database, target_node, out_plan, target_index);
        if (target_node != NULL) {
            target_node = target_node->next_sibling;
        }
    }
    if (rc == MYLITE_OK && !drop_if_exists && out_plan->missing_count != 0U) {
        rc = finish_drop_table_missing_targets(database, out_plan);
    }
    if (rc != MYLITE_OK) {
        planned_drop_table_deinit(out_plan);
    }

    return rc;
}

static int plan_drop_table_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *target_node,
    struct planned_drop_table *out_plan,
    size_t target_index
) {
    struct planned_drop_table_target *target = &out_plan->targets[target_index];
    bool missing_schema = false;
    bool found = false;
    int rc = MYLITE_OK;

    rc = resolve_drop_if_exists_table_name(database, target_node, &target->target, &missing_schema);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(target->target.table_name)) {
        set_reserved_name_error(database, "table", target->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = check_drop_table_duplicate_targets(database, out_plan, target_index);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (missing_schema) {
        target->missing = true;
        ++out_plan->missing_count;
        return MYLITE_OK;
    }

    rc = mylite_catalog_try_read_table_by_name(
        database,
        target->target.schema.schema_id,
        target->target.table_name,
        &target->table,
        &found
    );
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to read table descriptor");
        return rc;
    }
    if (!found) {
        target->missing = true;
        ++out_plan->missing_count;
        return MYLITE_OK;
    }

    ++out_plan->existing_count;
    return MYLITE_OK;
}

static int check_drop_table_duplicate_targets(
    struct mylite_db *database,
    const struct planned_drop_table *plan,
    size_t target_index
) {
    const struct planned_drop_table_target *target = &plan->targets[target_index];

    for (size_t previous_index = 0U; previous_index < target_index; ++previous_index) {
        if (drop_table_targets_match(&plan->targets[previous_index], target)) {
            set_duplicate_table_alias_error(database, target->target.table_name);
            return MYLITE_ERROR;
        }
    }

    return MYLITE_OK;
}

static bool drop_table_targets_match(
    const struct planned_drop_table_target *left,
    const struct planned_drop_table_target *right
) {
    if (strcmp(left->target.schema.name, right->target.schema.name) != 0) {
        return false;
    }
    return strcmp(left->target.table_name, right->target.table_name) == 0;
}

static int finish_drop_table_missing_targets(
    struct mylite_db *database,
    const struct planned_drop_table *plan
) {
    if (plan->missing_count == 1U) {
        for (size_t target_index = 0U; target_index < plan->target_count; ++target_index) {
            const struct planned_drop_table_target *target = &plan->targets[target_index];

            if (target->missing) {
                set_unknown_table_error(
                    database,
                    target->target.schema.name,
                    target->target.table_name
                );
                return MYLITE_ERROR;
            }
        }
    }

    return set_unknown_drop_tables_error(database, plan);
}

static int append_drop_table_missing_notes(
    struct mylite_db *database,
    const struct planned_drop_table *plan
) {
    int rc = MYLITE_OK;

    for (size_t target_index = 0U; rc == MYLITE_OK && target_index < plan->target_count;
         ++target_index) {
        const struct planned_drop_table_target *target = &plan->targets[target_index];

        if (target->missing) {
            rc = append_unknown_table_note(
                database,
                target->target.schema.name,
                target->target.table_name
            );
        }
    }

    return rc;
}

static void planned_drop_table_deinit(struct planned_drop_table *plan) {
    if (plan == NULL) {
        return;
    }

    free(plan->targets);
    *plan = (struct planned_drop_table){0};
}

static int execute_drop_table_from_plan(
    struct mylite_db *database,
    const struct planned_drop_table *plan
) {
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    int rc = MYLITE_OK;

    if (plan->existing_count == 0U) {
        return MYLITE_OK;
    }

    rc = mylite_catalog_begin_mutation(database, &mutation);
    for (size_t target_index = 0U; rc == MYLITE_OK && target_index < plan->target_count;
         ++target_index) {
        const struct planned_drop_table_target *target = &plan->targets[target_index];

        if (!target->missing) {
            rc = mylite_catalog_delete_table_in_mutation(
                database,
                &mutation,
                target->table.table_id
            );
        }
    }
    for (size_t target_index = 0U; rc == MYLITE_OK && target_index < plan->target_count;
         ++target_index) {
        const struct planned_drop_table_target *target = &plan->targets[target_index];

        if (!target->missing) {
            rc = execute_physical_drop_table(database, target->table.physical_name);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        mylite_catalog_rollback_mutation(database, &mutation);
        return rc;
    }

    ++database->session.sqlite_schema_generation;

    return MYLITE_OK;
}

static int maybe_finish_create_if_not_exists_noop(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct planned_create_table *plan,
    bool *out_finished
) {
    struct mylite_catalog_table_descriptor table = {0};
    bool found = false;
    int rc = MYLITE_OK;

    *out_finished = false;
    if (!create_table_has_if_not_exists(statement)) {
        return MYLITE_OK;
    }

    rc = mylite_catalog_try_read_table_by_name(
        database,
        plan->target.schema.schema_id,
        plan->target.table_name,
        &table,
        &found
    );
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to read table descriptor");
        return rc;
    }
    if (!found) {
        return MYLITE_OK;
    }

    rc = append_table_exists_note(database, plan->target.table_name);
    if (rc == MYLITE_OK) {
        *out_finished = true;
    }
    return rc;
}

static bool create_table_has_if_not_exists(const struct mylite_sql_ast_node *statement) {
    return child_with_kind(statement, MYLITE_SQL_AST_CREATE_IF_NOT_EXISTS_CLAUSE) != NULL;
}

static const struct mylite_sql_ast_node *create_table_options_node(
    const struct mylite_sql_ast_node *statement
) {
    return child_with_kind(statement, MYLITE_SQL_AST_TABLE_OPTION_LIST);
}

static bool drop_table_has_if_exists(const struct mylite_sql_ast_node *statement) {
    return child_with_kind(statement, MYLITE_SQL_AST_DROP_IF_EXISTS_CLAUSE) != NULL;
}

static int execute_drop_schema_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_drop_schema plan = {0};
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    mylite_result *result = NULL;
    bool finished = false;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = maybe_finish_drop_schema_if_exists_noop(database, statement, &finished);
    if (rc == MYLITE_OK && finished) {
        return finish_successful_result_with_warning_count(result, 1U, out_result);
    }
    if (rc == MYLITE_OK) {
        rc = plan_drop_schema(database, statement, &mutation, &plan);
    }
    if (rc == MYLITE_OK) {
        rc = drop_schema_from_plan(database, &mutation, &plan, result);
    }
    if (rc != MYLITE_OK) {
        mylite_catalog_rollback_mutation(database, &mutation);
    }
    planned_drop_schema_deinit(&plan);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int maybe_finish_drop_schema_if_exists_noop(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool *out_finished
) {
    char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    struct mylite_catalog_schema_descriptor schema = {0};
    bool found = false;
    int rc = MYLITE_OK;

    *out_finished = false;
    if (!drop_schema_has_if_exists(statement)) {
        return MYLITE_OK;
    }

    rc = copy_identifier_text(child_at(statement, 0U), schema_name, sizeof(schema_name), database);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(schema_name)) {
        set_reserved_name_error(database, "database", schema_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_try_read_schema_by_name(database, schema_name, &schema, &found);
        if (rc != MYLITE_OK) {
            set_internal_error_if_clear(database, rc, "failed to read schema descriptor");
        }
    }
    if (rc != MYLITE_OK || found) {
        return rc;
    }

    *out_finished = true;
    return MYLITE_OK;
}

static bool drop_schema_has_if_exists(const struct mylite_sql_ast_node *statement) {
    return child_with_kind(statement, MYLITE_SQL_AST_DROP_SCHEMA_IF_EXISTS_CLAUSE) != NULL;
}

static int execute_truncate_table_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_truncate_table plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_truncate_table(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = execute_truncate_from_plan(database, &plan, result);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_rename_table_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_rename_table_statement plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_rename_table_statement(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = rename_table_statement_from_plan(database, &plan);
    }
    planned_rename_table_statement_deinit(&plan);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_alter_table_rename_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_rename_table plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_rename(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = alter_table_rename_from_plan(database, &plan);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_alter_table_add_column_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_add_column plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_add_column(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = alter_table_add_column_from_plan(database, &plan);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_alter_table_drop_column_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_drop_column plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_drop_column(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = alter_table_drop_column_from_plan(database, &plan);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_alter_table_rename_column_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_rename_column plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_rename_column(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = alter_table_rename_column_from_plan(database, &plan);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_alter_table_modify_column_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_modify_column plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_modify_column(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = alter_table_modify_column_from_plan(database, &plan);
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_modify_column_deinit(&plan);
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, plan.affected_rows);
    planned_alter_table_modify_column_deinit(&plan);
    return finish_successful_result(database, result, out_result);
}

static int execute_alter_table_change_column_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_modify_column plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_change_column(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = alter_table_modify_column_from_plan(database, &plan);
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_modify_column_deinit(&plan);
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, plan.affected_rows);
    planned_alter_table_modify_column_deinit(&plan);
    return finish_successful_result(database, result, out_result);
}

static int execute_alter_table_set_default_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_set_default plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_set_default(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = alter_table_set_default_from_plan(database, &plan);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int execute_alter_table_drop_default_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_drop_default plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_drop_default(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = alter_table_drop_default_from_plan(database, &plan);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int execute_alter_table_column_visibility_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_column_visibility plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_column_visibility(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = alter_table_column_visibility_from_plan(database, &plan);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int execute_insert_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_insert plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_insert(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = execute_insert_from_plan(database, &plan, result);
    }
    planned_insert_deinit(&plan);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_insert_set_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_insert plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_insert_set(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = execute_insert_from_plan(database, &plan, result);
    }
    planned_insert_deinit(&plan);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_delete_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_delete plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_delete(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = execute_delete_from_plan(database, &plan, result);
    }
    planned_delete_deinit(&plan);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_update_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_update plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_update(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = execute_update_from_plan(database, &plan, result);
    }
    planned_update_deinit(&plan);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_select plan = {0};
    struct planned_count count_plan = {false};
    struct planned_min_max_aggregate min_max_plan = {0};
    const char *argument_count_error_function = NULL;
    int rc = MYLITE_OK;

    argument_count_error_function = select_statement_argument_count_error_function(statement);
    if (argument_count_error_function != NULL) {
        set_native_function_parameter_count_error(database, argument_count_error_function);
        return MYLITE_ERROR;
    }
    if (select_statement_is_session_scalar_projection(statement) ||
        select_statement_is_literal_projection(statement)) {
        return execute_scalar_projection_select_statement(database, statement, out_result);
    }
    if (select_statement_has_count_aggregate(statement)) {
        rc = plan_count(database, statement, &count_plan);
        if (rc == MYLITE_OK) {
            rc = execute_count_from_plan(database, &count_plan, out_result);
        }
        return rc;
    }
    if (select_statement_has_min_max_aggregate(statement)) {
        rc = plan_min_max_aggregate(database, statement, &min_max_plan);
        if (rc == MYLITE_OK) {
            rc = execute_min_max_aggregate_from_plan(database, &min_max_plan, out_result);
        }
        return rc;
    }
    if (select_statement_is_literal_projection_attempt(statement)) {
        set_unsupported_error(
            database,
            "SELECT literal projection supports only unparenthesized integer, boolean, and NULL "
            "literals"
        );
        return MYLITE_ERROR;
    }

    rc = plan_select(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = execute_select_from_plan(database, &plan, out_result);
    }
    planned_select_deinit(&plan);

    return rc;
}

static int execute_show_tables_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct mylite_catalog_schema_descriptor schema = {0};
    const struct mylite_sql_ast_node *first_child = child_at(statement, 0U);
    const struct mylite_sql_ast_node *second_child = child_at(statement, 1U);
    const struct mylite_sql_ast_node *schema_node = first_child;
    const struct mylite_sql_ast_node *like_node = second_child;
    struct show_like_filter filter = {
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
    char *column_name = NULL;
    struct show_tables_context context = {0};
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    if (first_child != NULL && first_child->kind == MYLITE_SQL_AST_LITERAL) {
        schema_node = NULL;
        like_node = first_child;
    }
    if (schema_node == NULL) {
        rc = resolve_selected_schema(database, &schema);
    } else {
        char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];

        rc = copy_identifier_text(schema_node, schema_name, sizeof(schema_name), database);
        if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(schema_name)) {
            set_reserved_name_error(database, "database", schema_name);
            rc = MYLITE_ERROR;
        }
        if (rc == MYLITE_OK) {
            rc = resolve_schema_name(database, schema_name, &schema);
        }
    }
    if (rc == MYLITE_OK) {
        rc = make_show_like_filter(database, like_node, &filter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = build_show_tables_column_name(schema.name, &filter, &column_name);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_column(result, column_name);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        context.result = result;
        context.filter = &filter;
        rc = mylite_catalog_for_each_table_in_schema(
            database,
            schema.schema_id,
            append_show_table,
            &context
        );
        if (rc != MYLITE_OK) {
            set_runtime_error(database, "failed to build SHOW TABLES result");
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        free(column_name);
        show_like_filter_deinit(&filter);
        return rc;
    }

    free(column_name);
    show_like_filter_deinit(&filter);
    return finish_successful_result(database, result, out_result);
}

static int execute_show_table_status_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[show_table_status_result_column_count] = {
        "Name",
        "Engine",
        "Version",
        "Row_format",
        "Rows",
        "Avg_row_length",
        "Data_length",
        "Max_data_length",
        "Index_length",
        "Data_free",
        "Auto_increment",
        "Create_time",
        "Update_time",
        "Check_time",
        "Collation",
        "Checksum",
        "Create_options",
        "Comment",
    };
    struct mylite_catalog_schema_descriptor schema = {0};
    const struct mylite_sql_ast_node *first_child = child_at(statement, 0U);
    const struct mylite_sql_ast_node *second_child = child_at(statement, 1U);
    const struct mylite_sql_ast_node *schema_node = first_child;
    const struct mylite_sql_ast_node *like_node = second_child;
    struct show_like_filter filter = {
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
    struct show_table_status_context context = {0};
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    if (first_child != NULL && first_child->kind == MYLITE_SQL_AST_LITERAL) {
        schema_node = NULL;
        like_node = first_child;
    }
    if (schema_node == NULL) {
        rc = resolve_selected_schema(database, &schema);
    } else {
        char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];

        rc = copy_identifier_text(schema_node, schema_name, sizeof(schema_name), database);
        if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(schema_name)) {
            set_reserved_name_error(database, "database", schema_name);
            rc = MYLITE_ERROR;
        }
        if (rc == MYLITE_OK) {
            rc = resolve_schema_name(database, schema_name, &schema);
        }
    }
    if (rc == MYLITE_OK) {
        rc = make_show_like_filter(database, like_node, &filter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_table_status_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        context.database = database;
        context.result = result;
        context.filter = &filter;
        rc = mylite_catalog_for_each_table_in_schema(
            database,
            schema.schema_id,
            append_show_table_status,
            &context
        );
        if (rc != MYLITE_OK &&
            mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) == MYLITE_OK) {
            set_runtime_error(database, "failed to build SHOW TABLE STATUS result");
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        show_like_filter_deinit(&filter);
        return rc;
    }

    show_like_filter_deinit(&filter);
    return finish_successful_result(database, result, out_result);
}

static int execute_show_character_set_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[show_character_set_result_column_count] = {
        "Charset",
        "Description",
        "Default collation",
        "Maxlen",
    };
    static const char *const values[show_character_set_result_column_count] = {
        "utf8mb4",
        "UTF-8 Unicode",
        "utf8mb4_0900_ai_ci",
        "4",
    };
    struct show_like_filter filter = {
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    rc = make_show_like_filter(database, child_at(statement, 0U), &filter);
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_character_set_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK && show_like_filter_matches(&filter, values[0], false)) {
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        show_like_filter_deinit(&filter);
        return rc;
    }

    show_like_filter_deinit(&filter);
    return finish_successful_result(database, result, out_result);
}

static int execute_show_collation_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[show_collation_result_column_count] = {
        "Collation",
        "Charset",
        "Id",
        "Default",
        "Compiled",
        "Sortlen",
        "Pad_attribute",
    };
    static const char *const values[show_collation_result_column_count] = {
        "utf8mb4_0900_ai_ci",
        "utf8mb4",
        "255",
        "Yes",
        "Yes",
        "0",
        "NO PAD",
    };
    struct show_like_filter filter = {
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    rc = make_show_like_filter(database, child_at(statement, 0U), &filter);
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_collation_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK && show_like_filter_matches(&filter, values[0], false)) {
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        show_like_filter_deinit(&filter);
        return rc;
    }

    show_like_filter_deinit(&filter);
    return finish_successful_result(database, result, out_result);
}

static int execute_show_triggers_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[show_triggers_result_column_count] = {
        "Trigger",
        "Event",
        "Table",
        "Statement",
        "Timing",
        "Created",
        "sql_mode",
        "Definer",
        "character_set_client",
        "collation_connection",
        "Database Collation",
    };
    struct mylite_catalog_schema_descriptor schema = {0};
    const struct mylite_sql_ast_node *first_child = child_at(statement, 0U);
    const struct mylite_sql_ast_node *second_child = child_at(statement, 1U);
    const struct mylite_sql_ast_node *schema_node = first_child;
    const struct mylite_sql_ast_node *like_node = second_child;
    struct show_like_filter filter = {
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    if (first_child != NULL && first_child->kind == MYLITE_SQL_AST_LITERAL) {
        schema_node = NULL;
        like_node = first_child;
    }
    if (schema_node == NULL) {
        rc = resolve_selected_schema(database, &schema);
    } else {
        char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];

        rc = copy_identifier_text(schema_node, schema_name, sizeof(schema_name), database);
        if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(schema_name)) {
            set_reserved_name_error(database, "database", schema_name);
            rc = MYLITE_ERROR;
        }
        if (rc == MYLITE_OK) {
            rc = resolve_schema_name(database, schema_name, &schema);
        }
    }
    if (rc == MYLITE_OK) {
        rc = make_show_like_filter(database, like_node, &filter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_triggers_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        show_like_filter_deinit(&filter);
        return rc;
    }

    show_like_filter_deinit(&filter);
    return finish_successful_result(database, result, out_result);
}

static int execute_show_events_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[show_events_result_column_count] = {
        "Db",
        "Name",
        "Definer",
        "Time zone",
        "Type",
        "Execute at",
        "Interval value",
        "Interval field",
        "Starts",
        "Ends",
        "Status",
        "Originator",
        "character_set_client",
        "collation_connection",
        "Database Collation",
    };
    const struct mylite_sql_ast_node *first_child = child_at(statement, 0U);
    const struct mylite_sql_ast_node *second_child = child_at(statement, 1U);
    const struct mylite_sql_ast_node *schema_node = first_child;
    const struct mylite_sql_ast_node *like_node = second_child;
    struct show_like_filter filter = {
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    if (first_child != NULL && first_child->kind == MYLITE_SQL_AST_LITERAL) {
        schema_node = NULL;
        like_node = first_child;
    }
    if (schema_node == NULL) {
        struct mylite_catalog_schema_descriptor schema = {0};

        rc = resolve_selected_schema(database, &schema);
    } else {
        char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];

        rc = copy_identifier_text(schema_node, schema_name, sizeof(schema_name), database);
        if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(schema_name)) {
            set_reserved_name_error(database, "database", schema_name);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        rc = make_show_like_filter(database, like_node, &filter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_events_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        show_like_filter_deinit(&filter);
        return rc;
    }

    show_like_filter_deinit(&filter);
    return finish_successful_result(database, result, out_result);
}

static int execute_show_open_tables_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[show_open_tables_result_column_count] = {
        "Database",
        "Table",
        "In_use",
        "Name_locked",
    };
    const struct mylite_sql_ast_node *first_child = child_at(statement, 0U);
    const struct mylite_sql_ast_node *second_child = child_at(statement, 1U);
    const struct mylite_sql_ast_node *schema_node = first_child;
    const struct mylite_sql_ast_node *like_node = second_child;
    struct show_like_filter filter = {
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    if (first_child != NULL && first_child->kind == MYLITE_SQL_AST_LITERAL) {
        schema_node = NULL;
        like_node = first_child;
    }
    if (schema_node != NULL) {
        char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];

        rc = copy_identifier_text(schema_node, schema_name, sizeof(schema_name), database);
        if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(schema_name)) {
            set_reserved_name_error(database, "database", schema_name);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        rc = make_show_like_filter(database, like_node, &filter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_open_tables_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        show_like_filter_deinit(&filter);
        return rc;
    }

    show_like_filter_deinit(&filter);
    return finish_successful_result(database, result, out_result);
}

static int execute_show_routine_status_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[show_routine_status_result_column_count] = {
        "Db",
        "Name",
        "Type",
        "Language",
        "Definer",
        "Modified",
        "Created",
        "Security_type",
        "Comment",
        "character_set_client",
        "collation_connection",
        "Database Collation",
    };
    const struct mylite_sql_ast_node *like_node = child_at(statement, 0U);
    struct show_like_filter filter = {
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    rc = make_show_like_filter(database, like_node, &filter);
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_routine_status_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        show_like_filter_deinit(&filter);
        return rc;
    }

    show_like_filter_deinit(&filter);
    return finish_successful_result(database, result, out_result);
}

static int execute_show_processlist_statement(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[show_processlist_result_column_count] = {
        "Id",
        "User",
        "Host",
        "db",
        "Command",
        "Time",
        "State",
        "Info",
    };
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_processlist_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_show_processlist_row(database, context, statement, result);
    }
    if (rc == MYLITE_OK) {
        rc = append_show_processlist_warning(database);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int append_show_processlist_row(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    mylite_result *result
) {
    char connection_id_text[integer_text_capacity];
    char user[MYLITE_SESSION_IDENTIFIER_CAPACITY];
    char host[MYLITE_SESSION_IDENTIFIER_CAPACITY];
    char *info = NULL;
    const char *selected_schema = NULL;
    int written = 0;
    int rc = MYLITE_OK;

    const char *values[show_processlist_result_column_count] = {
        connection_id_text,
        user,
        host,
        NULL,
        "Query",
        "0",
        "init",
        NULL,
    };

    written = snprintf(
        connection_id_text,
        sizeof(connection_id_text),
        "%" PRIu64,
        database->session.connection_id
    );
    if (written < 0 || (size_t)written >= sizeof(connection_id_text)) {
        set_runtime_error(database, "failed to format SHOW PROCESSLIST Id");
        return MYLITE_ERROR;
    }
    if (database->session.has_selected_schema) {
        selected_schema = database->session.selected_schema;
    }
    values[show_processlist_db_column] = selected_schema;

    rc = format_show_processlist_user_host(database, user, sizeof(user), host, sizeof(host));
    if (rc == MYLITE_OK) {
        rc = copy_show_processlist_info(database, context, statement, &info);
    }
    if (rc == MYLITE_OK) {
        values[show_processlist_info_column] = info;
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }

    free(info);
    return rc;
}

static int format_show_processlist_user_host(
    struct mylite_db *database,
    char *user,
    size_t user_size,
    char *host,
    size_t host_size
) {
    const char *identity = database->session.client_user_identity;
    const char *at_sign = strchr(identity, '@');
    const char *host_part = at_sign == NULL ? "" : at_sign + 1;
    size_t user_length = at_sign == NULL ? strlen(identity) : (size_t)(at_sign - identity);
    size_t host_length = strlen(host_part);

    if (user_length >= user_size || host_length >= host_size) {
        set_runtime_error(database, "failed to format SHOW PROCESSLIST user identity");
        return MYLITE_ERROR;
    }

    memcpy(user, identity, user_length);
    user[user_length] = '\0';
    memcpy(host, host_part, host_length);
    host[host_length] = '\0';

    return MYLITE_OK;
}

static int copy_show_processlist_info(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    char **out_info
) {
    const char *sql = mylite_statement_context_sql(context);
    size_t sql_size = mylite_statement_context_sql_size(context);
    size_t length = 0U;
    char *info = NULL;

    if (out_info == NULL) {
        return MYLITE_MISUSE;
    }
    *out_info = NULL;
    if (sql == NULL) {
        set_runtime_error(database, "failed to read SHOW PROCESSLIST statement text");
        return MYLITE_ERROR;
    }

    length = statement_info_length_without_terminator(sql, sql_size);
    if (statement->kind != MYLITE_SQL_AST_SHOW_FULL_PROCESSLIST_STATEMENT &&
        length > show_processlist_info_truncation_length) {
        length = show_processlist_info_truncation_length;
    }
    if (length == SIZE_MAX) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    info = (char *)malloc(length + 1U);
    if (info == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (length > 0U) {
        memcpy(info, sql, length);
    }
    info[length] = '\0';
    *out_info = info;

    return MYLITE_OK;
}

static size_t statement_info_length_without_terminator(const char *sql, size_t sql_size) {
    size_t length = sql_size;

    while (length > 0U &&
           (sql[length - 1U] == ' ' || sql[length - 1U] == '\t' || sql[length - 1U] == '\n' ||
            sql[length - 1U] == '\r' || sql[length - 1U] == '\f' || sql[length - 1U] == '\v')) {
        --length;
    }
    if (length > 0U && sql[length - 1U] == ';') {
        --length;
    }
    while (length > 0U &&
           (sql[length - 1U] == ' ' || sql[length - 1U] == '\t' || sql[length - 1U] == '\n' ||
            sql[length - 1U] == '\r' || sql[length - 1U] == '\f' || sql[length - 1U] == '\v')) {
        --length;
    }

    return length;
}

static int append_show_processlist_warning(struct mylite_db *database) {
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_information_schema_processlist_deprecated,
        "HY000",
        "'INFORMATION_SCHEMA.PROCESSLIST' is deprecated and will be removed in a future "
        "release. Please use performance_schema.processlist instead"
    );
}

static int execute_show_warnings_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[show_warnings_result_column_count] = {
        "Level",
        "Code",
        "Message",
    };
    struct planned_diagnostics_show_limit limit;
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    rc = plan_diagnostics_show_limit(database, child_at(statement, 0U), &limit);
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_warnings_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_show_diagnostics_rows(database, result, &limit);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_show_count_warnings_statement(
    struct mylite_db *database,
    mylite_result **out_result
) {
    static const char *const result_columns[show_count_warnings_result_column_count] = {
        "@@session.warning_count",
    };
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_count_warnings_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_show_count_warnings_row(database, result);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_show_errors_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[show_errors_result_column_count] = {
        "Level",
        "Code",
        "Message",
    };
    struct planned_diagnostics_show_limit limit;
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    rc = plan_diagnostics_show_limit(database, child_at(statement, 0U), &limit);
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_errors_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_show_errors_rows(database, result, &limit);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_show_count_errors_statement(
    struct mylite_db *database,
    mylite_result **out_result
) {
    static const char *const result_columns[show_count_errors_result_column_count] = {
        "@@session.error_count",
    };
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_count_errors_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_show_count_errors_row(database, result);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int plan_diagnostics_show_limit(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *limit_clause,
    struct planned_diagnostics_show_limit *out_limit
) {
    int rc = MYLITE_OK;

    if (out_limit == NULL) {
        set_runtime_error(database, "invalid diagnostics SHOW LIMIT plan");
        return MYLITE_ERROR;
    }

    out_limit->has_limit = false;
    out_limit->row_count = UINT64_MAX;
    out_limit->has_offset = false;
    out_limit->offset = 0U;
    if (limit_clause == NULL) {
        return MYLITE_OK;
    }
    if (limit_clause->kind != MYLITE_SQL_AST_LIMIT_CLAUSE) {
        set_unsupported_error(database, "diagnostics SHOW supports only literal LIMIT clauses");
        return MYLITE_ERROR;
    }

    rc = convert_diagnostics_show_limit_integer_literal(
        database,
        child_at(limit_clause, 0U),
        &out_limit->row_count
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    out_limit->has_limit = true;

    if (child_at(limit_clause, 1U) != NULL) {
        rc = convert_diagnostics_show_limit_integer_literal(
            database,
            child_at(limit_clause, 1U),
            &out_limit->offset
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        out_limit->has_offset = true;
    }

    return MYLITE_OK;
}

static int convert_diagnostics_show_limit_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_value
) {
    uint64_t value = 0U;
    int rc = MYLITE_OK;

    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_unsupported_error(
            database,
            "diagnostics SHOW supports only unsigned decimal integer LIMIT literals"
        );
        return MYLITE_ERROR;
    }

    rc = parse_unsigned_integer_literal(&value_node->span, &value);
    if (rc != MYLITE_OK) {
        set_limit_out_of_range_error(database);
        return MYLITE_ERROR;
    }

    *out_value = value;
    return MYLITE_OK;
}

static int append_show_diagnostics_rows(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_diagnostics_show_limit *limit
) {
    const struct mylite_diagnostics *diagnostics = &database->previous_diagnostics;
    uint64_t total_count = 0U;
    uint64_t start_index = 0U;
    uint64_t row_count = UINT64_MAX;
    uint64_t emitted_count = 0U;
    uint64_t condition_count = 0U;
    uint64_t warning_count = 0U;
    int rc = previous_diagnostics_condition_count(diagnostics, &condition_count);

    if (limit->has_offset) {
        start_index = limit->offset;
    }
    if (limit->has_limit) {
        row_count = limit->row_count;
    }
    if (rc == MYLITE_OK) {
        warning_count = (uint64_t)mylite_diagnostics_warning_count(diagnostics);
        total_count = condition_count + warning_count;
    }

    for (uint64_t index = start_index;
         rc == MYLITE_OK && index < total_count && emitted_count < row_count;
         ++index, ++emitted_count) {
        if (index < warning_count) {
            const struct mylite_diagnostic_record *warning =
                mylite_diagnostics_warning_at(diagnostics, (size_t)index);
            if (warning == NULL) {
                set_runtime_error(database, "invalid previous diagnostics warning index");
                rc = MYLITE_ERROR;
            } else {
                rc = append_show_diagnostics_row(database, result, warning->level, warning);
            }
        } else if (condition_count != 0U) {
            rc = append_show_diagnostics_row(database, result, "Error", &diagnostics->condition);
        }
    }

    return rc;
}

static int append_show_diagnostics_row(
    struct mylite_db *database,
    mylite_result *result,
    const char *level,
    const struct mylite_diagnostic_record *record
) {
    char code_text[integer_text_capacity];
    const char *values[show_warnings_result_column_count] = {
        level,
        code_text,
        record == NULL ? NULL : record->message,
    };
    int written = 0;
    int rc = MYLITE_OK;

    if (record == NULL) {
        set_runtime_error(database, "invalid diagnostics record");
        return MYLITE_ERROR;
    }

    written = snprintf(code_text, sizeof(code_text), "%d", record->code);
    if (written < 0 || (size_t)written >= sizeof(code_text)) {
        set_runtime_error(database, "failed to format diagnostic code");
        return MYLITE_ERROR;
    }

    rc = mylite_result_append_text_row(result, values);
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }
    return rc;
}

static int append_show_count_warnings_row(struct mylite_db *database, mylite_result *result) {
    const struct mylite_diagnostics *diagnostics = &database->previous_diagnostics;
    char count_text[integer_text_capacity];
    const char *values[show_count_warnings_result_column_count] = {count_text};
    uint64_t condition_count = 0U;
    uint64_t count = 0U;
    int rc = previous_diagnostics_condition_count(diagnostics, &condition_count);

    if (rc != MYLITE_OK) {
        return rc;
    }

    count = condition_count + (uint64_t)mylite_diagnostics_warning_count(diagnostics);
    rc = format_uint64(database, count, count_text, sizeof(count_text));
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }

    return rc;
}

static int append_show_errors_rows(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_diagnostics_show_limit *limit
) {
    const struct mylite_diagnostics *diagnostics = &database->previous_diagnostics;
    uint64_t start_index = 0U;
    uint64_t row_count = UINT64_MAX;

    if (!diagnostics_has_error_condition(diagnostics)) {
        return MYLITE_OK;
    }

    if (limit->has_offset) {
        start_index = limit->offset;
    }
    if (limit->has_limit) {
        row_count = limit->row_count;
    }
    if (start_index != 0U || row_count == 0U) {
        return MYLITE_OK;
    }

    return append_show_diagnostics_row(database, result, "Error", &diagnostics->condition);
}

static int append_show_count_errors_row(struct mylite_db *database, mylite_result *result) {
    const struct mylite_diagnostics *diagnostics = &database->previous_diagnostics;
    char count_text[integer_text_capacity];
    const char *values[show_count_errors_result_column_count] = {count_text};
    uint64_t count = 0U;
    int rc = MYLITE_OK;

    if (diagnostics_has_error_condition(diagnostics)) {
        count = 1U;
    }

    rc = format_uint64(database, count, count_text, sizeof(count_text));
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }

    return rc;
}

static int previous_diagnostics_condition_count(
    const struct mylite_diagnostics *diagnostics,
    uint64_t *out_count
) {
    uint64_t warning_count = 0U;

    if (diagnostics == NULL || out_count == NULL) {
        return MYLITE_MISUSE;
    }

    warning_count = (uint64_t)mylite_diagnostics_warning_count(diagnostics);
    if (diagnostics_has_error_condition(diagnostics) && warning_count == UINT64_MAX) {
        return MYLITE_NOMEM;
    }

    *out_count = 0U;
    if (diagnostics_has_error_condition(diagnostics)) {
        *out_count = 1U;
    }
    return MYLITE_OK;
}

static bool diagnostics_has_error_condition(const struct mylite_diagnostics *diagnostics) {
    if (diagnostics == NULL) {
        return false;
    }
    if (mylite_diagnostics_errcode(diagnostics) == MYLITE_OK) {
        return false;
    }
    return true;
}

static int format_uint64(
    struct mylite_db *database,
    uint64_t value,
    char *buffer,
    size_t buffer_size
) {
    int written = snprintf(buffer, buffer_size, "%" PRIu64, value);

    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format unsigned integer");
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int execute_show_columns_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[] =
        {"Field", "Type", "Null", "Key", "Default", "Extra"};
    struct table_name_resolution target = {0};
    struct mylite_catalog_table_descriptor table = {0};
    const struct mylite_sql_ast_node *second_child = child_at(statement, 1U);
    const struct mylite_sql_ast_node *schema_node = second_child;
    const struct mylite_sql_ast_node *like_node = child_at(statement, 2U);
    struct show_like_filter filter = {
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
    struct show_columns_context context = {0};
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    if (second_child != NULL && second_child->kind == MYLITE_SQL_AST_LITERAL) {
        schema_node = NULL;
        like_node = second_child;
    }
    rc = resolve_show_columns_table_name(
        database,
        (struct show_columns_target_nodes){
            .table = child_at(statement, 0U),
            .schema = schema_node,
        },
        &target
    );
    if (rc == MYLITE_OK) {
        rc = resolve_readable_base_table(database, &target, &table);
    }
    if (rc == MYLITE_OK) {
        rc = make_show_like_filter(database, like_node, &filter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < sizeof(result_columns) / sizeof(result_columns[0]);
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        context.database = database;
        context.result = result;
        context.filter = &filter;
        rc = mylite_catalog_for_each_column_in_table(
            database,
            table.table_id,
            append_show_column,
            &context
        );
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        } else if (
            rc != MYLITE_OK &&
            mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) == MYLITE_OK
        ) {
            set_runtime_error(database, "failed to build SHOW COLUMNS result");
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        show_like_filter_deinit(&filter);
        return rc;
    }

    show_like_filter_deinit(&filter);
    return finish_successful_result(database, result, out_result);
}

static int execute_show_index_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[show_index_result_column_count] = {
        "Table",
        "Non_unique",
        "Key_name",
        "Seq_in_index",
        "Column_name",
        "Collation",
        "Cardinality",
        "Sub_part",
        "Packed",
        "Null",
        "Index_type",
        "Comment",
        "Index_comment",
        "Visible",
        "Expression",
    };
    struct table_name_resolution target = {0};
    struct mylite_catalog_table_descriptor table = {0};
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    rc = resolve_show_columns_table_name(
        database,
        (struct show_columns_target_nodes){
            .table = child_at(statement, 0U),
            .schema = child_at(statement, 1U),
        },
        &target
    );
    if (rc == MYLITE_OK) {
        rc = resolve_readable_base_table(database, &target, &table);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < show_index_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_show_create_table_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_show_create_table plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_show_create_table(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = execute_show_create_table_from_plan(database, &plan, result);
    }
    planned_show_create_table_deinit(&plan);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_show_create_database_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[show_create_database_result_column_count] = {
        "Database",
        "Create Database",
    };
    struct mylite_catalog_schema_descriptor schema = {0};
    char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char *create_sql = NULL;
    const char *values[show_create_database_result_column_count] = {NULL, NULL};
    mylite_result *result = NULL;
    int rc =
        copy_identifier_text(child_at(statement, 0U), schema_name, sizeof(schema_name), database);

    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(schema_name)) {
        set_reserved_name_error(database, "database", schema_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = resolve_schema_name(database, schema_name, &schema);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_create_database_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = build_show_create_database_sql(schema.name, &create_sql);
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        values[0] = schema.name;
        values[1] = create_sql;
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    free(create_sql);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_show_engines_statement(struct mylite_db *database, mylite_result **out_result) {
    static const char *const result_columns[show_engines_result_column_count] = {
        "Engine",
        "Support",
        "Comment",
        "Transactions",
        "XA",
        "Savepoints",
    };
    static const char *const values[show_engines_result_column_count] = {
        "InnoDB",
        "DEFAULT",
        "Supports transactions, row-level locking, and foreign keys",
        "YES",
        "YES",
        "YES",
    };
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_engines_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_show_databases_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct show_like_filter filter = {
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
    struct show_databases_context context = {0};
    char *column_name = NULL;
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    rc = make_show_like_filter(database, child_at(statement, 0U), &filter);
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = build_show_databases_column_name(&filter, &column_name);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_column(result, column_name);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        context.result = result;
        context.filter = &filter;
        rc = mylite_catalog_for_each_schema(database, append_show_database, &context);
        if (rc != MYLITE_OK) {
            set_runtime_error(database, "failed to build SHOW DATABASES result");
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        free(column_name);
        show_like_filter_deinit(&filter);
        return rc;
    }

    free(column_name);
    show_like_filter_deinit(&filter);
    return finish_successful_result(database, result, out_result);
}

static int plan_show_create_table(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_show_create_table *out_plan
) {
    int rc = MYLITE_OK;

    *out_plan = (struct planned_show_create_table){0};
    rc = resolve_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = resolve_readable_base_table(database, &out_plan->target, &out_plan->table);
    }
    if (rc == MYLITE_OK) {
        rc = load_table_columns(
            database,
            out_plan->table.table_id,
            &out_plan->columns,
            &out_plan->column_count
        );
    }
    if (rc != MYLITE_OK) {
        planned_show_create_table_deinit(out_plan);
    }

    return rc;
}

static void planned_show_create_table_deinit(struct planned_show_create_table *plan) {
    if (plan == NULL) {
        return;
    }

    free(plan->columns);
    *plan = (struct planned_show_create_table){0};
}

static int execute_show_create_table_from_plan(
    struct mylite_db *database,
    const struct planned_show_create_table *plan,
    mylite_result *result
) {
    static const char *const result_columns[show_create_table_result_column_count] = {
        "Table",
        "Create Table",
    };
    char *create_sql = NULL;
    const char *values[show_create_table_result_column_count] = {NULL, NULL};
    int rc = MYLITE_OK;

    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_create_table_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = build_show_create_table_sql(database, plan, &create_sql);
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        values[0] = plan->table.name;
        values[1] = create_sql;
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }

    free(create_sql);
    return rc;
}

static int build_show_create_table_sql(
    struct mylite_db *database,
    const struct planned_show_create_table *plan,
    char **out_sql
) {
    struct dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    dynamic_string_init(&string);

    rc = dynamic_string_append(&string, "CREATE TABLE ");
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_mysql_quoted_identifier(&string, plan->table.name);
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append(&string, " (\n");
    }
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        rc = append_show_create_table_column_definition(
            database,
            &string,
            &plan->columns[column_index],
            column_index + 1U == plan->column_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append(
            &string,
            ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci"
        );
    }
    if (rc == MYLITE_OK) {
        *out_sql = dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    dynamic_string_deinit(&string);
    return rc;
}

static int append_show_create_table_column_definition(
    struct mylite_db *database,
    struct dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column,
    bool is_last_column
) {
    const char *type_text = NULL;
    int rc = show_create_table_type_text(database, column->logical_type, &type_text);

    if (rc == MYLITE_OK) {
        rc = dynamic_string_append(string, "  ");
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_mysql_quoted_identifier(string, column->name);
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_char(string, ' ');
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append(string, type_text);
    }
    if (rc == MYLITE_OK && !column->is_nullable) {
        rc = dynamic_string_append(string, " NOT NULL");
    }
    if (rc == MYLITE_OK) {
        rc = append_show_create_table_column_default(database, string, column);
    }
    if (rc == MYLITE_OK) {
        rc = append_show_create_table_column_suffix(string, column, is_last_column);
    }

    return rc;
}

static int append_show_create_table_column_default(
    struct mylite_db *database,
    struct dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column
) {
    char default_text[show_create_integer_default_text_capacity];
    int written = 0;

    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_NONE && column->is_nullable) {
        return dynamic_string_append(string, " DEFAULT NULL");
    }
    if (column->default_kind != MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER) {
        return MYLITE_OK;
    }

    written = snprintf(
        default_text,
        sizeof(default_text),
        " DEFAULT '%" PRId64 "'",
        column->default_integer
    );
    if (written < 0 || (size_t)written >= sizeof(default_text)) {
        set_runtime_error(database, "failed to format column default");
        return MYLITE_ERROR;
    }

    return dynamic_string_append(string, default_text);
}

static int append_show_create_table_column_suffix(
    struct dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column,
    bool is_last_column
) {
    int rc = MYLITE_OK;

    if (!column->is_visible) {
        rc = dynamic_string_append(string, " /*!80023 INVISIBLE */");
    }
    if (rc == MYLITE_OK && !is_last_column) {
        rc = dynamic_string_append_char(string, ',');
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_char(string, '\n');
    }

    return rc;
}

static int show_create_table_type_text(
    struct mylite_db *database,
    const char *logical_type,
    const char **out_type_text
) {
    if (logical_type == NULL || out_type_text == NULL) {
        set_runtime_error(database, "invalid column descriptor");
        return MYLITE_ERROR;
    }
    if (strcmp(logical_type, "INT") == 0 || strcmp(logical_type, "INTEGER") == 0) {
        *out_type_text = "int";
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "TINYINT") == 0) {
        *out_type_text = "tinyint";
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "TINYINT(1)") == 0) {
        *out_type_text = "tinyint(1)";
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "SMALLINT") == 0) {
        *out_type_text = "smallint";
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "MEDIUMINT") == 0) {
        *out_type_text = "mediumint";
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "INT UNSIGNED") == 0 ||
        strcmp(logical_type, "INTEGER UNSIGNED") == 0) {
        *out_type_text = "int unsigned";
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "TINYINT UNSIGNED") == 0) {
        *out_type_text = "tinyint unsigned";
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "SMALLINT UNSIGNED") == 0) {
        *out_type_text = "smallint unsigned";
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "MEDIUMINT UNSIGNED") == 0) {
        *out_type_text = "mediumint unsigned";
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "BIGINT") == 0) {
        *out_type_text = "bigint";
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "BIGINT UNSIGNED") == 0) {
        *out_type_text = "bigint unsigned";
        return MYLITE_OK;
    }

    set_unsupported_error(database, "SHOW CREATE TABLE supports only integer column descriptors");
    return MYLITE_ERROR;
}

static int build_show_create_database_sql(const char *schema_name, char **out_sql) {
    struct dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    dynamic_string_init(&string);

    rc = dynamic_string_append(&string, "CREATE DATABASE ");
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_mysql_quoted_identifier(&string, schema_name);
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append(
            &string,
            " /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci */ "
            "/*!80016 DEFAULT ENCRYPTION='N' */"
        );
    }
    if (rc == MYLITE_OK) {
        *out_sql = string.text;
        return MYLITE_OK;
    }

    dynamic_string_deinit(&string);
    return rc;
}

static int64_t row_count_for_completed_statement(
    const struct mylite_sql_ast_node *statement,
    const mylite_result *result
) {
    if (result != NULL && mylite_result_column_count(result) != 0U) {
        return -1;
    }
    if (statement == NULL) {
        return 0;
    }

    switch (statement->kind) {
    case MYLITE_SQL_AST_CREATE_SCHEMA_STATEMENT:
    case MYLITE_SQL_AST_INSERT_STATEMENT:
    case MYLITE_SQL_AST_INSERT_SET_STATEMENT:
    case MYLITE_SQL_AST_DELETE_STATEMENT:
    case MYLITE_SQL_AST_UPDATE_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_MODIFY_COLUMN_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_CHANGE_COLUMN_STATEMENT:
        return result == NULL ? 0 : mylite_result_affected_rows(result);
    case MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT:
        return -1;
    case MYLITE_SQL_AST_USE_STATEMENT:
    case MYLITE_SQL_AST_CREATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_DROP_TABLE_STATEMENT:
    case MYLITE_SQL_AST_TRUNCATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_RENAME_TABLE_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_RENAME_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_ADD_COLUMN_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_COLUMN_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_RENAME_COLUMN_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_SET_DEFAULT_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_DEFAULT_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_COLUMN_VISIBILITY_STATEMENT:
        return 0;
    case MYLITE_SQL_AST_SELECT_STATEMENT:
    case MYLITE_SQL_AST_SHOW_TABLES_STATEMENT:
    case MYLITE_SQL_AST_SHOW_TABLE_STATUS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_CHARACTER_SET_STATEMENT:
    case MYLITE_SQL_AST_SHOW_COLLATION_STATEMENT:
    case MYLITE_SQL_AST_SHOW_TRIGGERS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_EVENTS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_OPEN_TABLES_STATEMENT:
    case MYLITE_SQL_AST_SHOW_PROCEDURE_STATUS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_FUNCTION_STATUS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_PROCESSLIST_STATEMENT:
    case MYLITE_SQL_AST_SHOW_FULL_PROCESSLIST_STATEMENT:
    case MYLITE_SQL_AST_SHOW_WARNINGS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_COUNT_WARNINGS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_ERRORS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_COUNT_ERRORS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_COLUMNS_STATEMENT:
    case MYLITE_SQL_AST_SHOW_INDEX_STATEMENT:
    case MYLITE_SQL_AST_SHOW_CREATE_TABLE_STATEMENT:
    case MYLITE_SQL_AST_SHOW_CREATE_DATABASE_STATEMENT:
    case MYLITE_SQL_AST_SHOW_ENGINES_STATEMENT:
    case MYLITE_SQL_AST_SHOW_DATABASES_STATEMENT:
        return -1;
    case MYLITE_SQL_AST_SCRIPT:
    case MYLITE_SQL_AST_SELECT_LIST:
    case MYLITE_SQL_AST_SELECT_ITEM:
    case MYLITE_SQL_AST_FROM_DUAL:
    case MYLITE_SQL_AST_IDENTIFIER:
    case MYLITE_SQL_AST_QUALIFIED_IDENTIFIER:
    case MYLITE_SQL_AST_WILDCARD:
    case MYLITE_SQL_AST_LITERAL:
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION:
    case MYLITE_SQL_AST_COLUMN_DEFINITION_LIST:
    case MYLITE_SQL_AST_COLUMN_DEFINITION:
    case MYLITE_SQL_AST_INTEGER_TYPE:
    case MYLITE_SQL_AST_NULLABILITY:
    case MYLITE_SQL_AST_COLUMN_DEFAULT_NULL:
    case MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE:
    case MYLITE_SQL_AST_IDENTIFIER_LIST:
    case MYLITE_SQL_AST_INSERT_ROW_LIST:
    case MYLITE_SQL_AST_INSERT_ROW:
    case MYLITE_SQL_AST_INSERT_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_INSERT_ASSIGNMENT:
    case MYLITE_SQL_AST_FROM_TABLE:
    case MYLITE_SQL_AST_WHERE_CLAUSE:
    case MYLITE_SQL_AST_COMPARISON_PREDICATE:
    case MYLITE_SQL_AST_IS_NULL_PREDICATE:
    case MYLITE_SQL_AST_ORDER_BY_CLAUSE:
    case MYLITE_SQL_AST_ORDER_DIRECTION:
    case MYLITE_SQL_AST_LIMIT_CLAUSE:
    case MYLITE_SQL_AST_UPDATE_ASSIGNMENT_LIST:
    case MYLITE_SQL_AST_UPDATE_ASSIGNMENT:
    case MYLITE_SQL_AST_TABLE_ENGINE_OPTION:
    case MYLITE_SQL_AST_TABLE_OPTION_LIST:
    case MYLITE_SQL_AST_TABLE_CHARSET_OPTION:
    case MYLITE_SQL_AST_TABLE_COLLATION_OPTION:
    case MYLITE_SQL_AST_CREATE_IF_NOT_EXISTS_CLAUSE:
    case MYLITE_SQL_AST_DROP_IF_EXISTS_CLAUSE:
    case MYLITE_SQL_AST_CREATE_SCHEMA_IF_NOT_EXISTS_CLAUSE:
    case MYLITE_SQL_AST_DROP_SCHEMA_IF_EXISTS_CLAUSE:
    case MYLITE_SQL_AST_TABLE_NAME_LIST:
    case MYLITE_SQL_AST_RENAME_TABLE_PAIR:
    case MYLITE_SQL_AST_RENAME_TABLE_PAIR_LIST:
    case MYLITE_SQL_AST_DATABASE_FUNCTION:
    case MYLITE_SQL_AST_SCHEMA_FUNCTION:
    case MYLITE_SQL_AST_USER_FUNCTION:
    case MYLITE_SQL_AST_SESSION_USER_FUNCTION:
    case MYLITE_SQL_AST_SYSTEM_USER_FUNCTION:
    case MYLITE_SQL_AST_CURRENT_USER_FUNCTION:
    case MYLITE_SQL_AST_CURRENT_ROLE_FUNCTION:
    case MYLITE_SQL_AST_CURRENT_ROLE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_CONNECTION_ID_FUNCTION:
    case MYLITE_SQL_AST_CONNECTION_ID_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_VERSION_FUNCTION:
    case MYLITE_SQL_AST_VERSION_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_FUNCTION_ARGUMENT_LIST:
    case MYLITE_SQL_AST_ROW_COUNT_FUNCTION:
    case MYLITE_SQL_AST_LAST_INSERT_ID_FUNCTION:
    case MYLITE_SQL_AST_COUNT_STAR_FUNCTION:
    case MYLITE_SQL_AST_COUNT_COLUMN_FUNCTION:
    case MYLITE_SQL_AST_COUNT_LITERAL_FUNCTION:
    case MYLITE_SQL_AST_COUNT_DISTINCT_COLUMN_FUNCTION:
    case MYLITE_SQL_AST_MIN_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_MAX_AGGREGATE_FUNCTION:
    case MYLITE_SQL_AST_SYSTEM_VARIABLE:
        break;
    }

    return 0;
}

static bool statement_preserves_diagnostics_snapshot(const struct mylite_sql_ast_node *statement) {
    if (statement == NULL) {
        return false;
    }
    if (statement->kind == MYLITE_SQL_AST_SHOW_WARNINGS_STATEMENT) {
        return true;
    }
    if (statement->kind == MYLITE_SQL_AST_SHOW_COUNT_WARNINGS_STATEMENT) {
        return true;
    }
    if (statement->kind == MYLITE_SQL_AST_SHOW_ERRORS_STATEMENT) {
        return true;
    }
    if (statement->kind == MYLITE_SQL_AST_SHOW_COUNT_ERRORS_STATEMENT) {
        return true;
    }
    return false;
}

static int snapshot_current_diagnostics(struct mylite_db *database) {
    int rc = MYLITE_OK;

    if (database == NULL) {
        return MYLITE_MISUSE;
    }

    rc = mylite_diagnostics_replace(&database->previous_diagnostics, &database->diagnostics);
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }
    return rc;
}

static int finish_successful_result(
    struct mylite_db *database,
    mylite_result *result,
    mylite_result **out_result
) {
    mylite_result_set_warning_count(
        result,
        mylite_diagnostics_warning_count(mylite_connection_diagnostics(database))
    );
    *out_result = result;

    return MYLITE_OK;
}

static int finish_successful_result_with_warning_count(
    mylite_result *result,
    size_t warning_count,
    mylite_result **out_result
) {
    mylite_result_set_warning_count(result, warning_count);
    *out_result = result;

    return MYLITE_OK;
}

static int plan_create_table(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_create_table *out_plan
) {
    const struct mylite_sql_ast_node *column_list = child_at(statement, 1U);
    size_t column_count = mylite_sql_ast_node_child_count(column_list);
    int rc = MYLITE_OK;

    *out_plan = (struct planned_create_table){0};
    rc = resolve_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        return MYLITE_ERROR;
    }
    rc = validate_create_table_options(database, create_table_options_node(statement));
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (column_count == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (column_count > SIZE_MAX / sizeof(*out_plan->columns)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    out_plan->columns = calloc(column_count, sizeof(*out_plan->columns));
    if (out_plan->columns == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_plan->column_count = column_count;

    rc = plan_columns(database, column_list, out_plan->columns, out_plan->column_count);
    if (rc != MYLITE_OK) {
        planned_create_table_deinit(out_plan);
        return rc;
    }

    return MYLITE_OK;
}

static int validate_create_table_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_options
) {
    const struct mylite_sql_ast_node *table_option = NULL;

    if (table_options == NULL) {
        return MYLITE_OK;
    }
    if (table_options->kind != MYLITE_SQL_AST_TABLE_OPTION_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    table_option = child_at(table_options, 0U);
    while (table_option != NULL) {
        int rc = validate_create_table_option(database, table_option);

        if (rc != MYLITE_OK) {
            return rc;
        }
        table_option = table_option->next_sibling;
    }

    return MYLITE_OK;
}

static int validate_create_table_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_option
) {
    if (table_option == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    if (table_option->kind == MYLITE_SQL_AST_TABLE_ENGINE_OPTION) {
        return validate_create_table_engine_option(database, table_option);
    }
    if (table_option->kind == MYLITE_SQL_AST_TABLE_CHARSET_OPTION) {
        return validate_create_table_charset_option(database, table_option);
    }
    if (table_option->kind == MYLITE_SQL_AST_TABLE_COLLATION_OPTION) {
        return validate_create_table_collation_option(database, table_option);
    }

    set_parse_error(database, NULL);
    return MYLITE_ERROR;
}

static int validate_create_table_engine_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *engine_option
) {
    char engine_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = MYLITE_OK;

    if (engine_option == NULL) {
        return MYLITE_OK;
    }
    if (engine_option->kind != MYLITE_SQL_AST_TABLE_ENGINE_OPTION) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    rc = copy_table_option_name_text(
        database,
        child_at(engine_option, 0U),
        engine_name,
        sizeof(engine_name),
        (struct table_option_name_policy){
            .identifier_kind = "storage engine",
            .nul_message = "table engine names do not support NUL bytes",
        }
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!text_equals_ascii_case_insensitive(engine_name, "InnoDB")) {
        set_unknown_storage_engine_error(database, engine_name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_create_table_charset_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *charset_option
) {
    char charset_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = MYLITE_OK;

    if (charset_option == NULL || charset_option->kind != MYLITE_SQL_AST_TABLE_CHARSET_OPTION) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    rc = copy_table_option_name_text(
        database,
        child_at(charset_option, 0U),
        charset_name,
        sizeof(charset_name),
        (struct table_option_name_policy){
            .identifier_kind = "character set",
            .nul_message = "table character set names do not support NUL bytes",
        }
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!text_equals_ascii_case_insensitive(charset_name, "utf8mb4")) {
        set_unknown_character_set_error(database, charset_name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_create_table_collation_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *collation_option
) {
    char collation_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = MYLITE_OK;

    if (collation_option == NULL ||
        collation_option->kind != MYLITE_SQL_AST_TABLE_COLLATION_OPTION) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    rc = copy_table_option_name_text(
        database,
        child_at(collation_option, 0U),
        collation_name,
        sizeof(collation_name),
        (struct table_option_name_policy){
            .identifier_kind = "collation",
            .nul_message = "table collation names do not support NUL bytes",
        }
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!text_equals_ascii_case_insensitive(collation_name, "utf8mb4_0900_ai_ci")) {
        set_unknown_collation_error(database, collation_name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int copy_table_option_name_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *option_name_node,
    char *destination,
    size_t destination_size,
    struct table_option_name_policy policy
) {
    char *decoded = NULL;
    size_t decoded_length = 0U;
    int rc = MYLITE_OK;

    if (option_name_node == NULL || destination == NULL || destination_size == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (option_name_node->span.text == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    for (size_t index = 0U; index < option_name_node->span.length; ++index) {
        if (option_name_node->span.text[index] == '\0') {
            set_unsupported_error(database, policy.nul_message);
            return MYLITE_ERROR;
        }
    }
    if (option_name_node->kind == MYLITE_SQL_AST_IDENTIFIER) {
        return copy_identifier_text(option_name_node, destination, destination_size, database);
    }
    if (option_name_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(option_name_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    rc = decode_table_option_string_literal(database, option_name_node, &decoded, policy);
    if (rc != MYLITE_OK) {
        return rc;
    }

    decoded_length = strlen(decoded);
    if (decoded_length >= destination_size) {
        free(decoded);
        set_identifier_too_long_error(database, policy.identifier_kind);
        return MYLITE_ERROR;
    }

    memcpy(destination, decoded, decoded_length + 1U);
    free(decoded);
    return MYLITE_OK;
}

static int decode_table_option_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *option_name_node,
    char **out_name,
    struct table_option_name_policy policy
) {
    struct dynamic_string string;
    const char *text = NULL;
    size_t length = 0U;
    char quote = '\0';
    int rc = MYLITE_OK;

    if (out_name == NULL) {
        set_runtime_error(database, "invalid table option name");
        return MYLITE_ERROR;
    }
    *out_name = NULL;
    if (option_name_node == NULL || option_name_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(option_name_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    text = option_name_node->span.text;
    length = option_name_node->span.length;
    if (text == NULL || length < 2U) {
        set_runtime_error(database, "invalid table option name");
        return MYLITE_ERROR;
    }

    quote = text[0];
    dynamic_string_init(&string);
    for (size_t index = 1U; rc == MYLITE_OK && index + 1U < length; ++index) {
        char byte = text[index];

        if (byte == quote && index + 2U < length && text[index + 1U] == quote) {
            rc = dynamic_string_append_char(&string, quote);
            ++index;
        } else if (byte == '\\' && index + 2U < length) {
            ++index;
            rc = append_decoded_table_option_name_escape(database, &string, text[index], policy);
        } else {
            rc = dynamic_string_append_char(&string, byte);
        }
    }
    if (rc == MYLITE_OK && string.text == NULL) {
        rc = dynamic_string_append(&string, "");
    }
    if (rc == MYLITE_OK) {
        *out_name = dynamic_string_take(&string);
        if (*out_name == NULL) {
            rc = MYLITE_NOMEM;
        }
    }
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    dynamic_string_deinit(&string);
    return rc;
}

static int append_decoded_table_option_name_escape(
    struct mylite_db *database,
    struct dynamic_string *string,
    char escaped_byte,
    struct table_option_name_policy policy
) {
    switch (escaped_byte) {
    case '0':
        set_unsupported_error(database, policy.nul_message);
        return MYLITE_ERROR;
    case 'n':
        return dynamic_string_append_char(string, '\n');
    case 'r':
        return dynamic_string_append_char(string, '\r');
    case 't':
        return dynamic_string_append_char(string, '\t');
    case 'b':
        return dynamic_string_append_char(string, '\b');
    case 'Z':
        return dynamic_string_append_char(string, '\032');
    case '\\':
        return dynamic_string_append_char(string, '\\');
    case '\'':
        return dynamic_string_append_char(string, '\'');
    case '"':
        return dynamic_string_append_char(string, '"');
    default:
        return dynamic_string_append_char(string, escaped_byte);
    }
}

static void planned_create_table_deinit(struct planned_create_table *plan) {
    if (plan == NULL) {
        return;
    }

    free(plan->columns);
    *plan = (struct planned_create_table){0};
}

static int create_table_from_plan(
    struct mylite_db *database,
    const struct planned_create_table *plan
) {
    struct mylite_catalog_table_descriptor existing_table = {0};
    struct mylite_catalog_table_descriptor table = {0};
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    char physical_name[MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY];
    bool existing_table_found = false;
    int64_t table_id = 0;
    int rc = mylite_catalog_try_read_table_by_name(
        database,
        plan->target.schema.schema_id,
        plan->target.table_name,
        &existing_table,
        &existing_table_found
    );

    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to read table descriptor");
        return rc;
    }
    if (existing_table_found) {
        set_table_exists_error(database, plan->target.table_name);
        return MYLITE_ERROR;
    }

    rc = mylite_catalog_begin_mutation(database, &mutation);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_allocate_table_id_in_mutation(database, &mutation, &table_id);
    }
    if (rc == MYLITE_OK) {
        rc = build_physical_table_name(table_id, physical_name, sizeof(physical_name));
    }
    if (rc == MYLITE_OK) {
        rc = insert_create_table_catalog_rows(
            database,
            plan,
            &mutation,
            table_id,
            physical_name,
            &table
        );
    }
    if (rc == MYLITE_OK) {
        rc = execute_physical_create_table(database, plan, table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        mylite_catalog_rollback_mutation(database, &mutation);
        return rc;
    }

    ++database->session.sqlite_schema_generation;

    return MYLITE_OK;
}

static int insert_create_table_catalog_rows(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const struct mylite_catalog_mutation *mutation,
    int64_t table_id,
    const char *physical_name,
    struct mylite_catalog_table_descriptor *out_table
) {
    int rc = mylite_catalog_insert_table_in_mutation(
        database,
        mutation,
        table_id,
        plan->target.schema.schema_id,
        plan->target.table_name,
        physical_name,
        MYLITE_CATALOG_TABLE_KIND_BASE,
        out_table
    );

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        const struct planned_column *column = &plan->columns[column_index];

        rc = mylite_catalog_insert_column_in_mutation(
            database,
            mutation,
            table_id,
            (int64_t)column_index + 1,
            column->name,
            column->logical_type,
            column->physical_type,
            column->is_nullable,
            column->default_kind,
            column->default_integer,
            NULL
        );
    }

    return rc;
}

static int execute_physical_create_table(
    struct mylite_db *database,
    const struct planned_create_table *plan,
    const char *physical_name
) {
    char *sql = NULL;
    int rc = build_create_table_sql(plan, physical_name, &sql);

    if (rc == MYLITE_OK) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);

    return rc;
}

static int execute_physical_drop_table(struct mylite_db *database, const char *physical_name) {
    char *sql = NULL;
    int rc = build_drop_table_sql(physical_name, &sql);

    if (rc == MYLITE_OK) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);

    return rc;
}

static int create_schema_from_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result *result
) {
    char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    struct mylite_catalog_schema_descriptor existing_schema = {0};
    bool existing_schema_found = false;
    int rc =
        copy_identifier_text(child_at(statement, 0U), schema_name, sizeof(schema_name), database);

    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(schema_name)) {
        set_reserved_name_error(database, "database", schema_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_try_read_schema_by_name(
            database,
            schema_name,
            &existing_schema,
            &existing_schema_found
        );
        if (rc != MYLITE_OK) {
            set_internal_error_if_clear(database, rc, "failed to read schema descriptor");
        } else if (existing_schema_found) {
            set_database_exists_error(database, schema_name);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_create_schema(database, schema_name, NULL);
    }
    if (rc == MYLITE_OK) {
        mylite_result_set_affected_rows(result, 1);
    }

    return rc;
}

static int plan_drop_schema(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct mylite_catalog_mutation *mutation,
    struct planned_drop_schema *out_plan
) {
    char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    struct collect_drop_schema_tables_context context = {0};
    bool schema_found = false;
    int rc =
        copy_identifier_text(child_at(statement, 0U), schema_name, sizeof(schema_name), database);

    *out_plan = (struct planned_drop_schema){0};
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(schema_name)) {
        set_reserved_name_error(database, "database", schema_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_begin_mutation(database, mutation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_try_read_schema_by_name(
            database,
            schema_name,
            &out_plan->schema,
            &schema_found
        );
        if (rc != MYLITE_OK) {
            set_internal_error_if_clear(database, rc, "failed to read schema descriptor");
        } else if (!schema_found) {
            set_cant_drop_database_error(database, schema_name);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        context.database = database;
        context.plan = out_plan;
        rc = mylite_catalog_for_each_table_in_schema(
            database,
            out_plan->schema.schema_id,
            collect_drop_schema_table,
            &context
        );
        if (rc != MYLITE_OK) {
            planned_drop_schema_deinit(out_plan);
            if (mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) == 0) {
                set_runtime_error(database, "failed to plan DROP DATABASE");
            }
        }
    }

    return rc;
}

static void planned_drop_schema_deinit(struct planned_drop_schema *plan) {
    if (plan == NULL) {
        return;
    }

    free(plan->tables);
    *plan = (struct planned_drop_schema){0};
}

static int collect_drop_schema_table(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
) {
    struct collect_drop_schema_tables_context *context = user_data;
    struct planned_drop_schema *plan = NULL;
    int rc = MYLITE_OK;

    if (table == NULL || context == NULL || context->database == NULL || context->plan == NULL) {
        return MYLITE_MISUSE;
    }
    if (table->kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(
            context->database,
            "DROP DATABASE supports only persistent base tables"
        );
        return MYLITE_ERROR;
    }

    plan = context->plan;
    rc = reserve_drop_schema_tables(plan, plan->table_count + 1U);
    if (rc != MYLITE_OK) {
        set_nomem_error(context->database);
        return rc;
    }

    memcpy(
        plan->tables[plan->table_count].physical_name,
        table->physical_name,
        sizeof(plan->tables[plan->table_count].physical_name)
    );
    ++plan->table_count;

    return MYLITE_OK;
}

static int reserve_drop_schema_tables(struct planned_drop_schema *plan, size_t required_capacity) {
    enum { initial_table_capacity = 4 };

    struct planned_drop_schema_table *tables = NULL;
    size_t capacity = 0U;

    if (plan == NULL) {
        return MYLITE_MISUSE;
    }
    if (required_capacity <= plan->table_capacity) {
        return MYLITE_OK;
    }

    capacity = plan->table_capacity == 0U ? initial_table_capacity : plan->table_capacity;
    while (capacity < required_capacity) {
        if (capacity > SIZE_MAX / 2U) {
            return MYLITE_NOMEM;
        }
        capacity *= 2U;
    }
    if (capacity > SIZE_MAX / sizeof(*tables)) {
        return MYLITE_NOMEM;
    }

    tables = (struct planned_drop_schema_table *)realloc(plan->tables, capacity * sizeof(*tables));
    if (tables == NULL) {
        return MYLITE_NOMEM;
    }

    plan->tables = tables;
    plan->table_capacity = capacity;

    return MYLITE_OK;
}

static int drop_schema_from_plan(
    struct mylite_db *database,
    struct mylite_catalog_mutation *mutation,
    const struct planned_drop_schema *plan,
    mylite_result *result
) {
    int rc = mylite_catalog_delete_schema_in_mutation(database, mutation, plan->schema.schema_id);

    for (size_t table_index = 0U; rc == MYLITE_OK && table_index < plan->table_count;
         ++table_index) {
        rc = execute_physical_drop_table(database, plan->tables[table_index].physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, mutation);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (plan->table_count > 0U) {
        ++database->session.sqlite_schema_generation;
    }
    if (database->session.has_selected_schema &&
        strcmp(database->session.selected_schema, plan->schema.name) == 0) {
        database->session.has_selected_schema = false;
        database->session.selected_schema[0] = '\0';
    }
    mylite_result_set_affected_rows(result, (int64_t)plan->table_count);

    return MYLITE_OK;
}

static int plan_truncate_table(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_truncate_table *out_plan
) {
    int rc = MYLITE_OK;

    *out_plan = (struct planned_truncate_table){0};
    rc = resolve_truncate_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_read_table_by_name(
            database,
            out_plan->target.schema.schema_id,
            out_plan->target.table_name,
            &out_plan->table
        );
        if (rc != MYLITE_OK) {
            set_table_does_not_exist_error(
                database,
                out_plan->target.schema.name,
                out_plan->target.table_name
            );
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && out_plan->table.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(database, "TRUNCATE TABLE supports only persistent base tables");
        rc = MYLITE_ERROR;
    }

    return rc;
}

static int execute_truncate_from_plan(
    struct mylite_db *database,
    const struct planned_truncate_table *plan,
    mylite_result *result
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    bool transaction_started = false;
    int sqlite_rc = SQLITE_OK;
    int rc = build_truncate_table_sql(plan, &sql);

    (void)result;
    if (rc == MYLITE_OK) {
        rc = execute_sqlite_control_sql(database, "BEGIN IMMEDIATE");
    }
    if (rc == MYLITE_OK) {
        transaction_started = true;
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc != SQLITE_DONE) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finalize_sqlite_statement(statement, rc);
    statement = NULL;
    if (rc == MYLITE_OK) {
        rc = execute_sqlite_control_sql(database, "COMMIT");
        if (rc == MYLITE_OK) {
            transaction_started = false;
        }
    }
    if (rc != MYLITE_OK && transaction_started) {
        (void)execute_sqlite_control_sql(database, "ROLLBACK");
    }
    free(sql);

    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
            return rc;
        }
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int plan_rename_table_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_rename_table_statement *out_plan
) {
    const struct mylite_sql_ast_node *pair_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *pair_node = NULL;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_rename_table_statement){0};
    if (pair_list == NULL || pair_list->kind != MYLITE_SQL_AST_RENAME_TABLE_PAIR_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    out_plan->pair_count = mylite_sql_ast_node_child_count(pair_list);
    if (out_plan->pair_count == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (out_plan->pair_count > SIZE_MAX / sizeof(*out_plan->pairs)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    out_plan->pairs = calloc(out_plan->pair_count, sizeof(*out_plan->pairs));
    if (out_plan->pairs == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    pair_node = child_at(pair_list, 0U);
    for (size_t pair_index = 0U; rc == MYLITE_OK && pair_index < out_plan->pair_count;
         ++pair_index) {
        rc = plan_rename_table_pair(database, pair_node, &out_plan->pairs[pair_index]);
        if (pair_node != NULL) {
            pair_node = pair_node->next_sibling;
        }
    }
    if (rc != MYLITE_OK) {
        planned_rename_table_statement_deinit(out_plan);
    }

    return rc;
}

static int plan_rename_table_pair(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *pair_node,
    struct planned_rename_table *out_pair
) {
    int rc = MYLITE_OK;

    *out_pair = (struct planned_rename_table){0};
    if (pair_node == NULL || pair_node->kind != MYLITE_SQL_AST_RENAME_TABLE_PAIR) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    rc = resolve_table_name(database, child_at(pair_node, 0U), &out_pair->source);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_pair->source.table_name)) {
        set_reserved_name_error(database, "table", out_pair->source.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = resolve_table_name(database, child_at(pair_node, 1U), &out_pair->target);
    }
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_pair->target.table_name)) {
        set_reserved_name_error(database, "table", out_pair->target.table_name);
        rc = MYLITE_ERROR;
    }

    return rc;
}

static void planned_rename_table_statement_deinit(struct planned_rename_table_statement *plan) {
    if (plan == NULL) {
        return;
    }

    free(plan->pairs);
    *plan = (struct planned_rename_table_statement){0};
}

static int rename_table_statement_from_plan(
    struct mylite_db *database,
    const struct planned_rename_table_statement *plan
) {
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    int rc = mylite_catalog_begin_mutation(database, &mutation);

    for (size_t pair_index = 0U; rc == MYLITE_OK && pair_index < plan->pair_count; ++pair_index) {
        rc = rename_table_pair_in_mutation(
            database,
            &mutation,
            &plan->pairs[pair_index],
            false,
            "RENAME TABLE supports only persistent base tables"
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to rename table descriptor");
        mylite_catalog_rollback_mutation(database, &mutation);
        return rc;
    }

    return MYLITE_OK;
}

static int plan_alter_table_rename(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_rename_table *out_plan
) {
    int rc = MYLITE_OK;

    *out_plan = (struct planned_rename_table){0};
    rc = require_selected_schema_for_unqualified_table_name(database, child_at(statement, 1U));
    if (rc == MYLITE_OK) {
        rc = resolve_table_name(database, child_at(statement, 0U), &out_plan->source);
    }
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->source.table_name)) {
        set_reserved_name_error(database, "table", out_plan->source.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = resolve_table_name(database, child_at(statement, 1U), &out_plan->target);
    }
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }

    return rc;
}

static int alter_table_rename_from_plan(
    struct mylite_db *database,
    const struct planned_rename_table *plan
) {
    return rename_table_from_plan_with_policy(
        database,
        plan,
        true,
        "ALTER TABLE RENAME supports only persistent base tables"
    );
}

static int plan_alter_table_add_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_add_column *out_plan
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    size_t column_count = 0U;
    size_t duplicate_index = 0U;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_add_column){0};
    rc = resolve_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = plan_column(database, child_at(statement, 1U), &out_plan->column);
    }
    if (rc == MYLITE_OK) {
        rc = finalize_planned_column_default(database, &out_plan->column);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_read_table_by_name(
            database,
            out_plan->target.schema.schema_id,
            out_plan->target.table_name,
            &out_plan->table
        );
        if (rc != MYLITE_OK) {
            set_table_does_not_exist_error(
                database,
                out_plan->target.schema.name,
                out_plan->target.table_name
            );
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && out_plan->table.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(
            database,
            "ALTER TABLE ADD COLUMN supports only persistent base tables"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = load_table_columns(database, out_plan->table.table_id, &columns, &column_count);
    }
    if (rc == MYLITE_OK &&
        find_column_index(columns, column_count, out_plan->column.name, &duplicate_index) ==
            MYLITE_OK) {
        set_duplicate_column_error(database, out_plan->column.name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && column_count >= (size_t)INT64_MAX) {
        set_runtime_error(database, "too many columns in table descriptor");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        out_plan->ordinal_position = (int64_t)column_count + 1;
    }

    free(columns);

    return rc;
}

static int alter_table_add_column_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_add_column *plan
) {
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    int rc = mylite_catalog_begin_mutation(database, &mutation);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_insert_column_in_mutation(
            database,
            &mutation,
            plan->table.table_id,
            plan->ordinal_position,
            plan->column.name,
            plan->column.logical_type,
            plan->column.physical_type,
            plan->column.is_nullable,
            plan->column.default_kind,
            plan->column.default_integer,
            NULL
        );
    }
    if (rc == MYLITE_OK) {
        rc = execute_physical_alter_table_add_column(database, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_update_table_identity_in_mutation(
            database,
            &mutation,
            plan->table.table_id,
            plan->table.schema_id,
            plan->table.name,
            NULL
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to add table column");
        mylite_catalog_rollback_mutation(database, &mutation);
        return rc;
    }

    ++database->session.sqlite_schema_generation;

    return MYLITE_OK;
}

static int execute_physical_alter_table_add_column(
    struct mylite_db *database,
    const struct planned_alter_table_add_column *plan
) {
    char *sql = NULL;
    int rc = build_alter_table_add_column_sql(plan, &sql);

    if (rc == MYLITE_OK) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);

    return rc;
}

static int plan_alter_table_drop_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_drop_column *out_plan
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t column_index = 0U;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_drop_column){0};
    rc = resolve_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = copy_identifier_text(
            child_at(statement, 1U),
            column_name,
            sizeof(column_name),
            database
        );
    }
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(column_name)) {
        set_reserved_name_error(database, "column", column_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_read_table_by_name(
            database,
            out_plan->target.schema.schema_id,
            out_plan->target.table_name,
            &out_plan->table
        );
        if (rc != MYLITE_OK) {
            set_table_does_not_exist_error(
                database,
                out_plan->target.schema.name,
                out_plan->target.table_name
            );
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && out_plan->table.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(
            database,
            "ALTER TABLE DROP COLUMN supports only persistent base tables"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = load_table_columns(
            database,
            out_plan->table.table_id,
            &columns,
            &out_plan->column_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = find_column_index(columns, out_plan->column_count, column_name, &column_index);
        if (rc != MYLITE_OK) {
            set_cant_drop_field_or_key_error(database, column_name);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && out_plan->column_count <= 1U) {
        set_cant_remove_all_fields_error(database);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && columns[column_index].is_visible &&
        count_visible_columns(columns, out_plan->column_count) <= 1U) {
        set_must_have_visible_column_error(database);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        out_plan->column = columns[column_index];
    }

    free(columns);

    return rc;
}

static int alter_table_drop_column_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_drop_column *plan
) {
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    int rc = mylite_catalog_begin_mutation(database, &mutation);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_delete_column_in_mutation(
            database,
            &mutation,
            plan->table.table_id,
            plan->column.column_id,
            plan->column.ordinal_position
        );
    }
    if (rc == MYLITE_OK) {
        rc = execute_physical_alter_table_drop_column(database, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_update_table_identity_in_mutation(
            database,
            &mutation,
            plan->table.table_id,
            plan->table.schema_id,
            plan->table.name,
            NULL
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to drop table column");
        mylite_catalog_rollback_mutation(database, &mutation);
        return rc;
    }

    ++database->session.sqlite_schema_generation;

    return MYLITE_OK;
}

static int execute_physical_alter_table_drop_column(
    struct mylite_db *database,
    const struct planned_alter_table_drop_column *plan
) {
    char *sql = NULL;
    int rc = build_alter_table_drop_column_sql(plan, &sql);

    if (rc == MYLITE_OK) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);

    return rc;
}

static int plan_alter_table_rename_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_rename_column *out_plan
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    char old_column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t old_column_index = 0U;
    size_t new_column_index = 0U;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_rename_column){0};
    rc = resolve_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = copy_identifier_text(
            child_at(statement, 1U),
            old_column_name,
            sizeof(old_column_name),
            database
        );
    }
    if (rc == MYLITE_OK) {
        rc = copy_identifier_text(
            child_at(statement, 2U),
            out_plan->new_column_name,
            sizeof(out_plan->new_column_name),
            database
        );
    }
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(old_column_name)) {
        set_reserved_name_error(database, "column", old_column_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->new_column_name)) {
        set_reserved_name_error(database, "column", out_plan->new_column_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_read_table_by_name(
            database,
            out_plan->target.schema.schema_id,
            out_plan->target.table_name,
            &out_plan->table
        );
        if (rc != MYLITE_OK) {
            set_table_does_not_exist_error(
                database,
                out_plan->target.schema.name,
                out_plan->target.table_name
            );
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && out_plan->table.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(
            database,
            "ALTER TABLE RENAME COLUMN supports only persistent base tables"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        size_t column_count = 0U;

        rc = load_table_columns(database, out_plan->table.table_id, &columns, &column_count);
        if (rc == MYLITE_OK) {
            rc = find_column_index(columns, column_count, old_column_name, &old_column_index);
            if (rc != MYLITE_OK) {
                set_unknown_column_in_table_error(database, old_column_name, out_plan->table.name);
                rc = MYLITE_ERROR;
            }
        }
        if (rc == MYLITE_OK) {
            out_plan->column = columns[old_column_index];
            out_plan->is_noop = strcmp(out_plan->column.name, out_plan->new_column_name) == 0;
        }
        if (rc == MYLITE_OK && !out_plan->is_noop &&
            find_column_index(
                columns,
                column_count,
                out_plan->new_column_name,
                &new_column_index
            ) == MYLITE_OK &&
            new_column_index != old_column_index) {
            set_duplicate_column_error(database, columns[new_column_index].name);
            rc = MYLITE_ERROR;
        }
    }

    free(columns);

    return rc;
}

static int alter_table_rename_column_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_rename_column *plan
) {
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    int rc = MYLITE_OK;

    if (plan->is_noop) {
        return MYLITE_OK;
    }

    rc = mylite_catalog_begin_mutation(database, &mutation);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_rename_column_in_mutation(
            database,
            &mutation,
            plan->table.table_id,
            plan->column.column_id,
            plan->new_column_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = execute_physical_alter_table_rename_column(database, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_update_table_identity_in_mutation(
            database,
            &mutation,
            plan->table.table_id,
            plan->table.schema_id,
            plan->table.name,
            NULL
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to rename table column");
        mylite_catalog_rollback_mutation(database, &mutation);
        return rc;
    }

    ++database->session.sqlite_schema_generation;

    return MYLITE_OK;
}

static int execute_physical_alter_table_rename_column(
    struct mylite_db *database,
    const struct planned_alter_table_rename_column *plan
) {
    char *sql = NULL;
    int rc = build_alter_table_rename_column_sql(plan, &sql);

    if (rc == MYLITE_OK) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);

    return rc;
}

static int plan_alter_table_modify_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_modify_column *out_plan
) {
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_modify_column){0};
    out_plan->unsupported_object_message =
        "ALTER TABLE MODIFY COLUMN supports only persistent base tables";
    out_plan->rowid_alias_message =
        "ALTER TABLE MODIFY COLUMN requires an unshadowed SQLite rowid alias";
    out_plan->integer_support_message =
        "ALTER TABLE MODIFY COLUMN supports only baseline integer columns";
    out_plan->row_count_overflow_message =
        "too many rows to validate for ALTER TABLE MODIFY COLUMN";
    out_plan->failure_message = "failed to modify table column";

    rc = resolve_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = plan_column(database, child_at(statement, 1U), &out_plan->column);
    }
    if (rc == MYLITE_OK) {
        rc = finalize_planned_column_default(database, &out_plan->column);
    }
    if (rc == MYLITE_OK) {
        memcpy(
            out_plan->lookup_column_name,
            out_plan->column.name,
            sizeof(out_plan->lookup_column_name)
        );
        rc = resolve_alter_table_column_replacement_plan(database, out_plan);
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_modify_column_deinit(out_plan);
    }

    return rc;
}

static int plan_alter_table_change_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_modify_column *out_plan
) {
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_modify_column){0};
    out_plan->checks_duplicate_replacement = true;
    out_plan->unsupported_object_message =
        "ALTER TABLE CHANGE COLUMN supports only persistent base tables";
    out_plan->rowid_alias_message =
        "ALTER TABLE CHANGE COLUMN requires an unshadowed SQLite rowid alias";
    out_plan->integer_support_message =
        "ALTER TABLE CHANGE COLUMN supports only baseline integer columns";
    out_plan->row_count_overflow_message =
        "too many rows to validate for ALTER TABLE CHANGE COLUMN";
    out_plan->failure_message = "failed to change table column";

    rc = resolve_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = copy_identifier_text(
            child_at(statement, 1U),
            out_plan->lookup_column_name,
            sizeof(out_plan->lookup_column_name),
            database
        );
    }
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->lookup_column_name)) {
        set_reserved_name_error(database, "column", out_plan->lookup_column_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = plan_column(database, child_at(statement, 2U), &out_plan->column);
    }
    if (rc == MYLITE_OK) {
        rc = finalize_planned_column_default(database, &out_plan->column);
    }
    if (rc == MYLITE_OK) {
        rc = resolve_alter_table_column_replacement_plan(database, out_plan);
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_modify_column_deinit(out_plan);
    }

    return rc;
}

static int plan_alter_table_set_default(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_set_default *out_plan
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t column_count = 0U;
    size_t column_index = 0U;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_set_default){0};
    rc = resolve_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = copy_identifier_text(
            child_at(statement, 1U),
            column_name,
            sizeof(column_name),
            database
        );
    }
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(column_name)) {
        set_reserved_name_error(database, "column", column_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_read_table_by_name(
            database,
            out_plan->target.schema.schema_id,
            out_plan->target.table_name,
            &out_plan->table
        );
        if (rc != MYLITE_OK) {
            set_table_does_not_exist_error(
                database,
                out_plan->target.schema.name,
                out_plan->target.table_name
            );
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && out_plan->table.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(
            database,
            "ALTER TABLE ALTER COLUMN SET DEFAULT supports only persistent base tables"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = load_table_columns(database, out_plan->table.table_id, &columns, &column_count);
    }
    if (rc == MYLITE_OK) {
        rc = find_column_index(columns, column_count, column_name, &column_index);
        if (rc != MYLITE_OK) {
            set_unknown_column_in_table_error(database, column_name, out_plan->table.name);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        out_plan->original_column = columns[column_index];
        planned_column_from_catalog_descriptor(
            &out_plan->original_column,
            child_at(statement, 2U),
            &out_plan->column
        );
        rc = validate_column_default(database, out_plan->column.default_node, &out_plan->column);
    }
    if (rc == MYLITE_OK) {
        rc = finalize_planned_column_default(database, &out_plan->column);
    }

    free(columns);

    return rc;
}

static int alter_table_set_default_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_set_default *plan
) {
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    int rc = mylite_catalog_begin_mutation(database, &mutation);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_replace_column_in_mutation(
            database,
            &mutation,
            plan->table.table_id,
            plan->original_column.column_id,
            plan->original_column.name,
            plan->original_column.logical_type,
            plan->original_column.physical_type,
            plan->original_column.is_nullable,
            plan->original_column.is_visible,
            plan->column.default_kind,
            plan->column.default_integer
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_update_table_identity_in_mutation(
            database,
            &mutation,
            plan->table.table_id,
            plan->table.schema_id,
            plan->table.name,
            NULL
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to set column default");
        mylite_catalog_rollback_mutation(database, &mutation);
        return rc;
    }

    return MYLITE_OK;
}

static int plan_alter_table_drop_default(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_drop_default *out_plan
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t column_count = 0U;
    size_t column_index = 0U;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_drop_default){0};
    rc = resolve_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = copy_identifier_text(
            child_at(statement, 1U),
            column_name,
            sizeof(column_name),
            database
        );
    }
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(column_name)) {
        set_reserved_name_error(database, "column", column_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_read_table_by_name(
            database,
            out_plan->target.schema.schema_id,
            out_plan->target.table_name,
            &out_plan->table
        );
        if (rc != MYLITE_OK) {
            set_table_does_not_exist_error(
                database,
                out_plan->target.schema.name,
                out_plan->target.table_name
            );
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && out_plan->table.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(
            database,
            "ALTER TABLE ALTER COLUMN DROP DEFAULT supports only persistent base tables"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = load_table_columns(database, out_plan->table.table_id, &columns, &column_count);
    }
    if (rc == MYLITE_OK) {
        rc = find_column_index(columns, column_count, column_name, &column_index);
        if (rc != MYLITE_OK) {
            set_unknown_column_in_table_error(database, column_name, out_plan->table.name);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        out_plan->column = columns[column_index];
    }

    free(columns);

    return rc;
}

static int alter_table_drop_default_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_drop_default *plan
) {
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    int rc = mylite_catalog_begin_mutation(database, &mutation);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_replace_column_in_mutation(
            database,
            &mutation,
            plan->table.table_id,
            plan->column.column_id,
            plan->column.name,
            plan->column.logical_type,
            plan->column.physical_type,
            plan->column.is_nullable,
            plan->column.is_visible,
            MYLITE_CATALOG_COLUMN_DEFAULT_NO_EXPLICIT,
            0
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_update_table_identity_in_mutation(
            database,
            &mutation,
            plan->table.table_id,
            plan->table.schema_id,
            plan->table.name,
            NULL
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to drop column default");
        mylite_catalog_rollback_mutation(database, &mutation);
        return rc;
    }

    return MYLITE_OK;
}

static int plan_alter_table_column_visibility(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_column_visibility *out_plan
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t column_count = 0U;
    size_t column_index = 0U;
    size_t visible_count = 0U;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_column_visibility){0};
    out_plan->is_visible = mylite_sql_ast_node_column_visibility(statement) ==
                           MYLITE_SQL_AST_COLUMN_VISIBILITY_VISIBLE;
    rc = resolve_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = copy_identifier_text(
            child_at(statement, 1U),
            column_name,
            sizeof(column_name),
            database
        );
    }
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(column_name)) {
        set_reserved_name_error(database, "column", column_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_read_table_by_name(
            database,
            out_plan->target.schema.schema_id,
            out_plan->target.table_name,
            &out_plan->table
        );
        if (rc != MYLITE_OK) {
            set_table_does_not_exist_error(
                database,
                out_plan->target.schema.name,
                out_plan->target.table_name
            );
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && out_plan->table.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(
            database,
            "ALTER TABLE ALTER COLUMN SET VISIBLE/INVISIBLE supports only persistent base tables"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = load_table_columns(database, out_plan->table.table_id, &columns, &column_count);
    }
    if (rc == MYLITE_OK) {
        rc = find_column_index(columns, column_count, column_name, &column_index);
        if (rc != MYLITE_OK) {
            set_unknown_column_in_table_error(database, column_name, out_plan->table.name);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        visible_count = count_visible_columns(columns, column_count);
        if (!out_plan->is_visible && columns[column_index].is_visible && visible_count <= 1U) {
            set_must_have_visible_column_error(database);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        out_plan->column = columns[column_index];
    }

    free(columns);

    return rc;
}

static int alter_table_column_visibility_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_column_visibility *plan
) {
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    int rc = mylite_catalog_begin_mutation(database, &mutation);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_set_column_visibility_in_mutation(
            database,
            &mutation,
            plan->table.table_id,
            plan->column.column_id,
            plan->is_visible
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_update_table_identity_in_mutation(
            database,
            &mutation,
            plan->table.table_id,
            plan->table.schema_id,
            plan->table.name,
            NULL
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to set column visibility");
        mylite_catalog_rollback_mutation(database, &mutation);
        return rc;
    }

    return MYLITE_OK;
}

static void planned_column_from_catalog_descriptor(
    const struct mylite_catalog_column_descriptor *descriptor,
    const struct mylite_sql_ast_node *default_node,
    struct planned_column *out_column
) {
    *out_column = (struct planned_column){0};
    snprintf(out_column->name, sizeof(out_column->name), "%s", descriptor->name);
    out_column->logical_type = descriptor->logical_type;
    out_column->physical_type = descriptor->physical_type;
    out_column->is_nullable = descriptor->is_nullable;
    out_column->default_node = default_node;
    out_column->default_kind = descriptor->default_kind;
    out_column->default_integer = descriptor->default_integer;
}

static int resolve_alter_table_column_replacement_plan(
    struct mylite_db *database,
    struct planned_alter_table_modify_column *out_plan
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    size_t column_count = 0U;
    size_t column_index = 0U;
    int rc = mylite_catalog_read_table_by_name(
        database,
        out_plan->target.schema.schema_id,
        out_plan->target.table_name,
        &out_plan->table
    );

    if (rc != MYLITE_OK) {
        set_table_does_not_exist_error(
            database,
            out_plan->target.schema.name,
            out_plan->target.table_name
        );
        return MYLITE_ERROR;
    }
    if (out_plan->table.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(database, out_plan->unsupported_object_message);
        return MYLITE_ERROR;
    }

    rc = load_table_columns(database, out_plan->table.table_id, &columns, &column_count);
    if (rc == MYLITE_OK) {
        rc = find_column_index(columns, column_count, out_plan->lookup_column_name, &column_index);
        if (rc != MYLITE_OK) {
            set_unknown_column_in_table_error(
                database,
                out_plan->lookup_column_name,
                out_plan->table.name
            );
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && out_plan->checks_duplicate_replacement) {
        size_t replacement_index = 0U;

        if (find_column_index(columns, column_count, out_plan->column.name, &replacement_index) ==
                MYLITE_OK &&
            replacement_index != column_index) {
            set_duplicate_column_error(database, columns[replacement_index].name);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        out_plan->original_column = columns[column_index];
        out_plan->column_index = column_index;
        out_plan->column_count = column_count;
        rc = complete_alter_table_modify_column_plan(database, columns, out_plan);
    }

    free(columns);

    return rc;
}

static int complete_alter_table_modify_column_plan(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    struct planned_alter_table_modify_column *out_plan
) {
    int rc = MYLITE_OK;

    if (modify_column_definition_matches(&out_plan->original_column, &out_plan->column)) {
        if (modify_column_name_matches(&out_plan->original_column, &out_plan->column)) {
            if (out_plan->original_column.is_visible) {
                out_plan->is_noop = true;
            } else {
                out_plan->is_metadata_only = true;
            }
            return MYLITE_OK;
        }
        out_plan->is_metadata_only = true;
        return MYLITE_OK;
    }
    if (modify_column_metadata_only_replacement(&out_plan->original_column, &out_plan->column)) {
        out_plan->is_metadata_only = true;
        return MYLITE_OK;
    }

    if (!modify_column_type_matches(&out_plan->original_column, &out_plan->column)) {
        out_plan->reports_rebuild_row_count = true;
    }
    rc = choose_sqlite_rowid_alias(
        database,
        columns,
        out_plan->column_count,
        out_plan->rowid_alias_message,
        &out_plan->rowid_alias
    );
    if (rc == MYLITE_OK) {
        rc = collect_modify_column_rebuild_columns(
            database,
            columns,
            out_plan->column_count,
            out_plan
        );
    }

    return rc;
}

static bool modify_column_definition_matches(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
) {
    if (strcmp(original_column->logical_type, replacement_column->logical_type) != 0) {
        return false;
    }
    if (strcmp(original_column->physical_type, replacement_column->physical_type) != 0) {
        return false;
    }
    if (original_column->is_nullable != replacement_column->is_nullable) {
        return false;
    }
    if (original_column->default_kind != replacement_column->default_kind) {
        return false;
    }
    if (original_column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER &&
        original_column->default_integer != replacement_column->default_integer) {
        return false;
    }

    return true;
}

static bool modify_column_type_matches(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
) {
    if (strcmp(original_column->logical_type, replacement_column->logical_type) != 0) {
        return false;
    }
    if (strcmp(original_column->physical_type, replacement_column->physical_type) != 0) {
        return false;
    }

    return true;
}

static bool modify_column_metadata_only_replacement(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
) {
    if (strcmp(original_column->physical_type, replacement_column->physical_type) != 0) {
        return false;
    }
    if (original_column->is_nullable != replacement_column->is_nullable) {
        return false;
    }
    if (!modify_column_integer_value_domain_matches(original_column, replacement_column)) {
        return false;
    }

    return true;
}

static bool modify_column_name_matches(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
) {
    if (strcmp(original_column->name, replacement_column->name) != 0) {
        return false;
    }

    return true;
}

static void planned_alter_table_modify_column_deinit(
    struct planned_alter_table_modify_column *plan
) {
    if (plan == NULL) {
        return;
    }

    free(plan->columns);
    *plan = (struct planned_alter_table_modify_column){0};
}

static int alter_table_modify_column_from_plan(
    struct mylite_db *database,
    struct planned_alter_table_modify_column *plan
) {
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    int64_t validated_row_count = 0;
    int rc = MYLITE_OK;

    if (plan->is_noop) {
        return MYLITE_OK;
    }

    rc = mylite_catalog_begin_mutation(database, &mutation);
    if (rc == MYLITE_OK && !plan->is_metadata_only) {
        rc = validate_modify_column_existing_rows(database, plan, &validated_row_count);
        if (rc == MYLITE_OK && plan->reports_rebuild_row_count) {
            plan->affected_rows = validated_row_count;
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_replace_column_in_mutation(
            database,
            &mutation,
            plan->table.table_id,
            plan->original_column.column_id,
            plan->column.name,
            plan->column.logical_type,
            plan->column.physical_type,
            plan->column.is_nullable,
            true,
            plan->column.default_kind,
            plan->column.default_integer
        );
    }
    if (rc == MYLITE_OK && plan->is_metadata_only) {
        if (!modify_column_name_matches(&plan->original_column, &plan->column)) {
            struct planned_alter_table_rename_column rename_plan = {
                .target = plan->target,
                .table = plan->table,
                .column = plan->original_column,
                .is_noop = false,
            };

            memcpy(
                rename_plan.new_column_name,
                plan->column.name,
                sizeof(rename_plan.new_column_name)
            );
            rc = execute_physical_alter_table_rename_column(database, &rename_plan);
        }
    } else if (rc == MYLITE_OK) {
        rc = execute_physical_alter_table_modify_column(database, plan, &mutation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_update_table_identity_in_mutation(
            database,
            &mutation,
            plan->table.table_id,
            plan->table.schema_id,
            plan->table.name,
            NULL
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, plan->failure_message);
        mylite_catalog_rollback_mutation(database, &mutation);
        return rc;
    }

    if (!plan->is_metadata_only ||
        !modify_column_name_matches(&plan->original_column, &plan->column)) {
        ++database->session.sqlite_schema_generation;
    }

    return MYLITE_OK;
}

static int collect_modify_column_rebuild_columns(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    struct planned_alter_table_modify_column *out_plan
) {
    struct mylite_catalog_column_descriptor *rebuild_columns = NULL;

    if (column_count > SIZE_MAX / sizeof(*rebuild_columns)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rebuild_columns = calloc(column_count, sizeof(*rebuild_columns));
    if (rebuild_columns == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    for (size_t index = 0U; index < column_count; ++index) {
        rebuild_columns[index] = columns[index];
        if (index == out_plan->column_index) {
            make_modify_target_descriptor(out_plan, &rebuild_columns[index]);
        }
    }

    out_plan->columns = rebuild_columns;
    return MYLITE_OK;
}

static int validate_modify_column_existing_rows(
    struct mylite_db *database,
    const struct planned_alter_table_modify_column *plan,
    int64_t *out_row_count
) {
    sqlite3_stmt *statement = NULL;
    struct mylite_catalog_column_descriptor target_column = {0};
    char *sql = NULL;
    size_t row_number = 0U;
    int sqlite_rc = SQLITE_OK;
    int rc = build_alter_table_modify_validation_sql(plan, &sql);

    *out_row_count = 0;
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    make_modify_target_descriptor(plan, &target_column);
    while (rc == MYLITE_OK && (sqlite_rc = sqlite3_step(statement)) == SQLITE_ROW) {
        int sqlite_type = sqlite3_column_type(statement, 0);

        if (row_number >= (size_t)INT64_MAX) {
            set_runtime_error(database, plan->row_count_overflow_message);
            rc = MYLITE_ERROR;
            break;
        }
        ++row_number;
        if (sqlite_type == SQLITE_NULL) {
            if (!target_column.is_nullable) {
                set_data_truncated_error(database, target_column.name, row_number);
                rc = MYLITE_ERROR;
            }
        } else if (sqlite_type == SQLITE_INTEGER) {
            rc = validate_existing_integer_for_column(
                database,
                (int64_t)sqlite3_column_int64(statement, 0),
                &target_column,
                row_number,
                plan->integer_support_message
            );
        } else {
            set_physical_sqlite_row_error(database);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && sqlite_rc != SQLITE_DONE) {
        rc = mylite_sqlite_status_to_mylite(sqlite_rc);
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);

    if (rc == MYLITE_OK) {
        *out_row_count = (int64_t)row_number;
    } else if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    } else if (mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) == MYLITE_OK) {
        set_physical_sqlite_row_error(database);
        rc = MYLITE_ERROR;
    }

    return rc;
}

static int validate_existing_integer_for_column(
    struct mylite_db *database,
    int64_t value,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    const char *unsupported_message
) {
    struct integer_column_range range = {0};
    int rc = integer_range_for_column(database, column, unsupported_message, &range);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (value < 0) {
        const uint64_t int64_min_magnitude = (uint64_t)INT64_MAX + 1U;
        uint64_t magnitude = value == INT64_MIN ? int64_min_magnitude : (uint64_t)(-value);

        if (range.negative_abs_max == 0U || magnitude > range.negative_abs_max) {
            set_out_of_range_error(database, column->name, row_number);
            return MYLITE_ERROR;
        }
        return MYLITE_OK;
    }

    if ((uint64_t)value > range.positive_max) {
        set_out_of_range_error(database, column->name, row_number);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static void make_modify_target_descriptor(
    const struct planned_alter_table_modify_column *plan,
    struct mylite_catalog_column_descriptor *out_column
) {
    *out_column = plan->original_column;
    memcpy(out_column->name, plan->column.name, sizeof(out_column->name));
    snprintf(
        out_column->logical_type,
        sizeof(out_column->logical_type),
        "%s",
        plan->column.logical_type
    );
    snprintf(
        out_column->physical_type,
        sizeof(out_column->physical_type),
        "%s",
        plan->column.physical_type
    );
    out_column->is_nullable = plan->column.is_nullable;
    out_column->default_kind = plan->column.default_kind;
    out_column->default_integer = plan->column.default_integer;
}

static int execute_physical_alter_table_modify_column(
    struct mylite_db *database,
    const struct planned_alter_table_modify_column *plan,
    const struct mylite_catalog_mutation *mutation
) {
    char temporary_physical_name[MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY];
    char *sql = NULL;
    int rc = build_modify_temporary_physical_name(
        plan,
        mutation,
        temporary_physical_name,
        sizeof(temporary_physical_name)
    );

    if (rc == MYLITE_OK) {
        rc = build_alter_table_modify_create_sql(plan, temporary_physical_name, &sql);
    }
    if (rc == MYLITE_OK) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);
    sql = NULL;

    if (rc == MYLITE_OK) {
        rc = build_alter_table_modify_copy_sql(plan, temporary_physical_name, &sql);
    }
    if (rc == MYLITE_OK) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);
    sql = NULL;

    if (rc == MYLITE_OK) {
        rc = execute_physical_drop_table(database, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = build_alter_table_rename_physical_table_sql(
            temporary_physical_name,
            plan->table.physical_name,
            &sql
        );
    }
    if (rc == MYLITE_OK) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);

    return rc;
}

static int rename_table_from_plan_with_policy(
    struct mylite_db *database,
    const struct planned_rename_table *plan,
    bool allow_same_object_noop,
    const char *unsupported_object_message
) {
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    struct mylite_catalog_table_descriptor source = {0};
    int rc = MYLITE_OK;

    if (allow_same_object_noop) {
        rc = mylite_catalog_read_table_by_name(
            database,
            plan->source.schema.schema_id,
            plan->source.table_name,
            &source
        );
        if (rc != MYLITE_OK) {
            set_table_does_not_exist_error(
                database,
                plan->source.schema.name,
                plan->source.table_name
            );
            return MYLITE_ERROR;
        }
        if (source.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
            set_unsupported_error(database, unsupported_object_message);
            return MYLITE_ERROR;
        }
        if (source.schema_id == plan->target.schema.schema_id &&
            text_equals_ascii_case_insensitive(source.name, plan->target.table_name)) {
            return MYLITE_OK;
        }
    }

    rc = mylite_catalog_begin_mutation(database, &mutation);
    if (rc == MYLITE_OK) {
        rc = rename_table_pair_in_mutation(
            database,
            &mutation,
            plan,
            allow_same_object_noop,
            unsupported_object_message
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to rename table descriptor");
        mylite_catalog_rollback_mutation(database, &mutation);
        return rc;
    }

    return MYLITE_OK;
}

static int rename_table_pair_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    const struct planned_rename_table *plan,
    bool allow_same_object_noop,
    const char *unsupported_object_message
) {
    struct mylite_catalog_table_descriptor source = {0};
    struct mylite_catalog_table_descriptor target = {0};
    bool found = false;
    int rc = mylite_catalog_read_table_by_name(
        database,
        plan->source.schema.schema_id,
        plan->source.table_name,
        &source
    );

    if (rc != MYLITE_OK) {
        set_table_does_not_exist_error(database, plan->source.schema.name, plan->source.table_name);
        return MYLITE_ERROR;
    }
    if (source.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(database, unsupported_object_message);
        return MYLITE_ERROR;
    }
    if (allow_same_object_noop && source.schema_id == plan->target.schema.schema_id &&
        text_equals_ascii_case_insensitive(source.name, plan->target.table_name)) {
        return MYLITE_OK;
    }

    rc = mylite_catalog_try_read_table_by_name(
        database,
        plan->target.schema.schema_id,
        plan->target.table_name,
        &target,
        &found
    );
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to read target table descriptor");
        return rc;
    }
    if (found) {
        set_table_exists_error(database, plan->target.table_name);
        return MYLITE_ERROR;
    }

    return mylite_catalog_update_table_identity_in_mutation(
        database,
        mutation,
        source.table_id,
        plan->target.schema.schema_id,
        plan->target.table_name,
        NULL
    );
}

static int plan_insert(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_insert *out_plan
) {
    const struct mylite_sql_ast_node *column_list = child_at(statement, 1U);
    const struct mylite_sql_ast_node *row_list = child_at(statement, 2U);
    size_t *target_indexes = NULL;
    size_t target_count = 0U;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_insert){0};
    rc = resolve_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = resolve_readable_base_table(database, &out_plan->target, &out_plan->table);
    }
    if (rc == MYLITE_OK) {
        rc = load_table_columns(
            database,
            out_plan->table.table_id,
            &out_plan->columns,
            &out_plan->column_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = collect_insert_target_indexes(
            database,
            column_list,
            out_plan,
            &target_indexes,
            &target_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = check_insert_target_duplicate(
            database,
            out_plan->columns,
            target_indexes,
            target_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = check_insert_omitted_columns(database, out_plan, target_indexes, target_count);
    }
    if (rc == MYLITE_OK) {
        rc = plan_insert_rows(database, row_list, target_indexes, target_count, out_plan);
    }

    free(target_indexes);
    if (rc != MYLITE_OK) {
        planned_insert_deinit(out_plan);
    }

    return rc;
}

static int plan_insert_set(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_insert *out_plan
) {
    const struct mylite_sql_ast_node *assignment_list = child_at(statement, 1U);
    size_t *target_indexes = NULL;
    size_t target_count = 0U;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_insert){0};
    rc = resolve_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = resolve_readable_base_table(database, &out_plan->target, &out_plan->table);
    }
    if (rc == MYLITE_OK) {
        rc = load_table_columns(
            database,
            out_plan->table.table_id,
            &out_plan->columns,
            &out_plan->column_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = collect_insert_set_target_indexes(
            database,
            assignment_list,
            out_plan,
            &target_indexes,
            &target_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = check_insert_target_duplicate(
            database,
            out_plan->columns,
            target_indexes,
            target_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_insert_set_row(database, assignment_list, target_indexes, target_count, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = check_insert_omitted_columns(database, out_plan, target_indexes, target_count);
    }

    free(target_indexes);
    if (rc != MYLITE_OK) {
        planned_insert_deinit(out_plan);
    }

    return rc;
}

static void planned_insert_deinit(struct planned_insert *plan) {
    if (plan == NULL) {
        return;
    }

    if (plan->rows != NULL) {
        for (size_t row_index = 0U; row_index < plan->row_count; ++row_index) {
            free(plan->rows[row_index].values);
        }
    }
    free(plan->rows);
    free(plan->columns);
    *plan = (struct planned_insert){0};
}

static int execute_insert_from_plan(
    struct mylite_db *database,
    const struct planned_insert *plan,
    mylite_result *result
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    bool transaction_started = false;
    int rc = build_insert_sql(plan, &sql);

    if (rc == MYLITE_OK) {
        rc = execute_sqlite_control_sql(database, "BEGIN IMMEDIATE");
    }
    if (rc == MYLITE_OK) {
        transaction_started = true;
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    for (size_t row_index = 0U; rc == MYLITE_OK && row_index < plan->row_count; ++row_index) {
        rc = bind_insert_row(statement, plan, row_index);
        if (rc == MYLITE_OK) {
            rc = step_insert_row(statement);
        }
    }
    rc = finalize_sqlite_statement(statement, rc);
    statement = NULL;
    if (rc == MYLITE_OK) {
        rc = execute_sqlite_control_sql(database, "COMMIT");
        if (rc == MYLITE_OK) {
            transaction_started = false;
        }
    }
    if (rc != MYLITE_OK && transaction_started) {
        (void)execute_sqlite_control_sql(database, "ROLLBACK");
    }
    free(sql);

    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
            return rc;
        }
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }

    mylite_result_set_affected_rows(result, (int64_t)plan->row_count);

    return MYLITE_OK;
}

static int plan_update(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_update *out_plan
) {
    const struct mylite_sql_ast_node *assignment_list = child_at(statement, 1U);
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *order_clause = NULL;
    const struct mylite_sql_ast_node *limit_clause = NULL;
    const struct mylite_sql_ast_node *optional_clause = NULL;
    struct mylite_catalog_column_descriptor *table_columns = NULL;
    size_t table_column_count = 0U;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_update){0};
    optional_clause = child_at(statement, 2U);
    while (optional_clause != NULL) {
        if (optional_clause->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
            where_clause = optional_clause;
        } else if (optional_clause->kind == MYLITE_SQL_AST_ORDER_BY_CLAUSE) {
            order_clause = optional_clause;
        } else if (optional_clause->kind == MYLITE_SQL_AST_LIMIT_CLAUSE) {
            limit_clause = optional_clause;
        } else {
            set_unsupported_error(database, "UPDATE supports only SET, WHERE, ORDER BY, and LIMIT");
            return MYLITE_ERROR;
        }
        optional_clause = optional_clause->next_sibling;
    }

    rc = resolve_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = resolve_readable_base_table(database, &out_plan->target, &out_plan->table);
    }
    if (rc == MYLITE_OK) {
        rc = load_table_columns(
            database,
            out_plan->table.table_id,
            &table_columns,
            &table_column_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_update_assignment(
            database,
            assignment_list,
            table_columns,
            table_column_count,
            out_plan
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_select_predicate(
            database,
            where_clause,
            NULL,
            table_columns,
            table_column_count,
            &out_plan->predicate
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_select_order(
            database,
            order_clause,
            NULL,
            NULL,
            table_columns,
            table_column_count,
            &out_plan->order
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_update_limit(database, limit_clause, &out_plan->limit);
    }
    if (rc == MYLITE_OK && out_plan->limit.has_limit && out_plan->limit.row_count > 0) {
        rc = choose_sqlite_rowid_alias(
            database,
            table_columns,
            table_column_count,
            "UPDATE LIMIT requires an unshadowed SQLite rowid alias",
            &out_plan->rowid_alias
        );
    }

    free(table_columns);
    if (rc != MYLITE_OK) {
        planned_update_deinit(out_plan);
    }

    return rc;
}

static void planned_update_deinit(struct planned_update *plan) {
    if (plan == NULL) {
        return;
    }

    *plan = (struct planned_update){0};
}

static int execute_update_from_plan(
    struct mylite_db *database,
    const struct planned_update *plan,
    mylite_result *result
) {
    sqlite3_stmt *statement = NULL;
    struct planned_update executable_plan = *plan;
    char *sql = NULL;
    bool transaction_started = false;
    bool matches_any_row = false;
    int64_t affected_rows = 0;
    int sqlite_rc = SQLITE_OK;
    int rc = execute_sqlite_control_sql(database, "BEGIN IMMEDIATE");

    if (rc == MYLITE_OK) {
        transaction_started = true;
        rc = update_matches_any_row(database, plan, &matches_any_row);
    }
    if (rc == MYLITE_OK && matches_any_row) {
        rc = convert_update_value(
            database,
            plan->assignment_value_node,
            &plan->assignment_column,
            &executable_plan.assignment_value
        );
    }
    if (rc == MYLITE_OK && matches_any_row) {
        rc = build_update_sql(&executable_plan, &sql);
    }
    if (rc == MYLITE_OK && matches_any_row) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK && matches_any_row) {
        rc = bind_update_parameters(statement, &executable_plan);
    }
    if (rc == MYLITE_OK && matches_any_row) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            affected_rows = (int64_t)sqlite3_changes64(database->sqlite);
        } else {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finalize_sqlite_statement(statement, rc);
    statement = NULL;
    if (rc == MYLITE_OK) {
        rc = execute_sqlite_control_sql(database, "COMMIT");
        if (rc == MYLITE_OK) {
            transaction_started = false;
        }
    }
    if (rc != MYLITE_OK && transaction_started) {
        (void)execute_sqlite_control_sql(database, "ROLLBACK");
    }
    free(sql);

    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
            return rc;
        }
        if (mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) != MYLITE_OK) {
            return rc;
        }
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }

    mylite_result_set_affected_rows(result, affected_rows);

    return MYLITE_OK;
}

static int update_matches_any_row(
    struct mylite_db *database,
    const struct planned_update *plan,
    bool *out_matches
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = MYLITE_OK;

    *out_matches = false;
    if (plan->limit.has_limit && plan->limit.row_count == 0) {
        return MYLITE_OK;
    }

    rc = build_update_matched_sql(plan, &sql);
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_update_matched_parameters(statement, plan);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            *out_matches = true;
        } else if (sqlite_rc != SQLITE_DONE) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);

    return rc;
}

static int init_select_source_context(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    const struct table_name_resolution *source,
    struct select_source_context *out_context
) {
    const struct mylite_sql_ast_node *alias = child_at(from_clause, 1U);

    *out_context = (struct select_source_context){.source = source};
    if (alias == NULL) {
        return MYLITE_OK;
    }
    if (alias->kind != MYLITE_SQL_AST_IDENTIFIER) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    int rc = copy_identifier_text(alias, out_context->alias, sizeof(out_context->alias), database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    out_context->has_alias = true;

    return MYLITE_OK;
}

static int plan_select(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_select *out_plan
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *from_clause = child_at(statement, 1U);
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *order_clause = NULL;
    const struct mylite_sql_ast_node *limit_clause = NULL;
    const struct mylite_sql_ast_node *optional_clause = NULL;
    struct mylite_catalog_column_descriptor *table_columns = NULL;
    struct select_source_context source_context = {0};
    size_t table_column_count = 0U;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_select){0};
    out_plan->is_distinct =
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT;
    if (from_clause == NULL || from_clause->kind != MYLITE_SQL_AST_FROM_TABLE) {
        set_unsupported_error(database, "SELECT supports only descriptor-backed table reads");
        return MYLITE_ERROR;
    }
    optional_clause = child_at(statement, 2U);
    while (optional_clause != NULL) {
        if (optional_clause->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
            where_clause = optional_clause;
        } else if (optional_clause->kind == MYLITE_SQL_AST_ORDER_BY_CLAUSE) {
            order_clause = optional_clause;
        } else if (optional_clause->kind == MYLITE_SQL_AST_LIMIT_CLAUSE) {
            limit_clause = optional_clause;
        } else {
            set_unsupported_error(database, "SELECT supports only WHERE, ORDER BY, and LIMIT");
            return MYLITE_ERROR;
        }
        optional_clause = optional_clause->next_sibling;
    }

    rc = resolve_table_name(database, child_at(from_clause, 0U), &out_plan->source);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->source.table_name)) {
        set_reserved_name_error(database, "table", out_plan->source.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = resolve_readable_base_table(database, &out_plan->source, &out_plan->table);
    }
    if (rc == MYLITE_OK) {
        rc = init_select_source_context(database, from_clause, &out_plan->source, &source_context);
    }
    if (rc == MYLITE_OK) {
        rc = load_table_columns(
            database,
            out_plan->table.table_id,
            &table_columns,
            &table_column_count
        );
    }
    if (rc == MYLITE_OK) {
        if (out_plan->is_distinct) {
            rc = plan_select_distinct_column(
                database,
                select_list,
                &source_context,
                table_columns,
                table_column_count,
                out_plan
            );
        } else {
            rc = plan_select_columns(
                database,
                select_list,
                &source_context,
                table_columns,
                table_column_count,
                out_plan
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = plan_select_predicate(
            database,
            where_clause,
            &source_context,
            table_columns,
            table_column_count,
            &out_plan->predicate
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_select_order(
            database,
            order_clause,
            &source_context,
            out_plan,
            table_columns,
            table_column_count,
            &out_plan->order
        );
    }
    if (rc == MYLITE_OK && out_plan->is_distinct && out_plan->order.has_order &&
        out_plan->order.column.column_id != out_plan->columns[0].column_id) {
        set_unsupported_error(
            database,
            "SELECT DISTINCT supports ORDER BY only on the selected column"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = plan_select_limit(database, limit_clause, &out_plan->limit);
    }

    free(table_columns);
    if (rc != MYLITE_OK) {
        planned_select_deinit(out_plan);
    }

    return rc;
}

static void planned_select_deinit(struct planned_select *plan) {
    if (plan == NULL) {
        return;
    }

    free(plan->columns);
    free((void *)plan->column_aliases);
    *plan = (struct planned_select){0};
}

static int execute_select_from_plan(
    struct mylite_db *database,
    const struct planned_select *plan,
    mylite_result **out_result
) {
    sqlite3_stmt *statement = NULL;
    mylite_result *result = NULL;
    char *sql = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        rc = append_select_result_column(database, result, plan, column_index);
    }
    if (rc == MYLITE_OK) {
        rc = build_select_sql(plan, &sql);
    }
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_select_parameters(statement, plan);
    }
    while (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            break;
        }
        if (sqlite_rc != SQLITE_ROW) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            break;
        }
        rc = append_selected_sqlite_row(statement, result);
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
            return rc;
        }
        if (mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) != MYLITE_OK) {
            return rc;
        }
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }

    return finish_successful_result(database, result, out_result);
}

static bool select_statement_has_count_aggregate(const struct mylite_sql_ast_node *statement) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *select_item = NULL;

    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        return false;
    }

    select_item = child_at(select_list, 0U);
    while (select_item != NULL) {
        const struct mylite_sql_ast_node *expression = child_at(select_item, 0U);

        expression = unwrap_parenthesized_expression(expression);
        if (count_function_from_expression(expression) != PLANNED_COUNT_NONE) {
            return true;
        }
        select_item = select_item->next_sibling;
    }

    return false;
}

static int plan_count(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_count *out_plan
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *select_item = child_at(select_list, 0U);
    const struct mylite_sql_ast_node *from_clause = child_at(statement, 1U);
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *optional_clause = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    const struct mylite_sql_ast_node *count_expression = NULL;
    struct planned_count_source_nodes source_nodes = {0};
    int rc = MYLITE_OK;

    *out_plan = (struct planned_count){false};
    expression = child_at(select_item, 0U);
    out_plan->expression = expression;
    out_plan->alias = child_at(select_item, 1U);
    count_expression = unwrap_parenthesized_expression(expression);
    out_plan->function = count_function_from_expression(count_expression);
    if (mylite_sql_ast_node_child_count(select_list) != 1U) {
        set_unsupported_error(database, count_exactly_one_message(out_plan->function));
        return MYLITE_ERROR;
    }
    if (out_plan->function == PLANNED_COUNT_NONE) {
        set_unsupported_error(database, count_exactly_one_message(out_plan->function));
        return MYLITE_ERROR;
    }
    if (out_plan->function == PLANNED_COUNT_LITERAL) {
        rc = plan_count_literal(database, count_expression, &out_plan->count_literal);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    optional_clause = child_at(statement, 2U);
    while (optional_clause != NULL) {
        if (optional_clause->kind != MYLITE_SQL_AST_WHERE_CLAUSE) {
            set_unsupported_error(database, count_supported_clauses_message(out_plan->function));
            return MYLITE_ERROR;
        }
        where_clause = optional_clause;
        optional_clause = optional_clause->next_sibling;
    }

    source_nodes.from_clause = from_clause;
    source_nodes.where_clause = where_clause;
    source_nodes.count_expression = count_expression;
    if (from_clause == NULL || from_clause->kind == MYLITE_SQL_AST_FROM_DUAL) {
        return plan_count_without_source(database, &source_nodes, out_plan);
    }
    if (from_clause->kind != MYLITE_SQL_AST_FROM_TABLE) {
        set_unsupported_error(database, count_descriptor_table_message(out_plan->function));
        return MYLITE_ERROR;
    }

    return plan_count_table_source(database, &source_nodes, out_plan);
}

static int plan_count_without_source(
    struct mylite_db *database,
    const struct planned_count_source_nodes *nodes,
    struct planned_count *out_plan
) {
    char column_name[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int rc = MYLITE_OK;

    if (out_plan->function == PLANNED_COUNT_COLUMN ||
        out_plan->function == PLANNED_COUNT_DISTINCT_COLUMN) {
        const struct mylite_sql_ast_node *column_node = child_at(nodes->count_expression, 0U);
        char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
        size_t part_count = 0U;

        rc = collect_column_reference_parts(database, column_node, parts, &part_count);
        if (rc == MYLITE_OK) {
            rc = format_column_reference_name(
                database,
                parts,
                part_count,
                column_name,
                sizeof(column_name)
            );
        }
        if (rc == MYLITE_OK) {
            set_unknown_column_error(database, column_name);
            rc = MYLITE_ERROR;
        }
        return rc;
    }
    if (nodes->where_clause != NULL) {
        if (out_plan->function == PLANNED_COUNT_LITERAL) {
            set_unsupported_error(
                database,
                "COUNT(literal) WHERE requires a descriptor-backed table"
            );
        } else {
            set_unsupported_error(database, "COUNT(*) WHERE requires a descriptor-backed table");
        }
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int plan_count_table_source(
    struct mylite_db *database,
    const struct planned_count_source_nodes *nodes,
    struct planned_count *out_plan
) {
    struct mylite_catalog_column_descriptor *table_columns = NULL;
    struct select_source_context source_context = {0};
    size_t table_column_count = 0U;
    int rc = MYLITE_OK;

    out_plan->has_source = true;
    rc = resolve_table_name(database, child_at(nodes->from_clause, 0U), &out_plan->source);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->source.table_name)) {
        set_reserved_name_error(database, "table", out_plan->source.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = resolve_readable_base_table(database, &out_plan->source, &out_plan->table);
    }
    if (rc == MYLITE_OK) {
        rc = init_select_source_context(
            database,
            nodes->from_clause,
            &out_plan->source,
            &source_context
        );
    }
    if (rc == MYLITE_OK &&
        (nodes->where_clause != NULL || out_plan->function == PLANNED_COUNT_COLUMN ||
         out_plan->function == PLANNED_COUNT_DISTINCT_COLUMN)) {
        rc = load_table_columns(
            database,
            out_plan->table.table_id,
            &table_columns,
            &table_column_count
        );
    }
    if (rc == MYLITE_OK && (out_plan->function == PLANNED_COUNT_COLUMN ||
                            out_plan->function == PLANNED_COUNT_DISTINCT_COLUMN)) {
        rc = plan_count_column(
            database,
            nodes->count_expression,
            &source_context,
            table_columns,
            table_column_count,
            &out_plan->count_column
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_select_predicate(
            database,
            nodes->where_clause,
            &source_context,
            table_columns,
            table_column_count,
            &out_plan->predicate
        );
    }

    free(table_columns);
    return rc;
}

static enum planned_count_function count_function_from_expression(
    const struct mylite_sql_ast_node *expression
) {
    if (expression == NULL) {
        return PLANNED_COUNT_NONE;
    }
    if (expression->kind == MYLITE_SQL_AST_COUNT_STAR_FUNCTION) {
        return PLANNED_COUNT_STAR;
    }
    if (expression->kind == MYLITE_SQL_AST_COUNT_COLUMN_FUNCTION) {
        return PLANNED_COUNT_COLUMN;
    }
    if (expression->kind == MYLITE_SQL_AST_COUNT_LITERAL_FUNCTION) {
        return PLANNED_COUNT_LITERAL;
    }
    if (expression->kind == MYLITE_SQL_AST_COUNT_DISTINCT_COLUMN_FUNCTION) {
        return PLANNED_COUNT_DISTINCT_COLUMN;
    }

    return PLANNED_COUNT_NONE;
}

static const char *count_exactly_one_message(enum planned_count_function function) {
    if (function == PLANNED_COUNT_STAR) {
        return "COUNT(*) supports exactly one aggregate select item";
    }
    if (function == PLANNED_COUNT_COLUMN) {
        return "COUNT(column) supports exactly one aggregate select item";
    }
    if (function == PLANNED_COUNT_LITERAL) {
        return "COUNT(literal) supports exactly one aggregate select item";
    }
    if (function == PLANNED_COUNT_DISTINCT_COLUMN) {
        return "COUNT(DISTINCT column) supports exactly one aggregate select item";
    }

    return "COUNT aggregate supports exactly one aggregate select item";
}

static const char *count_supported_clauses_message(enum planned_count_function function) {
    if (function == PLANNED_COUNT_DISTINCT_COLUMN) {
        return "COUNT(DISTINCT column) supports only WHERE";
    }
    if (function == PLANNED_COUNT_COLUMN) {
        return "COUNT(column) supports only WHERE";
    }
    if (function == PLANNED_COUNT_LITERAL) {
        return "COUNT(literal) supports only WHERE";
    }

    return "COUNT(*) supports only WHERE";
}

static const char *count_descriptor_table_message(enum planned_count_function function) {
    if (function == PLANNED_COUNT_DISTINCT_COLUMN) {
        return "COUNT(DISTINCT column) supports only descriptor-backed table reads";
    }
    if (function == PLANNED_COUNT_COLUMN) {
        return "COUNT(column) supports only descriptor-backed table reads";
    }
    if (function == PLANNED_COUNT_LITERAL) {
        return "COUNT(literal) supports only descriptor-backed table reads";
    }

    return "COUNT(*) supports only descriptor-backed table reads";
}

static int plan_count_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    struct planned_value *out_literal
) {
    const struct mylite_sql_ast_node *argument = child_at(function, 0U);
    const struct mylite_sql_ast_node *literal = argument;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    *out_literal = (struct planned_value){.is_null = false, .integer = 1};
    if (argument == NULL) {
        set_unsupported_error(
            database,
            "COUNT(literal) supports only integer, boolean, and NULL literals"
        );
        return MYLITE_ERROR;
    }
    if (argument->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(argument);

        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            set_unsupported_error(
                database,
                "COUNT(literal) supports only integer, boolean, and NULL literals"
            );
            return MYLITE_ERROR;
        }
        literal = child_at(argument, 0U);
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "COUNT(literal) supports only integer, boolean, and NULL literals"
        );
        return MYLITE_ERROR;
    }
    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        out_literal->is_null = true;
        out_literal->integer = 0;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        out_literal->integer = 1;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        out_literal->integer = 0;
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_unsupported_error(
            database,
            "COUNT(literal) supports only integer, boolean, and NULL literals"
        );
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int plan_count_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column
) {
    const struct mylite_sql_ast_node *column_node = child_at(function, 0U);

    return resolve_descriptor_column_reference(
        database,
        column_node,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "COUNT(column) supports only descriptor columns",
        table_columns,
        table_column_count,
        out_column
    );
}

static int execute_count_from_plan(
    struct mylite_db *database,
    const struct planned_count *plan,
    mylite_result **out_result
) {
    mylite_result *result = NULL;
    char count_text[integer_text_capacity];
    int64_t count_value = 1;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }
    if (plan->function == PLANNED_COUNT_LITERAL && plan->count_literal.is_null) {
        count_value = 0;
    }

    rc = append_count_result_column(database, result, plan);
    if (rc == MYLITE_OK && plan->has_source) {
        rc = read_count_from_source(database, plan, &count_value);
    }
    if (rc == MYLITE_OK) {
        rc = format_count_value(database, count_value, count_text, sizeof(count_text));
    }
    if (rc == MYLITE_OK) {
        rc = append_count_value_row(database, result, count_text);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return count_execution_error(database, rc);
    }

    return finish_successful_result(database, result, out_result);
}

static int append_count_result_column(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_count *plan
) {
    char *column_name = NULL;
    int rc = MYLITE_OK;

    if (plan->alias != NULL) {
        rc = copy_select_item_alias_text(database, plan->alias, &column_name);
    } else {
        rc = copy_aggregate_result_column_name(database, &plan->expression->span, &column_name);
    }

    if (rc == MYLITE_OK) {
        rc = mylite_result_append_column(result, column_name);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }

    free(column_name);
    return rc;
}

static int copy_aggregate_result_column_name(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    char **out_text
) {
    char *text = NULL;
    size_t extra_spaces = 0U;

    if (out_text == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    if (span == NULL || span->text == NULL || span->length == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (span->length == SIZE_MAX) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    extra_spaces = aggregate_label_extra_spaces_after_block_comments(span);
    if (extra_spaces > SIZE_MAX - span->length - 1U) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    text = (char *)malloc(span->length + extra_spaces + 1U);
    if (text == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    copy_aggregate_label_with_spacing(span, text);
    *out_text = text;

    return MYLITE_OK;
}

static size_t aggregate_label_extra_spaces_after_block_comments(
    const struct mylite_sql_source_span *span
) {
    size_t extra_spaces = 0U;
    size_t source_index = 0U;

    while (source_index + 1U < span->length) {
        if (span->text[source_index] == '/' && span->text[source_index + 1U] == '*') {
            source_index += 2U;
            while (source_index + 1U < span->length &&
                   (span->text[source_index] != '*' || span->text[source_index + 1U] != '/')) {
                ++source_index;
            }
            if (source_index + 1U < span->length) {
                source_index += 2U;
                if (source_index < span->length &&
                    aggregate_label_needs_space_after_block_comment(span->text[source_index])) {
                    ++extra_spaces;
                }
            }
        } else {
            ++source_index;
        }
    }

    return extra_spaces;
}

static void copy_aggregate_label_with_spacing(
    const struct mylite_sql_source_span *span,
    char *destination
) {
    size_t destination_index = 0U;
    size_t source_index = 0U;

    while (source_index < span->length) {
        if (source_index + 1U < span->length && span->text[source_index] == '/' &&
            span->text[source_index + 1U] == '*') {
            destination[destination_index] = span->text[source_index];
            destination[destination_index + 1U] = span->text[source_index + 1U];
            destination_index += 2U;
            source_index += 2U;
            while (source_index + 1U < span->length &&
                   (span->text[source_index] != '*' || span->text[source_index + 1U] != '/')) {
                destination[destination_index] = span->text[source_index];
                ++destination_index;
                ++source_index;
            }
            if (source_index + 1U < span->length) {
                destination[destination_index] = span->text[source_index];
                destination[destination_index + 1U] = span->text[source_index + 1U];
                destination_index += 2U;
                source_index += 2U;
                if (source_index < span->length &&
                    aggregate_label_needs_space_after_block_comment(span->text[source_index])) {
                    destination[destination_index] = ' ';
                    ++destination_index;
                }
            }
        } else {
            destination[destination_index] = span->text[source_index];
            ++destination_index;
            ++source_index;
        }
    }

    destination[destination_index] = '\0';
}

static bool aggregate_label_needs_space_after_block_comment(char next_byte) {
    switch (next_byte) {
    case ' ':
    case '\f':
    case '\n':
    case '\r':
    case '\t':
    case '\v':
    case ')':
    case ',':
        return false;
    default:
        return true;
    }
}

static int read_count_from_source(
    struct mylite_db *database,
    const struct planned_count *plan,
    int64_t *out_count
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int rc = build_count_sql(plan, &sql);

    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_count_parameters(statement, plan);
    }
    if (rc == MYLITE_OK) {
        rc = step_count_statement(statement, out_count);
    }

    rc = finalize_sqlite_statement(statement, rc);
    free(sql);

    return rc;
}

static int step_count_statement(sqlite3_stmt *statement, int64_t *out_count) {
    int sqlite_rc = sqlite3_step(statement);

    if (sqlite_rc != SQLITE_ROW) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    *out_count = sqlite3_column_int64(statement, 0);
    sqlite_rc = sqlite3_step(statement);
    if (sqlite_rc != SQLITE_DONE) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    return MYLITE_OK;
}

static int format_count_value(
    struct mylite_db *database,
    int64_t count_value,
    char *buffer,
    size_t buffer_size
) {
    int written = snprintf(buffer, buffer_size, "%" PRId64, count_value);

    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format COUNT(*) value");
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int append_count_value_row(
    struct mylite_db *database,
    mylite_result *result,
    const char *count_text
) {
    const char *values[] = {count_text};
    int rc = mylite_result_append_text_row(result, values);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }

    return rc;
}

static int count_execution_error(struct mylite_db *database, int rc) {
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
        return rc;
    }
    if (mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) != MYLITE_OK) {
        return rc;
    }

    set_physical_sqlite_row_error(database);
    return MYLITE_ERROR;
}

static bool select_statement_has_min_max_aggregate(const struct mylite_sql_ast_node *statement) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *select_item = NULL;

    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        return false;
    }

    select_item = child_at(select_list, 0U);
    while (select_item != NULL) {
        const struct mylite_sql_ast_node *expression = child_at(select_item, 0U);

        expression = unwrap_parenthesized_expression(expression);
        if (min_max_aggregate_function_from_expression(expression) !=
            PLANNED_MIN_MAX_AGGREGATE_NONE) {
            return true;
        }
        select_item = select_item->next_sibling;
    }

    return false;
}

static int plan_min_max_aggregate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_min_max_aggregate *out_plan
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *select_item = child_at(select_list, 0U);
    const struct mylite_sql_ast_node *from_clause = child_at(statement, 1U);
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *optional_clause = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    struct mylite_catalog_column_descriptor *table_columns = NULL;
    struct select_source_context source_context = {0};
    size_t table_column_count = 0U;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_min_max_aggregate){0};
    if (mylite_sql_ast_node_child_count(select_list) != 1U) {
        set_unsupported_error(database, "MIN/MAX supports exactly one aggregate select item");
        return MYLITE_ERROR;
    }

    expression = child_at(select_item, 0U);
    out_plan->expression = expression;
    out_plan->alias = child_at(select_item, 1U);
    expression = unwrap_parenthesized_expression(expression);
    out_plan->function = min_max_aggregate_function_from_expression(expression);
    if (out_plan->function == PLANNED_MIN_MAX_AGGREGATE_NONE) {
        set_unsupported_error(database, "MIN/MAX supports exactly one aggregate select item");
        return MYLITE_ERROR;
    }

    optional_clause = child_at(statement, 2U);
    while (optional_clause != NULL) {
        if (optional_clause->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
            where_clause = optional_clause;
        } else {
            set_unsupported_error(database, "MIN/MAX supports only WHERE");
            return MYLITE_ERROR;
        }
        optional_clause = optional_clause->next_sibling;
    }

    if (from_clause == NULL || from_clause->kind == MYLITE_SQL_AST_FROM_DUAL) {
        set_unsupported_error(database, "MIN/MAX supports only descriptor-backed table reads");
        return MYLITE_ERROR;
    }
    if (from_clause->kind != MYLITE_SQL_AST_FROM_TABLE) {
        set_unsupported_error(database, "MIN/MAX supports only descriptor-backed table reads");
        return MYLITE_ERROR;
    }

    rc = resolve_table_name(database, child_at(from_clause, 0U), &out_plan->source);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->source.table_name)) {
        set_reserved_name_error(database, "table", out_plan->source.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = resolve_readable_base_table(database, &out_plan->source, &out_plan->table);
    }
    if (rc == MYLITE_OK) {
        rc = init_select_source_context(database, from_clause, &out_plan->source, &source_context);
    }
    if (rc == MYLITE_OK) {
        rc = load_table_columns(
            database,
            out_plan->table.table_id,
            &table_columns,
            &table_column_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_min_max_aggregate_column(
            database,
            expression,
            &source_context,
            table_columns,
            table_column_count,
            &out_plan->aggregate_column
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_select_predicate(
            database,
            where_clause,
            &source_context,
            table_columns,
            table_column_count,
            &out_plan->predicate
        );
    }

    free(table_columns);
    return rc;
}

static enum planned_min_max_aggregate_function min_max_aggregate_function_from_expression(
    const struct mylite_sql_ast_node *expression
) {
    if (expression == NULL) {
        return PLANNED_MIN_MAX_AGGREGATE_NONE;
    }
    if (expression->kind == MYLITE_SQL_AST_MIN_AGGREGATE_FUNCTION) {
        return PLANNED_MIN_MAX_AGGREGATE_MIN;
    }
    if (expression->kind == MYLITE_SQL_AST_MAX_AGGREGATE_FUNCTION) {
        return PLANNED_MIN_MAX_AGGREGATE_MAX;
    }

    return PLANNED_MIN_MAX_AGGREGATE_NONE;
}

static int plan_min_max_aggregate_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column
) {
    const struct mylite_sql_ast_node *column_node = child_at(function, 0U);
    struct integer_column_range range;
    int rc = MYLITE_OK;

    rc = resolve_descriptor_column_reference(
        database,
        column_node,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "MIN/MAX supports only descriptor columns",
        table_columns,
        table_column_count,
        out_column
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = integer_range_for_column(
        database,
        out_column,
        "MIN/MAX supports only integer descriptor columns",
        &range
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    return MYLITE_OK;
}

static int execute_min_max_aggregate_from_plan(
    struct mylite_db *database,
    const struct planned_min_max_aggregate *plan,
    mylite_result **out_result
) {
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = append_min_max_aggregate_result_column(database, result, plan);
    if (rc == MYLITE_OK) {
        rc = read_min_max_aggregate_from_source(database, plan, result);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return min_max_aggregate_execution_error(database, rc);
    }

    return finish_successful_result(database, result, out_result);
}

static int append_min_max_aggregate_result_column(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_min_max_aggregate *plan
) {
    char *column_name = NULL;
    int rc = MYLITE_OK;

    if (plan->alias != NULL) {
        rc = copy_select_item_alias_text(database, plan->alias, &column_name);
    } else {
        rc = copy_aggregate_result_column_name(database, &plan->expression->span, &column_name);
    }

    if (rc == MYLITE_OK) {
        rc = mylite_result_append_column(result, column_name);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }

    free(column_name);
    return rc;
}

static int read_min_max_aggregate_from_source(
    struct mylite_db *database,
    const struct planned_min_max_aggregate *plan,
    mylite_result *result
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int rc = build_min_max_aggregate_sql(plan, &sql);

    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_min_max_aggregate_parameters(statement, plan);
    }
    if (rc == MYLITE_OK) {
        rc = step_min_max_aggregate_statement(statement, result);
    }

    rc = finalize_sqlite_statement(statement, rc);
    free(sql);

    return rc;
}

static int step_min_max_aggregate_statement(sqlite3_stmt *statement, mylite_result *result) {
    int sqlite_rc = sqlite3_step(statement);
    int rc = MYLITE_OK;

    if (sqlite_rc != SQLITE_ROW) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    rc = append_selected_sqlite_row(statement, result);
    if (rc != MYLITE_OK) {
        return rc;
    }
    sqlite_rc = sqlite3_step(statement);
    if (sqlite_rc != SQLITE_DONE) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    return MYLITE_OK;
}

static int min_max_aggregate_execution_error(struct mylite_db *database, int rc) {
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
        return rc;
    }
    if (mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) != MYLITE_OK) {
        return rc;
    }

    set_physical_sqlite_row_error(database);
    return MYLITE_ERROR;
}

static bool select_statement_is_session_scalar_projection(
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *from_clause = child_at(statement, 1U);
    const struct mylite_sql_ast_node *select_item = NULL;

    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        return false;
    }
    if (from_clause != NULL && from_clause->kind != MYLITE_SQL_AST_FROM_DUAL) {
        return false;
    }
    if (child_at(statement, 2U) != NULL) {
        return false;
    }

    select_item = child_at(select_list, 0U);
    if (select_item == NULL) {
        return false;
    }
    while (select_item != NULL) {
        if (select_item->kind != MYLITE_SQL_AST_SELECT_ITEM ||
            !is_session_scalar_expression(child_at(select_item, 0U))) {
            return false;
        }
        select_item = select_item->next_sibling;
    }

    return true;
}

static bool select_statement_is_literal_projection(const struct mylite_sql_ast_node *statement) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *from_clause = child_at(statement, 1U);
    const struct mylite_sql_ast_node *select_item = NULL;

    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        return false;
    }
    if (from_clause != NULL && from_clause->kind != MYLITE_SQL_AST_FROM_DUAL) {
        return false;
    }
    if (child_at(statement, 2U) != NULL) {
        return false;
    }

    select_item = child_at(select_list, 0U);
    if (select_item == NULL) {
        return false;
    }
    while (select_item != NULL) {
        if (select_item->kind != MYLITE_SQL_AST_SELECT_ITEM ||
            !is_literal_projection_expression(child_at(select_item, 0U))) {
            return false;
        }
        select_item = select_item->next_sibling;
    }

    return true;
}

static bool select_statement_has_no_source_or_dual(const struct mylite_sql_ast_node *statement) {
    const struct mylite_sql_ast_node *from_clause = child_at(statement, 1U);

    if (from_clause == NULL) {
        return true;
    }
    if (from_clause->kind == MYLITE_SQL_AST_FROM_DUAL) {
        return true;
    }
    return false;
}

static bool select_statement_is_literal_projection_attempt(
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *select_item = NULL;

    if (!select_statement_has_no_source_or_dual(statement)) {
        return false;
    }
    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST ||
        child_at(statement, 2U) != NULL) {
        return false;
    }

    select_item = child_at(select_list, 0U);
    if (select_item == NULL) {
        return false;
    }
    while (select_item != NULL) {
        if (select_item->kind != MYLITE_SQL_AST_SELECT_ITEM ||
            !is_literal_projection_attempt_expression(child_at(select_item, 0U))) {
            return false;
        }
        select_item = select_item->next_sibling;
    }

    return true;
}

static int execute_scalar_projection_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *select_item = child_at(select_list, 0U);
    struct session_scalar_cell *cells = NULL;
    const char **values = NULL;
    mylite_result *result = NULL;
    size_t column_count = mylite_sql_ast_node_child_count(select_list);
    size_t column_index = 0U;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }
    if (column_count > SIZE_MAX / sizeof(*values) || column_count > SIZE_MAX / sizeof(*cells)) {
        mylite_result_free(result);
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    cells = (struct session_scalar_cell *)calloc(column_count, sizeof(*cells));
    if (cells == NULL) {
        mylite_result_free(result);
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    values = (const char **)calloc(column_count, sizeof(*values));
    if (values == NULL) {
        free(cells);
        mylite_result_free(result);
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = append_session_scalar_select_warnings(database, select_list);
    while (rc == MYLITE_OK && select_item != NULL) {
        const struct mylite_sql_ast_node *expression = child_at(select_item, 0U);
        const struct mylite_sql_ast_node *alias = child_at(select_item, 1U);
        char *column_name = NULL;

        if (alias != NULL) {
            rc = copy_select_item_alias_text(database, alias, &column_name);
        } else {
            rc = copy_scalar_projection_column_name(database, expression, &column_name);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_result_append_column(result, column_name);
            if (rc != MYLITE_OK) {
                set_nomem_error(database);
            }
        }
        free(column_name);
        if (rc == MYLITE_OK) {
            rc = session_scalar_value(database, expression, &cells[column_index]);
        }
        if (rc == MYLITE_OK) {
            values[column_index] = cells[column_index].value;
        }
        ++column_index;
        select_item = select_item->next_sibling;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    free((void *)values);
    free(cells);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int copy_scalar_projection_column_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char **out_text
) {
    if (out_text == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    if (expression == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION &&
        mylite_sql_ast_node_operator(expression) == MYLITE_SQL_AST_OPERATOR_POSITIVE) {
        const struct mylite_sql_ast_node *literal = child_at(expression, 0U);

        if (literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL &&
            mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_INTEGER) {
            return copy_source_span_text(database, &literal->span, out_text);
        }
    }

    return copy_source_span_text(database, &expression->span, out_text);
}

static int append_session_scalar_select_warnings(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list
) {
    const struct mylite_sql_ast_node *select_item = child_at(select_list, 0U);
    int rc = MYLITE_OK;

    while (rc == MYLITE_OK && select_item != NULL) {
        const struct mylite_sql_ast_node *expression =
            unwrap_parenthesized_expression(child_at(select_item, 0U));
        enum session_system_variable_kind variable = SESSION_SYSTEM_VARIABLE_NONE;

        if (expression != NULL && expression->kind == MYLITE_SQL_AST_SYSTEM_VARIABLE) {
            rc = resolve_session_system_variable(database, expression, &variable);
            if (rc == MYLITE_OK && system_variable_kind_warns_on_scalar_read(variable)) {
                rc = append_system_variable_read_warning(database, variable);
            }
        }
        select_item = select_item->next_sibling;
    }

    return rc;
}

static const char *select_statement_argument_count_error_function(
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *select_item = NULL;
    const char *function_name = NULL;

    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        return NULL;
    }

    select_item = child_at(select_list, 0U);
    while (select_item != NULL) {
        const struct mylite_sql_ast_node *expression = child_at(select_item, 0U);

        expression = unwrap_parenthesized_expression(expression);
        function_name = argument_count_error_function_name(expression);
        if (function_name != NULL) {
            return function_name;
        }
        select_item = select_item->next_sibling;
    }

    return NULL;
}

static const char *argument_count_error_function_name(
    const struct mylite_sql_ast_node *expression
) {
    if (expression == NULL) {
        return NULL;
    }
    if (expression->kind == MYLITE_SQL_AST_CONNECTION_ID_ARGUMENT_COUNT_ERROR) {
        return "CONNECTION_ID";
    }
    if (expression->kind == MYLITE_SQL_AST_VERSION_ARGUMENT_COUNT_ERROR) {
        return "VERSION";
    }
    if (expression->kind == MYLITE_SQL_AST_CURRENT_ROLE_ARGUMENT_COUNT_ERROR) {
        return "CURRENT_ROLE";
    }

    return NULL;
}

static int session_scalar_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    expression = unwrap_parenthesized_expression(expression);
    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    if (expression == NULL) {
        return MYLITE_OK;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_DATABASE_FUNCTION:
    case MYLITE_SQL_AST_SCHEMA_FUNCTION:
        if (database->session.has_selected_schema) {
            out_cell->value = database->session.selected_schema;
        }
        return MYLITE_OK;
    case MYLITE_SQL_AST_USER_FUNCTION:
    case MYLITE_SQL_AST_SESSION_USER_FUNCTION:
    case MYLITE_SQL_AST_SYSTEM_USER_FUNCTION:
        out_cell->value = database->session.client_user_identity;
        return MYLITE_OK;
    case MYLITE_SQL_AST_CURRENT_USER_FUNCTION:
        out_cell->value = database->session.current_user_identity;
        return MYLITE_OK;
    case MYLITE_SQL_AST_CURRENT_ROLE_FUNCTION:
        out_cell->value = "NONE";
        return MYLITE_OK;
    case MYLITE_SQL_AST_CONNECTION_ID_FUNCTION: {
        int written = snprintf(
            out_cell->integer_text,
            sizeof(out_cell->integer_text),
            "%" PRIu64,
            database->session.connection_id
        );
        if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
            set_runtime_error(database, "failed to format CONNECTION_ID() value");
            return MYLITE_ERROR;
        }
        out_cell->value = out_cell->integer_text;
        return MYLITE_OK;
    }
    case MYLITE_SQL_AST_VERSION_FUNCTION:
        out_cell->value = mylite_version();
        return MYLITE_OK;
    case MYLITE_SQL_AST_ROW_COUNT_FUNCTION: {
        int written = snprintf(
            out_cell->integer_text,
            sizeof(out_cell->integer_text),
            "%" PRId64,
            database->session.previous_row_count
        );
        if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
            set_runtime_error(database, "failed to format ROW_COUNT() value");
            return MYLITE_ERROR;
        }
        out_cell->value = out_cell->integer_text;
        return MYLITE_OK;
    }
    case MYLITE_SQL_AST_LAST_INSERT_ID_FUNCTION: {
        int written = snprintf(
            out_cell->integer_text,
            sizeof(out_cell->integer_text),
            "%" PRIu64,
            database->session.last_insert_id
        );
        if (written < 0 || (size_t)written >= sizeof(out_cell->integer_text)) {
            set_runtime_error(database, "failed to format LAST_INSERT_ID() value");
            return MYLITE_ERROR;
        }
        out_cell->value = out_cell->integer_text;
        return MYLITE_OK;
    }
    case MYLITE_SQL_AST_LITERAL:
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
        return literal_projection_value(database, expression, out_cell);
    case MYLITE_SQL_AST_SYSTEM_VARIABLE:
        return system_variable_value(database, expression, out_cell);
    default:
        return MYLITE_OK;
    }
}

static int literal_projection_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    const struct mylite_sql_ast_node *literal = expression;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    bool has_sign = false;
    bool is_negative = false;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "SELECT literal projection supports only integer, boolean, and NULL literals"
        );
        return MYLITE_ERROR;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);

        has_sign = true;
        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_unsupported_error(
                database,
                "SELECT literal projection supports only signed integer literals"
            );
            return MYLITE_ERROR;
        }
        literal = child_at(expression, 0U);
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "SELECT literal projection supports only integer, boolean, and NULL literals"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (has_sign && literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_unsupported_error(
            database,
            "SELECT literal projection supports only signed integer literals"
        );
        return MYLITE_ERROR;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        out_cell->value = NULL;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        out_cell->value = "1";
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        out_cell->value = "0";
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_unsupported_error(
            database,
            "SELECT literal projection supports only integer, boolean, and NULL literals"
        );
        return MYLITE_ERROR;
    }

    int rc = normalize_decimal_integer_literal(
        database,
        &literal->span,
        is_negative,
        out_cell->literal_text,
        sizeof(out_cell->literal_text)
    );

    if (rc == MYLITE_OK) {
        out_cell->value = out_cell->literal_text;
    }
    return rc;
}

static int normalize_decimal_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    bool is_negative,
    char *buffer,
    size_t buffer_size
) {
    size_t first_significant = 0U;
    size_t significant_digit_count = 0U;
    size_t output_offset = 0U;
    size_t required_size = 0U;

    if (span == NULL || span->text == NULL || span->length == 0U || buffer == NULL ||
        buffer_size == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    first_significant = span->length;
    for (size_t index = 0U; index < span->length; ++index) {
        unsigned char byte = (unsigned char)span->text[index];

        if (byte < '0' || byte > '9') {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        if (byte != '0' && first_significant == span->length) {
            first_significant = index;
        }
    }
    if (first_significant == span->length) {
        if (buffer_size < sizeof("0")) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        buffer[0] = '0';
        buffer[1] = '\0';
        return MYLITE_OK;
    }

    significant_digit_count = span->length - first_significant;
    if (significant_digit_count > literal_projection_max_significant_digits) {
        set_unsupported_error(
            database,
            "SELECT literal projection supports at most 81 significant decimal digits"
        );
        return MYLITE_ERROR;
    }
    required_size = significant_digit_count + 1U;
    if (is_negative) {
        ++required_size;
    }
    if (required_size > buffer_size) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (is_negative) {
        buffer[output_offset] = '-';
        ++output_offset;
    }
    memcpy(&buffer[output_offset], &span->text[first_significant], significant_digit_count);
    output_offset += significant_digit_count;
    buffer[output_offset] = '\0';

    return MYLITE_OK;
}

static int system_variable_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    enum session_system_variable_kind variable = SESSION_SYSTEM_VARIABLE_NONE;
    const struct mylite_diagnostics *count_diagnostics = NULL;
    uint64_t count = 0U;
    uint64_t error_count = 0U;
    int rc = resolve_session_system_variable(database, expression, &variable);

    if (rc != MYLITE_OK) {
        return rc;
    }

    switch (variable) {
    case SESSION_SYSTEM_VARIABLE_CHARACTER_SET_CLIENT:
        out_cell->value = database->session.character_set_client;
        return MYLITE_OK;
    case SESSION_SYSTEM_VARIABLE_CHARACTER_SET_CONNECTION:
        out_cell->value = database->session.character_set_connection;
        return MYLITE_OK;
    case SESSION_SYSTEM_VARIABLE_CHARACTER_SET_RESULTS:
        out_cell->value = database->session.character_set_results;
        return MYLITE_OK;
    case SESSION_SYSTEM_VARIABLE_COLLATION_CONNECTION:
        out_cell->value = database->session.collation_connection;
        return MYLITE_OK;
    case SESSION_SYSTEM_VARIABLE_CHARACTER_SET_SERVER:
        out_cell->value = "utf8mb4";
        return MYLITE_OK;
    case SESSION_SYSTEM_VARIABLE_COLLATION_SERVER:
        out_cell->value = "utf8mb4_0900_ai_ci";
        return MYLITE_OK;
    case SESSION_SYSTEM_VARIABLE_CHARACTER_SET_DATABASE:
        out_cell->value = "utf8mb4";
        return MYLITE_OK;
    case SESSION_SYSTEM_VARIABLE_COLLATION_DATABASE:
        out_cell->value = "utf8mb4_0900_ai_ci";
        return MYLITE_OK;
    case SESSION_SYSTEM_VARIABLE_DEFAULT_STORAGE_ENGINE:
        out_cell->value = "InnoDB";
        return MYLITE_OK;
    case SESSION_SYSTEM_VARIABLE_UPDATABLE_VIEWS_WITH_LIMIT:
        out_cell->value = "YES";
        return MYLITE_OK;
    case SESSION_SYSTEM_VARIABLE_CHARACTER_SET_SYSTEM:
        out_cell->value = "utf8mb3";
        return MYLITE_OK;
    case SESSION_SYSTEM_VARIABLE_CHARACTER_SET_FILESYSTEM:
        out_cell->value = "binary";
        return MYLITE_OK;
    case SESSION_SYSTEM_VARIABLE_SQL_MODE:
        out_cell->value = default_sql_mode_value();
        return MYLITE_OK;
    case SESSION_SYSTEM_VARIABLE_AUTOCOMMIT:
    case SESSION_SYSTEM_VARIABLE_SQL_QUOTE_SHOW_CREATE:
    case SESSION_SYSTEM_VARIABLE_FOREIGN_KEY_CHECKS:
    case SESSION_SYSTEM_VARIABLE_UNIQUE_CHECKS:
    case SESSION_SYSTEM_VARIABLE_SQL_NOTES:
    case SESSION_SYSTEM_VARIABLE_SQL_BIG_SELECTS:
    case SESSION_SYSTEM_VARIABLE_SQL_LOG_BIN:
        rc = format_uint64(database, 1U, out_cell->integer_text, sizeof(out_cell->integer_text));
        if (rc == MYLITE_OK) {
            out_cell->value = out_cell->integer_text;
        }
        return rc;
    case SESSION_SYSTEM_VARIABLE_SQL_SAFE_UPDATES:
    case SESSION_SYSTEM_VARIABLE_SQL_WARNINGS:
    case SESSION_SYSTEM_VARIABLE_SQL_BUFFER_RESULT:
    case SESSION_SYSTEM_VARIABLE_SQL_AUTO_IS_NULL:
    case SESSION_SYSTEM_VARIABLE_SQL_GENERATE_INVISIBLE_PRIMARY_KEY:
    case SESSION_SYSTEM_VARIABLE_SQL_LOG_OFF:
    case SESSION_SYSTEM_VARIABLE_SQL_REPLICA_SKIP_COUNTER:
    case SESSION_SYSTEM_VARIABLE_SQL_REQUIRE_PRIMARY_KEY:
    case SESSION_SYSTEM_VARIABLE_SQL_SLAVE_SKIP_COUNTER:
        rc = format_uint64(database, 0U, out_cell->integer_text, sizeof(out_cell->integer_text));
        if (rc == MYLITE_OK) {
            out_cell->value = out_cell->integer_text;
        }
        return rc;
    case SESSION_SYSTEM_VARIABLE_SQL_SELECT_LIMIT:
        rc = format_uint64(
            database,
            UINT64_MAX,
            out_cell->integer_text,
            sizeof(out_cell->integer_text)
        );
        if (rc == MYLITE_OK) {
            out_cell->value = out_cell->integer_text;
        }
        return rc;
    case SESSION_SYSTEM_VARIABLE_VERSION:
        out_cell->value = mylite_version();
        return MYLITE_OK;
    case SESSION_SYSTEM_VARIABLE_VERSION_COMMENT:
        out_cell->value = "MyLite";
        return MYLITE_OK;
    default:
        break;
    }

    count_diagnostics = system_variable_count_diagnostics(database);
    rc = previous_diagnostics_condition_count(count_diagnostics, &error_count);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (variable == SESSION_SYSTEM_VARIABLE_ERROR_COUNT) {
        count = error_count;
    } else {
        count = error_count + (uint64_t)mylite_diagnostics_warning_count(count_diagnostics);
    }

    rc = format_uint64(database, count, out_cell->integer_text, sizeof(out_cell->integer_text));
    if (rc == MYLITE_OK) {
        out_cell->value = out_cell->integer_text;
    }
    return rc;
}

static const char *default_sql_mode_value(void) {
    return "ONLY_FULL_GROUP_BY,STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,"
           "ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION";
}

static const struct mylite_diagnostics *system_variable_count_diagnostics(
    const struct mylite_db *database
) {
    if (database == NULL) {
        return NULL;
    }
    if (mylite_diagnostics_errcode(&database->diagnostics) != MYLITE_OK ||
        mylite_diagnostics_warning_count(&database->diagnostics) > 0U) {
        return &database->diagnostics;
    }
    return &database->previous_diagnostics;
}

static int resolve_session_system_variable(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum session_system_variable_kind *out_kind
) {
    const struct mylite_sql_source_span *span = expression == NULL ? NULL : &expression->span;
    struct system_variable_component first = {0};
    struct system_variable_component second = {0};
    const struct system_variable_component *name = &first;
    size_t offset = 2U;
    bool has_scope = false;
    int rc = MYLITE_OK;

    *out_kind = SESSION_SYSTEM_VARIABLE_NONE;
    if (span == NULL || span->text == NULL || span->length < 3U || span->text[0] != '@' ||
        span->text[1] != '@') {
        set_unknown_system_variable_error(database, expression);
        return MYLITE_ERROR;
    }

    rc = parse_system_variable_component(database, span, &offset, &first);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (offset < span->length && span->text[offset] == '.') {
        has_scope = true;
        ++offset;
        if (first.quoted) {
            set_unsupported_error(database, "unsupported quoted system variable scope");
            return MYLITE_ERROR;
        }
        rc = parse_system_variable_component(database, span, &offset, &second);
        if (rc != MYLITE_OK) {
            return rc;
        }
        name = &second;
    }
    if (offset != span->length || system_variable_component_is_empty(name)) {
        set_unknown_system_variable_error(database, expression);
        return MYLITE_ERROR;
    }

    if (!resolve_system_variable_kind(name, out_kind)) {
        set_unknown_system_variable_error(database, expression);
        return MYLITE_ERROR;
    }

    if (!has_scope) {
        return MYLITE_OK;
    }
    if (system_variable_component_equals(&first, "global")) {
        if (system_variable_kind_allows_global_scope(*out_kind)) {
            return MYLITE_OK;
        }
        set_session_variable_only_error(database, name->text);
        return MYLITE_ERROR;
    }
    if (system_variable_component_equals(&first, "session") ||
        system_variable_component_equals(&first, "local")) {
        if (!system_variable_kind_allows_session_scope(*out_kind)) {
            set_global_variable_only_error(database, name->text);
            return MYLITE_ERROR;
        }
        return MYLITE_OK;
    }

    set_unknown_system_variable_error(database, expression);
    return MYLITE_ERROR;
}

static bool resolve_system_variable_kind(
    const struct system_variable_component *name,
    enum session_system_variable_kind *out_kind
) {
    struct system_variable_name {
        const char *name;
        enum session_system_variable_kind kind;
    };
    static const struct system_variable_name names[] = {
        {"warning_count", SESSION_SYSTEM_VARIABLE_WARNING_COUNT},
        {"error_count", SESSION_SYSTEM_VARIABLE_ERROR_COUNT},
        {"character_set_client", SESSION_SYSTEM_VARIABLE_CHARACTER_SET_CLIENT},
        {"character_set_connection", SESSION_SYSTEM_VARIABLE_CHARACTER_SET_CONNECTION},
        {"character_set_results", SESSION_SYSTEM_VARIABLE_CHARACTER_SET_RESULTS},
        {"collation_connection", SESSION_SYSTEM_VARIABLE_COLLATION_CONNECTION},
        {"character_set_server", SESSION_SYSTEM_VARIABLE_CHARACTER_SET_SERVER},
        {"collation_server", SESSION_SYSTEM_VARIABLE_COLLATION_SERVER},
        {"character_set_database", SESSION_SYSTEM_VARIABLE_CHARACTER_SET_DATABASE},
        {"collation_database", SESSION_SYSTEM_VARIABLE_COLLATION_DATABASE},
        {"default_storage_engine", SESSION_SYSTEM_VARIABLE_DEFAULT_STORAGE_ENGINE},
        {"character_set_system", SESSION_SYSTEM_VARIABLE_CHARACTER_SET_SYSTEM},
        {"character_set_filesystem", SESSION_SYSTEM_VARIABLE_CHARACTER_SET_FILESYSTEM},
        {"autocommit", SESSION_SYSTEM_VARIABLE_AUTOCOMMIT},
        {"sql_quote_show_create", SESSION_SYSTEM_VARIABLE_SQL_QUOTE_SHOW_CREATE},
        {"foreign_key_checks", SESSION_SYSTEM_VARIABLE_FOREIGN_KEY_CHECKS},
        {"unique_checks", SESSION_SYSTEM_VARIABLE_UNIQUE_CHECKS},
        {"updatable_views_with_limit", SESSION_SYSTEM_VARIABLE_UPDATABLE_VIEWS_WITH_LIMIT},
        {"sql_auto_is_null", SESSION_SYSTEM_VARIABLE_SQL_AUTO_IS_NULL},
        {"sql_big_selects", SESSION_SYSTEM_VARIABLE_SQL_BIG_SELECTS},
        {"sql_buffer_result", SESSION_SYSTEM_VARIABLE_SQL_BUFFER_RESULT},
        {"sql_generate_invisible_primary_key",
         SESSION_SYSTEM_VARIABLE_SQL_GENERATE_INVISIBLE_PRIMARY_KEY},
        {"sql_log_bin", SESSION_SYSTEM_VARIABLE_SQL_LOG_BIN},
        {"sql_log_off", SESSION_SYSTEM_VARIABLE_SQL_LOG_OFF},
        {"sql_mode", SESSION_SYSTEM_VARIABLE_SQL_MODE},
        {"sql_replica_skip_counter", SESSION_SYSTEM_VARIABLE_SQL_REPLICA_SKIP_COUNTER},
        {"sql_require_primary_key", SESSION_SYSTEM_VARIABLE_SQL_REQUIRE_PRIMARY_KEY},
        {"sql_safe_updates", SESSION_SYSTEM_VARIABLE_SQL_SAFE_UPDATES},
        {"sql_select_limit", SESSION_SYSTEM_VARIABLE_SQL_SELECT_LIMIT},
        {"sql_slave_skip_counter", SESSION_SYSTEM_VARIABLE_SQL_SLAVE_SKIP_COUNTER},
        {"sql_notes", SESSION_SYSTEM_VARIABLE_SQL_NOTES},
        {"sql_warnings", SESSION_SYSTEM_VARIABLE_SQL_WARNINGS},
        {"version", SESSION_SYSTEM_VARIABLE_VERSION},
        {"version_comment", SESSION_SYSTEM_VARIABLE_VERSION_COMMENT},
    };

    for (size_t index = 0U; index < sizeof(names) / sizeof(names[0]); ++index) {
        if (system_variable_component_equals(name, names[index].name)) {
            *out_kind = names[index].kind;
            return true;
        }
    }

    return false;
}

static bool system_variable_kind_allows_global_scope(enum session_system_variable_kind kind) {
    switch (kind) {
    case SESSION_SYSTEM_VARIABLE_CHARACTER_SET_CLIENT:
    case SESSION_SYSTEM_VARIABLE_CHARACTER_SET_CONNECTION:
    case SESSION_SYSTEM_VARIABLE_CHARACTER_SET_RESULTS:
    case SESSION_SYSTEM_VARIABLE_COLLATION_CONNECTION:
    case SESSION_SYSTEM_VARIABLE_CHARACTER_SET_SERVER:
    case SESSION_SYSTEM_VARIABLE_COLLATION_SERVER:
    case SESSION_SYSTEM_VARIABLE_CHARACTER_SET_DATABASE:
    case SESSION_SYSTEM_VARIABLE_COLLATION_DATABASE:
    case SESSION_SYSTEM_VARIABLE_DEFAULT_STORAGE_ENGINE:
    case SESSION_SYSTEM_VARIABLE_CHARACTER_SET_SYSTEM:
    case SESSION_SYSTEM_VARIABLE_CHARACTER_SET_FILESYSTEM:
    case SESSION_SYSTEM_VARIABLE_AUTOCOMMIT:
    case SESSION_SYSTEM_VARIABLE_SQL_QUOTE_SHOW_CREATE:
    case SESSION_SYSTEM_VARIABLE_FOREIGN_KEY_CHECKS:
    case SESSION_SYSTEM_VARIABLE_UNIQUE_CHECKS:
    case SESSION_SYSTEM_VARIABLE_UPDATABLE_VIEWS_WITH_LIMIT:
    case SESSION_SYSTEM_VARIABLE_SQL_AUTO_IS_NULL:
    case SESSION_SYSTEM_VARIABLE_SQL_BIG_SELECTS:
    case SESSION_SYSTEM_VARIABLE_SQL_BUFFER_RESULT:
    case SESSION_SYSTEM_VARIABLE_SQL_GENERATE_INVISIBLE_PRIMARY_KEY:
    case SESSION_SYSTEM_VARIABLE_SQL_LOG_OFF:
    case SESSION_SYSTEM_VARIABLE_SQL_MODE:
    case SESSION_SYSTEM_VARIABLE_SQL_REPLICA_SKIP_COUNTER:
    case SESSION_SYSTEM_VARIABLE_SQL_REQUIRE_PRIMARY_KEY:
    case SESSION_SYSTEM_VARIABLE_SQL_SAFE_UPDATES:
    case SESSION_SYSTEM_VARIABLE_SQL_SELECT_LIMIT:
    case SESSION_SYSTEM_VARIABLE_SQL_SLAVE_SKIP_COUNTER:
    case SESSION_SYSTEM_VARIABLE_SQL_NOTES:
    case SESSION_SYSTEM_VARIABLE_SQL_WARNINGS:
    case SESSION_SYSTEM_VARIABLE_VERSION:
    case SESSION_SYSTEM_VARIABLE_VERSION_COMMENT:
        return true;
    default:
        return false;
    }
}

static bool system_variable_kind_allows_session_scope(enum session_system_variable_kind kind) {
    switch (kind) {
    case SESSION_SYSTEM_VARIABLE_CHARACTER_SET_SYSTEM:
    case SESSION_SYSTEM_VARIABLE_SQL_REPLICA_SKIP_COUNTER:
    case SESSION_SYSTEM_VARIABLE_SQL_SLAVE_SKIP_COUNTER:
    case SESSION_SYSTEM_VARIABLE_VERSION:
    case SESSION_SYSTEM_VARIABLE_VERSION_COMMENT:
        return false;
    default:
        return true;
    }
}

static bool system_variable_kind_warns_on_scalar_read(enum session_system_variable_kind kind) {
    return kind == SESSION_SYSTEM_VARIABLE_SQL_SLAVE_SKIP_COUNTER;
}

static int append_system_variable_read_warning(
    struct mylite_db *database,
    enum session_system_variable_kind kind
) {
    if (kind == SESSION_SYSTEM_VARIABLE_SQL_SLAVE_SKIP_COUNTER) {
        return mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_deprecated_system_variable,
            "HY000",
            "'@@sql_slave_skip_counter' is deprecated and will be removed in a future release. "
            "Please use sql_replica_skip_counter instead."
        );
    }

    return MYLITE_OK;
}

static int parse_system_variable_component(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    size_t *offset,
    struct system_variable_component *out_component
) {
    size_t component_length = 0U;

    *out_component = (struct system_variable_component){0};
    if (*offset >= span->length) {
        return MYLITE_OK;
    }

    if (span->text[*offset] == '`') {
        out_component->quoted = true;
        ++*offset;
        while (*offset < span->length) {
            char value = span->text[*offset];

            if (value == '`') {
                ++*offset;
                if (*offset < span->length && span->text[*offset] == '`') {
                    int rc = append_quoted_system_variable_byte(
                        database,
                        out_component,
                        &component_length,
                        '`'
                    );
                    if (rc != MYLITE_OK) {
                        return rc;
                    }
                    ++*offset;
                    continue;
                }
                return MYLITE_OK;
            }

            {
                int rc = append_quoted_system_variable_byte(
                    database,
                    out_component,
                    &component_length,
                    value
                );
                if (rc != MYLITE_OK) {
                    return rc;
                }
            }
            ++*offset;
        }

        set_unsupported_error(database, "unterminated system variable identifier");
        return MYLITE_ERROR;
    }

    while (*offset < span->length && span->text[*offset] != '.') {
        if (component_length + 1U >= sizeof(out_component->text)) {
            set_unknown_system_variable_error(database, NULL);
            return MYLITE_ERROR;
        }
        out_component->text[component_length] = span->text[*offset];
        ++component_length;
        ++*offset;
    }
    out_component->text[component_length] = '\0';

    return MYLITE_OK;
}

static int append_quoted_system_variable_byte(
    struct mylite_db *database,
    struct system_variable_component *component,
    size_t *component_length,
    char value
) {
    if (*component_length + 1U >= sizeof(component->text)) {
        set_unknown_system_variable_error(database, NULL);
        return MYLITE_ERROR;
    }

    component->text[*component_length] = value;
    ++*component_length;
    component->text[*component_length] = '\0';
    return MYLITE_OK;
}

static bool system_variable_component_equals(
    const struct system_variable_component *component,
    const char *expected
) {
    return text_equals_ascii_case_insensitive(component == NULL ? NULL : component->text, expected);
}

static bool system_variable_component_is_empty(const struct system_variable_component *component) {
    if (component == NULL) {
        return true;
    }
    return component->text[0] == '\0';
}

static bool is_session_scalar_expression(const struct mylite_sql_ast_node *expression) {
    expression = unwrap_parenthesized_expression(expression);

    if (expression == NULL) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_DATABASE_FUNCTION) {
        return true;
    }
    if (expression->kind == MYLITE_SQL_AST_SCHEMA_FUNCTION) {
        return true;
    }
    if (expression->kind == MYLITE_SQL_AST_USER_FUNCTION) {
        return true;
    }
    if (expression->kind == MYLITE_SQL_AST_SESSION_USER_FUNCTION) {
        return true;
    }
    if (expression->kind == MYLITE_SQL_AST_SYSTEM_USER_FUNCTION) {
        return true;
    }
    if (expression->kind == MYLITE_SQL_AST_CURRENT_USER_FUNCTION) {
        return true;
    }
    if (expression->kind == MYLITE_SQL_AST_CURRENT_ROLE_FUNCTION) {
        return true;
    }
    if (expression->kind == MYLITE_SQL_AST_CONNECTION_ID_FUNCTION) {
        return true;
    }
    if (expression->kind == MYLITE_SQL_AST_VERSION_FUNCTION) {
        return true;
    }
    if (expression->kind == MYLITE_SQL_AST_ROW_COUNT_FUNCTION) {
        return true;
    }
    if (expression->kind == MYLITE_SQL_AST_LAST_INSERT_ID_FUNCTION) {
        return true;
    }
    if (expression->kind == MYLITE_SQL_AST_SYSTEM_VARIABLE) {
        return true;
    }

    return false;
}

static bool is_literal_projection_expression(const struct mylite_sql_ast_node *expression) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    if (expression == NULL) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);
        const struct mylite_sql_ast_node *literal = child_at(expression, 0U);

        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            return false;
        }
        if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
            return false;
        }
        if (mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_INTEGER) {
            return true;
        }
        return false;
    }
    if (expression->kind != MYLITE_SQL_AST_LITERAL) {
        return false;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(expression);
    switch (literal_kind) {
    case MYLITE_SQL_AST_LITERAL_INTEGER:
    case MYLITE_SQL_AST_LITERAL_NULL:
    case MYLITE_SQL_AST_LITERAL_TRUE:
    case MYLITE_SQL_AST_LITERAL_FALSE:
        return true;
    default:
        return false;
    }
}

static bool is_literal_projection_attempt_expression(const struct mylite_sql_ast_node *expression) {
    if (expression == NULL) {
        return false;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_LITERAL:
        return true;
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
        return is_literal_projection_attempt_operand(expression);
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
        if (!is_literal_projection_attempt_operand(child_at(expression, 0U))) {
            return false;
        }
        return is_literal_projection_attempt_operand(child_at(expression, 1U));
    case MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION:
        return is_literal_projection_attempt_operand(expression);
    default:
        return false;
    }
}

static bool is_literal_projection_attempt_operand(const struct mylite_sql_ast_node *expression) {
    if (expression == NULL) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL) {
        return true;
    }
    if (expression->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
        const struct mylite_sql_ast_node *child = child_at(expression, 0U);

        if (child != NULL && child->kind == MYLITE_SQL_AST_LITERAL) {
            return true;
        }
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(expression);
        const struct mylite_sql_ast_node *child = child_at(expression, 0U);

        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            return false;
        }
        if (child != NULL && child->kind == MYLITE_SQL_AST_LITERAL) {
            return true;
        }
        return false;
    }

    return false;
}

static const struct mylite_sql_ast_node *unwrap_parenthesized_expression(
    const struct mylite_sql_ast_node *expression
) {
    while (expression != NULL && expression->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
        expression = child_at(expression, 0U);
    }

    return expression;
}

static int copy_source_span_text(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    char **out_text
) {
    char *text = NULL;

    if (out_text == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    if (span == NULL || span->text == NULL || span->length == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (span->length == SIZE_MAX) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    text = (char *)malloc(span->length + 1U);
    if (text == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(text, span->text, span->length);
    text[span->length] = '\0';
    *out_text = text;

    return MYLITE_OK;
}

static int plan_delete(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_delete *out_plan
) {
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *order_clause = NULL;
    const struct mylite_sql_ast_node *limit_clause = NULL;
    const struct mylite_sql_ast_node *optional_clause = NULL;
    struct mylite_catalog_column_descriptor *table_columns = NULL;
    size_t table_column_count = 0U;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_delete){0};
    optional_clause = child_at(statement, 1U);
    while (optional_clause != NULL) {
        if (optional_clause->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
            where_clause = optional_clause;
        } else if (optional_clause->kind == MYLITE_SQL_AST_ORDER_BY_CLAUSE) {
            order_clause = optional_clause;
        } else if (optional_clause->kind == MYLITE_SQL_AST_LIMIT_CLAUSE) {
            limit_clause = optional_clause;
        } else {
            set_unsupported_error(database, "DELETE supports only WHERE, ORDER BY, and LIMIT");
            return MYLITE_ERROR;
        }
        optional_clause = optional_clause->next_sibling;
    }

    rc = resolve_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = resolve_readable_base_table(database, &out_plan->target, &out_plan->table);
    }
    if (rc == MYLITE_OK && (where_clause != NULL || order_clause != NULL || limit_clause != NULL)) {
        rc = load_table_columns(
            database,
            out_plan->table.table_id,
            &table_columns,
            &table_column_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_select_predicate(
            database,
            where_clause,
            NULL,
            table_columns,
            table_column_count,
            &out_plan->predicate
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_select_order(
            database,
            order_clause,
            NULL,
            NULL,
            table_columns,
            table_column_count,
            &out_plan->order
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_delete_limit(database, limit_clause, &out_plan->limit);
    }
    if (rc == MYLITE_OK && out_plan->limit.has_limit) {
        rc = choose_sqlite_rowid_alias(
            database,
            table_columns,
            table_column_count,
            "DELETE LIMIT requires an unshadowed SQLite rowid alias",
            &out_plan->rowid_alias
        );
    }

    free(table_columns);
    if (rc != MYLITE_OK) {
        planned_delete_deinit(out_plan);
    }

    return rc;
}

static void planned_delete_deinit(struct planned_delete *plan) {
    if (plan == NULL) {
        return;
    }

    *plan = (struct planned_delete){0};
}

static int execute_delete_from_plan(
    struct mylite_db *database,
    const struct planned_delete *plan,
    mylite_result *result
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    bool transaction_started = false;
    int64_t affected_rows = 0;
    int sqlite_rc = SQLITE_OK;
    int rc = build_delete_sql(plan, &sql);

    if (rc == MYLITE_OK) {
        rc = execute_sqlite_control_sql(database, "BEGIN IMMEDIATE");
    }
    if (rc == MYLITE_OK) {
        transaction_started = true;
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_delete_parameters(statement, plan);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            affected_rows = (int64_t)sqlite3_changes64(database->sqlite);
        } else {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finalize_sqlite_statement(statement, rc);
    statement = NULL;
    if (rc == MYLITE_OK) {
        rc = execute_sqlite_control_sql(database, "COMMIT");
        if (rc == MYLITE_OK) {
            transaction_started = false;
        }
    }
    if (rc != MYLITE_OK && transaction_started) {
        (void)execute_sqlite_control_sql(database, "ROLLBACK");
    }
    free(sql);

    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
            return rc;
        }
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }

    mylite_result_set_affected_rows(result, affected_rows);

    return MYLITE_OK;
}

static int resolve_table_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct table_name_resolution *out_resolution
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t part_count = 0U;
    int rc = MYLITE_OK;

    *out_resolution = (struct table_name_resolution){0};
    rc = collect_identifier_parts(
        database == NULL ? NULL : node,
        parts,
        table_name_part_capacity,
        &part_count,
        database
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (part_count == 1U) {
        rc = resolve_selected_schema(database, &out_resolution->schema);
        if (rc == MYLITE_OK) {
            memcpy(out_resolution->table_name, parts[0], sizeof(out_resolution->table_name));
        }
        return rc;
    }
    if (part_count == 2U) {
        if (mylite_catalog_name_is_reserved(parts[0])) {
            set_reserved_name_error(database, "database", parts[0]);
            return MYLITE_ERROR;
        }
        rc = resolve_schema_name(database, parts[0], &out_resolution->schema);
        if (rc == MYLITE_OK) {
            memcpy(out_resolution->table_name, parts[1], sizeof(out_resolution->table_name));
        }
        return rc;
    }

    set_parse_error(database, NULL);

    return MYLITE_ERROR;
}

static int resolve_drop_if_exists_table_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct table_name_resolution *out_resolution,
    bool *out_missing_schema
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t part_count = 0U;
    int rc = MYLITE_OK;

    *out_resolution = (struct table_name_resolution){0};
    *out_missing_schema = false;
    rc = collect_identifier_parts(
        database == NULL ? NULL : node,
        parts,
        table_name_part_capacity,
        &part_count,
        database
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (part_count == 1U) {
        rc = resolve_selected_schema(database, &out_resolution->schema);
        if (rc == MYLITE_OK) {
            memcpy(out_resolution->table_name, parts[0], sizeof(out_resolution->table_name));
        }
        return rc;
    }
    if (part_count == 2U) {
        bool schema_found = false;

        if (mylite_catalog_name_is_reserved(parts[0])) {
            set_reserved_name_error(database, "database", parts[0]);
            return MYLITE_ERROR;
        }
        if (mylite_catalog_name_is_reserved(parts[1])) {
            set_reserved_name_error(database, "table", parts[1]);
            return MYLITE_ERROR;
        }
        rc = mylite_catalog_try_read_schema_by_name(
            database,
            parts[0],
            &out_resolution->schema,
            &schema_found
        );
        memcpy(out_resolution->table_name, parts[1], sizeof(out_resolution->table_name));
        if (rc != MYLITE_OK) {
            set_internal_error_if_clear(database, rc, "failed to read schema descriptor");
            return rc;
        }
        if (!schema_found) {
            memcpy(out_resolution->schema.name, parts[0], sizeof(out_resolution->schema.name));
            *out_missing_schema = true;
            return MYLITE_OK;
        }
        return MYLITE_OK;
    }

    set_parse_error(database, NULL);

    return MYLITE_ERROR;
}

static int resolve_show_columns_table_name(
    struct mylite_db *database,
    struct show_columns_target_nodes nodes,
    struct table_name_resolution *out_resolution
) {
    char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = MYLITE_OK;

    *out_resolution = (struct table_name_resolution){0};
    if (nodes.schema == NULL) {
        rc = resolve_table_name(database, nodes.table, out_resolution);
    } else {
        rc = copy_identifier_text(nodes.schema, schema_name, sizeof(schema_name), database);
        if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(schema_name)) {
            set_reserved_name_error(database, "database", schema_name);
            rc = MYLITE_ERROR;
        }
        if (rc == MYLITE_OK) {
            rc = resolve_schema_name(database, schema_name, &out_resolution->schema);
        }
        if (rc == MYLITE_OK) {
            rc = copy_show_columns_explicit_table_name(
                database,
                nodes.table,
                out_resolution->table_name,
                sizeof(out_resolution->table_name)
            );
        }
    }
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_resolution->table_name)) {
        set_reserved_name_error(database, "table", out_resolution->table_name);
        rc = MYLITE_ERROR;
    }

    return rc;
}

static int copy_show_columns_explicit_table_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node,
    char *destination,
    size_t destination_size
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t part_count = 0U;
    int rc = MYLITE_OK;

    if (destination == NULL || destination_size < MYLITE_CATALOG_IDENTIFIER_CAPACITY) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    rc = collect_identifier_parts(
        table_node,
        parts,
        table_name_part_capacity,
        &part_count,
        database
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (part_count == 0U || part_count > 2U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    memcpy(destination, parts[part_count - 1U], MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    return MYLITE_OK;
}

static int resolve_truncate_table_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct table_name_resolution *out_resolution
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t part_count = 0U;
    int rc = MYLITE_OK;

    *out_resolution = (struct table_name_resolution){0};
    rc = collect_identifier_parts(
        database == NULL ? NULL : node,
        parts,
        table_name_part_capacity,
        &part_count,
        database
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (part_count == 1U) {
        rc = resolve_selected_schema(database, &out_resolution->schema);
        if (rc == MYLITE_OK) {
            memcpy(out_resolution->table_name, parts[0], sizeof(out_resolution->table_name));
        }
        return rc;
    }
    if (part_count == 2U) {
        if (mylite_catalog_name_is_reserved(parts[0])) {
            set_reserved_name_error(database, "database", parts[0]);
            return MYLITE_ERROR;
        }
        rc = mylite_catalog_read_schema_by_name(database, parts[0], &out_resolution->schema);
        if (rc != MYLITE_OK) {
            memcpy(out_resolution->schema.name, parts[0], sizeof(out_resolution->schema.name));
            set_table_does_not_exist_error(database, parts[0], parts[1]);
            return MYLITE_ERROR;
        }
        memcpy(out_resolution->table_name, parts[1], sizeof(out_resolution->table_name));
        return MYLITE_OK;
    }

    set_parse_error(database, NULL);

    return MYLITE_ERROR;
}

static int require_selected_schema_for_unqualified_table_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t part_count = 0U;
    int rc = collect_identifier_parts(
        database == NULL ? NULL : node,
        parts,
        table_name_part_capacity,
        &part_count,
        database
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (part_count == 1U && (database == NULL || !database->session.has_selected_schema)) {
        set_no_database_error(database);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int resolve_schema_name(
    struct mylite_db *database,
    const char *schema_name,
    struct mylite_catalog_schema_descriptor *out_schema
) {
    int rc = mylite_catalog_read_schema_by_name(database, schema_name, out_schema);

    if (rc != MYLITE_OK) {
        set_unknown_database_error(database, schema_name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int resolve_selected_schema(
    struct mylite_db *database,
    struct mylite_catalog_schema_descriptor *out_schema
) {
    if (database == NULL || !database->session.has_selected_schema) {
        set_no_database_error(database);
        return MYLITE_ERROR;
    }

    return resolve_schema_name(database, database->session.selected_schema, out_schema);
}

static int collect_identifier_parts(
    const struct mylite_sql_ast_node *node,
    char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t part_capacity,
    size_t *part_count,
    struct mylite_db *database
) {
    const struct mylite_sql_ast_node *tail_nodes[table_name_part_capacity];
    const struct mylite_sql_ast_node *current = node;
    size_t tail_count = 0U;
    int rc = MYLITE_OK;

    if (current == NULL || part_count == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    while (current->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        if (tail_count >= part_capacity || tail_count >= table_name_part_capacity) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        tail_nodes[tail_count] = child_at(current, 1U);
        ++tail_count;
        current = child_at(current, 0U);
        if (current == NULL) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
    }

    if (current->kind != MYLITE_SQL_AST_IDENTIFIER) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (*part_count >= part_capacity) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    rc = copy_identifier_text(
        current,
        parts[*part_count],
        MYLITE_CATALOG_IDENTIFIER_CAPACITY,
        database
    );
    if (rc != MYLITE_OK) {
        return MYLITE_ERROR;
    }
    ++(*part_count);

    while (tail_count > 0U) {
        --tail_count;
        if (*part_count >= part_capacity) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        rc = copy_identifier_text(
            tail_nodes[tail_count],
            parts[*part_count],
            MYLITE_CATALOG_IDENTIFIER_CAPACITY,
            database
        );
        if (rc != MYLITE_OK) {
            return MYLITE_ERROR;
        }
        ++(*part_count);
    }

    return MYLITE_OK;
}

static int copy_identifier_text(
    const struct mylite_sql_ast_node *node,
    char *destination,
    size_t destination_size,
    struct mylite_db *database
) {
    const char *source = NULL;
    size_t source_size = 0U;
    int rc = MYLITE_OK;

    if (node == NULL || node->kind != MYLITE_SQL_AST_IDENTIFIER || destination == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    source = node->span.text;
    source_size = node->span.length;
    if (source == NULL || source_size == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    if (source[0] == '`') {
        rc = copy_quoted_identifier_text(source, source_size, destination, destination_size);
    } else {
        rc = copy_unquoted_identifier_text(source, source_size, destination, destination_size);
    }
    if (rc != MYLITE_OK) {
        set_identifier_too_long_error(database, "identifier");
        return rc;
    }

    return MYLITE_OK;
}

static int copy_quoted_identifier_text(
    const char *source,
    size_t source_size,
    char *destination,
    size_t destination_size
) {
    size_t destination_index = 0U;

    if (source_size < 2U || source[source_size - 1U] != '`') {
        return MYLITE_ERROR;
    }

    for (size_t source_index = 1U; source_index + 1U < source_size; ++source_index) {
        if (destination_index + 1U >= destination_size) {
            return MYLITE_ERROR;
        }
        if (source[source_index] == '`' && source[source_index + 1U] == '`') {
            destination[destination_index] = '`';
            ++source_index;
        } else {
            destination[destination_index] = source[source_index];
        }
        ++destination_index;
    }
    destination[destination_index] = '\0';

    return destination_index == 0U ? MYLITE_ERROR : MYLITE_OK;
}

static int copy_unquoted_identifier_text(
    const char *source,
    size_t source_size,
    char *destination,
    size_t destination_size
) {
    if (source_size == 0U || source_size >= destination_size) {
        return MYLITE_ERROR;
    }

    memcpy(destination, source, source_size);
    destination[source_size] = '\0';

    return MYLITE_OK;
}

static int plan_columns(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_list,
    struct planned_column *columns,
    size_t column_count
) {
    const struct mylite_sql_ast_node *column_node = child_at(column_list, 0U);

    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        int rc = plan_column(database, column_node, &columns[column_index]);

        if (rc != MYLITE_OK) {
            return rc;
        }
        column_node = column_node == NULL ? NULL : column_node->next_sibling;
    }

    return MYLITE_OK;
}

static int plan_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    struct planned_column *out_column
) {
    int rc = MYLITE_OK;

    *out_column = (struct planned_column){0};
    rc = copy_identifier_text(
        child_at(column_node, 0U),
        out_column->name,
        sizeof(out_column->name),
        database
    );
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_column->name)) {
        set_reserved_name_error(database, "column", out_column->name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = map_integer_type(
            database,
            child_at(column_node, 1U),
            out_column->name,
            &out_column->logical_type,
            &out_column->physical_type
        );
    }
    if (rc == MYLITE_OK) {
        out_column->is_nullable =
            column_is_nullable(child_with_kind(column_node, MYLITE_SQL_AST_NULLABILITY));
    }
    if (rc == MYLITE_OK) {
        out_column->default_node = child_with_kind(column_node, MYLITE_SQL_AST_COLUMN_DEFAULT_NULL);
        if (out_column->default_node == NULL) {
            out_column->default_node =
                child_with_kind(column_node, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE);
        }
        rc = validate_column_default(database, out_column->default_node, out_column);
    }

    return rc;
}

static int validate_column_default(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *default_node,
    const struct planned_column *column
) {
    if (default_node == NULL) {
        return MYLITE_OK;
    }
    if (default_node->kind == MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE) {
        return MYLITE_OK;
    }
    if (default_node->kind != MYLITE_SQL_AST_COLUMN_DEFAULT_NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (!column->is_nullable) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int finalize_planned_column_defaults(
    struct mylite_db *database,
    struct planned_column *columns,
    size_t column_count
) {
    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        int rc = finalize_planned_column_default(database, &columns[column_index]);

        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return MYLITE_OK;
}

static int finalize_planned_column_default(
    struct mylite_db *database,
    struct planned_column *column
) {
    int64_t default_integer = 0;
    int rc = MYLITE_OK;

    column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_NONE;
    column->default_integer = 0;
    if (column->default_node == NULL ||
        column->default_node->kind == MYLITE_SQL_AST_COLUMN_DEFAULT_NULL) {
        return MYLITE_OK;
    }
    if (column->default_node->kind != MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    rc = convert_column_default_value(
        database,
        child_at(column->default_node, 0U),
        column,
        &default_integer
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    column->default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER;
    column->default_integer = default_integer;

    return MYLITE_OK;
}

static int convert_column_default_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct planned_column *column,
    int64_t *out_value
) {
    const uint64_t bigint_signed_negative_abs_max = 9223372036854775808ULL;
    const struct mylite_sql_ast_node *literal = value_node;
    struct mylite_catalog_column_descriptor descriptor = {0};
    struct integer_column_range range = {0};
    bool is_negative = false;
    uint64_t magnitude = 0U;
    int rc = MYLITE_OK;

    if (value_node == NULL || column == NULL || out_value == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(value_node);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_invalid_default_error(database, column->name);
            return MYLITE_ERROR;
        }
        literal = child_at(value_node, 0U);
    }
    if (!boolean_literal_magnitude(literal, &magnitude)) {
        if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
            mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
            set_invalid_default_error(database, column->name);
            return MYLITE_ERROR;
        }
        rc = parse_unsigned_integer_literal(&literal->span, &magnitude);
        if (rc != MYLITE_OK) {
            set_invalid_default_error(database, column->name);
            return MYLITE_ERROR;
        }
    }

    snprintf(descriptor.name, sizeof(descriptor.name), "%s", column->name);
    snprintf(descriptor.logical_type, sizeof(descriptor.logical_type), "%s", column->logical_type);
    snprintf(
        descriptor.physical_type,
        sizeof(descriptor.physical_type),
        "%s",
        column->physical_type
    );
    descriptor.is_nullable = column->is_nullable;
    rc = integer_range_for_column(
        database,
        &descriptor,
        "DEFAULT supports only baseline integer columns",
        &range
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (is_negative) {
        if ((range.negative_abs_max == 0U && magnitude != 0U) ||
            magnitude > range.negative_abs_max) {
            set_invalid_default_error(database, column->name);
            return MYLITE_ERROR;
        }
        *out_value = magnitude == bigint_signed_negative_abs_max ? INT64_MIN : -(int64_t)magnitude;
        return MYLITE_OK;
    }
    if (magnitude > range.positive_max) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }

    *out_value = (int64_t)magnitude;
    return MYLITE_OK;
}

static int check_duplicate_column_names(
    struct mylite_db *database,
    const struct planned_column *columns,
    size_t column_count
) {
    for (size_t left = 0U; left < column_count; ++left) {
        for (size_t right = left + 1U; right < column_count; ++right) {
            if (text_equals_ascii_case_insensitive(columns[left].name, columns[right].name)) {
                set_duplicate_column_error(database, columns[right].name);
                return MYLITE_ERROR;
            }
        }
    }

    return MYLITE_OK;
}

static bool text_equals_ascii_case_insensitive(const char *left, const char *right) {
    size_t index = 0U;

    if (left == NULL || right == NULL) {
        return false;
    }
    while (left[index] != '\0' && right[index] != '\0') {
        if (ascii_lower((unsigned char)left[index]) != ascii_lower((unsigned char)right[index])) {
            return false;
        }
        ++index;
    }

    if (left[index] != '\0') {
        return false;
    }
    if (right[index] != '\0') {
        return false;
    }

    return true;
}

static char ascii_lower(unsigned char byte) {
    if (byte >= 'A' && byte <= 'Z') {
        return (char)(byte + ('a' - 'A'));
    }

    return (char)byte;
}

static int map_integer_type(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    const char **out_logical_type,
    const char **out_physical_type
) {
    enum mylite_sql_ast_integer_type type = mylite_sql_ast_node_integer_type(type_node);
    int is_unsigned = mylite_sql_ast_node_integer_type_is_unsigned(type_node);
    bool has_display_width = false;
    uint64_t display_width = 0U;
    const char *logical_type = NULL;
    int rc = MYLITE_OK;

    if (out_logical_type == NULL || out_physical_type == NULL) {
        return MYLITE_MISUSE;
    }

    rc = map_integer_display_width(
        database,
        type_node,
        column_name,
        &has_display_width,
        &display_width
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    logical_type = logical_type_for_mapped_integer((struct mapped_integer_type){
        .type = type,
        .is_unsigned = is_unsigned,
        .has_display_width = has_display_width,
        .is_bool_alias = mylite_sql_ast_node_integer_type_is_bool_alias(type_node) != 0,
        .display_width = display_width,
    });
    if (logical_type == NULL) {
        return MYLITE_ERROR;
    }
    *out_logical_type = logical_type;
    *out_physical_type = "INTEGER";

    return MYLITE_OK;
}

static int map_integer_display_width(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *type_node,
    const char *column_name,
    bool *out_has_display_width,
    uint64_t *out_display_width
) {
    enum { max_integer_display_width = 255 };

    struct mylite_sql_source_span display_width_span = {0};
    int rc = MYLITE_OK;

    *out_has_display_width = mylite_sql_ast_node_integer_type_has_display_width(type_node) != 0;
    *out_display_width = 0U;
    if (!*out_has_display_width) {
        return MYLITE_OK;
    }

    display_width_span = mylite_sql_ast_node_integer_type_display_width_span(type_node);
    rc = parse_unsigned_integer_literal(&display_width_span, out_display_width);
    if (rc != MYLITE_OK || *out_display_width > max_integer_display_width) {
        set_display_width_out_of_range_error(database, column_name);
        return MYLITE_ERROR;
    }

    rc = append_integer_display_width_warning(database);
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }

    return rc;
}

static int append_integer_display_width_warning(struct mylite_db *database) {
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_integer_display_width_deprecated,
        "HY000",
        "Integer display width is deprecated and will be removed in a future release."
    );
}

static const char *logical_type_for_mapped_integer(struct mapped_integer_type integer_type) {
    switch (integer_type.type) {
    case MYLITE_SQL_AST_INTEGER_TYPE_NONE:
        return NULL;
    case MYLITE_SQL_AST_INTEGER_TYPE_TINYINT:
        if (integer_type.is_unsigned != 0) {
            return "TINYINT UNSIGNED";
        }
        if (integer_type.is_bool_alias) {
            return "TINYINT(1)";
        }
        if (integer_type.has_display_width && integer_type.display_width == 1U) {
            return "TINYINT(1)";
        }
        return "TINYINT";
    case MYLITE_SQL_AST_INTEGER_TYPE_SMALLINT:
        return integer_type.is_unsigned == 0 ? "SMALLINT" : "SMALLINT UNSIGNED";
    case MYLITE_SQL_AST_INTEGER_TYPE_MEDIUMINT:
        return integer_type.is_unsigned == 0 ? "MEDIUMINT" : "MEDIUMINT UNSIGNED";
    case MYLITE_SQL_AST_INTEGER_TYPE_INT:
        return integer_type.is_unsigned == 0 ? "INT" : "INT UNSIGNED";
    case MYLITE_SQL_AST_INTEGER_TYPE_BIGINT:
        return integer_type.is_unsigned == 0 ? "BIGINT" : "BIGINT UNSIGNED";
    }

    return NULL;
}

static bool modify_column_integer_value_domain_matches(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
) {
    const char *original_logical_type = original_column->logical_type;
    const char *replacement_logical_type = replacement_column->logical_type;

    if (original_logical_type == NULL || replacement_logical_type == NULL) {
        return false;
    }
    if (strcmp(original_logical_type, "TINYINT(1)") == 0) {
        original_logical_type = "TINYINT";
    }
    if (strcmp(replacement_logical_type, "TINYINT(1)") == 0) {
        replacement_logical_type = "TINYINT";
    }

    return strcmp(original_logical_type, replacement_logical_type) == 0;
}

static bool column_is_nullable(const struct mylite_sql_ast_node *nullability_node) {
    return mylite_sql_ast_node_nullability(nullability_node) != MYLITE_SQL_AST_NULLABILITY_NOT_NULL;
}

static int resolve_readable_base_table(
    struct mylite_db *database,
    const struct table_name_resolution *resolution,
    struct mylite_catalog_table_descriptor *out_table
) {
    int rc = mylite_catalog_read_table_by_name(
        database,
        resolution->schema.schema_id,
        resolution->table_name,
        out_table
    );

    if (rc != MYLITE_OK) {
        set_table_does_not_exist_error(database, resolution->schema.name, resolution->table_name);
        return MYLITE_ERROR;
    }
    if (out_table->kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(database, "statement supports only persistent base tables");
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int load_table_columns(
    struct mylite_db *database,
    int64_t table_id,
    struct mylite_catalog_column_descriptor **out_columns,
    size_t *out_column_count
) {
    struct load_columns_context context = {0};
    int rc = MYLITE_OK;

    *out_columns = NULL;
    *out_column_count = 0U;
    rc =
        mylite_catalog_for_each_column_in_table(database, table_id, append_loaded_column, &context);
    if (rc != MYLITE_OK) {
        free(context.columns);
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        } else {
            set_runtime_error(database, "failed to load table columns");
        }
        return rc;
    }
    if (context.count == 0U) {
        free(context.columns);
        set_runtime_error(database, "table descriptor has no columns");
        return MYLITE_ERROR;
    }

    *out_columns = context.columns;
    *out_column_count = context.count;

    return MYLITE_OK;
}

static int append_loaded_column(
    const struct mylite_catalog_column_descriptor *column,
    void *user_data
) {
    struct load_columns_context *context = user_data;
    int rc = MYLITE_OK;

    if (column == NULL || context == NULL) {
        return MYLITE_MISUSE;
    }

    rc = load_columns_reserve(context, context->count + 1U);
    if (rc != MYLITE_OK) {
        return rc;
    }

    context->columns[context->count] = *column;
    ++context->count;

    return MYLITE_OK;
}

static int load_columns_reserve(struct load_columns_context *context, size_t required_capacity) {
    enum { initial_column_capacity = 4 };

    struct mylite_catalog_column_descriptor *columns = NULL;
    size_t capacity = context->capacity;

    if (required_capacity <= capacity) {
        return MYLITE_OK;
    }
    if (capacity == 0U) {
        capacity = initial_column_capacity;
    }
    while (capacity < required_capacity) {
        if (capacity > SIZE_MAX / 2U) {
            return MYLITE_NOMEM;
        }
        capacity *= 2U;
    }
    if (capacity > SIZE_MAX / sizeof(*columns)) {
        return MYLITE_NOMEM;
    }

    columns = realloc(context->columns, capacity * sizeof(*columns));
    if (columns == NULL) {
        return MYLITE_NOMEM;
    }

    context->columns = columns;
    context->capacity = capacity;

    return MYLITE_OK;
}

static int find_column_index(
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const char *name,
    size_t *out_index
) {
    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        if (text_equals_ascii_case_insensitive(columns[column_index].name, name)) {
            *out_index = column_index;
            return MYLITE_OK;
        }
    }

    return MYLITE_ERROR;
}

static int resolve_descriptor_column_reference(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct select_source_context *source_context,
    enum column_reference_diagnostic_context diagnostic_context,
    const char *unsupported_message,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char column_name[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    size_t part_count = 0U;
    size_t column_index = 0U;
    int rc = MYLITE_OK;

    *out_column = (struct mylite_catalog_column_descriptor){0};
    if (column_node == NULL || (column_node->kind != MYLITE_SQL_AST_IDENTIFIER &&
                                column_node->kind != MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }

    rc = collect_column_reference_parts(database, column_node, parts, &part_count);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc =
        format_column_reference_name(database, parts, part_count, column_name, sizeof(column_name));
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (source_context == NULL && part_count > 1U) {
        set_unsupported_error(database, unsupported_message);
        return MYLITE_ERROR;
    }
    if (source_context != NULL &&
        !column_reference_qualifier_matches_source(parts, part_count, source_context)) {
        set_unknown_column_reference_error(database, diagnostic_context, column_name);
        return MYLITE_ERROR;
    }

    rc =
        find_column_index(table_columns, table_column_count, parts[part_count - 1U], &column_index);
    if (rc != MYLITE_OK) {
        set_unknown_column_reference_error(database, diagnostic_context, column_name);
        return MYLITE_ERROR;
    }

    *out_column = table_columns[column_index];
    return MYLITE_OK;
}

static int collect_column_reference_parts(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t *out_part_count
) {
    *out_part_count = 0U;
    return collect_identifier_parts(
        column_node,
        parts,
        table_name_part_capacity,
        out_part_count,
        database
    );
}

static int format_column_reference_name(
    struct mylite_db *database,
    char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t part_count,
    char *destination,
    size_t destination_size
) {
    size_t offset = 0U;

    if (part_count == 0U || part_count > table_name_part_capacity || destination == NULL ||
        destination_size == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    destination[0] = '\0';
    for (size_t part_index = 0U; part_index < part_count; ++part_index) {
        int written = 0;

        if (part_index > 0U) {
            if (offset + 1U >= destination_size) {
                set_parse_error(database, NULL);
                return MYLITE_ERROR;
            }
            destination[offset] = '.';
            ++offset;
            destination[offset] = '\0';
        }
        written =
            snprintf(destination + offset, destination_size - offset, "%s", parts[part_index]);
        if (written < 0 || (size_t)written >= destination_size - offset) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        offset += (size_t)written;
    }

    return MYLITE_OK;
}

static bool column_reference_qualifier_matches_source(
    const char parts[][MYLITE_CATALOG_IDENTIFIER_CAPACITY],
    size_t part_count,
    const struct select_source_context *source_context
) {
    if (part_count == 1U) {
        return true;
    }
    if (source_context == NULL || source_context->source == NULL) {
        return false;
    }
    if (part_count == 2U) {
        const char *expected = source_context->source->table_name;

        if (source_context->has_alias) {
            expected = source_context->alias;
        }

        return text_equals_ascii_case_insensitive(parts[0], expected);
    }
    if (part_count == 3U && !source_context->has_alias) {
        bool schema_matches =
            text_equals_ascii_case_insensitive(parts[0], source_context->source->schema.name);
        bool table_matches =
            text_equals_ascii_case_insensitive(parts[1], source_context->source->table_name);

        if (schema_matches && table_matches) {
            return true;
        }
        return false;
    }

    return false;
}

static void set_unknown_column_reference_error(
    struct mylite_db *database,
    enum column_reference_diagnostic_context context,
    const char *column_name
) {
    if (context == COLUMN_REFERENCE_WHERE) {
        set_unknown_where_column_error(database, column_name);
        return;
    }
    if (context == COLUMN_REFERENCE_ORDER) {
        set_unknown_order_column_error(database, column_name);
        return;
    }

    set_unknown_column_error(database, column_name);
}

static size_t count_visible_columns(
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count
) {
    size_t visible_count = 0U;

    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        if (columns[column_index].is_visible) {
            ++visible_count;
        }
    }

    return visible_count;
}

static int collect_insert_target_indexes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_list,
    const struct planned_insert *plan,
    size_t **out_indexes,
    size_t *out_index_count
) {
    size_t explicit_column_count = mylite_sql_ast_node_child_count(column_list);
    size_t column_count = explicit_column_count;
    size_t *indexes = NULL;

    *out_indexes = NULL;
    *out_index_count = 0U;
    if (column_list == NULL || column_list->kind != MYLITE_SQL_AST_IDENTIFIER_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (explicit_column_count == 0U) {
        column_count = count_visible_insert_target_columns(plan);
    }
    if (column_count > SIZE_MAX / sizeof(*indexes)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    indexes = calloc(column_count, sizeof(*indexes));
    if (indexes == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    if (explicit_column_count == 0U) {
        collect_visible_insert_target_indexes(plan, indexes);
    } else {
        int rc = collect_explicit_insert_target_indexes(
            database,
            column_list,
            plan,
            indexes,
            column_count
        );

        if (rc != MYLITE_OK) {
            free(indexes);
            return rc;
        }
    }

    *out_indexes = indexes;
    *out_index_count = column_count;

    return MYLITE_OK;
}

static size_t count_visible_insert_target_columns(const struct planned_insert *plan) {
    size_t column_count = 0U;

    for (size_t column_index = 0U; column_index < plan->column_count; ++column_index) {
        if (plan->columns[column_index].is_visible) {
            ++column_count;
        }
    }

    return column_count;
}

static void collect_visible_insert_target_indexes(
    const struct planned_insert *plan,
    size_t *indexes
) {
    size_t target_index = 0U;

    for (size_t column_index = 0U; column_index < plan->column_count; ++column_index) {
        if (plan->columns[column_index].is_visible) {
            indexes[target_index] = column_index;
            ++target_index;
        }
    }
}

static int collect_explicit_insert_target_indexes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_list,
    const struct planned_insert *plan,
    size_t *indexes,
    size_t column_count
) {
    const struct mylite_sql_ast_node *column_node = child_at(column_list, 0U);

    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
        int rc = copy_identifier_text(column_node, column_name, sizeof(column_name), database);

        if (rc != MYLITE_OK) {
            return rc;
        }
        rc = find_column_index(
            plan->columns,
            plan->column_count,
            column_name,
            &indexes[column_index]
        );
        if (rc != MYLITE_OK) {
            set_unknown_column_error(database, column_name);
            return MYLITE_ERROR;
        }
        column_node = column_node->next_sibling;
    }

    return MYLITE_OK;
}

static int collect_insert_set_target_indexes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment_list,
    const struct planned_insert *plan,
    size_t **out_indexes,
    size_t *out_index_count
) {
    const struct mylite_sql_ast_node *assignment = NULL;
    size_t assignment_count = 0U;
    size_t *indexes = NULL;

    *out_indexes = NULL;
    *out_index_count = 0U;
    if (assignment_list == NULL || assignment_list->kind != MYLITE_SQL_AST_INSERT_ASSIGNMENT_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    assignment_count = mylite_sql_ast_node_child_count(assignment_list);
    if (assignment_count == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (assignment_count > SIZE_MAX / sizeof(*indexes)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    indexes = calloc(assignment_count, sizeof(*indexes));
    if (indexes == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    assignment = child_at(assignment_list, 0U);
    for (size_t assignment_index = 0U; assignment_index < assignment_count; ++assignment_index) {
        const struct mylite_sql_ast_node *target = NULL;
        char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
        int rc = MYLITE_OK;

        if (assignment == NULL || assignment->kind != MYLITE_SQL_AST_INSERT_ASSIGNMENT) {
            set_parse_error(database, NULL);
            free(indexes);
            return MYLITE_ERROR;
        }

        target = child_at(assignment, 0U);
        if (target == NULL || target->kind != MYLITE_SQL_AST_IDENTIFIER) {
            set_unsupported_error(
                database,
                "INSERT ... SET supports only unqualified assignment columns"
            );
            free(indexes);
            return MYLITE_ERROR;
        }

        rc = copy_identifier_text(target, column_name, sizeof(column_name), database);
        if (rc == MYLITE_OK) {
            rc = find_column_index(
                plan->columns,
                plan->column_count,
                column_name,
                &indexes[assignment_index]
            );
            if (rc != MYLITE_OK) {
                set_unknown_column_error(database, column_name);
                free(indexes);
                return MYLITE_ERROR;
            }
        } else {
            free(indexes);
            return rc;
        }
        assignment = assignment->next_sibling;
    }

    *out_indexes = indexes;
    *out_index_count = assignment_count;

    return MYLITE_OK;
}

static int check_insert_target_duplicate(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    const size_t *target_indexes,
    size_t target_count
) {
    for (size_t left = 0U; left < target_count; ++left) {
        for (size_t right = left + 1U; right < target_count; ++right) {
            if (target_indexes[left] == target_indexes[right]) {
                set_column_specified_twice_error(database, columns[target_indexes[right]].name);
                return MYLITE_ERROR;
            }
        }
    }

    return MYLITE_OK;
}

static int check_insert_omitted_columns(
    struct mylite_db *database,
    const struct planned_insert *plan,
    const size_t *target_indexes,
    size_t target_count
) {
    for (size_t column_index = 0U; column_index < plan->column_count; ++column_index) {
        bool is_assigned = false;

        for (size_t target_index = 0U; target_index < target_count; ++target_index) {
            if (target_indexes[target_index] == column_index) {
                is_assigned = true;
                break;
            }
        }
        if (!is_assigned &&
            (plan->columns[column_index].default_kind ==
                 MYLITE_CATALOG_COLUMN_DEFAULT_NO_EXPLICIT ||
             (!plan->columns[column_index].is_nullable &&
              plan->columns[column_index].default_kind != MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER))) {
            set_no_default_error(database, plan->columns[column_index].name);
            return MYLITE_ERROR;
        }
    }

    return MYLITE_OK;
}

static int plan_insert_rows(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *row_list,
    const size_t *target_indexes,
    size_t target_count,
    struct planned_insert *plan
) {
    const struct mylite_sql_ast_node *row_node = NULL;

    if (row_list == NULL || row_list->kind != MYLITE_SQL_AST_INSERT_ROW_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    plan->row_count = mylite_sql_ast_node_child_count(row_list);
    if (plan->row_count == 0U || plan->row_count > SIZE_MAX / sizeof(*plan->rows)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    plan->rows = calloc(plan->row_count, sizeof(*plan->rows));
    if (plan->rows == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    row_node = child_at(row_list, 0U);
    for (size_t row_index = 0U; row_index < plan->row_count; ++row_index) {
        int rc = plan_insert_row(
            database,
            row_node,
            row_index + 1U,
            target_indexes,
            target_count,
            plan,
            &plan->rows[row_index]
        );

        if (rc != MYLITE_OK) {
            return rc;
        }
        row_node = row_node == NULL ? NULL : row_node->next_sibling;
    }

    return MYLITE_OK;
}

static int plan_insert_row(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *row_node,
    size_t row_number,
    const size_t *target_indexes,
    size_t target_count,
    struct planned_insert *plan,
    struct planned_insert_row *out_row
) {
    const struct mylite_sql_ast_node *value_node = child_at(row_node, 0U);
    int rc = MYLITE_OK;

    if (row_node == NULL || row_node->kind != MYLITE_SQL_AST_INSERT_ROW) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_child_count(row_node) != target_count) {
        set_column_count_mismatch_error(database, row_number);
        return MYLITE_ERROR;
    }
    rc = allocate_insert_row_values(database, plan, out_row);

    for (size_t target_index = 0U; rc == MYLITE_OK && target_index < target_count; ++target_index) {
        size_t column_index = target_indexes[target_index];
        rc = convert_insert_value(
            database,
            value_node,
            &plan->columns[column_index],
            row_number,
            &out_row->values[column_index]
        );

        value_node = value_node == NULL ? NULL : value_node->next_sibling;
    }

    return rc;
}

static int plan_insert_set_row(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment_list,
    const size_t *target_indexes,
    size_t target_count,
    struct planned_insert *plan
) {
    const struct mylite_sql_ast_node *assignment = NULL;
    int rc = MYLITE_OK;

    plan->row_count = 1U;
    plan->rows = calloc(1U, sizeof(*plan->rows));
    if (plan->rows == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = allocate_insert_row_values(database, plan, &plan->rows[0]);
    assignment = child_at(assignment_list, 0U);
    for (size_t target_index = 0U; rc == MYLITE_OK && target_index < target_count; ++target_index) {
        const struct mylite_sql_ast_node *value_node = NULL;
        size_t column_index = target_indexes[target_index];

        if (assignment == NULL || assignment->kind != MYLITE_SQL_AST_INSERT_ASSIGNMENT) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }

        value_node = child_at(assignment, 1U);
        rc = convert_insert_value(
            database,
            value_node,
            &plan->columns[column_index],
            1U,
            &plan->rows[0].values[column_index]
        );
        assignment = assignment->next_sibling;
    }

    return rc;
}

static int allocate_insert_row_values(
    struct mylite_db *database,
    const struct planned_insert *plan,
    struct planned_insert_row *out_row
) {
    if (plan->column_count > SIZE_MAX / sizeof(*out_row->values)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    out_row->values = calloc(plan->column_count, sizeof(*out_row->values));
    if (out_row->values == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    for (size_t column_index = 0U; column_index < plan->column_count; ++column_index) {
        if (plan->columns[column_index].default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER) {
            out_row->values[column_index].is_null = false;
            out_row->values[column_index].integer = plan->columns[column_index].default_integer;
        } else {
            out_row->values[column_index].is_null = true;
        }
    }

    return MYLITE_OK;
}

static int convert_insert_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    struct planned_value *out_value
) {
    if (value_node == NULL || column == NULL || out_value == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    *out_value = (struct planned_value){.is_null = true, .integer = 0};
    if (value_node->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_NULL) {
        if (!column->is_nullable) {
            set_bad_null_error(database, column->name);
            return MYLITE_ERROR;
        }
        return MYLITE_OK;
    }

    return convert_integer_literal(database, value_node, column, row_number, out_value);
}

static int convert_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    struct planned_value *out_value
) {
    const struct mylite_sql_ast_node *literal = value_node;
    bool is_negative = false;
    uint64_t magnitude = 0U;
    int rc = MYLITE_OK;

    if (value_node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(value_node);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_unsupported_error(database, "INSERT supports only integer and NULL values");
            return MYLITE_ERROR;
        }
        literal = child_at(value_node, 0U);
    }
    if (!boolean_literal_magnitude(literal, &magnitude)) {
        if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
            mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
            set_unsupported_error(
                database,
                "INSERT supports only integer, boolean, and NULL values"
            );
            return MYLITE_ERROR;
        }

        rc = parse_unsigned_integer_literal(&literal->span, &magnitude);
        if (rc != MYLITE_OK) {
            set_out_of_range_error(database, column->name, row_number);
            return MYLITE_ERROR;
        }
    }

    out_value->is_null = false;
    rc = convert_integer_for_column(
        database,
        magnitude,
        is_negative,
        column,
        row_number,
        &out_value->integer
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    return MYLITE_OK;
}

static bool boolean_literal_magnitude(
    const struct mylite_sql_ast_node *literal,
    uint64_t *out_value
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL || out_value == NULL) {
        return false;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        *out_value = 1U;
        return true;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        *out_value = 0U;
        return true;
    }

    return false;
}

static int parse_unsigned_integer_literal(
    const struct mylite_sql_source_span *span,
    uint64_t *out_value
) {
    uint64_t value = 0U;

    if (span == NULL || span->text == NULL || span->length == 0U || out_value == NULL) {
        return MYLITE_ERROR;
    }

    for (size_t index = 0U; index < span->length; ++index) {
        unsigned char byte = (unsigned char)span->text[index];
        uint64_t digit = 0U;

        if (byte < '0' || byte > '9') {
            return MYLITE_ERROR;
        }
        digit = (uint64_t)(byte - '0');
        if (value > (UINT64_MAX - digit) / decimal_base) {
            return MYLITE_ERROR;
        }
        value = (value * decimal_base) + digit;
    }

    *out_value = value;

    return MYLITE_OK;
}

static int convert_integer_for_column(
    struct mylite_db *database,
    uint64_t magnitude,
    bool is_negative,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    int64_t *out_value
) {
    const uint64_t bigint_signed_negative_abs_max = 9223372036854775808ULL;
    struct integer_column_range range = {0};
    int rc = integer_range_for_column(
        database,
        column,
        "INSERT supports only baseline integer columns",
        &range
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (is_negative) {
        if ((range.negative_abs_max == 0U && magnitude != 0U) ||
            magnitude > range.negative_abs_max) {
            set_out_of_range_error(database, column->name, row_number);
            return MYLITE_ERROR;
        }
        if (magnitude == bigint_signed_negative_abs_max) {
            *out_value = INT64_MIN;
        } else {
            *out_value = -(int64_t)magnitude;
        }
        return MYLITE_OK;
    }

    if (magnitude > range.positive_max) {
        set_out_of_range_error(database, column->name, row_number);
        return MYLITE_ERROR;
    }
    *out_value = (int64_t)magnitude;

    return MYLITE_OK;
}

static int plan_select_columns(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select *out_plan
) {
    const struct mylite_sql_ast_node *item = NULL;

    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (select_list_is_wildcard(select_list)) {
        for (size_t column_index = 0U; column_index < table_column_count; ++column_index) {
            if (!table_columns[column_index].is_visible) {
                continue;
            }
            int rc = append_select_column(database, out_plan, &table_columns[column_index], NULL);

            if (rc != MYLITE_OK) {
                set_nomem_error(database);
                return rc;
            }
        }
        return MYLITE_OK;
    }

    item = child_at(select_list, 0U);
    while (item != NULL) {
        const struct mylite_sql_ast_node *column_node = NULL;
        struct mylite_catalog_column_descriptor column = {0};
        int rc = select_item_column_reference(item, &column_node);

        if (rc != MYLITE_OK) {
            set_unsupported_error(database, "SELECT supports only descriptor table columns");
            return MYLITE_ERROR;
        }
        rc = resolve_descriptor_column_reference(
            database,
            column_node,
            source_context,
            COLUMN_REFERENCE_FIELD,
            "SELECT supports only descriptor table columns",
            table_columns,
            table_column_count,
            &column
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        rc = append_select_column(database, out_plan, &column, child_at(item, 1U));
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
            return rc;
        }
        item = item->next_sibling;
    }

    return MYLITE_OK;
}

static int plan_select_distinct_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select *out_plan
) {
    const struct mylite_sql_ast_node *item = NULL;
    const struct mylite_sql_ast_node *column_node = NULL;
    struct mylite_catalog_column_descriptor column = {0};
    int rc = MYLITE_OK;

    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_child_count(select_list) != 1U) {
        set_unsupported_error(database, "SELECT DISTINCT supports exactly one selected column");
        return MYLITE_ERROR;
    }

    item = child_at(select_list, 0U);
    rc = select_item_column_reference(item, &column_node);
    if (rc != MYLITE_OK) {
        set_unsupported_error(database, "SELECT DISTINCT supports only one descriptor column");
        return MYLITE_ERROR;
    }

    rc = resolve_descriptor_column_reference(
        database,
        column_node,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "SELECT DISTINCT supports only one descriptor column",
        table_columns,
        table_column_count,
        &column
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = append_select_column(database, out_plan, &column, child_at(item, 1U));
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    return MYLITE_OK;
}

static bool select_list_is_wildcard(const struct mylite_sql_ast_node *select_list) {
    const struct mylite_sql_ast_node *item = child_at(select_list, 0U);
    const struct mylite_sql_ast_node *expression = child_at(item, 0U);

    if (mylite_sql_ast_node_child_count(select_list) == 1U && expression != NULL &&
        expression->kind == MYLITE_SQL_AST_WILDCARD) {
        return true;
    }

    return false;
}

static int append_select_column(
    struct mylite_db *database,
    struct planned_select *plan,
    const struct mylite_catalog_column_descriptor *column,
    const struct mylite_sql_ast_node *alias
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    const struct mylite_sql_ast_node **aliases = NULL;
    size_t required_count = plan->column_count + 1U;

    if (required_count > SIZE_MAX / sizeof(*columns)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (required_count > SIZE_MAX / sizeof(*aliases)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    columns = realloc(plan->columns, required_count * sizeof(*columns));
    if (columns == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    plan->columns = columns;

    aliases = (const struct mylite_sql_ast_node **)
        realloc((void *)plan->column_aliases, required_count * sizeof(*aliases));
    if (aliases == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    plan->column_aliases = aliases;

    plan->columns[plan->column_count] = *column;
    plan->column_aliases[plan->column_count] = alias;
    plan->column_count = required_count;

    return MYLITE_OK;
}

static int select_item_column_reference(
    const struct mylite_sql_ast_node *item,
    const struct mylite_sql_ast_node **out_column
) {
    const struct mylite_sql_ast_node *expression = child_at(item, 0U);

    *out_column = NULL;
    if (item == NULL || item->kind != MYLITE_SQL_AST_SELECT_ITEM || expression == NULL ||
        (expression->kind != MYLITE_SQL_AST_IDENTIFIER &&
         expression->kind != MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) {
        return MYLITE_ERROR;
    }

    *out_column = expression;

    return MYLITE_OK;
}

static int append_select_result_column(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_select *plan,
    size_t column_index
) {
    const char *column_name = plan->columns[column_index].name;
    const struct mylite_sql_ast_node *alias = plan->column_aliases[column_index];
    char *alias_text = NULL;
    int rc = MYLITE_OK;

    if (alias != NULL) {
        rc = copy_select_item_alias_text(database, alias, &alias_text);
        if (rc != MYLITE_OK) {
            return rc;
        }
        column_name = alias_text;
    }

    rc = mylite_result_append_column(result, column_name);
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }
    free(alias_text);

    return rc;
}

static int copy_select_item_alias_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *alias,
    char **out_text
) {
    int rc = MYLITE_OK;

    if (out_text == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    if (alias == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (alias->kind == MYLITE_SQL_AST_IDENTIFIER) {
        return copy_select_item_identifier_alias_text(database, alias, out_text);
    }
    if (alias->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(alias) == MYLITE_SQL_AST_LITERAL_STRING) {
        rc = decode_table_option_string_literal(
            database,
            alias,
            out_text,
            (struct table_option_name_policy){
                .identifier_kind = "alias",
                .nul_message = "select-item aliases do not support NUL bytes",
            }
        );
        if (rc == MYLITE_OK) {
            rc = validate_select_item_alias_text(database, out_text);
        }
        return rc;
    }

    set_parse_error(database, NULL);
    return MYLITE_ERROR;
}

static int copy_select_item_identifier_alias_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *alias,
    char **out_text
) {
    char identifier[select_item_alias_capacity];
    const char *source = NULL;
    size_t source_size = 0U;
    int rc = MYLITE_OK;

    if (out_text == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    if (alias == NULL || alias->kind != MYLITE_SQL_AST_IDENTIFIER) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    source = alias->span.text;
    source_size = alias->span.length;
    if (source == NULL || source_size == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (source[0] == '`') {
        rc = copy_quoted_identifier_text(source, source_size, identifier, sizeof(identifier));
    } else {
        rc = copy_unquoted_identifier_text(source, source_size, identifier, sizeof(identifier));
    }
    if (rc != MYLITE_OK) {
        set_identifier_too_long_error(database, "alias");
        return rc;
    }

    return duplicate_text(database, identifier, out_text);
}

static int validate_select_item_alias_text(struct mylite_db *database, char **text) {
    if (text == NULL || *text == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (strlen(*text) > select_item_alias_max_length) {
        free(*text);
        *text = NULL;
        set_identifier_too_long_error(database, "alias");
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int duplicate_text(struct mylite_db *database, const char *source, char **out_text) {
    size_t length = 0U;
    char *text = NULL;

    if (out_text == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    if (source == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    length = strlen(source);
    if (length == SIZE_MAX) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    text = (char *)malloc(length + 1U);
    if (text == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(text, source, length + 1U);
    *out_text = text;

    return MYLITE_OK;
}

static int plan_select_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *out_predicate
) {
    *out_predicate = (struct planned_select_predicate){0};
    if (where_clause == NULL) {
        return MYLITE_OK;
    }
    if (where_clause->kind != MYLITE_SQL_AST_WHERE_CLAUSE) {
        set_unsupported_error(database, "SELECT supports only one descriptor column predicate");
        return MYLITE_ERROR;
    }

    return plan_select_predicate_node(
        database,
        child_at(where_clause, 0U),
        source_context,
        table_columns,
        table_column_count,
        out_predicate
    );
}

static int plan_select_predicate_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *out_predicate
) {
    const struct mylite_sql_ast_node *current = predicate_node;

    while (current != NULL && current->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
        current = child_at(current, 0U);
    }
    if (current == NULL) {
        set_unsupported_error(database, "SELECT supports only one descriptor column predicate");
        return MYLITE_ERROR;
    }
    if (current->kind == MYLITE_SQL_AST_COMPARISON_PREDICATE) {
        return plan_comparison_predicate(
            database,
            current,
            source_context,
            table_columns,
            table_column_count,
            out_predicate
        );
    }
    if (current->kind == MYLITE_SQL_AST_IS_NULL_PREDICATE) {
        return plan_is_null_predicate(
            database,
            current,
            source_context,
            table_columns,
            table_column_count,
            out_predicate
        );
    }

    set_unsupported_error(database, "SELECT supports only one descriptor column predicate");
    return MYLITE_ERROR;
}

static int plan_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *out_predicate
) {
    int rc = resolve_predicate_column(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &out_predicate->column
    );

    if (rc == MYLITE_OK) {
        rc = convert_predicate_integer_literal(
            database,
            child_at(predicate_node, 1U),
            &out_predicate->column,
            &out_predicate->value
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    out_predicate->kind = PLANNED_SELECT_PREDICATE_COMPARISON;
    out_predicate->operator_kind = mylite_sql_ast_node_operator(predicate_node);
    return MYLITE_OK;
}

static int plan_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_predicate *out_predicate
) {
    int rc = resolve_predicate_column(
        database,
        child_at(predicate_node, 0U),
        source_context,
        table_columns,
        table_column_count,
        &out_predicate->column
    );

    if (rc != MYLITE_OK) {
        return rc;
    }

    out_predicate->kind = PLANNED_SELECT_PREDICATE_IS_NULL;
    out_predicate->operator_kind = mylite_sql_ast_node_operator(predicate_node);
    return MYLITE_OK;
}

static int resolve_predicate_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column
) {
    return resolve_descriptor_column_reference(
        database,
        column_node,
        source_context,
        COLUMN_REFERENCE_WHERE,
        "WHERE supports only unqualified predicate columns",
        table_columns,
        table_column_count,
        out_column
    );
}

static int convert_predicate_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    const struct mylite_sql_ast_node *literal = value_node;
    bool is_negative = false;
    uint64_t magnitude = 0U;
    int rc = MYLITE_OK;

    *out_value = (struct planned_value){.is_null = false, .integer = 0};
    if (value_node == NULL || column == NULL) {
        set_unsupported_error(database, "WHERE supports only integer predicate literals");
        return MYLITE_ERROR;
    }

    if (value_node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(value_node);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_unsupported_error(database, "WHERE supports only integer predicate literals");
            return MYLITE_ERROR;
        }
        literal = child_at(value_node, 0U);
    }
    if (!boolean_literal_magnitude(literal, &magnitude)) {
        if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
            mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
            set_unsupported_error(
                database,
                "WHERE supports only integer or boolean predicate literals"
            );
            return MYLITE_ERROR;
        }

        rc = parse_unsigned_integer_literal(&literal->span, &magnitude);
        if (rc != MYLITE_OK) {
            set_predicate_out_of_range_error(database, column->name);
            return MYLITE_ERROR;
        }
    }

    rc = convert_integer_for_predicate(
        database,
        magnitude,
        is_negative,
        column,
        &out_value->integer
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    out_value->is_null = false;

    return MYLITE_OK;
}

static int convert_integer_for_predicate(
    struct mylite_db *database,
    uint64_t magnitude,
    bool is_negative,
    const struct mylite_catalog_column_descriptor *column,
    int64_t *out_value
) {
    const uint64_t bigint_signed_negative_abs_max = 9223372036854775808ULL;
    struct integer_column_range range = {0};
    int rc = integer_range_for_column(
        database,
        column,
        "WHERE supports only baseline integer columns",
        &range
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (is_negative) {
        if ((range.negative_abs_max == 0U && magnitude != 0U) ||
            magnitude > range.negative_abs_max) {
            set_predicate_out_of_range_error(database, column->name);
            return MYLITE_ERROR;
        }
        if (magnitude == bigint_signed_negative_abs_max) {
            *out_value = INT64_MIN;
        } else {
            *out_value = -(int64_t)magnitude;
        }
        return MYLITE_OK;
    }
    if (magnitude > range.positive_max) {
        set_predicate_out_of_range_error(database, column->name);
        return MYLITE_ERROR;
    }

    *out_value = (int64_t)magnitude;
    return MYLITE_OK;
}

static int plan_select_order(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_clause,
    const struct select_source_context *source_context,
    const struct planned_select *select_plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select_order *out_order
) {
    const struct mylite_sql_ast_node *direction = NULL;
    bool resolved_alias = false;
    int rc = MYLITE_OK;

    out_order->has_order = false;
    out_order->column = (struct mylite_catalog_column_descriptor){0};
    out_order->direction = PLANNED_SELECT_ORDER_DEFAULT;
    if (order_clause == NULL) {
        return MYLITE_OK;
    }
    if (order_clause->kind != MYLITE_SQL_AST_ORDER_BY_CLAUSE) {
        set_unsupported_error(database, "SELECT supports only one descriptor ORDER BY column");
        return MYLITE_ERROR;
    }

    rc = resolve_order_alias(
        database,
        child_at(order_clause, 0U),
        select_plan,
        &out_order->column,
        &resolved_alias
    );
    if (rc == MYLITE_OK && !resolved_alias) {
        rc = resolve_order_column(
            database,
            child_at(order_clause, 0U),
            source_context,
            table_columns,
            table_column_count,
            &out_order->column
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    out_order->has_order = true;
    out_order->direction = PLANNED_SELECT_ORDER_ASC;
    direction = child_at(order_clause, 1U);
    if (mylite_sql_ast_node_order_direction(direction) == MYLITE_SQL_AST_ORDER_DIRECTION_DESC) {
        out_order->direction = PLANNED_SELECT_ORDER_DESC;
    }

    return MYLITE_OK;
}

static int resolve_order_alias(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct planned_select *select_plan,
    struct mylite_catalog_column_descriptor *out_column,
    bool *out_resolved
) {
    char *order_name = NULL;
    bool found = false;
    int rc = MYLITE_OK;

    *out_resolved = false;
    if (select_plan == NULL || column_node == NULL ||
        column_node->kind != MYLITE_SQL_AST_IDENTIFIER) {
        return MYLITE_OK;
    }

    rc = copy_select_item_identifier_alias_text(database, column_node, &order_name);
    if (rc != MYLITE_OK) {
        return rc;
    }

    for (size_t column_index = 0U; column_index < select_plan->column_count; ++column_index) {
        const struct mylite_sql_ast_node *alias = select_plan->column_aliases[column_index];
        char *alias_text = NULL;

        if (alias == NULL) {
            continue;
        }

        rc = copy_select_item_alias_text(database, alias, &alias_text);
        if (rc != MYLITE_OK) {
            free(order_name);
            return rc;
        }
        if (text_equals_ascii_case_insensitive(alias_text, order_name)) {
            if (found) {
                free(alias_text);
                set_ambiguous_order_column_error(database, order_name);
                free(order_name);
                return MYLITE_ERROR;
            }
            *out_column = select_plan->columns[column_index];
            found = true;
        }
        free(alias_text);
    }

    free(order_name);
    *out_resolved = found;
    return MYLITE_OK;
}

static int resolve_order_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column
) {
    return resolve_descriptor_column_reference(
        database,
        column_node,
        source_context,
        COLUMN_REFERENCE_ORDER,
        "ORDER BY supports only unqualified descriptor columns",
        table_columns,
        table_column_count,
        out_column
    );
}

static int plan_select_limit(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *limit_clause,
    struct planned_select_limit *out_limit
) {
    int rc = MYLITE_OK;

    out_limit->has_limit = false;
    out_limit->row_count = 0;
    out_limit->has_offset = false;
    out_limit->offset = 0;
    if (limit_clause == NULL) {
        return MYLITE_OK;
    }
    if (limit_clause->kind != MYLITE_SQL_AST_LIMIT_CLAUSE) {
        set_unsupported_error(database, "SELECT supports only literal LIMIT clauses");
        return MYLITE_ERROR;
    }

    rc = convert_limit_integer_literal(database, child_at(limit_clause, 0U), &out_limit->row_count);
    if (rc != MYLITE_OK) {
        return rc;
    }
    out_limit->has_limit = true;

    if (child_at(limit_clause, 1U) != NULL) {
        rc =
            convert_limit_integer_literal(database, child_at(limit_clause, 1U), &out_limit->offset);
        if (rc != MYLITE_OK) {
            return rc;
        }
        out_limit->has_offset = true;
    }

    return MYLITE_OK;
}

static int plan_delete_limit(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *limit_clause,
    struct planned_select_limit *out_limit
) {
    int rc = MYLITE_OK;

    out_limit->has_limit = false;
    out_limit->row_count = 0;
    out_limit->has_offset = false;
    out_limit->offset = 0;
    if (limit_clause == NULL) {
        return MYLITE_OK;
    }
    if (limit_clause->kind != MYLITE_SQL_AST_LIMIT_CLAUSE) {
        set_unsupported_error(database, "DELETE supports only LIMIT row_count");
        return MYLITE_ERROR;
    }
    if (child_at(limit_clause, 1U) != NULL) {
        set_unsupported_error(database, "DELETE supports only LIMIT row_count");
        return MYLITE_ERROR;
    }

    rc = convert_limit_integer_literal(database, child_at(limit_clause, 0U), &out_limit->row_count);
    if (rc != MYLITE_OK) {
        return rc;
    }
    out_limit->has_limit = true;

    return MYLITE_OK;
}

static int plan_update_assignment(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment_list,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_update *out_plan
) {
    const struct mylite_sql_ast_node *assignment = child_at(assignment_list, 0U);
    const struct mylite_sql_ast_node *target = NULL;
    char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t column_index = 0U;
    int rc = MYLITE_OK;

    if (assignment_list == NULL || assignment_list->kind != MYLITE_SQL_AST_UPDATE_ASSIGNMENT_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_child_count(assignment_list) != 1U) {
        set_unsupported_error(database, "UPDATE supports exactly one assignment");
        return MYLITE_ERROR;
    }
    if (assignment == NULL || assignment->kind != MYLITE_SQL_AST_UPDATE_ASSIGNMENT) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    target = child_at(assignment, 0U);
    if (target == NULL || target->kind != MYLITE_SQL_AST_IDENTIFIER) {
        set_unsupported_error(database, "UPDATE supports only unqualified assignment columns");
        return MYLITE_ERROR;
    }
    rc = copy_identifier_text(target, column_name, sizeof(column_name), database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = find_column_index(table_columns, table_column_count, column_name, &column_index);
    if (rc != MYLITE_OK) {
        set_unknown_column_error(database, column_name);
        return MYLITE_ERROR;
    }

    out_plan->assignment_column = table_columns[column_index];
    out_plan->assignment_value_node = child_at(assignment, 1U);
    if (out_plan->assignment_value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int convert_update_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    if (value_node == NULL || column == NULL || out_value == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    *out_value = (struct planned_value){.is_null = true, .integer = 0};
    if (value_node->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_NULL) {
        if (!column->is_nullable) {
            set_bad_null_error(database, column->name);
            return MYLITE_ERROR;
        }
        return MYLITE_OK;
    }

    return convert_update_integer_literal(database, value_node, column, out_value);
}

static int convert_update_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    const struct mylite_sql_ast_node *literal = value_node;
    bool is_negative = false;
    uint64_t magnitude = 0U;
    int rc = MYLITE_OK;

    if (value_node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(value_node);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_unsupported_error(
                database,
                "UPDATE supports only integer and NULL assignment values"
            );
            return MYLITE_ERROR;
        }
        literal = child_at(value_node, 0U);
    }
    if (!boolean_literal_magnitude(literal, &magnitude)) {
        if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
            mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
            set_unsupported_error(
                database,
                "UPDATE supports only integer, boolean, and NULL assignment values"
            );
            return MYLITE_ERROR;
        }

        rc = parse_unsigned_integer_literal(&literal->span, &magnitude);
        if (rc != MYLITE_OK) {
            set_out_of_range_error(database, column->name, 1U);
            return MYLITE_ERROR;
        }
    }

    out_value->is_null = false;
    rc = convert_integer_for_column(
        database,
        magnitude,
        is_negative,
        column,
        1U,
        &out_value->integer
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    return MYLITE_OK;
}

static int plan_update_limit(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *limit_clause,
    struct planned_select_limit *out_limit
) {
    int rc = MYLITE_OK;

    out_limit->has_limit = false;
    out_limit->row_count = 0;
    out_limit->has_offset = false;
    out_limit->offset = 0;
    if (limit_clause == NULL) {
        return MYLITE_OK;
    }
    if (limit_clause->kind != MYLITE_SQL_AST_LIMIT_CLAUSE) {
        set_unsupported_error(database, "UPDATE supports only LIMIT row_count");
        return MYLITE_ERROR;
    }
    if (child_at(limit_clause, 1U) != NULL) {
        set_unsupported_error(database, "UPDATE supports only LIMIT row_count");
        return MYLITE_ERROR;
    }

    rc = convert_limit_integer_literal(database, child_at(limit_clause, 0U), &out_limit->row_count);
    if (rc != MYLITE_OK) {
        return rc;
    }
    out_limit->has_limit = true;

    return MYLITE_OK;
}

static int convert_limit_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    int64_t *out_value
) {
    const uint64_t sqlite_int64_positive_max = 9223372036854775807ULL;
    uint64_t magnitude = 0U;
    int rc = MYLITE_OK;

    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_unsupported_error(database, "LIMIT supports only unsigned decimal integer literals");
        return MYLITE_ERROR;
    }

    rc = parse_unsigned_integer_literal(&value_node->span, &magnitude);
    if (rc != MYLITE_OK || magnitude > sqlite_int64_positive_max) {
        set_limit_out_of_range_error(database);
        return MYLITE_ERROR;
    }

    *out_value = (int64_t)magnitude;
    return MYLITE_OK;
}

static int integer_range_for_column(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    const char *unsupported_message,
    struct integer_column_range *out_range
) {
    const uint64_t int_signed_positive_max = 2147483647ULL;
    const uint64_t int_signed_negative_abs_max = 2147483648ULL;
    const uint64_t int_unsigned_max = 4294967295ULL;
    const uint64_t mediumint_signed_positive_max = 8388607ULL;
    const uint64_t mediumint_signed_negative_abs_max = 8388608ULL;
    const uint64_t mediumint_unsigned_max = 16777215ULL;
    const uint64_t smallint_signed_positive_max = 32767ULL;
    const uint64_t smallint_signed_negative_abs_max = 32768ULL;
    const uint64_t smallint_unsigned_max = 65535ULL;
    const uint64_t tinyint_signed_positive_max = 127ULL;
    const uint64_t tinyint_signed_negative_abs_max = 128ULL;
    const uint64_t tinyint_unsigned_max = 255ULL;
    const uint64_t bigint_signed_positive_max = 9223372036854775807ULL;
    const uint64_t bigint_signed_negative_abs_max = 9223372036854775808ULL;

    if (strcmp(column->logical_type, "TINYINT") == 0) {
        *out_range = (struct integer_column_range){
            .positive_max = tinyint_signed_positive_max,
            .negative_abs_max = tinyint_signed_negative_abs_max,
        };
        return MYLITE_OK;
    }
    if (strcmp(column->logical_type, "TINYINT(1)") == 0) {
        *out_range = (struct integer_column_range){
            .positive_max = tinyint_signed_positive_max,
            .negative_abs_max = tinyint_signed_negative_abs_max,
        };
        return MYLITE_OK;
    }
    if (strcmp(column->logical_type, "TINYINT UNSIGNED") == 0) {
        *out_range = (struct integer_column_range){
            .positive_max = tinyint_unsigned_max,
            .negative_abs_max = 0U,
        };
        return MYLITE_OK;
    }
    if (strcmp(column->logical_type, "SMALLINT") == 0) {
        *out_range = (struct integer_column_range){
            .positive_max = smallint_signed_positive_max,
            .negative_abs_max = smallint_signed_negative_abs_max,
        };
        return MYLITE_OK;
    }
    if (strcmp(column->logical_type, "SMALLINT UNSIGNED") == 0) {
        *out_range = (struct integer_column_range){
            .positive_max = smallint_unsigned_max,
            .negative_abs_max = 0U,
        };
        return MYLITE_OK;
    }
    if (strcmp(column->logical_type, "MEDIUMINT") == 0) {
        *out_range = (struct integer_column_range){
            .positive_max = mediumint_signed_positive_max,
            .negative_abs_max = mediumint_signed_negative_abs_max,
        };
        return MYLITE_OK;
    }
    if (strcmp(column->logical_type, "MEDIUMINT UNSIGNED") == 0) {
        *out_range = (struct integer_column_range){
            .positive_max = mediumint_unsigned_max,
            .negative_abs_max = 0U,
        };
        return MYLITE_OK;
    }
    if (strcmp(column->logical_type, "INT") == 0) {
        *out_range = (struct integer_column_range){
            .positive_max = int_signed_positive_max,
            .negative_abs_max = int_signed_negative_abs_max,
        };
        return MYLITE_OK;
    }
    if (strcmp(column->logical_type, "INT UNSIGNED") == 0) {
        *out_range = (struct integer_column_range){
            .positive_max = int_unsigned_max,
            .negative_abs_max = 0U,
        };
        return MYLITE_OK;
    }
    if (strcmp(column->logical_type, "BIGINT") == 0) {
        *out_range = (struct integer_column_range){
            .positive_max = bigint_signed_positive_max,
            .negative_abs_max = bigint_signed_negative_abs_max,
        };
        return MYLITE_OK;
    }
    if (strcmp(column->logical_type, "BIGINT UNSIGNED") == 0) {
        *out_range = (struct integer_column_range){
            .positive_max = bigint_signed_positive_max,
            .negative_abs_max = 0U,
        };
        return MYLITE_OK;
    }

    set_unsupported_error(database, unsupported_message);
    return MYLITE_ERROR;
}

static int append_show_table(const struct mylite_catalog_table_descriptor *table, void *user_data) {
    struct show_tables_context *context = user_data;
    const char *values[1] = {NULL};

    if (table == NULL || context == NULL || context->result == NULL) {
        return MYLITE_MISUSE;
    }

    if (!show_like_filter_matches(context->filter, table->name, true)) {
        return MYLITE_OK;
    }

    values[0] = table->name;

    return mylite_result_append_text_row(context->result, values);
}

static int append_show_table_status(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
) {
    struct show_table_status_context *context = user_data;
    int64_t row_count = 0;
    int64_t average_row_length = 0;
    char row_count_text[integer_text_capacity];
    char average_row_length_text[integer_text_capacity];
    const char *values[show_table_status_result_column_count] = {
        NULL,
        "InnoDB",
        "10",
        "Dynamic",
        row_count_text,
        average_row_length_text,
        "16384",
        "0",
        "0",
        "0",
        NULL,
        NULL,
        NULL,
        NULL,
        "utf8mb4_0900_ai_ci",
        NULL,
        "",
        "",
    };
    int rc = MYLITE_OK;

    if (table == NULL || context == NULL || context->database == NULL || context->result == NULL) {
        return MYLITE_MISUSE;
    }
    if (table->kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        return MYLITE_OK;
    }
    if (!show_like_filter_matches(context->filter, table->name, true)) {
        return MYLITE_OK;
    }

    rc = read_show_table_status_row_count(context->database, table, &row_count);
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(context->database);
        return rc;
    }
    if (rc != MYLITE_OK) {
        set_runtime_error(context->database, "failed to read SHOW TABLE STATUS row count");
        return MYLITE_ERROR;
    }

    if (row_count > 0) {
        average_row_length = show_table_status_data_length / row_count;
    }
    rc = format_show_table_status_integer(
        context->database,
        row_count,
        row_count_text,
        sizeof(row_count_text)
    );
    if (rc == MYLITE_OK) {
        rc = format_show_table_status_integer(
            context->database,
            average_row_length,
            average_row_length_text,
            sizeof(average_row_length_text)
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    values[0] = table->name;

    rc = mylite_result_append_text_row(context->result, values);
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(context->database);
    }
    return rc;
}

static int append_show_column(
    const struct mylite_catalog_column_descriptor *column,
    void *user_data
) {
    struct show_columns_context *context = user_data;
    const char *type_text = NULL;
    char default_text[integer_text_capacity];
    const char *values[show_columns_result_column_count] = {NULL, NULL, NULL, "", NULL, ""};
    int rc = MYLITE_OK;

    if (column == NULL || context == NULL || context->result == NULL) {
        return MYLITE_MISUSE;
    }

    if (!show_like_filter_matches(context->filter, column->name, false)) {
        return MYLITE_OK;
    }

    rc = show_column_type_text(context->database, column->logical_type, &type_text);
    if (rc != MYLITE_OK) {
        return rc;
    }

    values[0] = column->name;
    values[1] = type_text;
    values[2] = "NO";
    if (column->is_nullable) {
        values[2] = "YES";
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER) {
        int written =
            snprintf(default_text, sizeof(default_text), "%" PRId64, column->default_integer);

        if (written < 0 || (size_t)written >= sizeof(default_text)) {
            set_runtime_error(context->database, "failed to format column default");
            return MYLITE_ERROR;
        }
        values[4] = default_text;
    }
    if (!column->is_visible) {
        values[show_columns_extra_column] = "INVISIBLE";
    }

    return mylite_result_append_text_row(context->result, values);
}

static int show_column_type_text(
    struct mylite_db *database,
    const char *logical_type,
    const char **out_type_text
) {
    if (logical_type == NULL || out_type_text == NULL) {
        set_runtime_error(database, "invalid column descriptor");
        return MYLITE_ERROR;
    }
    if (strcmp(logical_type, "INT") == 0 || strcmp(logical_type, "INTEGER") == 0) {
        *out_type_text = "int";
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "TINYINT") == 0) {
        *out_type_text = "tinyint";
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "TINYINT(1)") == 0) {
        *out_type_text = "tinyint(1)";
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "SMALLINT") == 0) {
        *out_type_text = "smallint";
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "MEDIUMINT") == 0) {
        *out_type_text = "mediumint";
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "INT UNSIGNED") == 0 ||
        strcmp(logical_type, "INTEGER UNSIGNED") == 0) {
        *out_type_text = "int unsigned";
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "TINYINT UNSIGNED") == 0) {
        *out_type_text = "tinyint unsigned";
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "SMALLINT UNSIGNED") == 0) {
        *out_type_text = "smallint unsigned";
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "MEDIUMINT UNSIGNED") == 0) {
        *out_type_text = "mediumint unsigned";
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "BIGINT") == 0) {
        *out_type_text = "bigint";
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "BIGINT UNSIGNED") == 0) {
        *out_type_text = "bigint unsigned";
        return MYLITE_OK;
    }

    set_unsupported_error(database, "SHOW COLUMNS supports only integer column descriptors");
    return MYLITE_ERROR;
}

static int append_show_database(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
) {
    struct show_databases_context *context = user_data;
    const char *values[1] = {NULL};

    if (schema == NULL || context == NULL || context->result == NULL) {
        return MYLITE_MISUSE;
    }

    if (!show_like_filter_matches(context->filter, schema->name, true)) {
        return MYLITE_OK;
    }

    values[0] = schema->name;

    return mylite_result_append_text_row(context->result, values);
}

static int make_show_like_filter(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *pattern_node,
    struct show_like_filter *out_filter
) {
    char *pattern = NULL;
    int rc = MYLITE_OK;

    if (out_filter == NULL) {
        set_runtime_error(database, "invalid SHOW LIKE filter");
        return MYLITE_ERROR;
    }
    *out_filter = (struct show_like_filter){
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
    if (pattern_node == NULL) {
        return MYLITE_OK;
    }

    rc = decode_show_like_pattern(database, pattern_node, &pattern);
    if (rc != MYLITE_OK) {
        return rc;
    }

    out_filter->has_pattern = true;
    out_filter->pattern = pattern;
    out_filter->pattern_length = strlen(pattern);
    return MYLITE_OK;
}

static void show_like_filter_deinit(struct show_like_filter *filter) {
    if (filter == NULL) {
        return;
    }

    free(filter->pattern);
    *filter = (struct show_like_filter){
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
}

static bool show_like_filter_matches(
    const struct show_like_filter *filter,
    const char *value,
    bool case_sensitive
) {
    if (filter == NULL || !filter->has_pattern) {
        return true;
    }
    if (value == NULL) {
        return false;
    }

    return show_like_pattern_matches(
        filter->pattern,
        filter->pattern_length,
        value,
        strlen(value),
        case_sensitive
    );
}

static int build_show_databases_column_name(
    const struct show_like_filter *filter,
    char **out_name
) {
    struct dynamic_string string;
    int rc = MYLITE_OK;

    if (out_name == NULL) {
        return MYLITE_MISUSE;
    }
    *out_name = NULL;

    dynamic_string_init(&string);
    rc = dynamic_string_append(&string, "Database");
    if (rc == MYLITE_OK && filter != NULL && filter->has_pattern) {
        rc = dynamic_string_append(&string, " (");
        if (rc == MYLITE_OK) {
            rc = dynamic_string_append(&string, filter->pattern);
        }
        if (rc == MYLITE_OK) {
            rc = dynamic_string_append_char(&string, ')');
        }
    }
    if (rc == MYLITE_OK) {
        *out_name = dynamic_string_take(&string);
        if (*out_name == NULL) {
            rc = MYLITE_NOMEM;
        }
    }
    dynamic_string_deinit(&string);
    return rc;
}

static int build_show_tables_column_name(
    const char *schema_name,
    const struct show_like_filter *filter,
    char **out_name
) {
    struct dynamic_string string;
    int rc = MYLITE_OK;

    if (schema_name == NULL || out_name == NULL) {
        return MYLITE_MISUSE;
    }
    *out_name = NULL;

    dynamic_string_init(&string);
    rc = dynamic_string_append(&string, "Tables_in_");
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append(&string, schema_name);
    }
    if (rc == MYLITE_OK && filter != NULL && filter->has_pattern) {
        rc = dynamic_string_append(&string, " (");
        if (rc == MYLITE_OK) {
            rc = dynamic_string_append(&string, filter->pattern);
        }
        if (rc == MYLITE_OK) {
            rc = dynamic_string_append_char(&string, ')');
        }
    }
    if (rc == MYLITE_OK) {
        *out_name = dynamic_string_take(&string);
        if (*out_name == NULL) {
            rc = MYLITE_NOMEM;
        }
    }
    dynamic_string_deinit(&string);
    return rc;
}

static int read_show_table_status_row_count(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    int64_t *out_count
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int rc = build_show_table_status_count_sql(table, &sql);

    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = step_count_statement(statement, out_count);
    }

    rc = finalize_sqlite_statement(statement, rc);
    free(sql);

    return rc;
}

static int build_show_table_status_count_sql(
    const struct mylite_catalog_table_descriptor *table,
    char **out_sql
) {
    struct dynamic_string string;
    int rc = MYLITE_OK;

    if (table == NULL || out_sql == NULL) {
        return MYLITE_MISUSE;
    }
    *out_sql = NULL;
    dynamic_string_init(&string);

    rc = dynamic_string_append(&string, "SELECT COUNT(*) FROM ");
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_quoted_identifier(&string, table->physical_name);
    }
    if (rc == MYLITE_OK) {
        *out_sql = dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    dynamic_string_deinit(&string);

    return rc;
}

static int format_show_table_status_integer(
    struct mylite_db *database,
    int64_t value,
    char *buffer,
    size_t buffer_size
) {
    int written = snprintf(buffer, buffer_size, "%" PRId64, value);

    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format SHOW TABLE STATUS value");
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int decode_show_like_pattern(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *pattern_node,
    char **out_pattern
) {
    struct dynamic_string string;
    const char *text = NULL;
    size_t length = 0U;
    char quote = '\0';
    int rc = MYLITE_OK;

    if (out_pattern == NULL) {
        set_runtime_error(database, "invalid SHOW LIKE pattern");
        return MYLITE_ERROR;
    }
    *out_pattern = NULL;
    if (pattern_node == NULL || pattern_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(pattern_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(database, "SHOW LIKE supports only string literal patterns");
        return MYLITE_ERROR;
    }

    text = pattern_node->span.text;
    length = pattern_node->span.length;
    if (text == NULL || length < 2U) {
        set_runtime_error(database, "invalid SHOW LIKE pattern");
        return MYLITE_ERROR;
    }

    quote = text[0];
    dynamic_string_init(&string);
    for (size_t index = 1U; rc == MYLITE_OK && index + 1U < length; ++index) {
        char byte = text[index];

        if (byte == quote && index + 2U < length && text[index + 1U] == quote) {
            rc = dynamic_string_append_char(&string, quote);
            ++index;
        } else if (byte == '\\' && index + 2U < length) {
            ++index;
            rc = append_decoded_string_escape(database, &string, text[index]);
        } else {
            rc = dynamic_string_append_char(&string, byte);
        }
    }
    if (rc == MYLITE_OK && string.text == NULL) {
        rc = dynamic_string_append(&string, "");
    }
    if (rc == MYLITE_OK) {
        *out_pattern = dynamic_string_take(&string);
        if (*out_pattern == NULL) {
            rc = MYLITE_NOMEM;
        }
    }
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    dynamic_string_deinit(&string);
    return rc;
}

static int append_decoded_string_escape(
    struct mylite_db *database,
    struct dynamic_string *string,
    char escaped_byte
) {
    switch (escaped_byte) {
    case '0':
        set_unsupported_error(database, "SHOW LIKE does not support NUL bytes in patterns");
        return MYLITE_ERROR;
    case 'n':
        return dynamic_string_append_char(string, '\n');
    case 'r':
        return dynamic_string_append_char(string, '\r');
    case 't':
        return dynamic_string_append_char(string, '\t');
    case 'b':
        return dynamic_string_append_char(string, '\b');
    case 'Z':
        return dynamic_string_append_char(string, '\032');
    case '\\':
        return dynamic_string_append_char(string, '\\');
    case '\'':
        return dynamic_string_append_char(string, '\'');
    case '"':
        return dynamic_string_append_char(string, '"');
    case '%':
        if (dynamic_string_append_char(string, '\\') != MYLITE_OK) {
            return MYLITE_NOMEM;
        }
        return dynamic_string_append_char(string, '%');
    case '_':
        if (dynamic_string_append_char(string, '\\') != MYLITE_OK) {
            return MYLITE_NOMEM;
        }
        return dynamic_string_append_char(string, '_');
    default:
        return dynamic_string_append_char(string, escaped_byte);
    }
}

static bool show_like_pattern_matches(
    const char *pattern,
    size_t pattern_length,
    const char *value,
    size_t value_length,
    bool case_sensitive
) {
    const size_t no_retry_pattern = SIZE_MAX;
    size_t pattern_index = 0U;
    size_t value_index = 0U;
    size_t retry_pattern_index = no_retry_pattern;
    size_t retry_value_index = 0U;

    while (value_index < value_length) {
        size_t next_pattern_index = pattern_index;

        if (pattern_index < pattern_length && pattern[pattern_index] == '%') {
            pattern_index = show_like_skip_percent_run(pattern, pattern_length, pattern_index);
            if (pattern_index == pattern_length) {
                return true;
            }
            retry_pattern_index = pattern_index;
            retry_value_index = value_index;
            continue;
        }
        if (show_like_pattern_item_matches(
                (struct show_like_pattern_item_request){
                    .pattern = pattern,
                    .pattern_length = pattern_length,
                    .pattern_index = pattern_index,
                    .value_byte = value[value_index],
                    .case_sensitive = case_sensitive,
                },
                &next_pattern_index
            )) {
            pattern_index = next_pattern_index;
            ++value_index;
            continue;
        }
        if (retry_pattern_index == no_retry_pattern || retry_value_index >= value_length) {
            return false;
        }
        ++retry_value_index;
        value_index = retry_value_index;
        pattern_index = retry_pattern_index;
    }

    pattern_index = show_like_skip_percent_run(pattern, pattern_length, pattern_index);

    return pattern_index == pattern_length;
}

static size_t show_like_skip_percent_run(
    const char *pattern,
    size_t pattern_length,
    size_t pattern_index
) {
    while (pattern_index < pattern_length && pattern[pattern_index] == '%') {
        ++pattern_index;
    }
    return pattern_index;
}

static bool show_like_pattern_item_matches(
    struct show_like_pattern_item_request request,
    size_t *out_next_pattern_index
) {
    char pattern_byte = '\0';
    size_t next_pattern_index = request.pattern_index;

    if (request.pattern_index >= request.pattern_length || out_next_pattern_index == NULL) {
        return false;
    }

    pattern_byte = request.pattern[request.pattern_index];
    if (pattern_byte == '_') {
        *out_next_pattern_index = request.pattern_index + 1U;
        return true;
    }
    if (pattern_byte == '\\' && request.pattern_index + 1U < request.pattern_length) {
        ++next_pattern_index;
        pattern_byte = request.pattern[next_pattern_index];
    }
    ++next_pattern_index;
    if (!show_like_bytes_equal(pattern_byte, request.value_byte, request.case_sensitive)) {
        return false;
    }

    *out_next_pattern_index = next_pattern_index;
    return true;
}

static bool show_like_bytes_equal(char left, char right, bool case_sensitive) {
    if (case_sensitive) {
        return left == right;
    }

    return show_like_ascii_lower(left) == show_like_ascii_lower(right);
}

static char show_like_ascii_lower(char byte) {
    if (byte >= 'A' && byte <= 'Z') {
        return (char)(byte - 'A' + 'a');
    }
    return byte;
}

static int build_physical_table_name(int64_t table_id, char *destination, size_t destination_size) {
    int written = snprintf(destination, destination_size, "_mylite_user_table_%" PRId64, table_id);

    if (written < 0 || (size_t)written >= destination_size) {
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int build_create_table_sql(
    const struct planned_create_table *plan,
    const char *physical_name,
    char **out_sql
) {
    struct dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    dynamic_string_init(&string);

    rc = dynamic_string_append(&string, "CREATE TABLE ");
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_quoted_identifier(&string, physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append(&string, " (");
    }
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        const struct planned_column *column = &plan->columns[column_index];

        if (column_index != 0U) {
            rc = dynamic_string_append(&string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = dynamic_string_append_quoted_identifier(&string, column->name);
        }
        if (rc == MYLITE_OK) {
            rc = dynamic_string_append(&string, " INTEGER");
        }
        if (rc == MYLITE_OK && !column->is_nullable) {
            rc = dynamic_string_append(&string, " NOT NULL");
        }
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_char(&string, ')');
    }
    if (rc == MYLITE_OK) {
        *out_sql = dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    dynamic_string_deinit(&string);

    return rc;
}

static int build_drop_table_sql(const char *physical_name, char **out_sql) {
    struct dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    dynamic_string_init(&string);

    rc = dynamic_string_append(&string, "DROP TABLE ");
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_quoted_identifier(&string, physical_name);
    }
    if (rc == MYLITE_OK) {
        *out_sql = dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    dynamic_string_deinit(&string);

    return rc;
}

static int build_alter_table_add_column_sql(
    const struct planned_alter_table_add_column *plan,
    char **out_sql
) {
    struct dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    dynamic_string_init(&string);

    rc = dynamic_string_append(&string, "ALTER TABLE ");
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append(&string, " ADD COLUMN ");
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_quoted_identifier(&string, plan->column.name);
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append(&string, " INTEGER");
    }
    if (rc == MYLITE_OK && !plan->column.is_nullable) {
        rc = dynamic_string_append(&string, " NOT NULL");
    }
    if (rc == MYLITE_OK && plan->column.default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER) {
        char default_text[integer_text_capacity];
        int written = snprintf(
            default_text,
            sizeof(default_text),
            " DEFAULT %" PRId64,
            plan->column.default_integer
        );

        if (written < 0 || (size_t)written >= sizeof(default_text)) {
            rc = MYLITE_NOMEM;
        } else {
            rc = dynamic_string_append(&string, default_text);
        }
    }
    if (rc == MYLITE_OK && !plan->column.is_nullable &&
        plan->column.default_kind != MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER) {
        rc = dynamic_string_append(&string, " DEFAULT 0");
    }
    if (rc == MYLITE_OK) {
        *out_sql = dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    dynamic_string_deinit(&string);

    return rc;
}

static int build_alter_table_drop_column_sql(
    const struct planned_alter_table_drop_column *plan,
    char **out_sql
) {
    struct dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    dynamic_string_init(&string);

    rc = dynamic_string_append(&string, "ALTER TABLE ");
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append(&string, " DROP COLUMN ");
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_quoted_identifier(&string, plan->column.name);
    }
    if (rc == MYLITE_OK) {
        *out_sql = dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    dynamic_string_deinit(&string);

    return rc;
}

static int build_alter_table_rename_column_sql(
    const struct planned_alter_table_rename_column *plan,
    char **out_sql
) {
    struct dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    dynamic_string_init(&string);

    rc = dynamic_string_append(&string, "ALTER TABLE ");
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append(&string, " RENAME COLUMN ");
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_quoted_identifier(&string, plan->column.name);
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append(&string, " TO ");
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_quoted_identifier(&string, plan->new_column_name);
    }
    if (rc == MYLITE_OK) {
        *out_sql = dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    dynamic_string_deinit(&string);

    return rc;
}

static int build_modify_temporary_physical_name(
    const struct planned_alter_table_modify_column *plan,
    const struct mylite_catalog_mutation *mutation,
    char *destination,
    size_t destination_size
) {
    int written = 0;

    if (plan == NULL || mutation == NULL || destination == NULL || destination_size == 0U) {
        return MYLITE_MISUSE;
    }

    written = snprintf(
        destination,
        destination_size,
        "_mylite_user_table_%" PRId64 "_modify_%" PRIu64,
        plan->table.table_id,
        mylite_catalog_mutation_generation(mutation)
    );
    if (written < 0 || (size_t)written >= destination_size) {
        return MYLITE_NOMEM;
    }

    return MYLITE_OK;
}

static int build_alter_table_modify_validation_sql(
    const struct planned_alter_table_modify_column *plan,
    char **out_sql
) {
    struct dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    dynamic_string_init(&string);

    rc = dynamic_string_append(&string, "SELECT ");
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_quoted_identifier(&string, plan->original_column.name);
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append(&string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append(&string, " ORDER BY ");
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append(&string, plan->rowid_alias);
    }
    if (rc == MYLITE_OK) {
        *out_sql = dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    dynamic_string_deinit(&string);

    return rc;
}

static int build_alter_table_modify_create_sql(
    const struct planned_alter_table_modify_column *plan,
    const char *temporary_physical_name,
    char **out_sql
) {
    struct dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    dynamic_string_init(&string);

    rc = dynamic_string_append(&string, "CREATE TABLE ");
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_quoted_identifier(&string, temporary_physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append(&string, " (");
    }
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        const struct mylite_catalog_column_descriptor *column = &plan->columns[column_index];

        if (column_index != 0U) {
            rc = dynamic_string_append(&string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = dynamic_string_append_quoted_identifier(&string, column->name);
        }
        if (rc == MYLITE_OK) {
            rc = dynamic_string_append(&string, " INTEGER");
        }
        if (rc == MYLITE_OK && !column->is_nullable) {
            rc = dynamic_string_append(&string, " NOT NULL");
        }
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_char(&string, ')');
    }
    if (rc == MYLITE_OK) {
        *out_sql = dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    dynamic_string_deinit(&string);

    return rc;
}

static int build_alter_table_modify_copy_sql(
    const struct planned_alter_table_modify_column *plan,
    const char *temporary_physical_name,
    char **out_sql
) {
    struct dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    dynamic_string_init(&string);

    rc = dynamic_string_append(&string, "INSERT INTO ");
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_quoted_identifier(&string, temporary_physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append(&string, " (");
    }
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        if (column_index != 0U) {
            rc = dynamic_string_append(&string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = dynamic_string_append_quoted_identifier(&string, plan->columns[column_index].name);
        }
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append(&string, ") SELECT ");
    }
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        const char *source_name = plan->columns[column_index].name;

        if (column_index == plan->column_index) {
            source_name = plan->original_column.name;
        }
        if (column_index != 0U) {
            rc = dynamic_string_append(&string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = dynamic_string_append_quoted_identifier(&string, source_name);
        }
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append(&string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append(&string, " ORDER BY ");
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append(&string, plan->rowid_alias);
    }
    if (rc == MYLITE_OK) {
        *out_sql = dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    dynamic_string_deinit(&string);

    return rc;
}

static int build_alter_table_rename_physical_table_sql(
    const char *source_physical_name,
    const char *target_physical_name,
    char **out_sql
) {
    struct dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    dynamic_string_init(&string);

    rc = dynamic_string_append(&string, "ALTER TABLE ");
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_quoted_identifier(&string, source_physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append(&string, " RENAME TO ");
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_quoted_identifier(&string, target_physical_name);
    }
    if (rc == MYLITE_OK) {
        *out_sql = dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    dynamic_string_deinit(&string);

    return rc;
}

static int build_truncate_table_sql(const struct planned_truncate_table *plan, char **out_sql) {
    struct dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    dynamic_string_init(&string);

    rc = dynamic_string_append(&string, "DELETE FROM ");
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        *out_sql = dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    dynamic_string_deinit(&string);

    return rc;
}

static int build_insert_sql(const struct planned_insert *plan, char **out_sql) {
    struct dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    dynamic_string_init(&string);

    rc = dynamic_string_append(&string, "INSERT INTO ");
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append(&string, " (");
    }
    if (rc == MYLITE_OK) {
        rc = append_insert_column_names(&string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append(&string, ") VALUES (");
    }
    if (rc == MYLITE_OK) {
        rc = append_insert_parameters(&string, plan->column_count);
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_char(&string, ')');
    }
    if (rc == MYLITE_OK) {
        *out_sql = dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    dynamic_string_deinit(&string);

    return rc;
}

static int append_insert_column_names(
    struct dynamic_string *string,
    const struct planned_insert *plan
) {
    int rc = MYLITE_OK;

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        if (column_index != 0U) {
            rc = dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = dynamic_string_append_quoted_identifier(string, plan->columns[column_index].name);
        }
    }

    return rc;
}

static int append_insert_parameters(struct dynamic_string *string, size_t column_count) {
    int rc = MYLITE_OK;

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < column_count; ++column_index) {
        if (column_index != 0U) {
            rc = dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_numbered_parameter(string, column_index + 1U);
        }
    }

    return rc;
}

static int append_numbered_parameter(struct dynamic_string *string, size_t parameter_index) {
    char parameter[integer_text_capacity];
    int written = snprintf(parameter, sizeof(parameter), "?%zu", parameter_index);

    if (written < 0 || (size_t)written >= sizeof(parameter)) {
        return MYLITE_NOMEM;
    }

    return dynamic_string_append(string, parameter);
}

static int build_select_sql(const struct planned_select *plan, char **out_sql) {
    struct dynamic_string string;
    size_t next_parameter = 1U;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    dynamic_string_init(&string);

    if (plan->is_distinct) {
        rc = dynamic_string_append(&string, "SELECT DISTINCT ");
    } else {
        rc = dynamic_string_append(&string, "SELECT ");
    }
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        if (column_index != 0U) {
            rc = dynamic_string_append(&string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = dynamic_string_append_quoted_identifier(&string, plan->columns[column_index].name);
        }
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append(&string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_predicate_sql(&string, &plan->predicate, &next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_order_sql(&string, &plan->order);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_limit_sql(&string, &plan->limit, &next_parameter);
    }
    if (rc == MYLITE_OK) {
        *out_sql = dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    dynamic_string_deinit(&string);

    return rc;
}

static int build_count_sql(const struct planned_count *plan, char **out_sql) {
    struct dynamic_string string;
    size_t next_parameter = plan->function == PLANNED_COUNT_LITERAL ? 2U : 1U;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    dynamic_string_init(&string);

    rc = dynamic_string_append(&string, "SELECT COUNT(");
    if (rc == MYLITE_OK && plan->function == PLANNED_COUNT_STAR) {
        rc = dynamic_string_append(&string, "*");
    }
    if (rc == MYLITE_OK && plan->function == PLANNED_COUNT_COLUMN) {
        rc = dynamic_string_append_quoted_identifier(&string, plan->count_column.name);
    }
    if (rc == MYLITE_OK && plan->function == PLANNED_COUNT_DISTINCT_COLUMN) {
        rc = dynamic_string_append(&string, "DISTINCT ");
        if (rc == MYLITE_OK) {
            rc = dynamic_string_append_quoted_identifier(&string, plan->count_column.name);
        }
    }
    if (rc == MYLITE_OK && plan->function == PLANNED_COUNT_LITERAL) {
        rc = append_numbered_parameter(&string, 1U);
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append(&string, ") FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_predicate_sql(&string, &plan->predicate, &next_parameter);
    }
    if (rc == MYLITE_OK) {
        *out_sql = dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    dynamic_string_deinit(&string);

    return rc;
}

static int build_min_max_aggregate_sql(
    const struct planned_min_max_aggregate *plan,
    char **out_sql
) {
    struct dynamic_string string;
    size_t next_parameter = 1U;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    dynamic_string_init(&string);

    rc = dynamic_string_append(&string, "SELECT ");
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append(&string, min_max_aggregate_sql_function(plan->function));
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_char(&string, '(');
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_quoted_identifier(&string, plan->aggregate_column.name);
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append(&string, ") FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_predicate_sql(&string, &plan->predicate, &next_parameter);
    }
    if (rc == MYLITE_OK) {
        *out_sql = dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    dynamic_string_deinit(&string);

    return rc;
}

static const char *min_max_aggregate_sql_function(
    enum planned_min_max_aggregate_function function
) {
    switch (function) {
    case PLANNED_MIN_MAX_AGGREGATE_NONE:
        return "";
    case PLANNED_MIN_MAX_AGGREGATE_MIN:
        return "MIN";
    case PLANNED_MIN_MAX_AGGREGATE_MAX:
        return "MAX";
    }

    return "";
}

static int append_select_predicate_sql(
    struct dynamic_string *string,
    const struct planned_select_predicate *predicate,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (predicate->kind == PLANNED_SELECT_PREDICATE_NONE) {
        return MYLITE_OK;
    }

    rc = dynamic_string_append(string, " WHERE ");
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_quoted_identifier(string, predicate->column.name);
    }
    if (predicate->kind == PLANNED_SELECT_PREDICATE_COMPARISON) {
        if (rc == MYLITE_OK) {
            rc = dynamic_string_append_char(string, ' ');
        }
        if (rc == MYLITE_OK) {
            rc = dynamic_string_append(string, comparison_operator_sql(predicate->operator_kind));
        }
        if (rc == MYLITE_OK) {
            rc = dynamic_string_append_char(string, ' ');
        }
        if (rc == MYLITE_OK) {
            rc = append_numbered_parameter(string, *next_parameter);
        }
        if (rc == MYLITE_OK) {
            ++(*next_parameter);
        }
    } else if (predicate->kind == PLANNED_SELECT_PREDICATE_IS_NULL) {
        if (predicate->operator_kind == MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL) {
            if (rc == MYLITE_OK) {
                rc = dynamic_string_append(string, " IS NOT NULL");
            }
        } else {
            if (rc == MYLITE_OK) {
                rc = dynamic_string_append(string, " IS NULL");
            }
        }
    }

    return rc;
}

static int append_select_order_sql(
    struct dynamic_string *string,
    const struct planned_select_order *order
) {
    int rc = MYLITE_OK;

    if (!order->has_order) {
        return MYLITE_OK;
    }

    rc = dynamic_string_append(string, " ORDER BY ");
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_quoted_identifier(string, order->column.name);
    }
    if (rc == MYLITE_OK) {
        if (order->direction == PLANNED_SELECT_ORDER_DESC) {
            rc = dynamic_string_append(string, " DESC");
        } else {
            rc = dynamic_string_append(string, " ASC");
        }
    }

    return rc;
}

static int append_select_limit_sql(
    struct dynamic_string *string,
    const struct planned_select_limit *limit,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (!limit->has_limit) {
        return MYLITE_OK;
    }

    rc = dynamic_string_append(string, " LIMIT ");
    if (rc == MYLITE_OK) {
        rc = append_numbered_parameter(string, *next_parameter);
    }
    if (rc == MYLITE_OK) {
        ++(*next_parameter);
    }
    if (rc == MYLITE_OK && limit->has_offset) {
        rc = dynamic_string_append(string, " OFFSET ");
    }
    if (rc == MYLITE_OK && limit->has_offset) {
        rc = append_numbered_parameter(string, *next_parameter);
    }
    if (rc == MYLITE_OK && limit->has_offset) {
        ++(*next_parameter);
    }

    return rc;
}

static int build_delete_sql(const struct planned_delete *plan, char **out_sql) {
    struct dynamic_string string;
    size_t next_parameter = 1U;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    dynamic_string_init(&string);

    rc = dynamic_string_append(&string, "DELETE FROM ");
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK && plan->limit.has_limit) {
        rc = append_delete_rowid_limited_sql(&string, plan, &next_parameter);
    } else if (rc == MYLITE_OK) {
        rc = append_select_predicate_sql(&string, &plan->predicate, &next_parameter);
    }
    if (rc == MYLITE_OK) {
        *out_sql = dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    dynamic_string_deinit(&string);

    return rc;
}

static int append_delete_rowid_limited_sql(
    struct dynamic_string *string,
    const struct planned_delete *plan,
    size_t *next_parameter
) {
    int rc = dynamic_string_append(string, " WHERE ");

    if (rc == MYLITE_OK) {
        rc = dynamic_string_append(string, plan->rowid_alias);
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append(string, " IN (SELECT ");
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append(string, plan->rowid_alias);
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append(string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_quoted_identifier(string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_predicate_sql(string, &plan->predicate, next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_order_sql(string, &plan->order);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_limit_sql(string, &plan->limit, next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_char(string, ')');
    }

    return rc;
}

static int build_update_sql(const struct planned_update *plan, char **out_sql) {
    struct dynamic_string string;
    size_t next_parameter = 2U;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    dynamic_string_init(&string);

    rc = dynamic_string_append(&string, "UPDATE ");
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append(&string, " SET ");
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_quoted_identifier(&string, plan->assignment_column.name);
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append(&string, " = ?1");
    }
    if (rc == MYLITE_OK && plan->limit.has_limit) {
        rc = append_update_rowid_limited_sql(&string, plan, &next_parameter);
    } else if (rc == MYLITE_OK && plan->predicate.kind != PLANNED_SELECT_PREDICATE_NONE) {
        rc = append_select_predicate_sql(&string, &plan->predicate, &next_parameter);
    } else if (rc == MYLITE_OK) {
        rc = dynamic_string_append(&string, " WHERE ");
    }
    if (rc == MYLITE_OK &&
        (plan->limit.has_limit || plan->predicate.kind != PLANNED_SELECT_PREDICATE_NONE)) {
        rc = dynamic_string_append(&string, " AND ");
    }
    if (rc == MYLITE_OK) {
        rc = append_update_changed_condition_sql(&string, plan, &next_parameter);
    }
    if (rc == MYLITE_OK) {
        *out_sql = dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    dynamic_string_deinit(&string);

    return rc;
}

static int build_update_matched_sql(const struct planned_update *plan, char **out_sql) {
    struct dynamic_string string;
    size_t next_parameter = 1U;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    dynamic_string_init(&string);

    rc = dynamic_string_append(&string, "SELECT 1 FROM ");
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_predicate_sql(&string, &plan->predicate, &next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append(&string, " LIMIT 1");
    }
    if (rc == MYLITE_OK) {
        *out_sql = dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    dynamic_string_deinit(&string);

    return rc;
}

static int append_update_rowid_limited_sql(
    struct dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
) {
    int rc = dynamic_string_append(string, " WHERE ");

    if (rc == MYLITE_OK) {
        rc = dynamic_string_append(string, plan->rowid_alias);
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append(string, " IN (SELECT ");
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append(string, plan->rowid_alias);
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append(string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_quoted_identifier(string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_predicate_sql(string, &plan->predicate, next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_order_sql(string, &plan->order);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_limit_sql(string, &plan->limit, next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_char(string, ')');
    }

    return rc;
}

static int append_update_changed_condition_sql(
    struct dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (plan->assignment_value.is_null) {
        rc = dynamic_string_append_quoted_identifier(string, plan->assignment_column.name);
        if (rc == MYLITE_OK) {
            rc = dynamic_string_append(string, " IS NOT NULL");
        }
        return rc;
    }

    rc = dynamic_string_append_char(string, '(');
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_quoted_identifier(string, plan->assignment_column.name);
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append(string, " IS NULL OR ");
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_quoted_identifier(string, plan->assignment_column.name);
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append(string, " <> ");
    }
    if (rc == MYLITE_OK) {
        rc = append_numbered_parameter(string, *next_parameter);
    }
    if (rc == MYLITE_OK) {
        ++(*next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_char(string, ')');
    }

    return rc;
}

static const char *comparison_operator_sql(enum mylite_sql_ast_operator operator_kind) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
        return "=";
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
        return "<>";
    case MYLITE_SQL_AST_OPERATOR_LESS:
        return "<";
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
        return "<=";
    case MYLITE_SQL_AST_OPERATOR_GREATER:
        return ">";
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
        return ">=";
    case MYLITE_SQL_AST_OPERATOR_NONE:
    case MYLITE_SQL_AST_OPERATOR_POSITIVE:
    case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
    case MYLITE_SQL_AST_OPERATOR_ADD:
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
    case MYLITE_SQL_AST_OPERATOR_DIVIDE:
    case MYLITE_SQL_AST_OPERATOR_IS_NULL:
    case MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL:
        break;
    }

    return "=";
}

static int execute_sqlite_schema_sql(struct mylite_db *database, const char *sql) {
    int sqlite_rc = sqlite3_exec(database->sqlite, sql, NULL, NULL, NULL);
    int rc = mylite_sqlite_status_to_mylite(sqlite_rc);

    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
        return rc;
    }
    if (rc != MYLITE_OK) {
        set_physical_sqlite_error(database);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int execute_sqlite_control_sql(const struct mylite_db *database, const char *sql) {
    int sqlite_rc = sqlite3_exec(database->sqlite, sql, NULL, NULL, NULL);

    return mylite_sqlite_status_to_mylite(sqlite_rc);
}

static int prepare_sqlite_statement(
    const struct mylite_db *database,
    const char *sql,
    sqlite3_stmt **out_statement
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;

    *out_statement = NULL;
    sqlite_rc = sqlite3_prepare_v2(
        database->sqlite,
        sql,
        sqlite_use_nul_terminated_string,
        &statement,
        NULL
    );
    if (sqlite_rc != SQLITE_OK) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    *out_statement = statement;

    return MYLITE_OK;
}

static int finalize_sqlite_statement(sqlite3_stmt *statement, int rc) {
    int sqlite_rc = SQLITE_OK;

    if (statement == NULL) {
        return rc;
    }

    sqlite_rc = sqlite3_finalize(statement);
    if (rc != MYLITE_OK) {
        return rc;
    }

    return mylite_sqlite_status_to_mylite(sqlite_rc);
}

static int bind_insert_row(sqlite3_stmt *statement, const struct planned_insert *plan, size_t row) {
    int sqlite_rc = sqlite3_reset(statement);

    if (sqlite_rc == SQLITE_OK) {
        sqlite_rc = sqlite3_clear_bindings(statement);
    }
    if (sqlite_rc != SQLITE_OK) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    for (size_t column_index = 0U; column_index < plan->column_count; ++column_index) {
        const struct planned_value *value = &plan->rows[row].values[column_index];
        int bind_index = 0;

        if (column_index >= (size_t)INT_MAX) {
            return MYLITE_ERROR;
        }
        bind_index = (int)column_index + 1;
        if (value->is_null) {
            sqlite_rc = sqlite3_bind_null(statement, bind_index);
        } else {
            sqlite_rc = sqlite3_bind_int64(statement, bind_index, (sqlite3_int64)value->integer);
        }
        if (sqlite_rc != SQLITE_OK) {
            return mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }

    return MYLITE_OK;
}

static int step_insert_row(sqlite3_stmt *statement) {
    int sqlite_rc = sqlite3_step(statement);

    if (sqlite_rc != SQLITE_DONE) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    return MYLITE_OK;
}

static int bind_select_parameters(sqlite3_stmt *statement, const struct planned_select *plan) {
    int parameter_index = 1;
    int rc = MYLITE_OK;

    if (plan->predicate.kind == PLANNED_SELECT_PREDICATE_COMPARISON) {
        rc = bind_int64_parameter(statement, parameter_index, plan->predicate.value.integer);
        if (rc == MYLITE_OK) {
            ++parameter_index;
        }
    }
    if (rc == MYLITE_OK && plan->limit.has_limit) {
        rc = bind_int64_parameter(statement, parameter_index, plan->limit.row_count);
        if (rc == MYLITE_OK) {
            ++parameter_index;
        }
    }
    if (rc == MYLITE_OK && plan->limit.has_offset) {
        rc = bind_int64_parameter(statement, parameter_index, plan->limit.offset);
    }

    return rc;
}

static int bind_count_parameters(sqlite3_stmt *statement, const struct planned_count *plan) {
    int parameter_index = 1;
    int rc = MYLITE_OK;

    if (plan->function == PLANNED_COUNT_LITERAL) {
        rc = bind_planned_value_parameter(statement, parameter_index, &plan->count_literal);
        if (rc == MYLITE_OK) {
            ++parameter_index;
        }
    }
    if (plan->predicate.kind == PLANNED_SELECT_PREDICATE_COMPARISON) {
        rc = bind_int64_parameter(statement, parameter_index, plan->predicate.value.integer);
    }

    return rc;
}

static int bind_min_max_aggregate_parameters(
    sqlite3_stmt *statement,
    const struct planned_min_max_aggregate *plan
) {
    if (plan->predicate.kind == PLANNED_SELECT_PREDICATE_COMPARISON) {
        return bind_int64_parameter(statement, 1, plan->predicate.value.integer);
    }

    return MYLITE_OK;
}

static int bind_delete_parameters(sqlite3_stmt *statement, const struct planned_delete *plan) {
    int parameter_index = 1;
    int rc = MYLITE_OK;

    if (plan->predicate.kind == PLANNED_SELECT_PREDICATE_COMPARISON) {
        rc = bind_int64_parameter(statement, parameter_index, plan->predicate.value.integer);
        if (rc == MYLITE_OK) {
            ++parameter_index;
        }
    }
    if (rc == MYLITE_OK && plan->limit.has_limit) {
        rc = bind_int64_parameter(statement, parameter_index, plan->limit.row_count);
    }

    return rc;
}

static int bind_update_parameters(sqlite3_stmt *statement, const struct planned_update *plan) {
    int parameter_index = 1;
    int rc = bind_planned_value_parameter(statement, parameter_index, &plan->assignment_value);

    if (rc == MYLITE_OK) {
        ++parameter_index;
    }
    if (rc == MYLITE_OK && plan->predicate.kind == PLANNED_SELECT_PREDICATE_COMPARISON) {
        rc = bind_int64_parameter(statement, parameter_index, plan->predicate.value.integer);
        if (rc == MYLITE_OK) {
            ++parameter_index;
        }
    }
    if (rc == MYLITE_OK && plan->limit.has_limit) {
        rc = bind_int64_parameter(statement, parameter_index, plan->limit.row_count);
        if (rc == MYLITE_OK) {
            ++parameter_index;
        }
    }
    if (rc == MYLITE_OK && !plan->assignment_value.is_null) {
        rc = bind_planned_value_parameter(statement, parameter_index, &plan->assignment_value);
    }

    return rc;
}

static int bind_update_matched_parameters(
    sqlite3_stmt *statement,
    const struct planned_update *plan
) {
    if (plan->predicate.kind == PLANNED_SELECT_PREDICATE_COMPARISON) {
        return bind_int64_parameter(statement, 1, plan->predicate.value.integer);
    }

    return MYLITE_OK;
}

static int bind_planned_value_parameter(
    sqlite3_stmt *statement,
    int parameter_index,
    const struct planned_value *value
) {
    int sqlite_rc = SQLITE_OK;

    if (parameter_index <= 0 || value == NULL) {
        return MYLITE_ERROR;
    }
    if (value->is_null) {
        sqlite_rc = sqlite3_bind_null(statement, parameter_index);
    } else {
        sqlite_rc = sqlite3_bind_int64(statement, parameter_index, (sqlite3_int64)value->integer);
    }
    if (sqlite_rc != SQLITE_OK) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    return MYLITE_OK;
}

static int bind_int64_parameter(sqlite3_stmt *statement, int parameter_index, int64_t value) {
    int sqlite_rc = SQLITE_OK;

    if (parameter_index <= 0) {
        return MYLITE_ERROR;
    }

    sqlite_rc = sqlite3_bind_int64(statement, parameter_index, (sqlite3_int64)value);
    if (sqlite_rc != SQLITE_OK) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    return MYLITE_OK;
}

static int append_selected_sqlite_row(sqlite3_stmt *statement, mylite_result *result) {
    size_t column_count = mylite_result_column_count(result);
    const char **values = NULL;
    char *texts = NULL;
    int rc = MYLITE_OK;

    if (column_count > (size_t)INT_MAX) {
        return MYLITE_NOMEM;
    }

    values = (const char **)calloc(column_count, sizeof(*values));
    texts = calloc(column_count, integer_text_capacity);
    if (values == NULL || texts == NULL) {
        free((void *)values);
        free(texts);
        return MYLITE_NOMEM;
    }

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < column_count; ++column_index) {
        int sqlite_type = sqlite3_column_type(statement, (int)column_index);
        char *text = &texts[column_index * integer_text_capacity];
        int written = 0;

        if (sqlite_type == SQLITE_NULL) {
            values[column_index] = NULL;
        } else if (sqlite_type == SQLITE_INTEGER) {
            written = snprintf(
                text,
                integer_text_capacity,
                "%" PRId64,
                (int64_t)sqlite3_column_int64(statement, (int)column_index)
            );
            if (written < 0 || written >= integer_text_capacity) {
                rc = MYLITE_ERROR;
            } else {
                values[column_index] = text;
            }
        } else {
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_text_row(result, values);
    }

    free((void *)values);
    free(texts);

    return rc;
}

static int choose_sqlite_rowid_alias(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const char *unsupported_message,
    const char **out_alias
) {
    static const char *const rowid_aliases[] = {"rowid", "_rowid_", "oid"};

    if (out_alias == NULL) {
        return MYLITE_MISUSE;
    }
    *out_alias = NULL;
    for (size_t index = 0U; index < sizeof(rowid_aliases) / sizeof(rowid_aliases[0]); ++index) {
        if (!column_name_exists(columns, column_count, rowid_aliases[index])) {
            *out_alias = rowid_aliases[index];
            return MYLITE_OK;
        }
    }

    set_unsupported_error(database, unsupported_message);
    return MYLITE_ERROR;
}

static bool column_name_exists(
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const char *name
) {
    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        if (text_equals_ascii_case_insensitive(columns[column_index].name, name)) {
            return true;
        }
    }

    return false;
}

static void dynamic_string_init(struct dynamic_string *string) {
    *string = (struct dynamic_string){0};
}

static void dynamic_string_deinit(struct dynamic_string *string) {
    if (string == NULL) {
        return;
    }

    free(string->text);
    *string = (struct dynamic_string){0};
}

static int dynamic_string_append(struct dynamic_string *string, const char *text) {
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (string == NULL || text == NULL) {
        return MYLITE_MISUSE;
    }

    text_length = strlen(text);
    if (text_length > SIZE_MAX - string->length - 1U) {
        return MYLITE_NOMEM;
    }

    rc = dynamic_string_reserve(string, string->length + text_length + 1U);
    if (rc != MYLITE_OK) {
        return rc;
    }

    memcpy(&string->text[string->length], text, text_length);
    string->length += text_length;
    string->text[string->length] = '\0';

    return MYLITE_OK;
}

static int dynamic_string_append_char(struct dynamic_string *string, char byte) {
    int rc = MYLITE_OK;

    if (string == NULL) {
        return MYLITE_MISUSE;
    }
    if (string->length > SIZE_MAX - 2U) {
        return MYLITE_NOMEM;
    }

    rc = dynamic_string_reserve(string, string->length + 2U);
    if (rc != MYLITE_OK) {
        return rc;
    }

    string->text[string->length] = byte;
    ++string->length;
    string->text[string->length] = '\0';

    return MYLITE_OK;
}

static int dynamic_string_append_quoted_identifier(
    struct dynamic_string *string,
    const char *text
) {
    int rc = dynamic_string_append_char(string, '"');

    for (size_t index = 0U; rc == MYLITE_OK && text[index] != '\0'; ++index) {
        if (text[index] == '"') {
            rc = dynamic_string_append(string, "\"\"");
        } else {
            rc = dynamic_string_append_char(string, text[index]);
        }
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_char(string, '"');
    }

    return rc;
}

static int dynamic_string_append_mysql_quoted_identifier(
    struct dynamic_string *string,
    const char *text
) {
    int rc = dynamic_string_append_char(string, '`');

    for (size_t index = 0U; rc == MYLITE_OK && text[index] != '\0'; ++index) {
        if (text[index] == '`') {
            rc = dynamic_string_append(string, "``");
        } else {
            rc = dynamic_string_append_char(string, text[index]);
        }
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_char(string, '`');
    }

    return rc;
}

static int dynamic_string_reserve(struct dynamic_string *string, size_t required_capacity) {
    enum { initial_capacity = 128 };

    char *text = NULL;
    size_t capacity = string->capacity;

    if (required_capacity <= capacity) {
        return MYLITE_OK;
    }
    if (capacity == 0U) {
        capacity = initial_capacity;
    }
    while (capacity < required_capacity) {
        if (capacity > SIZE_MAX / 2U) {
            return MYLITE_NOMEM;
        }
        capacity *= 2U;
    }

    text = realloc(string->text, capacity);
    if (text == NULL) {
        return MYLITE_NOMEM;
    }
    if (string->capacity == 0U) {
        text[0] = '\0';
    }
    string->text = text;
    string->capacity = capacity;

    return MYLITE_OK;
}

static char *dynamic_string_take(struct dynamic_string *string) {
    char *text = NULL;

    if (string == NULL) {
        return NULL;
    }

    text = string->text;
    string->text = NULL;
    string->length = 0U;
    string->capacity = 0U;

    return text;
}

static const struct mylite_sql_ast_node *child_at(
    const struct mylite_sql_ast_node *node,
    size_t index
) {
    const struct mylite_sql_ast_node *child = NULL;

    if (node == NULL) {
        return NULL;
    }

    child = node->first_child;
    for (size_t current = 0U; current < index && child != NULL; ++current) {
        child = child->next_sibling;
    }
    return child;
}

static const struct mylite_sql_ast_node *child_with_kind(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_node_kind kind
) {
    const struct mylite_sql_ast_node *child = NULL;

    if (node == NULL) {
        return NULL;
    }

    child = node->first_child;
    while (child != NULL) {
        if (child->kind == kind) {
            return child;
        }
        child = child->next_sibling;
    }
    return NULL;
}

static int script_statement_count(const struct mylite_sql_ast_node *root, size_t *out_count) {
    if (root == NULL || root->kind != MYLITE_SQL_AST_SCRIPT || out_count == NULL) {
        return MYLITE_ERROR;
    }

    *out_count = mylite_sql_ast_node_child_count(root);

    return MYLITE_OK;
}

static void set_parse_error(
    struct mylite_db *database,
    const struct mylite_sql_parse_result *parse_result
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    const char *near_text = "end of input";
    int written = 0;

    if (parse_result != NULL && parse_result->error_token.text != NULL &&
        parse_result->error_token.length != 0U) {
        near_text = parse_result->error_token.text;
        written = snprintf(
            message,
            sizeof(message),
            "You have an error in your SQL syntax near '%.*s' at line %zu",
            (int)parse_result->error_token.length,
            near_text,
            parse_result->error_token.line
        );
    } else {
        written = snprintf(
            message,
            sizeof(message),
            "You have an error in your SQL syntax near '%s'",
            near_text
        );
    }
    if (written < 0) {
        message[0] = '\0';
    }

    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_parse,
        "42000",
        message
    );
}

static void set_unsupported_error(struct mylite_db *database, const char *message) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_parse,
        "42000",
        message
    );
}

static void set_session_variable_only_error(struct mylite_db *database, const char *variable_name) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Variable '%s' is a SESSION variable", variable_name);

    if (written < 0) {
        message[0] = '\0';
    }

    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_session_variable_only,
        "HY000",
        message
    );
}

static void set_global_variable_only_error(struct mylite_db *database, const char *variable_name) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Variable '%s' is a GLOBAL variable", variable_name);

    if (written < 0) {
        message[0] = '\0';
    }

    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_session_variable_only,
        "HY000",
        message
    );
}

static void set_unknown_system_variable_error(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
) {
    char *variable_name = NULL;
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    const char *display_name = "unknown";
    int written = 0;

    if (expression != NULL) {
        (void)copy_system_variable_name_for_error(&expression->span, &variable_name);
    }
    if (variable_name != NULL) {
        display_name = variable_name;
    }

    written = snprintf(message, sizeof(message), "Unknown system variable '%s'", display_name);
    if (written < 0) {
        message[0] = '\0';
    }
    free(variable_name);

    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_system_variable,
        "HY000",
        message
    );
}

static int copy_system_variable_name_for_error(
    const struct mylite_sql_source_span *span,
    char **out_name
) {
    char *first = NULL;
    char *second = NULL;
    size_t offset = system_variable_body_offset;
    int rc = MYLITE_OK;

    if (out_name == NULL) {
        return MYLITE_MISUSE;
    }
    *out_name = NULL;
    if (span == NULL || span->text == NULL || span->length <= system_variable_body_offset) {
        return MYLITE_ERROR;
    }

    rc = copy_system_variable_component_name_for_error(span, &offset, &first);
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (offset < span->length && span->text[offset] == '.') {
        ++offset;
        rc = copy_system_variable_component_name_for_error(span, &offset, &second);
        if (rc != MYLITE_OK) {
            free(first);
            return rc;
        }
        if (text_equals_ascii_case_insensitive(first, "session") ||
            text_equals_ascii_case_insensitive(first, "local") ||
            text_equals_ascii_case_insensitive(first, "global")) {
            free(first);
            *out_name = second;
            return MYLITE_OK;
        }
        free(second);
        free(first);
        return copy_system_variable_raw_body_for_error(span, out_name);
    }

    *out_name = first;
    return MYLITE_OK;
}

static int copy_system_variable_component_name_for_error(
    const struct mylite_sql_source_span *span,
    size_t *offset,
    char **out_name
) {
    const size_t capacity = span->length + 1U;
    size_t length = 0U;
    char *name = (char *)malloc(capacity);

    if (name == NULL) {
        return MYLITE_NOMEM;
    }
    name[0] = '\0';

    if (*offset < span->length && span->text[*offset] == '`') {
        ++*offset;
        while (*offset < span->length) {
            char value = span->text[*offset];

            if (value == '`') {
                ++*offset;
                if (*offset < span->length && span->text[*offset] == '`') {
                    int byte_rc =
                        append_system_variable_error_name_byte('`', &name, &length, capacity);
                    if (byte_rc != MYLITE_OK) {
                        return byte_rc;
                    }
                    ++*offset;
                    continue;
                }
                *out_name = name;
                return MYLITE_OK;
            }

            {
                int byte_rc =
                    append_system_variable_error_name_byte(value, &name, &length, capacity);
                if (byte_rc != MYLITE_OK) {
                    return byte_rc;
                }
            }
            ++*offset;
        }

        free(name);
        return MYLITE_ERROR;
    }

    while (*offset < span->length && span->text[*offset] != '.') {
        int byte_rc =
            append_system_variable_error_name_byte(span->text[*offset], &name, &length, capacity);
        if (byte_rc != MYLITE_OK) {
            return byte_rc;
        }
        ++*offset;
    }

    *out_name = name;
    return MYLITE_OK;
}

static int copy_system_variable_raw_body_for_error(
    const struct mylite_sql_source_span *span,
    char **out_name
) {
    const size_t length = span->length - system_variable_body_offset;
    char *name = (char *)malloc(length + 1U);

    if (name == NULL) {
        return MYLITE_NOMEM;
    }

    memcpy(name, span->text + system_variable_body_offset, length);
    name[length] = '\0';
    *out_name = name;
    return MYLITE_OK;
}

static int append_system_variable_error_name_byte(
    char value,
    char **name,
    size_t *length,
    size_t capacity
) {
    if (*length + 1U >= capacity) {
        free(*name);
        *name = NULL;
        return MYLITE_NOMEM;
    }

    (*name)[*length] = value;
    ++*length;
    (*name)[*length] = '\0';
    return MYLITE_OK;
}

static void set_native_function_parameter_count_error(
    struct mylite_db *database,
    const char *function_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Incorrect parameter count in the call to native function '%s'",
        function_name
    );

    if (written < 0) {
        message[0] = '\0';
    }

    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_incorrect_parameter_count,
        "42000",
        message
    );
}

static void set_no_database_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_no_database_selected,
        "3D000",
        "No database selected"
    );
}

static void set_database_exists_error(struct mylite_db *database, const char *schema_name) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Can't create database '%s'; database exists",
        schema_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_database_exists,
        "HY000",
        message
    );
}

static int append_database_exists_note(struct mylite_db *database, const char *schema_name) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Can't create database '%s'; database exists",
        schema_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    return mylite_diagnostics_append_note(
        mylite_connection_diagnostics(database),
        mysql_error_database_exists,
        "HY000",
        message
    );
}

static void set_cant_drop_database_error(struct mylite_db *database, const char *schema_name) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Can't drop database '%s'; database doesn't exist",
        schema_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_cant_drop_database,
        "HY000",
        message
    );
}

static void set_unknown_database_error(struct mylite_db *database, const char *schema_name) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Unknown database '%s'", schema_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_database,
        "42000",
        message
    );
}

static void set_table_exists_error(struct mylite_db *database, const char *table_name) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Table '%s' already exists", table_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_table_exists,
        "42S01",
        message
    );
}

static int append_table_exists_note(struct mylite_db *database, const char *table_name) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Table '%s' already exists", table_name);

    if (written < 0) {
        message[0] = '\0';
    }
    return mylite_diagnostics_append_note(
        mylite_connection_diagnostics(database),
        mysql_error_table_exists,
        "42S01",
        message
    );
}

static void set_unknown_table_error(
    struct mylite_db *database,
    const char *schema_name,
    const char *table_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Unknown table '%s.%s'", schema_name, table_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_table,
        "42S02",
        message
    );
}

static int set_unknown_drop_tables_error(
    struct mylite_db *database,
    const struct planned_drop_table *plan
) {
    struct dynamic_string message;
    char *owned_message = NULL;
    size_t missing_index = 0U;
    int rc = MYLITE_OK;

    dynamic_string_init(&message);
    rc = dynamic_string_append(&message, "Unknown table '");
    for (size_t target_index = 0U; rc == MYLITE_OK && target_index < plan->target_count;
         ++target_index) {
        const struct planned_drop_table_target *target = &plan->targets[target_index];

        if (!target->missing) {
            continue;
        }
        if (missing_index != 0U) {
            rc = dynamic_string_append_char(&message, ',');
        }
        if (rc == MYLITE_OK) {
            rc = dynamic_string_append(&message, target->target.schema.name);
        }
        if (rc == MYLITE_OK) {
            rc = dynamic_string_append_char(&message, '.');
        }
        if (rc == MYLITE_OK) {
            rc = dynamic_string_append(&message, target->target.table_name);
        }
        ++missing_index;
    }
    if (rc == MYLITE_OK) {
        rc = dynamic_string_append_char(&message, '\'');
    }
    if (rc == MYLITE_OK) {
        owned_message = dynamic_string_take(&message);
        if (owned_message == NULL) {
            rc = MYLITE_NOMEM;
        }
    }
    if (rc != MYLITE_OK) {
        dynamic_string_deinit(&message);
        set_nomem_error(database);
        return rc;
    }

    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_table,
        "42S02",
        owned_message
    );
    free(owned_message);

    return MYLITE_ERROR;
}

static int append_unknown_table_note(
    struct mylite_db *database,
    const char *schema_name,
    const char *table_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Unknown table '%s.%s'", schema_name, table_name);

    if (written < 0) {
        message[0] = '\0';
    }
    return mylite_diagnostics_append_note(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_table,
        "42S02",
        message
    );
}

static void set_table_does_not_exist_error(
    struct mylite_db *database,
    const char *schema_name,
    const char *table_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Table '%s.%s' doesn't exist", schema_name, table_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_table_does_not_exist,
        "42S02",
        message
    );
}

static void set_unknown_storage_engine_error(struct mylite_db *database, const char *engine_name) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Unknown storage engine '%s'", engine_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_storage_engine,
        "42000",
        message
    );
}

static void set_unknown_character_set_error(struct mylite_db *database, const char *charset_name) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Unknown character set: '%s'", charset_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_character_set,
        "42000",
        message
    );
}

static void set_unknown_collation_error(struct mylite_db *database, const char *collation_name) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Unknown collation: '%s'", collation_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_collation,
        "HY000",
        message
    );
}

static void set_duplicate_column_error(struct mylite_db *database, const char *column_name) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Duplicate column name '%s'", column_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_duplicate_column,
        "42S21",
        message
    );
}

static void set_duplicate_table_alias_error(struct mylite_db *database, const char *table_name) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Not unique table/alias: '%s'", table_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_not_unique_table_alias,
        "42000",
        message
    );
}

static void set_cant_drop_field_or_key_error(struct mylite_db *database, const char *column_name) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Can't DROP '%s'; check that column/key exists",
        column_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_cant_drop_field_or_key,
        "42000",
        message
    );
}

static void set_cant_remove_all_fields_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_cant_remove_all_fields,
        "42000",
        "You can't delete all columns with ALTER TABLE; use DROP TABLE instead"
    );
}

static void set_must_have_visible_column_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_must_have_visible_column,
        "HY000",
        "A table must have at least one visible column."
    );
}

static void set_unknown_column_in_table_error(
    struct mylite_db *database,
    const char *column_name,
    const char *table_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Unknown column '%s' in '%s'", column_name, table_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_column,
        "42S22",
        message
    );
}

static void set_unknown_column_error(struct mylite_db *database, const char *column_name) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Unknown column '%s' in 'field list'", column_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_column,
        "42S22",
        message
    );
}

static void set_unknown_where_column_error(struct mylite_db *database, const char *column_name) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Unknown column '%s' in 'where clause'", column_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_column,
        "42S22",
        message
    );
}

static void set_unknown_order_column_error(struct mylite_db *database, const char *column_name) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Unknown column '%s' in 'order clause'", column_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_column,
        "42S22",
        message
    );
}

static void set_ambiguous_order_column_error(struct mylite_db *database, const char *column_name) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Column '%s' in order clause is ambiguous", column_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_column_ambiguous,
        "23000",
        message
    );
}

static void set_column_specified_twice_error(struct mylite_db *database, const char *column_name) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Column '%s' specified twice", column_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_column_specified_twice,
        "42000",
        message
    );
}

static void set_column_count_mismatch_error(struct mylite_db *database, size_t row_number) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Column count doesn't match value count at row %zu",
        row_number
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_column_count_mismatch,
        "21S01",
        message
    );
}

static void set_bad_null_error(struct mylite_db *database, const char *column_name) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Column '%s' cannot be null", column_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_bad_null,
        "23000",
        message
    );
}

static void set_data_truncated_error(
    struct mylite_db *database,
    const char *column_name,
    size_t row_number
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Data truncated for column '%s' at row %zu",
        column_name,
        row_number
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_data_truncated,
        "01000",
        message
    );
}

static void set_invalid_default_error(struct mylite_db *database, const char *column_name) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Invalid default value for '%s'", column_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_invalid_default,
        "42000",
        message
    );
}

static void set_no_default_error(struct mylite_db *database, const char *column_name) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written =
        snprintf(message, sizeof(message), "Field '%s' doesn't have a default value", column_name);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_field_no_default,
        "HY000",
        message
    );
}

static void set_out_of_range_error(
    struct mylite_db *database,
    const char *column_name,
    size_t row_number
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Out of range value for column '%s' at row %zu",
        column_name,
        row_number
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_data_out_of_range,
        "22003",
        message
    );
}

static void set_display_width_out_of_range_error(
    struct mylite_db *database,
    const char *column_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Display width out of range for column '%s' (max = 255)",
        column_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_display_width_out_of_range,
        "42000",
        message
    );
}

static void set_predicate_out_of_range_error(struct mylite_db *database, const char *column_name) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Out of range value for column '%s' in WHERE",
        column_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_data_out_of_range,
        "22003",
        message
    );
}

static void set_limit_out_of_range_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_parse,
        "42000",
        "LIMIT literal is outside the supported range"
    );
}

static void set_identifier_too_long_error(struct mylite_db *database, const char *kind) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "%s identifier is too long", kind);

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_identifier_too_long,
        "42000",
        message
    );
}

static void set_reserved_name_error(
    struct mylite_db *database,
    const char *kind,
    const char *name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int code = mysql_error_incorrect_table_name;
    int written = 0;

    if (strcmp(kind, "database") == 0) {
        code = mysql_error_incorrect_database_name;
    } else if (strcmp(kind, "column") == 0) {
        code = mysql_error_incorrect_column_name;
    }

    written = snprintf(message, sizeof(message), "Incorrect %s name '%s'", kind, name);
    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(mylite_connection_diagnostics(database), code, "42000", message);
}

static void set_nomem_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        MYLITE_NOMEM,
        "HY001",
        "out of memory"
    );
}

static void set_physical_sqlite_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown,
        "HY000",
        "internal SQLite schema operation failed"
    );
}

static void set_physical_sqlite_row_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown,
        "HY000",
        "internal SQLite row operation failed"
    );
}

static void set_runtime_error(struct mylite_db *database, const char *message) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown,
        "HY000",
        message
    );
}

static void set_internal_error_if_clear(struct mylite_db *database, int rc, const char *message) {
    if (database == NULL) {
        return;
    }
    if (mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) != MYLITE_OK) {
        return;
    }
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
        return;
    }
    if (rc == MYLITE_MISUSE) {
        mylite_diagnostics_set_error(
            mylite_connection_diagnostics(database),
            MYLITE_MISUSE,
            "HY000",
            mylite_diagnostics_misuse_message()
        );
        return;
    }

    set_runtime_error(database, message);
}

static int status_from_parse_status(enum mylite_sql_parse_status status) {
    switch (status) {
    case MYLITE_SQL_PARSE_OK:
        return MYLITE_OK;
    case MYLITE_SQL_PARSE_NOMEM:
        return MYLITE_NOMEM;
    case MYLITE_SQL_PARSE_MISUSE:
        return MYLITE_MISUSE;
    case MYLITE_SQL_PARSE_LEXER_ERROR:
    case MYLITE_SQL_PARSE_SYNTAX_ERROR:
    case MYLITE_SQL_PARSE_STACK_OVERFLOW:
        return MYLITE_ERROR;
    }

    return MYLITE_ERROR;
}
