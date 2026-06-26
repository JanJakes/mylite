static const uint64_t replication_global_rpl_read_size_default_value = 8192ULL;
static const uint64_t replication_global_stop_timeout_default_value = 31536000ULL;
static const uint64_t replication_global_sync_info_default_value = 10000ULL;

static int apply_set_replication_global_system_variable_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
) {
    const char *unsupported_message = "SET supports only fixed no-op system variable assignments";
    int rc = MYLITE_OK;

    if (target == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    if (!mylite_execution_system_variable_is_replication_global(target->kind)) {
        return MYLITE_OK;
    }

    *out_handled = true;
    if (mylite_execution_system_variable_is_read_only_replication_global(target->kind)) {
        set_read_only_system_variable_error(database, target->name);
        return MYLITE_ERROR;
    }
    if (target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_global_variable_set_global_required_error(database, target->name);
        return MYLITE_ERROR;
    }

    switch (target->kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RELAY_LOG_PURGE:
        return validate_set_fixed_boolean_value(database, value_node, true);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICATION_OPTIMIZE_FOR_STATIC_PLUGIN_CONFIG:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICATION_SENDER_OBSERVE_COMMIT_ONLY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SOURCE_VERIFY_CHECKSUM:
        return validate_set_fixed_boolean_value(database, value_node, false);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RPL_READ_SIZE:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            replication_global_rpl_read_size_default_value,
            unsupported_message
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RPL_STOP_REPLICA_TIMEOUT:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            replication_global_stop_timeout_default_value,
            unsupported_message
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RPL_STOP_SLAVE_TIMEOUT:
        rc = validate_set_fixed_uint64_value(
            database,
            value_node,
            replication_global_stop_timeout_default_value,
            unsupported_message
        );
        if (rc == MYLITE_OK) {
            rc = append_system_variable_read_warning(database, target->kind);
        }
        return rc;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SYNC_MASTER_INFO:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SYNC_RELAY_LOG_INFO:
        rc = validate_set_fixed_uint64_value(
            database,
            value_node,
            replication_global_sync_info_default_value,
            unsupported_message
        );
        if (rc == MYLITE_OK) {
            rc = append_system_variable_read_warning(database, target->kind);
        }
        return rc;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SYNC_RELAY_LOG:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SYNC_SOURCE_INFO:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            replication_global_sync_info_default_value,
            unsupported_message
        );
    default:
        set_runtime_error(database, "invalid replication global system variable");
        return MYLITE_ERROR;
    }
}

static int apply_set_replication_global_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    bool *out_handled
) {
    if (target == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    if (!mylite_execution_system_variable_is_replication_global(target->kind)) {
        return MYLITE_OK;
    }

    *out_handled = true;
    if (mylite_execution_system_variable_is_read_only_replication_global(target->kind)) {
        set_read_only_system_variable_error(database, target->name);
        return MYLITE_ERROR;
    }
    if (target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_global_variable_set_global_required_error(database, target->name);
        return MYLITE_ERROR;
    }
    set_unsupported_error(
        database,
        "SET replication system variables from user variables are not supported"
    );
    return MYLITE_ERROR;
}
