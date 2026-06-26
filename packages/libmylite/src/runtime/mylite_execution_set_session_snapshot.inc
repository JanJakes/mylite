static int copy_set_session_snapshot(
    struct mylite_db *database,
    struct set_session_snapshot *out_snapshot
) {
    const struct mylite_session_state *session = database == NULL ? NULL : &database->session;
    struct mylite_session_user_variable *variables = NULL;
    struct mylite_session_system_variable_override *overrides = NULL;
    int rc = MYLITE_OK;

    if (session == NULL || out_snapshot == NULL) {
        return MYLITE_MISUSE;
    }
    *out_snapshot = (struct set_session_snapshot){0};
    rc = copy_session_user_variables(database, session, &variables);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = copy_session_system_variable_overrides(database, session, &overrides);
    if (rc != MYLITE_OK) {
        free_session_user_variables(variables, session->user_variable_count);
        return rc;
    }

    out_snapshot->sql_mode = session->sql_mode;
    out_snapshot->auto_increment_increment = session->auto_increment_increment;
    out_snapshot->auto_increment_offset = session->auto_increment_offset;
    out_snapshot->sql_select_limit = session->sql_select_limit;
    out_snapshot->group_concat_max_len = session->group_concat_max_len;
    out_snapshot->information_schema_stats_expiry = session->information_schema_stats_expiry;
    out_snapshot->max_error_count = session->max_error_count;
    out_snapshot->wait_timeout = session->wait_timeout;
    out_snapshot->interactive_timeout = session->interactive_timeout;
    out_snapshot->long_query_time_microseconds = session->long_query_time_microseconds;
    out_snapshot->lock_wait_timeout = session->lock_wait_timeout;
    out_snapshot->net_read_timeout = session->net_read_timeout;
    out_snapshot->net_retry_count = session->net_retry_count;
    out_snapshot->net_write_timeout = session->net_write_timeout;
    out_snapshot->connection_memory_chunk_size = session->connection_memory_chunk_size;
    out_snapshot->connection_memory_limit = session->connection_memory_limit;
    out_snapshot->join_buffer_size = session->join_buffer_size;
    out_snapshot->sort_buffer_size = session->sort_buffer_size;
    out_snapshot->timestamp_override = session->timestamp_override;
    out_snapshot->time_zone_offset_minutes = session->time_zone_offset_minutes;
    out_snapshot->session_transaction_isolation = session->session_transaction_isolation;
    out_snapshot->session_transaction_access_mode = session->session_transaction_access_mode;
    out_snapshot->next_transaction_isolation = session->next_transaction_isolation;
    out_snapshot->next_transaction_access_mode = session->next_transaction_access_mode;
    out_snapshot->internal_tmp_mem_storage_engine = session->internal_tmp_mem_storage_engine;
    out_snapshot->sql_mode_is_placeholder = session->sql_mode_is_placeholder;
    out_snapshot->time_zone_is_placeholder = session->time_zone_is_placeholder;
    out_snapshot->character_set_state_is_placeholder = session->character_set_state_is_placeholder;
    out_snapshot->system_variables_are_placeholder = session->system_variables_are_placeholder;
    out_snapshot->big_tables = session->big_tables;
    out_snapshot->foreign_key_checks_enabled = session->foreign_key_checks_enabled;
    out_snapshot->sql_require_primary_key = session->sql_require_primary_key;
    out_snapshot->low_priority_updates = session->low_priority_updates;
    out_snapshot->autocommit_enabled = session->autocommit_enabled;
    out_snapshot->has_next_transaction_isolation = session->has_next_transaction_isolation;
    out_snapshot->has_next_transaction_access_mode = session->has_next_transaction_access_mode;
    out_snapshot->next_transaction_isolation_from_system_variable =
        session->next_transaction_isolation_from_system_variable;
    out_snapshot->next_transaction_access_mode_from_system_variable =
        session->next_transaction_access_mode_from_system_variable;
    out_snapshot->has_timestamp_override = session->has_timestamp_override;
    memcpy(out_snapshot->time_zone, session->time_zone, sizeof(out_snapshot->time_zone));
    memcpy(
        out_snapshot->character_set_client,
        session->character_set_client,
        sizeof(out_snapshot->character_set_client)
    );
    memcpy(
        out_snapshot->character_set_connection,
        session->character_set_connection,
        sizeof(out_snapshot->character_set_connection)
    );
    memcpy(
        out_snapshot->character_set_results,
        session->character_set_results,
        sizeof(out_snapshot->character_set_results)
    );
    memcpy(
        out_snapshot->collation_connection,
        session->collation_connection,
        sizeof(out_snapshot->collation_connection)
    );
    memcpy(
        out_snapshot->sql_mode_text,
        session->sql_mode_text,
        sizeof(out_snapshot->sql_mode_text)
    );
    out_snapshot->user_variables = variables;
    out_snapshot->user_variable_count = session->user_variable_count;
    out_snapshot->user_variable_capacity = session->user_variable_capacity;
    out_snapshot->system_variable_overrides = overrides;
    out_snapshot->system_variable_override_count = session->system_variable_override_count;
    out_snapshot->system_variable_override_capacity = session->system_variable_override_capacity;
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

static int copy_session_system_variable_overrides(
    struct mylite_db *database,
    const struct mylite_session_state *source,
    struct mylite_session_system_variable_override **out_overrides
) {
    struct mylite_session_system_variable_override *copy = NULL;
    const struct mylite_session_system_variable_override *overrides = NULL;
    size_t count = 0U;
    size_t capacity = 0U;

    if (source == NULL || out_overrides == NULL) {
        return MYLITE_MISUSE;
    }
    *out_overrides = NULL;
    overrides = source->system_variable_overrides;
    count = source->system_variable_override_count;
    capacity = source->system_variable_override_capacity;
    if (capacity == 0U) {
        return MYLITE_OK;
    }
    if (capacity > SIZE_MAX / sizeof(*copy)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    copy = (struct mylite_session_system_variable_override *)calloc(capacity, sizeof(*copy));
    if (copy == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    for (size_t index = 0U; index < count; ++index) {
        copy[index] = overrides[index];
        copy[index].value = NULL;
        if (overrides[index].value != NULL) {
            copy[index].value = (char *)malloc(overrides[index].value_size + 1U);
            if (copy[index].value == NULL) {
                free_session_system_variable_overrides(copy, index);
                set_nomem_error(database);
                return MYLITE_NOMEM;
            }
            memcpy(copy[index].value, overrides[index].value, overrides[index].value_size);
            copy[index].value[overrides[index].value_size] = '\0';
        }
    }

    *out_overrides = copy;
    return MYLITE_OK;
}

static void restore_set_session_snapshot(
    struct mylite_db *database,
    struct set_session_snapshot *snapshot
) {
    struct mylite_session_state *session = database == NULL ? NULL : &database->session;
    struct mylite_session_user_variable *current_variables = NULL;
    struct mylite_session_system_variable_override *current_overrides = NULL;
    size_t current_variable_count = 0U;
    size_t current_override_count = 0U;

    if (session == NULL || snapshot == NULL) {
        return;
    }

    current_variables = session->user_variables;
    current_variable_count = session->user_variable_count;
    current_overrides = session->system_variable_overrides;
    current_override_count = session->system_variable_override_count;

    session->sql_mode = snapshot->sql_mode;
    session->auto_increment_increment = snapshot->auto_increment_increment;
    session->auto_increment_offset = snapshot->auto_increment_offset;
    session->sql_select_limit = snapshot->sql_select_limit;
    session->group_concat_max_len = snapshot->group_concat_max_len;
    session->information_schema_stats_expiry = snapshot->information_schema_stats_expiry;
    session->max_error_count = snapshot->max_error_count;
    session->wait_timeout = snapshot->wait_timeout;
    session->interactive_timeout = snapshot->interactive_timeout;
    session->long_query_time_microseconds = snapshot->long_query_time_microseconds;
    session->lock_wait_timeout = snapshot->lock_wait_timeout;
    session->net_read_timeout = snapshot->net_read_timeout;
    session->net_retry_count = snapshot->net_retry_count;
    session->net_write_timeout = snapshot->net_write_timeout;
    session->connection_memory_chunk_size = snapshot->connection_memory_chunk_size;
    session->connection_memory_limit = snapshot->connection_memory_limit;
    session->join_buffer_size = snapshot->join_buffer_size;
    session->sort_buffer_size = snapshot->sort_buffer_size;
    session->timestamp_override = snapshot->timestamp_override;
    session->time_zone_offset_minutes = snapshot->time_zone_offset_minutes;
    session->session_transaction_isolation = snapshot->session_transaction_isolation;
    session->session_transaction_access_mode = snapshot->session_transaction_access_mode;
    session->next_transaction_isolation = snapshot->next_transaction_isolation;
    session->next_transaction_access_mode = snapshot->next_transaction_access_mode;
    session->internal_tmp_mem_storage_engine = snapshot->internal_tmp_mem_storage_engine;
    session->sql_mode_is_placeholder = snapshot->sql_mode_is_placeholder;
    session->time_zone_is_placeholder = snapshot->time_zone_is_placeholder;
    session->character_set_state_is_placeholder = snapshot->character_set_state_is_placeholder;
    session->system_variables_are_placeholder = snapshot->system_variables_are_placeholder;
    session->big_tables = snapshot->big_tables;
    session->foreign_key_checks_enabled = snapshot->foreign_key_checks_enabled;
    session->sql_require_primary_key = snapshot->sql_require_primary_key;
    session->low_priority_updates = snapshot->low_priority_updates;
    session->autocommit_enabled = snapshot->autocommit_enabled;
    session->has_next_transaction_isolation = snapshot->has_next_transaction_isolation;
    session->has_next_transaction_access_mode = snapshot->has_next_transaction_access_mode;
    session->next_transaction_isolation_from_system_variable =
        snapshot->next_transaction_isolation_from_system_variable;
    session->next_transaction_access_mode_from_system_variable =
        snapshot->next_transaction_access_mode_from_system_variable;
    session->has_timestamp_override = snapshot->has_timestamp_override;
    memcpy(session->time_zone, snapshot->time_zone, sizeof(session->time_zone));
    memcpy(
        session->character_set_client,
        snapshot->character_set_client,
        sizeof(session->character_set_client)
    );
    memcpy(
        session->character_set_connection,
        snapshot->character_set_connection,
        sizeof(session->character_set_connection)
    );
    memcpy(
        session->character_set_results,
        snapshot->character_set_results,
        sizeof(session->character_set_results)
    );
    memcpy(
        session->collation_connection,
        snapshot->collation_connection,
        sizeof(session->collation_connection)
    );
    memcpy(session->sql_mode_text, snapshot->sql_mode_text, sizeof(session->sql_mode_text));
    session->user_variables = snapshot->user_variables;
    session->user_variable_count = snapshot->user_variable_count;
    session->user_variable_capacity = snapshot->user_variable_capacity;
    session->system_variable_overrides = snapshot->system_variable_overrides;
    session->system_variable_override_count = snapshot->system_variable_override_count;
    session->system_variable_override_capacity = snapshot->system_variable_override_capacity;

    snapshot->user_variables = NULL;
    snapshot->user_variable_count = 0U;
    snapshot->user_variable_capacity = 0U;
    snapshot->system_variable_overrides = NULL;
    snapshot->system_variable_override_count = 0U;
    snapshot->system_variable_override_capacity = 0U;
    free_session_user_variables(current_variables, current_variable_count);
    free_session_system_variable_overrides(current_overrides, current_override_count);
}

static void deinit_set_session_snapshot(struct set_session_snapshot *snapshot) {
    if (snapshot == NULL) {
        return;
    }
    free_session_user_variables(snapshot->user_variables, snapshot->user_variable_count);
    free_session_system_variable_overrides(
        snapshot->system_variable_overrides,
        snapshot->system_variable_override_count
    );
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

static void free_session_system_variable_overrides(
    struct mylite_session_system_variable_override *overrides,
    size_t count
) {
    if (overrides == NULL) {
        return;
    }
    for (size_t index = 0U; index < count; ++index) {
        free(overrides[index].value);
    }
    free(overrides);
}
