static int apply_set_bootstrap_system_variable_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
) {
    if (target == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    if (!mylite_execution_system_variable_is_bootstrap_placeholder(target->kind)) {
        return MYLITE_OK;
    }

    *out_handled = true;
    if (bootstrap_system_variable_is_global_only(target->kind) &&
        target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_global_variable_set_global_required_error(database, target->name);
        return MYLITE_ERROR;
    }
    return validate_set_bootstrap_system_variable_value(database, target, value_node);
}

static int apply_set_bootstrap_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target
) {
    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (bootstrap_system_variable_is_global_only(target->kind) &&
        target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_global_variable_set_global_required_error(database, target->name);
        return MYLITE_ERROR;
    }
    set_unsupported_error(
        database,
        "SET bootstrap system variables from user variables is not supported"
    );
    return MYLITE_ERROR;
}

static bool bootstrap_system_variable_is_global_only(enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ACTIVATE_ALL_ROLES_ON_LOGIN:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTOMATIC_SP_PRIVILEGES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHECK_PROXY_USERS:
        return true;
    default:
        return false;
    }
}

static int validate_set_bootstrap_system_variable_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    const char *unsupported_message = "SET supports only fixed no-op system variable assignments";

    if (target == NULL) {
        return MYLITE_MISUSE;
    }

    switch (target->kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_ACTIVATE_ALL_ROLES_ON_LOGIN:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHECK_PROXY_USERS:
        return validate_set_fixed_boolean_value(database, value_node, false);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTOMATIC_SP_PRIVILEGES:
        return validate_set_fixed_boolean_value(database, value_node, true);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BLOCK_ENCRYPTION_MODE:
        return validate_set_fixed_text_value(
            database,
            value_node,
            mylite_execution_system_variable_default_value(target->kind)
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_BULK_INSERT_BUFFER_SIZE:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            bulk_insert_buffer_size_default_value,
            unsupported_message
        );
    default:
        set_runtime_error(database, "invalid bootstrap system variable");
        return MYLITE_ERROR;
    }
}
