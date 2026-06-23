static int execute_insert_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    int rc = append_insert_delayed_warning_if_needed(database, statement);

    if (rc != MYLITE_OK) {
        return rc;
    }
    return execute_planned_insert_statement(database, statement, out_result);
}

static int append_insert_delayed_warning_if_needed(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
) {
    int rc = MYLITE_OK;

    if (child_with_kind(statement, MYLITE_SQL_AST_INSERT_DELAYED_MODIFIER) == NULL) {
        return MYLITE_OK;
    }

    rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_legacy_syntax_converted,
        "HY000",
        "INSERT DELAYED is no longer supported. The statement was converted to INSERT."
    );
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    return rc;
}

static int execute_replace_values_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    int rc = append_replace_delayed_warning_if_needed(database, statement);

    if (rc != MYLITE_OK) {
        return rc;
    }
    return execute_planned_insert_statement(database, statement, out_result);
}

static int append_replace_delayed_warning_if_needed(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
) {
    int rc = MYLITE_OK;

    if (child_with_kind(statement, MYLITE_SQL_AST_REPLACE_DELAYED_MODIFIER) == NULL) {
        return MYLITE_OK;
    }

    rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_legacy_syntax_converted,
        "HY000",
        "REPLACE DELAYED is no longer supported. The statement was converted to REPLACE."
    );
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    return rc;
}

static int execute_planned_insert_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_insert plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_insert(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = append_insert_duplicate_update_warnings_if_needed(database, &plan);
    }
    if (rc == MYLITE_OK) {
        rc = reject_read_only_persistent_write(database, &plan.target, &plan.table);
    }
    if (rc == MYLITE_OK) {
        rc = execute_insert_from_plan(database, &plan, result);
    }
    planned_insert_deinit(&plan);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    clear_next_transaction_characteristics_after_statement(database);
    return finish_successful_result(database, result, out_result);
}

static int execute_insert_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    int rc = MYLITE_OK;

    if (child_with_kind(statement, MYLITE_SQL_AST_INSERT_IGNORE_MODIFIER) != NULL &&
        child_with_kind(statement, MYLITE_SQL_AST_INSERT_DELAYED_MODIFIER) != NULL) {
        set_unsupported_error(database, "INSERT IGNORE ... SELECT does not support DELAYED");
        return MYLITE_ERROR;
    }
    rc = append_insert_delayed_warning_if_needed(database, statement);
    if (rc != MYLITE_OK) {
        return rc;
    }
    return execute_planned_insert_select_statement(database, statement, out_result);
}

static int execute_replace_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    int rc = append_replace_delayed_warning_if_needed(database, statement);

    if (rc != MYLITE_OK) {
        return rc;
    }
    return execute_planned_insert_select_statement(database, statement, out_result);
}

static int execute_planned_insert_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_insert_select plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_insert_select(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = append_insert_duplicate_update_warnings_if_needed(database, &plan.target);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_modifier_warnings(database, child_at(statement, 2U));
    }
    if (rc == MYLITE_OK) {
        rc = reject_read_only_persistent_write(database, &plan.target.target, &plan.target.table);
    }
    if (rc == MYLITE_OK) {
        rc = execute_insert_select_from_plan(database, &plan, result);
    }
    planned_insert_select_deinit(&plan);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    clear_next_transaction_characteristics_after_statement(database);
    return finish_successful_result(database, result, out_result);
}

static int execute_insert_set_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    int rc = append_insert_delayed_warning_if_needed(database, statement);

    if (rc != MYLITE_OK) {
        return rc;
    }
    return execute_planned_insert_set_statement(database, statement, out_result);
}

static int execute_replace_set_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    int rc = append_replace_delayed_warning_if_needed(database, statement);

    if (rc != MYLITE_OK) {
        return rc;
    }
    return execute_planned_insert_set_statement(database, statement, out_result);
}

static int execute_planned_insert_set_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_insert plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_insert_set(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = append_insert_duplicate_update_warnings_if_needed(database, &plan);
    }
    if (rc == MYLITE_OK) {
        rc = reject_read_only_persistent_write(database, &plan.target, &plan.table);
    }
    if (rc == MYLITE_OK) {
        rc = execute_insert_from_plan(database, &plan, result);
    }
    planned_insert_deinit(&plan);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    clear_next_transaction_characteristics_after_statement(database);
    return finish_successful_result(database, result, out_result);
}

static int execute_load_data_infile_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_load_data_infile plan = {0};
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    if (child_with_kind(statement, MYLITE_SQL_AST_LOAD_DATA_LOCAL_MODIFIER) != NULL) {
        set_load_data_local_disabled_error(database);
        return MYLITE_ERROR;
    }

    rc = mylite_result_create(&result);
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_load_data_infile(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = reject_read_only_persistent_write(database, &plan.insert.target, &plan.insert.table);
    }
    if (rc == MYLITE_OK) {
        rc = execute_load_data_infile_from_plan(database, &plan, result);
    }
    planned_load_data_infile_deinit(&plan);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    clear_next_transaction_characteristics_after_statement(database);
    return finish_successful_result(database, result, out_result);
}

static int append_insert_duplicate_update_warnings_if_needed(
    struct mylite_db *database,
    const struct planned_insert *plan
) {
    int rc = MYLITE_OK;

    if (plan == NULL || !plan->duplicate_update.has_clause) {
        return MYLITE_OK;
    }

    for (size_t index = 0U; rc == MYLITE_OK && index < plan->duplicate_update.assignment_count;
         ++index) {
        const size_t warning_count =
            plan->duplicate_update.assignments[index].values_reference_count;

        for (size_t warning_index = 0U; rc == MYLITE_OK && warning_index < warning_count;
             ++warning_index) {
            rc = mylite_diagnostics_append_warning(
                mylite_connection_diagnostics(database),
                mysql_warning_values_function_deprecated,
                "HY000",
                "'VALUES function' is deprecated and will be removed in a future release. "
                "Please use an alias (INSERT INTO ... VALUES (...) AS alias) and replace "
                "VALUES(col) in the ON DUPLICATE KEY UPDATE clause with alias.col instead"
            );
            if (rc == MYLITE_NOMEM) {
                set_nomem_error(database);
            }
        }
    }
    return rc;
}

static int execute_delete_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_delete plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_delete(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = reject_read_only_persistent_write(database, &plan.target, &plan.table);
    }
    if (rc == MYLITE_OK) {
        rc = execute_delete_from_plan(database, &plan, result);
    }
    planned_delete_deinit(&plan);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    clear_next_transaction_characteristics_after_statement(database);
    return finish_successful_result(database, result, out_result);
}

static int execute_update_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_update plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_update(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = reject_read_only_persistent_write(database, &plan.target, &plan.table);
    }
    if (rc == MYLITE_OK) {
        rc = execute_update_from_plan(database, &plan, result);
    }
    planned_update_deinit(&plan);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    clear_next_transaction_characteristics_after_statement(database);
    return finish_successful_result(database, result, out_result);
}
