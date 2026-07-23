#if defined(__linux__) && !defined(_XOPEN_SOURCE)
#  define _XOPEN_SOURCE 700 /* NOLINT(bugprone-reserved-identifier): POSIX feature macro. */
#endif

#include <mylite/mylite.h>

#include "mylite_ast.h"
#include "mylite_catalog.h"
#include "mylite_collation.h"
#include "mylite_connection.h"
#include "mylite_date_interval_second.h"
#include "mylite_diagnostics.h"
#include "mylite_dynamic_string.h"
#include "mylite_execution_ast_internal.h"
#include "mylite_execution_catalog.h"
#include "mylite_execution_completion.h"
#include "mylite_execution_diagnostics.h"
#include "mylite_execution_dml_numeric.h"
#include "mylite_execution_information_schema_join_plan.h"
#include "mylite_execution_information_schema_plan.h"
#include "mylite_execution_loaded_catalog.h"
#include "mylite_execution_plan_types.h"
#include "mylite_execution_result_rows.h"
#include "mylite_execution_scalar.h"
#include "mylite_execution_scalar_regexp.h"
#include "mylite_execution_scalar_string_position.h"
#include "mylite_execution_select_analysis.h"
#include "mylite_execution_select_order_plan.h"
#include "mylite_execution_session_system_variables.h"
#include "mylite_execution_set.h"
#include "mylite_execution_set_support.h"
#include "mylite_execution_sql_normalization.h"
#include "mylite_execution_sqlite_internal.h"
#include "mylite_execution_statement_transaction.h"
#include "mylite_execution_system_variables.h"
#include "mylite_execution_transaction_control.h"
#include "mylite_execution_value.h"
#include "mylite_mysql_error_codes.h"
#include "mylite_numeric_locale.h"
#include "mylite_rand.h"
#include "mylite_result.h"
#include "mylite_spatial.h"
#include "mylite_sqlite_registration.h"
#include "mylite_statement_completion.h"
#include "mylite_statement_context.h"
#include "mylite_string_bitmask.h"
#include "mylite_sys_functions.h"
#include "sqlite3.h"

#include <errno.h>
#include <float.h>
#include <inttypes.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mylite_execution_declarations_00_constants.inc"
#include "mylite_execution_declarations_01_types.inc"
#define MYLITE_EXECUTION_SET_MODULE 1
#include "mylite_execution_declarations_10_statement.inc"
#undef MYLITE_EXECUTION_SET_MODULE

int mylite_execution_execute_set_connection_character_set_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool allow_collation,
    mylite_result **out_result
) {
    return execute_set_connection_character_set_statement(
        database,
        statement,
        allow_collation,
        out_result
    );
}

int mylite_execution_apply_set_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
) {
    return apply_set_statement(database, statement);
}

int mylite_execution_set_session_user_variable(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    return set_session_user_variable(database, target, value, value_kind);
}

int mylite_execution_set_session_user_variable_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct session_scalar_cell *out_cell
) {
    return session_user_variable_value(database, node, out_cell);
}

int mylite_execution_set_session_user_variable_value_kind(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    enum mylite_session_user_variable_value_kind *out_value_kind
) {
    return session_user_variable_value_kind(database, node, out_value_kind);
}

void mylite_execution_clear_session_system_variable_override(
    struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind
) {
    clear_session_system_variable_override(database, kind);
}

const char *mylite_execution_set_session_system_variable_override_value(
    const struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind
) {
    return session_system_variable_override_value(database, kind);
}

const char *mylite_execution_set_session_system_variable_override_show_value(
    const struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind
) {
    return session_system_variable_override_show_value(database, kind);
}

const char *mylite_execution_myisam_stats_method_text(enum mylite_session_myisam_stats_method value
) {
    return myisam_stats_method_text(value);
}

void mylite_execution_set_system_variable_value_error(
    struct mylite_db *database,
    const char *variable_name,
    const char *value
) {
    set_system_variable_cant_be_set_value_error(database, variable_name, value);
}

int mylite_execution_set_copy_user_variable_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    char *destination,
    size_t destination_size,
    bool allow_system_variable
) {
    return copy_user_variable_name(
        database,
        node,
        destination,
        destination_size,
        allow_system_variable
    );
}

struct mylite_session_user_variable *mylite_execution_set_find_session_user_variable(
    struct mylite_session_state *session,
    const char *name
) {
    return find_session_user_variable(session, name);
}

bool mylite_execution_set_text_is_decimal_integer_literal(const char *text, size_t text_size) {
    return text_is_decimal_integer_literal(text, text_size);
}

void mylite_execution_set_fold_user_variable_name(char *text) {
    fold_user_variable_name(text);
}

enum mylite_session_user_variable_value_kind mylite_execution_set_infer_user_variable_value_kind(
    const struct mylite_sql_ast_node *value_node,
    const struct session_scalar_cell *value
) {
    return infer_user_variable_value_kind(value_node, value);
}

bool mylite_execution_session_sql_mode_has(
    const struct mylite_session_state *session,
    uint64_t mode
) {
    return session_sql_mode_has(session, mode);
}

unsigned int mylite_execution_lexer_modes_for_session_sql_mode(
    const struct mylite_session_state *session
) {
    return lexer_modes_for_session_sql_mode(session);
}

unsigned int mylite_execution_lexer_modes_for_statement(const struct mylite_db *database) {
    return lexer_modes_for_statement(database);
}

bool mylite_execution_set_sql_mode_token_matches(
    const char *text,
    size_t length,
    const char *expected
) {
    return sql_mode_token_matches(text, length, expected);
}

static int append_system_variable_read_warning(
    struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind
) {
    return mylite_execution_session_append_system_variable_read_warning(database, kind);
}

static const struct mylite_sql_ast_node *unwrap_parenthesized_expression(
    const struct mylite_sql_ast_node *expression
) {
    return mylite_execution_unwrap_parenthesized_expression(expression);
}

static void session_scalar_cell_deinit(struct session_scalar_cell *cell) {
    mylite_execution_session_scalar_cell_deinit(cell);
}

static int decode_sql_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    const char *unsupported_message,
    const char *nul_message,
    char **out_text,
    size_t *out_text_length
) {
    return mylite_execution_decode_sql_string_literal(
        database,
        literal_node,
        unsupported_message,
        nul_message,
        out_text,
        out_text_length
    );
}

static int session_scalar_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return mylite_execution_session_scalar_value(database, expression, out_cell);
}

static int validate_utf8_text(const char *text, size_t text_length, size_t *out_character_count) {
    return mylite_execution_validate_utf8_text(text, text_length, out_character_count);
}

static char ascii_lower(unsigned char value) {
    return (char)(value >= (unsigned char)'A' && value <= (unsigned char)'Z'
                      ? value + ((unsigned char)'a' - (unsigned char)'A')
                      : value);
}

static bool text_equals_ascii_case_insensitive(const char *left, const char *right) {
    return mylite_execution_text_equals_ascii_case_insensitive(left, right);
}

static int database_character_set_system_variable_value(
    struct mylite_db *database,
    bool global_scope,
    char *buffer,
    size_t buffer_size,
    const char **out_value
) {
    return mylite_execution_session_database_character_set_system_variable_value(
        database,
        global_scope,
        buffer,
        buffer_size,
        out_value
    );
}

static int database_collation_system_variable_value(
    struct mylite_db *database,
    bool global_scope,
    char *buffer,
    size_t buffer_size,
    const char **out_value
) {
    return mylite_execution_session_database_collation_system_variable_value(
        database,
        global_scope,
        buffer,
        buffer_size,
        out_value
    );
}

static int append_utf8_alias_warning(struct mylite_db *database) {
    return mylite_execution_set_append_utf8_alias_warning(database);
}

static int append_utf8mb3_deprecation_warning(struct mylite_db *database) {
    return mylite_execution_set_append_utf8mb3_deprecation_warning(database);
}

static uint64_t timeout_system_variable_default_value_for_kind(
    enum mylite_execution_system_variable_kind kind
) {
    return mylite_execution_session_timeout_default_value(kind);
}

static uint64_t timeout_system_variable_min_value_for_kind(
    enum mylite_execution_system_variable_kind kind
) {
    return mylite_execution_session_timeout_min_value(kind);
}

static uint64_t timeout_system_variable_max_value_for_kind(
    enum mylite_execution_system_variable_kind kind
) {
    return mylite_execution_session_timeout_max_value(kind);
}

static int format_uint64(
    struct mylite_db *database,
    uint64_t value,
    char *buffer,
    size_t buffer_size
) {
    return mylite_execution_format_uint64(database, value, buffer, buffer_size);
}

static bool resolve_system_variable_kind(
    const struct system_variable_component *name,
    enum mylite_execution_system_variable_kind *out_kind
) {
    return mylite_execution_session_resolve_system_variable_kind(name, out_kind);
}

static int parse_system_variable_component(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    size_t *offset,
    struct system_variable_component *out_component
) {
    return mylite_execution_session_parse_system_variable_component(
        database,
        span,
        offset,
        out_component
    );
}

static bool system_variable_component_is_empty(const struct system_variable_component *component) {
    return mylite_execution_session_system_variable_component_is_empty(component);
}

static bool system_variable_component_equals(
    const struct system_variable_component *component,
    const char *expected
) {
    return mylite_execution_session_system_variable_component_equals(component, expected);
}

static int parse_unsigned_integer_literal(
    const struct mylite_sql_source_span *span,
    uint64_t *out_value
) {
    return mylite_execution_parse_unsigned_integer_literal(span, out_value);
}

static int decode_table_option_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    char **out_name,
    size_t *out_name_length,
    struct table_option_name_policy policy
) {
    return mylite_execution_set_decode_table_option_string_literal(
        database,
        node,
        out_name,
        out_name_length,
        policy.identifier_kind,
        policy.nul_message
    );
}

static int resolve_session_system_variable(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum mylite_execution_system_variable_kind *out_kind
) {
    return mylite_execution_session_resolve_system_variable(database, expression, out_kind);
}

static int64_t current_timestamp_epoch(const struct mylite_db *database) {
    return mylite_execution_current_timestamp_epoch(database);
}

#include "mylite_execution_set_assignments.inc"
#include "mylite_execution_set_binary_log_system_variables.inc"
#include "mylite_execution_set_boolean_variables.inc"
#include "mylite_execution_set_bootstrap_system_variables.inc"
#include "mylite_execution_set_compatibility_system_variables.inc"
#include "mylite_execution_set_connection_charset.inc"
#include "mylite_execution_set_connection_system_variables.inc"
#include "mylite_execution_set_innodb_core_system_variables.inc"
#include "mylite_execution_set_innodb_storage_system_variables.inc"
#include "mylite_execution_set_internal_session_system_variables.inc"
#include "mylite_execution_set_jl_system_variables.inc"
#include "mylite_execution_set_last_insert_id_variables.inc"
#include "mylite_execution_set_limit_size_expiry_variables.inc"
#include "mylite_execution_set_m_session_limit_system_variables.inc"
#include "mylite_execution_set_myisam_system_variables.inc"
#include "mylite_execution_set_numeric_transaction_variables.inc"
#include "mylite_execution_set_o_optimizer_system_variables.inc"
#include "mylite_execution_set_remaining_system_variables.inc"
#include "mylite_execution_set_replication_global_system_variables.inc"
#include "mylite_execution_set_resource_tuning_system_variables.inc"
#include "mylite_execution_set_session_snapshot.inc"
#include "mylite_execution_set_sql_mode_timestamp_time_zone.inc"
#include "mylite_execution_set_system_variable_dispatch.inc"
#include "mylite_execution_set_timeout_variables.inc"
