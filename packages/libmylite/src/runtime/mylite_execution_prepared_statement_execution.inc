static int execute_prepare_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    char name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char *source_sql = NULL;
    mylite_stmt *native_statement = NULL;
    mylite_result *result = NULL;
    size_t source_sql_size = 0U;
    size_t parameter_count = 0U;
    size_t existing_index = 0U;
    unsigned int lexer_modes = lexer_modes_for_session_sql_mode(&database->session);
    int rc = MYLITE_OK;

    rc = copy_prepared_statement_name(database, child_at(statement, 0U), name, sizeof(name));
    if (rc == MYLITE_OK) {
        rc = prepare_statement_source_sql(
            database,
            child_at(statement, 1U),
            &source_sql,
            &source_sql_size
        );
    }
    if (rc == MYLITE_OK) {
        existing_index = find_session_prepared_statement_index(&database->session, name);
        if (existing_index != SIZE_MAX) {
            remove_session_prepared_statement_at(&database->session, existing_index);
        }
        rc = validate_prepared_statement_sql(
            database,
            source_sql,
            source_sql_size,
            lexer_modes,
            &parameter_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_prepare_buffered(database, source_sql, source_sql_size, &native_statement);
    }
    if (rc == MYLITE_OK && mylite_stmt_parameter_count(native_statement) != parameter_count) {
        rc = MYLITE_ERROR;
        set_internal_error_if_clear(
            database,
            rc,
            "prepared statement parameter count changed during preparation"
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = ensure_session_prepared_statement_capacity(database);
    }
    if (rc == MYLITE_OK) {
        struct mylite_session_prepared_statement *prepared =
            &database->session.prepared_statements[database->session.prepared_statement_count];

        *prepared = (struct mylite_session_prepared_statement){0};
        memcpy(prepared->name, name, strlen(name) + 1U);
        prepared->statement = native_statement;
        prepared->lexer_modes = lexer_modes;
        prepared->has_selected_schema = database->session.has_selected_schema;
        (void)snprintf(
            prepared->selected_schema,
            sizeof(prepared->selected_schema),
            "%s",
            database->session.selected_schema
        );
        (void)snprintf(
            prepared->character_set_client,
            sizeof(prepared->character_set_client),
            "%s",
            database->session.character_set_client
        );
        (void)snprintf(
            prepared->character_set_connection,
            sizeof(prepared->character_set_connection),
            "%s",
            database->session.character_set_connection
        );
        (void)snprintf(
            prepared->collation_connection,
            sizeof(prepared->collation_connection),
            "%s",
            database->session.collation_connection
        );
        native_statement = NULL;
        ++database->session.prepared_statement_count;
    }

    free(source_sql);
    (void)mylite_stmt_finalize(native_statement);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int execute_execute_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    char name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    struct mylite_session_prepared_statement *prepared = NULL;
    const struct mylite_sql_ast_node *using_list = NULL;
    size_t parameter_count = 0U;
    int rc = copy_prepared_statement_name(database, child_at(statement, 0U), name, sizeof(name));

    if (rc != MYLITE_OK) {
        return rc;
    }

    prepared = find_session_prepared_statement(&database->session, name);
    if (prepared == NULL) {
        set_unknown_prepared_statement_error(database, name, "EXECUTE");
        return MYLITE_ERROR;
    }

    using_list = child_at(statement, 1U);
    parameter_count = using_list == NULL ? 0U : mylite_sql_ast_node_child_count(using_list);
    if (parameter_count != mylite_stmt_parameter_count(prepared->statement)) {
        set_prepared_statement_argument_count_error(database);
        return MYLITE_ERROR;
    }

    return execute_session_prepared_statement(database, prepared, using_list, out_result);
}

static int execute_session_prepared_statement(
    struct mylite_db *database,
    struct mylite_session_prepared_statement *prepared,
    const struct mylite_sql_ast_node *using_list,
    mylite_result **out_result
) {
    const struct mylite_sql_ast_node *statement = NULL;
    const struct mylite_session_prepared_statement *saved_active_prepared_statement = NULL;
    bool saved_has_selected_schema = false;
    bool saved_selected_schema_is_drop_target = false;
    char saved_selected_schema[MYLITE_SESSION_SCHEMA_CAPACITY] = {0};
    char dropped_schema[MYLITE_CATALOG_IDENTIFIER_CAPACITY] = {0};
    int rc = MYLITE_OK;

    if (database == NULL || prepared == NULL || prepared->statement == NULL || out_result == NULL) {
        return MYLITE_MISUSE;
    }
    *out_result = NULL;
    statement = prepared->statement->statement;
    saved_active_prepared_statement = database->active_prepared_statement;
    saved_has_selected_schema = database->session.has_selected_schema;

    (void)snprintf(
        saved_selected_schema,
        sizeof(saved_selected_schema),
        "%s",
        database->session.selected_schema
    );
    if (saved_has_selected_schema && statement != NULL &&
        statement->kind == MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT &&
        copy_identifier_text(
            child_at(statement, 0U),
            dropped_schema,
            sizeof(dropped_schema),
            database
        ) == MYLITE_OK) {
        saved_selected_schema_is_drop_target =
            text_equals_ascii_case_insensitive(saved_selected_schema, dropped_schema);
    }

    database->active_prepared_statement = prepared;
    database->session.has_selected_schema = prepared->has_selected_schema;
    (void)snprintf(
        database->session.selected_schema,
        sizeof(database->session.selected_schema),
        "%s",
        prepared->selected_schema
    );

    rc = mylite_stmt_reset(prepared->statement);
    if (rc == MYLITE_OK) {
        rc = mylite_stmt_clear_bindings(prepared->statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_execute_parameters(database, prepared->statement, using_list);
    }
    if (rc == MYLITE_OK) {
        rc = transfer_executed_prepared_statement_result(prepared->statement, out_result);
    }

    database->session.has_selected_schema =
        saved_has_selected_schema && !(rc == MYLITE_OK && saved_selected_schema_is_drop_target);
    if (database->session.has_selected_schema) {
        (void)snprintf(
            database->session.selected_schema,
            sizeof(database->session.selected_schema),
            "%s",
            saved_selected_schema
        );
    } else {
        database->session.selected_schema[0] = '\0';
    }
    database->active_prepared_statement = saved_active_prepared_statement;
    return rc;
}

static int bind_execute_parameters(
    struct mylite_db *database,
    mylite_stmt *statement,
    const struct mylite_sql_ast_node *using_list
) {
    const struct mylite_sql_ast_node *variable =
        using_list == NULL ? NULL : using_list->first_child;
    size_t parameter_index = 0U;
    int rc = MYLITE_OK;

    while (variable != NULL && rc == MYLITE_OK) {
        rc = bind_execute_user_variable(database, statement, parameter_index, variable);
        variable = variable->next_sibling;
        ++parameter_index;
    }
    return rc;
}

static int bind_execute_user_variable(
    struct mylite_db *database,
    mylite_stmt *statement,
    size_t parameter_index,
    const struct mylite_sql_ast_node *variable
) {
    char name[MYLITE_SESSION_USER_VARIABLE_NAME_CAPACITY];
    struct mylite_session_user_variable *value = NULL;
    int rc = copy_user_variable_name(database, variable, name, sizeof(name), true);

    if (rc != MYLITE_OK) {
        return rc;
    }
    value = find_session_user_variable(&database->session, name);
    if (value == NULL || value->is_null || value->value == NULL) {
        return mylite_stmt_bind_null(statement, parameter_index);
    }
    if (value->value_kind == MYLITE_SESSION_USER_VARIABLE_VALUE_INTEGER &&
        text_is_decimal_integer_literal(value->value, value->value_size)) {
        return bind_execute_integer_user_variable(statement, parameter_index, value);
    }
    return mylite_stmt_bind_text(statement, parameter_index, value->value, value->value_size);
}

static int bind_execute_integer_user_variable(
    mylite_stmt *statement,
    size_t parameter_index,
    const struct mylite_session_user_variable *variable
) {
    char *end = NULL;

    errno = 0;
    if (variable->value[0] == '-') {
        intmax_t value = strtoimax(variable->value, &end, decimal_base);

        if (errno == 0 && end == variable->value + variable->value_size && value >= INT64_MIN &&
            value <= INT64_MAX) {
            return mylite_stmt_bind_int64(statement, parameter_index, (int64_t)value);
        }
    } else {
        uintmax_t value = strtoumax(variable->value, &end, decimal_base);

        if (errno == 0 && end == variable->value + variable->value_size && value <= UINT64_MAX) {
            return mylite_stmt_bind_uint64(statement, parameter_index, (uint64_t)value);
        }
    }

    return mylite_stmt_bind_text(statement, parameter_index, variable->value, variable->value_size);
}

static int transfer_executed_prepared_statement_result(
    mylite_stmt *statement,
    mylite_result **out_result
) {
    int rc = start_cursor_execution(statement);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (statement->metadata_result != NULL) {
        *out_result = statement->metadata_result;
        statement->metadata_result = NULL;
        statement->has_materialized_rows = false;
    } else if (statement->completed_result != NULL) {
        *out_result = statement->completed_result;
        statement->completed_result = NULL;
    } else {
        set_runtime_error(statement->database, "prepared statement produced no result");
        return MYLITE_ERROR;
    }

    statement->current_row_available = false;
    statement->done = true;
    return MYLITE_OK;
}

static int execute_expanded_prepared_statement_sql(
    struct mylite_db *database,
    const char *sql,
    size_t sql_size,
    const struct mylite_session_prepared_statement *prepared,
    mylite_result **out_result
) {
    struct mylite_statement_context context;
    struct mylite_sql_parse_result parse_result;
    const struct mylite_sql_ast_node *statement = NULL;
    int64_t completed_row_count = -1;
    size_t statement_count = 0U;
    bool preserve_diagnostics_snapshot = false;
    bool completed_statement_is_select = false;
    bool saved_has_selected_schema = false;
    bool saved_selected_schema_is_drop_target = false;
    char saved_selected_schema[MYLITE_SESSION_SCHEMA_CAPACITY] = {0};
    char dropped_schema[MYLITE_CATALOG_IDENTIFIER_CAPACITY] = {0};
    const struct mylite_session_prepared_statement *saved_active_prepared_statement = NULL;
    int rc = MYLITE_OK;

    if (database == NULL || sql == NULL || out_result == NULL) {
        return MYLITE_MISUSE;
    }
    *out_result = NULL;

    mylite_statement_context_init(&context);
    rc = mylite_statement_context_begin(&context, database, sql, sql_size);
    if (rc != MYLITE_OK) {
        mylite_statement_context_deinit(&context);
        return rc;
    }
    mylite_statement_context_set_previous_row_count(&context, database->session.previous_row_count);
    mylite_statement_context_set_previous_found_rows(&context, database->session.found_rows);

    rc = status_from_parse_status(mylite_sql_parse(
        (struct mylite_sql_parse_config){
            .input = sql,
            .length = sql_size,
            .modes = prepared == NULL ? lexer_modes_for_session_sql_mode(&database->session)
                                      : prepared->lexer_modes,
        },
        &parse_result
    ));
    if (rc != MYLITE_OK) {
        rc = finish_parse_failure(database, &parse_result, rc);
        mylite_sql_parse_result_deinit(&parse_result);
        (void)mylite_statement_context_end(&context, rc);
        mylite_statement_context_deinit(&context);
        return rc;
    }

    rc = script_statement_count(parse_result.root, &statement_count);
    if (rc == MYLITE_OK && statement_count == 0U) {
        rc = execute_empty_statement(database, out_result);
    } else if (rc == MYLITE_OK && statement_count == 1U) {
        statement = child_at(parse_result.root, 0U);
        if (prepared_statement_disallows_statement(statement)) {
            set_prepared_statement_command_unsupported_error(database);
            rc = MYLITE_ERROR;
        }
    } else if (rc == MYLITE_OK) {
        set_unsupported_error(database, "multiple statements are not supported");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && statement != NULL) {
        saved_active_prepared_statement = database->active_prepared_statement;
        database->active_prepared_statement = prepared;
        if (prepared != NULL) {
            saved_has_selected_schema = database->session.has_selected_schema;
            (void)snprintf(
                saved_selected_schema,
                sizeof(saved_selected_schema),
                "%s",
                database->session.selected_schema
            );
            if (saved_has_selected_schema &&
                statement->kind == MYLITE_SQL_AST_DROP_SCHEMA_STATEMENT &&
                copy_identifier_text(
                    child_at(statement, 0U),
                    dropped_schema,
                    sizeof(dropped_schema),
                    database
                ) == MYLITE_OK) {
                saved_selected_schema_is_drop_target =
                    text_equals_ascii_case_insensitive(saved_selected_schema, dropped_schema);
            }
            database->session.has_selected_schema = prepared->has_selected_schema;
            (void)snprintf(
                database->session.selected_schema,
                sizeof(database->session.selected_schema),
                "%s",
                prepared->selected_schema
            );
        }
        database->session.active_statement_time = (int64_t)mylite_statement_context_time(&context);
        rc = mylite_execution_prepare_statement_transaction_boundary(database, statement);
        if (rc == MYLITE_OK) {
            rc = mylite_catalog_synchronize_snapshot(database);
        }
        if (rc == MYLITE_OK) {
            rc = validate_alter_table_algorithm_lock_options(database, statement);
        }
        if (rc == MYLITE_OK) {
            rc = execute_non_prepared_statement(database, &context, statement, out_result);
        }
        if (prepared != NULL) {
            database->session.has_selected_schema =
                saved_has_selected_schema &&
                !(rc == MYLITE_OK && saved_selected_schema_is_drop_target);
            if (database->session.has_selected_schema) {
                (void)snprintf(
                    database->session.selected_schema,
                    sizeof(database->session.selected_schema),
                    "%s",
                    saved_selected_schema
                );
            } else {
                database->session.selected_schema[0] = '\0';
            }
        }
        database->active_prepared_statement = saved_active_prepared_statement;
    }
    if (rc == MYLITE_OK) {
        completed_row_count = row_count_for_completed_statement(statement, *out_result);
        preserve_diagnostics_snapshot = statement_preserves_diagnostics_snapshot(statement);
        completed_statement_is_select = statement_result_is_select(statement, *out_result);
    }

    mylite_sql_parse_result_deinit(&parse_result);
    if (rc != MYLITE_OK) {
        rc = finish_failed_statement(database, NULL, rc, out_result);
    } else {
        rc = finish_completed_statement(
            database,
            NULL,
            completed_statement_is_select,
            completed_row_count,
            preserve_diagnostics_snapshot,
            out_result
        );
    }
    (void)mylite_statement_context_end(&context, rc);
    mylite_statement_context_deinit(&context);
    return rc;
}

static int execute_deallocate_prepare_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    char name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    mylite_result *result = NULL;
    size_t existing_index = 0U;
    int rc = copy_prepared_statement_name(database, child_at(statement, 0U), name, sizeof(name));

    if (rc != MYLITE_OK) {
        return rc;
    }

    existing_index = find_session_prepared_statement_index(&database->session, name);
    if (existing_index == SIZE_MAX) {
        set_unknown_prepared_statement_error(database, name, "DEALLOCATE PREPARE");
        return MYLITE_ERROR;
    }
    remove_session_prepared_statement_at(&database->session, existing_index);

    rc = mylite_result_create(&result);
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}
