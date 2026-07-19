#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_SESSION_SYSTEM_VARIABLES_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_SESSION_SYSTEM_VARIABLES_H

#include "mylite_execution_system_variables.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct mylite_db;
struct mylite_diagnostics;
struct mylite_sql_ast_node;
struct mylite_sql_source_span;
struct session_scalar_cell;
struct system_variable_component;

int mylite_execution_session_system_variable_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct session_scalar_cell *out_cell
);
int mylite_execution_session_database_character_set_system_variable_value(
    struct mylite_db *database,
    bool global_scope,
    char *buffer,
    size_t buffer_size,
    const char **out_value
);
int mylite_execution_session_database_collation_system_variable_value(
    struct mylite_db *database,
    bool global_scope,
    char *buffer,
    size_t buffer_size,
    const char **out_value
);
int mylite_execution_session_format_scalar_uint64_value(
    struct mylite_db *database,
    uint64_t value,
    struct session_scalar_cell *out_cell
);
uint64_t mylite_execution_session_timeout_default_value(
    enum mylite_execution_system_variable_kind kind
);
uint64_t mylite_execution_session_timeout_min_value(enum mylite_execution_system_variable_kind kind
);
uint64_t mylite_execution_session_timeout_max_value(enum mylite_execution_system_variable_kind kind
);
int mylite_execution_session_resolve_system_variable(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    enum mylite_execution_system_variable_kind *out_kind
);
bool mylite_execution_session_foreign_key_checks_value(
    const struct mylite_db *database,
    bool global_scope
);
int mylite_execution_session_append_system_variable_read_warning(
    struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind
);
bool mylite_execution_session_resolve_system_variable_kind(
    const struct system_variable_component *name,
    enum mylite_execution_system_variable_kind *out_kind
);
int mylite_execution_session_parse_system_variable_component(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    size_t *offset,
    struct system_variable_component *out_component
);
bool mylite_execution_session_system_variable_component_equals(
    const struct system_variable_component *component,
    const char *expected
);
bool mylite_execution_session_system_variable_component_is_empty(
    const struct system_variable_component *component
);
int mylite_execution_session_show_system_variable_value(
    struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind,
    bool global_scope,
    char *integer_buffer,
    size_t integer_buffer_size,
    const char **out_value
);

#endif
