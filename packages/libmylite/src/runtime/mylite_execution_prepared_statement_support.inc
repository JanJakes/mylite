static int prepare_statement_source_sql(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *source,
    char **out_sql,
    size_t *out_sql_size
) {
    struct session_scalar_cell value = {0};
    size_t decoded_length = 0U;
    int rc = MYLITE_OK;

    if (out_sql == NULL || out_sql_size == NULL) {
        return MYLITE_MISUSE;
    }
    *out_sql = NULL;
    *out_sql_size = 0U;
    source = unwrap_parenthesized_expression(source);
    if (source == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (source->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(source) == MYLITE_SQL_AST_LITERAL_STRING) {
        return decode_sql_string_literal(
            database,
            source,
            "PREPARE supports only string literal or user-variable source text",
            "PREPARE source text does not support NUL bytes",
            out_sql,
            out_sql_size
        );
    }
    if (source->kind != MYLITE_SQL_AST_USER_VARIABLE) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    rc = session_user_variable_value(database, source, &value);
    if (rc == MYLITE_OK) {
        rc = copy_session_scalar_sql_text(database, &value, "NULL", out_sql, &decoded_length);
    }
    if (rc == MYLITE_OK) {
        *out_sql_size = decoded_length;
    }
    session_scalar_cell_deinit(&value);
    return rc;
}

static int copy_session_scalar_sql_text(
    struct mylite_db *database,
    const struct session_scalar_cell *value,
    const char *null_text,
    char **out_sql,
    size_t *out_sql_size
) {
    const char *source = NULL;
    size_t source_size = 0U;
    char *copy = NULL;

    if (out_sql == NULL || out_sql_size == NULL) {
        return MYLITE_MISUSE;
    }
    *out_sql = NULL;
    *out_sql_size = 0U;
    if (value == NULL || value->value == NULL) {
        source = null_text;
        source_size = strlen(null_text);
    } else {
        source = value->value;
        if (value->has_value_size) {
            source_size = value->value_size;
        } else {
            source_size = strlen(value->value);
        }
    }
    if (source_size > SIZE_MAX - 1U) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    copy = (char *)malloc(source_size + 1U);
    if (copy == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(copy, source, source_size);
    copy[source_size] = '\0';

    *out_sql = copy;
    *out_sql_size = source_size;
    return MYLITE_OK;
}

static int validate_prepared_statement_sql(
    struct mylite_db *database,
    const char *sql,
    size_t sql_size,
    unsigned int lexer_modes,
    size_t *out_parameter_count
) {
    struct mylite_sql_parse_result parse_result = {0};
    const struct mylite_sql_ast_node *prepared_statement = NULL;
    size_t statement_count = 0U;
    int rc = MYLITE_OK;

    if (out_parameter_count == NULL) {
        return MYLITE_MISUSE;
    }
    *out_parameter_count = 0U;

    rc = status_from_parse_status(mylite_sql_parse(
        (struct mylite_sql_parse_config){
            .input = sql,
            .length = sql_size,
            .modes = lexer_modes,
            .allow_parameters = true,
        },
        &parse_result
    ));
    if (rc != MYLITE_OK) {
        rc = finish_parse_failure(database, &parse_result, rc);
        mylite_sql_parse_result_deinit(&parse_result);
        return rc;
    }
    *out_parameter_count = parse_result.parameter_count;

    rc = script_statement_count(parse_result.root, &statement_count);
    if (rc == MYLITE_OK && statement_count != 1U) {
        set_parse_error(database, NULL);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        prepared_statement = child_at(parse_result.root, 0U);
        if (prepared_statement_disallows_statement(prepared_statement)) {
            set_prepared_statement_command_unsupported_error(database);
            rc = MYLITE_ERROR;
        }
    }

    mylite_sql_parse_result_deinit(&parse_result);
    return rc;
}

static int append_execute_string_literal(
    struct mylite_dynamic_string *string,
    unsigned int lexer_modes,
    const char *text,
    size_t text_size
) {
    bool no_backslash_escapes = (lexer_modes & MYLITE_SQL_MODE_NO_BACKSLASH_ESCAPES) != 0U;
    int rc = mylite_dynamic_string_append_char(string, '\'');

    for (size_t index = 0U; rc == MYLITE_OK && index < text_size; ++index) {
        if (text[index] == '\'') {
            rc = mylite_dynamic_string_append(string, "''");
        } else if (!no_backslash_escapes && text[index] == '\\') {
            rc = mylite_dynamic_string_append(string, "\\\\");
        } else {
            rc = mylite_dynamic_string_append_char(string, text[index]);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '\'');
    }

    return rc;
}

static bool prepared_statement_disallows_statement(const struct mylite_sql_ast_node *statement) {
    if (statement == NULL) {
        return false;
    }

    switch (statement->kind) {
    case MYLITE_SQL_AST_PREPARE_STATEMENT:
    case MYLITE_SQL_AST_EXECUTE_STATEMENT:
    case MYLITE_SQL_AST_DEALLOCATE_PREPARE_STATEMENT:
        return true;
    default:
        return false;
    }
}

static int copy_prepared_statement_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    char *buffer,
    size_t buffer_size
) {
    int rc = copy_identifier_text(node, buffer, buffer_size, database);

    if (rc == MYLITE_OK) {
        fold_user_variable_name(buffer);
    }
    return rc;
}

static struct mylite_session_prepared_statement *find_session_prepared_statement(
    struct mylite_session_state *session,
    const char *name
) {
    size_t index = find_session_prepared_statement_index(session, name);

    return index == SIZE_MAX ? NULL : &session->prepared_statements[index];
}

static size_t find_session_prepared_statement_index(
    const struct mylite_session_state *session,
    const char *name
) {
    if (session == NULL || name == NULL) {
        return SIZE_MAX;
    }
    for (size_t index = 0U; index < session->prepared_statement_count; ++index) {
        if (strcmp(session->prepared_statements[index].name, name) == 0) {
            return index;
        }
    }
    return SIZE_MAX;
}

static void remove_session_prepared_statement_at(
    struct mylite_session_state *session,
    size_t index
) {
    if (session == NULL || index >= session->prepared_statement_count) {
        return;
    }

    deinit_session_prepared_statement(&session->prepared_statements[index]);
    for (size_t move_index = index + 1U; move_index < session->prepared_statement_count;
         ++move_index) {
        session->prepared_statements[move_index - 1U] = session->prepared_statements[move_index];
    }
    --session->prepared_statement_count;
    session->prepared_statements[session->prepared_statement_count] =
        (struct mylite_session_prepared_statement){0};
}

static void deinit_session_prepared_statement(struct mylite_session_prepared_statement *statement) {
    if (statement == NULL) {
        return;
    }

    (void)mylite_stmt_finalize(statement->statement);
    *statement = (struct mylite_session_prepared_statement){0};
}

static int ensure_session_prepared_statement_capacity(struct mylite_db *database) {
    struct mylite_session_state *session = database == NULL ? NULL : &database->session;
    struct mylite_session_prepared_statement *statements = NULL;
    size_t new_capacity = 0U;

    if (session == NULL) {
        return MYLITE_MISUSE;
    }
    if (session->prepared_statement_count < session->prepared_statement_capacity) {
        return MYLITE_OK;
    }
    new_capacity =
        session->prepared_statement_capacity == 0U ? 4U : session->prepared_statement_capacity * 2U;
    if (new_capacity < session->prepared_statement_capacity ||
        new_capacity > SIZE_MAX / sizeof(*statements)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    statements = (struct mylite_session_prepared_statement *)
        realloc(session->prepared_statements, new_capacity * sizeof(*statements));
    if (statements == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memset(
        statements + session->prepared_statement_capacity,
        0,
        (new_capacity - session->prepared_statement_capacity) * sizeof(*statements)
    );
    session->prepared_statements = statements;
    session->prepared_statement_capacity = new_capacity;
    return MYLITE_OK;
}

static void set_prepared_statement_argument_count_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_incorrect_arguments,
        "HY000",
        "Incorrect arguments to EXECUTE"
    );
}

static void set_unknown_prepared_statement_error(
    struct mylite_db *database,
    const char *statement_name,
    const char *command_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Unknown prepared statement handler (%s) given to %s",
        statement_name == NULL ? "" : statement_name,
        command_name == NULL ? "" : command_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_prepared_statement_handler,
        "HY000",
        message
    );
}

static void set_prepared_statement_command_unsupported_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_prepared_command_not_supported,
        "HY000",
        "This command is not supported in the prepared statement protocol yet"
    );
}
