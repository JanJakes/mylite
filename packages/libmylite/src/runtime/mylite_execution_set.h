#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_SET_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_SET_H

#include "mylite_connection.h"
#include "mylite_execution_system_variables.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct mylite_db;
struct mylite_result;
struct mylite_sql_ast_node;
struct session_scalar_cell;

int mylite_execution_execute_set_connection_character_set_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool allow_collation,
    struct mylite_result **out_result
);
int mylite_execution_apply_set_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
);
int mylite_execution_set_session_user_variable(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
);
int mylite_execution_set_session_user_variable_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct session_scalar_cell *out_cell
);
int mylite_execution_set_session_user_variable_value_kind(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    enum mylite_session_user_variable_value_kind *out_value_kind
);
void mylite_execution_clear_session_system_variable_override(
    struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind
);
const char *mylite_execution_set_session_system_variable_override_value(
    const struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind
);
const char *mylite_execution_set_session_system_variable_override_show_value(
    const struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind
);
const char *mylite_execution_myisam_stats_method_text(enum mylite_session_myisam_stats_method value
);
void mylite_execution_set_system_variable_value_error(
    struct mylite_db *database,
    const char *variable_name,
    const char *value
);
int mylite_execution_set_copy_user_variable_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    char *destination,
    size_t destination_size,
    bool allow_system_variable
);
struct mylite_session_user_variable *mylite_execution_set_find_session_user_variable(
    struct mylite_session_state *session,
    const char *name
);
bool mylite_execution_set_text_is_decimal_integer_literal(const char *text, size_t text_size);
void mylite_execution_set_fold_user_variable_name(char *text);
enum mylite_session_user_variable_value_kind mylite_execution_set_infer_user_variable_value_kind(
    const struct mylite_sql_ast_node *value_node,
    const struct session_scalar_cell *value
);
bool mylite_execution_session_sql_mode_has(
    const struct mylite_session_state *session,
    uint64_t mode
);
unsigned int mylite_execution_lexer_modes_for_session_sql_mode(
    const struct mylite_session_state *session
);
unsigned int mylite_execution_lexer_modes_for_statement(const struct mylite_db *database);
bool mylite_execution_set_sql_mode_token_matches(
    const char *text,
    size_t length,
    const char *expected
);

#endif
