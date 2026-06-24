struct stored_procedure_target {
    struct table_name_resolution resolution;
    bool was_schema_qualified;
};

enum sys_stored_procedure_behavior {
    SYS_STORED_PROCEDURE_EMPTY_RESULT = 0,
    SYS_STORED_PROCEDURE_SUMMARY_RESULT = 1,
    SYS_STORED_PROCEDURE_TABLE_EXISTS = 2,
    SYS_STORED_PROCEDURE_EXECUTE_PREPARED_STMT = 3,
    SYS_STORED_PROCEDURE_UNSUPPORTED_DIAGNOSTIC = 4,
};

struct sys_stored_procedure_definition {
    const char *name;
    size_t argument_count;
    enum sys_stored_procedure_behavior behavior;
    const char *const *columns;
    size_t column_count;
    const char *summary;
};

static const char *const sys_summary_result_columns[] = {"summary"};
static const char *const sys_disabled_consumers_columns[] = {"disabled_consumers"};
static const char *const sys_enabled_consumers_columns[] = {"enabled_consumers"};
static const char *const sys_disabled_instruments_columns[] = {"disabled_instruments", "timed"};
static const char *const sys_enabled_instruments_columns[] = {"enabled_instruments", "timed"};

static const struct sys_stored_procedure_definition sys_stored_procedures[] = {
    {"create_synonym_db", 2U, SYS_STORED_PROCEDURE_UNSUPPORTED_DIAGNOSTIC, NULL, 0U, NULL},
    {"diagnostics", 3U, SYS_STORED_PROCEDURE_UNSUPPORTED_DIAGNOSTIC, NULL, 0U, NULL},
    {"execute_prepared_stmt", 1U, SYS_STORED_PROCEDURE_EXECUTE_PREPARED_STMT, NULL, 0U, NULL},
    {"ps_setup_disable_background_threads",
     0U,
     SYS_STORED_PROCEDURE_SUMMARY_RESULT,
     sys_summary_result_columns,
     sizeof(sys_summary_result_columns) / sizeof(sys_summary_result_columns[0]),
     "Disabled 0 background threads"},
    {"ps_setup_disable_consumer",
     1U,
     SYS_STORED_PROCEDURE_SUMMARY_RESULT,
     sys_summary_result_columns,
     sizeof(sys_summary_result_columns) / sizeof(sys_summary_result_columns[0]),
     "Disabled 0 consumers"},
    {"ps_setup_disable_instrument",
     1U,
     SYS_STORED_PROCEDURE_SUMMARY_RESULT,
     sys_summary_result_columns,
     sizeof(sys_summary_result_columns) / sizeof(sys_summary_result_columns[0]),
     "Disabled 0 instruments"},
    {"ps_setup_disable_thread",
     1U,
     SYS_STORED_PROCEDURE_SUMMARY_RESULT,
     sys_summary_result_columns,
     sizeof(sys_summary_result_columns) / sizeof(sys_summary_result_columns[0]),
     "Disabled 0 threads"},
    {"ps_setup_enable_background_threads",
     0U,
     SYS_STORED_PROCEDURE_SUMMARY_RESULT,
     sys_summary_result_columns,
     sizeof(sys_summary_result_columns) / sizeof(sys_summary_result_columns[0]),
     "Enabled 0 background threads"},
    {"ps_setup_enable_consumer",
     1U,
     SYS_STORED_PROCEDURE_SUMMARY_RESULT,
     sys_summary_result_columns,
     sizeof(sys_summary_result_columns) / sizeof(sys_summary_result_columns[0]),
     "Enabled 0 consumers"},
    {"ps_setup_enable_instrument",
     1U,
     SYS_STORED_PROCEDURE_SUMMARY_RESULT,
     sys_summary_result_columns,
     sizeof(sys_summary_result_columns) / sizeof(sys_summary_result_columns[0]),
     "Enabled 0 instruments"},
    {"ps_setup_enable_thread",
     1U,
     SYS_STORED_PROCEDURE_SUMMARY_RESULT,
     sys_summary_result_columns,
     sizeof(sys_summary_result_columns) / sizeof(sys_summary_result_columns[0]),
     "Enabled 0 threads"},
    {"ps_setup_reload_saved",
     0U,
     SYS_STORED_PROCEDURE_SUMMARY_RESULT,
     sys_summary_result_columns,
     sizeof(sys_summary_result_columns) / sizeof(sys_summary_result_columns[0]),
     "Reloaded 0 saved Performance Schema setup rows"},
    {"ps_setup_reset_to_default",
     1U,
     SYS_STORED_PROCEDURE_SUMMARY_RESULT,
     sys_summary_result_columns,
     sizeof(sys_summary_result_columns) / sizeof(sys_summary_result_columns[0]),
     "Reset Performance Schema setup to MyLite defaults"},
    {"ps_setup_save",
     1U,
     SYS_STORED_PROCEDURE_SUMMARY_RESULT,
     sys_summary_result_columns,
     sizeof(sys_summary_result_columns) / sizeof(sys_summary_result_columns[0]),
     "Saved 0 Performance Schema setup rows"},
    {"ps_setup_show_disabled",
     2U,
     SYS_STORED_PROCEDURE_SUMMARY_RESULT,
     sys_summary_result_columns,
     sizeof(sys_summary_result_columns) / sizeof(sys_summary_result_columns[0]),
     "No mutable Performance Schema setup rows are disabled"},
    {"ps_setup_show_disabled_consumers",
     0U,
     SYS_STORED_PROCEDURE_EMPTY_RESULT,
     sys_disabled_consumers_columns,
     sizeof(sys_disabled_consumers_columns) / sizeof(sys_disabled_consumers_columns[0]),
     NULL},
    {"ps_setup_show_disabled_instruments",
     0U,
     SYS_STORED_PROCEDURE_EMPTY_RESULT,
     sys_disabled_instruments_columns,
     sizeof(sys_disabled_instruments_columns) / sizeof(sys_disabled_instruments_columns[0]),
     NULL},
    {"ps_setup_show_enabled",
     2U,
     SYS_STORED_PROCEDURE_SUMMARY_RESULT,
     sys_summary_result_columns,
     sizeof(sys_summary_result_columns) / sizeof(sys_summary_result_columns[0]),
     "No mutable Performance Schema setup rows are enabled"},
    {"ps_setup_show_enabled_consumers",
     0U,
     SYS_STORED_PROCEDURE_EMPTY_RESULT,
     sys_enabled_consumers_columns,
     sizeof(sys_enabled_consumers_columns) / sizeof(sys_enabled_consumers_columns[0]),
     NULL},
    {"ps_setup_show_enabled_instruments",
     0U,
     SYS_STORED_PROCEDURE_EMPTY_RESULT,
     sys_enabled_instruments_columns,
     sizeof(sys_enabled_instruments_columns) / sizeof(sys_enabled_instruments_columns[0]),
     NULL},
    {"ps_statement_avg_latency_histogram",
     0U,
     SYS_STORED_PROCEDURE_UNSUPPORTED_DIAGNOSTIC,
     NULL,
     0U,
     NULL},
    {"ps_trace_statement_digest",
     5U,
     SYS_STORED_PROCEDURE_UNSUPPORTED_DIAGNOSTIC,
     NULL,
     0U,
     NULL},
    {"ps_trace_thread", 7U, SYS_STORED_PROCEDURE_UNSUPPORTED_DIAGNOSTIC, NULL, 0U, NULL},
    {"ps_truncate_all_tables",
     1U,
     SYS_STORED_PROCEDURE_SUMMARY_RESULT,
     sys_summary_result_columns,
     sizeof(sys_summary_result_columns) / sizeof(sys_summary_result_columns[0]),
     "Truncated 0 Performance Schema tables"},
    {"statement_performance_analyzer",
     3U,
     SYS_STORED_PROCEDURE_UNSUPPORTED_DIAGNOSTIC,
     NULL,
     0U,
     NULL},
    {"table_exists", 3U, SYS_STORED_PROCEDURE_TABLE_EXISTS, NULL, 0U, NULL},
};

static int resolve_stored_procedure_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *name_node,
    struct stored_procedure_target *out_target
);
static int try_resolve_sys_stored_procedure_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *name_node,
    struct stored_procedure_target *out_target,
    bool *out_found
);
static int execute_sys_stored_procedure_call(
    struct mylite_db *database,
    const struct stored_procedure_target *target,
    const struct mylite_sql_ast_node *arguments,
    mylite_result **out_result
);
static const struct sys_stored_procedure_definition *find_sys_stored_procedure_definition(
    const char *name
);
static size_t stored_procedure_argument_count(const struct mylite_sql_ast_node *arguments);
static const struct mylite_sql_ast_node *stored_procedure_argument_at(
    const struct mylite_sql_ast_node *arguments,
    size_t index
);
static int execute_sys_table_exists_procedure(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *arguments,
    mylite_result **out_result
);
static int execute_sys_execute_prepared_stmt_procedure(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *arguments,
    mylite_result **out_result
);
static int execute_sys_summary_procedure(
    struct mylite_db *database,
    const struct sys_stored_procedure_definition *definition,
    mylite_result **out_result
);
static int execute_sys_empty_result_procedure(
    struct mylite_db *database,
    const struct sys_stored_procedure_definition *definition,
    mylite_result **out_result
);
static int execute_sys_unsupported_procedure(
    struct mylite_db *database,
    const struct sys_stored_procedure_definition *definition
);
static int append_sys_result_columns(
    struct mylite_db *database,
    mylite_result *result,
    const char *const *columns,
    size_t column_count
);
static int copy_sys_procedure_string_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *argument,
    char *buffer,
    size_t buffer_size
);
static int sys_table_exists_value(
    struct mylite_db *database,
    const char *schema_name,
    const char *table_name,
    const char **out_value
);
static int finish_sys_empty_ok_result(struct mylite_db *database, mylite_result **out_result);
static int fill_builtin_stored_procedure_resolution(
    const struct mylite_execution_catalog_builtin_schema *schema,
    const char *table_name,
    struct table_name_resolution *out_resolution
);
static void set_sys_stored_procedure_argument_count_error(
    struct mylite_db *database,
    const struct stored_procedure_target *target,
    size_t expected,
    size_t actual
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
    bool is_sys_target = false;
    int rc = try_resolve_sys_stored_procedure_target(
        database,
        child_at(statement, 0U),
        &target,
        &is_sys_target
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (is_sys_target) {
        return execute_sys_stored_procedure_call(
            database,
            &target,
            child_at(statement, 1U),
            out_result
        );
    }

    if (child_at(statement, 1U) != NULL) {
        set_unsupported_error(database, "stored procedure arguments are not supported");
        return MYLITE_ERROR;
    }

    rc = resolve_stored_procedure_target(database, child_at(statement, 0U), &target);
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

static int execute_sys_stored_procedure_call(
    struct mylite_db *database,
    const struct stored_procedure_target *target,
    const struct mylite_sql_ast_node *arguments,
    mylite_result **out_result
) {
    const struct sys_stored_procedure_definition *definition =
        find_sys_stored_procedure_definition(target->resolution.table_name);
    size_t actual_argument_count = stored_procedure_argument_count(arguments);

    if (definition == NULL) {
        set_routine_does_not_exist_error(database, target, true);
        return MYLITE_ERROR;
    }
    if (actual_argument_count != definition->argument_count) {
        set_sys_stored_procedure_argument_count_error(
            database,
            target,
            definition->argument_count,
            actual_argument_count
        );
        return MYLITE_ERROR;
    }

    switch (definition->behavior) {
    case SYS_STORED_PROCEDURE_TABLE_EXISTS:
        return execute_sys_table_exists_procedure(database, arguments, out_result);
    case SYS_STORED_PROCEDURE_EXECUTE_PREPARED_STMT:
        return execute_sys_execute_prepared_stmt_procedure(database, arguments, out_result);
    case SYS_STORED_PROCEDURE_SUMMARY_RESULT:
        return execute_sys_summary_procedure(database, definition, out_result);
    case SYS_STORED_PROCEDURE_EMPTY_RESULT:
        return execute_sys_empty_result_procedure(database, definition, out_result);
    case SYS_STORED_PROCEDURE_UNSUPPORTED_DIAGNOSTIC:
        return execute_sys_unsupported_procedure(database, definition);
    }

    set_parse_error(database, NULL);
    return MYLITE_ERROR;
}

static const struct sys_stored_procedure_definition *find_sys_stored_procedure_definition(
    const char *name
) {
    size_t procedure_count = sizeof(sys_stored_procedures) / sizeof(sys_stored_procedures[0]);

    for (size_t index = 0U; index < procedure_count; ++index) {
        if (mylite_execution_text_equals_ascii_case_insensitive(
                name,
                sys_stored_procedures[index].name
            )) {
            return &sys_stored_procedures[index];
        }
    }

    return NULL;
}

static size_t stored_procedure_argument_count(const struct mylite_sql_ast_node *arguments) {
    size_t count = 0U;

    if (arguments == NULL) {
        return 0U;
    }
    for (const struct mylite_sql_ast_node *argument = child_at(arguments, 0U); argument != NULL;
         argument = argument->next_sibling) {
        ++count;
    }

    return count;
}

static const struct mylite_sql_ast_node *stored_procedure_argument_at(
    const struct mylite_sql_ast_node *arguments,
    size_t index
) {
    const struct mylite_sql_ast_node *argument = arguments == NULL ? NULL : child_at(arguments, 0U);

    while (argument != NULL && index > 0U) {
        argument = argument->next_sibling;
        --index;
    }

    return argument;
}

static int execute_sys_table_exists_procedure(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *arguments,
    mylite_result **out_result
) {
    char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char table_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    struct session_scalar_cell output = {0};
    const struct mylite_sql_ast_node *out_argument = stored_procedure_argument_at(arguments, 2U);
    const char *exists_value = "";
    int rc = copy_sys_procedure_string_argument(
        database,
        stored_procedure_argument_at(arguments, 0U),
        schema_name,
        sizeof(schema_name)
    );

    if (rc == MYLITE_OK) {
        rc = copy_sys_procedure_string_argument(
            database,
            stored_procedure_argument_at(arguments, 1U),
            table_name,
            sizeof(table_name)
        );
    }
    if (rc == MYLITE_OK &&
        (out_argument == NULL || out_argument->kind != MYLITE_SQL_AST_USER_VARIABLE)) {
        set_parse_error(database, NULL);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = sys_table_exists_value(database, schema_name, table_name, &exists_value);
    }
    if (rc == MYLITE_OK) {
        output.value = exists_value;
        output.value_size = strlen(exists_value);
        output.has_value_size = true;
        rc = set_session_user_variable(
            database,
            out_argument,
            &output,
            MYLITE_SESSION_USER_VARIABLE_VALUE_STRING
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    return finish_sys_empty_ok_result(database, out_result);
}

static int execute_sys_execute_prepared_stmt_procedure(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *arguments,
    mylite_result **out_result
) {
    struct session_scalar_cell query = {0};
    char saved_schema[MYLITE_SESSION_SCHEMA_CAPACITY];
    char *sql = NULL;
    size_t sql_size = 0U;
    bool saved_has_schema = database->session.has_selected_schema;
    int written =
        snprintf(saved_schema, sizeof(saved_schema), "%s", database->session.selected_schema);
    int rc = MYLITE_OK;

    if (written < 0 || (size_t)written >= sizeof(saved_schema)) {
        set_identifier_too_long_error(database, "database");
        return MYLITE_ERROR;
    }

    rc = session_scalar_value(database, stored_procedure_argument_at(arguments, 0U), &query);
    if (rc == MYLITE_OK) {
        rc = copy_session_scalar_sql_text(database, &query, "NULL", &sql, &sql_size);
    }
    if (rc == MYLITE_OK) {
        written = snprintf(
            database->session.selected_schema,
            sizeof(database->session.selected_schema),
            "sys"
        );
        if (written < 0 || (size_t)written >= sizeof(database->session.selected_schema)) {
            set_identifier_too_long_error(database, "database");
            rc = MYLITE_ERROR;
        } else {
            database->session.has_selected_schema = true;
        }
    }
    if (rc == MYLITE_OK) {
        rc = execute_expanded_prepared_statement_sql(database, sql, sql_size, out_result);
    }

    database->session.has_selected_schema = saved_has_schema;
    memcpy(
        database->session.selected_schema,
        saved_schema,
        sizeof(database->session.selected_schema)
    );
    session_scalar_cell_deinit(&query);
    free(sql);
    return rc;
}

static int execute_sys_summary_procedure(
    struct mylite_db *database,
    const struct sys_stored_procedure_definition *definition,
    mylite_result **out_result
) {
    mylite_result *result = NULL;
    const char *values[] = {definition == NULL ? "" : definition->summary};
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }
    rc = append_sys_result_columns(database, result, definition->columns, definition->column_count);
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int execute_sys_empty_result_procedure(
    struct mylite_db *database,
    const struct sys_stored_procedure_definition *definition,
    mylite_result **out_result
) {
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }
    rc = append_sys_result_columns(database, result, definition->columns, definition->column_count);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int execute_sys_unsupported_procedure(
    struct mylite_db *database,
    const struct sys_stored_procedure_definition *definition
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "sys.%s() requires live Performance Schema instrumentation and is not supported by MyLite",
        definition == NULL ? "" : definition->name
    );

    if (written < 0 || (size_t)written >= sizeof(message)) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_not_supported_yet,
        "42000",
        message
    );
    return MYLITE_ERROR;
}

static int append_sys_result_columns(
    struct mylite_db *database,
    mylite_result *result,
    const char *const *columns,
    size_t column_count
) {
    int rc = MYLITE_OK;

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }

    return rc;
}

static int copy_sys_procedure_string_argument(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *argument,
    char *buffer,
    size_t buffer_size
) {
    struct session_scalar_cell value = {0};
    const char *source = NULL;
    size_t source_size = 0U;
    int rc = MYLITE_OK;

    if (buffer == NULL || buffer_size == 0U) {
        return MYLITE_MISUSE;
    }
    buffer[0] = '\0';
    rc = session_scalar_value(database, argument, &value);
    if (rc == MYLITE_OK && value.value != NULL) {
        source = value.value;
        source_size = value.has_value_size ? value.value_size : strlen(value.value);
        if (source_size >= buffer_size) {
            set_identifier_too_long_error(database, "identifier");
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && source != NULL) {
        memcpy(buffer, source, source_size);
        buffer[source_size] = '\0';
    }

    session_scalar_cell_deinit(&value);
    return rc;
}

static int sys_table_exists_value(
    struct mylite_db *database,
    const char *schema_name,
    const char *table_name,
    const char **out_value
) {
    struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_catalog_table_descriptor table = {0};
    bool found = false;
    int rc = MYLITE_OK;

    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = "";
    if (schema_name == NULL || schema_name[0] == '\0' || table_name == NULL ||
        table_name[0] == '\0') {
        return MYLITE_OK;
    }

    rc = mylite_temporary_catalog_try_read_table_by_name(
        &database->session.temporary_catalog,
        schema_name,
        table_name,
        &table,
        &found
    );
    if (rc != MYLITE_OK || found) {
        *out_value = found ? "TEMPORARY" : "";
        return rc;
    }

    rc = mylite_catalog_try_read_schema_by_name(database, schema_name, &schema, &found);
    if (rc != MYLITE_OK || !found) {
        return rc;
    }
    rc = mylite_catalog_try_read_table_by_name(
        database,
        schema.schema_id,
        table_name,
        &table,
        &found
    );
    if (rc != MYLITE_OK || !found) {
        return rc;
    }

    *out_value = table.kind == MYLITE_CATALOG_TABLE_KIND_VIEW ? "VIEW" : "BASE TABLE";
    return MYLITE_OK;
}

static int finish_sys_empty_ok_result(struct mylite_db *database, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }
    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int resolve_stored_procedure_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *name_node,
    struct stored_procedure_target *out_target
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t part_count = 0U;
    bool is_sys_target = false;
    int rc = MYLITE_OK;

    if (out_target == NULL) {
        return MYLITE_MISUSE;
    }

    *out_target = (struct stored_procedure_target){0};
    rc =
        try_resolve_sys_stored_procedure_target(database, name_node, out_target, &is_sys_target);
    if (rc != MYLITE_OK || is_sys_target) {
        return rc;
    }

    rc =
        collect_identifier_parts(name_node, parts, table_name_part_capacity, &part_count, database);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = resolve_table_name(database, name_node, &out_target->resolution);
    if (rc != MYLITE_OK) {
        return rc;
    }

    out_target->was_schema_qualified = part_count == 2U;
    return MYLITE_OK;
}

static int try_resolve_sys_stored_procedure_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *name_node,
    struct stored_procedure_target *out_target,
    bool *out_found
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    const struct mylite_execution_catalog_builtin_schema *schema = NULL;
    size_t part_count = 0U;
    int rc = MYLITE_OK;

    if (out_target == NULL || out_found == NULL) {
        return MYLITE_MISUSE;
    }
    *out_found = false;
    rc =
        collect_identifier_parts(name_node, parts, table_name_part_capacity, &part_count, database);
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (part_count == 2U && mylite_execution_text_equals_ascii_case_insensitive(parts[0], "sys")) {
        schema = find_builtin_schema_descriptor(parts[0]);
        if (schema == NULL) {
            return MYLITE_OK;
        }
        rc = fill_builtin_stored_procedure_resolution(schema, parts[1], &out_target->resolution);
        out_target->was_schema_qualified = true;
        *out_found = rc == MYLITE_OK;
        return rc;
    }
    if (part_count == 1U && database != NULL && database->session.has_selected_schema &&
        mylite_execution_text_equals_ascii_case_insensitive(
            database->session.selected_schema,
            "sys"
        )) {
        schema = find_builtin_schema_descriptor(database->session.selected_schema);
        if (schema == NULL) {
            return MYLITE_OK;
        }
        rc = fill_builtin_stored_procedure_resolution(schema, parts[0], &out_target->resolution);
        out_target->was_schema_qualified = false;
        *out_found = rc == MYLITE_OK;
        return rc;
    }

    return MYLITE_OK;
}

static int fill_builtin_stored_procedure_resolution(
    const struct mylite_execution_catalog_builtin_schema *schema,
    const char *table_name,
    struct table_name_resolution *out_resolution
) {
    int written = 0;

    if (schema == NULL || table_name == NULL || out_resolution == NULL) {
        return MYLITE_MISUSE;
    }
    *out_resolution = (struct table_name_resolution){0};
    written = snprintf(
        out_resolution->schema.name,
        sizeof(out_resolution->schema.name),
        "%s",
        schema->name
    );
    if (written < 0 || (size_t)written >= sizeof(out_resolution->schema.name)) {
        return MYLITE_ERROR;
    }
    written = snprintf(
        out_resolution->schema.default_charset,
        sizeof(out_resolution->schema.default_charset),
        "%s",
        schema->default_charset
    );
    if (written < 0 || (size_t)written >= sizeof(out_resolution->schema.default_charset)) {
        return MYLITE_ERROR;
    }
    written = snprintf(
        out_resolution->schema.default_collation,
        sizeof(out_resolution->schema.default_collation),
        "%s",
        schema->default_collation
    );
    if (written < 0 || (size_t)written >= sizeof(out_resolution->schema.default_collation)) {
        return MYLITE_ERROR;
    }
    written =
        snprintf(out_resolution->table_name, sizeof(out_resolution->table_name), "%s", table_name);
    return written < 0 || (size_t)written >= sizeof(out_resolution->table_name) ? MYLITE_ERROR
                                                                                 : MYLITE_OK;
}

static void set_sys_stored_procedure_argument_count_error(
    struct mylite_db *database,
    const struct stored_procedure_target *target,
    size_t expected,
    size_t actual
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Incorrect number of arguments for PROCEDURE %s.%s; expected %zu, got %zu",
        target == NULL ? "" : target->resolution.schema.name,
        target == NULL ? "" : target->resolution.table_name,
        expected,
        actual
    );

    if (written < 0 || (size_t)written >= sizeof(message)) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_incorrect_routine_argument_count,
        "42000",
        message
    );
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
