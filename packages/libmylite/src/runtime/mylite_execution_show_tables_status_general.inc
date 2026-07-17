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
    static const struct show_result_column_definition table_definition = {
        MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING,
        MYLITE_RESULT_COLUMN_FLAG_NOT_NULL | MYLITE_RESULT_COLUMN_FLAG_BINARY |
            MYLITE_RESULT_COLUMN_FLAG_NO_DEFAULT,
        64U,
        0U,
        false,
        true,
    };
    static const struct show_result_column_definition table_type_definition = {
        MYLITE_RESULT_LOGICAL_TYPE_STRING,
        MYLITE_RESULT_COLUMN_FLAG_NOT_NULL | MYLITE_RESULT_COLUMN_FLAG_BINARY |
            MYLITE_RESULT_COLUMN_FLAG_ENUM | MYLITE_RESULT_COLUMN_FLAG_NO_DEFAULT,
        11U,
        0U,
        false,
        true,
    };
    static const char *const table_type_label = "Table_type";
    int rc = append_show_result_columns(database, result, &column_name, &table_definition, 1U);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }
    if (!is_full) {
        return MYLITE_OK;
    }

    rc = append_show_result_columns(
        database,
        result,
        &table_type_label,
        &table_type_definition,
        1U
    );
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
    if (rc == MYLITE_OK) {
        rc = append_show_result_columns(
            database,
            result,
            show_table_status_result_columns,
            show_table_status_result_column_definitions,
            show_table_status_result_column_count
        );
        if (rc == MYLITE_NOMEM) {
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
