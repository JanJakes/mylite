
static int execute_show_triggers_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[show_triggers_result_column_count] = {
        "Trigger",
        "Event",
        "Table",
        "Statement",
        "Timing",
        "Created",
        "sql_mode",
        "Definer",
        "character_set_client",
        "collation_connection",
        "Database Collation",
    };
    struct mylite_catalog_schema_descriptor schema = {0};
    const struct mylite_sql_ast_node *first_child = child_at(statement, 0U);
    const struct mylite_sql_ast_node *second_child = child_at(statement, 1U);
    const struct mylite_sql_ast_node *schema_node = first_child;
    const struct mylite_sql_ast_node *like_node = second_child;
    struct show_like_filter filter = {
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    if (first_child != NULL && first_child->kind == MYLITE_SQL_AST_LITERAL) {
        schema_node = NULL;
        like_node = first_child;
    }
    if (schema_node == NULL) {
        rc = resolve_show_triggers_selected_schema(database, &schema);
    } else {
        char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];

        rc = copy_identifier_text(schema_node, schema_name, sizeof(schema_name), database);
        if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(schema_name)) {
            set_reserved_name_error(database, "database", schema_name);
            rc = MYLITE_ERROR;
        }
        if (rc == MYLITE_OK) {
            rc = resolve_show_triggers_schema_name(database, schema_name, &schema);
        }
    }
    if (rc == MYLITE_OK) {
        rc = make_show_like_filter(database, like_node, &filter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_triggers_result_column_count;
         ++column_index) {
        rc = append_show_result_column(database, result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_show_sys_sys_config_trigger_rows(database, result, &schema, &filter);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        show_like_filter_deinit(&filter);
        return rc;
    }

    show_like_filter_deinit(&filter);
    return finish_successful_result(database, result, out_result);
}

static int resolve_show_triggers_selected_schema(
    struct mylite_db *database,
    struct mylite_catalog_schema_descriptor *out_schema
) {
    if (database == NULL || !database->session.has_selected_schema) {
        set_no_database_error(database);
        return MYLITE_ERROR;
    }

    return resolve_show_triggers_schema_name(
        database,
        database->session.selected_schema,
        out_schema
    );
}

static int resolve_show_triggers_schema_name(
    struct mylite_db *database,
    const char *schema_name,
    struct mylite_catalog_schema_descriptor *out_schema
) {
    const struct mylite_execution_catalog_builtin_schema *builtin_schema =
        find_builtin_schema_descriptor(schema_name);

    if (builtin_schema != NULL) {
        int name_written = 0;
        int charset_written = 0;
        int collation_written = 0;

        *out_schema = (struct mylite_catalog_schema_descriptor){0};
        name_written =
            snprintf(out_schema->name, sizeof(out_schema->name), "%s", builtin_schema->name);
        charset_written = snprintf(
            out_schema->default_charset,
            sizeof(out_schema->default_charset),
            "%s",
            builtin_schema->default_charset
        );
        collation_written = snprintf(
            out_schema->default_collation,
            sizeof(out_schema->default_collation),
            "%s",
            builtin_schema->default_collation
        );

        if (name_written < 0 || (size_t)name_written >= sizeof(out_schema->name) ||
            charset_written < 0 || (size_t)charset_written >= sizeof(out_schema->default_charset) ||
            collation_written < 0 ||
            (size_t)collation_written >= sizeof(out_schema->default_collation)) {
            set_runtime_error(database, "failed to format builtin schema metadata");
            return MYLITE_ERROR;
        }
        return MYLITE_OK;
    }

    return resolve_schema_name(database, schema_name, out_schema);
}

static int append_show_sys_sys_config_trigger_rows(
    struct mylite_db *database,
    mylite_result *result,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct show_like_filter *filter
) {
    char created[timestamp_2_text_length + 1U];
    int rc = MYLITE_OK;

    if (schema == NULL || strcmp(schema->name, "sys") != 0 ||
        !show_like_filter_matches(filter, "sys_config", false)) {
        return MYLITE_OK;
    }

    rc = mysql_system_current_timestamp2(database, created, sizeof(created));
    for (size_t trigger_index = 0U;
         rc == MYLITE_OK && trigger_index < mylite_execution_catalog_sys_sys_config_trigger_count();
         ++trigger_index) {
        const struct mylite_execution_catalog_sys_config_trigger *trigger =
            mylite_execution_catalog_sys_sys_config_trigger_at(trigger_index);
        const char *values[show_triggers_result_column_count] = {
            trigger->name,
            trigger->event,
            "sys_config",
            mylite_execution_catalog_sys_sys_config_trigger_action_statement(),
            "BEFORE",
            created,
            mylite_execution_catalog_sys_sys_config_trigger_sql_mode(),
            "mysql.sys@localhost",
            "utf8mb4",
            "utf8mb4_0900_ai_ci",
            "utf8mb4_0900_ai_ci",
        };

        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    return rc;
}

static int execute_show_events_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[show_events_result_column_count] = {
        "Db",
        "Name",
        "Definer",
        "Time zone",
        "Type",
        "Execute at",
        "Interval value",
        "Interval field",
        "Starts",
        "Ends",
        "Status",
        "Originator",
        "character_set_client",
        "collation_connection",
        "Database Collation",
    };
    const struct mylite_sql_ast_node *first_child = child_at(statement, 0U);
    const struct mylite_sql_ast_node *second_child = child_at(statement, 1U);
    const struct mylite_sql_ast_node *schema_node = first_child;
    const struct mylite_sql_ast_node *filter_node = second_child;
    const struct mylite_sql_ast_node *like_node = NULL;
    struct show_like_filter filter = {
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    if (first_child != NULL && (first_child->kind == MYLITE_SQL_AST_LITERAL ||
                                first_child->kind == MYLITE_SQL_AST_WHERE_CLAUSE)) {
        schema_node = NULL;
        filter_node = first_child;
    }
    if (filter_node != NULL && filter_node->kind == MYLITE_SQL_AST_LITERAL) {
        like_node = filter_node;
    }
    if (schema_node == NULL) {
        struct mylite_catalog_schema_descriptor schema = {0};

        rc = resolve_selected_schema(database, &schema);
    } else {
        char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];

        rc = copy_identifier_text(schema_node, schema_name, sizeof(schema_name), database);
        if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(schema_name)) {
            set_reserved_name_error(database, "database", schema_name);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        rc = make_show_like_filter(database, like_node, &filter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_events_result_column_count;
         ++column_index) {
        rc = append_show_result_column(database, result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        show_like_filter_deinit(&filter);
        return rc;
    }

    show_like_filter_deinit(&filter);
    return finish_successful_result(database, result, out_result);
}

static int execute_show_open_tables_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[show_open_tables_result_column_count] = {
        "Database",
        "Table",
        "In_use",
        "Name_locked",
    };
    const struct mylite_sql_ast_node *first_child = child_at(statement, 0U);
    const struct mylite_sql_ast_node *second_child = child_at(statement, 1U);
    const struct mylite_sql_ast_node *schema_node = first_child;
    const struct mylite_sql_ast_node *filter_node = second_child;
    const struct mylite_sql_ast_node *like_node = NULL;
    struct show_like_filter filter = {
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    if (first_child != NULL && (first_child->kind == MYLITE_SQL_AST_LITERAL ||
                                first_child->kind == MYLITE_SQL_AST_WHERE_CLAUSE)) {
        schema_node = NULL;
        filter_node = first_child;
    }
    if (filter_node != NULL && filter_node->kind == MYLITE_SQL_AST_LITERAL) {
        like_node = filter_node;
    }
    if (schema_node != NULL) {
        char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];

        rc = copy_identifier_text(schema_node, schema_name, sizeof(schema_name), database);
        if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(schema_name)) {
            set_reserved_name_error(database, "database", schema_name);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        rc = make_show_like_filter(database, like_node, &filter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_open_tables_result_column_count;
         ++column_index) {
        rc = append_show_result_column(database, result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        show_like_filter_deinit(&filter);
        return rc;
    }

    show_like_filter_deinit(&filter);
    return finish_successful_result(database, result, out_result);
}

static int execute_show_routine_status_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[show_routine_status_result_column_count] = {
        "Db",
        "Name",
        "Type",
        "Language",
        "Definer",
        "Modified",
        "Created",
        "Security_type",
        "Comment",
        "character_set_client",
        "collation_connection",
        "Database Collation",
    };
    const struct mylite_sql_ast_node *like_node = child_at(statement, 0U);
    struct show_like_filter filter = {
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    if (like_node != NULL && like_node->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
        like_node = NULL;
    }

    rc = make_show_like_filter(database, like_node, &filter);
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_routine_status_result_column_count;
         ++column_index) {
        rc = append_show_result_column(database, result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        show_like_filter_deinit(&filter);
        return rc;
    }

    show_like_filter_deinit(&filter);
    return finish_successful_result(database, result, out_result);
}

static int execute_show_processlist_statement(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[show_processlist_result_column_count] = {
        "Id",
        "User",
        "Host",
        "db",
        "Command",
        "Time",
        "State",
        "Info",
    };
    static const struct show_result_column_definition
        result_column_definitions[show_processlist_result_column_count] = {
            {MYLITE_RESULT_LOGICAL_TYPE_LONGLONG,
             MYLITE_RESULT_COLUMN_FLAG_NOT_NULL | MYLITE_RESULT_COLUMN_FLAG_BINARY |
                 MYLITE_RESULT_COLUMN_FLAG_NUM,
             22U,
             0U,
             false,
             false},
            {MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING,
             MYLITE_RESULT_COLUMN_FLAG_NOT_NULL,
             32U,
             31U,
             false,
             true},
            {MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING,
             MYLITE_RESULT_COLUMN_FLAG_NOT_NULL,
             255U,
             31U,
             false,
             true},
            {MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING, 0U, 64U, 31U, true, true},
            {MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING,
             MYLITE_RESULT_COLUMN_FLAG_NOT_NULL,
             16U,
             31U,
             false,
             true},
            {MYLITE_RESULT_LOGICAL_TYPE_LONG,
             MYLITE_RESULT_COLUMN_FLAG_NOT_NULL | MYLITE_RESULT_COLUMN_FLAG_BINARY |
                 MYLITE_RESULT_COLUMN_FLAG_NUM,
             8U,
             0U,
             false,
             false},
            {MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING, 0U, 30U, 31U, true, true},
            {MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING, 0U, 100U, 31U, true, true},
        };
    mylite_result *result = NULL;
    struct mylite_processlist_session_snapshot *sessions = NULL;
    size_t session_count = 0U;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }
    if (rc == MYLITE_OK) {
        rc = append_show_result_columns(
            database,
            result,
            result_columns,
            result_column_definitions,
            show_processlist_result_column_count
        );
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_connection_collect_processlist_sessions(database, &sessions, &session_count);
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        } else if (rc != MYLITE_OK) {
            set_runtime_error(database, "failed to collect SHOW PROCESSLIST sessions");
        }
    }
    for (size_t session_index = 0U; rc == MYLITE_OK && session_index < session_count;
         ++session_index) {
        rc = append_show_processlist_row(
            database,
            context,
            statement,
            &sessions[session_index],
            result
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_show_processlist_warning(database);
    }
    if (rc != MYLITE_OK) {
        free(sessions);
        mylite_result_free(result);
        return rc;
    }

    free(sessions);
    return finish_successful_result(database, result, out_result);
}

static int append_show_processlist_row(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    const struct mylite_processlist_session_snapshot *session,
    mylite_result *result
) {
    char connection_id_text[integer_text_capacity];
    char user[MYLITE_SESSION_IDENTIFIER_CAPACITY];
    char host[MYLITE_SESSION_IDENTIFIER_CAPACITY];
    char *info = NULL;
    const char *selected_schema = NULL;
    int written = 0;
    int rc = MYLITE_OK;

    const char *values[show_processlist_result_column_count] = {
        connection_id_text,
        user,
        host,
        NULL,
        session->is_current ? "Query" : "Sleep",
        "0",
        session->is_current ? "init" : NULL,
        NULL,
    };

    written = snprintf(
        connection_id_text,
        sizeof(connection_id_text),
        "%" PRIu64,
        session->connection_id
    );
    if (written < 0 || (size_t)written >= sizeof(connection_id_text)) {
        set_runtime_error(database, "failed to format SHOW PROCESSLIST Id");
        return MYLITE_ERROR;
    }
    if (session->has_selected_schema) {
        selected_schema = session->selected_schema;
    }
    values[show_processlist_db_column] = selected_schema;

    rc = format_processlist_user_host(
        database,
        session->client_user_identity,
        user,
        sizeof(user),
        host,
        sizeof(host)
    );
    if (rc == MYLITE_OK && session->is_current) {
        rc = copy_show_processlist_info(database, context, statement, &info);
    }
    if (rc == MYLITE_OK) {
        values[show_processlist_info_column] = info;
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }

    free(info);
    return rc;
}

static int format_processlist_user_host(
    struct mylite_db *database,
    const char *identity,
    char *user,
    size_t user_size,
    char *host,
    size_t host_size
) {
    const char *at_sign = strchr(identity, '@');
    const char *host_part = at_sign == NULL ? "" : at_sign + 1;
    size_t user_length = at_sign == NULL ? strlen(identity) : (size_t)(at_sign - identity);
    size_t host_length = strlen(host_part);

    if (user_length >= user_size || host_length >= host_size) {
        set_runtime_error(database, "failed to format SHOW PROCESSLIST user identity");
        return MYLITE_ERROR;
    }

    memcpy(user, identity, user_length);
    user[user_length] = '\0';
    memcpy(host, host_part, host_length);
    host[host_length] = '\0';

    return MYLITE_OK;
}

static int copy_show_processlist_info(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    char **out_info
) {
    const char *sql = mylite_statement_context_sql(context);
    size_t sql_size = mylite_statement_context_sql_size(context);
    size_t length = 0U;
    char *info = NULL;

    if (out_info == NULL) {
        return MYLITE_MISUSE;
    }
    *out_info = NULL;
    if (sql == NULL) {
        set_runtime_error(database, "failed to read SHOW PROCESSLIST statement text");
        return MYLITE_ERROR;
    }

    length = statement_info_length_without_terminator(sql, sql_size);
    if (statement->kind != MYLITE_SQL_AST_SHOW_FULL_PROCESSLIST_STATEMENT &&
        length > show_processlist_info_truncation_length) {
        length = show_processlist_info_truncation_length;
    }
    if (length == SIZE_MAX) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    info = (char *)malloc(length + 1U);
    if (info == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (length > 0U) {
        memcpy(info, sql, length);
    }
    info[length] = '\0';
    *out_info = info;

    return MYLITE_OK;
}

static size_t statement_info_length_without_terminator(const char *sql, size_t sql_size) {
    size_t length = sql_size;

    while (length > 0U &&
           (sql[length - 1U] == ' ' || sql[length - 1U] == '\t' || sql[length - 1U] == '\n' ||
            sql[length - 1U] == '\r' || sql[length - 1U] == '\f' || sql[length - 1U] == '\v')) {
        --length;
    }
    if (length > 0U && sql[length - 1U] == ';') {
        --length;
    }
    while (length > 0U &&
           (sql[length - 1U] == ' ' || sql[length - 1U] == '\t' || sql[length - 1U] == '\n' ||
            sql[length - 1U] == '\r' || sql[length - 1U] == '\f' || sql[length - 1U] == '\v')) {
        --length;
    }

    return length;
}

static int append_show_processlist_warning(struct mylite_db *database) {
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_information_schema_processlist_deprecated,
        "HY000",
        "'INFORMATION_SCHEMA.PROCESSLIST' is deprecated and will be removed in a future "
        "release. Please use performance_schema.processlist instead"
    );
}

static int append_information_schema_profiling_warning(struct mylite_db *database) {
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_information_schema_profiling_deprecated,
        "HY000",
        "'INFORMATION_SCHEMA.PROFILING' is deprecated and will be removed in a future release. "
        "Please use Performance Schema instead"
    );
}

static int execute_show_grants_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[show_grants_result_column_count] = {
        "Grants for root@%",
    };
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    if (!show_grants_statement_targets_embedded_root(statement)) {
        return set_show_grants_no_such_grant_error(database, statement);
    }
    if (show_grants_statement_role_list(statement) != NULL) {
        return set_show_grants_role_not_granted_error(database, statement);
    }

    rc = mylite_result_create(&result);
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_grants_result_column_count;
         ++column_index) {
        rc = append_show_result_column(database, result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t row_index = 0U;
         rc == MYLITE_OK && row_index < sizeof(show_grants_embedded_root_rows) /
                                            sizeof(show_grants_embedded_root_rows[0]);
         ++row_index) {
        const char *const values[show_grants_result_column_count] = {
            show_grants_embedded_root_rows[row_index],
        };

        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static bool show_grants_statement_targets_embedded_root(const struct mylite_sql_ast_node *statement
) {
    return show_grants_target_is_embedded_root(show_grants_statement_target(statement));
}

static const struct mylite_sql_ast_node *show_grants_statement_target(
    const struct mylite_sql_ast_node *statement
) {
    return child_at(statement, 0U);
}

static const struct mylite_sql_ast_node *show_grants_statement_role_list(
    const struct mylite_sql_ast_node *statement
) {
    return child_at(statement, 1U);
}

static bool show_grants_target_is_embedded_root(const struct mylite_sql_ast_node *target) {
    const struct mylite_sql_ast_node *user = NULL;
    const struct mylite_sql_ast_node *host = NULL;

    if (target == NULL) {
        return true;
    }
    if (target->kind == MYLITE_SQL_AST_CURRENT_USER_FUNCTION) {
        return true;
    }
    if (target->kind != MYLITE_SQL_AST_SHOW_GRANTS_ACCOUNT) {
        return false;
    }
    user = child_at(target, 0U);
    host = child_at(target, 1U);
    if (user == NULL) {
        return false;
    }

    return show_grants_user_span_equals(&user->span, "root") &&
           show_grants_host_span_equals(host == NULL ? NULL : &host->span, "%");
}

static int set_show_grants_no_such_grant_error(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
) {
    char user[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char host[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    const struct mylite_sql_ast_node *target = show_grants_statement_target(statement);
    const struct mylite_sql_ast_node *user_node = child_at(target, 0U);
    const struct mylite_sql_ast_node *host_node = child_at(target, 1U);
    int written = 0;

    if (copy_show_grants_account_part(
            user_node == NULL ? NULL : &user_node->span,
            false,
            "",
            user,
            sizeof(user)
        ) != MYLITE_OK ||
        copy_show_grants_account_part(
            host_node == NULL ? NULL : &host_node->span,
            true,
            "%",
            host,
            sizeof(host)
        ) != MYLITE_OK) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    written = snprintf(
        message,
        sizeof(message),
        "There is no such grant defined for user '%s' on host '%s'",
        user,
        host
    );
    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_no_such_grant,
        "42000",
        message
    );
    return MYLITE_ERROR;
}

static int set_show_grants_role_not_granted_error(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
) {
    char role_user[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char role_host[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    const struct mylite_sql_ast_node *role_list = show_grants_statement_role_list(statement);
    const struct mylite_sql_ast_node *role = child_at(role_list, 0U);
    const struct mylite_sql_ast_node *role_user_node = child_at(role, 0U);
    const struct mylite_sql_ast_node *role_host_node = child_at(role, 1U);
    int written = 0;

    if (copy_show_grants_account_part(
            role_user_node == NULL ? NULL : &role_user_node->span,
            false,
            "",
            role_user,
            sizeof(role_user)
        ) != MYLITE_OK ||
        copy_show_grants_account_part(
            role_host_node == NULL ? NULL : &role_host_node->span,
            true,
            "%",
            role_host,
            sizeof(role_host)
        ) != MYLITE_OK) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    written = snprintf(
        message,
        sizeof(message),
        "`%s`@`%s` is not granted to `root`@`%%`",
        role_user,
        role_host
    );
    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_role_not_granted,
        "HY000",
        message
    );
    return MYLITE_ERROR;
}

static bool show_grants_user_span_equals(
    const struct mylite_sql_source_span *span,
    const char *expected
) {
    return show_grants_quoted_span_text_matches(span, 0U, expected);
}

static bool show_grants_host_span_equals(
    const struct mylite_sql_source_span *span,
    const char *expected
) {
    if (span == NULL) {
        return expected != NULL && strcmp(expected, "%") == 0;
    }
    return show_grants_quoted_span_text_matches(span, 1U, expected);
}

static bool show_grants_quoted_span_text_matches(
    const struct mylite_sql_source_span *span,
    size_t offset,
    const char *expected
) {
    const char *text = NULL;
    size_t expected_length = 0U;
    size_t length = 0U;
    char quote = '\0';

    if (span == NULL || span->text == NULL || span->length <= offset || expected == NULL) {
        return false;
    }
    expected_length = strlen(expected);
    text = span->text + offset;
    length = span->length - offset;
    quote = text[0];
    if ((quote == '\'' || quote == '"' || quote == '`') && length >= 2U &&
        text[length - 1U] == quote) {
        ++text;
        length -= 2U;
    }
    return length == expected_length && memcmp(text, expected, expected_length) == 0;
}

static int copy_show_grants_account_part(
    const struct mylite_sql_source_span *span,
    bool skip_at,
    const char *default_text,
    char *buffer,
    size_t buffer_size
) {
    const char *text = NULL;
    size_t length = 0U;
    size_t offset = skip_at ? 1U : 0U;
    char quote = '\0';

    if (buffer == NULL || buffer_size == 0U) {
        return MYLITE_NOMEM;
    }
    buffer[0] = '\0';
    if (span == NULL || span->text == NULL) {
        if (default_text != NULL) {
            (void)snprintf(buffer, buffer_size, "%s", default_text);
        }
        return MYLITE_OK;
    }
    if (span->length <= offset) {
        return MYLITE_OK;
    }

    text = span->text + offset;
    length = span->length - offset;
    quote = text[0];
    if ((quote == '\'' || quote == '"' || quote == '`') && length >= 2U &&
        text[length - 1U] == quote) {
        ++text;
        length -= 2U;
    }
    if (length >= buffer_size) {
        length = buffer_size - 1U;
    }
    memcpy(buffer, text, length);
    buffer[length] = '\0';
    return MYLITE_OK;
}

static int execute_show_privileges_statement(
    struct mylite_db *database,
    mylite_result **out_result
) {
    static const char *const result_columns[show_privileges_result_column_count] = {
        "Privilege",
        "Context",
        "Comment",
    };
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_privileges_result_column_count;
         ++column_index) {
        rc = append_show_result_column(database, result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t row_index = 0U; rc == MYLITE_OK && row_index < sizeof(show_privileges_rows) /
                                                                   sizeof(show_privileges_rows[0]);
         ++row_index) {
        const char *const values[show_privileges_result_column_count] = {
            show_privileges_rows[row_index].privilege,
            show_privileges_rows[row_index].context,
            show_privileges_rows[row_index].comment,
        };

        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}
