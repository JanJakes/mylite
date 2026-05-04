#ifndef MYLITE_RUNTIME_MYLITE_RUNTIME_H
#define MYLITE_RUNTIME_MYLITE_RUNTIME_H

#include <mylite/mylite.h>

#include "mylite_dml_types.h"
#include "mylite_expression.h"
#include "mylite_field_descriptor.h"
#include "mylite_metadata_types.h"
#include "mylite_schema_types.h"
#include "mylite_table_ddl_types.h"
#include "mylite_transaction_types.h"
#include "sql/mylite_ast.h"
#include "sqlite3.h"
#include "types/mylite_column_type.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

enum mylite_stmt_kind {
    MYLITE_STMT_SQLITE = 0,
    MYLITE_STMT_CREATE_SCHEMA = 1,
    MYLITE_STMT_ALTER_SCHEMA = 2,
    MYLITE_STMT_DROP_SCHEMA = 3,
    MYLITE_STMT_USE_SCHEMA = 4,
    MYLITE_STMT_SET_NAMES = 5,
    MYLITE_STMT_SET_CHARACTER_SET = 6,
    MYLITE_STMT_CREATE_TABLE = 7,
    MYLITE_STMT_DROP_TABLE = 8,
    MYLITE_STMT_INSERT_VALUES = 9,
    MYLITE_STMT_INSERT_SET = 10,
    MYLITE_STMT_REPLACE_VALUES = 11,
    MYLITE_STMT_REPLACE_SET = 12,
    MYLITE_STMT_SCALAR_SELECT = 13,
    MYLITE_STMT_TABLE_SELECT = 14,
    MYLITE_STMT_UNION_QUERY = 15,
    MYLITE_STMT_UPDATE = 16,
    MYLITE_STMT_DELETE = 17,
    MYLITE_STMT_START_TRANSACTION = 18,
    MYLITE_STMT_BEGIN_TRANSACTION = 19,
    MYLITE_STMT_COMMIT = 20,
    MYLITE_STMT_ROLLBACK = 21,
    MYLITE_STMT_SAVEPOINT = 22,
    MYLITE_STMT_ROLLBACK_TO_SAVEPOINT = 23,
    MYLITE_STMT_RELEASE_SAVEPOINT = 24,
    MYLITE_STMT_CREATE_INDEX = 25,
    MYLITE_STMT_DROP_INDEX = 26,
    MYLITE_STMT_ALTER_TABLE = 27,
    MYLITE_STMT_RENAME_TABLE = 28,
    MYLITE_STMT_TRUNCATE_TABLE = 29,
};

enum mylite_information_schema_table {
    MYLITE_INFORMATION_SCHEMA_NONE = 0,
    MYLITE_INFORMATION_SCHEMA_SCHEMATA = 1,
    MYLITE_INFORMATION_SCHEMA_TABLES = 2,
    MYLITE_INFORMATION_SCHEMA_COLUMNS = 3,
    MYLITE_INFORMATION_SCHEMA_STATISTICS = 4,
    MYLITE_INFORMATION_SCHEMA_ENGINES = 5,
    MYLITE_INFORMATION_SCHEMA_CHARACTER_SETS = 6,
    MYLITE_INFORMATION_SCHEMA_COLLATIONS = 7,
    MYLITE_INFORMATION_SCHEMA_COLLATION_CHARACTER_SET_APPLICABILITY = 8,
    MYLITE_INFORMATION_SCHEMA_KEYWORDS = 9,
    MYLITE_INFORMATION_SCHEMA_TABLE_CONSTRAINTS = 10,
    MYLITE_INFORMATION_SCHEMA_KEY_COLUMN_USAGE = 11,
    MYLITE_INFORMATION_SCHEMA_CHECK_CONSTRAINTS = 12,
    MYLITE_INFORMATION_SCHEMA_REFERENTIAL_CONSTRAINTS = 13,
};

struct mylite_catalog_text_match {
    const char *text;
    const char *word;
};

struct mylite_catalog_column_descriptor_source {
    sqlite3_stmt *select;
    const char *extra;
    const char *is_nullable;
    const char *data_type;
    const char *collation_name;
    const char *column_type;
    const char *column_key;
    int column_default_index;
    int character_octet_length_index;
    int numeric_precision_index;
    int numeric_scale_index;
    int datetime_precision_index;
    bool nullable;
    bool is_unsigned;
    bool is_zerofill;
    bool auto_increment;
};

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

struct mylite_charset_collation_info {
    const char *character_set;
    const char *collation;
    int coercibility;
};

struct mylite_strcmp_compare_options {
    bool ignore_trailing_spaces;
    bool case_sensitive;
};

struct mylite_connection_charset_request {
    const char *character_set_name;
    const char *collation_name;
};

struct mylite_show_variables_query {
    enum mylite_sql_ast_show_variables_scope scope;
    const char *like_pattern;
};

struct mylite_show_status_query {
    enum mylite_sql_ast_show_status_scope scope;
    const char *like_pattern;
};

struct mylite_storage_engine_row {
    const char *engine;
    const char *support;
    const char *comment;
    const char *transactions;
    const char *xa;
    const char *savepoints;
};

struct mylite_storage_engine_columns {
    const char *engine;
    const char *support;
    const char *comment;
    const char *transactions;
    const char *xa;
    const char *savepoints;
};

struct mylite_show_engines_metadata_column {
    const char *name;
    uint64_t length;
    bool nullable;
};

struct mylite_show_character_set_query {
    const char *like_pattern;
};

struct mylite_show_collation_query {
    const char *like_pattern;
};

struct mylite_show_tables_query {
    const char *schema_name;
    const char *column_name;
    const char *glob_pattern;
    bool full;
};

struct mylite_show_table_status_query {
    const char *schema_name;
    const char *glob_pattern;
};

struct mylite_show_columns_target {
    char *schema_name;
    char *table_name;
};

struct mylite_show_columns_source_nodes {
    const struct mylite_sql_ast_node *table_name;
    const struct mylite_sql_ast_node *explicit_schema;
};

struct mylite_show_columns_query {
    const char *schema_name;
    const char *table_name;
    const char *like_pattern;
    bool full;
};

struct mylite_show_index_target {
    char *schema_name;
    char *table_name;
};

struct mylite_show_index_source_nodes {
    const struct mylite_sql_ast_node *table_name;
    const struct mylite_sql_ast_node *explicit_schema;
};

struct mylite_show_index_query {
    const char *schema_name;
    const char *table_name;
};

struct mylite_show_create_table_target {
    char *schema_name;
    char *table_name;
};

struct mylite_show_create_schema_info {
    char *name;
    char *character_set;
    char *collation;
    char *encryption;
};

struct mylite_show_diagnostics_query {
    enum mylite_sql_ast_show_diagnostics_kind kind;
    uint64_t offset;
    uint64_t row_count;
    bool has_limit;
};

struct mylite_show_create_table_info {
    char *engine;
    bool has_auto_increment;
    sqlite3_int64 auto_increment;
    char *table_collation;
    char *table_comment;
};

struct mylite_show_create_column_collation {
    const char *character_set_name;
    const char *column_collation;
    const char *table_collation;
};

struct mylite_db {
    sqlite3 *sqlite;
    char *error_message;
    struct mylite_expression_warnings warnings;
    char *selected_schema;
    uint64_t connection_id;
    uint64_t last_insert_id;
    int64_t previous_row_count;
    enum mylite_transaction_access_mode transaction_access_mode;
    bool transaction_active;
    bool transaction_consistent_snapshot;
    bool transaction_released;
    time_t status_started_at;
    struct mylite_savepoint_state savepoints;
    struct mylite_pending_auto_increment *pending_auto_increments;
    size_t pending_auto_increment_count;
    const char *character_set_client;
    const char *character_set_connection;
    const char *character_set_results;
    const char *collation_connection;
};

struct mylite_statement_timestamp {
    time_t seconds;
    long microseconds;
};

struct mylite_stmt {
    mylite_db *database;
    enum mylite_stmt_kind kind;
    sqlite3_stmt *sqlite_stmt;
    char *schema_name;
    bool if_exists;
    bool if_not_exists;
    bool executed;
    bool previous_row_count_recorded;
    bool preserve_prepare_warnings;
    bool has_statement_timestamp;
    struct mylite_statement_timestamp statement_timestamp;
    struct mylite_schema_options options;
    struct mylite_create_table_plan create_table;
    struct mylite_drop_table_plan drop_table;
    struct mylite_rename_table_plan rename_table;
    struct mylite_truncate_table_plan truncate_table;
    struct mylite_alter_table_plan alter_table;
    struct mylite_index_ddl_plan index_ddl;
    struct mylite_insert_values_plan insert_values;
    struct mylite_insert_set_plan insert_set;
    struct mylite_insert_duplicate_update_plan insert_update;
    struct mylite_update_plan update;
    struct mylite_delete_plan delete_plan;
    struct mylite_transaction_plan transaction;
    struct mylite_savepoint_plan savepoint;
    struct mylite_select_plan select_plan;
    struct mylite_union_plan union_plan;
    struct mylite_result_metadata result_metadata;
    struct mylite_scalar_result scalar_result;
    struct mylite_table_select_result select_result;
    struct mylite_sql_ast select_predicate_ast;
    struct mylite_sql_ast scalar_select_ast;
    struct mylite_sql_ast update_ast;
    struct mylite_sql_ast delete_ast;
    const struct mylite_sql_ast_node *select_predicate;
    char *select_sql_text;
    char *scalar_select_sql_text;
    char *update_sql_text;
    char *delete_sql_text;
    struct mylite_cached_expression_value *select_constant_values;
    size_t select_constant_value_count;
    bool select_constant_predicate_evaluated;
    bool select_constant_predicate_matches;
    char *character_set_name;
    char *collation_name;
    int64_t affected_rows;
    uint64_t matched_rows;
    bool use_default_connection_charset;
};

#endif
