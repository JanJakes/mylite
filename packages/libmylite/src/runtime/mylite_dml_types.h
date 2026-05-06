#ifndef MYLITE_RUNTIME_MYLITE_DML_TYPES_H
#define MYLITE_RUNTIME_MYLITE_DML_TYPES_H

#include <mylite/mylite.h>

#include "sql/mylite_ast.h"
#include "sqlite3.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct mylite_expression_value;
struct mylite_expression_eval_context;
struct mylite_expression_warnings;
struct mylite_dml_expression_callbacks;
struct mylite_select_order_key;
struct mylite_select_table;

enum mylite_insert_value_kind {
    MYLITE_INSERT_VALUE_UNSUPPORTED = 0,
    MYLITE_INSERT_VALUE_DEFAULT = 1,
    MYLITE_INSERT_VALUE_NULL = 2,
    MYLITE_INSERT_VALUE_INTEGER = 3,
    MYLITE_INSERT_VALUE_REAL = 4,
    MYLITE_INSERT_VALUE_TEXT = 5,
    MYLITE_INSERT_VALUE_CURRENT_TIMESTAMP = 6,
    MYLITE_INSERT_VALUE_COLUMN_REFERENCE = 7,
    MYLITE_INSERT_VALUE_UNARY_EXPRESSION = 8,
    MYLITE_INSERT_VALUE_BINARY_EXPRESSION = 9,
    MYLITE_INSERT_VALUE_VALUES_FUNCTION = 10,
    MYLITE_INSERT_VALUE_EXPRESSION = 11,
};

enum mylite_insert_bound_value_kind {
    MYLITE_INSERT_BOUND_NULL = 0,
    MYLITE_INSERT_BOUND_INTEGER = 1,
    MYLITE_INSERT_BOUND_REAL = 2,
    MYLITE_INSERT_BOUND_TEXT = 3,
};

struct mylite_insert_column_reference {
    char *schema_name;
    char *table_name;
    char *column_name;
};

struct mylite_insert_value {
    enum mylite_insert_value_kind kind;
    enum mylite_sql_ast_operator operator_kind;
    size_t values_function_count;
    char *text;
    size_t text_length;
    const struct mylite_sql_ast_node *expression;
    struct mylite_insert_column_reference column_reference;
    struct mylite_insert_value *left;
    struct mylite_insert_value *right;
};

struct mylite_insert_row {
    struct mylite_insert_value *values;
    size_t value_count;
};

struct mylite_insert_values_plan {
    char *schema_name;
    char *table_name;
    char **columns;
    size_t column_count;
    char *row_alias;
    char **alias_columns;
    size_t alias_column_count;
    bool has_column_list;
    bool ignore;
    bool replace_low_priority;
    bool replace_delayed;
    struct mylite_insert_row *rows;
    size_t row_count;
};

struct mylite_insert_set_assignment {
    struct mylite_insert_column_reference target;
    struct mylite_insert_value value;
};

struct mylite_insert_set_plan {
    struct mylite_insert_set_assignment *assignments;
    size_t assignment_count;
};

struct mylite_insert_update_assignment {
    struct mylite_insert_column_reference target;
    struct mylite_insert_value value;
};

struct mylite_insert_duplicate_update_plan {
    struct mylite_insert_update_assignment *assignments;
    size_t assignment_count;
    bool has_clause;
};

struct mylite_update_target {
    char *schema_name;
    char *table_name;
    char *alias;
};

enum mylite_update_form {
    MYLITE_UPDATE_SINGLE_TABLE = 0,
    MYLITE_UPDATE_JOINED_TABLES = 1,
};

struct mylite_update_column_reference {
    char *schema_name;
    char *table_name;
    char *column_name;
};

struct mylite_update_assignment {
    struct mylite_update_column_reference target;
    const struct mylite_sql_ast_node *value;
};

struct mylite_update_plan {
    enum mylite_update_form form;
    struct mylite_update_target target;
    struct mylite_update_assignment *assignments;
    size_t assignment_count;
    const struct mylite_sql_ast_node *from_clause;
    const struct mylite_sql_ast_node *where_clause;
    const struct mylite_sql_ast_node *order_by_clause;
    const struct mylite_sql_ast_node *limit_clause;
};

struct mylite_delete_target {
    char *schema_name;
    char *table_name;
    char *alias;
};

struct mylite_delete_plan {
    enum mylite_sql_ast_delete_form form;
    struct mylite_delete_target target;
    struct mylite_delete_target *targets;
    size_t target_count;
    const struct mylite_sql_ast_node *from_clause;
    const struct mylite_sql_ast_node *where_clause;
    const struct mylite_sql_ast_node *order_by_clause;
    const struct mylite_sql_ast_node *limit_clause;
};

struct mylite_insert_table_column {
    char *name;
    char *default_text;
    char *data_type;
    char *extra;
    bool nullable;
    bool auto_increment;
    bool generated_default;
};

struct mylite_insert_unique_index {
    char *name;
    size_t *column_indexes;
    uint64_t *prefix_lengths;
    size_t column_count;
    bool is_primary;
};

struct mylite_insert_unique_index_part {
    size_t column_index;
    uint64_t prefix_length;
    bool has_prefix_length;
};

struct mylite_insert_table {
    char *physical_name;
    struct mylite_insert_table_column *columns;
    size_t column_count;
    struct mylite_insert_unique_index *unique_indexes;
    size_t unique_index_count;
    size_t auto_increment_column_index;
    uint64_t next_auto_increment;
    bool has_auto_increment;
};

struct mylite_insert_bound_value {
    enum mylite_insert_bound_value_kind kind;
    int64_t integer_value;
    double real_value;
    char *text_value;
    size_t text_length;
    bool generated_auto_increment;
};

struct mylite_insert_execution_state {
    uint64_t next_auto_increment;
    uint64_t reserved_auto_increment_end;
    uint64_t first_insert_id;
    bool *warned_omitted_no_default_columns;
    bool *warned_null_columns;
    size_t accepted_row_count;
    size_t duplicate_count;
    bool generated_insert_id;
};

struct mylite_insert_transaction_result {
    int64_t affected_rows;
    uint64_t last_insert_id;
    bool generated_insert_id;
};

struct mylite_insert_unique_conflict {
    const struct mylite_insert_unique_index *index;
    sqlite3_int64 rowid;
    bool conflicts;
};

struct mylite_insert_row_column_indexes {
    const size_t *insert_columns;
    const size_t *update_columns;
    size_t source_column_count;
};

struct mylite_insert_update_row_values {
    const struct mylite_insert_bound_value *target_values;
    const struct mylite_insert_bound_value *candidate_values;
};

struct mylite_insert_set_row_state {
    bool *generate_auto_increment;
    bool *assigned_columns;
};

struct mylite_update_bound_assignment {
    size_t column_index;
    const struct mylite_sql_ast_node *value;
};

struct mylite_update_order_plan {
    struct mylite_select_order_key *order_keys;
    size_t order_key_count;
};

struct mylite_update_row {
    sqlite3_int64 rowid;
    struct mylite_expression_value *values;
    struct mylite_expression_value *order_values;
    size_t value_count;
    size_t order_value_count;
};

struct mylite_update_rowset {
    struct mylite_update_row *rows;
    size_t row_count;
};

typedef int (*mylite_dml_eval_session_function_fn)(
    void *user_data,
    const struct mylite_select_table *table,
    const struct mylite_sql_ast_node *function_call,
    const struct mylite_expression_eval_context *context,
    struct mylite_expression_warnings *warnings,
    struct mylite_expression_value *out_value
);
typedef int (*mylite_dml_eval_subquery_fn)(
    void *user_data,
    const struct mylite_sql_ast_node *subquery,
    struct mylite_expression_warnings *warnings,
    struct mylite_expression_value *out_value
);
typedef int (*mylite_dml_set_where_predicate_eval_error_fn)(void *user_data);

struct mylite_dml_expression_callbacks {
    void *user_data;
    mylite_dml_eval_session_function_fn eval_session_function;
    mylite_dml_eval_subquery_fn eval_subquery;
    mylite_dml_set_where_predicate_eval_error_fn set_where_predicate_eval_error;
};

struct mylite_update_expression_context {
    mylite_db *database;
    const struct mylite_select_table *table;
    const struct mylite_insert_table *write_table;
    const struct mylite_update_row *row;
    const struct mylite_dml_expression_callbacks *callbacks;
};

#endif
