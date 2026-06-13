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

    rc = information_schema_resolve_source(database, from_clause, out_query);
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
        rc = information_schema_plan_projection(database, select_list, out_query);
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_plan_where(database, where_clause, out_query);
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_plan_group(database, group_clause, out_query);
    }
    if (rc == MYLITE_OK) {
        out_query->limit = (struct planned_select_limit){
            .has_limit = false,
            .row_count = 0,
            .has_offset = false,
            .offset = 0,
        };
        rc = information_schema_plan_order(database, order_clause, out_query);
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_plan_limit(database, limit_clause, out_query);
    }
    if (rc == MYLITE_OK) {
        return MYLITE_OK;
    }

    information_schema_query_deinit(out_query);
    return rc;
}

static void information_schema_query_deinit(struct information_schema_query *query) {
    if (query == NULL) {
        return;
    }
    for (size_t index = 0U; index < query->projection_count; ++index) {
        free(query->projection_names[index]);
    }
    free((void *)query->projection_names);
    free(query->projection_indexes);
    free(query->projection_kinds);
    free((void *)query->projection_expressions);
    *query = (struct information_schema_query){0};
}

static int execute_information_schema_query(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_sql_ast_node *statement,
    const struct information_schema_query *query,
    bool apply_sql_select_limit,
    mylite_result **out_result
) {
    struct information_schema_row_set rows = {0};
    struct information_schema_catalog_filter filter = {0};
    mylite_result *result = NULL;
    size_t read_row_count = 0U;
    int rc = information_schema_catalog_filter_init_from_statement(
        database,
        statement,
        query,
        &filter
    );

    if (rc == MYLITE_OK) {
        rc = build_information_schema_rows_with_filter(
            database,
            context,
            query->definition,
            &filter,
            &rows
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_append_result_columns(database, result, query);
    }
    if (rc == MYLITE_OK && query->is_count_star) {
        rc = information_schema_append_count_result(
            database,
            statement,
            query,
            &rows,
            result,
            &read_row_count
        );
    } else if (rc == MYLITE_OK && query->has_group) {
        rc = information_schema_append_grouped_statistics_result_rows(
            database,
            statement,
            query,
            &rows,
            result,
            &read_row_count
        );
    } else if (rc == MYLITE_OK) {
        rc = information_schema_append_result_rows(
            database,
            statement,
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

static int build_information_schema_rows(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_execution_catalog_table_definition *definition,
    struct information_schema_row_set *out_rows
) {
    return build_information_schema_rows_with_filter(database, context, definition, NULL, out_rows);
}

static int build_information_schema_rows_with_filter(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_execution_catalog_table_definition *definition,
    const struct information_schema_catalog_filter *filter,
    struct information_schema_row_set *out_rows
) {
    int rc = MYLITE_OK;

    *out_rows = (struct information_schema_row_set){.definition = definition};
    if (definition->kind == MYLITE_EXECUTION_CATALOG_TABLE_PROCESSLIST) {
        rc = append_information_schema_processlist_system_row(database, context, out_rows);
    } else {
        rc = append_information_schema_system_rows(database, out_rows, filter);
    }
    if (rc == MYLITE_OK) {
        rc = append_information_schema_catalog_rows(database, out_rows, filter);
    }
    if (rc != MYLITE_OK) {
        information_schema_row_set_deinit(out_rows);
    }
    return rc;
}

static int information_schema_catalog_filter_init_from_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct information_schema_query *query,
    struct information_schema_catalog_filter *out_filter
) {
    const struct mylite_sql_ast_node *optional_clause =
        select_statement_optional_clause_start(statement);
    const struct mylite_sql_ast_node *where_clause = NULL;
    int rc = MYLITE_OK;

    if (out_filter == NULL) {
        return MYLITE_MISUSE;
    }
    *out_filter = (struct information_schema_catalog_filter){
        .schema_name = NULL,
        .table_name = NULL,
        .schema_column_index = SIZE_MAX,
        .table_column_index = SIZE_MAX,
        .has_schema_name = false,
        .has_table_name = false,
    };
    if (query == NULL || query->definition == NULL) {
        return MYLITE_OK;
    }
    if (information_schema_table_definition_index(
            query->definition,
            "TABLE_SCHEMA",
            &out_filter->schema_column_index
        ) != MYLITE_OK ||
        information_schema_table_definition_index(
            query->definition,
            "TABLE_NAME",
            &out_filter->table_column_index
        ) != MYLITE_OK) {
        return MYLITE_OK;
    }

    while (optional_clause != NULL) {
        if (optional_clause->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
            where_clause = optional_clause;
            break;
        }
        optional_clause = optional_clause->next_sibling;
    }
    if (where_clause == NULL) {
        return MYLITE_OK;
    }

    rc = information_schema_catalog_filter_collect_predicate(
        database,
        query,
        child_at(where_clause, 0U),
        out_filter
    );
    if (rc != MYLITE_OK) {
        information_schema_catalog_filter_deinit(out_filter);
    }
    return rc;
}

static void information_schema_catalog_filter_deinit(
    struct information_schema_catalog_filter *filter
) {
    if (filter == NULL) {
        return;
    }
    free(filter->schema_name);
    free(filter->table_name);
    *filter = (struct information_schema_catalog_filter){0};
}

static int information_schema_catalog_filter_collect_predicate(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate,
    struct information_schema_catalog_filter *filter
) {
    predicate = unwrap_parenthesized_predicate(predicate);
    if (predicate == NULL) {
        return MYLITE_OK;
    }
    if (predicate->kind == MYLITE_SQL_AST_AND_PREDICATE) {
        int rc = information_schema_catalog_filter_collect_predicate(
            database,
            query,
            child_at(predicate, 0U),
            filter
        );

        if (rc != MYLITE_OK) {
            return rc;
        }
        return information_schema_catalog_filter_collect_predicate(
            database,
            query,
            child_at(predicate, 1U),
            filter
        );
    }
    if (predicate->kind == MYLITE_SQL_AST_COMPARISON_PREDICATE) {
        return information_schema_catalog_filter_collect_comparison(
            database,
            query,
            predicate,
            filter
        );
    }
    return MYLITE_OK;
}

static int information_schema_catalog_filter_collect_comparison(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct mylite_sql_ast_node *predicate,
    struct information_schema_catalog_filter *filter
) {
    enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(predicate);
    struct information_schema_predicate_value value = {0};
    size_t column_index = 0U;
    int rc = MYLITE_OK;

    if (operator_kind != MYLITE_SQL_AST_OPERATOR_EQUAL &&
        operator_kind != MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL) {
        return MYLITE_OK;
    }

    rc = information_schema_resolve_column_reference(
        database,
        query,
        child_at(predicate, 0U),
        COLUMN_REFERENCE_WHERE,
        &column_index
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (column_index != filter->schema_column_index &&
        column_index != filter->table_column_index) {
        return MYLITE_OK;
    }

    rc = information_schema_predicate_value_text(database, child_at(predicate, 1U), &value);
    if (rc == MYLITE_OK) {
        rc = information_schema_catalog_filter_set_value(filter, column_index, &value);
    }
    free(value.text);
    return rc;
}

static int information_schema_catalog_filter_set_value(
    struct information_schema_catalog_filter *filter,
    size_t column_index,
    struct information_schema_predicate_value *value
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
    } else {
        return MYLITE_OK;
    }
    if (*target != NULL) {
        return MYLITE_OK;
    }

    *target = value->text;
    value->text = NULL;
    *has_target = true;
    return MYLITE_OK;
}

static bool information_schema_catalog_filter_allows_system_rows(
    const struct mylite_execution_catalog_table_definition *definition,
    const struct information_schema_catalog_filter *filter
) {
    return information_schema_catalog_filter_allows_schema(definition, filter, "information_schema") ||
           information_schema_catalog_filter_allows_schema(definition, filter, "mysql") ||
           information_schema_catalog_filter_allows_schema(definition, filter, "performance_schema") ||
           information_schema_catalog_filter_allows_schema(definition, filter, "sys");
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
    for (size_t row_index = 0U; row_index < rows->row_count; ++row_index) {
        for (size_t column_index = 0U; column_index < rows->definition->column_count;
             ++column_index) {
            free(rows->rows[row_index][column_index]);
        }
        free((void *)rows->rows[row_index]);
    }
    free((void *)rows->rows);
    *rows = (struct information_schema_row_set){0};
}
