static int execute_prepare_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    char name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char *source_sql = NULL;
    mylite_result *result = NULL;
    size_t source_sql_size = 0U;
    size_t parameter_count = 0U;
    size_t existing_index = 0U;
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
            &parameter_count
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
        prepared->sql = source_sql;
        prepared->sql_size = source_sql_size;
        prepared->parameter_count = parameter_count;
        source_sql = NULL;
        ++database->session.prepared_statement_count;
    }

    free(source_sql);
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
    struct prepared_statement_expanded_sql expanded_sql = {0};
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
    if (parameter_count != prepared->parameter_count) {
        set_prepared_statement_argument_count_error(database);
        return MYLITE_ERROR;
    }

    rc = build_prepared_statement_sql(
        database,
        prepared->sql,
        prepared->sql_size,
        using_list,
        &expanded_sql
    );
    if (rc == MYLITE_OK) {
        rc = execute_expanded_prepared_statement_sql(
            database,
            expanded_sql.text,
            expanded_sql.text_size,
            out_result
        );
    }

    free(expanded_sql.text);
    return rc;
}

static int execute_expanded_prepared_statement_sql(
    struct mylite_db *database,
    const char *sql,
    size_t sql_size,
    mylite_result **out_result
) {
    struct mylite_statement_context context;
    struct mylite_sql_parse_result parse_result;
    const struct mylite_sql_ast_node *statement = NULL;
    int64_t completed_row_count = -1;
    size_t statement_count = 0U;
    bool preserve_diagnostics_snapshot = false;
    bool completed_statement_is_select = false;
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
            .modes = lexer_modes_for_session_sql_mode(&database->session),
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
        database->session.active_statement_time = (int64_t)mylite_statement_context_time(&context);
        rc = prepare_statement_transaction_boundary(database, statement);
        if (rc == MYLITE_OK) {
            rc = mylite_catalog_synchronize_snapshot(database);
        }
        if (rc == MYLITE_OK) {
            rc = validate_alter_table_algorithm_lock_options(database, statement);
        }
        if (rc == MYLITE_OK) {
            rc = execute_non_prepared_statement(database, &context, statement, out_result);
        }
    }
    if (rc == MYLITE_OK) {
        completed_row_count = row_count_for_completed_statement(statement, *out_result);
        preserve_diagnostics_snapshot = statement_preserves_diagnostics_snapshot(statement);
        completed_statement_is_select = statement_result_is_select(statement, *out_result);
    }

    mylite_sql_parse_result_deinit(&parse_result);
    if (rc != MYLITE_OK) {
        rc = finish_failed_statement(database, rc, out_result);
    } else {
        rc = finish_completed_statement(
            database,
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
