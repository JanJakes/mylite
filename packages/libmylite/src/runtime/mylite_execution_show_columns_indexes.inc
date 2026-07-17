static bool show_columns_statement_uses_identifier_filter(
    const struct mylite_sql_ast_node *statement
) {
    return statement != NULL && statement->span.text != NULL &&
           (text_has_ascii_case_insensitive_prefix(statement->span.text, "DESC") ||
            text_has_ascii_case_insensitive_prefix(statement->span.text, "EXPLAIN"));
}

static int execute_show_columns_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct show_columns_filter_nodes nodes = {0};
    struct table_name_resolution target = {0};
    struct mylite_catalog_table_descriptor table = {0};
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_table_columns_cache_entry *columns_cache_entry = NULL;
    struct loaded_table_key_metadata_reference key_metadata =
        mylite_execution_loaded_table_key_metadata_reference_init();
    const struct mylite_execution_catalog_mysql_system_table *mysql_system_definition = NULL;
    struct primary_key_info empty_primary_key = mylite_execution_primary_key_info_init();
    size_t column_count = 0U;
    bool missing_schema = false;
    bool mysql_system_target = false;
    struct show_like_filter filter = {
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
    struct show_columns_context context = {0};
    mylite_result *result = NULL;
    bool full = statement->kind == MYLITE_SQL_AST_SHOW_FULL_COLUMNS_STATEMENT;
    int rc = MYLITE_OK;

    rc = resolve_show_columns_filter_nodes(database, statement, &nodes);
    if (rc == MYLITE_OK) {
        rc = resolve_show_columns_mysql_system_table(
            database,
            (struct show_columns_target_nodes){
                .table = nodes.table,
                .schema = nodes.schema,
            },
            &mysql_system_definition,
            &mysql_system_target
        );
    }
    if (rc == MYLITE_OK && mysql_system_definition != NULL) {
        return execute_show_columns_mysql_system_table_statement(
            database,
            &nodes,
            mysql_system_definition,
            full,
            out_result
        );
    }
    if (rc == MYLITE_OK && mysql_system_target) {
        set_unsupported_error(database, "SHOW COLUMNS supports selected mysql system tables");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = reject_unknown_show_columns_information_schema_table(
            database,
            (struct show_columns_target_nodes){
                .table = nodes.table,
                .schema = nodes.schema,
            }
        );
    }
    if (rc == MYLITE_OK) {
        rc = resolve_show_columns_table_name(
            database,
            (struct show_columns_target_nodes){
                .table = nodes.table,
                .schema = nodes.schema,
            },
            &target,
            &missing_schema
        );
    }
    if (rc == MYLITE_OK) {
        rc = resolve_metadata_table_reference(database, &target, missing_schema, &table);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_acquire_table_columns(
            database,
            table.table_id,
            &columns,
            &column_count,
            &columns_cache_entry
        );
    }
    if (rc == MYLITE_OK) {
        rc = load_show_columns_key_metadata(database, &table, columns, column_count, &key_metadata);
    }
    if (rc == MYLITE_OK) {
        rc = make_show_like_filter(database, nodes.like, &filter);
    }
    if (rc == MYLITE_OK) {
        rc = validate_show_columns_where_clause(database, nodes.where, full);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_show_columns_result_columns(database, result, full);
    }
    if (rc == MYLITE_OK) {
        context.database = database;
        context.result = result;
        context.filter = &filter;
        context.where_clause = nodes.where;
        context.table = &table;
        context.primary_key = key_metadata.metadata == NULL ? &empty_primary_key
                                                            : &key_metadata.metadata->primary_key;
        context.indexes = key_metadata.metadata == NULL ? NULL : key_metadata.metadata->indexes;
        context.index_count =
            key_metadata.metadata == NULL ? 0U : key_metadata.metadata->index_count;
        context.full = full;
        for (size_t column_index = 0U; rc == MYLITE_OK && column_index < column_count;
             ++column_index) {
            rc = append_show_column(&columns[column_index], &context);
        }
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        } else if (
            rc != MYLITE_OK &&
            mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) == MYLITE_OK
        ) {
            set_runtime_error(database, "failed to build SHOW COLUMNS result");
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        show_like_filter_deinit(&filter);
        mylite_execution_loaded_table_key_metadata_reference_deinit(&key_metadata);
        mylite_execution_release_table_columns(columns_cache_entry, columns);
        return rc;
    }

    show_like_filter_deinit(&filter);
    mylite_execution_loaded_table_key_metadata_reference_deinit(&key_metadata);
    mylite_execution_release_table_columns(columns_cache_entry, columns);
    return finish_successful_result(database, result, out_result);
}

static int resolve_show_columns_mysql_system_table(
    struct mylite_db *database,
    struct show_columns_target_nodes nodes,
    const struct mylite_execution_catalog_mysql_system_table **out_definition,
    bool *out_mysql_system_target
) {
    char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char table_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    bool has_target = false;
    int rc = MYLITE_OK;

    if (out_definition == NULL || out_mysql_system_target == NULL) {
        return MYLITE_MISUSE;
    }
    *out_definition = NULL;
    *out_mysql_system_target = false;

    rc = copy_show_columns_target_schema_and_table(
        database,
        nodes,
        schema_name,
        table_name,
        &has_target
    );
    if (rc != MYLITE_OK || !has_target) {
        return rc;
    }

    *out_definition = find_mysql_system_table_definition(schema_name, table_name);
    if (*out_definition != NULL) {
        *out_mysql_system_target = true;
        return MYLITE_OK;
    }
    if (schema_name_is_information_schema(schema_name)) {
        return MYLITE_OK;
    }
    if (mylite_execution_catalog_builtin_schema_by_name(schema_name) == NULL) {
        return MYLITE_OK;
    }

    *out_mysql_system_target = true;
    if (mylite_catalog_name_is_reserved(table_name)) {
        set_reserved_name_error(database, "table", table_name);
        return MYLITE_ERROR;
    }
    if (schema_name_is_mysql_system_schema(schema_name) &&
        mysql_data_dictionary_table_is_hidden(table_name)) {
        set_mysql_data_dictionary_table_access_error(database, table_name);
        return MYLITE_ERROR;
    }

    if (builtin_system_schema_table_directory_contains(schema_name, table_name)) {
        return MYLITE_OK;
    }

    set_table_does_not_exist_error(database, schema_name, table_name);
    return MYLITE_ERROR;
}

static int reject_unknown_show_columns_information_schema_table(
    struct mylite_db *database,
    struct show_columns_target_nodes nodes
) {
    char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char table_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    bool has_target = false;
    int rc = copy_show_columns_target_schema_and_table(
        database,
        nodes,
        schema_name,
        table_name,
        &has_target
    );

    if (rc != MYLITE_OK || !has_target || !schema_name_is_information_schema(schema_name)) {
        return rc;
    }
    if (mylite_catalog_name_is_reserved(table_name)) {
        set_reserved_name_error(database, "table", table_name);
        return MYLITE_ERROR;
    }
    if (find_information_schema_table_definition(table_name) != NULL) {
        return MYLITE_OK;
    }

    set_unknown_information_schema_table_error(database, table_name);
    return MYLITE_ERROR;
}

static int copy_show_columns_target_schema_and_table(
    struct mylite_db *database,
    struct show_columns_target_nodes nodes,
    char *schema_name,
    char *table_name,
    bool *out_has_target
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t part_count = 0U;
    int rc = MYLITE_OK;

    if (schema_name == NULL || table_name == NULL || out_has_target == NULL) {
        return MYLITE_MISUSE;
    }
    *out_has_target = false;
    memset(schema_name, 0, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
    memset(table_name, 0, MYLITE_CATALOG_IDENTIFIER_CAPACITY);

    if (nodes.schema != NULL) {
        rc = copy_identifier_text(
            nodes.schema,
            schema_name,
            MYLITE_CATALOG_IDENTIFIER_CAPACITY,
            database
        );
        if (rc == MYLITE_OK) {
            rc = copy_show_columns_explicit_table_name(
                database,
                nodes.table,
                table_name,
                MYLITE_CATALOG_IDENTIFIER_CAPACITY
            );
        }
        if (rc == MYLITE_OK) {
            *out_has_target = true;
        }
        return rc;
    }

    rc = collect_identifier_parts(
        nodes.table,
        parts,
        table_name_part_capacity,
        &part_count,
        database
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (part_count == 1U) {
        if (database == NULL || !database->session.has_selected_schema) {
            return MYLITE_OK;
        }
        memcpy(schema_name, database->session.selected_schema, MYLITE_CATALOG_IDENTIFIER_CAPACITY);
        memcpy(table_name, parts[0], MYLITE_CATALOG_IDENTIFIER_CAPACITY);
        *out_has_target = true;
        return MYLITE_OK;
    }
    if (part_count == 2U) {
        memcpy(schema_name, parts[0], MYLITE_CATALOG_IDENTIFIER_CAPACITY);
        memcpy(table_name, parts[1], MYLITE_CATALOG_IDENTIFIER_CAPACITY);
        *out_has_target = true;
        return MYLITE_OK;
    }

    set_parse_error(database, NULL);
    return MYLITE_ERROR;
}

static bool builtin_system_schema_table_directory_contains(
    const char *schema_name,
    const char *table_name
) {
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory = NULL;

    if (schema_name == NULL || table_name == NULL) {
        return false;
    }
    directory = find_builtin_schema_table_directory(schema_name);
    if (directory == NULL) {
        return false;
    }
    for (size_t index = 0U; index < directory->table_count; ++index) {
        if (strcmp(table_name, directory->table_names[index]) == 0) {
            return true;
        }
    }
    return false;
}

static int execute_show_columns_mysql_system_table_statement(
    struct mylite_db *database,
    const struct show_columns_filter_nodes *nodes,
    const struct mylite_execution_catalog_mysql_system_table *definition,
    bool full,
    mylite_result **out_result
) {
    struct show_like_filter filter = {
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    if (nodes == NULL || definition == NULL) {
        return MYLITE_MISUSE;
    }

    rc = make_show_like_filter(database, nodes->like, &filter);
    if (rc == MYLITE_OK) {
        rc = validate_show_columns_where_clause(database, nodes->where, full);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_show_columns_result_columns(database, result, full);
    }
    if (rc == MYLITE_OK) {
        rc = append_show_columns_mysql_system_table_rows(
            database,
            result,
            &filter,
            nodes->where,
            definition,
            full
        );
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        } else if (
            rc != MYLITE_OK &&
            mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) == MYLITE_OK
        ) {
            set_runtime_error(database, "failed to build SHOW COLUMNS result");
        }
    }
    show_like_filter_deinit(&filter);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int append_show_columns_mysql_system_table_rows(
    struct mylite_db *database,
    mylite_result *result,
    const struct show_like_filter *filter,
    const struct mylite_sql_ast_node *where_clause,
    const struct mylite_execution_catalog_mysql_system_table *definition,
    bool full
) {
    int rc = MYLITE_OK;

    if (result == NULL || definition == NULL) {
        return MYLITE_MISUSE;
    }

    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < definition->query_definition.column_count;
         ++column_index) {
        const struct mylite_execution_catalog_column_definition *column =
            &definition->query_definition.columns[column_index];
        const char *values[show_full_columns_result_column_count] = {0};
        bool where_matches = true;

        if (!show_like_filter_matches(filter, column->name, false)) {
            continue;
        }

        values[0] = column->name;
        values[1] = column->column_type;
        if (full) {
            values[2] = column->collation_name;
            values[3] = column->is_nullable;
            values[4] = definition->column_keys[column_index];
            values[show_full_columns_default_column] = column->column_default;
            values[show_full_columns_extra_column] = definition->column_extras[column_index];
            values[show_full_columns_privileges_column] =
                definition->column_privileges[column_index];
            values[show_full_columns_comment_column] =
                mysql_system_table_column_comment(definition, column_index);
        } else {
            values[2] = column->is_nullable;
            values[3] = definition->column_keys[column_index];
            values[4] = column->column_default;
            values[show_columns_extra_column] = definition->column_extras[column_index];
        }

        if (where_clause != NULL) {
            rc = show_columns_where_clause_matches(
                database,
                where_clause,
                values,
                full,
                &where_matches
            );
        }
        if (rc == MYLITE_OK && where_matches) {
            rc = mylite_result_append_text_row(result, values);
        }
    }
    return rc;
}

static int load_show_columns_key_metadata(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    struct loaded_table_key_metadata_reference *out_reference
) {
    if (table->kind != MYLITE_CATALOG_TABLE_KIND_BASE &&
        table->kind != MYLITE_CATALOG_TABLE_KIND_TEMPORARY) {
        return MYLITE_OK;
    }

    return mylite_execution_acquire_table_key_metadata(
        database,
        table->table_id,
        columns,
        column_count,
        out_reference
    );
}

static int append_show_columns_result_columns(
    struct mylite_db *database,
    mylite_result *result,
    bool full
) {
    const char *const *selected_result_columns = show_columns_result_columns;
    size_t selected_result_column_count = show_columns_result_column_count;
    int rc = MYLITE_OK;

    if (full) {
        selected_result_columns = show_full_columns_result_columns;
        selected_result_column_count = show_full_columns_result_column_count;
    }
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < selected_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, selected_result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }

    return rc;
}

static int resolve_show_columns_filter_nodes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct show_columns_filter_nodes *out_nodes
) {
    size_t child_count = 0U;

    if (statement == NULL || out_nodes == NULL) {
        return MYLITE_MISUSE;
    }

    *out_nodes = (struct show_columns_filter_nodes){0};
    out_nodes->table = child_at(statement, 0U);
    child_count = mylite_sql_ast_node_child_count(statement);
    for (size_t index = 1U; index < child_count; ++index) {
        const struct mylite_sql_ast_node *child = child_at(statement, index);

        if (child == NULL) {
            continue;
        }
        if (child->kind == MYLITE_SQL_AST_LITERAL ||
            (child->kind == MYLITE_SQL_AST_IDENTIFIER &&
             show_columns_statement_uses_identifier_filter(statement))) {
            if (out_nodes->like != NULL || out_nodes->where != NULL) {
                set_runtime_error(database, "invalid SHOW COLUMNS filter");
                return MYLITE_ERROR;
            }
            out_nodes->like = child;
        } else if (child->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
            if (out_nodes->like != NULL || out_nodes->where != NULL) {
                set_runtime_error(database, "invalid SHOW COLUMNS filter");
                return MYLITE_ERROR;
            }
            out_nodes->where = child;
        } else {
            if (out_nodes->schema != NULL) {
                set_runtime_error(database, "invalid SHOW COLUMNS schema");
                return MYLITE_ERROR;
            }
            out_nodes->schema = child;
        }
    }

    return MYLITE_OK;
}

static int validate_show_columns_where_clause(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    bool full
) {
    static const char *const validation_values[show_columns_result_column_count] = {
        "field",
        "type",
        "YES",
        "",
        NULL,
        "extra",
    };
    static const char *const full_validation_values[show_full_columns_result_column_count] = {
        "field",
        "type",
        NULL,
        "YES",
        "",
        NULL,
        "extra",
        "select,insert,update,references",
        "",
    };
    const char *const *selected_validation_values = validation_values;
    bool matches = false;

    if (where_clause == NULL) {
        return MYLITE_OK;
    }
    if (full) {
        selected_validation_values = full_validation_values;
    }
    return show_columns_where_clause_matches(
        database,
        where_clause,
        selected_validation_values,
        full,
        &matches
    );
}

static int show_columns_where_clause_matches(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const char *const *values,
    bool full,
    bool *out_matches
) {
    enum show_variables_where_truth truth = SHOW_VARIABLES_WHERE_FALSE;
    int rc = MYLITE_OK;

    if (where_clause == NULL || where_clause->kind != MYLITE_SQL_AST_WHERE_CLAUSE ||
        values == NULL || out_matches == NULL) {
        set_runtime_error(database, "invalid SHOW COLUMNS WHERE predicate");
        return MYLITE_ERROR;
    }

    *out_matches = false;
    rc = evaluate_show_columns_where_predicate(
        database,
        child_at(where_clause, 0U),
        values,
        full,
        &truth
    );
    if (rc == MYLITE_OK) {
        *out_matches = truth == SHOW_VARIABLES_WHERE_TRUE;
    }
    return rc;
}

static int evaluate_show_columns_where_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *const *values,
    bool full,
    enum show_variables_where_truth *out_truth
) {
    struct show_variables_where_frame_stack frame_stack = {0};
    struct show_variables_where_truth_stack truth_stack = {0};
    struct show_variables_where_eval_frame frame = {
        .node = predicate,
        .action = SHOW_VARIABLES_WHERE_VISIT,
    };
    int rc = MYLITE_OK;

    if (predicate == NULL || values == NULL || out_truth == NULL) {
        set_runtime_error(database, "invalid SHOW COLUMNS WHERE predicate");
        return MYLITE_ERROR;
    }

    rc = show_variables_where_frame_stack_push(database, &frame_stack, frame);
    while (rc == MYLITE_OK && show_variables_where_frame_stack_pop(&frame_stack, &frame)) {
        const struct mylite_sql_ast_node *current = unwrap_parenthesized_predicate(frame.node);

        if (current == NULL) {
            set_runtime_error(database, "invalid SHOW COLUMNS WHERE predicate");
            rc = MYLITE_ERROR;
        } else if (frame.action == SHOW_VARIABLES_WHERE_VISIT) {
            rc = visit_show_columns_where_predicate(database, &frame_stack, current);
        } else {
            rc = evaluate_show_columns_where_frame(database, current, values, full, &truth_stack);
        }
    }

    if (rc == MYLITE_OK) {
        if (truth_stack.count != 1U ||
            !show_variables_where_truth_stack_pop(&truth_stack, out_truth)) {
            set_runtime_error(database, "invalid SHOW COLUMNS WHERE predicate state");
            rc = MYLITE_ERROR;
        }
    }

    show_variables_where_truth_stack_deinit(&truth_stack);
    show_variables_where_frame_stack_deinit(&frame_stack);
    return rc;
}

static int visit_show_columns_where_predicate(
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

static int evaluate_show_columns_where_frame(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *predicate,
    const char *const *values,
    bool full,
    struct show_variables_where_truth_stack *truth_stack
) {
    enum show_variables_where_truth truth = SHOW_VARIABLES_WHERE_FALSE;
    int rc = MYLITE_OK;

    if (predicate->kind == MYLITE_SQL_AST_COMPARISON_PREDICATE) {
        rc = evaluate_show_columns_where_comparison_predicate(
            database,
            predicate,
            values,
            full,
            &truth
        );
    } else if (predicate->kind == MYLITE_SQL_AST_IN_PREDICATE) {
        rc = evaluate_show_columns_where_in_predicate(database, predicate, values, full, &truth);
    } else if (predicate->kind == MYLITE_SQL_AST_IS_NULL_PREDICATE) {
        rc = evaluate_show_columns_where_is_null_predicate(
            database,
            predicate,
            values,
            full,
            &truth
        );
    } else if (predicate->kind == MYLITE_SQL_AST_NOT_PREDICATE) {
        enum show_variables_where_truth child_truth = SHOW_VARIABLES_WHERE_FALSE;

        if (!show_variables_where_truth_stack_pop(truth_stack, &child_truth)) {
            set_runtime_error(database, "invalid SHOW COLUMNS WHERE predicate stack");
            return MYLITE_ERROR;
        }
        truth = negate_show_variables_where_truth(child_truth);
    } else if (
        predicate->kind == MYLITE_SQL_AST_AND_PREDICATE ||
        predicate->kind == MYLITE_SQL_AST_OR_PREDICATE
    ) {
        rc = evaluate_show_variables_where_logical_frame(database, predicate, truth_stack, &truth);
    } else if (predicate->kind == MYLITE_SQL_AST_XOR_PREDICATE) {
        set_unsupported_error(database, "SHOW COLUMNS WHERE does not support XOR predicates");
        return MYLITE_ERROR;
    } else {
        set_unsupported_error(database, "SHOW COLUMNS WHERE supports output-column predicates");
        return MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = show_variables_where_truth_stack_push(database, truth_stack, truth);
    }
    return rc;
}

static int execute_show_index_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct show_index_filter_nodes nodes = {0};
    struct table_name_resolution target = {0};
    struct mylite_catalog_table_descriptor table = {0};
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_table_columns_cache_entry *columns_cache_entry = NULL;
    struct loaded_table_key_metadata_reference key_metadata =
        mylite_execution_loaded_table_key_metadata_reference_init();
    const struct loaded_index_info *indexes = NULL;
    size_t column_count = 0U;
    size_t index_count = 0U;
    bool missing_schema = false;
    bool mysql_system_handled = false;
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    rc = resolve_show_index_filter_nodes(database, statement, &nodes);
    if (rc == MYLITE_OK) {
        rc = execute_show_index_mysql_system_target(
            database,
            &nodes,
            out_result,
            &mysql_system_handled
        );
    }
    if (rc != MYLITE_OK || mysql_system_handled) {
        return rc;
    }
    if (rc == MYLITE_OK) {
        rc = resolve_show_columns_table_name(
            database,
            (struct show_columns_target_nodes){
                .table = nodes.table,
                .schema = nodes.schema,
            },
            &target,
            &missing_schema
        );
    }
    if (rc == MYLITE_OK) {
        rc = resolve_readable_table(database, &target, missing_schema, &table);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_acquire_table_columns(
            database,
            table.table_id,
            &columns,
            &column_count,
            &columns_cache_entry
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_acquire_table_key_metadata(
            database,
            table.table_id,
            columns,
            column_count,
            &key_metadata
        );
        if (rc == MYLITE_OK) {
            indexes = key_metadata.metadata->indexes;
            index_count = key_metadata.metadata->index_count;
        }
    }
    if (rc == MYLITE_OK) {
        rc = validate_show_index_where_clause(database, nodes.where);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < show_index_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, show_index_result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (int group = index_display_group_primary;
         rc == MYLITE_OK && group <= index_display_group_fulltext;
         ++group) {
        for (size_t index = 0U; rc == MYLITE_OK && index < index_count; ++index) {
            if (index_display_group(&indexes[index]) != group) {
                continue;
            }
            rc = append_show_index(database, result, &target, &indexes[index], nodes.where);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        mylite_execution_loaded_table_key_metadata_reference_deinit(&key_metadata);
        mylite_execution_release_table_columns(columns_cache_entry, columns);
        return rc;
    }

    mylite_execution_loaded_table_key_metadata_reference_deinit(&key_metadata);
    mylite_execution_release_table_columns(columns_cache_entry, columns);
    return finish_successful_result(database, result, out_result);
}

static int execute_show_index_mysql_system_target(
    struct mylite_db *database,
    const struct show_index_filter_nodes *nodes,
    mylite_result **out_result,
    bool *out_handled
) {
    const struct mylite_execution_catalog_mysql_system_table *definition = NULL;
    bool mysql_system_target = false;
    int rc = MYLITE_OK;

    if (nodes == NULL || out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;

    rc = resolve_show_index_mysql_system_table(
        database,
        (struct show_columns_target_nodes){
            .table = nodes->table,
            .schema = nodes->schema,
        },
        &definition,
        &mysql_system_target
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (definition != NULL) {
        *out_handled = true;
        return execute_show_index_mysql_system_table_statement(
            database,
            nodes->where,
            definition,
            out_result
        );
    }
    if (mysql_system_target) {
        *out_handled = true;
        set_unsupported_error(database, "SHOW INDEX supports selected mysql system tables");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int resolve_show_index_mysql_system_table(
    struct mylite_db *database,
    struct show_columns_target_nodes nodes,
    const struct mylite_execution_catalog_mysql_system_table **out_definition,
    bool *out_mysql_system_target
) {
    char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char table_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    bool has_target = false;
    int rc = MYLITE_OK;

    if (out_definition == NULL || out_mysql_system_target == NULL) {
        return MYLITE_MISUSE;
    }
    *out_definition = NULL;
    *out_mysql_system_target = false;

    rc = copy_show_columns_target_schema_and_table(
        database,
        nodes,
        schema_name,
        table_name,
        &has_target
    );
    if (rc != MYLITE_OK || !has_target) {
        return rc;
    }

    *out_definition = find_mysql_system_table_definition(schema_name, table_name);
    if (*out_definition != NULL) {
        *out_mysql_system_target = true;
        return MYLITE_OK;
    }
    if (schema_name_is_information_schema(schema_name)) {
        return MYLITE_OK;
    }
    if (mylite_execution_catalog_builtin_schema_by_name(schema_name) == NULL) {
        return MYLITE_OK;
    }

    *out_mysql_system_target = true;
    if (mylite_catalog_name_is_reserved(table_name)) {
        set_reserved_name_error(database, "table", table_name);
        return MYLITE_ERROR;
    }
    if (schema_name_is_mysql_system_schema(schema_name) &&
        mysql_data_dictionary_table_is_hidden(table_name)) {
        set_mysql_data_dictionary_table_access_error(database, table_name);
        return MYLITE_ERROR;
    }

    if (builtin_system_schema_table_directory_contains(schema_name, table_name)) {
        return MYLITE_OK;
    }

    set_table_does_not_exist_error(database, schema_name, table_name);
    return MYLITE_ERROR;
}

static int execute_show_index_mysql_system_table_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const struct mylite_execution_catalog_mysql_system_table *definition,
    mylite_result **out_result
) {
    mylite_result *result = NULL;
    int rc = validate_show_index_where_clause(database, where_clause);

    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < show_index_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, show_index_result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_show_index_mysql_system_table_rows(database, result, where_clause, definition);
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        } else if (
            rc != MYLITE_OK &&
            mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) == MYLITE_OK
        ) {
            set_runtime_error(database, "failed to build SHOW INDEX result");
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int append_show_index_mysql_system_table_rows(
    struct mylite_db *database,
    mylite_result *result,
    const struct mylite_sql_ast_node *where_clause,
    const struct mylite_execution_catalog_mysql_system_table *definition
) {
    if (result == NULL || definition == NULL) {
        return MYLITE_MISUSE;
    }

    int rc = append_show_index_mysql_system_table_primary_key_rows(
        database,
        result,
        where_clause,
        definition
    );
    if (rc == MYLITE_OK) {
        rc = append_show_index_mysql_system_table_secondary_rows(
            database,
            result,
            where_clause,
            definition
        );
    }

    return rc;
}

static int append_show_index_mysql_system_table_primary_key_rows(
    struct mylite_db *database,
    mylite_result *result,
    const struct mylite_sql_ast_node *where_clause,
    const struct mylite_execution_catalog_mysql_system_table *definition
) {
    int rc = MYLITE_OK;
    size_t primary_key_column_count = mysql_system_table_primary_key_column_count(definition);

    for (size_t key_index = 0U; rc == MYLITE_OK && key_index < primary_key_column_count;
         ++key_index) {
        const struct mylite_execution_catalog_column_definition *column =
            mysql_system_table_primary_key_column(definition, key_index);
        char sequence_text[integer_text_capacity];

        if (column == NULL) {
            set_runtime_error(database, "invalid mysql system table primary key");
            return MYLITE_ERROR;
        }

        rc = information_schema_format_i64(
            database,
            (int64_t)key_index + 1,
            sequence_text,
            sizeof(sequence_text)
        );
        if (rc == MYLITE_OK) {
            const char *values[show_index_result_column_count] = {
                definition->query_definition.name,
                "0",
                "PRIMARY",
                sequence_text,
                column->name,
                mysql_system_table_primary_key_collation(definition, key_index + 1),
                mysql_system_table_primary_key_cardinality(definition, key_index + 1),
                NULL,
                NULL,
                mysql_system_table_index_nullable_value(column),
                mysql_system_table_primary_key_index_type(definition),
                "",
                "",
                "YES",
                NULL,
            };

            rc = append_show_index_mysql_system_table_row_if_matches(
                database,
                result,
                where_clause,
                values
            );
        }
    }

    return rc;
}

static int append_show_index_mysql_system_table_secondary_rows(
    struct mylite_db *database,
    mylite_result *result,
    const struct mylite_sql_ast_node *where_clause,
    const struct mylite_execution_catalog_mysql_system_table *definition
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < definition->secondary_index_count; ++index) {
        const struct mylite_execution_catalog_mysql_system_secondary_index *secondary_index =
            &definition->secondary_indexes[index];
        char sequence_text[integer_text_capacity];

        if (secondary_index->column_index >= definition->query_definition.column_count) {
            set_runtime_error(database, "invalid mysql system table secondary index");
            return MYLITE_ERROR;
        }

        rc = information_schema_format_i64(
            database,
            (int64_t)mysql_system_table_secondary_index_sequence(definition, index),
            sequence_text,
            sizeof(sequence_text)
        );
        if (rc == MYLITE_OK) {
            const struct mylite_execution_catalog_column_definition *column =
                &definition->query_definition.columns[secondary_index->column_index];
            const char *values[show_index_result_column_count] = {
                definition->query_definition.name,
                secondary_index->non_unique,
                secondary_index->name,
                sequence_text,
                column->name,
                secondary_index->collation,
                secondary_index->cardinality,
                NULL,
                NULL,
                mysql_system_table_index_nullable_value(column),
                secondary_index->index_type,
                "",
                "",
                "YES",
                NULL,
            };

            rc = append_show_index_mysql_system_table_row_if_matches(
                database,
                result,
                where_clause,
                values
            );
        }
    }

    return rc;
}

static size_t mysql_system_table_secondary_index_sequence(
    const struct mylite_execution_catalog_mysql_system_table *definition,
    size_t secondary_index_index
) {
    const struct mylite_execution_catalog_mysql_system_secondary_index *secondary_index = NULL;
    size_t sequence = 1U;

    if (definition == NULL || secondary_index_index >= definition->secondary_index_count) {
        return sequence;
    }
    secondary_index = &definition->secondary_indexes[secondary_index_index];
    for (size_t index = 0U; index < secondary_index_index; ++index) {
        if (strcmp(definition->secondary_indexes[index].name, secondary_index->name) == 0) {
            ++sequence;
        }
    }
    return sequence;
}

static bool mysql_system_table_secondary_index_name_seen_before(
    const struct mylite_execution_catalog_mysql_system_table *definition,
    size_t secondary_index_index
) {
    const struct mylite_execution_catalog_mysql_system_secondary_index *secondary_index = NULL;

    if (definition == NULL || secondary_index_index >= definition->secondary_index_count) {
        return false;
    }
    secondary_index = &definition->secondary_indexes[secondary_index_index];
    for (size_t index = 0U; index < secondary_index_index; ++index) {
        if (strcmp(definition->secondary_indexes[index].name, secondary_index->name) == 0) {
            return true;
        }
    }
    return false;
}

static const char *mysql_system_table_index_nullable_value(
    const struct mylite_execution_catalog_column_definition *column
) {
    if (column != NULL && column->is_nullable != NULL && strcmp(column->is_nullable, "YES") == 0) {
        return "YES";
    }
    return "";
}

static int append_show_index_mysql_system_table_row_if_matches(
    struct mylite_db *database,
    mylite_result *result,
    const struct mylite_sql_ast_node *where_clause,
    const char *const values[show_index_result_column_count]
) {
    bool where_matches = true;
    int rc = MYLITE_OK;

    if (where_clause != NULL) {
        rc = show_index_where_clause_matches(database, where_clause, values, &where_matches);
    }
    if (rc == MYLITE_OK && where_matches) {
        rc = mylite_result_append_text_row(result, values);
    }

    return rc;
}

static const char *mysql_system_table_primary_key_cardinality(
    const struct mylite_execution_catalog_mysql_system_table *definition,
    size_t sequence
) {
    if (definition == NULL) {
        return "0";
    }
    if (strcmp(definition->schema_name, "performance_schema") == 0) {
        return NULL;
    }
    return mysql_system_table_primary_key_cardinality_for_name(
        definition->query_definition.name,
        sequence
    );
}

static const char *mysql_system_table_primary_key_cardinality_for_name(
    const char *table_name,
    size_t sequence
) {
    enum {
        mysql_primary_key_no_sequence_split = 0,
        mysql_innodb_index_stats_stat_name_sequence = 4,
        mysql_second_key_part_sequence = 2,
        mysql_third_key_part_sequence = 3,
    };

    static const struct mysql_system_table_primary_key_cardinality_value {
        const char *table_name;
        size_t split_sequence;
        const char *first_cardinality;
        const char *split_cardinality;
    } cardinalities[] = {
        {"db", mysql_second_key_part_sequence, "1", "2"},
        {"engine_cost", mysql_primary_key_no_sequence_split, "2", "2"},
        {"global_grants", mysql_third_key_part_sequence, "1", "103"},
        {"proxies_priv", mysql_primary_key_no_sequence_split, "1", "1"},
        {"tables_priv", mysql_second_key_part_sequence, "1", "2"},
        {"innodb_table_stats", mysql_primary_key_no_sequence_split, "2", "2"},
        {"innodb_index_stats", mysql_innodb_index_stats_stat_name_sequence, "2", "6"},
        {"plugin", mysql_primary_key_no_sequence_split, "1", "1"},
        {"server_cost", mysql_primary_key_no_sequence_split, "6", "6"},
        {"sys_config", mysql_primary_key_no_sequence_split, "6", "6"},
        {"help_category", mysql_primary_key_no_sequence_split, "53", "53"},
        {"help_keyword", mysql_primary_key_no_sequence_split, "551", "551"},
        {"help_relation", mysql_second_key_part_sequence, "1393", "2258"},
        {"help_topic", mysql_primary_key_no_sequence_split, "596", "596"},
        {"time_zone", mysql_primary_key_no_sequence_split, "1457", "1457"},
        {"time_zone_leap_second", mysql_primary_key_no_sequence_split, "0", "0"},
        {"time_zone_name", mysql_primary_key_no_sequence_split, "1712", "1712"},
        {"time_zone_transition", mysql_second_key_part_sequence, "1252", "119074"},
        {"time_zone_transition_type", mysql_second_key_part_sequence, "1954", "10529"},
        {"user", mysql_second_key_part_sequence, "2", "5"},
    };

    for (size_t index = 0U; index < sizeof(cardinalities) / sizeof(cardinalities[0]); ++index) {
        if (strcmp(table_name, cardinalities[index].table_name) == 0) {
            if (cardinalities[index].split_sequence != mysql_primary_key_no_sequence_split &&
                sequence >= cardinalities[index].split_sequence) {
                return cardinalities[index].split_cardinality;
            }
            return cardinalities[index].first_cardinality;
        }
    }
    return "0";
}

static const char *mysql_system_table_primary_key_collation(
    const struct mylite_execution_catalog_mysql_system_table *definition,
    size_t sequence
) {
    if (definition == NULL) {
        return "A";
    }
    if (strcmp(definition->schema_name, "performance_schema") == 0) {
        return NULL;
    }
    return mysql_system_table_primary_key_collation_for_name(
        definition->query_definition.name,
        sequence
    );
}

static const char *mysql_system_table_primary_key_collation_for_name(
    const char *table_name,
    size_t sequence
) {
    enum {
        mysql_password_history_timestamp_sequence = 3,
    };

    if (table_name != NULL && strcmp(table_name, "password_history") == 0 &&
        sequence == mysql_password_history_timestamp_sequence) {
        return "D";
    }
    return "A";
}

static const char *mysql_system_table_primary_key_index_type(
    const struct mylite_execution_catalog_mysql_system_table *definition
) {
    if (definition != NULL && strcmp(definition->schema_name, "performance_schema") == 0) {
        return "HASH";
    }
    return "BTREE";
}

static int resolve_show_index_filter_nodes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct show_index_filter_nodes *out_nodes
) {
    size_t child_count = 0U;

    if (out_nodes == NULL) {
        return MYLITE_MISUSE;
    }
    *out_nodes = (struct show_index_filter_nodes){0};
    if (statement == NULL) {
        set_runtime_error(database, "invalid SHOW INDEX statement");
        return MYLITE_ERROR;
    }

    child_count = mylite_sql_ast_node_child_count(statement);
    out_nodes->table = child_at(statement, 0U);
    if (out_nodes->table == NULL) {
        set_runtime_error(database, "invalid SHOW INDEX target");
        return MYLITE_ERROR;
    }

    for (size_t child_index = 1U; child_index < child_count; ++child_index) {
        const struct mylite_sql_ast_node *child = child_at(statement, child_index);

        if (child == NULL) {
            continue;
        }
        if (child->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
            if (out_nodes->where != NULL) {
                set_runtime_error(database, "invalid SHOW INDEX filter");
                return MYLITE_ERROR;
            }
            out_nodes->where = child;
            continue;
        }
        if (out_nodes->schema == NULL) {
            out_nodes->schema = child;
            continue;
        }
        set_runtime_error(database, "invalid SHOW INDEX filter");
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}
