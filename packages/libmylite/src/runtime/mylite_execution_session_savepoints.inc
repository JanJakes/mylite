static void clear_user_savepoints(struct mylite_db *database) {
    if (database == NULL) {
        return;
    }

    database->session.savepoint_count = 0U;
}

static int create_or_replace_user_savepoint(struct mylite_db *database, const char *name) {
    char sqlite_name[MYLITE_SESSION_SAVEPOINT_INTERNAL_NAME_CAPACITY];
    size_t existing_index = 0U;
    bool has_existing = false;
    int rc = MYLITE_OK;

    has_existing = find_user_savepoint(&database->session, name, &existing_index);
    rc = reserve_user_savepoints(database, database->session.savepoint_count + 1U);
    if (rc == MYLITE_OK) {
        rc = format_next_sqlite_savepoint_name(database, sqlite_name, sizeof(sqlite_name));
    }
    if (rc == MYLITE_OK) {
        rc = execute_sqlite_savepoint_control(database, "SAVEPOINT ", sqlite_name);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (has_existing) {
        remove_user_savepoint_at(&database->session, existing_index);
    }

    rc = append_user_savepoint(
        database,
        (struct user_savepoint_values){
            .name = name,
            .sqlite_name = sqlite_name,
        }
    );
    if (rc == MYLITE_OK) {
        ++database->session.next_savepoint_id;
    }
    return rc;
}

static int rollback_to_user_savepoint(struct mylite_db *database, const char *name) {
    size_t index = 0U;
    int rc = MYLITE_OK;

    if (!database->session.user_transaction_active ||
        !find_user_savepoint(&database->session, name, &index)) {
        set_savepoint_does_not_exist_error(database, name);
        return MYLITE_ERROR;
    }

    rc = execute_sqlite_savepoint_control(
        database,
        "ROLLBACK TO SAVEPOINT ",
        database->session.savepoints[index].sqlite_name
    );
    if (rc == MYLITE_OK) {
        database->session.savepoint_count = index + 1U;
        rc = reconcile_temporary_physical_tables_after_user_rollback(database);
    }
    return rc;
}

static int release_user_savepoint(struct mylite_db *database, const char *name) {
    size_t index = 0U;
    int rc = MYLITE_OK;

    if (!database->session.user_transaction_active ||
        !find_user_savepoint(&database->session, name, &index)) {
        set_savepoint_does_not_exist_error(database, name);
        return MYLITE_ERROR;
    }

    rc = execute_sqlite_savepoint_control(
        database,
        "RELEASE SAVEPOINT ",
        database->session.savepoints[index].sqlite_name
    );
    if (rc == MYLITE_OK) {
        database->session.savepoint_count = index;
    }
    return rc;
}

static int copy_savepoint_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    char *destination,
    size_t destination_size
) {
    return copy_identifier_text(node, destination, destination_size, database);
}

static void fold_savepoint_name(const char *source, char *destination, size_t destination_size) {
    size_t index = 0U;

    if (destination == NULL || destination_size == 0U) {
        return;
    }
    if (source == NULL) {
        destination[0] = '\0';
        return;
    }

    for (; index + 1U < destination_size && source[index] != '\0'; ++index) {
        destination[index] = ascii_lower((unsigned char)source[index]);
    }
    destination[index] = '\0';
}

static bool find_user_savepoint(
    const struct mylite_session_state *session,
    const char *name,
    size_t *out_index
) {
    char folded_name[MYLITE_SESSION_SAVEPOINT_NAME_CAPACITY];

    fold_savepoint_name(name, folded_name, sizeof(folded_name));
    for (size_t index = 0U; index < session->savepoint_count; ++index) {
        if (strcmp(session->savepoints[index].folded_name, folded_name) == 0) {
            if (out_index != NULL) {
                *out_index = index;
            }
            return true;
        }
    }
    return false;
}

static int reserve_user_savepoints(struct mylite_db *database, size_t required_capacity) {
    struct mylite_session_state *session = &database->session;
    struct mylite_session_savepoint *savepoints = NULL;
    size_t new_capacity = 0U;

    if (required_capacity <= session->savepoint_capacity) {
        return MYLITE_OK;
    }

    new_capacity = session->savepoint_capacity == 0U ? 4U : session->savepoint_capacity;
    while (new_capacity < required_capacity) {
        if (new_capacity > SIZE_MAX / 2U) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        new_capacity *= 2U;
    }

    savepoints = realloc(session->savepoints, new_capacity * sizeof(session->savepoints[0]));
    if (savepoints == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    session->savepoints = savepoints;
    session->savepoint_capacity = new_capacity;
    return MYLITE_OK;
}

static int format_next_sqlite_savepoint_name(
    struct mylite_db *database,
    char *destination,
    size_t destination_size
) {
    int written = 0;

    if (database->session.next_savepoint_id == UINT64_MAX) {
        set_runtime_error(database, "too many savepoints in this transaction");
        return MYLITE_ERROR;
    }

    written = snprintf(
        destination,
        destination_size,
        "_mylite_user_savepoint_%" PRIu64 "_%" PRIu64,
        database->session.connection_id,
        database->session.next_savepoint_id
    );
    if (written < 0 || (size_t)written >= destination_size) {
        set_runtime_error(database, "failed to format savepoint name");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int execute_sqlite_savepoint_control(
    struct mylite_db *database,
    const char *prefix,
    const char *sqlite_name
) {
    char *sql = NULL;
    int rc = build_savepoint_control_sql(
        (struct savepoint_control_sql_request){
            .prefix = prefix,
            .sqlite_name = sqlite_name,
        },
        &sql
    );

    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        } else {
            set_runtime_error(database, "failed to build savepoint control SQL");
        }
        return rc;
    }

    rc = normalize_sqlite_control_rc(database, execute_sqlite_control_sql(database, sql));
    free(sql);
    return rc;
}

static int build_savepoint_control_sql(
    struct savepoint_control_sql_request request,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, request.prefix);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, request.sqlite_name);
    }
    if (rc == MYLITE_OK) {
        *out_sql = mylite_dynamic_string_take(&string);
        return *out_sql == NULL ? MYLITE_NOMEM : MYLITE_OK;
    }

    mylite_dynamic_string_deinit(&string);
    return rc;
}

static void remove_user_savepoint_at(struct mylite_session_state *session, size_t index) {
    if (index >= session->savepoint_count) {
        return;
    }

    if (index + 1U < session->savepoint_count) {
        memmove(
            &session->savepoints[index],
            &session->savepoints[index + 1U],
            (session->savepoint_count - index - 1U) * sizeof(session->savepoints[0])
        );
    }
    --session->savepoint_count;
}

static int append_user_savepoint(struct mylite_db *database, struct user_savepoint_values values) {
    struct mylite_session_state *session = &database->session;
    struct mylite_session_savepoint *savepoint = NULL;
    int written = 0;

    if (session->savepoint_count >= session->savepoint_capacity) {
        set_runtime_error(database, "savepoint registry capacity was not reserved");
        return MYLITE_ERROR;
    }

    savepoint = &session->savepoints[session->savepoint_count];
    written = snprintf(savepoint->name, sizeof(savepoint->name), "%s", values.name);
    if (written < 0 || (size_t)written >= sizeof(savepoint->name)) {
        set_identifier_too_long_error(database, "savepoint");
        return MYLITE_ERROR;
    }
    fold_savepoint_name(savepoint->name, savepoint->folded_name, sizeof(savepoint->folded_name));
    written =
        snprintf(savepoint->sqlite_name, sizeof(savepoint->sqlite_name), "%s", values.sqlite_name);
    if (written < 0 || (size_t)written >= sizeof(savepoint->sqlite_name)) {
        set_runtime_error(database, "failed to store savepoint name");
        return MYLITE_ERROR;
    }

    ++session->savepoint_count;
    return MYLITE_OK;
}
