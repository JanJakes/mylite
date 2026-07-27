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
    int rc = MYLITE_OK;

    if (database->active_bound_statement != NULL &&
        prepared_dml_plan_reuse_is_enabled(database->active_bound_statement)) {
        return execute_prepared_insert_statement(database, statement, out_result);
    }
    rc = mylite_result_create(&result);
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_insert(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = append_insert_duplicate_update_warnings_if_needed(database, &plan);
    }
    if (rc == MYLITE_OK) {
        rc =
            mylite_execution_reject_read_only_persistent_write(database, &plan.target, &plan.table);
    }
    if (rc == MYLITE_OK) {
        rc = execute_insert_from_plan(database, &plan, NULL, result);
    }
    planned_insert_deinit(&plan);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_execution_clear_next_transaction_characteristics_after_statement(database);
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
        rc = mylite_execution_reject_read_only_persistent_write(
            database,
            &plan.target.target,
            &plan.target.table
        );
    }
    if (rc == MYLITE_OK) {
        rc = execute_insert_select_from_plan(database, &plan, result);
    }
    planned_insert_select_deinit(&plan);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_execution_clear_next_transaction_characteristics_after_statement(database);
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
    int rc = MYLITE_OK;

    if (database->active_bound_statement != NULL &&
        prepared_dml_plan_reuse_is_enabled(database->active_bound_statement)) {
        return execute_prepared_insert_statement(database, statement, out_result);
    }
    rc = mylite_result_create(&result);
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_insert_set(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = append_insert_duplicate_update_warnings_if_needed(database, &plan);
    }
    if (rc == MYLITE_OK) {
        rc =
            mylite_execution_reject_read_only_persistent_write(database, &plan.target, &plan.table);
    }
    if (rc == MYLITE_OK) {
        rc = execute_insert_from_plan(database, &plan, NULL, result);
    }
    planned_insert_deinit(&plan);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_execution_clear_next_transaction_characteristics_after_statement(database);
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
        rc = mylite_execution_reject_read_only_persistent_write(
            database,
            &plan.insert.target,
            &plan.insert.table
        );
    }
    if (rc == MYLITE_OK) {
        rc = execute_load_data_infile_from_plan(database, &plan, result);
    }
    planned_load_data_infile_deinit(&plan);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_execution_clear_next_transaction_characteristics_after_statement(database);
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
    int rc = MYLITE_OK;

    if (database->active_bound_statement != NULL &&
        prepared_dml_plan_reuse_is_enabled(database->active_bound_statement)) {
        return execute_prepared_delete_statement(database, out_result);
    }
    rc = mylite_result_create(&result);
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_delete(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc =
            mylite_execution_reject_read_only_persistent_write(database, &plan.target, &plan.table);
    }
    if (rc == MYLITE_OK) {
        rc = execute_delete_from_plan(database, &plan, result);
    }
    planned_delete_deinit(&plan);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_execution_clear_next_transaction_characteristics_after_statement(database);
    return finish_successful_result(database, result, out_result);
}

static int execute_update_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_update plan = {0};
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    if (database->active_bound_statement != NULL &&
        prepared_dml_plan_reuse_is_enabled(database->active_bound_statement)) {
        return execute_prepared_update_statement(database, out_result);
    }
    rc = mylite_result_create(&result);
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_update(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc =
            mylite_execution_reject_read_only_persistent_write(database, &plan.target, &plan.table);
    }
    if (rc == MYLITE_OK) {
        rc = execute_update_from_plan(database, &plan, result);
    }
    if (rc == MYLITE_OK) {
        rc = append_update_assignment_warnings(database, &plan);
    }
    planned_update_deinit(&plan);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_execution_clear_next_transaction_characteristics_after_statement(database);
    return finish_successful_result(database, result, out_result);
}

static int append_update_assignment_warnings(
    struct mylite_db *database,
    const struct planned_update *plan
) {
    int rc = MYLITE_OK;

    if (plan == NULL) {
        return MYLITE_MISUSE;
    }
    if (planned_update_has_multiple_assignments(plan)) {
        for (size_t index = 0U; rc == MYLITE_OK && index < plan->assignment_count; ++index) {
            if (!planned_update_assignment_is_noop(&plan->assignments[index])) {
                rc = append_planned_scalar_warnings(database, &plan->assignments[index].warnings);
            }
        }
        return rc;
    }
    if (plan->generated_default_noop) {
        return MYLITE_OK;
    }
    return append_planned_scalar_warnings(database, &plan->assignment_warnings);
}

static int execute_prepared_insert_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    mylite_stmt *stmt = database->active_bound_statement;
    struct planned_insert *plan = NULL;
    mylite_result *result = NULL;
    bool created = false;
    int rc = acquire_prepared_dml_result(stmt, &result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }
    rc = ensure_prepared_dml_plan(stmt, MYLITE_ANALYZED_DML_PLAN_INSERT, &created);
    if (rc == MYLITE_OK) {
        plan = &stmt->analyzed_dml->plan.insert;
    }
    if (rc == MYLITE_OK && !created) {
        rc = plan_prepared_insert_dynamic_values(database, statement, plan);
    }
    if (rc == MYLITE_OK) {
        rc = append_insert_duplicate_update_warnings_if_needed(database, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_reject_read_only_persistent_write(
            database,
            &plan->target,
            &plan->table
        );
    }
    if (rc == MYLITE_OK) {
        rc = execute_insert_from_plan(database, plan, stmt->analyzed_dml->insert_sql, result);
    }
    release_one_shot_prepared_dml_plan(stmt);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_execution_clear_next_transaction_characteristics_after_statement(database);
    return finish_successful_result(database, result, out_result);
}

static int execute_prepared_delete_statement(
    struct mylite_db *database,
    mylite_result **out_result
) {
    mylite_stmt *stmt = database->active_bound_statement;
    struct planned_delete *plan = NULL;
    mylite_result *result = NULL;
    bool created = false;
    int rc = acquire_prepared_dml_result(stmt, &result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }
    rc = ensure_prepared_dml_plan(stmt, MYLITE_ANALYZED_DML_PLAN_DELETE, &created);
    (void)created;
    if (rc == MYLITE_OK) {
        plan = &stmt->analyzed_dml->plan.delete;
        rc = mylite_execution_reject_read_only_persistent_write(
            database,
            &plan->target,
            &plan->table
        );
    }
    if (rc == MYLITE_OK) {
        rc = execute_delete_from_plan(database, plan, result);
    }
    release_one_shot_prepared_dml_plan(stmt);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_execution_clear_next_transaction_characteristics_after_statement(database);
    return finish_successful_result(database, result, out_result);
}

static int execute_prepared_update_statement(
    struct mylite_db *database,
    mylite_result **out_result
) {
    mylite_stmt *stmt = database->active_bound_statement;
    struct planned_update *plan = NULL;
    mylite_result *result = NULL;
    bool created = false;
    int rc = acquire_prepared_dml_result(stmt, &result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }
    rc = ensure_prepared_dml_plan(stmt, MYLITE_ANALYZED_DML_PLAN_UPDATE, &created);
    (void)created;
    if (rc == MYLITE_OK) {
        plan = &stmt->analyzed_dml->plan.update;
        rc = mylite_execution_reject_read_only_persistent_write(
            database,
            &plan->target,
            &plan->table
        );
    }
    if (rc == MYLITE_OK) {
        rc = execute_update_from_plan(database, plan, result);
    }
    if (rc == MYLITE_OK) {
        rc = append_update_assignment_warnings(database, plan);
    }
    release_one_shot_prepared_dml_plan(stmt);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_execution_clear_next_transaction_characteristics_after_statement(database);
    return finish_successful_result(database, result, out_result);
}

static int acquire_prepared_dml_result(mylite_stmt *stmt, mylite_result **out_result) {
    if (stmt == NULL || out_result == NULL) {
        return MYLITE_MISUSE;
    }
    if (stmt->reusable_command_result == NULL) {
        return mylite_result_create(out_result);
    }

    *out_result = stmt->reusable_command_result;
    stmt->reusable_command_result = NULL;
    return MYLITE_OK;
}

static bool prepared_dml_plan_reuse_is_enabled(mylite_stmt *stmt) {
    if (stmt == NULL || stmt->statement == NULL) {
        return false;
    }
    if (!stmt->dml_plan_reuse_checked) {
        stmt->dml_plan_reusable = prepared_dml_parameter_contexts_are_reusable(stmt->statement);
        stmt->dml_plan_reuse_checked = true;
    }
    return stmt->dml_plan_reusable;
}

static int ensure_prepared_dml_plan(
    mylite_stmt *stmt,
    enum mylite_analyzed_dml_plan_kind kind,
    bool *out_created
) {
    struct mylite_analyzed_dml_plan *analysis = NULL;
#ifdef MYLITE_ENABLE_PROFILING
    uint64_t profile_plan_started_ns = 0U;
#endif
    int rc = MYLITE_OK;

    if (stmt == NULL || stmt->database == NULL || out_created == NULL) {
        return MYLITE_MISUSE;
    }
    *out_created = false;
    if (!prepared_dml_plan_matches(stmt, kind)) {
        deinit_analyzed_dml_plan(&stmt->analyzed_dml);
    }
    if (stmt->analyzed_dml != NULL) {
#ifdef MYLITE_ENABLE_PROFILING
        mylite_profile_record_dml_plan(stmt->database, 0U, true);
#endif
        return MYLITE_OK;
    }

#ifdef MYLITE_ENABLE_PROFILING
    profile_plan_started_ns = mylite_profile_now_ns();
#endif
    analysis = calloc(1U, sizeof(*analysis));
    if (analysis == NULL) {
        set_nomem_error(stmt->database);
        return MYLITE_NOMEM;
    }
    stmt->analyzed_dml = analysis;
    analysis->kind = kind;
    stmt->parameter_plan_reusable = true;
    switch (kind) {
    case MYLITE_ANALYZED_DML_PLAN_INSERT:
        if (stmt->statement->kind == MYLITE_SQL_AST_INSERT_SET_STATEMENT ||
            stmt->statement->kind == MYLITE_SQL_AST_REPLACE_SET_STATEMENT) {
            rc = plan_insert_set(stmt->database, stmt->statement, &analysis->plan.insert);
        } else {
            rc = plan_insert(stmt->database, stmt->statement, &analysis->plan.insert);
        }
        break;
    case MYLITE_ANALYZED_DML_PLAN_UPDATE:
        rc = plan_update(stmt->database, stmt->statement, &analysis->plan.update);
        break;
    case MYLITE_ANALYZED_DML_PLAN_DELETE:
        rc = plan_delete(stmt->database, stmt->statement, &analysis->plan.delete);
        break;
    case MYLITE_ANALYZED_DML_PLAN_NONE:
        rc = MYLITE_MISUSE;
        break;
    }
    if (rc == MYLITE_OK && kind != MYLITE_ANALYZED_DML_PLAN_INSERT &&
        !stmt->parameter_plan_reusable) {
        stmt->dml_plan_reusable = false;
        *out_created = true;
#ifdef MYLITE_ENABLE_PROFILING
        mylite_profile_record_dml_plan(stmt->database, profile_plan_started_ns, false);
#endif
        return MYLITE_OK;
    }
    if (rc == MYLITE_OK && kind == MYLITE_ANALYZED_DML_PLAN_INSERT) {
        if (insert_plan_can_execute_as_batch(stmt->database, &analysis->plan.insert)) {
            rc = build_insert_rows_sql(&analysis->plan.insert, &analysis->insert_sql);
        } else if (insert_plan_can_execute_with_foreign_key_guards(&analysis->plan.insert)) {
            rc = build_guarded_insert_sql(&analysis->plan.insert, &analysis->insert_sql);
        } else {
            rc = build_insert_sql(&analysis->plan.insert, &analysis->insert_sql);
        }
    }
    if (rc == MYLITE_OK) {
        rc = capture_prepared_dml_bindings(stmt);
    }
    if (rc == MYLITE_OK) {
        analysis->key = current_dml_analysis_key(stmt->database);
        analysis->valid = true;
        *out_created = true;
#ifdef MYLITE_ENABLE_PROFILING
        mylite_profile_record_dml_plan(stmt->database, profile_plan_started_ns, false);
#endif
        return MYLITE_OK;
    }

#ifdef MYLITE_ENABLE_PROFILING
    mylite_profile_record_dml_plan(stmt->database, profile_plan_started_ns, false);
#endif
    deinit_analyzed_dml_plan(&stmt->analyzed_dml);
    return rc;
}

static bool prepared_dml_plan_matches(
    const mylite_stmt *stmt,
    enum mylite_analyzed_dml_plan_kind kind
) {
    const struct mylite_analyzed_dml_plan *analysis = NULL;
    struct mylite_dml_analysis_key key = {0};

    if (stmt == NULL || stmt->database == NULL || stmt->analyzed_dml == NULL) {
        return false;
    }
    analysis = stmt->analyzed_dml;
    if (!analysis->valid || analysis->kind != kind ||
        analysis->binding_type_count != stmt->parameter_count ||
        (stmt->parameter_count != 0U && (analysis->binding_types == NULL || stmt->bindings == NULL)
        )) {
        return false;
    }
    key = current_dml_analysis_key(stmt->database);
    if (analysis->key.catalog_generation != key.catalog_generation ||
        analysis->key.sqlite_schema_generation != key.sqlite_schema_generation ||
        analysis->key.sql_mode != key.sql_mode ||
        (kind != MYLITE_ANALYZED_DML_PLAN_INSERT &&
         analysis->key.last_insert_id != key.last_insert_id) ||
        analysis->key.insert_id != key.insert_id ||
        analysis->key.auto_increment_increment != key.auto_increment_increment ||
        analysis->key.auto_increment_offset != key.auto_increment_offset ||
        analysis->key.time_zone_offset_minutes != key.time_zone_offset_minutes ||
        analysis->key.foreign_key_checks_enabled != key.foreign_key_checks_enabled ||
        analysis->key.sql_safe_updates_enabled != key.sql_safe_updates_enabled ||
        analysis->key.sql_auto_is_null != key.sql_auto_is_null) {
        return false;
    }
    if (kind != MYLITE_ANALYZED_DML_PLAN_INSERT) {
        for (size_t index = 0U; index < stmt->parameter_count; ++index) {
            if (analysis->binding_types[index] != stmt->bindings[index].type) {
                return false;
            }
        }
    }
    return true;
}

static struct mylite_dml_analysis_key current_dml_analysis_key(const struct mylite_db *database) {
    const char *sql_auto_is_null = NULL;

    if (database == NULL) {
        return (struct mylite_dml_analysis_key){0};
    }
    sql_auto_is_null = mylite_execution_session_system_variable_override_value(
        database,
        MYLITE_EXECUTION_SYSTEM_VARIABLE_SQL_AUTO_IS_NULL
    );
    return (struct mylite_dml_analysis_key){
        .catalog_generation = database->session.catalog_generation,
        .sqlite_schema_generation = database->session.sqlite_schema_generation,
        .sql_mode = database->session.sql_mode,
        .last_insert_id = database->session.last_insert_id,
        .insert_id = database->session.insert_id,
        .auto_increment_increment = database->session.auto_increment_increment,
        .auto_increment_offset = database->session.auto_increment_offset,
        .time_zone_offset_minutes = database->session.time_zone_offset_minutes,
        .foreign_key_checks_enabled = foreign_key_checks_system_variable_value(database, false),
        .sql_safe_updates_enabled = sql_safe_updates_is_enabled(database),
        .sql_auto_is_null = sql_auto_is_null != NULL && strcmp(sql_auto_is_null, "1") == 0,
    };
}

static int capture_prepared_dml_bindings(mylite_stmt *stmt) {
    struct mylite_analyzed_dml_plan *analysis = stmt->analyzed_dml;

    if (stmt->parameter_count == 0U) {
        return MYLITE_OK;
    }
    if (stmt->parameter_count > SIZE_MAX / sizeof(*analysis->binding_types)) {
        set_nomem_error(stmt->database);
        return MYLITE_NOMEM;
    }
    analysis->binding_types = malloc(stmt->parameter_count * sizeof(*analysis->binding_types));
    if (analysis->binding_types == NULL) {
        set_nomem_error(stmt->database);
        return MYLITE_NOMEM;
    }
    analysis->binding_type_count = stmt->parameter_count;
    for (size_t index = 0U; index < stmt->parameter_count; ++index) {
        analysis->binding_types[index] = stmt->bindings[index].type;
    }
    return MYLITE_OK;
}

static void release_one_shot_prepared_dml_plan(mylite_stmt *stmt) {
    if (stmt == NULL || stmt->analyzed_dml == NULL ||
        (stmt->analyzed_dml->kind == MYLITE_ANALYZED_DML_PLAN_INSERT && stmt->analyzed_dml->valid
        )) {
        return;
    }
    if (!stmt->analyzed_dml->valid || !stmt->parameter_plan_reusable) {
        stmt->dml_plan_reusable = false;
        deinit_analyzed_dml_plan(&stmt->analyzed_dml);
    }
}

static bool prepared_dml_parameter_contexts_are_reusable(const struct mylite_sql_ast_node *statement
) {
    struct mylite_dml_parameter_context_stack stack = {0};
    bool reusable = statement != NULL;

    if (!reusable) {
        return false;
    }
    if (statement->kind == MYLITE_SQL_AST_INSERT_STATEMENT ||
        statement->kind == MYLITE_SQL_AST_REPLACE_VALUES_STATEMENT ||
        statement->kind == MYLITE_SQL_AST_INSERT_SET_STATEMENT ||
        statement->kind == MYLITE_SQL_AST_REPLACE_SET_STATEMENT) {
        return true;
    }
    reusable = push_dml_parameter_context(
        &stack,
        (struct mylite_dml_parameter_context_frame){
            .node = statement,
            .parent_kind = MYLITE_SQL_AST_SCRIPT,
            .grandparent_kind = MYLITE_SQL_AST_SCRIPT,
            .child_index = SIZE_MAX,
        }
    );
    while (reusable && stack.count != 0U) {
        struct mylite_dml_parameter_context_frame frame = stack.items[--stack.count];
        const struct mylite_sql_ast_node *child = NULL;
        size_t child_index = 0U;

        if (frame.node->kind == MYLITE_SQL_AST_PARAMETER) {
            reusable = prepared_dml_parameter_context_is_reusable(&frame, statement->kind);
            continue;
        }
        for (child = frame.node->first_child; reusable && child != NULL;
             child = child->next_sibling) {
            struct mylite_dml_parameter_context_frame child_frame = {
                .node = child,
                .parent_kind = frame.node->kind,
                .grandparent_kind = frame.parent_kind,
                .child_index = child_index,
            };

            if (frame.node->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
                child_frame.parent_kind = frame.parent_kind;
                child_frame.grandparent_kind = frame.grandparent_kind;
                child_frame.child_index = frame.child_index;
            }
            reusable = push_dml_parameter_context(&stack, child_frame);
            ++child_index;
        }
    }
    free(stack.items);
    return reusable;
}

static bool prepared_dml_parameter_context_is_reusable(
    const struct mylite_dml_parameter_context_frame *frame,
    enum mylite_sql_ast_node_kind statement_kind
) {
    if (frame == NULL) {
        return false;
    }
    if ((statement_kind == MYLITE_SQL_AST_UPDATE_STATEMENT ||
         statement_kind == MYLITE_SQL_AST_JOINED_UPDATE_STATEMENT) &&
        frame->parent_kind == MYLITE_SQL_AST_UPDATE_ASSIGNMENT && frame->child_index == 1U) {
        return true;
    }
    if (frame->parent_kind == MYLITE_SQL_AST_COMPARISON_PREDICATE) {
        return frame->child_index == 1U;
    }
    if (frame->parent_kind == MYLITE_SQL_AST_BETWEEN_PREDICATE) {
        return frame->child_index == 1U || frame->child_index == 2U;
    }
    return frame->parent_kind == MYLITE_SQL_AST_PREDICATE_VALUE_LIST &&
           frame->grandparent_kind == MYLITE_SQL_AST_IN_PREDICATE;
}

static bool push_dml_parameter_context(
    struct mylite_dml_parameter_context_stack *stack,
    struct mylite_dml_parameter_context_frame frame
) {
    struct mylite_dml_parameter_context_frame *items = NULL;
    size_t capacity = 0U;

    if (stack->count == stack->capacity) {
        capacity = stack->capacity == 0U ? dml_parameter_context_stack_initial_capacity
                                         : stack->capacity * 2U;
        if (capacity < stack->capacity || capacity > SIZE_MAX / sizeof(*items)) {
            return false;
        }
        items = realloc(stack->items, capacity * sizeof(*items));
        if (items == NULL) {
            return false;
        }
        stack->items = items;
        stack->capacity = capacity;
    }
    stack->items[stack->count++] = frame;
    return true;
}

static void deinit_analyzed_dml_plan(struct mylite_analyzed_dml_plan **analysis) {
    if (analysis == NULL || *analysis == NULL) {
        return;
    }
    switch ((*analysis)->kind) {
    case MYLITE_ANALYZED_DML_PLAN_INSERT:
        planned_insert_deinit(&(*analysis)->plan.insert);
        break;
    case MYLITE_ANALYZED_DML_PLAN_UPDATE:
        planned_update_deinit(&(*analysis)->plan.update);
        break;
    case MYLITE_ANALYZED_DML_PLAN_DELETE:
        planned_delete_deinit(&(*analysis)->plan.delete);
        break;
    case MYLITE_ANALYZED_DML_PLAN_NONE:
        break;
    }
    free((*analysis)->insert_sql);
    free((*analysis)->binding_types);
    free(*analysis);
    *analysis = NULL;
}
