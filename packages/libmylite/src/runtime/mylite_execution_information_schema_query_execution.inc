static int resolve_information_schema_query(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct information_schema_query *out_query
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *from_clause = child_at(statement, 1U);
    const struct mylite_sql_ast_node *optional_clause = child_at(statement, 2U);
    const struct mylite_sql_ast_node *where_clause = NULL;
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
        } else if (optional_clause->kind == MYLITE_SQL_AST_ORDER_BY_CLAUSE) {
            order_clause = optional_clause;
        } else if (optional_clause->kind == MYLITE_SQL_AST_LIMIT_CLAUSE) {
            limit_clause = optional_clause;
        } else {
            set_unsupported_error(
                database,
                "INFORMATION_SCHEMA SELECT supports only WHERE, ORDER BY, and LIMIT"
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
    mylite_result *result = NULL;
    size_t read_row_count = 0U;
    int rc = build_information_schema_rows(database, context, query->definition, &rows);

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
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    if (apply_sql_select_limit && !query->limit.has_limit) {
        apply_sql_select_limit_to_result(database, result);
    }
    return finish_successful_result(database, result, out_result);
}

static int build_information_schema_rows(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    const struct mylite_execution_catalog_table_definition *definition,
    struct information_schema_row_set *out_rows
) {
    int rc = MYLITE_OK;

    *out_rows = (struct information_schema_row_set){.definition = definition};
    if (definition->kind == MYLITE_EXECUTION_CATALOG_TABLE_PROCESSLIST) {
        rc = append_information_schema_processlist_system_row(database, context, out_rows);
    } else {
        rc = append_information_schema_system_rows(database, out_rows);
    }
    if (rc == MYLITE_OK) {
        rc = append_information_schema_catalog_rows(database, out_rows);
    }
    if (rc != MYLITE_OK) {
        information_schema_row_set_deinit(out_rows);
    }
    return rc;
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
