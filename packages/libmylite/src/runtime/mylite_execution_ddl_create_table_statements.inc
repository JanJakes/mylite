static int execute_create_table_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_create_table plan = {0};
    mylite_result *result = NULL;
    bool finished = false;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_create_table(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = maybe_finish_create_if_not_exists_noop(database, statement, &plan, &finished);
    }
    if (rc == MYLITE_OK && !finished) {
        rc = validate_sql_require_primary_key_for_planned_create_table(database, &plan);
    }
    if (rc == MYLITE_OK && !finished) {
        rc = finalize_planned_column_defaults(database, plan.columns, plan.column_count);
    }
    if (rc == MYLITE_OK && !finished) {
        rc = check_duplicate_column_names(database, plan.columns, plan.column_count);
    }
    if (rc == MYLITE_OK && !finished) {
        rc = validate_planned_table_row_size(database, plan.columns, plan.column_count);
    }
    if (rc == MYLITE_OK && !finished) {
        rc = create_table_from_plan(database, &plan);
    }
    planned_create_table_deinit(&plan);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_create_temporary_table_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_create_table plan = {0};
    mylite_result *result = NULL;
    bool finished = false;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }
    rc = plan_create_table(database, statement, &plan);
    if (rc == MYLITE_OK && planned_create_table_has_fulltext_index(&plan)) {
        set_temporary_fulltext_index_error(database);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && planned_create_table_has_spatial_index(&plan)) {
        set_unsupported_error(database, "Temporary SPATIAL indexes are not yet supported");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && planned_create_table_has_spatial_column(&plan)) {
        set_unsupported_error(database, "Temporary SPATIAL columns are not yet supported");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc =
            maybe_finish_create_temporary_if_not_exists_noop(database, statement, &plan, &finished);
    }
    if (rc == MYLITE_OK && !finished) {
        rc = validate_sql_require_primary_key_for_planned_create_table(database, &plan);
    }
    if (rc == MYLITE_OK && !finished) {
        rc = finalize_planned_column_defaults(database, plan.columns, plan.column_count);
    }
    if (rc == MYLITE_OK && !finished) {
        rc = check_duplicate_column_names(database, plan.columns, plan.column_count);
    }
    if (rc == MYLITE_OK && !finished) {
        rc = validate_planned_table_row_size(database, plan.columns, plan.column_count);
    }
    if (rc == MYLITE_OK && !finished) {
        rc = create_temporary_table_from_plan(database, &plan);
    }
    planned_create_table_deinit(&plan);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_create_table_like_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_create_table_like plan = {0};
    mylite_result *result = NULL;
    bool finished = false;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_create_table_like(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = maybe_finish_create_if_not_exists_noop(
            database,
            statement,
            &plan.create_table,
            &finished
        );
    }
    if (rc == MYLITE_OK && !finished) {
        rc =
            validate_sql_require_primary_key_for_planned_create_table(database, &plan.create_table);
    }
    if (rc == MYLITE_OK && !finished) {
        rc = create_table_from_plan(database, &plan.create_table);
    }
    planned_create_table_like_deinit(&plan);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_create_temporary_table_like_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_create_table_like plan = {0};
    mylite_result *result = NULL;
    bool finished = false;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }
    rc = plan_create_table_like(database, statement, &plan);
    if (rc == MYLITE_OK && planned_create_table_has_fulltext_index(&plan.create_table)) {
        set_temporary_fulltext_index_error(database);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && planned_create_table_has_spatial_index(&plan.create_table)) {
        set_unsupported_error(database, "Temporary SPATIAL indexes are not yet supported");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && planned_create_table_has_spatial_column(&plan.create_table)) {
        set_unsupported_error(database, "Temporary SPATIAL columns are not yet supported");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = maybe_finish_create_temporary_if_not_exists_noop(
            database,
            statement,
            &plan.create_table,
            &finished
        );
    }
    if (rc == MYLITE_OK && !finished) {
        rc =
            validate_sql_require_primary_key_for_planned_create_table(database, &plan.create_table);
    }
    if (rc == MYLITE_OK && !finished) {
        rc = create_temporary_table_from_plan(database, &plan.create_table);
    }
    planned_create_table_like_deinit(&plan);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_create_table_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_create_table_select plan = {0};
    mylite_result *result = NULL;
    bool finished = false;
    int64_t affected_rows = 0;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_create_table_select(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = maybe_finish_create_if_not_exists_noop(
            database,
            statement,
            &plan.create_table,
            &finished
        );
    }
    if (rc == MYLITE_OK && !finished) {
        rc =
            validate_sql_require_primary_key_for_planned_create_table(database, &plan.create_table);
    }
    if (rc == MYLITE_OK && !finished) {
        rc = append_select_modifier_warnings(database, child_at(statement, 1U));
    }
    if (rc == MYLITE_OK && !finished) {
        rc = create_table_select_from_plan(database, &plan, &affected_rows);
    }
    planned_create_table_select_deinit(&plan);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, affected_rows);
    return finish_successful_result(database, result, out_result);
}

static int execute_create_temporary_table_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_create_table_select plan = {0};
    mylite_result *result = NULL;
    bool finished = false;
    int64_t affected_rows = 0;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }
    rc = plan_create_table_select(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = maybe_finish_create_temporary_if_not_exists_noop(
            database,
            statement,
            &plan.create_table,
            &finished
        );
    }
    if (rc == MYLITE_OK && !finished) {
        rc =
            validate_sql_require_primary_key_for_planned_create_table(database, &plan.create_table);
    }
    if (rc == MYLITE_OK && !finished) {
        rc = append_select_modifier_warnings(database, child_at(statement, 1U));
    }
    if (rc == MYLITE_OK && !finished) {
        rc = create_temporary_table_select_from_plan(database, &plan, &affected_rows);
    }
    planned_create_table_select_deinit(&plan);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, affected_rows);
    return finish_successful_result(database, result, out_result);
}
