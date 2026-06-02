static int copy_set_session_snapshot(
    struct mylite_db *database,
    struct set_session_snapshot *out_snapshot
) {
    const struct mylite_session_state *session = database == NULL ? NULL : &database->session;
    struct mylite_session_user_variable *variables = NULL;
    int rc = MYLITE_OK;

    if (session == NULL || out_snapshot == NULL) {
        return MYLITE_MISUSE;
    }
    *out_snapshot = (struct set_session_snapshot){0};
    rc = copy_session_user_variables(database, session, &variables);
    if (rc != MYLITE_OK) {
        return rc;
    }

    out_snapshot->sql_mode = session->sql_mode;
    out_snapshot->auto_increment_increment = session->auto_increment_increment;
    out_snapshot->auto_increment_offset = session->auto_increment_offset;
    out_snapshot->sql_select_limit = session->sql_select_limit;
    out_snapshot->group_concat_max_len = session->group_concat_max_len;
    out_snapshot->information_schema_stats_expiry = session->information_schema_stats_expiry;
    out_snapshot->wait_timeout = session->wait_timeout;
    out_snapshot->interactive_timeout = session->interactive_timeout;
    out_snapshot->timestamp_override = session->timestamp_override;
    out_snapshot->time_zone_offset_minutes = session->time_zone_offset_minutes;
    out_snapshot->session_transaction_isolation = session->session_transaction_isolation;
    out_snapshot->session_transaction_access_mode = session->session_transaction_access_mode;
    out_snapshot->next_transaction_isolation = session->next_transaction_isolation;
    out_snapshot->next_transaction_access_mode = session->next_transaction_access_mode;
    out_snapshot->sql_mode_is_placeholder = session->sql_mode_is_placeholder;
    out_snapshot->time_zone_is_placeholder = session->time_zone_is_placeholder;
    out_snapshot->system_variables_are_placeholder = session->system_variables_are_placeholder;
    out_snapshot->big_tables = session->big_tables;
    out_snapshot->foreign_key_checks_enabled = session->foreign_key_checks_enabled;
    out_snapshot->sql_require_primary_key = session->sql_require_primary_key;
    out_snapshot->has_next_transaction_isolation = session->has_next_transaction_isolation;
    out_snapshot->has_next_transaction_access_mode = session->has_next_transaction_access_mode;
    out_snapshot->next_transaction_isolation_from_system_variable =
        session->next_transaction_isolation_from_system_variable;
    out_snapshot->next_transaction_access_mode_from_system_variable =
        session->next_transaction_access_mode_from_system_variable;
    out_snapshot->has_timestamp_override = session->has_timestamp_override;
    memcpy(out_snapshot->time_zone, session->time_zone, sizeof(out_snapshot->time_zone));
    memcpy(
        out_snapshot->sql_mode_text,
        session->sql_mode_text,
        sizeof(out_snapshot->sql_mode_text)
    );
    out_snapshot->user_variables = variables;
    out_snapshot->user_variable_count = session->user_variable_count;
    out_snapshot->user_variable_capacity = session->user_variable_capacity;
    return MYLITE_OK;
}

static int copy_session_user_variables(
    struct mylite_db *database,
    const struct mylite_session_state *source,
    struct mylite_session_user_variable **out_variables
) {
    struct mylite_session_user_variable *copy = NULL;
    const struct mylite_session_user_variable *variables = NULL;
    size_t count = 0U;
    size_t capacity = 0U;

    if (source == NULL || out_variables == NULL) {
        return MYLITE_MISUSE;
    }
    *out_variables = NULL;
    variables = source->user_variables;
    count = source->user_variable_count;
    capacity = source->user_variable_capacity;
    if (capacity == 0U) {
        return MYLITE_OK;
    }
    if (capacity > SIZE_MAX / sizeof(*copy)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    copy = (struct mylite_session_user_variable *)calloc(capacity, sizeof(*copy));
    if (copy == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    for (size_t index = 0U; index < count; ++index) {
        copy[index] = variables[index];
        copy[index].value = NULL;
        if (!variables[index].is_null && variables[index].value != NULL) {
            copy[index].value = (char *)malloc(variables[index].value_size + 1U);
            if (copy[index].value == NULL) {
                free_session_user_variables(copy, index);
                set_nomem_error(database);
                return MYLITE_NOMEM;
            }
            memcpy(copy[index].value, variables[index].value, variables[index].value_size);
            copy[index].value[variables[index].value_size] = '\0';
        }
    }

    *out_variables = copy;
    return MYLITE_OK;
}

static void restore_set_session_snapshot(
    struct mylite_db *database,
    struct set_session_snapshot *snapshot
) {
    struct mylite_session_state *session = database == NULL ? NULL : &database->session;
    struct mylite_session_user_variable *current_variables = NULL;
    size_t current_variable_count = 0U;

    if (session == NULL || snapshot == NULL) {
        return;
    }

    current_variables = session->user_variables;
    current_variable_count = session->user_variable_count;

    session->sql_mode = snapshot->sql_mode;
    session->auto_increment_increment = snapshot->auto_increment_increment;
    session->auto_increment_offset = snapshot->auto_increment_offset;
    session->sql_select_limit = snapshot->sql_select_limit;
    session->group_concat_max_len = snapshot->group_concat_max_len;
    session->information_schema_stats_expiry = snapshot->information_schema_stats_expiry;
    session->wait_timeout = snapshot->wait_timeout;
    session->interactive_timeout = snapshot->interactive_timeout;
    session->timestamp_override = snapshot->timestamp_override;
    session->time_zone_offset_minutes = snapshot->time_zone_offset_minutes;
    session->session_transaction_isolation = snapshot->session_transaction_isolation;
    session->session_transaction_access_mode = snapshot->session_transaction_access_mode;
    session->next_transaction_isolation = snapshot->next_transaction_isolation;
    session->next_transaction_access_mode = snapshot->next_transaction_access_mode;
    session->sql_mode_is_placeholder = snapshot->sql_mode_is_placeholder;
    session->time_zone_is_placeholder = snapshot->time_zone_is_placeholder;
    session->system_variables_are_placeholder = snapshot->system_variables_are_placeholder;
    session->big_tables = snapshot->big_tables;
    session->foreign_key_checks_enabled = snapshot->foreign_key_checks_enabled;
    session->sql_require_primary_key = snapshot->sql_require_primary_key;
    session->has_next_transaction_isolation = snapshot->has_next_transaction_isolation;
    session->has_next_transaction_access_mode = snapshot->has_next_transaction_access_mode;
    session->next_transaction_isolation_from_system_variable =
        snapshot->next_transaction_isolation_from_system_variable;
    session->next_transaction_access_mode_from_system_variable =
        snapshot->next_transaction_access_mode_from_system_variable;
    session->has_timestamp_override = snapshot->has_timestamp_override;
    memcpy(session->time_zone, snapshot->time_zone, sizeof(session->time_zone));
    memcpy(session->sql_mode_text, snapshot->sql_mode_text, sizeof(session->sql_mode_text));
    session->user_variables = snapshot->user_variables;
    session->user_variable_count = snapshot->user_variable_count;
    session->user_variable_capacity = snapshot->user_variable_capacity;

    snapshot->user_variables = NULL;
    snapshot->user_variable_count = 0U;
    snapshot->user_variable_capacity = 0U;
    free_session_user_variables(current_variables, current_variable_count);
}

static void deinit_set_session_snapshot(struct set_session_snapshot *snapshot) {
    if (snapshot == NULL) {
        return;
    }
    free_session_user_variables(snapshot->user_variables, snapshot->user_variable_count);
    *snapshot = (struct set_session_snapshot){0};
}

static void free_session_user_variables(
    struct mylite_session_user_variable *variables,
    size_t count
) {
    if (variables == NULL) {
        return;
    }
    for (size_t index = 0U; index < count; ++index) {
        free(variables[index].value);
    }
    free(variables);
}
