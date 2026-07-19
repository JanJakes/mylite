static int resolve_information_schema_query(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct information_schema_query *out_query
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *from_clause = select_statement_source_clause(statement);
    const struct mylite_sql_ast_node *optional_clause =
        select_statement_optional_clause_start(statement);
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *group_clause = NULL;
    const struct mylite_sql_ast_node *order_clause = NULL;
    const struct mylite_sql_ast_node *limit_clause = NULL;
    int rc = MYLITE_OK;

    *out_query = (struct information_schema_query){0};
    if (mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT ||
        mylite_sql_ast_node_select_calc_found_rows(statement) != 0) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA SELECT supports only non-distinct metadata queries"
        );
        return MYLITE_ERROR;
    }

    rc = mylite_execution_information_schema_resolve_source(database, from_clause, out_query);
    while (rc == MYLITE_OK && optional_clause != NULL) {
        if (optional_clause->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
            where_clause = optional_clause;
        } else if (optional_clause->kind == MYLITE_SQL_AST_GROUP_BY_CLAUSE) {
            group_clause = optional_clause;
            out_query->has_group = true;
        } else if (optional_clause->kind == MYLITE_SQL_AST_ORDER_BY_CLAUSE) {
            order_clause = optional_clause;
        } else if (optional_clause->kind == MYLITE_SQL_AST_LIMIT_CLAUSE) {
            limit_clause = optional_clause;
        } else if (optional_clause->kind == MYLITE_SQL_AST_SELECT_INTO_LIST) {
            /* SELECT ... INTO is handled after normal SELECT execution. */
        } else {
            set_unsupported_error(
                database,
                "INFORMATION_SCHEMA SELECT supports only WHERE, GROUP BY, ORDER BY, and LIMIT"
            );
            rc = MYLITE_ERROR;
        }
        optional_clause = optional_clause->next_sibling;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_information_schema_plan_projection(database, select_list, out_query);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_information_schema_plan_predicate(database, where_clause, out_query);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_information_schema_plan_group(database, group_clause, out_query);
    }
    if (rc == MYLITE_OK) {
        out_query->limit = (struct planned_select_limit){
            .has_limit = false,
            .row_count = 0,
            .has_offset = false,
            .offset = 0,
        };
        rc = mylite_execution_information_schema_plan_order(database, order_clause, out_query);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_information_schema_plan_limit(database, limit_clause, out_query);
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_query_plan_materialization(database, out_query);
    }
    if (rc == MYLITE_OK) {
        return MYLITE_OK;
    }

    information_schema_query_deinit(out_query);
    return rc;
}

static int information_schema_query_plan_materialization(
    struct mylite_db *database,
    struct information_schema_query *query
) {
    int rc = MYLITE_OK;

    if (query == NULL || query->definition == NULL) {
        return MYLITE_MISUSE;
    }
    query->required_columns = calloc(query->definition->column_count, sizeof(bool));
    if (query->required_columns == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    for (size_t index = 0U; rc == MYLITE_OK && index < query->projection_count; ++index) {
        if (query->projection_kinds[index] ==
            INFORMATION_SCHEMA_PROJECTION_UNSIGNED_INTEGER_EXPRESSION) {
            const struct information_schema_projection_expression *expression =
                &query->projection_expressions[index];

            for (size_t instruction_index = 0U; instruction_index < expression->instruction_count;
                 ++instruction_index) {
                const struct information_schema_numeric_projection_instruction *instruction =
                    &expression->instructions[instruction_index];

                if (instruction->kind == INFORMATION_SCHEMA_NUMERIC_PROJECTION_COLUMN) {
                    information_schema_query_require_column(query, instruction->column_index);
                }
            }
        } else if (query->projection_kinds[index] !=
                   INFORMATION_SCHEMA_PROJECTION_INTEGER_LITERAL) {
            information_schema_query_require_column(query, query->projection_indexes[index]);
        }
    }
    for (size_t index = 0U; index < query->predicate.instruction_count; ++index) {
        enum information_schema_predicate_instruction_kind kind =
            query->predicate.instructions[index].kind;

        if (kind == INFORMATION_SCHEMA_PREDICATE_COMPARISON ||
            kind == INFORMATION_SCHEMA_PREDICATE_IS_NULL ||
            kind == INFORMATION_SCHEMA_PREDICATE_BETWEEN ||
            kind == INFORMATION_SCHEMA_PREDICATE_IN) {
            information_schema_query_require_column(
                query,
                query->predicate.instructions[index].column_index
            );
        }
    }
    for (size_t index = 0U; index < query->group_index_count; ++index) {
        information_schema_query_require_column(query, query->group_indexes[index]);
    }
    if (query->has_group && query->definition->kind == MYLITE_EXECUTION_CATALOG_TABLE_STATISTICS) {
        information_schema_query_require_column(
            query,
            information_schema_statistics_seq_in_index_column
        );
    }
    for (size_t index = 0U; index < query->order_count; ++index) {
        information_schema_query_require_column(query, query->order_indexes[index]);
    }
    if (query->definition->kind == MYLITE_EXECUTION_CATALOG_TABLE_TABLES) {
        information_schema_query_require_column(
            query,
            information_schema_tables_table_schema_column
        );
        information_schema_query_require_column(query, information_schema_tables_table_name_column);
    }
    if (query->definition->kind ==
        MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_AUTO_INCREMENT_COLUMNS) {
        information_schema_query_require_column(
            query,
            sys_schema_auto_increment_columns_table_schema_column
        );
        information_schema_query_require_column(
            query,
            sys_schema_auto_increment_columns_table_name_column
        );
        information_schema_query_require_column(
            query,
            sys_schema_auto_increment_columns_column_name_column
        );
        information_schema_query_require_column(
            query,
            sys_schema_auto_increment_columns_max_value_column
        );
        information_schema_query_require_column(
            query,
            sys_schema_auto_increment_columns_auto_increment_ratio_column
        );
    }
    return rc;
}

static void information_schema_query_require_column(
    struct information_schema_query *query,
    size_t column_index
) {
    if (query == NULL || query->definition == NULL || query->required_columns == NULL ||
        column_index >= query->definition->column_count) {
        return;
    }
    query->required_columns[column_index] = true;
    if (query->definition->kind == MYLITE_EXECUTION_CATALOG_TABLE_TABLES &&
        column_index == information_schema_tables_auto_increment_column) {
        query->required_columns[information_schema_tables_table_schema_column] = true;
        query->required_columns[information_schema_tables_table_name_column] = true;
    }
}

static void information_schema_query_deinit(struct information_schema_query *query) {
    if (query == NULL) {
        return;
    }
    for (size_t index = 0U; index < query->projection_count; ++index) {
        free(query->projection_names[index]);
        mylite_execution_information_schema_projection_expression_deinit(
            &query->projection_expressions[index]
        );
    }
    free(query->count_name);
    mylite_execution_information_schema_predicate_plan_deinit(&query->predicate);
    free((void *)query->projection_names);
    free(query->projection_indexes);
    free(query->projection_kinds);
    free(query->projection_expressions);
    free(query->required_columns);
    *query = (struct information_schema_query){0};
}

static int execute_information_schema_query(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct information_schema_query *query,
    bool apply_sql_select_limit,
    mylite_result **out_result
) {
    struct information_schema_row_set rows = {0};
    struct information_schema_catalog_filter filter = {0};
    struct information_schema_result_projection_buffers stream_buffers = {0};
    mylite_result *result = NULL;
    size_t read_row_count = 0U;
    bool stream_during_enumeration =
        !query->is_count_star && !query->has_group && !query->has_order &&
        query->definition->kind != MYLITE_EXECUTION_CATALOG_TABLE_TABLES &&
        query->definition->kind != MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_AUTO_INCREMENT_COLUMNS;
    bool skip_row_enumeration =
        (query->limit.has_limit && query->limit.row_count == 0) ||
        (query->is_count_star && query->limit.has_offset && query->limit.offset != 0);
    int rc = information_schema_catalog_filter_init_from_query(database, query, &filter);

    if (rc == MYLITE_OK) {
        bool handled = false;

        rc = information_schema_try_execute_tables_exact_base_table_query(
            database,
            query,
            &filter,
            apply_sql_select_limit,
            out_result,
            &handled
        );
        if (rc != MYLITE_OK || handled) {
            information_schema_catalog_filter_deinit(&filter);
            return rc;
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_information_schema_append_result_columns(database, result, query);
    }
    if (rc == MYLITE_OK && stream_during_enumeration && !skip_row_enumeration) {
        rc = mylite_execution_information_schema_result_projection_buffers_init(
            database,
            query,
            &stream_buffers
        );
    }
    if (rc == MYLITE_OK && !skip_row_enumeration) {
        rc = build_information_schema_rows_with_filter(
            database,
            context,
            query,
            query->definition,
            &filter,
            stream_during_enumeration ? result : NULL,
            stream_during_enumeration ? &stream_buffers : NULL,
            &rows
        );
    }
    if (rc == MYLITE_OK && query->is_count_star) {
        rc = mylite_execution_information_schema_append_count_result(
            database,
            query,
            &rows,
            result,
            &read_row_count
        );
    } else if (rc == MYLITE_OK && query->has_group) {
        rc = mylite_execution_information_schema_append_grouped_statistics_result_rows(
            database,
            query,
            &rows,
            result,
            &read_row_count
        );
    } else if (rc == MYLITE_OK && stream_during_enumeration) {
        read_row_count = rows.row_count;
    } else if (rc == MYLITE_OK) {
        rc = mylite_execution_information_schema_append_result_rows(
            database,
            query,
            &rows,
            result,
            &read_row_count
        );
    }
    if (rc == MYLITE_OK && query->definition->kind == MYLITE_EXECUTION_CATALOG_TABLE_PROCESSLIST &&
        read_row_count > 0U) {
        rc = append_show_processlist_warning(database);
    }
    if (rc == MYLITE_OK && query->definition->kind == MYLITE_EXECUTION_CATALOG_TABLE_PROFILING &&
        !(query->limit.has_limit && query->limit.row_count == 0)) {
        rc = append_information_schema_profiling_warning(database);
    }
    mylite_execution_information_schema_result_projection_buffers_deinit(&stream_buffers);
    information_schema_row_set_deinit(&rows);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        information_schema_catalog_filter_deinit(&filter);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    if (apply_sql_select_limit && !query->limit.has_limit) {
        apply_sql_select_limit_to_result(database, result);
    }
    information_schema_catalog_filter_deinit(&filter);
    return finish_successful_result(database, result, out_result);
}

static int information_schema_try_execute_tables_exact_base_table_query(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct information_schema_catalog_filter *filter,
    bool apply_sql_select_limit,
    mylite_result **out_result,
    bool *out_handled
) {
    enum mylite_catalog_table_kind table_kind = MYLITE_CATALOG_TABLE_KIND_INVALID;
    bool supported = false;
    bool found = false;
    int rc = MYLITE_OK;

    if (out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    rc = information_schema_tables_exact_base_table_query_is_supported(
        database,
        query,
        filter,
        &supported
    );
    if (rc != MYLITE_OK || !supported) {
        return rc;
    }

    rc = mylite_catalog_try_read_table_kind_by_schema_table_name(
        database,
        filter->schema_name,
        filter->table_name,
        &table_kind,
        &found
    );
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
        return rc;
    }
    if (rc != MYLITE_OK) {
        set_runtime_error(database, "failed to read INFORMATION_SCHEMA table existence metadata");
        return MYLITE_ERROR;
    }

    *out_handled = true;
    return information_schema_append_fast_table_existence_result(
        database,
        query,
        found && table_kind == MYLITE_CATALOG_TABLE_KIND_BASE,
        apply_sql_select_limit,
        out_result
    );
}

static int information_schema_tables_exact_base_table_query_is_supported(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct information_schema_catalog_filter *filter,
    bool *out_supported
) {
    if (out_supported == NULL) {
        return MYLITE_MISUSE;
    }
    *out_supported = false;
    if (query == NULL || query->definition == NULL || filter == NULL ||
        query->predicate.instruction_count == 0U ||
        query->definition->kind != MYLITE_EXECUTION_CATALOG_TABLE_TABLES || query->has_group ||
        !filter->has_schema_name || !filter->has_table_name || !filter->has_table_type ||
        filter->schema_name == NULL || filter->table_name == NULL || filter->table_type == NULL ||
        filter->schema_name[0] == '\0' || filter->table_name[0] == '\0' ||
        strlen(filter->schema_name) >= MYLITE_CATALOG_IDENTIFIER_CAPACITY ||
        strlen(filter->table_name) >= MYLITE_CATALOG_IDENTIFIER_CAPACITY ||
        filter->schema_column_index >= query->definition->column_count ||
        filter->table_column_index >= query->definition->column_count ||
        filter->table_type_column_index >= query->definition->column_count) {
        return MYLITE_OK;
    }
    if (information_schema_catalog_filter_allows_system_rows(query->definition, filter)) {
        return MYLITE_OK;
    }
    if (information_schema_compare_text(
            query->definition,
            filter->table_type_column_index,
            filter->table_type,
            "BASE TABLE"
        ) != 0) {
        return MYLITE_OK;
    }
    if (!query->is_count_star &&
        (query->projection_count != 1U ||
         query->projection_kinds[0] != INFORMATION_SCHEMA_PROJECTION_INTEGER_LITERAL)) {
        return MYLITE_OK;
    }

    return information_schema_exact_table_lookup_predicate_matches(
        database,
        query,
        filter,
        out_supported
    );
}

static int information_schema_exact_table_lookup_predicate_matches(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct information_schema_catalog_filter *filter,
    bool *out_matches
) {
    if (out_matches == NULL) {
        return MYLITE_MISUSE;
    }
    *out_matches = query != NULL && query->predicate.instruction_count != 0U;
    for (size_t index = 0U; *out_matches && index < query->predicate.instruction_count; ++index) {
        const struct information_schema_predicate_instruction *instruction =
            &query->predicate.instructions[index];

        if (instruction->kind == INFORMATION_SCHEMA_PREDICATE_AND) {
            continue;
        }
        if (instruction->kind != INFORMATION_SCHEMA_PREDICATE_COMPARISON) {
            *out_matches = false;
            continue;
        }
        *out_matches =
            information_schema_exact_table_lookup_comparison_matches(query, filter, instruction);
    }
    (void)database;
    return MYLITE_OK;
}

static bool information_schema_exact_table_lookup_comparison_matches(
    const struct information_schema_query *query,
    const struct information_schema_catalog_filter *filter,
    const struct information_schema_predicate_instruction *instruction
) {
    return instruction != NULL && instruction->value_count == 1U && instruction->values != NULL &&
           (instruction->operator_kind == MYLITE_SQL_AST_OPERATOR_EQUAL ||
            instruction->operator_kind == MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL) &&
           information_schema_exact_table_lookup_value_matches(
               query,
               filter,
               instruction->column_index,
               &instruction->values[0]
           );
}

static bool information_schema_exact_table_lookup_value_matches(
    const struct information_schema_query *query,
    const struct information_schema_catalog_filter *filter,
    size_t column_index,
    const struct information_schema_predicate_value *value
) {
    const char *expected = NULL;

    if (query == NULL || query->definition == NULL || filter == NULL || value == NULL ||
        value->is_null || value->is_numeric || value->text == NULL) {
        return false;
    }
    if (column_index == filter->schema_column_index) {
        expected = filter->schema_name;
    } else if (column_index == filter->table_column_index) {
        expected = filter->table_name;
    } else if (column_index == filter->table_type_column_index) {
        expected = filter->table_type;
    } else {
        return false;
    }
    if (expected == NULL || column_index >= query->definition->column_count) {
        return false;
    }

    return information_schema_compare_text(
               query->definition,
               column_index,
               value->text,
               expected
           ) == 0;
}

static int information_schema_append_fast_table_existence_result(
    struct mylite_db *database,
    const struct information_schema_query *query,
    bool table_exists,
    bool apply_sql_select_limit,
    mylite_result **out_result
) {
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }
    rc = mylite_execution_information_schema_append_result_columns(database, result, query);
    if (rc == MYLITE_OK) {
        if (query->is_count_star) {
            rc = information_schema_append_fast_table_existence_count_row(
                database,
                query,
                table_exists,
                result
            );
        } else if (table_exists && !(query->limit.has_limit && query->limit.row_count == 0) &&
                   !(query->limit.has_offset && query->limit.offset != 0)) {
            rc =
                information_schema_append_fast_table_existence_literal_row(database, query, result);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    if (apply_sql_select_limit && !query->limit.has_limit) {
        apply_sql_select_limit_to_result(database, result);
    }
    return finish_successful_result(database, result, out_result);
}

static int information_schema_append_fast_table_existence_count_row(
    struct mylite_db *database,
    const struct information_schema_query *query,
    bool table_exists,
    mylite_result *result
) {
    char count_text[integer_text_capacity];
    const char *values[1] = {count_text};
    int rc = MYLITE_OK;

    if ((query->limit.has_limit && query->limit.row_count == 0) ||
        (query->limit.has_offset && query->limit.offset != 0)) {
        return MYLITE_OK;
    }
    rc = information_schema_format_i64(
        database,
        table_exists ? 1 : 0,
        count_text,
        sizeof(count_text)
    );
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    return rc;
}

static int information_schema_append_fast_table_existence_literal_row(
    struct mylite_db *database,
    const struct information_schema_query *query,
    mylite_result *result
) {
    char value_text[integer_text_capacity];
    const char *values[1] = {NULL};
    int rc = mylite_execution_information_schema_integer_literal_projection_value(
        database,
        &query->projection_expressions[0],
        value_text,
        sizeof(value_text),
        &values[0]
    );

    if (rc == MYLITE_OK) {
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    return rc;
}

static int build_information_schema_rows(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_execution_catalog_table_definition *definition,
    struct information_schema_row_set *out_rows
) {
    return build_information_schema_rows_with_filter(
        database,
        context,
        NULL,
        definition,
        NULL,
        NULL,
        NULL,
        out_rows
    );
}

static int build_information_schema_rows_with_filter(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct information_schema_query *query,
    const struct mylite_execution_catalog_table_definition *definition,
    const struct information_schema_catalog_filter *filter,
    mylite_result *stream_result,
    struct information_schema_result_projection_buffers *stream_buffers,
    struct information_schema_row_set *out_rows
) {
    int rc = information_schema_row_set_init(
        database,
        out_rows,
        query,
        definition,
        stream_result,
        stream_buffers
    );

    if (rc == MYLITE_OK && out_rows->has_row_limit && out_rows->row_limit == 0U) {
        return MYLITE_OK;
    }
    if (rc == MYLITE_OK && definition->kind == MYLITE_EXECUTION_CATALOG_TABLE_PROCESSLIST) {
        rc = append_information_schema_processlist_system_row(database, context, out_rows);
    } else if (rc == MYLITE_OK) {
        rc = append_information_schema_system_rows(database, out_rows, filter);
    }
    if (rc == MYLITE_DONE) {
        return MYLITE_OK;
    }
    if (rc == MYLITE_OK) {
        rc = append_information_schema_catalog_rows(database, out_rows, filter);
    }
    if (rc == MYLITE_DONE) {
        return MYLITE_OK;
    }
    if (rc != MYLITE_OK) {
        information_schema_row_set_deinit(out_rows);
    }
    return rc;
}

static int information_schema_row_set_init(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct information_schema_query *query,
    const struct mylite_execution_catalog_table_definition *definition,
    mylite_result *stream_result,
    struct information_schema_result_projection_buffers *stream_buffers
) {
    bool includes_connection_control_failed_login_attempts = false;
    int rc = MYLITE_OK;

    *rows = (struct information_schema_row_set){
        .definition = definition,
        .query = query,
        .predicate = query == NULL ? NULL : &query->predicate,
        .required_columns = query == NULL ? NULL : query->required_columns,
        .stream_result = stream_result,
        .stream_buffers = stream_buffers,
    };
    if (query == NULL || definition == NULL) {
        return MYLITE_OK;
    }
    if (definition->kind == MYLITE_EXECUTION_CATALOG_TABLE_TABLES) {
        rc = mylite_execution_information_schema_includes_connection_control_failed_login_attempts(
            database,
            query,
            &includes_connection_control_failed_login_attempts
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        rows->exclude_connection_control_failed_login_attempts =
            !includes_connection_control_failed_login_attempts;
    }
    rows->predicate_applied = rows->predicate != NULL && rows->predicate->instruction_count != 0U;
    rows->count_only = query->is_count_star;
    if (!query->is_count_star && !query->has_order && !query->has_group && query->limit.has_limit &&
        definition->kind != MYLITE_EXECUTION_CATALOG_TABLE_TABLES &&
        definition->kind != MYLITE_EXECUTION_CATALOG_TABLE_SYS_SCHEMA_AUTO_INCREMENT_COLUMNS) {
        uint64_t row_limit = (uint64_t)query->limit.row_count;
        uint64_t offset = query->limit.has_offset ? (uint64_t)query->limit.offset : 0U;

        rows->has_row_limit = true;
        rows->row_limit = row_limit > SIZE_MAX ? SIZE_MAX : (size_t)row_limit;
        rows->row_offset = offset > SIZE_MAX ? SIZE_MAX : (size_t)offset;
        rows->offset_applied = query->limit.has_offset;
    }
    return MYLITE_OK;
}

static int information_schema_catalog_filter_init_from_query(
    struct mylite_db *database,
    const struct information_schema_query *query,
    struct information_schema_catalog_filter *out_filter
) {
    int rc = MYLITE_OK;

    if (out_filter == NULL) {
        return MYLITE_MISUSE;
    }
    *out_filter = (struct information_schema_catalog_filter){
        .schema_name = NULL,
        .table_name = NULL,
        .table_type = NULL,
        .schema_column_index = SIZE_MAX,
        .table_column_index = SIZE_MAX,
        .table_type_column_index = SIZE_MAX,
        .has_schema_name = false,
        .has_table_name = false,
        .has_table_type = false,
    };
    if (query == NULL || query->definition == NULL) {
        return MYLITE_OK;
    }
    if (!information_schema_catalog_filter_resolve_schema_table_columns(
            query->definition,
            &out_filter->schema_column_index,
            &out_filter->table_column_index
        )) {
        return MYLITE_OK;
    }
    if (query->definition->kind == MYLITE_EXECUTION_CATALOG_TABLE_TABLES) {
        (void)mylite_execution_information_schema_table_definition_index(
            query->definition,
            "TABLE_TYPE",
            &out_filter->table_type_column_index
        );
    }

    if (query->predicate.instruction_count == 0U) {
        return MYLITE_OK;
    }
    rc = information_schema_catalog_filter_collect_predicate(database, query, out_filter);
    if (rc != MYLITE_OK) {
        information_schema_catalog_filter_deinit(out_filter);
    }
    return rc;
}

static bool information_schema_catalog_filter_resolve_schema_table_columns(
    const struct mylite_execution_catalog_table_definition *definition,
    size_t *out_schema_column_index,
    size_t *out_table_column_index
) {
    bool has_schema_column = false;
    bool has_table_column = false;

    if (definition == NULL || out_schema_column_index == NULL || out_table_column_index == NULL) {
        return false;
    }
    if (definition->kind == MYLITE_EXECUTION_CATALOG_TABLE_VIEW_TABLE_USAGE) {
        has_schema_column = mylite_execution_information_schema_table_definition_index(
                                definition,
                                "VIEW_SCHEMA",
                                out_schema_column_index
                            ) == MYLITE_OK;
        has_table_column = mylite_execution_information_schema_table_definition_index(
                               definition,
                               "VIEW_NAME",
                               out_table_column_index
                           ) == MYLITE_OK;
        if (has_schema_column && has_table_column) {
            return true;
        }
    }

    has_schema_column = mylite_execution_information_schema_table_definition_index(
                            definition,
                            "TABLE_SCHEMA",
                            out_schema_column_index
                        ) == MYLITE_OK;
    has_table_column = mylite_execution_information_schema_table_definition_index(
                           definition,
                           "TABLE_NAME",
                           out_table_column_index
                       ) == MYLITE_OK;
    return has_schema_column && has_table_column;
}

static void information_schema_catalog_filter_deinit(
    struct information_schema_catalog_filter *filter
) {
    if (filter == NULL) {
        return;
    }
    free(filter->schema_name);
    free(filter->table_name);
    free(filter->table_type);
    *filter = (struct information_schema_catalog_filter){0};
}

static int information_schema_catalog_filter_collect_predicate(
    struct mylite_db *database,
    const struct information_schema_query *query,
    struct information_schema_catalog_filter *filter
) {
    for (size_t index = 0U; index < query->predicate.instruction_count; ++index) {
        enum information_schema_predicate_instruction_kind kind =
            query->predicate.instructions[index].kind;

        if (kind != INFORMATION_SCHEMA_PREDICATE_COMPARISON &&
            kind != INFORMATION_SCHEMA_PREDICATE_IS_NULL &&
            kind != INFORMATION_SCHEMA_PREDICATE_BETWEEN &&
            kind != INFORMATION_SCHEMA_PREDICATE_IN && kind != INFORMATION_SCHEMA_PREDICATE_AND) {
            return MYLITE_OK;
        }
    }
    for (size_t index = 0U; index < query->predicate.instruction_count; ++index) {
        const struct information_schema_predicate_instruction *instruction =
            &query->predicate.instructions[index];
        int rc = MYLITE_OK;

        if (instruction->kind != INFORMATION_SCHEMA_PREDICATE_COMPARISON) {
            continue;
        }
        rc = information_schema_catalog_filter_collect_comparison(database, instruction, filter);
        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    return MYLITE_OK;
}

static int information_schema_catalog_filter_collect_comparison(
    struct mylite_db *database,
    const struct information_schema_predicate_instruction *instruction,
    struct information_schema_catalog_filter *filter
) {
    if (instruction->operator_kind != MYLITE_SQL_AST_OPERATOR_EQUAL &&
        instruction->operator_kind != MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL) {
        return MYLITE_OK;
    }
    if (instruction->value_count != 1U || instruction->values == NULL ||
        (instruction->column_index != filter->schema_column_index &&
         instruction->column_index != filter->table_column_index &&
         instruction->column_index != filter->table_type_column_index)) {
        return MYLITE_OK;
    }
    return information_schema_catalog_filter_set_value(
        database,
        filter,
        instruction->column_index,
        &instruction->values[0]
    );
}

static int information_schema_catalog_filter_set_value(
    struct mylite_db *database,
    struct information_schema_catalog_filter *filter,
    size_t column_index,
    const struct information_schema_predicate_value *value
) {
    char **target = NULL;
    bool *has_target = NULL;

    if (filter == NULL || value == NULL || value->is_null || value->is_numeric ||
        value->text == NULL) {
        return MYLITE_OK;
    }
    if (column_index == filter->schema_column_index) {
        target = &filter->schema_name;
        has_target = &filter->has_schema_name;
    } else if (column_index == filter->table_column_index) {
        target = &filter->table_name;
        has_target = &filter->has_table_name;
    } else if (column_index == filter->table_type_column_index) {
        target = &filter->table_type;
        has_target = &filter->has_table_type;
    } else {
        return MYLITE_OK;
    }
    if (*target != NULL) {
        return MYLITE_OK;
    }

    if (duplicate_text(database, value->text, target) != MYLITE_OK) {
        return MYLITE_NOMEM;
    }
    *has_target = true;
    return MYLITE_OK;
}

static bool information_schema_catalog_filter_allows_system_rows(
    const struct mylite_execution_catalog_table_definition *definition,
    const struct information_schema_catalog_filter *filter
) {
    return information_schema_catalog_filter_allows_schema(
               definition,
               filter,
               "information_schema"
           ) ||
           information_schema_catalog_filter_allows_schema(definition, filter, "mysql") ||
           information_schema_catalog_filter_allows_schema(
               definition,
               filter,
               "performance_schema"
           ) ||
           information_schema_catalog_filter_allows_schema(definition, filter, "sys");
}

static bool information_schema_catalog_filter_allows_user_catalog_rows(
    const struct mylite_execution_catalog_table_definition *definition,
    const struct information_schema_catalog_filter *filter
) {
    return filter == NULL || !filter->has_schema_name ||
           !information_schema_catalog_filter_allows_system_rows(definition, filter);
}

static bool information_schema_catalog_filter_allows_schema(
    const struct mylite_execution_catalog_table_definition *definition,
    const struct information_schema_catalog_filter *filter,
    const char *schema_name
) {
    if (filter == NULL || !filter->has_schema_name) {
        return true;
    }
    if (schema_name == NULL || definition == NULL ||
        filter->schema_column_index >= definition->column_count) {
        return true;
    }

    return information_schema_compare_column_text(
               &definition->columns[filter->schema_column_index],
               schema_name,
               filter->schema_name
           ) == 0;
}

static bool information_schema_catalog_filter_allows_table(
    const struct mylite_execution_catalog_table_definition *definition,
    const struct information_schema_catalog_filter *filter,
    const char *table_name
) {
    if (filter == NULL || !filter->has_table_name) {
        return true;
    }
    if (table_name == NULL || definition == NULL ||
        filter->table_column_index >= definition->column_count) {
        return true;
    }

    return information_schema_compare_column_text(
               &definition->columns[filter->table_column_index],
               table_name,
               filter->table_name
           ) == 0;
}

static void information_schema_row_set_deinit(struct information_schema_row_set *rows) {
    if (rows == NULL) {
        return;
    }
    if (rows->rows != NULL) {
        for (size_t row_index = 0U; row_index < rows->row_count; ++row_index) {
            for (size_t column_index = 0U; column_index < rows->definition->column_count;
                 ++column_index) {
                free(rows->rows[row_index][column_index]);
            }
            free((void *)rows->rows[row_index]);
        }
    }
    free((void *)rows->rows);
    *rows = (struct information_schema_row_set){0};
}
