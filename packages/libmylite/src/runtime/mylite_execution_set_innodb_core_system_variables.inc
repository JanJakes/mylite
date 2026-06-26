static int apply_set_innodb_core_system_variable_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
) {
    if (out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    if (target == NULL) {
        return MYLITE_MISUSE;
    }

    switch (target->kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ADAPTIVE_FLUSHING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ADAPTIVE_FLUSHING_LWM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ADAPTIVE_HASH_INDEX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ADAPTIVE_MAX_SLEEP_DELAY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_AUTOEXTEND_INCREMENT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_DUMP_AT_SHUTDOWN:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_DUMP_NOW:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_DUMP_PCT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_FILENAME:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_IN_CORE_FILE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_LOAD_ABORT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_LOAD_NOW:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CHANGE_BUFFER_MAX_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CHANGE_BUFFERING:
        *out_handled = true;
        return validate_set_innodb_core_fixed_global_value(database, target, value_node);
    default:
        return MYLITE_OK;
    }
}

static int apply_set_innodb_core_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    bool *out_handled
) {
    if (out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    if (target == NULL) {
        return MYLITE_MISUSE;
    }

    switch (target->kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ADAPTIVE_FLUSHING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ADAPTIVE_FLUSHING_LWM:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ADAPTIVE_HASH_INDEX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ADAPTIVE_MAX_SLEEP_DELAY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_AUTOEXTEND_INCREMENT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_DUMP_AT_SHUTDOWN:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_DUMP_NOW:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_DUMP_PCT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_FILENAME:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_IN_CORE_FILE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_LOAD_ABORT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_LOAD_NOW:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CHANGE_BUFFER_MAX_SIZE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CHANGE_BUFFERING:
        *out_handled = true;
        return validate_set_innodb_core_fixed_global_cell_value(
            database,
            target,
            value,
            value_kind
        );
    default:
        return MYLITE_OK;
    }
}

static int validate_set_innodb_core_fixed_global_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_global_variable_set_global_required_error(database, target->name);
        return MYLITE_ERROR;
    }

    switch (target->kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ADAPTIVE_FLUSHING:
        return validate_set_fixed_boolean_value(database, value_node, true);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ADAPTIVE_FLUSHING_LWM:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_adaptive_flushing_lwm_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ADAPTIVE_HASH_INDEX:
        return validate_set_fixed_boolean_value(database, value_node, false);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ADAPTIVE_MAX_SLEEP_DELAY:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_adaptive_max_sleep_delay_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_AUTOEXTEND_INCREMENT:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_autoextend_increment_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_DUMP_AT_SHUTDOWN:
        return validate_set_fixed_boolean_value(database, value_node, true);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_DUMP_NOW:
        return validate_set_fixed_boolean_value(database, value_node, false);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_DUMP_PCT:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_buffer_pool_dump_pct_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_FILENAME:
        return validate_set_fixed_text_value(
            database,
            value_node,
            innodb_buffer_pool_filename_default_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_IN_CORE_FILE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_LOAD_ABORT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_LOAD_NOW:
        return validate_set_fixed_boolean_value(database, value_node, false);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_SIZE:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_buffer_pool_size_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CHANGE_BUFFER_MAX_SIZE:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            innodb_change_buffer_max_size_default_value,
            "SET supports only fixed no-op system variable assignments"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CHANGE_BUFFERING:
        return validate_set_fixed_text_value(
            database,
            value_node,
            innodb_change_buffering_default_value
        );
    default:
        return MYLITE_OK;
    }
}

static int validate_set_innodb_core_fixed_global_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_global_variable_set_global_required_error(database, target->name);
        return MYLITE_ERROR;
    }

    switch (target->kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ADAPTIVE_FLUSHING:
        return apply_set_fixed_boolean_system_variable_cell_value(database, target, value, true);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ADAPTIVE_FLUSHING_LWM:
        return validate_set_innodb_core_fixed_uint64_cell_value(
            database,
            target,
            innodb_adaptive_flushing_lwm_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ADAPTIVE_HASH_INDEX:
        return apply_set_fixed_boolean_system_variable_cell_value(database, target, value, false);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_ADAPTIVE_MAX_SLEEP_DELAY:
        return validate_set_innodb_core_fixed_uint64_cell_value(
            database,
            target,
            innodb_adaptive_max_sleep_delay_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_AUTOEXTEND_INCREMENT:
        return validate_set_innodb_core_fixed_uint64_cell_value(
            database,
            target,
            innodb_autoextend_increment_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_DUMP_AT_SHUTDOWN:
        return apply_set_fixed_boolean_system_variable_cell_value(database, target, value, true);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_DUMP_NOW:
        return apply_set_fixed_boolean_system_variable_cell_value(database, target, value, false);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_DUMP_PCT:
        return validate_set_innodb_core_fixed_uint64_cell_value(
            database,
            target,
            innodb_buffer_pool_dump_pct_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_FILENAME:
        return validate_set_innodb_core_fixed_text_cell_value(
            database,
            target,
            innodb_buffer_pool_filename_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_IN_CORE_FILE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_LOAD_ABORT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_LOAD_NOW:
        return apply_set_fixed_boolean_system_variable_cell_value(database, target, value, false);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_BUFFER_POOL_SIZE:
        return validate_set_innodb_core_fixed_uint64_cell_value(
            database,
            target,
            innodb_buffer_pool_size_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CHANGE_BUFFER_MAX_SIZE:
        return validate_set_innodb_core_fixed_uint64_cell_value(
            database,
            target,
            innodb_change_buffer_max_size_default_value,
            value,
            value_kind
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_CHANGE_BUFFERING:
        return validate_set_innodb_core_fixed_text_cell_value(
            database,
            target,
            innodb_change_buffering_default_value,
            value,
            value_kind
        );
    default:
        return MYLITE_OK;
    }
}

static int validate_set_innodb_core_fixed_uint64_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    uint64_t expected_value,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    struct mylite_sql_source_span unsigned_span = {0};
    const char *text = value == NULL ? NULL : value->value;
    size_t text_size = 0U;
    uint64_t actual = 0U;

    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (value == NULL || text == NULL || value_kind != MYLITE_SESSION_USER_VARIABLE_VALUE_INTEGER) {
        return set_incorrect_system_variable_argument_type_error(database, target->name);
    }

    text_size = value->has_value_size ? value->value_size : strlen(text);
    if (text_size == 0U || text[0] == '-' || text[0] == '+') {
        set_unsupported_error(
            database,
            "SET supports only fixed no-op system variable assignments"
        );
        return MYLITE_ERROR;
    }
    unsigned_span = (struct mylite_sql_source_span){.text = text, .length = text_size};
    if (parse_unsigned_integer_literal(&unsigned_span, &actual) != MYLITE_OK ||
        actual != expected_value) {
        set_unsupported_error(
            database,
            "SET supports only fixed no-op system variable assignments"
        );
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int validate_set_innodb_core_fixed_text_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const char *expected_value,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    const char *text = value == NULL ? NULL : value->value;

    if (target == NULL || expected_value == NULL) {
        return MYLITE_MISUSE;
    }
    if (value == NULL || text == NULL || value_kind != MYLITE_SESSION_USER_VARIABLE_VALUE_STRING) {
        return set_incorrect_system_variable_argument_type_error(database, target->name);
    }
    if (!text_equals_ascii_case_insensitive(text, expected_value)) {
        set_unsupported_error(
            database,
            "SET supports only fixed no-op system variable assignments"
        );
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}
