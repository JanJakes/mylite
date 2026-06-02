
static int execute_scalar_projection_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *select_item = child_at(select_list, 0U);
    struct session_scalar_cell *cells = NULL;
    struct mylite_result_cell *values = NULL;
    mylite_result *result = NULL;
    size_t column_count = mylite_sql_ast_node_child_count(select_list);
    size_t evaluated_column_count = 0U;
    uint64_t previous_last_insert_id = database->session.last_insert_id;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }
    if (column_count > SIZE_MAX / sizeof(*values) || column_count > SIZE_MAX / sizeof(*cells)) {
        mylite_result_free(result);
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    cells = (struct session_scalar_cell *)calloc(column_count, sizeof(*cells));
    if (cells == NULL) {
        mylite_result_free(result);
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    values = (struct mylite_result_cell *)calloc(column_count, sizeof(*values));
    if (values == NULL) {
        session_scalar_cell_array_deinit(cells, 0U);
        mylite_result_free(result);
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = append_select_modifier_warnings(database, statement);
    if (rc == MYLITE_OK) {
        rc = append_session_scalar_select_warnings(database, select_list);
    }
    if (rc == MYLITE_OK) {
        rc = append_scalar_projection_columns_and_values(
            database,
            select_item,
            result,
            cells,
            values,
            &evaluated_column_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_bytes_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK || evaluated_column_count != 0U) {
        int warning_rc =
            append_session_scalar_cells_warnings(database, cells, evaluated_column_count);

        if (warning_rc != MYLITE_OK) {
            rc = warning_rc;
        }
    }
    free(values);
    session_scalar_cell_array_deinit(cells, evaluated_column_count);
    if (rc != MYLITE_OK) {
        database->session.last_insert_id = previous_last_insert_id;
        mylite_result_free(result);
        return rc;
    }
    if (apply_sql_select_limit) {
        apply_sql_select_limit_to_result(database, result);
    }

    return finish_successful_result(database, result, out_result);
}

static int append_scalar_projection_columns_and_values(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_item,
    mylite_result *result,
    struct session_scalar_cell *cells,
    struct mylite_result_cell *values,
    size_t *out_column_count
) {
    size_t column_index = 0U;
    int rc = MYLITE_OK;

    while (rc == MYLITE_OK && select_item != NULL) {
        const struct mylite_sql_ast_node *expression = child_at(select_item, 0U);
        const struct mylite_sql_ast_node *alias = child_at(select_item, 1U);
        char *column_name = NULL;

        if (alias != NULL) {
            rc = copy_select_item_alias_text(database, alias, &column_name);
        } else {
            rc = copy_scalar_projection_column_name(database, expression, &column_name);
        }
        if (rc == MYLITE_OK) {
            rc = append_scalar_projection_result_column(database, expression, column_name, result);
        }
        free(column_name);
        if (rc == MYLITE_OK) {
            rc = session_scalar_value(database, expression, &cells[column_index]);
        }
        if (rc == MYLITE_OK) {
            values[column_index] = session_scalar_cell_result_cell(&cells[column_index]);
            ++column_index;
            select_item = select_item->next_sibling;
        }
    }

    *out_column_count = column_index;
    return rc;
}

static int append_scalar_projection_result_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *column_name,
    mylite_result *result
) {
    struct mylite_result_column_descriptor descriptor;
    int rc = make_scalar_result_column_descriptor(database, expression, column_name, &descriptor);

    if (rc == MYLITE_OK) {
        rc = mylite_result_append_column_descriptor(result, &descriptor);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    return rc;
}
