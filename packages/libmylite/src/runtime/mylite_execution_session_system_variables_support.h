#ifndef MYLITE_RUNTIME_MYLITE_EXECUTION_SESSION_SYSTEM_VARIABLES_SUPPORT_H
#define MYLITE_RUNTIME_MYLITE_EXECUTION_SESSION_SYSTEM_VARIABLES_SUPPORT_H

#include "mylite_connection.h"
#include "mylite_execution_system_variables.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct mylite_catalog_schema_descriptor;
struct mylite_db;
struct mylite_diagnostics;

const char *mylite_execution_session_system_variable_override_value(
    const struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind
);
int mylite_execution_session_previous_diagnostics_condition_count(
    const struct mylite_diagnostics *diagnostics,
    uint64_t *out_count
);
int mylite_execution_session_previous_diagnostics_error_count(
    const struct mylite_diagnostics *diagnostics,
    uint64_t *out_count
);
int mylite_execution_session_resolve_schema_name(
    struct mylite_db *database,
    const char *schema_name,
    struct mylite_catalog_schema_descriptor *out_schema
);
const char *mylite_execution_session_system_variable_override_show_value(
    const struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind
);
const char *mylite_execution_session_myisam_stats_method_text(
    enum mylite_session_myisam_stats_method value
);
const char *mylite_execution_session_transaction_isolation_text(
    enum mylite_transaction_isolation isolation
);
const char *mylite_execution_session_transaction_read_only_scalar_text(
    enum mylite_transaction_access_mode access_mode
);
const char *mylite_execution_session_transaction_read_only_show_text(
    enum mylite_transaction_access_mode access_mode
);
bool mylite_execution_session_sql_mode_token_matches(
    const char *text,
    size_t length,
    const char *expected
);

#endif
