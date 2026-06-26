static int apply_set_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    bool handled = false;
    bool boolean_value = false;
    int rc = MYLITE_OK;

    if (target == NULL || value == NULL) {
        return MYLITE_MISUSE;
    }
    if (reject_invalid_read_only_system_variable_cell_target(database, target)) {
        return MYLITE_ERROR;
    }
    rc = apply_set_performance_schema_system_variable_cell_value(
        database,
        target,
        value,
        value_kind,
        &handled
    );
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_MODE) {
        return apply_set_sql_mode_cell_value(database, value);
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_TIME_ZONE) {
        return apply_set_time_zone_cell_value(database, value);
    }
    if (set_system_variable_is_last_insert_id(target->kind)) {
        return apply_set_last_insert_id_system_variable_cell_value(
            database,
            target,
            value,
            value_kind
        );
    }
    rc = apply_set_jl_system_variable_cell_value(database, target, value, value_kind, &handled);
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    rc = apply_set_myisam_system_variable_cell_value(database, target, value, value_kind, &handled);
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    rc = apply_set_m_session_limit_system_variable_cell_value(
        database,
        target,
        value,
        value_kind,
        &handled
    );
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    rc = apply_set_o_optimizer_system_variable_cell_value(
        database,
        target,
        value,
        value_kind,
        &handled
    );
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    rc = apply_set_innodb_core_system_variable_cell_value(
        database,
        target,
        value,
        value_kind,
        &handled
    );
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    rc = apply_set_innodb_storage_system_variable_cell_value(
        database,
        target,
        value,
        value_kind,
        &handled
    );
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    if (mylite_execution_system_variable_is_fixed_global_server_security(target->kind)) {
        return apply_set_fixed_global_server_security_cell_value(database, target);
    }
    if (mylite_execution_system_variable_is_fixed_global_authentication_password(target->kind)) {
        return apply_set_fixed_global_authentication_password_cell_value(database, target);
    }
    if (mylite_execution_system_variable_is_fixed_global_admin_listener(target->kind)) {
        return apply_set_fixed_global_admin_listener_cell_value(database, target);
    }
    if (mylite_execution_system_variable_is_fixed_global_connection_system(target->kind)) {
        return apply_set_fixed_global_connection_system_cell_value(database, target);
    }
    if (mylite_execution_system_variable_is_binary_log(target->kind)) {
        return apply_set_binary_log_system_variable_cell_value(database, target);
    }
    if (mylite_execution_system_variable_is_bootstrap_placeholder(target->kind)) {
        return apply_set_bootstrap_system_variable_cell_value(database, target);
    }
    if (mylite_execution_system_variable_is_compatibility_placeholder(target->kind)) {
        return apply_set_compatibility_system_variable_cell_value(database, target);
    }
    if (mylite_execution_system_variable_is_fixed_global_mysqlx(target->kind)) {
        return apply_set_fixed_global_mysqlx_cell_value(database, target);
    }
    if (mylite_execution_system_variable_is_fixed_global_server_tls(target->kind)) {
        return apply_set_fixed_global_server_tls_cell_value(database, target);
    }
    if (mylite_execution_system_variable_is_fixed_global_server_logging(target->kind)) {
        return apply_set_fixed_global_server_logging_cell_value(database, target);
    }
    rc = apply_set_replication_global_system_variable_cell_value(database, target, &handled);
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    if (set_system_variable_is_text_session_state(target->kind)) {
        return apply_set_text_session_system_variable_cell_value(database, target, value);
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTOCOMMIT) {
        return apply_set_autocommit_cell_value(database, target, value, value_kind);
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_SELECT_LIMIT) {
        return apply_set_sql_select_limit_cell_value(database, target, value, value_kind);
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_GROUP_CONCAT_MAX_LEN) {
        return apply_set_group_concat_max_len_cell_value(database, target, value, value_kind);
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_INFORMATION_SCHEMA_STATS_EXPIRY) {
        return apply_set_information_schema_stats_expiry_cell_value(
            database,
            target,
            value,
            value_kind
        );
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_ERROR_COUNT) {
        return apply_set_max_error_count_cell_value(database, target, value, value_kind);
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_LONG_QUERY_TIME) {
        return apply_set_long_query_time_cell_value(database, target, value, value_kind);
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SORT_BUFFER_SIZE) {
        return apply_set_sort_buffer_size_cell_value(database, target, value, value_kind);
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SLOW_LAUNCH_TIME) {
        if (target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
            set_global_variable_set_global_required_error(database, target->name);
            return MYLITE_ERROR;
        }
        set_unsupported_error(
            database,
            "SET slow_launch_time from user variables is not supported"
        );
        return MYLITE_ERROR;
    }
    if (mylite_execution_system_variable_is_timeout(target->kind)) {
        return apply_set_timeout_system_variable_cell_value(database, target, value, value_kind);
    }
    if (mylite_execution_system_variable_is_connection_memory(target->kind)) {
        return apply_set_connection_memory_system_variable_cell_value(
            database,
            target,
            value,
            value_kind
        );
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_FOREIGN_KEY_CHECKS) {
        rc = parse_set_boolean_cell_value(database, target->name, value, &boolean_value);
        if (rc == MYLITE_OK) {
            database->session.foreign_key_checks_enabled = boolean_value;
            database->session.system_variables_are_placeholder = false;
        }
        return rc;
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_REQUIRE_PRIMARY_KEY) {
        return apply_set_sql_require_primary_key_cell_value(database, target, value, value_kind);
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_BIG_TABLES) {
        return apply_set_big_tables_cell_value(database, target, value, value_kind);
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_LOW_PRIORITY_UPDATES) {
        return apply_set_low_priority_updates_cell_value(database, target, value, value_kind);
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_EXPLICIT_DEFAULTS_FOR_TIMESTAMP) {
        if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
            set_unsupported_error(
                database,
                "SET GLOBAL system variable assignment is not supported"
            );
            return MYLITE_ERROR;
        }
        return apply_set_fixed_boolean_system_variable_cell_value(database, target, value, true);
    }
    if (mylite_execution_system_variable_is_session_placeholder(target->kind)) {
        return apply_set_session_placeholder_system_variable_cell_value(
            database,
            target,
            value,
            value_kind
        );
    }
    if (fixed_global_uint64_server_identity_system_variable_value(target->kind, NULL, NULL)) {
        set_unsupported_error(
            database,
            "SET server identity system variables from user variables are not supported"
        );
        return MYLITE_ERROR;
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN_TRUST_FUNCTION_CREATORS) {
        return apply_set_log_bin_trust_function_creators_cell_value(database, target, value);
    }
    if (mylite_execution_system_variable_fixed_boolean_value(target->kind, &boolean_value)) {
        return apply_set_fixed_boolean_system_variable_cell_value(
            database,
            target,
            value,
            boolean_value
        );
    }

    set_unsupported_error(
        database,
        "SET system variable assignments from user variables are not supported for this variable"
    );
    return MYLITE_ERROR;
}

static int apply_set_log_bin_trust_function_creators_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value
) {
    bool boolean_value = false;
    int rc = parse_set_boolean_cell_value(database, target->name, value, &boolean_value);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (boolean_value) {
        set_unsupported_error(
            database,
            "SET supports only fixed no-op system variable assignments"
        );
        return MYLITE_ERROR;
    }
    return append_system_variable_read_warning(database, target->kind);
}

static int apply_set_fixed_boolean_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    bool expected_value
) {
    bool actual = false;
    int rc = parse_set_boolean_cell_value(database, target->name, value, &actual);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (actual != expected_value) {
        set_unsupported_error(
            database,
            "SET supports only fixed no-op system variable assignments"
        );
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int apply_set_performance_schema_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind,
    bool *out_handled
) {
    if (target == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    if (target->kind != MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_DIGEST_SAMPLE_AGE &&
        target->kind != MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_SHOW_PROCESSLIST) {
        return MYLITE_OK;
    }

    *out_handled = true;
    if (target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_global_variable_set_global_required_error(database, target->name);
        return MYLITE_ERROR;
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_PERFORMANCE_SCHEMA_MAX_DIGEST_SAMPLE_AGE) {
        return validate_set_performance_schema_fixed_uint64_cell_value(
            database,
            target,
            performance_schema_max_digest_sample_age_default_value,
            value,
            value_kind
        );
    }
    return apply_set_fixed_boolean_system_variable_cell_value(database, target, value, false);
}

static int validate_set_performance_schema_fixed_uint64_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    uint64_t expected_value,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    const char *text = value == NULL ? NULL : value->value;
    struct mylite_sql_source_span value_span = {0};
    size_t text_size = 0U;

    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (value == NULL || text == NULL ||
        (value_kind != MYLITE_SESSION_USER_VARIABLE_VALUE_INTEGER &&
         value_kind != MYLITE_SESSION_USER_VARIABLE_VALUE_DECIMAL)) {
        return set_incorrect_system_variable_argument_type_error(database, target->name);
    }

    text_size = value->has_value_size ? value->value_size : strlen(text);
    value_span = (struct mylite_sql_source_span){.text = text, .length = text_size};
    if (text_size == 0U || text[0] == '-' || text[0] == '+' ||
        !fixed_decimal_text_matches_uint64(value_span, expected_value)) {
        set_unsupported_error(
            database,
            "SET supports only fixed no-op system variable assignments"
        );
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int apply_set_fixed_global_server_security_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target
) {
    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_global_variable_set_global_required_error(database, target->name);
        return MYLITE_ERROR;
    }
    set_unsupported_error(
        database,
        "SET server security system variables from user variables are not supported"
    );
    return MYLITE_ERROR;
}

static int apply_set_fixed_global_authentication_password_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target
) {
    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_global_variable_set_global_required_error(database, target->name);
        return MYLITE_ERROR;
    }
    set_unsupported_error(
        database,
        "SET authentication/password system variables from user variables are not supported"
    );
    return MYLITE_ERROR;
}

static int apply_set_fixed_global_admin_listener_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target
) {
    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_global_variable_set_global_required_error(database, target->name);
        return MYLITE_ERROR;
    }
    set_unsupported_error(
        database,
        "SET admin listener system variables from user variables are not supported"
    );
    return MYLITE_ERROR;
}

static int apply_set_fixed_global_connection_system_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target
) {
    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_global_variable_set_global_required_error(database, target->name);
        return MYLITE_ERROR;
    }
    set_unsupported_error(
        database,
        "SET connection system variables from user variables are not supported"
    );
    return MYLITE_ERROR;
}

static int apply_set_fixed_global_mysqlx_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target
) {
    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_global_variable_set_global_required_error(database, target->name);
        return MYLITE_ERROR;
    }
    set_unsupported_error(
        database,
        "SET X Plugin system variables from user variables are not supported"
    );
    return MYLITE_ERROR;
}

static int apply_set_fixed_global_server_tls_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target
) {
    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_global_variable_set_global_required_error(database, target->name);
        return MYLITE_ERROR;
    }
    set_unsupported_error(
        database,
        "SET server TLS system variables from user variables are not supported"
    );
    return MYLITE_ERROR;
}

static int apply_set_fixed_global_server_logging_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target
) {
    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_global_variable_set_global_required_error(database, target->name);
        return MYLITE_ERROR;
    }
    set_unsupported_error(
        database,
        "SET server logging system variables from user variables are not supported"
    );
    return MYLITE_ERROR;
}

static int apply_set_sql_mode_cell_value(
    struct mylite_db *database,
    const struct session_scalar_cell *value
) {
    uint64_t modes = 0U;
    int rc = MYLITE_OK;

    if (value == NULL) {
        return MYLITE_MISUSE;
    }
    if (value->value == NULL) {
        set_unsupported_error(database, "SET sql_mode from NULL user variables is not supported");
        return MYLITE_ERROR;
    }
    rc = parse_sql_mode_text(database, value->value, &modes);
    if (rc == MYLITE_OK) {
        rc = append_set_sql_mode_warnings(database, modes);
    }
    if (rc == MYLITE_OK) {
        rc = set_session_sql_mode(database, modes);
    }
    return rc;
}

static int apply_set_time_zone_cell_value(
    struct mylite_db *database,
    const struct session_scalar_cell *value
) {
    if (value == NULL) {
        return MYLITE_MISUSE;
    }
    if (value->value == NULL) {
        set_system_variable_cant_be_set_value_error(database, "time_zone", "NULL");
        return MYLITE_ERROR;
    }
    return set_session_time_zone(database, value->value);
}

static bool reject_invalid_read_only_system_variable_cell_target(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target
) {
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_READ_ONLY) {
        set_read_only_system_variable_error(database, target->name);
        return true;
    }
    if (mylite_execution_system_variable_is_read_only_server_environment(target->kind)) {
        set_read_only_system_variable_error(database, target->name);
        return true;
    }
    if (mylite_execution_system_variable_is_read_only_server_build(target->kind)) {
        set_read_only_system_variable_error(database, target->name);
        return true;
    }
    if (mylite_execution_system_variable_is_read_only_server_identity_binary_log(target->kind)) {
        set_read_only_system_variable_error(database, target->name);
        return true;
    }
    if (mylite_execution_system_variable_is_read_only_authentication_password(target->kind)) {
        set_read_only_system_variable_error(database, target->name);
        return true;
    }
    if (mylite_execution_system_variable_is_read_only_server_capability(target->kind)) {
        set_read_only_system_variable_error(database, target->name);
        return true;
    }
    if (mylite_execution_system_variable_is_read_only_admin_listener(target->kind)) {
        set_read_only_system_variable_error(database, target->name);
        return true;
    }
    if (mylite_execution_system_variable_is_read_only_connection_listener(target->kind)) {
        set_read_only_system_variable_error(database, target->name);
        return true;
    }
    if (mylite_execution_system_variable_is_read_only_binary_log(target->kind)) {
        set_read_only_system_variable_error(database, target->name);
        return true;
    }
    if (mylite_execution_system_variable_is_read_only_mysqlx(target->kind)) {
        set_read_only_system_variable_error(database, target->name);
        return true;
    }
    if (mylite_execution_system_variable_is_read_only_myisam(target->kind)) {
        set_read_only_system_variable_error(database, target->name);
        return true;
    }
    if (mylite_execution_system_variable_is_read_only_innodb_core(target->kind)) {
        set_read_only_system_variable_error(database, target->name);
        return true;
    }
    if (mylite_execution_system_variable_is_read_only_innodb_storage(target->kind)) {
        set_read_only_system_variable_error(database, target->name);
        return true;
    }
    if (mylite_execution_system_variable_is_read_only_performance_schema(target->kind)) {
        set_read_only_system_variable_error(database, target->name);
        return true;
    }
    if (mylite_execution_system_variable_is_read_only_replication_global(target->kind)) {
        set_read_only_system_variable_error(database, target->name);
        return true;
    }
    if ((fixed_global_uint64_server_identity_system_variable_value(target->kind, NULL, NULL) ||
         target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN_TRUST_FUNCTION_CREATORS) &&
        target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_global_variable_set_global_required_error(database, target->name);
        return true;
    }
    if (mylite_execution_system_variable_is_global_read_only_toggle(target->kind) &&
        target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_global_variable_set_global_required_error(database, target->name);
        return true;
    }
    if (mylite_execution_system_variable_is_fixed_global_server_security(target->kind) &&
        target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_global_variable_set_global_required_error(database, target->name);
        return true;
    }
    if (mylite_execution_system_variable_is_fixed_global_authentication_password(target->kind) &&
        target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_global_variable_set_global_required_error(database, target->name);
        return true;
    }
    if (mylite_execution_system_variable_is_fixed_global_admin_listener(target->kind) &&
        target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_global_variable_set_global_required_error(database, target->name);
        return true;
    }
    if (mylite_execution_system_variable_is_fixed_global_mysqlx(target->kind) &&
        target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_global_variable_set_global_required_error(database, target->name);
        return true;
    }
    if (mylite_execution_system_variable_is_fixed_global_server_tls(target->kind) &&
        target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_global_variable_set_global_required_error(database, target->name);
        return true;
    }
    if (mylite_execution_system_variable_is_replication_global(target->kind) &&
        !mylite_execution_system_variable_is_read_only_replication_global(target->kind) &&
        target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_global_variable_set_global_required_error(database, target->name);
        return true;
    }
    if (mylite_execution_system_variable_is_bootstrap_placeholder(target->kind) &&
        !mylite_execution_system_variable_allows_session_scope(target->kind) &&
        target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_global_variable_set_global_required_error(database, target->name);
        return true;
    }
    if (mylite_execution_system_variable_is_compatibility_placeholder(target->kind) &&
        !mylite_execution_system_variable_allows_session_scope(target->kind) &&
        target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_global_variable_set_global_required_error(database, target->name);
        return true;
    }

    return false;
}

static int apply_set_sql_select_limit_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    uint64_t sql_select_limit = UINT64_MAX;
    int rc = MYLITE_OK;

    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_unsupported_error(database, "SET GLOBAL sql_select_limit assignment is not supported");
        return MYLITE_ERROR;
    }

    rc = parse_set_sql_select_limit_cell_value(
        database,
        target->name,
        value,
        value_kind,
        &sql_select_limit
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    database->session.sql_select_limit = sql_select_limit;
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static int apply_set_timeout_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    uint64_t timeout = timeout_system_variable_default_value_for_kind(
        target == NULL ? MYLITE_EXECUTION_SYSTEM_VARIABLE_NONE : target->kind
    );
    int rc = MYLITE_OK;

    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL &&
        !mylite_execution_system_variable_allows_session_scope(target->kind)) {
        set_global_variable_set_global_required_error(database, target->name);
        return MYLITE_ERROR;
    }
    rc =
        parse_set_timeout_system_variable_cell_value(database, target, value, value_kind, &timeout);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        if (timeout == timeout_system_variable_default_value_for_kind(target->kind)) {
            return MYLITE_OK;
        }
        set_unsupported_error(
            database,
            "SET timeout system variable supports only fixed no-op global assignments"
        );
        return MYLITE_ERROR;
    }
    return apply_timeout_system_variable_value(database, target, timeout);
}

static int parse_set_boolean_cell_value(
    struct mylite_db *database,
    const char *variable_name,
    const struct session_scalar_cell *value,
    bool *out_value
) {
    if (out_value == NULL || value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = false;
    if (value->value == NULL) {
        set_system_variable_cant_be_set_value_error(database, variable_name, "NULL");
        return MYLITE_ERROR;
    }
    if (strcmp(value->value, "1") == 0 || text_equals_ascii_case_insensitive(value->value, "ON") ||
        text_equals_ascii_case_insensitive(value->value, "TRUE")) {
        *out_value = true;
        return MYLITE_OK;
    }
    if (strcmp(value->value, "0") == 0 || text_equals_ascii_case_insensitive(value->value, "OFF") ||
        text_equals_ascii_case_insensitive(value->value, "FALSE")) {
        *out_value = false;
        return MYLITE_OK;
    }

    set_system_variable_cant_be_set_value_error(database, variable_name, value->value);
    return MYLITE_ERROR;
}

static int resolve_set_system_variable_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *target,
    struct resolved_set_system_variable_target *out_target
) {
    const struct mylite_sql_ast_node *first = NULL;
    const struct mylite_sql_ast_node *second = NULL;
    size_t child_count = 0U;

    if (out_target == NULL) {
        set_runtime_error(database, "invalid SET system variable target");
        return MYLITE_ERROR;
    }
    *out_target = (struct resolved_set_system_variable_target){0};
    if (target == NULL || target->kind != MYLITE_SQL_AST_SET_SYSTEM_VARIABLE_TARGET) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    child_count = mylite_sql_ast_node_child_count(target);
    first = child_at(target, 0U);
    second = child_at(target, 1U);
    if (child_count == 1U && first != NULL && first->kind == MYLITE_SQL_AST_SYSTEM_VARIABLE) {
        return resolve_set_system_variable_system_target(database, first, out_target);
    }
    if (child_count == 1U) {
        return resolve_set_system_variable_identifier_target(database, NULL, first, out_target);
    }
    if (child_count == 2U) {
        return resolve_set_system_variable_identifier_target(database, first, second, out_target);
    }

    set_parse_error(database, NULL);
    return MYLITE_ERROR;
}

static int resolve_set_system_variable_identifier_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *scope_node,
    const struct mylite_sql_ast_node *name_node,
    struct resolved_set_system_variable_target *out_target
) {
    struct system_variable_component name = {0};
    char scope[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = MYLITE_OK;

    if (name_node == NULL || name_node->kind != MYLITE_SQL_AST_IDENTIFIER) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (scope_node == NULL) {
        out_target->scope = SET_SYSTEM_VARIABLE_SCOPE_NONE;
    } else {
        rc = copy_identifier_text(scope_node, scope, sizeof(scope), database);
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (text_equals_ascii_case_insensitive(scope, "session")) {
            out_target->scope = SET_SYSTEM_VARIABLE_SCOPE_SESSION;
        } else if (text_equals_ascii_case_insensitive(scope, "local")) {
            out_target->scope = SET_SYSTEM_VARIABLE_SCOPE_LOCAL;
        } else if (text_equals_ascii_case_insensitive(scope, "global")) {
            out_target->scope = SET_SYSTEM_VARIABLE_SCOPE_GLOBAL;
        } else {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
    }

    rc = copy_identifier_text(name_node, out_target->name, sizeof(out_target->name), database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    memcpy(name.text, out_target->name, strlen(out_target->name) + 1U);
    if (!resolve_system_variable_kind(&name, &out_target->kind)) {
        set_unknown_system_variable_name_error(database, out_target->name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int resolve_set_system_variable_system_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *name_node,
    struct resolved_set_system_variable_target *out_target
) {
    const struct mylite_sql_source_span *span = name_node == NULL ? NULL : &name_node->span;
    struct system_variable_component first = {0};
    struct system_variable_component second = {0};
    const struct system_variable_component *name = &first;
    size_t offset = 2U;
    bool has_scope = false;
    int rc = MYLITE_OK;

    if (span == NULL || span->text == NULL || span->length < 3U || span->text[0] != '@' ||
        span->text[1] != '@') {
        set_unknown_system_variable_error(database, name_node);
        return MYLITE_ERROR;
    }

    rc = parse_system_variable_component(database, span, &offset, &first);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (offset < span->length && span->text[offset] == '.') {
        has_scope = true;
        ++offset;
        if (first.quoted) {
            set_unsupported_error(database, "unsupported quoted system variable scope");
            return MYLITE_ERROR;
        }
        rc = parse_system_variable_component(database, span, &offset, &second);
        if (rc != MYLITE_OK) {
            return rc;
        }
        name = &second;
    }
    if (offset != span->length || system_variable_component_is_empty(name)) {
        set_unknown_system_variable_error(database, name_node);
        return MYLITE_ERROR;
    }

    if (!resolve_system_variable_kind(name, &out_target->kind)) {
        set_unknown_system_variable_error(database, name_node);
        return MYLITE_ERROR;
    }
    memcpy(out_target->name, name->text, strlen(name->text) + 1U);

    if (!has_scope) {
        out_target->is_system_variable_target = true;
        out_target->scope = SET_SYSTEM_VARIABLE_SCOPE_NONE;
        return MYLITE_OK;
    }
    if (system_variable_component_equals(&first, "global")) {
        out_target->is_system_variable_target = true;
        out_target->scope = SET_SYSTEM_VARIABLE_SCOPE_GLOBAL;
        return MYLITE_OK;
    }
    if (system_variable_component_equals(&first, "session")) {
        out_target->is_system_variable_target = true;
        out_target->scope = SET_SYSTEM_VARIABLE_SCOPE_SESSION;
    } else if (system_variable_component_equals(&first, "local")) {
        out_target->is_system_variable_target = true;
        out_target->scope = SET_SYSTEM_VARIABLE_SCOPE_LOCAL;
    } else {
        set_unknown_system_variable_error(database, name_node);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static bool fixed_global_uint64_server_identity_system_variable_value(
    enum mylite_execution_system_variable_kind kind,
    uint64_t *out_value,
    const char **out_unsupported_message
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SERVER_ID:
        if (out_value != NULL) {
            *out_value = server_id_system_variable_value;
        }
        if (out_unsupported_message != NULL) {
            *out_unsupported_message = "SET server_id supports only fixed no-op global assignments";
        }
        return true;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SERVER_ID_BITS:
        if (out_value != NULL) {
            *out_value = server_id_bits_system_variable_value;
        }
        if (out_unsupported_message != NULL) {
            *out_unsupported_message =
                "SET server_id_bits supports only fixed no-op global assignments";
        }
        return true;
    default:
        return false;
    }
}

static int validate_set_fixed_boolean_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    bool expected_value
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    uint64_t magnitude = 0U;
    bool actual_value = false;

    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        return MYLITE_OK;
    }
    if (value_node->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "SET supports only fixed no-op system variable assignments"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(value_node);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        actual_value = true;
    } else if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        actual_value = false;
    } else if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
        char *decoded = NULL;
        size_t decoded_size = 0U;
        int rc = decode_sql_string_literal(
            database,
            value_node,
            "SET system variables support only string literals",
            "SET system variable string values do not support NUL bytes",
            &decoded,
            &decoded_size
        );

        if (rc != MYLITE_OK) {
            return rc;
        }
        if (text_equals_ascii_case_insensitive(decoded, "ON") ||
            text_equals_ascii_case_insensitive(decoded, "TRUE")) {
            actual_value = true;
        } else if (
            text_equals_ascii_case_insensitive(decoded, "OFF") ||
            text_equals_ascii_case_insensitive(decoded, "FALSE")
        ) {
            actual_value = false;
        } else {
            free(decoded);
            set_unsupported_error(
                database,
                "SET supports only fixed no-op system variable assignments"
            );
            return MYLITE_ERROR;
        }
        free(decoded);
    } else if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER) {
        if (parse_unsigned_integer_literal(&value_node->span, &magnitude) != MYLITE_OK ||
            magnitude > 1U) {
            set_unsupported_error(
                database,
                "SET supports only fixed no-op system variable assignments"
            );
            return MYLITE_ERROR;
        }
        actual_value = magnitude == 1U;
    } else {
        set_unsupported_error(
            database,
            "SET supports only fixed no-op system variable assignments"
        );
        return MYLITE_ERROR;
    }

    if (actual_value != expected_value) {
        set_unsupported_error(
            database,
            "SET supports only fixed no-op system variable assignments"
        );
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int validate_set_fixed_text_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const char *expected_value
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    char identifier[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char *decoded = NULL;
    size_t decoded_size = 0U;
    int rc = MYLITE_OK;

    if (expected_value == NULL) {
        set_runtime_error(database, "invalid fixed system variable value");
        return MYLITE_ERROR;
    }
    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        return MYLITE_OK;
    }
    if (value_node->kind == MYLITE_SQL_AST_IDENTIFIER) {
        rc = copy_identifier_text(value_node, identifier, sizeof(identifier), database);
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (text_equals_ascii_case_insensitive(identifier, expected_value)) {
            return MYLITE_OK;
        }
        set_unsupported_error(
            database,
            "SET supports only fixed no-op system variable assignments"
        );
        return MYLITE_ERROR;
    }
    if (value_node->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "SET supports only fixed no-op system variable assignments"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(value_node);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ||
        literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        const char *literal_text = literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ? "ON" : "OFF";

        if (text_equals_ascii_case_insensitive(literal_text, expected_value)) {
            return MYLITE_OK;
        }
        set_unsupported_error(
            database,
            "SET supports only fixed no-op system variable assignments"
        );
        return MYLITE_ERROR;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(
            database,
            "SET supports only fixed no-op system variable assignments"
        );
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        value_node,
        "SET system variables support only string literals",
        "SET system variable string values do not support NUL bytes",
        &decoded,
        &decoded_size
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!text_equals_ascii_case_insensitive(decoded, expected_value)) {
        set_unsupported_error(
            database,
            "SET supports only fixed no-op system variable assignments"
        );
        rc = MYLITE_ERROR;
    }
    free(decoded);
    return rc;
}

static int validate_set_default_only_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const char *unsupported_message
) {
    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        return MYLITE_OK;
    }
    set_unsupported_error(database, unsupported_message);
    return MYLITE_ERROR;
}

static int validate_set_fixed_null_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const char *unsupported_message
) {
    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        return MYLITE_OK;
    }
    if (value_node->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_NULL) {
        return MYLITE_OK;
    }

    set_unsupported_error(database, unsupported_message);
    return MYLITE_ERROR;
}
