#ifndef MYLITE_RUNTIME_MYLITE_RUNTIME_H
#define MYLITE_RUNTIME_MYLITE_RUNTIME_H

#include <mylite/mylite.h>

#include "mylite_connection_statement_types.h"
#include "mylite_dml_types.h"
#include "mylite_expression.h"
#include "mylite_expression_collation_types.h"
#include "mylite_field_descriptor.h"
#include "mylite_metadata_types.h"
#include "mylite_prepared_statements_types.h"
#include "mylite_schema_types.h"
#include "mylite_select_types.h"
#include "mylite_show_types.h"
#include "mylite_statement_types.h"
#include "mylite_table_ddl_types.h"
#include "mylite_transaction_types.h"
#include "mylite_user_variables_types.h"
#include "sql/mylite_ast.h"
#include "sqlite3.h"
#include "types/mylite_column_type.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

struct mylite_uuid_state {
    uint64_t last_timestamp;
    uint16_t clock_sequence;
    unsigned char node[6];
    bool initialized;
};

struct mylite_uuid_short_state {
    uint64_t startup_seconds;
    uint32_t counter;
    uint8_t server_id;
    bool initialized;
};

struct mylite_db {
    sqlite3 *sqlite;
    char *error_message;
    struct mylite_expression_warnings warnings;
    char *selected_schema;
    uint64_t connection_id;
    uint64_t last_insert_id;
    uint64_t previous_found_rows;
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
    char *sql_mode;
    uint64_t group_concat_max_len;
    struct mylite_uuid_state uuid_state;
    struct mylite_uuid_short_state uuid_short_state;
    struct mylite_user_variable_store user_variables;
    struct mylite_prepared_statement_store prepared_statements;
};

struct mylite_statement_timestamp {
    time_t seconds;
    long microseconds;
};

struct mylite_rand_state {
    const struct mylite_sql_ast_node *function_call;
    uint32_t seed1;
    uint32_t seed2;
    bool initialized;
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
    bool previous_found_rows_recorded;
    bool preserve_prepare_warnings;
    bool has_statement_timestamp;
    struct mylite_statement_timestamp statement_timestamp;
    struct mylite_schema_options options;
    struct mylite_connection_charset_plan connection_charset;
    struct mylite_set_user_variable_plan set_user_variable;
    struct mylite_prepare_statement_plan prepare_statement;
    struct mylite_execute_prepared_plan execute_prepared;
    struct mylite_deallocate_prepare_plan deallocate_prepare;
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
    struct mylite_connection_system_variable_plan connection_system_variable;
    struct mylite_result_metadata result_metadata;
    struct mylite_scalar_result scalar_result;
    struct mylite_table_select_result select_result;
    struct mylite_sql_ast select_predicate_ast;
    struct mylite_sql_ast scalar_select_ast;
    struct mylite_sql_ast insert_ast;
    struct mylite_sql_ast update_ast;
    struct mylite_sql_ast delete_ast;
    const struct mylite_sql_ast_node *select_predicate;
    char *select_sql_text;
    char *scalar_select_sql_text;
    char *insert_sql_text;
    char *update_sql_text;
    char *delete_sql_text;
    struct mylite_cached_expression_value *select_constant_values;
    size_t select_constant_value_count;
    struct mylite_rand_state *rand_states;
    size_t rand_state_count;
    mylite_stmt *prepared_execute_stmt;
    bool select_constant_predicate_evaluated;
    bool select_constant_predicate_matches;
    int64_t affected_rows;
    uint64_t matched_rows;
    uint64_t found_rows;
};

#endif
