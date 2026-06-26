static const uint64_t replication_global_rpl_read_size_default_value = 8192ULL;
static const uint64_t replication_global_stop_timeout_default_value = 31536000ULL;
static const uint64_t replication_global_sync_info_default_value = 10000ULL;
static const uint64_t replication_global_checkpoint_group_default_value = 512ULL;
static const uint64_t replication_global_checkpoint_period_default_value = 300ULL;
static const uint64_t replication_global_max_allowed_packet_default_value = 1073741824ULL;
static const uint64_t replication_global_net_timeout_default_value = 60ULL;
static const uint64_t replication_global_parallel_workers_default_value = 4ULL;
static const uint64_t replication_global_pending_jobs_size_max_default_value = 134217728ULL;
static const uint64_t replication_global_transaction_retries_default_value = 10ULL;

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
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_ALLOW_BATCHING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_ALLOW_BATCHING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RELAY_LOG_PURGE:
        rc = validate_set_fixed_boolean_value(database, value_node, true);
        break;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_CHECKPOINT_GROUP:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_CHECKPOINT_GROUP:
        rc = validate_set_fixed_uint64_value(
            database,
            value_node,
            replication_global_checkpoint_group_default_value,
            unsupported_message
        );
        break;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_CHECKPOINT_PERIOD:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_CHECKPOINT_PERIOD:
        rc = validate_set_fixed_uint64_value(
            database,
            value_node,
            replication_global_checkpoint_period_default_value,
            unsupported_message
        );
        break;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_COMPRESSED_PROTOCOL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_COMPRESSED_PROTOCOL:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICATION_OPTIMIZE_FOR_STATIC_PLUGIN_CONFIG:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICATION_SENDER_OBSERVE_COMMIT_ONLY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SOURCE_VERIFY_CHECKSUM:
        rc = validate_set_fixed_boolean_value(database, value_node, false);
        break;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_EXEC_MODE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_EXEC_MODE:
        rc = validate_set_fixed_text_value(database, value_node, "STRICT");
        break;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_MAX_ALLOWED_PACKET:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_MAX_ALLOWED_PACKET:
        rc = validate_set_fixed_uint64_value(
            database,
            value_node,
            replication_global_max_allowed_packet_default_value,
            unsupported_message
        );
        break;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_NET_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_NET_TIMEOUT:
        rc = validate_set_fixed_uint64_value(
            database,
            value_node,
            replication_global_net_timeout_default_value,
            unsupported_message
        );
        break;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_PARALLEL_TYPE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_PARALLEL_TYPE:
        rc = validate_set_fixed_text_value(database, value_node, "LOGICAL_CLOCK");
        break;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_PARALLEL_WORKERS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_PARALLEL_WORKERS:
        rc = validate_set_fixed_uint64_value(
            database,
            value_node,
            replication_global_parallel_workers_default_value,
            unsupported_message
        );
        break;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_PENDING_JOBS_SIZE_MAX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_PENDING_JOBS_SIZE_MAX:
        rc = validate_set_fixed_uint64_value(
            database,
            value_node,
            replication_global_pending_jobs_size_max_default_value,
            unsupported_message
        );
        break;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_PRESERVE_COMMIT_ORDER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_PRESERVE_COMMIT_ORDER:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_SQL_VERIFY_CHECKSUM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_SQL_VERIFY_CHECKSUM:
        rc = validate_set_fixed_boolean_value(database, value_node, true);
        break;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_TRANSACTION_RETRIES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_TRANSACTION_RETRIES:
        rc = validate_set_fixed_uint64_value(
            database,
            value_node,
            replication_global_transaction_retries_default_value,
            unsupported_message
        );
        break;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REPLICA_TYPE_CONVERSIONS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLAVE_TYPE_CONVERSIONS:
        rc = validate_set_fixed_text_value(database, value_node, "");
        break;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RPL_READ_SIZE:
        rc = validate_set_fixed_uint64_value(
            database,
            value_node,
            replication_global_rpl_read_size_default_value,
            unsupported_message
        );
        break;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RPL_STOP_REPLICA_TIMEOUT:
        rc = validate_set_fixed_uint64_value(
            database,
            value_node,
            replication_global_stop_timeout_default_value,
            unsupported_message
        );
        break;
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
        rc = validate_set_fixed_uint64_value(
            database,
            value_node,
            replication_global_sync_info_default_value,
            unsupported_message
        );
        break;
    default:
        set_runtime_error(database, "invalid replication global system variable");
        return MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && mylite_execution_system_variable_warns_on_scalar_read(target->kind)) {
        rc = append_system_variable_read_warning(database, target->kind);
    }
    return rc;
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
