struct stored_procedure_target {
    struct table_name_resolution resolution;
    bool was_schema_qualified;
};

static int resolve_stored_procedure_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *name_node,
    struct stored_procedure_target *out_target
);
static int copy_stored_procedure_select_sql(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_statement,
    char **out_sql,
    size_t *out_sql_size
);
static int build_stored_procedure_show_create_sql(
    struct mylite_db *database,
    const struct stored_procedure_target *target,
    const char *select_sql,
    size_t select_sql_size,
    char **out_sql,
    size_t *out_sql_size
);
static int append_show_create_definer(struct mylite_dynamic_string *string, const char *identity);
static int append_mysql_quoted_identifier_bytes(
    struct mylite_dynamic_string *string,
    const char *text,
    size_t text_size
);
static int append_show_create_procedure_result(
    struct mylite_db *database,
    mylite_result *result,
    const struct mylite_session_stored_procedure *procedure
);
static void copy_stored_procedure_field(char *destination, size_t size, const char *source);
static bool drop_procedure_statement_has_if_exists(const struct mylite_sql_ast_node *statement);
static int append_missing_procedure_note(
    struct mylite_db *database,
    const struct stored_procedure_target *target
);
static void set_routine_exists_error(struct mylite_db *database, const char *routine_name);
static void set_routine_does_not_exist_error(
    struct mylite_db *database,
    const struct stored_procedure_target *target,
    bool include_schema
);
static void format_routine_does_not_exist_message(
    const struct stored_procedure_target *target,
    bool include_schema,
    char *message,
    size_t message_size
);

static int execute_create_procedure_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct stored_procedure_target target = {0};
    struct mylite_session_stored_procedure *procedure = NULL;
    char *select_sql = NULL;
    char *show_create_sql = NULL;
    mylite_result *result = NULL;
    size_t select_sql_size = 0U;
    size_t show_create_sql_size = 0U;
    int rc = resolve_stored_procedure_target(database, child_at(statement, 0U), &target);

    if (rc == MYLITE_OK && find_session_stored_procedure(
                               &database->session,
                               target.resolution.schema.name,
                               target.resolution.table_name
                           ) != NULL) {
        set_routine_exists_error(database, target.resolution.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = copy_stored_procedure_select_sql(
            database,
            child_at(statement, 1U),
            &select_sql,
            &select_sql_size
        );
    }
    if (rc == MYLITE_OK) {
        rc = build_stored_procedure_show_create_sql(
            database,
            &target,
            select_sql,
            select_sql_size,
            &show_create_sql,
            &show_create_sql_size
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = ensure_session_stored_procedure_capacity(database);
    }
    if (rc == MYLITE_OK) {
        procedure = &database->session.stored_procedures[database->session.stored_procedure_count];
        *procedure = (struct mylite_session_stored_procedure){0};
        memcpy(
            procedure->schema_name,
            target.resolution.schema.name,
            strlen(target.resolution.schema.name) + 1U
        );
        memcpy(
            procedure->name,
            target.resolution.table_name,
            strlen(target.resolution.table_name) + 1U
        );
        procedure->select_sql = select_sql;
        procedure->select_sql_size = select_sql_size;
        procedure->show_create_sql = show_create_sql;
        procedure->show_create_sql_size = show_create_sql_size;
        copy_stored_procedure_field(
            procedure->sql_mode_text,
            sizeof(procedure->sql_mode_text),
            database->session.sql_mode_text
        );
        copy_stored_procedure_field(
            procedure->character_set_client,
            sizeof(procedure->character_set_client),
            database->session.character_set_client
        );
        copy_stored_procedure_field(
            procedure->collation_connection,
            sizeof(procedure->collation_connection),
            database->session.collation_connection
        );
        copy_stored_procedure_field(
            procedure->database_collation,
            sizeof(procedure->database_collation),
            target.resolution.schema.default_collation
        );
        select_sql = NULL;
        show_create_sql = NULL;
        ++database->session.stored_procedure_count;
    }

    free(select_sql);
    free(show_create_sql);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int execute_drop_procedure_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct stored_procedure_target target = {0};
    mylite_result *result = NULL;
    size_t index = SIZE_MAX;
    bool if_exists = drop_procedure_statement_has_if_exists(statement);
    int rc = resolve_stored_procedure_target(database, child_at(statement, 0U), &target);

    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        index = find_session_stored_procedure_index(
            &database->session,
            target.resolution.schema.name,
            target.resolution.table_name
        );
        if (index != SIZE_MAX) {
            remove_session_stored_procedure_at(&database->session, index);
        } else if (if_exists) {
            rc = append_missing_procedure_note(database, &target);
        } else {
            set_routine_does_not_exist_error(database, &target, true);
            rc = MYLITE_ERROR;
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int execute_show_create_procedure_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct stored_procedure_target target = {0};
    struct mylite_session_stored_procedure *procedure = NULL;
    mylite_result *result = NULL;
    int rc = resolve_stored_procedure_target(database, child_at(statement, 0U), &target);

    if (rc == MYLITE_OK) {
        procedure = find_session_stored_procedure(
            &database->session,
            target.resolution.schema.name,
            target.resolution.table_name
        );
        if (procedure == NULL) {
            set_routine_does_not_exist_error(database, &target, target.was_schema_qualified);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_show_create_procedure_result(database, result, procedure);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_call_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct stored_procedure_target target = {0};
    struct mylite_session_stored_procedure *procedure = NULL;
    int rc = resolve_stored_procedure_target(database, child_at(statement, 0U), &target);

    if (rc != MYLITE_OK) {
        return rc;
    }

    procedure = find_session_stored_procedure(
        &database->session,
        target.resolution.schema.name,
        target.resolution.table_name
    );
    if (procedure == NULL) {
        set_routine_does_not_exist_error(database, &target, true);
        return MYLITE_ERROR;
    }

    return mylite_execute(database, procedure->select_sql, procedure->select_sql_size, out_result);
}

static int resolve_stored_procedure_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *name_node,
    struct stored_procedure_target *out_target
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t part_count = 0U;
    int rc = MYLITE_OK;

    if (out_target == NULL) {
        return MYLITE_MISUSE;
    }

    *out_target = (struct stored_procedure_target){0};
    rc = resolve_table_name(database, name_node, &out_target->resolution);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc =
        collect_identifier_parts(name_node, parts, table_name_part_capacity, &part_count, database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    out_target->was_schema_qualified = part_count == 2U;
    return MYLITE_OK;
}

static int copy_stored_procedure_select_sql(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_statement,
    char **out_sql,
    size_t *out_sql_size
) {
    char *copy = NULL;

    if (out_sql == NULL || out_sql_size == NULL) {
        return MYLITE_MISUSE;
    }
    *out_sql = NULL;
    *out_sql_size = 0U;
    if (select_statement == NULL || select_statement->span.text == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (select_statement->span.length > SIZE_MAX - 1U) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    copy = (char *)malloc(select_statement->span.length + 1U);
    if (copy == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    memcpy(copy, select_statement->span.text, select_statement->span.length);
    copy[select_statement->span.length] = '\0';
    *out_sql = copy;
    *out_sql_size = select_statement->span.length;
    return MYLITE_OK;
}

static int build_stored_procedure_show_create_sql(
    struct mylite_db *database,
    const struct stored_procedure_target *target,
    const char *select_sql,
    size_t select_sql_size,
    char **out_sql,
    size_t *out_sql_size
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    if (target == NULL || select_sql == NULL || out_sql == NULL || out_sql_size == NULL) {
        return MYLITE_MISUSE;
    }
    *out_sql = NULL;
    *out_sql_size = 0U;

    mylite_dynamic_string_init(&string);
    rc = mylite_dynamic_string_append(&string, "CREATE DEFINER=");
    if (rc == MYLITE_OK) {
        rc = append_show_create_definer(&string, database->session.current_user_identity);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " PROCEDURE ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_mysql_quoted_identifier(
            &string,
            target->resolution.table_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, "()\nBEGIN ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_bytes(&string, select_sql, select_sql_size);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, "; END");
    }
    if (rc == MYLITE_OK) {
        *out_sql_size = string.length;
        *out_sql = mylite_dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }

    mylite_dynamic_string_deinit(&string);
    return rc;
}

static int append_show_create_definer(struct mylite_dynamic_string *string, const char *identity) {
    const char *separator = NULL;
    const char *user = identity == NULL ? "" : identity;
    const char *host = "%";
    size_t user_size = strlen(user);
    size_t host_size = 1U;
    int rc = MYLITE_OK;

    separator = strchr(user, '@');
    if (separator != NULL) {
        user_size = (size_t)(separator - user);
        host = separator + 1;
        host_size = strlen(host);
    }

    rc = append_mysql_quoted_identifier_bytes(string, user, user_size);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '@');
    }
    if (rc == MYLITE_OK) {
        rc = append_mysql_quoted_identifier_bytes(string, host, host_size);
    }
    return rc;
}

static int append_mysql_quoted_identifier_bytes(
    struct mylite_dynamic_string *string,
    const char *text,
    size_t text_size
) {
    int rc = mylite_dynamic_string_append_char(string, '`');

    for (size_t index = 0U; rc == MYLITE_OK && index < text_size; ++index) {
        if (text[index] == '`') {
            rc = mylite_dynamic_string_append(string, "``");
        } else {
            rc = mylite_dynamic_string_append_char(string, text[index]);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '`');
    }
    return rc;
}

static int append_show_create_procedure_result(
    struct mylite_db *database,
    mylite_result *result,
    const struct mylite_session_stored_procedure *procedure
) {
    enum { column_count = 6 };

    static const char *const result_columns[column_count] = {
        "Procedure",
        "sql_mode",
        "Create Procedure",
        "character_set_client",
        "collation_connection",
        "Database Collation",
    };
    const char *values[column_count] = {
        procedure->name,
        procedure->sql_mode_text,
        procedure->show_create_sql,
        procedure->character_set_client,
        procedure->collation_connection,
        procedure->database_collation,
    };
    int rc = MYLITE_OK;

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < column_count; ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    return rc;
}

static void copy_stored_procedure_field(char *destination, size_t size, const char *source) {
    int written = snprintf(destination, size, "%s", source == NULL ? "" : source);

    if (written < 0 || (size_t)written >= size) {
        destination[0] = '\0';
    }
}

static bool drop_procedure_statement_has_if_exists(const struct mylite_sql_ast_node *statement) {
    return statement != NULL && child_at(statement, 1U) != NULL;
}

static int append_missing_procedure_note(
    struct mylite_db *database,
    const struct stored_procedure_target *target
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];

    format_routine_does_not_exist_message(target, true, message, sizeof(message));
    return mylite_diagnostics_append_note(
        mylite_connection_diagnostics(database),
        mysql_error_routine_does_not_exist,
        "42000",
        message
    );
}

static void set_routine_exists_error(struct mylite_db *database, const char *routine_name) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "PROCEDURE %s already exists",
        routine_name == NULL ? "" : routine_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_routine_exists,
        "42000",
        message
    );
}

static void set_routine_does_not_exist_error(
    struct mylite_db *database,
    const struct stored_procedure_target *target,
    bool include_schema
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];

    format_routine_does_not_exist_message(target, include_schema, message, sizeof(message));
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_routine_does_not_exist,
        "42000",
        message
    );
}

static void format_routine_does_not_exist_message(
    const struct stored_procedure_target *target,
    bool include_schema,
    char *message,
    size_t message_size
) {
    int written = 0;

    if (target == NULL) {
        written = snprintf(message, message_size, "PROCEDURE does not exist");
    } else if (include_schema) {
        written = snprintf(
            message,
            message_size,
            "PROCEDURE %s.%s does not exist",
            target->resolution.schema.name,
            target->resolution.table_name
        );
    } else {
        written = snprintf(
            message,
            message_size,
            "PROCEDURE %s does not exist",
            target->resolution.table_name
        );
    }
    if (written < 0 || (size_t)written >= message_size) {
        message[0] = '\0';
    }
}

static struct mylite_session_stored_procedure *find_session_stored_procedure(
    struct mylite_session_state *session,
    const char *schema_name,
    const char *name
) {
    size_t index = find_session_stored_procedure_index(session, schema_name, name);

    return index == SIZE_MAX ? NULL : &session->stored_procedures[index];
}

static size_t find_session_stored_procedure_index(
    const struct mylite_session_state *session,
    const char *schema_name,
    const char *name
) {
    if (session == NULL || schema_name == NULL || name == NULL) {
        return SIZE_MAX;
    }
    for (size_t index = 0U; index < session->stored_procedure_count; ++index) {
        const struct mylite_session_stored_procedure *procedure =
            &session->stored_procedures[index];

        if (strcmp(procedure->schema_name, schema_name) == 0 &&
            strcmp(procedure->name, name) == 0) {
            return index;
        }
    }
    return SIZE_MAX;
}

static void remove_session_stored_procedure_at(struct mylite_session_state *session, size_t index) {
    if (session == NULL || index >= session->stored_procedure_count) {
        return;
    }

    deinit_session_stored_procedure(&session->stored_procedures[index]);
    for (size_t move_index = index + 1U; move_index < session->stored_procedure_count;
         ++move_index) {
        session->stored_procedures[move_index - 1U] = session->stored_procedures[move_index];
    }
    --session->stored_procedure_count;
    session->stored_procedures[session->stored_procedure_count] =
        (struct mylite_session_stored_procedure){0};
}

static void deinit_session_stored_procedure(struct mylite_session_stored_procedure *procedure) {
    if (procedure == NULL) {
        return;
    }

    free(procedure->select_sql);
    free(procedure->show_create_sql);
    *procedure = (struct mylite_session_stored_procedure){0};
}

static int ensure_session_stored_procedure_capacity(struct mylite_db *database) {
    struct mylite_session_state *session = database == NULL ? NULL : &database->session;
    struct mylite_session_stored_procedure *procedures = NULL;
    size_t new_capacity = 0U;

    if (session == NULL) {
        return MYLITE_MISUSE;
    }
    if (session->stored_procedure_count < session->stored_procedure_capacity) {
        return MYLITE_OK;
    }

    new_capacity =
        session->stored_procedure_capacity == 0U ? 4U : session->stored_procedure_capacity * 2U;
    if (new_capacity < session->stored_procedure_capacity ||
        new_capacity > SIZE_MAX / sizeof(*procedures)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    procedures = (struct mylite_session_stored_procedure *)
        realloc(session->stored_procedures, new_capacity * sizeof(*procedures));
    if (procedures == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memset(
        procedures + session->stored_procedure_capacity,
        0,
        (new_capacity - session->stored_procedure_capacity) * sizeof(*procedures)
    );
    session->stored_procedures = procedures;
    session->stored_procedure_capacity = new_capacity;
    return MYLITE_OK;
}
