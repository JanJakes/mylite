static const uint64_t cte_max_recursion_depth_default_value = 1000ULL;
static const uint64_t default_week_format_default_value = 0ULL;
static const uint64_t delayed_insert_limit_default_value = 100ULL;
static const uint64_t delayed_insert_timeout_default_value = 300ULL;
static const uint64_t delayed_queue_size_default_value = 1000ULL;
static const uint64_t div_precision_increment_default_value = 4ULL;

static int apply_set_compatibility_system_variable_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
) {
    int rc = MYLITE_OK;

    if (target == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    if (!mylite_execution_system_variable_is_compatibility_placeholder(target->kind)) {
        return MYLITE_OK;
    }

    *out_handled = true;
    if (!mylite_execution_system_variable_allows_session_scope(target->kind) &&
        target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_global_variable_set_global_required_error(database, target->name);
        return MYLITE_ERROR;
    }

    rc = validate_set_compatibility_system_variable_value(database, target, value_node);
    if (rc != MYLITE_OK) {
        return rc;
    }
    return append_compatibility_system_variable_set_warning_if_needed(database, target->kind);
}

static int apply_set_compatibility_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target
) {
    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (!mylite_execution_system_variable_allows_session_scope(target->kind) &&
        target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_global_variable_set_global_required_error(database, target->name);
        return MYLITE_ERROR;
    }
    set_unsupported_error(
        database,
        "SET compatibility system variables from user variables is not supported"
    );
    return MYLITE_ERROR;
}

static int validate_set_compatibility_system_variable_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    const char *default_value = NULL;
    bool expected_boolean_value = false;

    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    default_value = mylite_execution_system_variable_default_value(target->kind);
    if (default_value == NULL) {
        set_runtime_error(database, "invalid compatibility system variable");
        return MYLITE_ERROR;
    }
    if (compatibility_system_variable_is_boolean(target->kind)) {
        expected_boolean_value = strcmp(default_value, "1") == 0;
        return validate_set_fixed_boolean_value(database, value_node, expected_boolean_value);
    }
    if (compatibility_system_variable_is_uint64(target->kind)) {
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            compatibility_system_variable_uint64_value(target->kind),
            "SET compatibility system variables support only fixed no-op assignments"
        );
    }
    return validate_set_fixed_text_value(database, value_node, default_value);
}

static bool compatibility_system_variable_is_boolean(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_TABLE_ENCRYPTION:
        return true;
    default:
        return false;
    }
}

static bool compatibility_system_variable_is_uint64(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CTE_MAX_RECURSION_DEPTH:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_WEEK_FORMAT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DELAYED_INSERT_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DELAYED_INSERT_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DELAYED_QUEUE_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DIV_PRECISION_INCREMENT:
        return true;
    default:
        return false;
    }
}

static uint64_t compatibility_system_variable_uint64_value(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CTE_MAX_RECURSION_DEPTH:
        return cte_max_recursion_depth_default_value;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_WEEK_FORMAT:
        return default_week_format_default_value;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DELAYED_INSERT_LIMIT:
        return delayed_insert_limit_default_value;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DELAYED_INSERT_TIMEOUT:
        return delayed_insert_timeout_default_value;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DELAYED_QUEUE_SIZE:
        return delayed_queue_size_default_value;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DIV_PRECISION_INCREMENT:
        return div_precision_increment_default_value;
    default:
        return 0ULL;
    }
}

static int append_compatibility_system_variable_set_warning_if_needed(
    struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DELAYED_INSERT_LIMIT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DELAYED_INSERT_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DELAYED_QUEUE_SIZE:
        return append_system_variable_read_warning(database, kind);
    default:
        return MYLITE_OK;
    }
}
