static int execute_alter_table_drop_column_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_drop_column plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_drop_column(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = validate_sql_require_primary_key_for_table_descriptor(database, &plan.table);
    }
    if (rc == MYLITE_OK && plan.removes_one_part_primary_key &&
        sql_require_primary_key_session_enabled(database)) {
        set_sql_require_primary_key_error(database);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = alter_table_drop_column_from_plan(database, &plan);
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_drop_column_deinit(&plan);
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, plan.affected_rows);
    planned_alter_table_drop_column_deinit(&plan);
    return finish_successful_result(database, result, out_result);
}

static int execute_alter_table_rename_column_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_rename_column plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_rename_column(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = validate_sql_require_primary_key_for_table_descriptor(database, &plan.table);
    }
    if (rc == MYLITE_OK) {
        rc = alter_table_rename_column_from_plan(database, &plan);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_alter_table_modify_column_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_modify_column plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_modify_column(database, statement, &plan);
    if (rc == MYLITE_OK && !plan.adds_inline_primary_key) {
        rc = validate_sql_require_primary_key_for_table_descriptor(database, &plan.table);
    }
    if (rc == MYLITE_OK) {
        rc = reserve_zero_temporal_default_warnings(database, &plan.zero_temporal_default_warnings);
    }
    if (rc == MYLITE_OK) {
        rc = alter_table_modify_column_from_plan(database, &plan);
    }
    if (rc == MYLITE_OK) {
        rc = append_zero_temporal_default_warnings(database, &plan.zero_temporal_default_warnings);
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_modify_column_deinit(&plan);
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, plan.affected_rows);
    planned_alter_table_modify_column_deinit(&plan);
    return finish_successful_result(database, result, out_result);
}

static int execute_alter_table_change_column_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_modify_column plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_change_column(database, statement, &plan);
    if (rc == MYLITE_OK && !plan.adds_inline_primary_key) {
        rc = validate_sql_require_primary_key_for_table_descriptor(database, &plan.table);
    }
    if (rc == MYLITE_OK) {
        rc = reserve_zero_temporal_default_warnings(database, &plan.zero_temporal_default_warnings);
    }
    if (rc == MYLITE_OK) {
        rc = alter_table_modify_column_from_plan(database, &plan);
    }
    if (rc == MYLITE_OK) {
        rc = append_zero_temporal_default_warnings(database, &plan.zero_temporal_default_warnings);
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_modify_column_deinit(&plan);
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, plan.affected_rows);
    planned_alter_table_modify_column_deinit(&plan);
    return finish_successful_result(database, result, out_result);
}

static int execute_alter_table_set_default_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_set_default plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_set_default(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = validate_sql_require_primary_key_for_table_descriptor(database, &plan.table);
    }
    if (rc == MYLITE_OK) {
        rc = reserve_zero_temporal_default_warnings(database, &plan.zero_temporal_default_warnings);
    }
    if (rc == MYLITE_OK) {
        rc = alter_table_set_default_from_plan(database, &plan);
    }
    if (rc == MYLITE_OK) {
        rc = append_zero_temporal_default_warnings(database, &plan.zero_temporal_default_warnings);
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_set_default_deinit(&plan);
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    planned_alter_table_set_default_deinit(&plan);
    return finish_successful_result(database, result, out_result);
}

static int execute_alter_table_drop_default_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_drop_default plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_drop_default(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = validate_sql_require_primary_key_for_table_descriptor(database, &plan.table);
    }
    if (rc == MYLITE_OK) {
        rc = alter_table_drop_default_from_plan(database, &plan);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int execute_alter_table_column_visibility_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_column_visibility plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_column_visibility(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = validate_sql_require_primary_key_for_table_descriptor(database, &plan.table);
    }
    if (rc == MYLITE_OK) {
        rc = alter_table_column_visibility_from_plan(database, &plan);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int execute_alter_table_index_visibility_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_index_visibility plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_index_visibility(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = validate_sql_require_primary_key_for_table_descriptor(database, &plan.table);
    }
    if (rc == MYLITE_OK) {
        rc = alter_table_index_visibility_from_plan(database, &plan);
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_index_visibility_deinit(&plan);
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, plan.affected_rows);
    planned_alter_table_index_visibility_deinit(&plan);
    return finish_successful_result(database, result, out_result);
}
