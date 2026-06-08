static int execute_set_connection_character_set_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool allow_collation,
    mylite_result **out_result
) {
    const struct mylite_sql_ast_node *assignment_list =
        set_connection_character_set_assignment_list(statement);
    struct set_session_snapshot snapshot = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    if (assignment_list != NULL) {
        rc = copy_set_session_snapshot(database, &snapshot);
        if (rc != MYLITE_OK) {
            mylite_result_free(result);
            return rc;
        }
    }

    rc = apply_set_connection_character_set_statement(database, statement, allow_collation);
    if (rc != MYLITE_OK) {
        if (assignment_list != NULL) {
            restore_set_session_snapshot(database, &snapshot);
        }
        mylite_result_free(result);
        return rc;
    }
    if (assignment_list != NULL) {
        deinit_set_session_snapshot(&snapshot);
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int apply_set_connection_character_set_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool allow_collation
) {
    const struct mylite_sql_ast_node *target = NULL;
    const struct mylite_sql_ast_node *collation =
        set_connection_character_set_collation_clause(statement);
    const struct mylite_sql_ast_node *assignment_list =
        set_connection_character_set_assignment_list(statement);
    char charset_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char collation_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    const struct mylite_execution_catalog_character_set *charset_descriptor = NULL;
    const struct mylite_execution_catalog_collation *collation_descriptor = NULL;
    int rc = validate_set_connection_character_set_statement(database, statement, allow_collation);

    if (rc != MYLITE_OK) {
        return rc;
    }

    target = child_at(statement, 0U);
    charset_descriptor = character_set_by_name(MYLITE_CATALOG_DEFAULT_TABLE_CHARSET);
    if (target != NULL && target->kind != MYLITE_SQL_AST_SET_CHARACTER_SET_DEFAULT_TARGET) {
        rc = copy_table_option_name_text(
            database,
            target,
            charset_name,
            sizeof(charset_name),
            (struct table_option_name_policy){
                .identifier_kind = "character set",
                .nul_message = "SET character set names do not support NUL bytes",
            }
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        charset_descriptor = character_set_by_name(charset_name);
        if (charset_descriptor == NULL) {
            set_unknown_character_set_error(database, charset_name);
            return MYLITE_ERROR;
        }
    }
    if (charset_descriptor == NULL) {
        set_runtime_error(database, "invalid default connection character set");
        return MYLITE_ERROR;
    }

    if (collation != NULL) {
        rc = copy_table_option_name_text(
            database,
            collation,
            collation_name,
            sizeof(collation_name),
            (struct table_option_name_policy){
                .identifier_kind = "collation",
                .nul_message = "SET collation names do not support NUL bytes",
            }
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        collation_descriptor = collation_by_name(collation_name);
        if (collation_descriptor == NULL) {
            set_unknown_collation_error(database, collation_name);
            return MYLITE_ERROR;
        }
        if (strcmp(charset_descriptor->name, collation_descriptor->character_set) != 0) {
            set_collation_not_valid_for_charset_error(
                database,
                collation_descriptor->name,
                charset_descriptor->name
            );
            return MYLITE_ERROR;
        }
    } else {
        collation_descriptor = collation_by_name(charset_descriptor->default_collation);
    }
    if (collation_descriptor == NULL) {
        set_runtime_error(database, "invalid default connection collation");
        return MYLITE_ERROR;
    }

    rc = set_session_connection_character_set(
        database,
        charset_descriptor->name,
        collation_descriptor->name
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    return apply_set_connection_character_set_tail_assignments(database, assignment_list);
}

static int apply_set_connection_character_set_tail_assignments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment_list
) {
    const struct mylite_sql_ast_node *assignment = NULL;
    int rc = MYLITE_OK;

    if (assignment_list == NULL) {
        return MYLITE_OK;
    }
    if (assignment_list->kind != MYLITE_SQL_AST_SET_ASSIGNMENT_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    assignment = child_at(assignment_list, 0U);
    while (rc == MYLITE_OK && assignment != NULL) {
        rc = apply_set_assignment(database, assignment);
        assignment = assignment->next_sibling;
    }
    return rc;
}

static const struct mylite_sql_ast_node *set_connection_character_set_collation_clause(
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *child = child_at(statement, 1U);

    while (child != NULL) {
        if (child->kind != MYLITE_SQL_AST_SET_ASSIGNMENT_LIST) {
            return child;
        }
        child = child->next_sibling;
    }
    return NULL;
}

static const struct mylite_sql_ast_node *set_connection_character_set_assignment_list(
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *child = child_at(statement, 1U);

    while (child != NULL) {
        if (child->kind == MYLITE_SQL_AST_SET_ASSIGNMENT_LIST) {
            return child;
        }
        child = child->next_sibling;
    }
    return NULL;
}

static int validate_set_connection_character_set_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool allow_collation
) {
    const struct mylite_sql_ast_node *target = NULL;
    const struct mylite_sql_ast_node *collation =
        set_connection_character_set_collation_clause(statement);
    int rc = MYLITE_OK;

    if (statement == NULL || (statement->kind != MYLITE_SQL_AST_SET_NAMES_STATEMENT &&
                              statement->kind != MYLITE_SQL_AST_SET_CHARACTER_SET_STATEMENT)) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    target = child_at(statement, 0U);
    if (collation != NULL && !allow_collation) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    rc = validate_set_connection_character_set_target(database, target);
    if (rc == MYLITE_OK && collation != NULL) {
        rc = validate_set_names_collation_target(database, collation);
    }
    return rc;
}

static int validate_set_connection_character_set_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *target
) {
    char charset_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = MYLITE_OK;

    if (target == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (target->kind == MYLITE_SQL_AST_SET_CHARACTER_SET_DEFAULT_TARGET) {
        return MYLITE_OK;
    }

    rc = copy_table_option_name_text(
        database,
        target,
        charset_name,
        sizeof(charset_name),
        (struct table_option_name_policy){
            .identifier_kind = "character set",
            .nul_message = "SET character set names do not support NUL bytes",
        }
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (character_set_by_name(charset_name) == NULL) {
        set_unknown_character_set_error(database, charset_name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_set_names_collation_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *target
) {
    char collation_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = copy_table_option_name_text(
        database,
        target,
        collation_name,
        sizeof(collation_name),
        (struct table_option_name_policy){
            .identifier_kind = "collation",
            .nul_message = "SET collation names do not support NUL bytes",
        }
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (collation_by_name(collation_name) == NULL) {
        set_unknown_collation_error(database, collation_name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int set_session_connection_character_set(
    struct mylite_db *database,
    const char *charset_name,
    const char *collation_name
) {
    size_t charset_length = strlen(charset_name);
    size_t collation_length = strlen(collation_name);

    if (database == NULL || charset_name == NULL || collation_name == NULL) {
        return MYLITE_MISUSE;
    }
    if (charset_length >= sizeof(database->session.character_set_client) ||
        collation_length >= sizeof(database->session.collation_connection)) {
        set_runtime_error(database, "connection character set state is too small");
        return MYLITE_ERROR;
    }

    memcpy(database->session.character_set_client, charset_name, charset_length + 1U);
    memcpy(database->session.character_set_connection, charset_name, charset_length + 1U);
    memcpy(database->session.character_set_results, charset_name, charset_length + 1U);
    memcpy(database->session.collation_connection, collation_name, collation_length + 1U);
    database->session.character_set_state_is_placeholder = false;
    database->session.system_variables_are_placeholder = false;
    return MYLITE_OK;
}
