static int apply_set_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
) {
    struct set_session_snapshot snapshot = {0};
    const struct mylite_sql_ast_node *assignment_list = NULL;
    const struct mylite_sql_ast_node *assignment = NULL;
    int rc = MYLITE_OK;

    if (statement == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (statement->kind == MYLITE_SQL_AST_SET_SYSTEM_VARIABLE_STATEMENT) {
        return apply_set_system_variable_assignment(database, statement);
    }
    if (statement->kind != MYLITE_SQL_AST_SET_STATEMENT) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    rc = copy_set_session_snapshot(database, &snapshot);
    if (rc != MYLITE_OK) {
        return rc;
    }

    assignment_list = child_at(statement, 0U);
    if (assignment_list == NULL || assignment_list->kind != MYLITE_SQL_AST_SET_ASSIGNMENT_LIST) {
        set_parse_error(database, NULL);
        restore_set_session_snapshot(database, &snapshot);
        return MYLITE_ERROR;
    }
    assignment = child_at(assignment_list, 0U);
    while (rc == MYLITE_OK && assignment != NULL) {
        rc = apply_set_assignment(database, assignment);
        assignment = assignment->next_sibling;
    }
    if (rc != MYLITE_OK) {
        restore_set_session_snapshot(database, &snapshot);
        return rc;
    }

    deinit_set_session_snapshot(&snapshot);
    return MYLITE_OK;
}

static int apply_set_assignment(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment
) {
    const struct mylite_sql_ast_node *target = NULL;

    if (assignment == NULL || assignment->kind != MYLITE_SQL_AST_SET_ASSIGNMENT) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    target = child_at(assignment, 0U);
    if (target != NULL && target->kind == MYLITE_SQL_AST_SET_SYSTEM_VARIABLE_TARGET) {
        return apply_set_system_variable_assignment(database, assignment);
    }
    if (target != NULL && target->kind == MYLITE_SQL_AST_USER_VARIABLE) {
        return apply_set_user_variable_assignment(database, assignment);
    }

    set_parse_error(database, NULL);
    return MYLITE_ERROR;
}

static int apply_set_system_variable_assignment(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment
) {
    struct resolved_set_system_variable_target target = {0};
    const struct mylite_sql_ast_node *target_node = NULL;
    const struct mylite_sql_ast_node *value_node = NULL;
    bool expected_boolean_value = false;
    bool handled = false;
    int rc = MYLITE_OK;

    if (assignment == NULL || (assignment->kind != MYLITE_SQL_AST_SET_ASSIGNMENT &&
                               assignment->kind != MYLITE_SQL_AST_SET_SYSTEM_VARIABLE_STATEMENT)) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    target_node = child_at(assignment, 0U);
    value_node = child_at(assignment, 1U);
    rc = resolve_set_system_variable_target(database, target_node, &target);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc =
        apply_set_system_variable_user_variable_assignment(database, &target, value_node, &handled);
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    if (set_system_variable_target_is_read_only(target.kind)) {
        set_read_only_system_variable_error(database, target.name);
        return MYLITE_ERROR;
    }
    rc = apply_set_performance_schema_system_variable_assignment(
        database,
        &target,
        value_node,
        &handled
    );
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    if (set_system_variable_is_last_insert_id(target.kind)) {
        return apply_set_last_insert_id_system_variable_value(database, &target, value_node);
    }
    rc =
        apply_set_remaining_system_variable_assignment(database, &target, value_node, &handled);
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    rc = apply_set_jl_system_variable_assignment(database, &target, value_node, &handled);
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    rc = apply_set_myisam_system_variable_assignment(database, &target, value_node, &handled);
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    rc = apply_set_m_session_limit_system_variable_assignment(
        database,
        &target,
        value_node,
        &handled
    );
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    rc = apply_set_o_optimizer_system_variable_assignment(database, &target, value_node, &handled);
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    rc = apply_set_innodb_core_system_variable_assignment(database, &target, value_node, &handled);
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    rc = apply_set_innodb_storage_system_variable_assignment(
        database,
        &target,
        value_node,
        &handled
    );
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    rc = apply_set_server_identity_binary_log_system_variable_assignment(
        database,
        &target,
        value_node,
        &handled
    );
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    rc = apply_set_global_read_only_toggle_assignment(database, &target, value_node, &handled);
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    rc = apply_set_global_server_security_assignment(database, &target, value_node, &handled);
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    rc = apply_set_global_server_tls_assignment(database, &target, value_node, &handled);
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    rc = apply_set_global_server_logging_assignment(database, &target, value_node, &handled);
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    rc = apply_set_replication_global_system_variable_assignment(
        database,
        &target,
        value_node,
        &handled
    );
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    rc = apply_set_global_admin_listener_assignment(database, &target, value_node, &handled);
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    rc = apply_set_global_connection_system_assignment(database, &target, value_node, &handled);
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    rc = apply_set_binary_log_system_variable_assignment(database, &target, value_node, &handled);
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    rc = apply_set_bootstrap_system_variable_assignment(database, &target, value_node, &handled);
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    rc =
        apply_set_compatibility_system_variable_assignment(database, &target, value_node, &handled);
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    rc = apply_set_global_mysqlx_assignment(database, &target, value_node, &handled);
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    rc = apply_set_global_authentication_password_assignment(
        database,
        &target,
        value_node,
        &handled
    );
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    rc = apply_set_internal_session_system_variable_assignment(
        database,
        &target,
        value_node,
        &handled
    );
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    rc = apply_set_resource_tuning_system_variable_assignment(
        database,
        &target,
        value_node,
        &handled
    );
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    rc = apply_set_default_engine_system_variable_assignment(
        database,
        &target,
        value_node,
        &handled
    );
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    rc = apply_set_global_session_placeholder_assignment(database, &target, value_node, &handled);
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    if (target.kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_TRANSACTION_ISOLATION ||
        target.kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_TRANSACTION_READ_ONLY) {
        return apply_set_transaction_system_variable_value(database, &target, value_node);
    }
    if (set_system_variable_is_text_session_state(target.kind)) {
        return apply_set_text_session_system_variable_value(database, &target, value_node);
    }
    if (target.kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_LONG_QUERY_TIME) {
        return apply_set_long_query_time_value(database, &target, value_node);
    }
    rc = apply_set_integer_system_variable_value(database, &target, value_node, &handled);
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    if (target.kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_FOREIGN_KEY_CHECKS) {
        return apply_set_foreign_key_checks_value(database, &target, value_node);
    }
    if (target.kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_REQUIRE_PRIMARY_KEY) {
        return apply_set_sql_require_primary_key_value(database, &target, value_node);
    }
    if (target.kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_BIG_TABLES) {
        return apply_set_big_tables_value(database, &target, value_node);
    }
    if (target.kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_LOW_PRIORITY_UPDATES) {
        return apply_set_low_priority_updates_value(database, &target, value_node);
    }
    if (target.kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTOCOMMIT) {
        return apply_set_autocommit_value(database, &target, value_node);
    }
    if (target.scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_unsupported_error(database, "SET GLOBAL system variable assignment is not supported");
        return MYLITE_ERROR;
    }
    if (target.kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_EXPLICIT_DEFAULTS_FOR_TIMESTAMP) {
        return validate_set_fixed_boolean_value(database, value_node, true);
    }
    if (mylite_execution_system_variable_is_session_placeholder(target.kind)) {
        return apply_set_session_placeholder_system_variable_value(database, &target, value_node);
    }
    if (mylite_execution_system_variable_fixed_boolean_value(
            target.kind,
            &expected_boolean_value
        )) {
        return validate_set_fixed_boolean_value(database, value_node, expected_boolean_value);
    }
    if (target.kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_MODE) {
        return apply_set_sql_mode_value(database, value_node);
    }
    if (target.kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_TIMESTAMP) {
        return apply_set_timestamp_value(database, value_node);
    }
    if (target.kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_TIME_ZONE) {
        return apply_set_time_zone_value(database, value_node);
    }

    set_read_only_system_variable_error(database, target.name);
    return MYLITE_ERROR;
}

static bool set_system_variable_target_is_read_only(
    enum mylite_execution_system_variable_kind kind
) {
    return kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_LOWER_CASE_FILE_SYSTEM ||
           kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_LOWER_CASE_TABLE_NAMES ||
           kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_INNODB_READ_ONLY ||
           kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_PROXY_USER ||
           mylite_execution_system_variable_is_read_only_server_build(kind) ||
           mylite_execution_system_variable_is_read_only_server_environment(kind) ||
           mylite_execution_system_variable_is_read_only_authentication_password(kind) ||
           mylite_execution_system_variable_is_read_only_server_capability(kind) ||
           mylite_execution_system_variable_is_read_only_admin_listener(kind) ||
           mylite_execution_system_variable_is_read_only_connection_listener(kind) ||
           mylite_execution_system_variable_is_read_only_binary_log(kind) ||
           mylite_execution_system_variable_is_read_only_mysqlx(kind) ||
           mylite_execution_system_variable_is_read_only_myisam(kind) ||
           mylite_execution_system_variable_is_read_only_innodb_core(kind) ||
           mylite_execution_system_variable_is_read_only_innodb_storage(kind) ||
           mylite_execution_system_variable_is_read_only_performance_schema(kind) ||
           mylite_execution_system_variable_is_read_only_server_logging(kind) ||
           mylite_execution_system_variable_is_read_only_replication_global(kind);
}

static int apply_set_performance_schema_system_variable_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
) {
    const char *unsupported_message = "SET supports only fixed no-op system variable assignments";

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
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            performance_schema_max_digest_sample_age_default_value,
            unsupported_message
        );
    }
    return validate_set_fixed_boolean_value(database, value_node, false);
}

static int apply_set_server_identity_binary_log_system_variable_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
) {
    uint64_t expected_value = 0U;
    const char *unsupported_message = NULL;
    int rc = MYLITE_OK;

    *out_handled = false;
    if (mylite_execution_system_variable_is_read_only_server_identity_binary_log(target->kind)) {
        *out_handled = true;
        set_read_only_system_variable_error(database, target->name);
        return MYLITE_ERROR;
    }
    if (fixed_global_uint64_server_identity_system_variable_value(
            target->kind,
            &expected_value,
            &unsupported_message
        )) {
        *out_handled = true;
        if (target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
            set_global_variable_set_global_required_error(database, target->name);
            return MYLITE_ERROR;
        }
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            expected_value,
            unsupported_message
        );
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_BIN_TRUST_FUNCTION_CREATORS) {
        *out_handled = true;
        if (target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
            set_global_variable_set_global_required_error(database, target->name);
            return MYLITE_ERROR;
        }
        rc = validate_set_fixed_boolean_value(database, value_node, false);
        if (rc != MYLITE_OK) {
            return rc;
        }
        return append_system_variable_read_warning(database, target->kind);
    }

    return MYLITE_OK;
}

static int apply_set_system_variable_user_variable_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
) {
    const struct mylite_sql_ast_node *unwrapped = unwrap_parenthesized_expression(value_node);
    struct session_scalar_cell value = {0};
    enum mylite_session_user_variable_value_kind value_kind =
        MYLITE_SESSION_USER_VARIABLE_VALUE_NULL;
    int rc = MYLITE_OK;

    if (target == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    if (unwrapped == NULL || unwrapped->kind != MYLITE_SQL_AST_USER_VARIABLE) {
        return MYLITE_OK;
    }
    *out_handled = true;
    rc = session_user_variable_value(database, unwrapped, &value);
    if (rc == MYLITE_OK) {
        rc = session_user_variable_value_kind(database, unwrapped, &value_kind);
    }
    if (rc == MYLITE_OK) {
        rc = apply_set_system_variable_cell_value(database, target, &value, value_kind);
    }
    session_scalar_cell_deinit(&value);
    return rc;
}

static int apply_set_global_server_tls_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
) {
    const char *unsupported_message = "SET supports only fixed no-op system variable assignments";

    if (target == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    if (!mylite_execution_system_variable_is_fixed_global_server_tls(target->kind)) {
        return MYLITE_OK;
    }

    *out_handled = true;
    if (target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_global_variable_set_global_required_error(database, target->name);
        return MYLITE_ERROR;
    }
    return validate_set_fixed_null_value(database, value_node, unsupported_message);
}

static int apply_set_global_server_logging_assignment(
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
    if (!mylite_execution_system_variable_is_fixed_global_server_logging(target->kind)) {
        return MYLITE_OK;
    }

    *out_handled = true;
    if (target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_global_variable_set_global_required_error(database, target->name);
        return MYLITE_ERROR;
    }

    switch (target->kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GENERAL_LOG:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_QUERIES_NOT_USING_INDEXES:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_RAW:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_SLOW_ADMIN_STATEMENTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_SLOW_EXTRA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_SLOW_REPLICA_STATEMENTS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLOW_QUERY_LOG:
        return validate_set_fixed_boolean_value(database, value_node, false);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_SLOW_SLAVE_STATEMENTS:
        rc = validate_set_fixed_boolean_value(database, value_node, false);
        if (rc == MYLITE_OK) {
            rc = append_system_variable_read_warning(database, target->kind);
        }
        return rc;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_STATEMENTS_UNSAFE_FOR_BINLOG:
        rc = validate_set_fixed_boolean_value(database, value_node, true);
        if (rc == MYLITE_OK) {
            rc = append_system_variable_read_warning(database, target->kind);
        }
        return rc;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GENERAL_LOG_FILE:
        return validate_set_fixed_text_value(
            database,
            value_node,
            general_log_file_system_variable_value
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_ERROR_SERVICES:
        return validate_set_fixed_text_value(
            database,
            value_node,
            "log_filter_internal; log_sink_internal"
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_ERROR_SUPPRESSION_LIST:
        return validate_set_fixed_text_value(database, value_node, "");
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_ERROR_VERBOSITY:
        return validate_set_fixed_uint64_value(database, value_node, 2U, unsupported_message);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_OUTPUT:
        return validate_set_fixed_text_value(database, value_node, "FILE");
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_THROTTLE_QUERIES_NOT_USING_INDEXES:
        return validate_set_fixed_uint64_value(database, value_node, 0U, unsupported_message);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_LOG_TIMESTAMPS:
        return validate_set_fixed_text_value(database, value_node, "UTC");
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SLOW_QUERY_LOG_FILE:
        return validate_set_fixed_text_value(
            database,
            value_node,
            slow_query_log_file_system_variable_value
        );
    default:
        set_runtime_error(database, "invalid fixed server logging system variable");
        return MYLITE_ERROR;
    }
}

static int apply_set_integer_system_variable_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
) {
    if (target == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = true;
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_ALLOWED_PACKET) {
        return apply_set_max_allowed_packet_value(database, target, value_node);
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTO_INCREMENT_INCREMENT ||
        target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTO_INCREMENT_OFFSET) {
        return apply_set_auto_increment_step_value(database, target, value_node);
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_SELECT_LIMIT) {
        return apply_set_sql_select_limit_value(database, target, value_node);
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_GROUP_CONCAT_MAX_LEN) {
        return apply_set_group_concat_max_len_value(database, target, value_node);
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_INFORMATION_SCHEMA_STATS_EXPIRY) {
        return apply_set_information_schema_stats_expiry_value(database, target, value_node);
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_MAX_ERROR_COUNT) {
        return apply_set_max_error_count_value(database, target, value_node);
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SORT_BUFFER_SIZE) {
        return apply_set_sort_buffer_size_value(database, target, value_node);
    }
    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_SLOW_LAUNCH_TIME) {
        if (target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
            set_global_variable_set_global_required_error(database, target->name);
            return MYLITE_ERROR;
        }
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            slow_launch_time_default_value,
            "SET GLOBAL slow_launch_time assignment is not supported"
        );
    }
    if (mylite_execution_system_variable_is_timeout(target->kind)) {
        return apply_set_timeout_system_variable_value(database, target, value_node);
    }
    if (mylite_execution_system_variable_is_connection_memory(target->kind)) {
        return apply_set_connection_memory_system_variable_value(database, target, value_node);
    }

    *out_handled = false;
    return MYLITE_OK;
}

static int apply_set_user_variable_assignment(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment
) {
    const struct mylite_sql_ast_node *target = NULL;
    const struct mylite_sql_ast_node *value_node = NULL;
    struct session_scalar_cell value = {0};
    enum mylite_session_user_variable_value_kind value_kind =
        MYLITE_SESSION_USER_VARIABLE_VALUE_NULL;
    int rc = MYLITE_OK;

    if (assignment == NULL || assignment->kind != MYLITE_SQL_AST_SET_ASSIGNMENT) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    target = child_at(assignment, 0U);
    value_node = child_at(assignment, 1U);
    rc = evaluate_user_variable_assignment_value(database, value_node, &value, &value_kind);
    if (rc == MYLITE_OK) {
        rc = set_session_user_variable(database, target, &value, value_kind);
    }
    session_scalar_cell_deinit(&value);
    return rc;
}

static int evaluate_user_variable_assignment_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    struct session_scalar_cell *out_cell,
    enum mylite_session_user_variable_value_kind *out_value_kind
) {
    const struct mylite_sql_ast_node *unwrapped = unwrap_parenthesized_expression(value_node);
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    size_t decoded_length = 0U;
    int rc = MYLITE_OK;

    if (out_cell == NULL || out_value_kind == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    *out_value_kind = MYLITE_SESSION_USER_VARIABLE_VALUE_NULL;
    if (unwrapped == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (unwrapped->kind == MYLITE_SQL_AST_LITERAL) {
        literal_kind = mylite_sql_ast_node_literal_kind(unwrapped);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
            rc = decode_sql_string_literal(
                database,
                unwrapped,
                "SET user variables support only string literals",
                "SET user variable string values do not support NUL bytes",
                &out_cell->owned_text,
                &decoded_length
            );
            if (rc == MYLITE_OK) {
                out_cell->value = out_cell->owned_text;
                out_cell->value_size = decoded_length;
                out_cell->has_value_size = true;
                *out_value_kind = MYLITE_SESSION_USER_VARIABLE_VALUE_STRING;
            }
            return rc;
        }
        if (literal_kind == MYLITE_SQL_AST_LITERAL_DECIMAL ||
            literal_kind == MYLITE_SQL_AST_LITERAL_FLOAT) {
            return copy_user_variable_source_text_value(
                database,
                unwrapped,
                literal_kind == MYLITE_SQL_AST_LITERAL_DECIMAL
                    ? MYLITE_SESSION_USER_VARIABLE_VALUE_DECIMAL
                    : MYLITE_SESSION_USER_VARIABLE_VALUE_STRING,
                out_cell,
                out_value_kind
            );
        }
    }
    if (unwrapped->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        const struct mylite_sql_ast_node *unary_value = child_at(unwrapped, 0U);

        if (unary_value != NULL && unary_value->kind == MYLITE_SQL_AST_LITERAL) {
            literal_kind = mylite_sql_ast_node_literal_kind(unary_value);
        }
        if (literal_kind == MYLITE_SQL_AST_LITERAL_DECIMAL ||
            literal_kind == MYLITE_SQL_AST_LITERAL_FLOAT) {
            return copy_user_variable_source_text_value(
                database,
                mylite_sql_ast_node_operator(unwrapped) == MYLITE_SQL_AST_OPERATOR_POSITIVE
                    ? unary_value
                    : unwrapped,
                literal_kind == MYLITE_SQL_AST_LITERAL_DECIMAL
                    ? MYLITE_SESSION_USER_VARIABLE_VALUE_DECIMAL
                    : MYLITE_SESSION_USER_VARIABLE_VALUE_STRING,
                out_cell,
                out_value_kind
            );
        }
    }

    switch (unwrapped->kind) {
    case MYLITE_SQL_AST_LITERAL:
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_SYSTEM_VARIABLE:
    case MYLITE_SQL_AST_USER_VARIABLE:
    case MYLITE_SQL_AST_CAST_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_CAST_CHAR_EXPRESSION:
    case MYLITE_SQL_AST_CAST_SIGNED_EXPRESSION:
    case MYLITE_SQL_AST_CAST_UNSIGNED_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_BINARY_TYPE_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_CHAR_TYPE_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_SIGNED_TYPE_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_UNSIGNED_TYPE_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_USING_BINARY_EXPRESSION:
    case MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION:
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP_VALUE:
    case MYLITE_SQL_AST_CURRENT_DATE_VALUE:
    case MYLITE_SQL_AST_CURRENT_TIME_VALUE:
    case MYLITE_SQL_AST_UTC_DATE_VALUE:
    case MYLITE_SQL_AST_UTC_TIME_VALUE:
    case MYLITE_SQL_AST_UTC_TIMESTAMP_VALUE:
    case MYLITE_SQL_AST_SYSDATE_FUNCTION:
    case MYLITE_SQL_AST_CONNECTION_ID_FUNCTION:
    case MYLITE_SQL_AST_ROW_COUNT_FUNCTION:
    case MYLITE_SQL_AST_FOUND_ROWS_FUNCTION:
    case MYLITE_SQL_AST_LAST_INSERT_ID_FUNCTION:
    case MYLITE_SQL_AST_LAST_INSERT_ID_SET_FUNCTION:
    case MYLITE_SQL_AST_UUID_FUNCTION:
    case MYLITE_SQL_AST_CONCAT_FUNCTION:
    case MYLITE_SQL_AST_CONCAT_WS_FUNCTION:
    case MYLITE_SQL_AST_REPEAT_FUNCTION:
    case MYLITE_SQL_AST_REPLACE_FUNCTION:
    case MYLITE_SQL_AST_REGEXP_REPLACE_FUNCTION:
    case MYLITE_SQL_AST_STR_TO_DATE_FUNCTION:
    case MYLITE_SQL_AST_SEC_TO_TIME_FUNCTION:
    case MYLITE_SQL_AST_IF_FUNCTION:
    case MYLITE_SQL_AST_IFNULL_FUNCTION:
    case MYLITE_SQL_AST_NULLIF_FUNCTION:
    case MYLITE_SQL_AST_COALESCE_FUNCTION:
    case MYLITE_SQL_AST_GREATEST_FUNCTION:
    case MYLITE_SQL_AST_LEAST_FUNCTION:
    case MYLITE_SQL_AST_JSON_ARRAY_FUNCTION:
    case MYLITE_SQL_AST_JSON_OBJECT_FUNCTION:
    case MYLITE_SQL_AST_JSON_EXTRACT_FUNCTION:
    case MYLITE_SQL_AST_JSON_UNQUOTE_FUNCTION:
    case MYLITE_SQL_AST_JSON_QUOTE_FUNCTION:
    case MYLITE_SQL_AST_JSON_VALID_FUNCTION:
    case MYLITE_SQL_AST_MD5_FUNCTION:
    case MYLITE_SQL_AST_SHA_FUNCTION:
    case MYLITE_SQL_AST_SHA1_FUNCTION:
    case MYLITE_SQL_AST_SHA2_FUNCTION:
    case MYLITE_SQL_AST_HEX_FUNCTION:
    case MYLITE_SQL_AST_COMPRESS_FUNCTION:
    case MYLITE_SQL_AST_UNCOMPRESS_FUNCTION:
    case MYLITE_SQL_AST_UNCOMPRESSED_LENGTH_FUNCTION:
    case MYLITE_SQL_AST_RANDOM_BYTES_FUNCTION:
    case MYLITE_SQL_AST_INET_ATON_FUNCTION:
    case MYLITE_SQL_AST_INET_NTOA_FUNCTION:
    case MYLITE_SQL_AST_UNHEX_FUNCTION:
    case MYLITE_SQL_AST_ABS_FUNCTION:
    case MYLITE_SQL_AST_FLOOR_FUNCTION:
    case MYLITE_SQL_AST_ROUND_FUNCTION:
    case MYLITE_SQL_AST_ACOS_FUNCTION:
    case MYLITE_SQL_AST_LOG2_FUNCTION:
    case MYLITE_SQL_AST_LOG10_FUNCTION:
    case MYLITE_SQL_AST_POW_FUNCTION:
    case MYLITE_SQL_AST_POWER_FUNCTION:
    case MYLITE_SQL_AST_BIT_COUNT_FUNCTION:
    case MYLITE_SQL_AST_DATE_ADD_FUNCTION:
    case MYLITE_SQL_AST_ADDTIME_FUNCTION:
    case MYLITE_SQL_AST_TIMESTAMP_FUNCTION:
    case MYLITE_SQL_AST_GENERIC_FUNCTION:
        rc = session_scalar_value(database, unwrapped, out_cell);
        if (rc == MYLITE_OK) {
            *out_value_kind = infer_user_variable_value_kind(unwrapped, out_cell);
            if (unwrapped->kind == MYLITE_SQL_AST_USER_VARIABLE) {
                rc = session_user_variable_value_kind(database, unwrapped, out_value_kind);
            }
        }
        return rc;
    default:
        set_unsupported_error(
            database,
            "SET user variables support only integer, decimal, string, boolean, NULL, variable, "
            "and supported scalar function values"
        );
        return MYLITE_ERROR;
    }
}

static int copy_user_variable_source_text_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    enum mylite_session_user_variable_value_kind value_kind,
    struct session_scalar_cell *out_cell,
    enum mylite_session_user_variable_value_kind *out_value_kind
) {
    const struct mylite_sql_source_span *span = value_node == NULL ? NULL : &value_node->span;

    if (out_cell == NULL || out_value_kind == NULL) {
        return MYLITE_MISUSE;
    }
    if (span == NULL || span->text == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    out_cell->owned_text = (char *)malloc(span->length + 1U);
    if (out_cell->owned_text == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(out_cell->owned_text, span->text, span->length);
    out_cell->owned_text[span->length] = '\0';
    out_cell->value = out_cell->owned_text;
    out_cell->value_size = span->length;
    out_cell->has_value_size = true;
    *out_value_kind = value_kind;
    return MYLITE_OK;
}

static int set_session_user_variable(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    char name[MYLITE_SESSION_USER_VARIABLE_NAME_CAPACITY];
    struct mylite_session_user_variable *variable = NULL;
    const char *source_value = value == NULL ? NULL : value->value;
    size_t source_size = 0U;
    char *owned_value = NULL;
    int rc = copy_user_variable_name(database, target, name, sizeof(name), false);

    if (rc != MYLITE_OK) {
        return rc;
    }

    variable = find_session_user_variable(&database->session, name);
    if (variable == NULL) {
        rc = ensure_session_user_variable_capacity(database);
        if (rc != MYLITE_OK) {
            return rc;
        }
        variable = &database->session.user_variables[database->session.user_variable_count];
        *variable = (struct mylite_session_user_variable){0};
        memcpy(variable->name, name, strlen(name) + 1U);
        ++database->session.user_variable_count;
    }

    if (source_value != NULL) {
        if (value->has_value_size) {
            source_size = value->value_size;
        } else {
            source_size = strlen(source_value);
        }
        owned_value = (char *)malloc(source_size + 1U);
        if (owned_value == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        memcpy(owned_value, source_value, source_size);
        owned_value[source_size] = '\0';
    }

    free(variable->value);
    variable->value = owned_value;
    variable->value_size = source_size;
    variable->is_null = source_value == NULL;
    variable->value_kind =
        source_value == NULL ? MYLITE_SESSION_USER_VARIABLE_VALUE_NULL : value_kind;
    return MYLITE_OK;
}

static int session_user_variable_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct session_scalar_cell *out_cell
) {
    char name[MYLITE_SESSION_USER_VARIABLE_NAME_CAPACITY];
    struct mylite_session_user_variable *variable = NULL;
    int rc = MYLITE_OK;

    if (out_cell == NULL) {
        return MYLITE_MISUSE;
    }
    *out_cell = (struct session_scalar_cell){0};
    rc = copy_user_variable_name(database, node, name, sizeof(name), true);
    if (rc != MYLITE_OK) {
        return rc;
    }
    variable = find_session_user_variable(&database->session, name);
    if (variable == NULL || variable->is_null) {
        return MYLITE_OK;
    }

    if (variable->value_size == SIZE_MAX) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_cell->owned_text = (char *)malloc(variable->value_size + 1U);
    if (out_cell->owned_text == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(out_cell->owned_text, variable->value, variable->value_size);
    out_cell->owned_text[variable->value_size] = '\0';
    out_cell->value = out_cell->owned_text;
    out_cell->value_size = variable->value_size;
    out_cell->has_value_size = true;
    return MYLITE_OK;
}

static int session_user_variable_value_kind(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    enum mylite_session_user_variable_value_kind *out_value_kind
) {
    char name[MYLITE_SESSION_USER_VARIABLE_NAME_CAPACITY];
    struct mylite_session_user_variable *variable = NULL;
    int rc = MYLITE_OK;

    if (out_value_kind == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value_kind = MYLITE_SESSION_USER_VARIABLE_VALUE_NULL;
    rc = copy_user_variable_name(database, node, name, sizeof(name), true);
    if (rc != MYLITE_OK) {
        return rc;
    }
    variable = find_session_user_variable(&database->session, name);
    if (variable == NULL || variable->is_null) {
        return MYLITE_OK;
    }

    *out_value_kind = variable->value_kind;
    return MYLITE_OK;
}

static int copy_user_variable_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    char *buffer,
    size_t buffer_size,
    bool allow_empty_name
) {
    const struct mylite_sql_source_span *span = node == NULL ? NULL : &node->span;
    char quote = '\0';
    int rc = MYLITE_OK;

    if (buffer == NULL || buffer_size == 0U) {
        return MYLITE_MISUSE;
    }
    buffer[0] = '\0';
    if (node == NULL || node->kind != MYLITE_SQL_AST_USER_VARIABLE || span == NULL ||
        span->text == NULL || span->length == 0U || span->text[0] != '@') {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (span->length == 1U) {
        return allow_empty_name ? MYLITE_OK
                                : set_illegal_user_variable_name_error(database, "", 0U);
    }
    quote = span->text[1];
    if (quote == '\'' || quote == '"' || quote == '`') {
        rc = copy_quoted_user_variable_name(
            database,
            span,
            quote,
            buffer,
            buffer_size,
            allow_empty_name
        );
    } else {
        rc =
            copy_unquoted_user_variable_name(database, span, buffer, buffer_size, allow_empty_name);
    }
    if (rc == MYLITE_OK) {
        fold_user_variable_name(buffer);
    }
    return rc;
}

static int copy_unquoted_user_variable_name(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    char *buffer,
    size_t buffer_size,
    bool allow_empty_name
) {
    size_t name_length = span == NULL ? 0U : span->length - 1U;

    if (span == NULL || span->text == NULL || name_length >= buffer_size) {
        return set_illegal_user_variable_name_error(
            database,
            span == NULL || span->text == NULL ? "" : span->text + 1U,
            name_length
        );
    }
    if (name_length == 0U) {
        return allow_empty_name ? MYLITE_OK
                                : set_illegal_user_variable_name_error(database, "", 0U);
    }
    memcpy(buffer, span->text + 1U, name_length);
    buffer[name_length] = '\0';
    return validate_user_variable_name(database, buffer, name_length);
}

static int copy_quoted_user_variable_name(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    char quote,
    char *buffer,
    size_t buffer_size,
    bool allow_empty_name
) {
    if (span == NULL || span->text == NULL || span->length < 3U) {
        return set_illegal_user_variable_name_error(database, "", 0U);
    }
    if (quote == '\'' || quote == '"') {
        return copy_string_quoted_user_variable_name(
            database,
            span,
            buffer,
            buffer_size,
            allow_empty_name
        );
    }
    return copy_identifier_quoted_user_variable_name(
        database,
        span,
        quote,
        buffer,
        buffer_size,
        allow_empty_name
    );
}

static int copy_string_quoted_user_variable_name(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    char *buffer,
    size_t buffer_size,
    bool allow_empty_name
) {
    char *decoded = NULL;
    size_t decoded_length = 0U;
    struct mylite_sql_ast_node literal = {
        .kind = MYLITE_SQL_AST_LITERAL,
        .span = {.text = span->text + 1U, .length = span->length - 1U},
        .payload = {.literal = {.kind = MYLITE_SQL_AST_LITERAL_STRING}},
    };
    int rc = decode_sql_string_literal(
        database,
        &literal,
        "SET user variable names support only quoted variable names",
        "SET user variable names do not support NUL bytes",
        &decoded,
        &decoded_length
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (decoded_length >= buffer_size) {
        rc = set_illegal_user_variable_name_error(database, decoded, decoded_length);
        free(decoded);
        return rc;
    }
    if (decoded_length == 0U) {
        free(decoded);
        return allow_empty_name ? MYLITE_OK
                                : set_illegal_user_variable_name_error(database, "", 0U);
    }
    memcpy(buffer, decoded, decoded_length);
    buffer[decoded_length] = '\0';
    rc = validate_user_variable_name(database, buffer, decoded_length);
    free(decoded);
    return rc;
}

static int copy_identifier_quoted_user_variable_name(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    char quote,
    char *buffer,
    size_t buffer_size,
    bool allow_empty_name
) {
    size_t name_length = 0U;

    for (size_t index = 2U; index < span->length; ++index) {
        char byte = span->text[index];

        if (byte == quote) {
            if (index + 1U < span->length && span->text[index + 1U] == quote) {
                int rc = append_user_variable_name_byte(
                    database,
                    buffer,
                    buffer_size,
                    &name_length,
                    byte
                );

                if (rc != MYLITE_OK) {
                    return rc;
                }
                ++index;
                continue;
            }
            if (index + 1U != span->length) {
                return set_illegal_user_variable_name_error(database, buffer, name_length);
            }
            if (name_length == 0U) {
                return allow_empty_name ? MYLITE_OK
                                        : set_illegal_user_variable_name_error(database, "", 0U);
            }
            buffer[name_length] = '\0';
            return validate_user_variable_name(database, buffer, name_length);
        }
        {
            int rc =
                append_user_variable_name_byte(database, buffer, buffer_size, &name_length, byte);

            if (rc != MYLITE_OK) {
                return rc;
            }
        }
    }

    return set_illegal_user_variable_name_error(database, buffer, name_length);
}

static int append_user_variable_name_byte(
    struct mylite_db *database,
    char *buffer,
    size_t buffer_size,
    size_t *inout_length,
    char byte
) {
    if (buffer == NULL || inout_length == NULL || buffer_size == 0U) {
        return MYLITE_MISUSE;
    }
    if (byte == '\0' || *inout_length + 1U >= buffer_size) {
        return set_illegal_user_variable_name_error(database, buffer, *inout_length);
    }

    buffer[*inout_length] = byte;
    ++*inout_length;
    buffer[*inout_length] = '\0';
    return MYLITE_OK;
}

static int validate_user_variable_name(
    struct mylite_db *database,
    const char *name,
    size_t name_length
) {
    size_t character_count = 0U;

    if (name == NULL) {
        return MYLITE_MISUSE;
    }
    if (name_length == 0U || name_length > MYLITE_SESSION_USER_VARIABLE_NAME_MAX_BYTES ||
        validate_utf8_text(name, name_length, &character_count) != MYLITE_OK ||
        character_count == 0U ||
        character_count > MYLITE_SESSION_USER_VARIABLE_NAME_MAX_CHARACTERS) {
        return set_illegal_user_variable_name_error(database, name, name_length);
    }
    return MYLITE_OK;
}

static int set_illegal_user_variable_name_error(
    struct mylite_db *database,
    const char *name,
    size_t name_length
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int printable_length = name_length > INT_MAX ? INT_MAX : (int)name_length;
    int written = snprintf(
        message,
        sizeof(message),
        "User variable name '%.*s' is illegal",
        printable_length,
        name == NULL ? "" : name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_illegal_user_variable_name,
        "42000",
        message
    );
    return MYLITE_ERROR;
}

static void fold_user_variable_name(char *text) {
    if (text == NULL) {
        return;
    }
    for (size_t index = 0U; text[index] != '\0'; ++index) {
        text[index] = ascii_lower((unsigned char)text[index]);
    }
}

static struct mylite_session_user_variable *find_session_user_variable(
    struct mylite_session_state *session,
    const char *name
) {
    if (session == NULL || name == NULL) {
        return NULL;
    }
    for (size_t index = 0U; index < session->user_variable_count; ++index) {
        if (strcmp(session->user_variables[index].name, name) == 0) {
            return &session->user_variables[index];
        }
    }
    return NULL;
}

static bool text_is_decimal_integer_literal(const char *text, size_t text_size) {
    size_t index = 0U;

    if (text == NULL || text_size == 0U) {
        return false;
    }
    if (text[index] == '+' || text[index] == '-') {
        ++index;
        if (index == text_size) {
            return false;
        }
    }
    for (; index < text_size; ++index) {
        if (text[index] < '0' || text[index] > '9') {
            return false;
        }
    }
    return true;
}

static enum mylite_session_user_variable_value_kind infer_user_variable_value_kind(
    const struct mylite_sql_ast_node *value_node,
    const struct session_scalar_cell *value
) {
    const struct mylite_sql_ast_node *literal = NULL;
    size_t value_size = 0U;

    if (value == NULL || value->value == NULL) {
        return MYLITE_SESSION_USER_VARIABLE_VALUE_NULL;
    }

    value_node = unwrap_parenthesized_expression(value_node);
    literal = value_node;
    if (value_node != NULL && value_node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        literal = child_at(value_node, 0U);
    }
    if (literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL) {
        switch (mylite_sql_ast_node_literal_kind(literal)) {
        case MYLITE_SQL_AST_LITERAL_INTEGER:
        case MYLITE_SQL_AST_LITERAL_TRUE:
        case MYLITE_SQL_AST_LITERAL_FALSE:
            return MYLITE_SESSION_USER_VARIABLE_VALUE_INTEGER;
        case MYLITE_SQL_AST_LITERAL_NULL:
            return MYLITE_SESSION_USER_VARIABLE_VALUE_NULL;
        case MYLITE_SQL_AST_LITERAL_STRING:
        case MYLITE_SQL_AST_LITERAL_NATIONAL_STRING:
        case MYLITE_SQL_AST_LITERAL_HEX:
        case MYLITE_SQL_AST_LITERAL_BIT:
        case MYLITE_SQL_AST_LITERAL_NONE:
            return MYLITE_SESSION_USER_VARIABLE_VALUE_STRING;
        case MYLITE_SQL_AST_LITERAL_DECIMAL:
            return MYLITE_SESSION_USER_VARIABLE_VALUE_DECIMAL;
        case MYLITE_SQL_AST_LITERAL_FLOAT:
            return MYLITE_SESSION_USER_VARIABLE_VALUE_STRING;
        }
    }

    if (value->has_value_size) {
        value_size = value->value_size;
    } else {
        value_size = strlen(value->value);
    }
    if (text_is_decimal_integer_literal(value->value, value_size)) {
        return MYLITE_SESSION_USER_VARIABLE_VALUE_INTEGER;
    }
    return MYLITE_SESSION_USER_VARIABLE_VALUE_STRING;
}

static int ensure_session_user_variable_capacity(struct mylite_db *database) {
    struct mylite_session_state *session = database == NULL ? NULL : &database->session;
    struct mylite_session_user_variable *variables = NULL;
    size_t new_capacity = 0U;

    if (session == NULL) {
        return MYLITE_MISUSE;
    }
    if (session->user_variable_count < session->user_variable_capacity) {
        return MYLITE_OK;
    }
    new_capacity =
        session->user_variable_capacity == 0U ? 4U : session->user_variable_capacity * 2U;
    if (new_capacity < session->user_variable_capacity ||
        new_capacity > SIZE_MAX / sizeof(*variables)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    variables = (struct mylite_session_user_variable *)
        realloc(session->user_variables, new_capacity * sizeof(*variables));
    if (variables == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memset(
        variables + session->user_variable_capacity,
        0,
        (new_capacity - session->user_variable_capacity) * sizeof(*variables)
    );
    session->user_variables = variables;
    session->user_variable_capacity = new_capacity;
    return MYLITE_OK;
}

static int set_session_system_variable_override(
    struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind,
    const char *value,
    size_t value_size
) {
    struct mylite_session_state *session = database == NULL ? NULL : &database->session;
    struct mylite_session_system_variable_override *override = NULL;
    char *owned_value = NULL;
    int rc = MYLITE_OK;

    if (session == NULL || value == NULL) {
        return MYLITE_MISUSE;
    }
    if (value_size == SIZE_MAX) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    override = find_session_system_variable_override(session, kind);
    if (override == NULL) {
        rc = ensure_session_system_variable_override_capacity(database);
        if (rc != MYLITE_OK) {
            return rc;
        }
        override = &session->system_variable_overrides[session->system_variable_override_count];
        *override = (struct mylite_session_system_variable_override){0};
        override->kind = kind;
        ++session->system_variable_override_count;
    }

    owned_value = (char *)malloc(value_size + 1U);
    if (owned_value == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(owned_value, value, value_size);
    owned_value[value_size] = '\0';

    free(override->value);
    override->value = owned_value;
    override->value_size = value_size;
    session->system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static const char *session_system_variable_override_value(
    const struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind
) {
    struct mylite_session_state *session =
        (struct mylite_session_state *)(database == NULL ? NULL : &database->session);
    struct mylite_session_system_variable_override *override =
        find_session_system_variable_override(session, kind);

    if (override == NULL) {
        return NULL;
    }
    return override->value;
}

static const char *session_system_variable_override_show_value(
    const struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind
) {
    const char *value = session_system_variable_override_value(database, kind);

    if (value == NULL) {
        return NULL;
    }
    if (!mylite_execution_system_variable_is_boolean_session_placeholder(kind)) {
        return value;
    }
    if (kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_UPDATABLE_VIEWS_WITH_LIMIT) {
        if (strcmp(value, "1") == 0) {
            return "YES";
        }
        if (strcmp(value, "0") == 0) {
            return "NO";
        }
        return value;
    }
    if (strcmp(value, "1") == 0) {
        return "ON";
    }
    if (strcmp(value, "0") == 0) {
        return "OFF";
    }
    return value;
}

static struct mylite_session_system_variable_override *find_session_system_variable_override(
    struct mylite_session_state *session,
    enum mylite_execution_system_variable_kind kind
) {
    if (session == NULL) {
        return NULL;
    }
    for (size_t index = 0U; index < session->system_variable_override_count; ++index) {
        if (session->system_variable_overrides[index].kind == kind) {
            return &session->system_variable_overrides[index];
        }
    }
    return NULL;
}

static int ensure_session_system_variable_override_capacity(struct mylite_db *database) {
    struct mylite_session_state *session = database == NULL ? NULL : &database->session;
    struct mylite_session_system_variable_override *overrides = NULL;
    size_t new_capacity = 0U;

    if (session == NULL) {
        return MYLITE_MISUSE;
    }
    if (session->system_variable_override_count < session->system_variable_override_capacity) {
        return MYLITE_OK;
    }
    new_capacity = session->system_variable_override_capacity == 0U
                       ? 4U
                       : session->system_variable_override_capacity * 2U;
    if (new_capacity < session->system_variable_override_capacity ||
        new_capacity > SIZE_MAX / sizeof(*overrides)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    overrides = (struct mylite_session_system_variable_override *)
        realloc(session->system_variable_overrides, new_capacity * sizeof(*overrides));
    if (overrides == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memset(
        overrides + session->system_variable_override_capacity,
        0,
        (new_capacity - session->system_variable_override_capacity) * sizeof(*overrides)
    );
    session->system_variable_overrides = overrides;
    session->system_variable_override_capacity = new_capacity;
    return MYLITE_OK;
}

static int apply_set_global_read_only_toggle_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
) {
    *out_handled = false;
    if (!mylite_execution_system_variable_is_global_read_only_toggle(target->kind)) {
        return MYLITE_OK;
    }
    *out_handled = true;
    if (target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_global_variable_set_global_required_error(database, target->name);
        return MYLITE_ERROR;
    }
    return validate_set_fixed_boolean_value(database, value_node, false);
}

static int apply_set_global_server_security_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
) {
    const char *unsupported_message = "SET supports only fixed no-op system variable assignments";

    if (target == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    if (!mylite_execution_system_variable_is_fixed_global_server_security(target->kind)) {
        return MYLITE_OK;
    }

    *out_handled = true;
    if (target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_global_variable_set_global_required_error(database, target->name);
        return MYLITE_ERROR;
    }

    switch (target->kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REQUIRE_SECURE_TRANSPORT:
        return validate_set_fixed_boolean_value(database, value_node, false);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_SESSION_CACHE_MODE:
        return validate_set_fixed_boolean_value(database, value_node, true);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SSL_SESSION_CACHE_TIMEOUT:
        return validate_set_fixed_uint64_value(
            database,
            value_node,
            ssl_session_cache_timeout_system_variable_value,
            unsupported_message
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_TLS_VERSION:
        return validate_set_fixed_text_value(
            database,
            value_node,
            mylite_execution_system_variable_default_value(target->kind)
        );
    default:
        set_runtime_error(database, "invalid fixed server security system variable");
        return MYLITE_ERROR;
    }
}

static int apply_set_global_admin_listener_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
) {
    const char *unsupported_message = "SET supports only fixed no-op system variable assignments";

    if (target == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    if (!mylite_execution_system_variable_is_fixed_global_admin_listener(target->kind)) {
        return MYLITE_OK;
    }

    *out_handled = true;
    if (target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_global_variable_set_global_required_error(database, target->name);
        return MYLITE_ERROR;
    }

    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_ADMIN_TLS_VERSION) {
        return validate_set_fixed_text_value(
            database,
            value_node,
            mylite_execution_system_variable_default_value(target->kind)
        );
    }
    return validate_set_default_only_value(database, value_node, unsupported_message);
}

static int apply_set_global_mysqlx_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
) {
    if (target == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    if (!mylite_execution_system_variable_is_fixed_global_mysqlx(target->kind)) {
        return MYLITE_OK;
    }

    *out_handled = true;
    if (target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_global_variable_set_global_required_error(database, target->name);
        return MYLITE_ERROR;
    }

    if (target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_ENABLE_HELLO_NOTICE) {
        return validate_set_fixed_boolean_value(database, value_node, true);
    }
    return validate_set_fixed_text_value(
        database,
        value_node,
        mylite_execution_system_variable_default_value(target->kind)
    );
}

static int apply_set_global_authentication_password_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
) {
    const char *unsupported_message = "SET supports only fixed no-op system variable assignments";

    if (target == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    if (!mylite_execution_system_variable_is_fixed_global_authentication_password(target->kind)) {
        return MYLITE_OK;
    }

    *out_handled = true;
    if (target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_global_variable_set_global_required_error(database, target->name);
        return MYLITE_ERROR;
    }

    switch (target->kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_AUTHENTICATION_POLICY:
        return validate_set_fixed_text_value(
            database,
            value_node,
            mylite_execution_system_variable_default_value(target->kind)
        );
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_PASSWORD_LIFETIME:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PASSWORD_HISTORY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PASSWORD_REUSE_INTERVAL:
        return validate_set_fixed_uint64_value(database, value_node, 0U, unsupported_message);
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQL_NATIVE_PASSWORD_PROXY_USERS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PASSWORD_REQUIRE_CURRENT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHA256_PASSWORD_PROXY_USERS:
        return validate_set_fixed_boolean_value(database, value_node, false);
    default:
        set_runtime_error(database, "invalid fixed authentication/password system variable");
        return MYLITE_ERROR;
    }
}

static int apply_set_global_session_placeholder_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
) {
    const char *default_value = NULL;
    bool expected_boolean_value = false;

    if (target == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    if (target->scope != SET_SYSTEM_VARIABLE_SCOPE_GLOBAL ||
        !set_global_session_placeholder_assignment_handles_kind(target->kind)) {
        return MYLITE_OK;
    }

    *out_handled = true;
    if (!mylite_execution_system_variable_allows_global_scope(target->kind)) {
        set_session_variable_set_global_error(database, target->name);
        return MYLITE_ERROR;
    }

    if (mylite_execution_system_variable_is_boolean_session_placeholder(target->kind)) {
        default_value = mylite_execution_system_variable_default_value(target->kind);
        expected_boolean_value = default_value != NULL && strcmp(default_value, "1") == 0;
        return validate_set_fixed_boolean_value(database, value_node, expected_boolean_value);
    }

    return validate_set_fixed_text_value(
        database,
        value_node,
        mylite_execution_system_variable_default_value(target->kind)
    );
}

static bool set_global_session_placeholder_assignment_handles_kind(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_COLLATION_FOR_UTF8MB4:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_END_MARKERS_IN_JSON:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_GLOBAL_CONNECTION_MEMORY_TRACKING:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_KEEP_FILES_ON_CREATE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_OLD_ALTER_TABLE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_PRINT_IDENTIFIED_WITH_AS_HEX:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_REQUIRE_ROW_FORMAT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_RESULTSET_METADATA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SELECT_INTO_DISK_SYNC:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SESSION_TRACK_GTIDS:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SESSION_TRACK_SCHEMA:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SESSION_TRACK_STATE_CHANGE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SESSION_TRACK_TRANSACTION_INFO:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHOW_CREATE_TABLE_SKIP_SECONDARY_ENGINE:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_SHOW_CREATE_TABLE_VERBOSITY:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_READ_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_WAIT_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_MYSQLX_WRITE_TIMEOUT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_USE_SECONDARY_ENGINE:
        return true;
    default:
        return false;
    }
}

static int apply_set_default_engine_system_variable_assignment(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node,
    bool *out_handled
) {
    if (target == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    if (!set_system_variable_is_default_engine(target->kind)) {
        return MYLITE_OK;
    }

    *out_handled = true;
    return validate_set_fixed_text_value(database, value_node, "InnoDB");
}

static int apply_set_default_engine_system_variable_cell_value(
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
    if (!set_system_variable_is_default_engine(target->kind)) {
        return MYLITE_OK;
    }

    *out_handled = true;
    return validate_set_fixed_text_cell_value(database, target, value, value_kind, "InnoDB");
}

static bool set_system_variable_is_default_engine(
    enum mylite_execution_system_variable_kind kind
) {
    return kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_STORAGE_ENGINE ||
           kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_DEFAULT_TMP_STORAGE_ENGINE;
}

static int apply_set_text_session_system_variable_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    char *value = NULL;
    size_t value_size = 0U;
    int rc = MYLITE_OK;

    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_unsupported_error(database, "SET GLOBAL system variable assignment is not supported");
        return MYLITE_ERROR;
    }

    rc = copy_set_text_system_variable_node_value(
        database,
        value_node,
        target->kind,
        target->name,
        &value,
        &value_size
    );
    if (rc == MYLITE_OK) {
        rc = set_text_session_system_variable(database, target->kind, value, value_size);
    }
    free(value);
    return rc;
}

static int apply_set_text_session_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value
) {
    const char *text = value == NULL ? NULL : value->value;
    size_t text_size = 0U;

    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_unsupported_error(database, "SET GLOBAL system variable assignment is not supported");
        return MYLITE_ERROR;
    }
    if (text == NULL) {
        set_system_variable_cant_be_set_value_error(database, target->name, "NULL");
        return MYLITE_ERROR;
    }

    text_size = value->has_value_size ? value->value_size : strlen(text);
    return set_text_session_system_variable(database, target->kind, text, text_size);
}

static bool set_system_variable_is_text_session_state(
    enum mylite_execution_system_variable_kind kind
) {
    return text_session_system_variable_default_value(kind) != NULL;
}

static int set_text_session_system_variable(
    struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind,
    const char *value,
    size_t value_size
) {
    char *target = NULL;
    size_t target_size = 0U;

    if (database == NULL || value == NULL) {
        return MYLITE_MISUSE;
    }
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_CLIENT:
        target = database->session.character_set_client;
        target_size = sizeof(database->session.character_set_client);
        break;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_CONNECTION:
        target = database->session.character_set_connection;
        target_size = sizeof(database->session.character_set_connection);
        break;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_RESULTS:
        target = database->session.character_set_results;
        target_size = sizeof(database->session.character_set_results);
        break;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_COLLATION_CONNECTION:
        target = database->session.collation_connection;
        target_size = sizeof(database->session.collation_connection);
        break;
    default:
        return MYLITE_MISUSE;
    }
    if (value_size >= target_size) {
        set_system_variable_cant_be_set_value_error(database, "character set", value);
        return MYLITE_ERROR;
    }

    memcpy(target, value, value_size);
    target[value_size] = '\0';
    database->session.character_set_state_is_placeholder = false;
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}

static const char *text_session_system_variable_default_value(
    enum mylite_execution_system_variable_kind kind
) {
    switch (kind) {
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_CLIENT:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_CONNECTION:
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_CHARACTER_SET_RESULTS:
        return MYLITE_CATALOG_DEFAULT_TABLE_CHARSET;
    case MYLITE_EXECUTION_SYSTEM_VARIABLE_COLLATION_CONNECTION:
        return MYLITE_CATALOG_DEFAULT_TABLE_COLLATION;
    default:
        return NULL;
    }
}

static int copy_set_text_system_variable_node_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    enum mylite_execution_system_variable_kind kind,
    const char *variable_name,
    char **out_value,
    size_t *out_value_size
) {
    if (out_value == NULL || out_value_size == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = NULL;
    *out_value_size = 0U;
    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        return copy_set_text_system_variable_default_value(
            database,
            kind,
            out_value,
            out_value_size
        );
    }
    if (value_node->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_STRING) {
        return decode_sql_string_literal(
            database,
            value_node,
            "SET system variable values support only string literals",
            "SET system variable string values do not support NUL bytes",
            out_value,
            out_value_size
        );
    }
    if (value_node->kind == MYLITE_SQL_AST_IDENTIFIER) {
        return copy_set_text_system_variable_identifier_value(
            database,
            value_node,
            out_value,
            out_value_size
        );
    }
    if (value_node->kind == MYLITE_SQL_AST_LITERAL ||
        value_node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return copy_set_text_system_variable_scalar_value(
            database,
            value_node,
            variable_name,
            out_value,
            out_value_size
        );
    }

    set_system_variable_cant_be_set_value_error(database, variable_name, "");
    return MYLITE_ERROR;
}

static int copy_set_text_system_variable_default_value(
    struct mylite_db *database,
    enum mylite_execution_system_variable_kind kind,
    char **out_value,
    size_t *out_value_size
) {
    const char *default_value = text_session_system_variable_default_value(kind);

    if (default_value == NULL) {
        default_value = mylite_execution_system_variable_default_value(kind);
    }
    if (default_value == NULL) {
        set_runtime_error(database, "invalid SET DEFAULT text system variable");
        return MYLITE_ERROR;
    }
    *out_value_size = strlen(default_value);
    *out_value = (char *)malloc(*out_value_size + 1U);
    if (*out_value == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(*out_value, default_value, *out_value_size + 1U);
    return MYLITE_OK;
}

static int copy_set_text_system_variable_identifier_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    char **out_value,
    size_t *out_value_size
) {
    char identifier[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = copy_identifier_text(value_node, identifier, sizeof(identifier), database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    *out_value_size = strlen(identifier);
    *out_value = (char *)malloc(*out_value_size + 1U);
    if (*out_value == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(*out_value, identifier, *out_value_size + 1U);
    return MYLITE_OK;
}

static int copy_set_text_system_variable_scalar_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const char *variable_name,
    char **out_value,
    size_t *out_value_size
) {
    struct session_scalar_cell value = {0};
    int rc = session_scalar_value(database, value_node, &value);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (value.value == NULL) {
        set_system_variable_cant_be_set_value_error(database, variable_name, "NULL");
        session_scalar_cell_deinit(&value);
        return MYLITE_ERROR;
    }
    *out_value_size = value.has_value_size ? value.value_size : strlen(value.value);
    *out_value = (char *)malloc(*out_value_size + 1U);
    if (*out_value == NULL) {
        set_nomem_error(database);
        session_scalar_cell_deinit(&value);
        return MYLITE_NOMEM;
    }
    memcpy(*out_value, value.value, *out_value_size);
    (*out_value)[*out_value_size] = '\0';
    session_scalar_cell_deinit(&value);
    return MYLITE_OK;
}

static int apply_set_session_placeholder_system_variable_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct mylite_sql_ast_node *value_node
) {
    char *value = NULL;
    size_t value_size = 0U;
    int rc = MYLITE_OK;

    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_unsupported_error(database, "SET GLOBAL system variable assignment is not supported");
        return MYLITE_ERROR;
    }

    rc = copy_set_session_placeholder_node_value(database, value_node, target, &value, &value_size);
    if (rc == MYLITE_OK) {
        rc = set_session_system_variable_override(database, target->kind, value, value_size);
    }
    free(value);
    return rc;
}

static int apply_set_session_placeholder_system_variable_cell_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct session_scalar_cell *value,
    enum mylite_session_user_variable_value_kind value_kind
) {
    const char *text = value == NULL ? NULL : value->value;
    size_t text_size = 0U;
    bool boolean_value = false;
    int rc = MYLITE_OK;

    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        set_unsupported_error(database, "SET GLOBAL system variable assignment is not supported");
        return MYLITE_ERROR;
    }
    if (mylite_execution_system_variable_is_boolean_session_placeholder(target->kind)) {
        rc = parse_set_big_tables_cell_value(
            database,
            target->name,
            value,
            value_kind,
            &boolean_value
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        return apply_set_session_placeholder_boolean_value(database, target, boolean_value);
    }
    if (text == NULL) {
        set_system_variable_cant_be_set_value_error(database, target->name, "NULL");
        return MYLITE_ERROR;
    }

    text_size = value->has_value_size ? value->value_size : strlen(text);
    return set_session_system_variable_override(database, target->kind, text, text_size);
}

static int apply_set_session_placeholder_boolean_value(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    bool value
) {
    const char *text = value ? "1" : "0";

    if (target == NULL) {
        return MYLITE_MISUSE;
    }
    return set_session_system_variable_override(database, target->kind, text, 1U);
}

static int copy_set_session_placeholder_node_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct resolved_set_system_variable_target *target,
    char **out_value,
    size_t *out_value_size
) {
    const char *default_value = NULL;
    bool boolean_value = false;
    int rc = MYLITE_OK;

    if (target == NULL || out_value == NULL || out_value_size == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = NULL;
    *out_value_size = 0U;
    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        default_value = target->kind == MYLITE_EXECUTION_SYSTEM_VARIABLE_UNIQUE_CHECKS
                            ? session_system_variable_override_value(database, target->kind)
                            : NULL;
        if (default_value == NULL) {
            default_value = mylite_execution_system_variable_default_value(target->kind);
        }
        if (default_value == NULL) {
            set_runtime_error(database, "invalid SET DEFAULT placeholder system variable");
            return MYLITE_ERROR;
        }
        *out_value_size = strlen(default_value);
        *out_value = (char *)malloc(*out_value_size + 1U);
        if (*out_value == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        memcpy(*out_value, default_value, *out_value_size + 1U);
        return MYLITE_OK;
    }
    if (mylite_execution_system_variable_is_boolean_session_placeholder(target->kind)) {
        rc = parse_set_big_tables_value(database, target->name, value_node, &boolean_value);
        if (rc != MYLITE_OK) {
            return rc;
        }
        *out_value_size = 1U;
        *out_value = (char *)malloc(2U);
        if (*out_value == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        (*out_value)[0] = boolean_value ? '1' : '0';
        (*out_value)[1] = '\0';
        return MYLITE_OK;
    }
    return copy_set_text_system_variable_node_value(
        database,
        value_node,
        target->kind,
        target->name,
        out_value,
        out_value_size
    );
}
