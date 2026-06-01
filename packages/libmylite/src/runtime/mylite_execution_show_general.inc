static int execute_show_tables_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct show_schema_resolution schema_resolution = {0};
    struct show_tables_filter_nodes nodes = {0};
    struct show_like_filter filter = {
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
    bool is_full = mylite_sql_ast_node_show_tables_is_full(statement) != 0;
    char *column_name = NULL;
    struct show_tables_context context = {0};
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    rc = resolve_show_tables_filter_nodes(database, statement, &nodes);
    if (rc == MYLITE_OK) {
        rc = resolve_show_statement_schema(database, nodes.schema, &schema_resolution);
    }
    if (rc == MYLITE_OK) {
        rc = make_show_like_filter(database, nodes.like, &filter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = build_show_tables_column_name(
            show_schema_resolution_name(&schema_resolution),
            &filter,
            &column_name
        );
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        rc = validate_show_tables_where_clause(database, nodes.where, column_name, is_full);
    }
    if (rc == MYLITE_OK) {
        rc = append_show_tables_result_columns(database, result, column_name, is_full);
    }
    if (rc == MYLITE_OK) {
        context.database = database;
        context.result = result;
        context.filter = &filter;
        context.where_clause = nodes.where;
        context.table_name_column = column_name;
        context.is_full = is_full;
        if (schema_resolution.builtin_directory != NULL) {
            rc = append_show_builtin_schema_tables(schema_resolution.builtin_directory, &context);
        } else {
            rc = mylite_catalog_for_each_table_in_schema(
                database,
                schema_resolution.schema.schema_id,
                append_show_table,
                &context
            );
        }
        if (rc != MYLITE_OK &&
            mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) == MYLITE_OK) {
            set_runtime_error(database, "failed to build SHOW TABLES result");
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        free(column_name);
        show_like_filter_deinit(&filter);
        return rc;
    }

    free(column_name);
    show_like_filter_deinit(&filter);
    return finish_successful_result(database, result, out_result);
}

static int resolve_show_statement_schema(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *schema_node,
    struct show_schema_resolution *out_resolution
) {
    *out_resolution = (struct show_schema_resolution){0};
    if (schema_node == NULL) {
        return resolve_show_statement_selected_schema(database, out_resolution);
    }
    return resolve_show_statement_explicit_schema(database, schema_node, out_resolution);
}

static int resolve_show_statement_selected_schema(
    struct mylite_db *database,
    struct show_schema_resolution *out_resolution
) {
    if (database == NULL || !database->session.has_selected_schema) {
        set_no_database_error(database);
        return MYLITE_ERROR;
    }

    out_resolution->builtin_directory =
        find_builtin_schema_table_directory(database->session.selected_schema);
    if (out_resolution->builtin_directory != NULL) {
        return MYLITE_OK;
    }
    return resolve_selected_schema(database, &out_resolution->schema);
}

static int resolve_show_statement_explicit_schema(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *schema_node,
    struct show_schema_resolution *out_resolution
) {
    char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = copy_identifier_text(schema_node, schema_name, sizeof(schema_name), database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (mylite_catalog_name_is_reserved(schema_name)) {
        set_reserved_name_error(database, "database", schema_name);
        return MYLITE_ERROR;
    }

    out_resolution->builtin_directory = find_builtin_schema_table_directory(schema_name);
    if (out_resolution->builtin_directory != NULL) {
        return MYLITE_OK;
    }
    return resolve_schema_name(database, schema_name, &out_resolution->schema);
}

static const char *show_schema_resolution_name(const struct show_schema_resolution *resolution) {
    if (resolution->builtin_directory != NULL) {
        return resolution->builtin_directory->schema_name;
    }
    return resolution->schema.name;
}

static int append_show_tables_result_columns(
    struct mylite_db *database,
    mylite_result *result,
    const char *column_name,
    bool is_full
) {
    int rc = mylite_result_append_column(result, column_name);
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }
    if (!is_full) {
        return MYLITE_OK;
    }

    rc = mylite_result_append_column(result, "Table_type");
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }
    return rc;
}

static int resolve_show_tables_filter_nodes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct show_tables_filter_nodes *out_nodes
) {
    const struct mylite_sql_ast_node *first_child = child_at(statement, 0U);
    const struct mylite_sql_ast_node *second_child = child_at(statement, 1U);
    const struct mylite_sql_ast_node *schema_node = first_child;
    const struct mylite_sql_ast_node *filter_node = second_child;

    if (out_nodes == NULL) {
        return MYLITE_MISUSE;
    }
    *out_nodes = (struct show_tables_filter_nodes){0};
    if (first_child != NULL && (first_child->kind == MYLITE_SQL_AST_LITERAL ||
                                first_child->kind == MYLITE_SQL_AST_WHERE_CLAUSE)) {
        schema_node = NULL;
        filter_node = first_child;
    }
    if (filter_node != NULL && filter_node->kind == MYLITE_SQL_AST_LITERAL) {
        out_nodes->like = filter_node;
    } else if (filter_node != NULL && filter_node->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
        out_nodes->where = filter_node;
    } else if (filter_node != NULL) {
        set_runtime_error(database, "invalid SHOW TABLES filter");
        return MYLITE_ERROR;
    }

    out_nodes->schema = schema_node;
    return MYLITE_OK;
}

static int execute_show_table_status_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct show_schema_resolution schema_resolution = {0};
    struct show_table_status_filter_nodes nodes = {0};
    struct show_like_filter filter = {
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
    struct show_table_status_context context = {0};
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    rc = resolve_show_table_status_filter_nodes(database, statement, &nodes);
    if (rc == MYLITE_OK) {
        rc = resolve_show_statement_schema(database, nodes.schema, &schema_resolution);
    }
    if (rc == MYLITE_OK) {
        rc = make_show_like_filter(database, nodes.like, &filter);
    }
    if (rc == MYLITE_OK) {
        rc = validate_show_table_status_where_clause(database, nodes.where);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_table_status_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, show_table_status_result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        context.database = database;
        context.result = result;
        context.filter = &filter;
        context.where_clause = nodes.where;
        if (schema_resolution.builtin_directory != NULL) {
            rc = append_show_builtin_schema_table_status(
                schema_resolution.builtin_directory,
                &context
            );
        } else {
            rc = mylite_catalog_for_each_table_in_schema(
                database,
                schema_resolution.schema.schema_id,
                append_show_table_status,
                &context
            );
        }
        if (rc != MYLITE_OK &&
            mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) == MYLITE_OK) {
            set_runtime_error(database, "failed to build SHOW TABLE STATUS result");
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

static int resolve_show_table_status_filter_nodes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct show_table_status_filter_nodes *out_nodes
) {
    const struct mylite_sql_ast_node *first_child = child_at(statement, 0U);
    const struct mylite_sql_ast_node *second_child = child_at(statement, 1U);
    const struct mylite_sql_ast_node *schema_node = first_child;
    const struct mylite_sql_ast_node *filter_node = second_child;

    if (out_nodes == NULL) {
        return MYLITE_MISUSE;
    }
    *out_nodes = (struct show_table_status_filter_nodes){0};
    if (first_child != NULL && (first_child->kind == MYLITE_SQL_AST_LITERAL ||
                                first_child->kind == MYLITE_SQL_AST_WHERE_CLAUSE)) {
        schema_node = NULL;
        filter_node = first_child;
    }
    if (filter_node != NULL && filter_node->kind == MYLITE_SQL_AST_LITERAL) {
        out_nodes->like = filter_node;
    } else if (filter_node != NULL && filter_node->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
        out_nodes->where = filter_node;
    } else if (filter_node != NULL) {
        set_runtime_error(database, "invalid SHOW TABLE STATUS filter");
        return MYLITE_ERROR;
    }

    out_nodes->schema = schema_node;
    return MYLITE_OK;
}

static int execute_show_character_set_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[show_character_set_result_column_count] = {
        "Charset",
        "Description",
        "Default collation",
        "Maxlen",
    };
    struct show_like_filter filter = {
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    rc = make_show_like_filter(database, child_at(statement, 0U), &filter);
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_character_set_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t row_index = 0U;
         rc == MYLITE_OK && row_index < mylite_execution_catalog_mysql_character_set_count();
         ++row_index) {
        const struct mylite_execution_catalog_character_set *charset =
            mylite_execution_catalog_mysql_character_set_at(row_index);
        const char *const values[show_character_set_result_column_count] = {
            charset->name,
            charset->description,
            charset->default_collation,
            charset->maxlen,
        };

        if (show_like_filter_matches(&filter, values[0], false)) {
            rc = mylite_result_append_text_row(result, values);
            if (rc != MYLITE_OK) {
                set_nomem_error(database);
            }
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

static int execute_show_collation_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[show_collation_result_column_count] = {
        "Collation",
        "Charset",
        "Id",
        "Default",
        "Compiled",
        "Sortlen",
        "Pad_attribute",
    };
    struct show_like_filter filter = {
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    rc = make_show_like_filter(database, child_at(statement, 0U), &filter);
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_collation_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t row_index = 0U;
         rc == MYLITE_OK && row_index < mylite_execution_catalog_mysql_collation_count();
         ++row_index) {
        const struct mylite_execution_catalog_collation *collation =
            mylite_execution_catalog_mysql_collation_at(row_index);
        const char *const values[show_collation_result_column_count] = {
            collation->name,
            collation->character_set,
            collation->id,
            collation->is_default,
            collation->compiled,
            collation->sortlen,
            collation->pad_attribute,
        };

        if (show_like_filter_matches(&filter, values[0], false)) {
            rc = mylite_result_append_text_row(result, values);
            if (rc != MYLITE_OK) {
                set_nomem_error(database);
            }
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

static int execute_show_variables_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[2] = {"Variable_name", "Value"};
    const struct mylite_sql_ast_node *first_child = child_at(statement, 0U);
    const struct mylite_sql_ast_node *scope = NULL;
    const struct mylite_sql_ast_node *like_pattern = NULL;
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *filter_node = NULL;
    bool global_scope = false;
    struct show_like_filter filter = {
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    if (first_child != NULL && first_child->kind == MYLITE_SQL_AST_IDENTIFIER) {
        scope = first_child;
        filter_node = child_at(statement, 1U);
    } else {
        filter_node = first_child;
    }
    if (filter_node != NULL && filter_node->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
        where_clause = filter_node;
    } else {
        like_pattern = filter_node;
    }
    global_scope = show_variables_scope_is_global(scope);

    rc = make_show_like_filter(database, like_pattern, &filter);
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < sizeof(result_columns) / sizeof(result_columns[0]);
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_system_variable_descriptor_count();
         ++index) {
        rc = append_show_variable(
            database,
            result,
            &filter,
            where_clause,
            global_scope,
            mylite_execution_system_variable_descriptor_at(index)
        );
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        show_like_filter_deinit(&filter);
        return rc;
    }

    show_like_filter_deinit(&filter);
    return finish_successful_result(database, result, out_result);
}

static int execute_show_status_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[2] = {"Variable_name", "Value"};
    const struct mylite_sql_ast_node *first_child = child_at(statement, 0U);
    const struct mylite_sql_ast_node *scope = NULL;
    const struct mylite_sql_ast_node *like_pattern = NULL;
    bool global_scope = false;
    struct show_like_filter filter = {
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    if (first_child != NULL && first_child->kind == MYLITE_SQL_AST_IDENTIFIER) {
        scope = first_child;
        like_pattern = child_at(statement, 1U);
    } else {
        like_pattern = first_child;
    }
    global_scope = show_variables_scope_is_global(scope);

    rc = make_show_like_filter(database, like_pattern, &filter);
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < sizeof(result_columns) / sizeof(result_columns[0]);
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_show_status_descriptor_count();
         ++index) {
        rc = append_show_status(
            database,
            result,
            &filter,
            global_scope,
            mylite_execution_show_status_descriptor_at(index)
        );
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        show_like_filter_deinit(&filter);
        return rc;
    }

    show_like_filter_deinit(&filter);
    return finish_successful_result(database, result, out_result);
}

static int append_show_status(
    struct mylite_db *database,
    mylite_result *result,
    const struct show_like_filter *filter,
    bool global_scope,
    const struct mylite_execution_show_status_descriptor *descriptor
) {
    const char *values[2] = {NULL, NULL};
    int rc = MYLITE_OK;

    if (!show_status_descriptor_visible(descriptor, global_scope)) {
        return MYLITE_OK;
    }
    if (!show_like_filter_matches(filter, descriptor->name, false)) {
        return MYLITE_OK;
    }

    values[0] = descriptor->name;
    values[1] = descriptor->value;
    rc = mylite_result_append_text_row(result, values);
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }

    return rc;
}

static bool show_status_descriptor_visible(
    const struct mylite_execution_show_status_descriptor *descriptor,
    bool global_scope
) {
    if (descriptor == NULL) {
        return false;
    }
    if (global_scope) {
        return descriptor->show_global;
    }
    return descriptor->show_session;
}

static bool show_variables_scope_is_global(const struct mylite_sql_ast_node *scope) {
    static const char expected[] = "global";

    if (scope == NULL) {
        return false;
    }
    if (scope->span.text == NULL || scope->span.length != sizeof(expected) - 1U) {
        return false;
    }
    for (size_t index = 0U; index < sizeof(expected) - 1U; ++index) {
        if (ascii_lower((unsigned char)scope->span.text[index]) != expected[index]) {
            return false;
        }
    }
    return true;
}

static int append_show_variable(
    struct mylite_db *database,
    mylite_result *result,
    const struct show_like_filter *filter,
    const struct mylite_sql_ast_node *where_clause,
    bool global_scope,
    const struct mylite_execution_system_variable_descriptor *descriptor
) {
    char integer_buffer[integer_text_capacity];
    const char *values[2] = {NULL, NULL};
    struct show_variables_where_row row = {0};
    bool where_matches = true;
    int rc = MYLITE_OK;

    if (!show_variable_descriptor_visible(descriptor, global_scope)) {
        return MYLITE_OK;
    }
    if (!show_like_filter_matches(filter, descriptor->name, false)) {
        return MYLITE_OK;
    }

    values[0] = descriptor->name;
    rc = show_system_variable_value(
        database,
        descriptor->kind,
        global_scope,
        integer_buffer,
        sizeof(integer_buffer),
        &values[1]
    );
    if (rc == MYLITE_OK && where_clause != NULL) {
        row.variable_name = values[0];
        row.value = values[1];
        rc = show_variables_where_clause_matches(database, where_clause, &row, &where_matches);
    }
    if (rc == MYLITE_OK && !where_matches) {
        return MYLITE_OK;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }

    return rc;
}

static bool show_variable_descriptor_visible(
    const struct mylite_execution_system_variable_descriptor *descriptor,
    bool global_scope
) {
    if (descriptor == NULL) {
        return false;
    }
    if (global_scope) {
        return descriptor->show_global;
    }
    return descriptor->show_session;
}

static int show_variables_where_clause_matches(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const struct show_variables_where_row *row,
    bool *out_matches
) {
    enum show_variables_where_truth truth = SHOW_VARIABLES_WHERE_FALSE;
    int rc = MYLITE_OK;

    if (where_clause == NULL || where_clause->kind != MYLITE_SQL_AST_WHERE_CLAUSE || row == NULL ||
        out_matches == NULL) {
        set_runtime_error(database, "invalid SHOW VARIABLES WHERE predicate");
        return MYLITE_ERROR;
    }

    *out_matches = false;
    rc = evaluate_show_variables_where_predicate(database, child_at(where_clause, 0U), row, &truth);
    if (rc == MYLITE_OK) {
        *out_matches = truth == SHOW_VARIABLES_WHERE_TRUE;
    }
    return rc;
}

static int evaluate_show_variables_where_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const struct show_variables_where_row *row,
    enum show_variables_where_truth *out_truth
) {
    struct show_variables_where_frame_stack frame_stack = {0};
    struct show_variables_where_truth_stack truth_stack = {0};
    struct show_variables_where_eval_frame frame = {
        .node = predicate,
        .action = SHOW_VARIABLES_WHERE_VISIT,
    };
    int rc = MYLITE_OK;

    if (predicate == NULL || row == NULL || out_truth == NULL) {
        set_runtime_error(database, "invalid SHOW VARIABLES WHERE predicate");
        return MYLITE_ERROR;
    }

    rc = show_variables_where_frame_stack_push(database, &frame_stack, frame);
    while (rc == MYLITE_OK && show_variables_where_frame_stack_pop(&frame_stack, &frame)) {
        const struct mylite_sql_ast_node *current = unwrap_parenthesized_predicate(frame.node);

        if (current == NULL) {
            set_runtime_error(database, "invalid SHOW VARIABLES WHERE predicate");
            rc = MYLITE_ERROR;
        } else if (frame.action == SHOW_VARIABLES_WHERE_VISIT) {
            rc = visit_show_variables_where_predicate(database, &frame_stack, current);
        } else {
            rc = evaluate_show_variables_where_frame(database, current, row, &truth_stack);
        }
    }

    if (rc == MYLITE_OK) {
        if (truth_stack.count != 1U ||
            !show_variables_where_truth_stack_pop(&truth_stack, out_truth)) {
            set_runtime_error(database, "invalid SHOW VARIABLES WHERE predicate state");
            rc = MYLITE_ERROR;
        }
    }

    show_variables_where_truth_stack_deinit(&truth_stack);
    show_variables_where_frame_stack_deinit(&frame_stack);
    return rc;
}

static int visit_show_variables_where_predicate(
    struct mylite_db *database,
    struct show_variables_where_frame_stack *frame_stack,
    const struct mylite_sql_ast_node *predicate
) {
    int rc = show_variables_where_frame_stack_push(
        database,
        frame_stack,
        (struct show_variables_where_eval_frame){
            .node = predicate,
            .action = SHOW_VARIABLES_WHERE_EVALUATE,
        }
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (predicate->kind == MYLITE_SQL_AST_NOT_PREDICATE) {
        return show_variables_where_frame_stack_push(
            database,
            frame_stack,
            (struct show_variables_where_eval_frame){
                .node = child_at(predicate, 0U),
                .action = SHOW_VARIABLES_WHERE_VISIT,
            }
        );
    }
    if (predicate->kind == MYLITE_SQL_AST_AND_PREDICATE ||
        predicate->kind == MYLITE_SQL_AST_OR_PREDICATE) {
        rc = show_variables_where_frame_stack_push(
            database,
            frame_stack,
            (struct show_variables_where_eval_frame){
                .node = child_at(predicate, 1U),
                .action = SHOW_VARIABLES_WHERE_VISIT,
            }
        );
        if (rc == MYLITE_OK) {
            rc = show_variables_where_frame_stack_push(
                database,
                frame_stack,
                (struct show_variables_where_eval_frame){
                    .node = child_at(predicate, 0U),
                    .action = SHOW_VARIABLES_WHERE_VISIT,
                }
            );
        }
    }
    return rc;
}

static int evaluate_show_variables_where_frame(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const struct show_variables_where_row *row,
    struct show_variables_where_truth_stack *truth_stack
) {
    enum show_variables_where_truth truth = SHOW_VARIABLES_WHERE_FALSE;
    int rc = MYLITE_OK;

    if (predicate->kind == MYLITE_SQL_AST_COMPARISON_PREDICATE) {
        rc = evaluate_show_variables_where_comparison_predicate(database, predicate, row, &truth);
    } else if (predicate->kind == MYLITE_SQL_AST_IN_PREDICATE) {
        rc = evaluate_show_variables_where_in_predicate(database, predicate, row, &truth);
    } else if (predicate->kind == MYLITE_SQL_AST_IS_NULL_PREDICATE) {
        rc = evaluate_show_variables_where_is_null_predicate(database, predicate, row, &truth);
    } else if (predicate->kind == MYLITE_SQL_AST_NOT_PREDICATE) {
        enum show_variables_where_truth child_truth = SHOW_VARIABLES_WHERE_FALSE;

        if (!show_variables_where_truth_stack_pop(truth_stack, &child_truth)) {
            set_runtime_error(database, "invalid SHOW VARIABLES WHERE predicate stack");
            return MYLITE_ERROR;
        }
        truth = negate_show_variables_where_truth(child_truth);
    } else if (
        predicate->kind == MYLITE_SQL_AST_AND_PREDICATE ||
        predicate->kind == MYLITE_SQL_AST_OR_PREDICATE
    ) {
        rc = evaluate_show_variables_where_logical_frame(database, predicate, truth_stack, &truth);
    } else if (predicate->kind == MYLITE_SQL_AST_XOR_PREDICATE) {
        set_unsupported_error(database, "SHOW VARIABLES WHERE does not support XOR predicates");
        return MYLITE_ERROR;
    } else {
        set_unsupported_error(database, "SHOW VARIABLES WHERE supports output-column predicates");
        return MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = show_variables_where_truth_stack_push(database, truth_stack, truth);
    }
    return rc;
}

static int evaluate_show_variables_where_logical_frame(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    struct show_variables_where_truth_stack *truth_stack,
    enum show_variables_where_truth *out_truth
) {
    enum show_variables_where_truth left = SHOW_VARIABLES_WHERE_FALSE;
    enum show_variables_where_truth right = SHOW_VARIABLES_WHERE_FALSE;

    if (!show_variables_where_truth_stack_pop(truth_stack, &right) ||
        !show_variables_where_truth_stack_pop(truth_stack, &left)) {
        set_runtime_error(database, "invalid SHOW VARIABLES WHERE predicate stack");
        return MYLITE_ERROR;
    }

    if (predicate->kind == MYLITE_SQL_AST_AND_PREDICATE) {
        *out_truth = and_show_variables_where_truth(left, right);
    } else {
        *out_truth = or_show_variables_where_truth(left, right);
    }
    return MYLITE_OK;
}

static int evaluate_show_variables_where_comparison_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const struct show_variables_where_row *row,
    enum show_variables_where_truth *out_truth
) {
    const struct mylite_sql_ast_node *left_node = child_at(predicate, 0U);
    const struct mylite_sql_ast_node *right_node = child_at(predicate, 1U);
    const char *left = NULL;
    enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(predicate);
    int rc = show_variables_where_column_value(database, left_node, row, &left);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_REGEXP ||
        operator_kind == MYLITE_SQL_AST_OPERATOR_RLIKE) {
        set_unsupported_error(database, "SHOW VARIABLES WHERE does not support REGEXP predicates");
        return MYLITE_ERROR;
    }
    if (operator_kind == MYLITE_SQL_AST_OPERATOR_LIKE) {
        char *pattern = NULL;

        rc = decode_show_variables_where_string_literal(database, right_node, &pattern);
        if (rc == MYLITE_OK) {
            bool matches =
                show_like_pattern_matches(pattern, strlen(pattern), left, strlen(left), false);

            if (matches) {
                *out_truth = SHOW_VARIABLES_WHERE_TRUE;
            } else {
                *out_truth = SHOW_VARIABLES_WHERE_FALSE;
            }
        }
        free(pattern);
        return rc;
    }

    return compare_show_variables_where_literal(
        database,
        operator_kind,
        left,
        right_node,
        out_truth
    );
}

static int evaluate_show_variables_where_in_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const struct show_variables_where_row *row,
    enum show_variables_where_truth *out_truth
) {
    const struct mylite_sql_ast_node *left_node = child_at(predicate, 0U);
    const struct mylite_sql_ast_node *value_list = child_at(predicate, 1U);
    const char *left = NULL;
    size_t value_count = 0U;
    bool saw_null = false;
    int rc = show_variables_where_column_value(database, left_node, row, &left);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (value_list == NULL || value_list->kind != MYLITE_SQL_AST_PREDICATE_VALUE_LIST) {
        set_runtime_error(database, "invalid SHOW VARIABLES WHERE IN list");
        return MYLITE_ERROR;
    }

    value_count = mylite_sql_ast_node_child_count(value_list);
    for (size_t index = 0U; index < value_count; ++index) {
        const struct mylite_sql_ast_node *value_node = child_at(value_list, index);
        enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
        char *right = NULL;

        if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL) {
            set_unsupported_error(
                database,
                "SHOW VARIABLES WHERE IN supports only string and NULL literals"
            );
            return MYLITE_ERROR;
        }

        literal_kind = mylite_sql_ast_node_literal_kind(value_node);
        if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
            saw_null = true;
            continue;
        }
        rc = decode_show_variables_where_string_literal(database, value_node, &right);
        if (rc == MYLITE_OK && compare_ascii_case_insensitive_text(left, right) == 0) {
            *out_truth = SHOW_VARIABLES_WHERE_TRUE;
            free(right);
            return MYLITE_OK;
        }
        free(right);
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    if (saw_null) {
        *out_truth = SHOW_VARIABLES_WHERE_UNKNOWN;
    } else {
        *out_truth = SHOW_VARIABLES_WHERE_FALSE;
    }
    return MYLITE_OK;
}

static int evaluate_show_variables_where_is_null_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const struct show_variables_where_row *row,
    enum show_variables_where_truth *out_truth
) {
    const char *value = NULL;
    int rc = show_variables_where_column_value(database, child_at(predicate, 0U), row, &value);

    if (rc != MYLITE_OK) {
        return rc;
    }
    *out_truth = mylite_sql_ast_node_operator(predicate) == MYLITE_SQL_AST_OPERATOR_IS_NOT_NULL
                     ? SHOW_VARIABLES_WHERE_TRUE
                     : SHOW_VARIABLES_WHERE_FALSE;
    return MYLITE_OK;
}

static enum show_variables_where_truth negate_show_variables_where_truth(
    enum show_variables_where_truth truth
) {
    switch (truth) {
    case SHOW_VARIABLES_WHERE_TRUE:
        return SHOW_VARIABLES_WHERE_FALSE;
    case SHOW_VARIABLES_WHERE_FALSE:
        return SHOW_VARIABLES_WHERE_TRUE;
    case SHOW_VARIABLES_WHERE_UNKNOWN:
    default:
        return SHOW_VARIABLES_WHERE_UNKNOWN;
    }
}

static enum show_variables_where_truth and_show_variables_where_truth(
    enum show_variables_where_truth left,
    enum show_variables_where_truth right
) {
    if (left == SHOW_VARIABLES_WHERE_FALSE || right == SHOW_VARIABLES_WHERE_FALSE) {
        return SHOW_VARIABLES_WHERE_FALSE;
    }
    if (left == SHOW_VARIABLES_WHERE_TRUE && right == SHOW_VARIABLES_WHERE_TRUE) {
        return SHOW_VARIABLES_WHERE_TRUE;
    }
    return SHOW_VARIABLES_WHERE_UNKNOWN;
}

static enum show_variables_where_truth or_show_variables_where_truth(
    enum show_variables_where_truth left,
    enum show_variables_where_truth right
) {
    if (left == SHOW_VARIABLES_WHERE_TRUE || right == SHOW_VARIABLES_WHERE_TRUE) {
        return SHOW_VARIABLES_WHERE_TRUE;
    }
    if (left == SHOW_VARIABLES_WHERE_FALSE && right == SHOW_VARIABLES_WHERE_FALSE) {
        return SHOW_VARIABLES_WHERE_FALSE;
    }
    return SHOW_VARIABLES_WHERE_UNKNOWN;
}

static int show_variables_where_column_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct show_variables_where_row *row,
    const char **out_value
) {
    enum show_variables_where_column column = SHOW_VARIABLES_WHERE_COLUMN_NONE;
    int rc = resolve_show_variables_where_column(database, column_node, &column);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (row == NULL || out_value == NULL) {
        set_runtime_error(database, "invalid SHOW VARIABLES WHERE row");
        return MYLITE_ERROR;
    }

    switch (column) {
    case SHOW_VARIABLES_WHERE_COLUMN_VARIABLE_NAME:
        *out_value = row->variable_name;
        return MYLITE_OK;
    case SHOW_VARIABLES_WHERE_COLUMN_VALUE:
        *out_value = row->value;
        return MYLITE_OK;
    case SHOW_VARIABLES_WHERE_COLUMN_NONE:
    default:
        set_runtime_error(database, "invalid SHOW VARIABLES WHERE column");
        return MYLITE_ERROR;
    }
}

static int resolve_show_variables_where_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    enum show_variables_where_column *out_column
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char *display_text = NULL;
    size_t part_count = 0U;
    int rc = MYLITE_OK;

    if (out_column == NULL) {
        return MYLITE_MISUSE;
    }
    *out_column = SHOW_VARIABLES_WHERE_COLUMN_NONE;
    if (column_node == NULL || (column_node->kind != MYLITE_SQL_AST_IDENTIFIER &&
                                column_node->kind != MYLITE_SQL_AST_QUALIFIED_IDENTIFIER)) {
        set_unsupported_error(database, "SHOW VARIABLES WHERE supports only output columns");
        return MYLITE_ERROR;
    }

    rc = collect_identifier_parts(
        column_node,
        parts,
        table_name_part_capacity,
        &part_count,
        database
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (part_count == 1U && text_equals_ascii_case_insensitive(parts[0], "Variable_name")) {
        *out_column = SHOW_VARIABLES_WHERE_COLUMN_VARIABLE_NAME;
        return MYLITE_OK;
    }
    if (part_count == 1U && text_equals_ascii_case_insensitive(parts[0], "Value")) {
        *out_column = SHOW_VARIABLES_WHERE_COLUMN_VALUE;
        return MYLITE_OK;
    }

    rc = show_variables_column_reference_text(database, column_node, &display_text);
    if (rc == MYLITE_OK) {
        set_unknown_where_column_error(database, display_text);
        rc = MYLITE_ERROR;
    }
    free(display_text);
    return rc;
}

static int show_variables_column_reference_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    char **out_text
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    struct mylite_dynamic_string string;
    size_t part_count = 0U;
    int rc = collect_identifier_parts(
        column_node,
        parts,
        table_name_part_capacity,
        &part_count,
        database
    );

    if (out_text == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    if (rc != MYLITE_OK) {
        return rc;
    }

    mylite_dynamic_string_init(&string);
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < part_count; ++part_index) {
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append_char(&string, '.');
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(&string, parts[part_index]);
        }
    }
    if (rc == MYLITE_OK) {
        *out_text = mylite_dynamic_string_take(&string);
        if (*out_text == NULL) {
            rc = MYLITE_NOMEM;
        }
    }
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    mylite_dynamic_string_deinit(&string);
    return rc;
}

static int compare_show_variables_where_literal(
    struct mylite_db *database,
    enum mylite_sql_ast_operator operator_kind,
    const char *left,
    const struct mylite_sql_ast_node *right,
    enum show_variables_where_truth *out_truth
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    char *right_text = NULL;
    int comparison = 0;
    int rc = MYLITE_OK;

    if (left == NULL || right == NULL || out_truth == NULL) {
        set_runtime_error(database, "invalid SHOW VARIABLES WHERE comparison");
        return MYLITE_ERROR;
    }
    if (right->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "SHOW VARIABLES WHERE comparisons support only string and NULL literals"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(right);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        *out_truth = operator_kind == MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL
                         ? SHOW_VARIABLES_WHERE_FALSE
                         : SHOW_VARIABLES_WHERE_UNKNOWN;
        return MYLITE_OK;
    }

    rc = decode_show_variables_where_string_literal(database, right, &right_text);
    if (rc != MYLITE_OK) {
        free(right_text);
        return rc;
    }

    comparison = compare_ascii_case_insensitive_text(left, right_text);
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_EQUAL:
    case MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL:
        *out_truth = comparison == 0 ? SHOW_VARIABLES_WHERE_TRUE : SHOW_VARIABLES_WHERE_FALSE;
        break;
    case MYLITE_SQL_AST_OPERATOR_NOT_EQUAL:
        *out_truth = comparison != 0 ? SHOW_VARIABLES_WHERE_TRUE : SHOW_VARIABLES_WHERE_FALSE;
        break;
    case MYLITE_SQL_AST_OPERATOR_LESS:
        *out_truth = comparison < 0 ? SHOW_VARIABLES_WHERE_TRUE : SHOW_VARIABLES_WHERE_FALSE;
        break;
    case MYLITE_SQL_AST_OPERATOR_LESS_EQUAL:
        *out_truth = comparison <= 0 ? SHOW_VARIABLES_WHERE_TRUE : SHOW_VARIABLES_WHERE_FALSE;
        break;
    case MYLITE_SQL_AST_OPERATOR_GREATER:
        *out_truth = comparison > 0 ? SHOW_VARIABLES_WHERE_TRUE : SHOW_VARIABLES_WHERE_FALSE;
        break;
    case MYLITE_SQL_AST_OPERATOR_GREATER_EQUAL:
        *out_truth = comparison >= 0 ? SHOW_VARIABLES_WHERE_TRUE : SHOW_VARIABLES_WHERE_FALSE;
        break;
    default:
        set_unsupported_error(
            database,
            "SHOW VARIABLES WHERE comparison operator is not supported"
        );
        rc = MYLITE_ERROR;
        break;
    }

    free(right_text);
    return rc;
}

static int decode_show_variables_where_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal_node,
    char **out_text
) {
    size_t text_length = 0U;

    return decode_sql_string_literal(
        database,
        literal_node,
        "SHOW VARIABLES WHERE supports only string literal predicates",
        "SHOW VARIABLES WHERE string literals do not support NUL bytes",
        out_text,
        &text_length
    );
}

static void show_variables_where_frame_stack_deinit(
    struct show_variables_where_frame_stack *stack
) {
    if (stack == NULL) {
        return;
    }
    free(stack->items);
    *stack = (struct show_variables_where_frame_stack){0};
}

static int show_variables_where_frame_stack_push(
    struct mylite_db *database,
    struct show_variables_where_frame_stack *stack,
    struct show_variables_where_eval_frame frame
) {
    struct show_variables_where_eval_frame *grown_items = NULL;
    size_t new_capacity = stack->capacity == 0U ? if_stack_initial_capacity : stack->capacity * 2U;

    if (stack->count < stack->capacity) {
        stack->items[stack->count] = frame;
        ++stack->count;
        return MYLITE_OK;
    }
    if (new_capacity < stack->capacity || new_capacity > SIZE_MAX / sizeof(*grown_items)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    grown_items = realloc(stack->items, new_capacity * sizeof(*grown_items));
    if (grown_items == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    stack->items = grown_items;
    stack->capacity = new_capacity;
    stack->items[stack->count] = frame;
    ++stack->count;
    return MYLITE_OK;
}

static bool show_variables_where_frame_stack_pop(
    struct show_variables_where_frame_stack *stack,
    struct show_variables_where_eval_frame *out_frame
) {
    if (stack == NULL || stack->count == 0U) {
        return false;
    }
    --stack->count;
    *out_frame = stack->items[stack->count];
    return true;
}

static void show_variables_where_truth_stack_deinit(
    struct show_variables_where_truth_stack *stack
) {
    if (stack == NULL) {
        return;
    }
    free(stack->items);
    *stack = (struct show_variables_where_truth_stack){0};
}

static int show_variables_where_truth_stack_push(
    struct mylite_db *database,
    struct show_variables_where_truth_stack *stack,
    enum show_variables_where_truth truth
) {
    enum show_variables_where_truth *grown_items = NULL;
    size_t new_capacity = stack->capacity == 0U ? if_stack_initial_capacity : stack->capacity * 2U;

    if (stack->count < stack->capacity) {
        stack->items[stack->count] = truth;
        ++stack->count;
        return MYLITE_OK;
    }
    if (new_capacity < stack->capacity || new_capacity > SIZE_MAX / sizeof(*grown_items)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    grown_items = realloc(stack->items, new_capacity * sizeof(*grown_items));
    if (grown_items == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    stack->items = grown_items;
    stack->capacity = new_capacity;
    stack->items[stack->count] = truth;
    ++stack->count;
    return MYLITE_OK;
}

static bool show_variables_where_truth_stack_pop(
    struct show_variables_where_truth_stack *stack,
    enum show_variables_where_truth *out_truth
) {
    if (stack == NULL || stack->count == 0U) {
        return false;
    }
    --stack->count;
    *out_truth = stack->items[stack->count];
    return true;
}

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
        rc = mylite_result_append_column(result, result_columns[column_index]);
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
        rc = mylite_result_append_column(result, result_columns[column_index]);
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
        rc = mylite_result_append_column(result, result_columns[column_index]);
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
        rc = mylite_result_append_column(result, result_columns[column_index]);
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
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_processlist_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_show_processlist_row(database, context, statement, result);
    }
    if (rc == MYLITE_OK) {
        rc = append_show_processlist_warning(database);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int append_show_processlist_row(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
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
        "Query",
        "0",
        "init",
        NULL,
    };

    written = snprintf(
        connection_id_text,
        sizeof(connection_id_text),
        "%" PRIu64,
        database->session.connection_id
    );
    if (written < 0 || (size_t)written >= sizeof(connection_id_text)) {
        set_runtime_error(database, "failed to format SHOW PROCESSLIST Id");
        return MYLITE_ERROR;
    }
    if (database->session.has_selected_schema) {
        selected_schema = database->session.selected_schema;
    }
    values[show_processlist_db_column] = selected_schema;

    rc = format_show_processlist_user_host(database, user, sizeof(user), host, sizeof(host));
    if (rc == MYLITE_OK) {
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

static int format_show_processlist_user_host(
    struct mylite_db *database,
    char *user,
    size_t user_size,
    char *host,
    size_t host_size
) {
    const char *identity = database->session.client_user_identity;
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

static int execute_show_grants_statement(struct mylite_db *database, mylite_result **out_result) {
    static const char *const result_columns[show_grants_result_column_count] = {
        "Grants for root@%",
    };
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_grants_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
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
        rc = mylite_result_append_column(result, result_columns[column_index]);
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

static int execute_show_binary_log_status_statement(
    struct mylite_db *database,
    mylite_result **out_result
) {
    static const char *const result_columns[show_binary_log_status_result_column_count] = {
        "File",
        "Position",
        "Binlog_Do_DB",
        "Binlog_Ignore_DB",
        "Executed_Gtid_Set",
    };
    static const char *const values[show_binary_log_status_result_column_count] = {
        show_binary_log_file_name,
        show_binary_log_placeholder_position,
        "",
        "",
        "",
    };
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_binary_log_status_result_column_count;
         ++column_index) {
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
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_show_binary_logs_statement(
    struct mylite_db *database,
    mylite_result **out_result
) {
    static const char *const result_columns[show_binary_logs_result_column_count] = {
        "Log_name",
        "File_size",
        "Encrypted",
    };
    static const char *const values[show_binary_logs_result_column_count] = {
        show_binary_log_file_name,
        show_binary_log_placeholder_file_size,
        "No",
    };
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_binary_logs_result_column_count;
         ++column_index) {
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
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_show_replica_status_statement(
    struct mylite_db *database,
    mylite_result **out_result
) {
    static const char *const result_columns[show_replica_status_result_column_count] = {
        "Replica_IO_State",
        "Source_Host",
        "Source_User",
        "Source_Port",
        "Connect_Retry",
        "Source_Log_File",
        "Read_Source_Log_Pos",
        "Relay_Log_File",
        "Relay_Log_Pos",
        "Relay_Source_Log_File",
        "Replica_IO_Running",
        "Replica_SQL_Running",
        "Replicate_Do_DB",
        "Replicate_Ignore_DB",
        "Replicate_Do_Table",
        "Replicate_Ignore_Table",
        "Replicate_Wild_Do_Table",
        "Replicate_Wild_Ignore_Table",
        "Last_Errno",
        "Last_Error",
        "Skip_Counter",
        "Exec_Source_Log_Pos",
        "Relay_Log_Space",
        "Until_Condition",
        "Until_Log_File",
        "Until_Log_Pos",
        "Source_SSL_Allowed",
        "Source_SSL_CA_File",
        "Source_SSL_CA_Path",
        "Source_SSL_Cert",
        "Source_SSL_Cipher",
        "Source_SSL_Key",
        "Seconds_Behind_Source",
        "Source_SSL_Verify_Server_Cert",
        "Last_IO_Errno",
        "Last_IO_Error",
        "Last_SQL_Errno",
        "Last_SQL_Error",
        "Replicate_Ignore_Server_Ids",
        "Source_Server_Id",
        "Source_UUID",
        "Source_Info_File",
        "SQL_Delay",
        "SQL_Remaining_Delay",
        "Replica_SQL_Running_State",
        "Source_Retry_Count",
        "Source_Bind",
        "Last_IO_Error_Timestamp",
        "Last_SQL_Error_Timestamp",
        "Source_SSL_Crl",
        "Source_SSL_Crlpath",
        "Retrieved_Gtid_Set",
        "Executed_Gtid_Set",
        "Auto_Position",
        "Replicate_Rewrite_DB",
        "Channel_Name",
        "Source_TLS_Version",
        "Source_public_key_path",
        "Get_Source_public_key",
        "Network_Namespace",
    };
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_replica_status_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
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

static int execute_show_replicas_statement(struct mylite_db *database, mylite_result **out_result) {
    static const char *const result_columns[show_replicas_result_column_count] = {
        "Server_Id",
        "Host",
        "Port",
        "Source_Id",
        "Replica_UUID",
    };
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_replicas_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
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

static int execute_show_warnings_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[show_warnings_result_column_count] = {
        "Level",
        "Code",
        "Message",
    };
    struct planned_diagnostics_show_limit limit;
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    rc = plan_diagnostics_show_limit(database, child_at(statement, 0U), &limit);
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_warnings_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_show_diagnostics_rows(database, result, &limit);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_show_count_warnings_statement(
    struct mylite_db *database,
    mylite_result **out_result
) {
    static const char *const result_columns[show_count_warnings_result_column_count] = {
        "@@session.warning_count",
    };
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_count_warnings_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_show_count_warnings_row(database, result);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_show_errors_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[show_errors_result_column_count] = {
        "Level",
        "Code",
        "Message",
    };
    struct planned_diagnostics_show_limit limit;
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    rc = plan_diagnostics_show_limit(database, child_at(statement, 0U), &limit);
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_errors_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_show_errors_rows(database, result, &limit);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_show_count_errors_statement(
    struct mylite_db *database,
    mylite_result **out_result
) {
    static const char *const result_columns[show_count_errors_result_column_count] = {
        "@@session.error_count",
    };
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_count_errors_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_show_count_errors_row(database, result);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int plan_diagnostics_show_limit(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *limit_clause,
    struct planned_diagnostics_show_limit *out_limit
) {
    int rc = MYLITE_OK;

    if (out_limit == NULL) {
        set_runtime_error(database, "invalid diagnostics SHOW LIMIT plan");
        return MYLITE_ERROR;
    }

    out_limit->has_limit = false;
    out_limit->row_count = UINT64_MAX;
    out_limit->has_offset = false;
    out_limit->offset = 0U;
    if (limit_clause == NULL) {
        return MYLITE_OK;
    }
    if (limit_clause->kind != MYLITE_SQL_AST_LIMIT_CLAUSE) {
        set_unsupported_error(database, "diagnostics SHOW supports only literal LIMIT clauses");
        return MYLITE_ERROR;
    }

    rc = convert_diagnostics_show_limit_integer_literal(
        database,
        child_at(limit_clause, 0U),
        &out_limit->row_count
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    out_limit->has_limit = true;

    if (child_at(limit_clause, 1U) != NULL) {
        rc = convert_diagnostics_show_limit_integer_literal(
            database,
            child_at(limit_clause, 1U),
            &out_limit->offset
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        out_limit->has_offset = true;
    }

    return MYLITE_OK;
}

static int convert_diagnostics_show_limit_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_value
) {
    uint64_t value = 0U;
    int rc = MYLITE_OK;

    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_unsupported_error(
            database,
            "diagnostics SHOW supports only unsigned decimal integer LIMIT literals"
        );
        return MYLITE_ERROR;
    }

    rc = parse_unsigned_integer_literal(&value_node->span, &value);
    if (rc != MYLITE_OK) {
        set_limit_out_of_range_error(database);
        return MYLITE_ERROR;
    }

    *out_value = value;
    return MYLITE_OK;
}

static int append_show_diagnostics_rows(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_diagnostics_show_limit *limit
) {
    const struct mylite_diagnostics *diagnostics = &database->previous_diagnostics;
    uint64_t total_count = 0U;
    uint64_t start_index = 0U;
    uint64_t row_count = UINT64_MAX;
    uint64_t emitted_count = 0U;
    uint64_t condition_count = 0U;
    uint64_t warning_count = 0U;
    int rc = previous_diagnostics_condition_count(diagnostics, &condition_count);

    if (limit->has_offset) {
        start_index = limit->offset;
    }
    if (limit->has_limit) {
        row_count = limit->row_count;
    }
    if (rc == MYLITE_OK) {
        warning_count = (uint64_t)mylite_diagnostics_warning_count(diagnostics);
        total_count = condition_count + warning_count;
    }

    for (uint64_t index = start_index;
         rc == MYLITE_OK && index < total_count && emitted_count < row_count;
         ++index, ++emitted_count) {
        if (index < warning_count) {
            const struct mylite_diagnostic_record *warning =
                mylite_diagnostics_warning_at(diagnostics, (size_t)index);
            if (warning == NULL) {
                set_runtime_error(database, "invalid previous diagnostics warning index");
                rc = MYLITE_ERROR;
            } else {
                rc = append_show_diagnostics_row(database, result, warning->level, warning);
            }
        } else if (condition_count != 0U) {
            rc = append_show_diagnostics_row(database, result, "Error", &diagnostics->condition);
        }
    }

    return rc;
}

static int append_show_diagnostics_row(
    struct mylite_db *database,
    mylite_result *result,
    const char *level,
    const struct mylite_diagnostic_record *record
) {
    char code_text[integer_text_capacity];
    const char *values[show_warnings_result_column_count] = {
        level,
        code_text,
        record == NULL ? NULL : record->message,
    };
    int written = 0;
    int rc = MYLITE_OK;

    if (record == NULL) {
        set_runtime_error(database, "invalid diagnostics record");
        return MYLITE_ERROR;
    }

    written = snprintf(code_text, sizeof(code_text), "%d", record->code);
    if (written < 0 || (size_t)written >= sizeof(code_text)) {
        set_runtime_error(database, "failed to format diagnostic code");
        return MYLITE_ERROR;
    }

    rc = mylite_result_append_text_row(result, values);
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }
    return rc;
}

static int append_show_count_warnings_row(struct mylite_db *database, mylite_result *result) {
    const struct mylite_diagnostics *diagnostics = &database->previous_diagnostics;
    char count_text[integer_text_capacity];
    const char *values[show_count_warnings_result_column_count] = {count_text};
    uint64_t condition_count = 0U;
    uint64_t count = 0U;
    int rc = previous_diagnostics_condition_count(diagnostics, &condition_count);

    if (rc != MYLITE_OK) {
        return rc;
    }

    count = condition_count + (uint64_t)mylite_diagnostics_warning_count(diagnostics);
    rc = format_uint64(database, count, count_text, sizeof(count_text));
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }

    return rc;
}

static int append_show_errors_rows(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_diagnostics_show_limit *limit
) {
    const struct mylite_diagnostics *diagnostics = &database->previous_diagnostics;
    uint64_t start_index = 0U;
    uint64_t row_count = UINT64_MAX;
    uint64_t error_index = 0U;
    uint64_t emitted_count = 0U;
    int rc = MYLITE_OK;

    if (limit->has_offset) {
        start_index = limit->offset;
    }
    if (limit->has_limit) {
        row_count = limit->row_count;
    }
    if (row_count == 0U) {
        return MYLITE_OK;
    }

    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_diagnostics_warning_count(diagnostics);
         ++index) {
        const struct mylite_diagnostic_record *record =
            mylite_diagnostics_warning_at(diagnostics, index);

        if (!diagnostic_record_level_is_error(record)) {
            continue;
        }
        if (error_index >= start_index && emitted_count < row_count) {
            rc = append_show_diagnostics_row(database, result, "Error", record);
            ++emitted_count;
        }
        ++error_index;
    }
    if (rc == MYLITE_OK && diagnostics_has_error_condition(diagnostics) &&
        error_index >= start_index && emitted_count < row_count) {
        rc = append_show_diagnostics_row(database, result, "Error", &diagnostics->condition);
    }
    return rc;
}

static int append_show_count_errors_row(struct mylite_db *database, mylite_result *result) {
    const struct mylite_diagnostics *diagnostics = &database->previous_diagnostics;
    char count_text[integer_text_capacity];
    const char *values[show_count_errors_result_column_count] = {count_text};
    uint64_t count = 0U;
    int rc = previous_diagnostics_error_count(diagnostics, &count);

    if (rc == MYLITE_OK) {
        rc = format_uint64(database, count, count_text, sizeof(count_text));
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }

    return rc;
}

static int previous_diagnostics_condition_count(
    const struct mylite_diagnostics *diagnostics,
    uint64_t *out_count
) {
    uint64_t warning_count = 0U;

    if (diagnostics == NULL || out_count == NULL) {
        return MYLITE_MISUSE;
    }

    warning_count = (uint64_t)mylite_diagnostics_warning_count(diagnostics);
    if (diagnostics_has_error_condition(diagnostics) && warning_count == UINT64_MAX) {
        return MYLITE_NOMEM;
    }

    *out_count = 0U;
    if (diagnostics_has_error_condition(diagnostics)) {
        *out_count = 1U;
    }
    return MYLITE_OK;
}

static bool diagnostics_has_error_condition(const struct mylite_diagnostics *diagnostics) {
    if (diagnostics == NULL) {
        return false;
    }
    if (mylite_diagnostics_errcode(diagnostics) == MYLITE_OK) {
        return false;
    }
    return true;
}

static int previous_diagnostics_error_count(
    const struct mylite_diagnostics *diagnostics,
    uint64_t *out_count
) {
    uint64_t count = 0U;

    if (diagnostics == NULL || out_count == NULL) {
        return MYLITE_MISUSE;
    }

    for (size_t index = 0U; index < mylite_diagnostics_warning_count(diagnostics); ++index) {
        if (diagnostic_record_level_is_error(mylite_diagnostics_warning_at(diagnostics, index))) {
            if (count == UINT64_MAX) {
                return MYLITE_NOMEM;
            }
            ++count;
        }
    }
    if (diagnostics_has_error_condition(diagnostics)) {
        if (count == UINT64_MAX) {
            return MYLITE_NOMEM;
        }
        ++count;
    }

    *out_count = count;
    return MYLITE_OK;
}

static bool diagnostic_record_level_is_error(const struct mylite_diagnostic_record *record) {
    if (record == NULL) {
        return false;
    }
    if (strcmp(record->level, "Error") != 0) {
        return false;
    }
    return true;
}

static int format_uint64(
    struct mylite_db *database,
    uint64_t value,
    char *buffer,
    size_t buffer_size
) {
    int written = snprintf(buffer, buffer_size, "%" PRIu64, value);

    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format unsigned integer");
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}
