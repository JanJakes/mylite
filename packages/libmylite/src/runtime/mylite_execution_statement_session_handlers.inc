static int execute_empty_statement(struct mylite_db *database, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_use_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct mylite_catalog_schema_descriptor schema = {0};
    const struct mylite_execution_catalog_builtin_schema *builtin_schema = NULL;
    char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    mylite_result *result = NULL;
    int written = 0;
    int rc =
        copy_identifier_text(child_at(statement, 0U), schema_name, sizeof(schema_name), database);

    if (rc == MYLITE_OK) {
        builtin_schema = find_builtin_schema_descriptor(schema_name);
    }
    if (rc == MYLITE_OK && builtin_schema != NULL) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK && result == NULL && mylite_catalog_name_is_reserved(schema_name)) {
        set_reserved_name_error(database, "database", schema_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && result == NULL) {
        rc = resolve_schema_name(database, schema_name, &schema);
    }
    if (rc == MYLITE_OK && result == NULL) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    written = snprintf(
        database->session.selected_schema,
        sizeof(database->session.selected_schema),
        "%s",
        builtin_schema != NULL ? builtin_schema->name : schema.name
    );
    if (written < 0 || (size_t)written >= sizeof(database->session.selected_schema)) {
        mylite_result_free(result);
        set_identifier_too_long_error(database, "database");
        return MYLITE_ERROR;
    }
    database->session.has_selected_schema = true;

    return finish_successful_result(database, result, out_result);
}

static int execute_set_names_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    return execute_set_connection_character_set_statement(database, statement, true, out_result);
}

static int execute_set_character_set_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    return execute_set_connection_character_set_statement(database, statement, false, out_result);
}

static int execute_set_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    if (set_statement_is_persist_noop(statement)) {
        return execute_admin_noop_statement(database, out_result);
    }

    rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = apply_set_statement(database, statement);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}
