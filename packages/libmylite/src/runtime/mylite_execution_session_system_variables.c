#if defined(__linux__) && !defined(_XOPEN_SOURCE)
#  define _XOPEN_SOURCE 700 /* NOLINT(bugprone-reserved-identifier): POSIX feature macro. */
#endif

#include <mylite/mylite.h>

#include "mylite_ast.h"
#include "mylite_catalog.h"
#include "mylite_connection.h"
#include "mylite_date_interval_second.h"
#include "mylite_diagnostics.h"
#include "mylite_execution_catalog.h"
#include "mylite_execution_diagnostics.h"
#include "mylite_execution_plan_types.h"
#include "mylite_execution_result_rows.h"
#include "mylite_execution_scalar.h"
#include "mylite_execution_scalar_regexp.h"
#include "mylite_execution_scalar_string_position.h"
#include "mylite_execution_scalar_temporal_format.h"
#include "mylite_execution_session_system_variables.h"
#include "mylite_execution_session_system_variables_support.h"
#include "mylite_execution_sql_normalization.h"
#include "mylite_execution_statement_transaction.h"
#include "mylite_execution_system_variables.h"
#include "mylite_mysql_error_codes.h"
#include "mylite_mysql_server_identity.h"
#include "mylite_result.h"
#include "mylite_spatial.h"
#include "mylite_statement_completion.h"
#include "mylite_statement_context.h"
#include "mylite_string_bitmask.h"
#include "mylite_string_padding.h"
#include "mylite_string_search.h"
#include "mylite_sys_functions.h"
#include "mylite_timestamp_function.h"

#include <float.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#  include <unistd.h>
#endif

#include "mylite_execution_declarations_00_constants.inc"
#include "mylite_execution_declarations_01_types.inc"
#include "mylite_execution_session_system_variables_internal.h"

int mylite_execution_scalar_hex_numeric_runtime_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct scalar_bitwise_value *out_value,
    bool *out_handled
) {
    enum mylite_execution_system_variable_kind variable = MYLITE_EXECUTION_SYSTEM_VARIABLE_NONE;
    int64_t timestamp = 0;
    int rc = MYLITE_OK;

    if (out_value == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = (struct scalar_bitwise_value){.is_null = false, .integer = 0U};
    *out_handled = true;
    if (expression == NULL) {
        *out_handled = false;
        return MYLITE_OK;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_CONNECTION_ID_FUNCTION:
        out_value->integer = database->session.connection_id;
        return MYLITE_OK;
    case MYLITE_SQL_AST_ROW_COUNT_FUNCTION:
        out_value->integer = (uint64_t)database->session.previous_row_count;
        return MYLITE_OK;
    case MYLITE_SQL_AST_FOUND_ROWS_FUNCTION:
        out_value->integer = database->session.found_rows;
        return MYLITE_OK;
    case MYLITE_SQL_AST_LAST_INSERT_ID_FUNCTION:
        out_value->integer = database->session.last_insert_id;
        return MYLITE_OK;
    case MYLITE_SQL_AST_SYSTEM_VARIABLE:
        rc = resolve_session_system_variable(database, expression, &variable);
        if (rc != MYLITE_OK) {
            return rc;
        }
        break;
    default:
        *out_handled = false;
        return MYLITE_OK;
    }

    switch (variable) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTO_INCREMENT_INCREMENT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTO_INCREMENT_OFFSET:
        out_value->integer = auto_increment_step_system_variable_value(
            database,
            variable,
            system_variable_expression_has_global_scope(expression)
        );
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INTERACTIVE_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_WAIT_TIMEOUT:
        out_value->integer = timeout_system_variable_value(
            database,
            variable,
            system_variable_expression_has_global_scope(expression)
        );
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GROUP_CONCAT_MAX_LEN:
        out_value->integer = system_variable_expression_has_global_scope(expression)
                                 ? group_concat_max_len_default_value
                                 : database->session.group_concat_max_len;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INFORMATION_SCHEMA_STATS_EXPIRY:
        out_value->integer = system_variable_expression_has_global_scope(expression)
                                 ? information_schema_stats_expiry_default_value
                                 : database->session.information_schema_stats_expiry;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_ERROR_COUNT:
        out_value->integer = system_variable_expression_has_global_scope(expression)
                                 ? max_error_count_default_value
                                 : database->session.max_error_count;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_FOREIGN_KEY_CHECKS:
        out_value->integer = foreign_key_checks_system_variable_uint64_value(
            database,
            system_variable_expression_has_global_scope(expression)
        );
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BIG_TABLES:
        out_value->integer = big_tables_system_variable_uint64_value(
            database,
            system_variable_expression_has_global_scope(expression)
        );
        return MYLITE_OK;
    default:
        break;
    }

    if (mylite_execution_system_variable_is_boolean_session_placeholder(variable)) {
        out_value->integer = boolean_session_placeholder_system_variable_uint64_value(
            database,
            variable,
            system_variable_expression_has_global_scope(expression)
        );
        return MYLITE_OK;
    }

    switch (variable) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTOCOMMIT:
        out_value->integer = system_variable_expression_has_global_scope(expression) ||
                                     database->session.autocommit_enabled
                                 ? 1U
                                 : 0U;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN:
        out_value->integer = 1U;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_GENERATE_INVISIBLE_PRIMARY_KEY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN_TRUST_FUNCTION_CREATORS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_REPLICA_SKIP_COUNTER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_REQUIRE_PRIMARY_KEY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_SLAVE_SKIP_COUNTER:
        out_value->integer = 0U;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SERVER_ID:
        out_value->integer = server_id_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SERVER_ID_BITS:
        out_value->integer = server_id_bits_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PORT:
        out_value->integer = port_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PROTOCOL_VERSION:
        out_value->integer = protocol_version_system_variable_value;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_SELECT_LIMIT:
        out_value->integer = sql_select_limit_system_variable_value(
            database,
            system_variable_expression_has_global_scope(expression)
        );
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TIMESTAMP:
        timestamp = current_timestamp_epoch(database);
        out_value->integer = (uint64_t)timestamp;
        return MYLITE_OK;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_WARNING_COUNT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ERROR_COUNT:
        return diagnostics_count_system_variable_value(
            system_variable_count_diagnostics(database),
            variable,
            &out_value->integer
        );
    default:
        break;
    }

    *out_handled = false;
    return MYLITE_OK;
}

int mylite_execution_session_system_variable_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
) {
    return system_variable_value(database, expression, out_cell);
}

int mylite_execution_session_database_character_set_system_variable_value(
    struct mylite_db *database,
    bool global_scope,
    char *buffer,
    size_t buffer_size,
    const char **out_value
) {
    return database_character_set_system_variable_value(
        database,
        global_scope,
        buffer,
        buffer_size,
        out_value
    );
}

int mylite_execution_session_database_collation_system_variable_value(
    struct mylite_db *database,
    bool global_scope,
    char *buffer,
    size_t buffer_size,
    const char **out_value
) {
    return database_collation_system_variable_value(
        database,
        global_scope,
        buffer,
        buffer_size,
        out_value
    );
}

int mylite_execution_session_format_scalar_uint64_value(
    struct mylite_db *database,
    uint64_t value,
    struct session_scalar_cell *out_cell
) {
    return format_session_scalar_uint64_value(database, value, out_cell);
}

uint64_t mylite_execution_session_timeout_default_value(
    enum mylite_execution_system_variable_kind kind
) {
    return timeout_system_variable_default_value_for_kind(kind);
}

uint64_t mylite_execution_session_timeout_min_value(enum mylite_execution_system_variable_kind kind
) {
    return timeout_system_variable_min_value_for_kind(kind);
}

uint64_t mylite_execution_session_timeout_max_value(enum mylite_execution_system_variable_kind kind
) {
    return timeout_system_variable_max_value_for_kind(kind);
}

int mylite_execution_session_resolve_system_variable(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum mylite_execution_system_variable_kind *out_kind
) {
    return resolve_session_system_variable(database, expression, out_kind);
}

bool mylite_execution_session_foreign_key_checks_value(
    const struct mylite_db *database,
    bool global_scope
) {
    return foreign_key_checks_system_variable_value(database, global_scope);
}

int mylite_execution_session_append_system_variable_read_warning(
    struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind
) {
    return append_system_variable_read_warning(database, kind);
}

bool mylite_execution_session_resolve_system_variable_kind(
    const struct system_variable_component *name,
    enum mylite_execution_system_variable_kind *out_kind
) {
    return resolve_system_variable_kind(name, out_kind);
}

int mylite_execution_session_parse_system_variable_component(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    size_t *offset,
    struct system_variable_component *out_component
) {
    return parse_system_variable_component(database, span, offset, out_component);
}

bool mylite_execution_session_system_variable_component_equals(
    const struct system_variable_component *component,
    const char *expected
) {
    return system_variable_component_equals(component, expected);
}

bool mylite_execution_session_system_variable_component_is_empty(
    const struct system_variable_component *component
) {
    return system_variable_component_is_empty(component);
}

int mylite_execution_session_show_system_variable_value(
    struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind,
    bool global_scope,
    char *integer_buffer,
    size_t integer_buffer_size,
    const char **out_value
) {
    return show_system_variable_value(
        database,
        kind,
        global_scope,
        integer_buffer,
        integer_buffer_size,
        out_value
    );
}

#include "mylite_execution_scalar_system_variables.inc"

static const char *session_system_variable_override_value(
    const struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind
) {
    return mylite_execution_session_system_variable_override_value(database, kind);
}

static int format_uint64(
    struct mylite_db *database,
    uint64_t value,
    char *buffer,
    size_t buffer_size
) {
    return mylite_execution_format_uint64(database, value, buffer, buffer_size);
}

static int previous_diagnostics_condition_count(
    const struct mylite_diagnostics *diagnostics,
    uint64_t *out_count
) {
    return mylite_execution_session_previous_diagnostics_condition_count(diagnostics, out_count);
}

static int previous_diagnostics_error_count(
    const struct mylite_diagnostics *diagnostics,
    uint64_t *out_count
) {
    return mylite_execution_session_previous_diagnostics_error_count(diagnostics, out_count);
}

static const struct mylite_execution_catalog_builtin_schema *find_builtin_schema_descriptor(
    const char *schema_name
) {
    return mylite_execution_catalog_builtin_schema_by_name(schema_name);
}

static int resolve_schema_name(
    struct mylite_db *database,
    const char *schema_name,
    struct mylite_catalog_schema_descriptor *out_schema
) {
    return mylite_execution_session_resolve_schema_name(database, schema_name, out_schema);
}

static const char *session_system_variable_override_show_value(
    const struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind
) {
    return mylite_execution_session_system_variable_override_show_value(database, kind);
}

static const char *myisam_stats_method_text(enum mylite_session_myisam_stats_method value) {
    return mylite_execution_session_myisam_stats_method_text(value);
}

static const char *transaction_isolation_value_text(enum mylite_transaction_isolation isolation) {
    return mylite_execution_session_transaction_isolation_text(isolation);
}

static const char *transaction_read_only_scalar_text(enum mylite_transaction_access_mode access_mode
) {
    return mylite_execution_session_transaction_read_only_scalar_text(access_mode);
}

static const char *transaction_read_only_show_text(enum mylite_transaction_access_mode access_mode
) {
    return mylite_execution_session_transaction_read_only_show_text(access_mode);
}

static bool sql_mode_token_matches(const char *text, size_t length, const char *expected) {
    return mylite_execution_session_sql_mode_token_matches(text, length, expected);
}

static int64_t current_timestamp_epoch(const struct mylite_db *database) {
    return mylite_execution_current_timestamp_epoch(database);
}

static bool text_equals_ascii_case_insensitive(const char *left, const char *right) {
    return mylite_execution_text_equals_ascii_case_insensitive(left, right);
}
