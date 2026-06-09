static int execute_truncate_table_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_truncate_table plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_truncate_table(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = execute_truncate_from_plan(database, &plan, result);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_rename_table_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_rename_table_statement plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_rename_table_statement(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = rename_table_statement_from_plan(database, &plan);
    }
    planned_rename_table_statement_deinit(&plan);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_alter_table_rename_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_rename_table plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_rename(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = alter_table_rename_from_plan(database, &plan);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_alter_table_add_column_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_add_column plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_add_column(database, statement, &plan);
    if (rc == MYLITE_OK && !plan.adds_inline_primary_key) {
        rc = validate_sql_require_primary_key_for_table_descriptor(database, &plan.table);
    }
    if (rc == MYLITE_OK) {
        rc = reserve_zero_temporal_default_warnings(database, &plan.zero_temporal_default_warnings);
    }
    if (rc == MYLITE_OK) {
        rc = alter_table_add_column_from_plan(database, &plan);
    }
    if (rc == MYLITE_OK) {
        rc = append_zero_temporal_default_warnings(database, &plan.zero_temporal_default_warnings);
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_add_column_deinit(&plan);
        mylite_result_free(result);
        return rc;
    }

    planned_alter_table_add_column_deinit(&plan);
    return finish_successful_result(database, result, out_result);
}

static int execute_alter_table_multi_action_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    const struct mylite_sql_ast_node *table_node = child_at(statement, 0U);
    const struct mylite_sql_ast_node *actions = child_at(statement, 1U);
    const struct mylite_sql_ast_node *action = NULL;
    struct alter_table_multi_action_state state = {0};
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    mylite_result *result = NULL;
    bool physical_schema_changed = false;
    size_t action_count = 0U;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }
    if (actions == NULL || actions->kind != MYLITE_SQL_AST_ALTER_TABLE_ACTION_LIST) {
        mylite_result_free(result);
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    action_count = mylite_sql_ast_node_child_count(actions);
    if (action_count == 0U) {
        mylite_result_free(result);
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    rc = mylite_catalog_begin_mutation(database, &mutation);
    action = child_at(actions, 0U);
    while (rc == MYLITE_OK && action != NULL) {
        bool action_changed_physical_schema = false;

        rc = execute_alter_table_multi_action(
            database,
            table_node,
            action,
            &mutation,
            &state,
            &action_changed_physical_schema
        );
        if (action_changed_physical_schema) {
            physical_schema_changed = true;
        }
        action = action->next_sibling;
    }
    if (rc == MYLITE_OK) {
        rc = validate_alter_table_multi_action_final_state(database, table_node);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to execute multi-action ALTER TABLE");
        mylite_catalog_rollback_mutation(database, &mutation);
        alter_table_multi_action_state_deinit(&state);
        mylite_result_free(result);
        return rc;
    }

    if (physical_schema_changed) {
        ++database->session.sqlite_schema_generation;
    }
    alter_table_multi_action_state_deinit(&state);
    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int execute_alter_table_multi_action(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node,
    const struct mylite_sql_ast_node *action,
    const struct mylite_catalog_mutation *mutation,
    struct alter_table_multi_action_state *state,
    bool *out_physical_schema_changed
) {
    if (action == NULL || out_physical_schema_changed == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    *out_physical_schema_changed = false;
    switch (action->kind) {
    case MYLITE_SQL_AST_ALTER_TABLE_ADD_COLUMN_STATEMENT:
        return execute_alter_table_multi_action_add_column(
            database,
            table_node,
            action,
            mutation,
            state,
            out_physical_schema_changed
        );
    case MYLITE_SQL_AST_ALTER_TABLE_ADD_INDEX_STATEMENT:
        return execute_alter_table_multi_action_add_index(
            database,
            table_node,
            action,
            mutation,
            out_physical_schema_changed
        );
    case MYLITE_SQL_AST_ALTER_TABLE_ADD_PRIMARY_KEY_STATEMENT:
        return execute_alter_table_multi_action_add_primary_key(
            database,
            table_node,
            action,
            mutation,
            out_physical_schema_changed
        );
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_INDEX_STATEMENT:
        return execute_alter_table_multi_action_drop_index(
            database,
            table_node,
            action,
            mutation,
            out_physical_schema_changed
        );
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_PRIMARY_KEY_STATEMENT:
        return execute_alter_table_multi_action_drop_primary_key(
            database,
            table_node,
            action,
            mutation,
            out_physical_schema_changed
        );
    case MYLITE_SQL_AST_ALTER_TABLE_MODIFY_COLUMN_STATEMENT:
        return execute_alter_table_multi_action_modify_column(
            database,
            table_node,
            action,
            mutation,
            out_physical_schema_changed
        );
    case MYLITE_SQL_AST_ALTER_TABLE_CHANGE_COLUMN_STATEMENT:
        return execute_alter_table_multi_action_change_column(
            database,
            table_node,
            action,
            mutation,
            out_physical_schema_changed
        );
    case MYLITE_SQL_AST_ALTER_TABLE_SET_DEFAULT_STATEMENT:
        return execute_alter_table_multi_action_set_default(
            database,
            table_node,
            action,
            mutation,
            state
        );
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_DEFAULT_STATEMENT:
        return execute_alter_table_multi_action_drop_default(
            database,
            table_node,
            action,
            mutation,
            state
        );
    default:
        set_unsupported_error(database, "multi-action ALTER TABLE does not support this action");
        return MYLITE_ERROR;
    }
}

static int execute_alter_table_multi_action_add_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node,
    const struct mylite_sql_ast_node *action,
    const struct mylite_catalog_mutation *mutation,
    struct alter_table_multi_action_state *state,
    bool *out_physical_schema_changed
) {
    struct alter_table_action_statement_view view = {0};
    struct planned_alter_table_add_column plan = {0};
    int rc = MYLITE_OK;

    make_alter_table_action_statement_view(table_node, &view, action);
    rc = plan_alter_table_add_column_for_multi_action(database, &view.statement, &plan);
    if (rc == MYLITE_OK) {
        rc = reject_warning_producing_multi_action_add_column(database, &plan);
    }
    if (rc == MYLITE_OK) {
        rc = alter_table_add_column_in_mutation(database, mutation, &plan);
    }
    if (rc == MYLITE_OK) {
        rc = append_multi_action_added_column(database, state, plan.column.name);
    }
    if (rc == MYLITE_OK) {
        *out_physical_schema_changed = true;
    }

    planned_alter_table_add_column_deinit(&plan);
    return rc;
}

static int execute_alter_table_multi_action_add_index(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node,
    const struct mylite_sql_ast_node *action,
    const struct mylite_catalog_mutation *mutation,
    bool *out_physical_schema_changed
) {
    struct alter_table_action_statement_view view = {0};
    struct planned_alter_table_add_index plan = {0};
    int rc = MYLITE_OK;

    make_alter_table_action_statement_view(table_node, &view, action);
    rc = plan_alter_table_add_index(database, &view.statement, &plan);
    if (rc == MYLITE_OK) {
        rc = reject_unsupported_multi_action_add_index(database, &plan);
    }
    if (rc == MYLITE_OK) {
        rc = add_persistent_secondary_index_in_mutation(
            database,
            mutation,
            &plan,
            out_physical_schema_changed
        );
    }

    planned_alter_table_add_index_deinit(&plan);
    return rc;
}

static int execute_alter_table_multi_action_add_primary_key(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node,
    const struct mylite_sql_ast_node *action,
    const struct mylite_catalog_mutation *mutation,
    bool *out_physical_schema_changed
) {
    struct alter_table_action_statement_view view = {0};
    struct planned_alter_table_add_primary_key plan = {0};
    int rc = MYLITE_OK;

    rc = reject_alter_table_multi_action_temporary_target(database, table_node);
    if (rc == MYLITE_OK) {
        make_alter_table_action_statement_view(table_node, &view, action);
        rc = plan_alter_table_add_primary_key(database, &view.statement, &plan);
    }
    if (rc == MYLITE_OK) {
        rc = reject_unsupported_multi_action_add_primary_key(database, &plan);
    }
    if (rc == MYLITE_OK) {
        rc = validate_alter_table_add_primary_key_existing_rows(database, &plan);
    }
    if (rc == MYLITE_OK) {
        rc = alter_table_add_primary_key_in_mutation(database, mutation, &plan);
    }
    if (rc == MYLITE_OK) {
        *out_physical_schema_changed = true;
    }

    planned_alter_table_add_primary_key_deinit(&plan);
    return rc;
}

static int execute_alter_table_multi_action_drop_index(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node,
    const struct mylite_sql_ast_node *action,
    const struct mylite_catalog_mutation *mutation,
    bool *out_physical_schema_changed
) {
    struct alter_table_action_statement_view view = {0};
    struct planned_drop_index plan = {0};
    int rc = MYLITE_OK;

    make_alter_table_action_statement_view(table_node, &view, action);
    rc = plan_alter_table_drop_index(database, &view.statement, &plan);
    if (rc == MYLITE_OK && plan.table.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(
            database,
            "multi-action ALTER TABLE supports only persistent base tables"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = drop_persistent_index_in_mutation(
            database,
            mutation,
            &plan,
            out_physical_schema_changed
        );
    }

    planned_drop_index_deinit(&plan);
    return rc;
}

static int execute_alter_table_multi_action_drop_primary_key(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node,
    const struct mylite_sql_ast_node *action,
    const struct mylite_catalog_mutation *mutation,
    bool *out_physical_schema_changed
) {
    struct alter_table_action_statement_view view = {0};
    struct planned_alter_table_drop_primary_key plan = {0};
    int rc = MYLITE_OK;

    rc = reject_alter_table_multi_action_temporary_target(database, table_node);
    if (rc == MYLITE_OK) {
        make_alter_table_action_statement_view(table_node, &view, action);
        rc = plan_alter_table_drop_primary_key_for_multi_action(database, &view.statement, &plan);
    }
    if (rc == MYLITE_OK && plan.table.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(
            database,
            "multi-action ALTER TABLE supports only persistent base tables"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = alter_table_drop_primary_key_in_mutation(database, mutation, &plan);
    }
    if (rc == MYLITE_OK) {
        *out_physical_schema_changed = true;
    }

    planned_alter_table_drop_primary_key_deinit(&plan);
    return rc;
}

static int execute_alter_table_multi_action_modify_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node,
    const struct mylite_sql_ast_node *action,
    const struct mylite_catalog_mutation *mutation,
    bool *out_physical_schema_changed
) {
    struct alter_table_action_statement_view view = {0};
    struct planned_alter_table_modify_column plan = {0};
    int rc = MYLITE_OK;

    rc = reject_alter_table_multi_action_temporary_target(database, table_node);
    if (rc == MYLITE_OK) {
        make_alter_table_action_statement_view(table_node, &view, action);
        rc = plan_alter_table_modify_column_for_multi_action(database, &view.statement, &plan);
    }
    if (rc == MYLITE_OK && plan.zero_temporal_default_warnings.count > 0U) {
        set_unsupported_error(
            database,
            "multi-action ALTER TABLE does not yet support warning-producing MODIFY COLUMN"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = alter_table_modify_column_in_mutation(
            database,
            mutation,
            &plan,
            out_physical_schema_changed
        );
    }

    planned_alter_table_modify_column_deinit(&plan);
    return rc;
}

static int execute_alter_table_multi_action_change_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node,
    const struct mylite_sql_ast_node *action,
    const struct mylite_catalog_mutation *mutation,
    bool *out_physical_schema_changed
) {
    struct alter_table_action_statement_view view = {0};
    struct planned_alter_table_modify_column plan = {0};
    int rc = MYLITE_OK;

    rc = reject_alter_table_multi_action_temporary_target(database, table_node);
    if (rc == MYLITE_OK) {
        make_alter_table_action_statement_view(table_node, &view, action);
        rc = plan_alter_table_change_column_for_multi_action(database, &view.statement, &plan);
    }
    if (rc == MYLITE_OK && plan.zero_temporal_default_warnings.count > 0U) {
        set_unsupported_error(
            database,
            "multi-action ALTER TABLE does not yet support warning-producing CHANGE COLUMN"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = alter_table_modify_column_in_mutation(
            database,
            mutation,
            &plan,
            out_physical_schema_changed
        );
    }

    planned_alter_table_modify_column_deinit(&plan);
    return rc;
}

static int execute_alter_table_multi_action_set_default(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node,
    const struct mylite_sql_ast_node *action,
    const struct mylite_catalog_mutation *mutation,
    const struct alter_table_multi_action_state *state
) {
    const struct alter_table_multi_default_target_view target = {
        .table_node = table_node,
        .action = action,
    };
    struct alter_table_action_statement_view view = {0};
    struct planned_alter_table_set_default plan = {0};
    int rc = reject_multi_action_default_target_added_in_statement(database, &target, state);

    if (rc == MYLITE_OK) {
        rc = reject_alter_table_multi_action_temporary_target(database, table_node);
    }
    if (rc == MYLITE_OK) {
        make_alter_table_action_statement_view(table_node, &view, action);
        rc = plan_alter_table_set_default(database, &view.statement, &plan);
    }
    if (rc == MYLITE_OK && plan.zero_temporal_default_warnings.count > 0U) {
        set_unsupported_error(
            database,
            "multi-action ALTER TABLE does not yet support warning-producing SET DEFAULT"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = alter_table_set_default_in_mutation(database, mutation, &plan);
    }

    planned_alter_table_set_default_deinit(&plan);
    return rc;
}

static int execute_alter_table_multi_action_drop_default(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node,
    const struct mylite_sql_ast_node *action,
    const struct mylite_catalog_mutation *mutation,
    const struct alter_table_multi_action_state *state
) {
    const struct alter_table_multi_default_target_view target = {
        .table_node = table_node,
        .action = action,
    };
    struct alter_table_action_statement_view view = {0};
    struct planned_alter_table_drop_default plan = {0};
    int rc = reject_multi_action_default_target_added_in_statement(database, &target, state);

    if (rc == MYLITE_OK) {
        rc = reject_alter_table_multi_action_temporary_target(database, table_node);
    }
    if (rc == MYLITE_OK) {
        make_alter_table_action_statement_view(table_node, &view, action);
        rc = plan_alter_table_drop_default(database, &view.statement, &plan);
    }
    if (rc == MYLITE_OK) {
        rc = alter_table_drop_default_in_mutation(database, mutation, &plan);
    }

    return rc;
}

static int validate_alter_table_multi_action_final_state(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node
) {
    struct table_name_resolution target = {0};
    struct mylite_catalog_table_descriptor table = {0};
    int rc = resolve_writable_table_name(database, table_node, &target);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_read_table_by_name(
            database,
            target.schema.schema_id,
            target.table_name,
            &table
        );
        if (rc != MYLITE_OK) {
            set_table_does_not_exist_error(database, target.schema.name, target.table_name);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && table.kind == MYLITE_CATALOG_TABLE_KIND_BASE) {
        rc = validate_sql_require_primary_key_for_table_descriptor(database, &table);
    }
    if (rc == MYLITE_OK && table.kind == MYLITE_CATALOG_TABLE_KIND_BASE) {
        rc = validate_alter_table_multi_action_auto_increment_keys(database, &table);
    }

    return rc;
}

static int validate_alter_table_multi_action_auto_increment_keys(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_index_info *indexes = NULL;
    size_t column_count = 0U;
    size_t index_count = 0U;
    int rc = MYLITE_OK;

    if (table == NULL) {
        set_runtime_error(database, "invalid multi-action final table");
        return MYLITE_ERROR;
    }

    rc = mylite_execution_load_table_columns(database, table->table_id, &columns, &column_count);
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_index_infos(
            database,
            table->table_id,
            columns,
            column_count,
            &indexes,
            &index_count
        );
    }
    for (size_t column = 0U; rc == MYLITE_OK && column < column_count; ++column) {
        const struct alter_table_multi_auto_increment_key_lookup lookup = {
            .indexes = indexes,
            .index_count = index_count,
            .column_id = columns[column].column_id,
        };

        if (!columns[column].is_auto_increment) {
            continue;
        }
        if (!table_index_infos_have_leading_column(&lookup)) {
            set_wrong_auto_key_error(database);
            rc = MYLITE_ERROR;
        }
    }

    mylite_execution_loaded_index_infos_deinit(&indexes, &index_count);
    free(columns);
    return rc;
}

static bool table_index_infos_have_leading_column(
    const struct alter_table_multi_auto_increment_key_lookup *lookup
) {
    if (lookup == NULL) {
        return false;
    }

    for (size_t index = 0U; index < lookup->index_count; ++index) {
        if (lookup->indexes[index].part_count == 0U) {
            continue;
        }
        if (lookup->indexes[index].parts[0].column.column_id == lookup->column_id) {
            return true;
        }
    }

    return false;
}

static void make_alter_table_action_statement_view(
    const struct mylite_sql_ast_node *table_node,
    struct alter_table_action_statement_view *out_view,
    const struct mylite_sql_ast_node *action
) {
    if (out_view == NULL) {
        return;
    }

    *out_view = (struct alter_table_action_statement_view){0};
    if (table_node != NULL) {
        out_view->table_proxy = *table_node;
        out_view->table_proxy.next_sibling = action == NULL ? NULL : action->first_child;
        out_view->table_proxy.next_allocated = NULL;
    }
    if (action != NULL) {
        out_view->statement = *action;
        out_view->statement.first_child = &out_view->table_proxy;
        out_view->statement.last_child =
            action->last_child == NULL ? &out_view->table_proxy : action->last_child;
        out_view->statement.next_sibling = NULL;
        out_view->statement.next_allocated = NULL;
    }
}

static int reject_warning_producing_multi_action_add_column(
    struct mylite_db *database,
    const struct planned_alter_table_add_column *plan
) {
    if (plan != NULL && plan->table.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(
            database,
            "multi-action ALTER TABLE supports only persistent base tables"
        );
        return MYLITE_ERROR;
    }
    if (plan != NULL && plan->zero_temporal_default_warnings.count > 0U) {
        set_unsupported_error(
            database,
            "multi-action ALTER TABLE does not yet support warning-producing ADD COLUMN"
        );
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int reject_unsupported_multi_action_add_index(
    struct mylite_db *database,
    const struct planned_alter_table_add_index *plan
) {
    if (plan == NULL) {
        set_runtime_error(database, "invalid multi-action add-index plan");
        return MYLITE_ERROR;
    }
    if (plan->table.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(
            database,
            "multi-action ALTER TABLE supports only persistent base tables"
        );
        return MYLITE_ERROR;
    }
    if (planned_add_index_is_fulltext(plan) || planned_add_index_is_spatial(plan) ||
        plan->uses_hash_index_type) {
        set_unsupported_error(
            database,
            "multi-action ALTER TABLE does not yet support warning-producing ADD INDEX"
        );
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int reject_unsupported_multi_action_add_primary_key(
    struct mylite_db *database,
    const struct planned_alter_table_add_primary_key *plan
) {
    if (plan == NULL) {
        set_runtime_error(database, "invalid multi-action add-primary-key plan");
        return MYLITE_ERROR;
    }
    if (plan->table.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(
            database,
            "multi-action ALTER TABLE supports only persistent base tables"
        );
        return MYLITE_ERROR;
    }
    if (plan->uses_hash_index_type) {
        set_unsupported_error(
            database,
            "multi-action ALTER TABLE does not yet support warning-producing ADD PRIMARY KEY"
        );
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int reject_multi_action_default_target_added_in_statement(
    struct mylite_db *database,
    const struct alter_table_multi_default_target_view *target,
    const struct alter_table_multi_action_state *state
) {
    char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char table_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = MYLITE_OK;

    if (target == NULL) {
        set_runtime_error(database, "invalid multi-action default target");
        return MYLITE_ERROR;
    }

    rc = copy_multi_action_default_target_name(
        database,
        target->action,
        column_name,
        sizeof(column_name)
    );

    if (rc == MYLITE_OK && multi_action_state_has_added_column(state, column_name)) {
        rc = copy_multi_action_table_leaf_name(
            database,
            target->table_node,
            table_name,
            sizeof(table_name)
        );
        if (rc == MYLITE_OK) {
            set_unknown_column_in_table_error(database, column_name, table_name);
            rc = MYLITE_ERROR;
        }
    }

    return rc;
}

static int reject_alter_table_multi_action_temporary_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node
) {
    struct table_name_resolution target = {0};
    struct mylite_catalog_table_descriptor temporary_table = {0};
    bool found_temporary = false;
    int rc = resolve_writable_table_name(database, table_node, &target);

    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_temporary_catalog_try_read_table_by_name(
        &database->session.temporary_catalog,
        target.schema.name,
        target.table_name,
        &temporary_table,
        &found_temporary
    );
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to read temporary table descriptor");
        return rc;
    }
    if (found_temporary) {
        set_unsupported_error(
            database,
            "multi-action ALTER TABLE supports only persistent base tables"
        );
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static bool multi_action_state_has_added_column(
    const struct alter_table_multi_action_state *state,
    const char *column_name
) {
    if (state == NULL || column_name == NULL) {
        return false;
    }
    for (size_t index = 0U; index < state->added_column_count; ++index) {
        if (text_equals_ascii_case_insensitive(state->added_columns[index].name, column_name)) {
            return true;
        }
    }
    return false;
}

static int copy_multi_action_default_target_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *action,
    char *out_name,
    size_t out_size
) {
    if (action == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    return copy_identifier_text(child_at(action, 0U), out_name, out_size, database);
}

static int copy_multi_action_table_leaf_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_node,
    char *out_name,
    size_t out_size
) {
    const struct mylite_sql_ast_node *name_node = table_node;

    if (table_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (table_node->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        name_node = child_at(table_node, 1U);
    }
    return copy_identifier_text(name_node, out_name, out_size, database);
}

static int append_multi_action_added_column(
    struct mylite_db *database,
    struct alter_table_multi_action_state *state,
    const char *column_name
) {
    int rc = MYLITE_OK;

    if (state == NULL || column_name == NULL) {
        return MYLITE_OK;
    }
    if (state->added_column_count == state->added_column_capacity) {
        size_t next_capacity =
            state->added_column_capacity == 0U ? 4U : state->added_column_capacity * 2U;

        if (next_capacity < state->added_column_capacity) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        rc = reserve_multi_action_added_columns(database, state, next_capacity);
    }
    if (rc == MYLITE_OK) {
        int copied = snprintf(
            state->added_columns[state->added_column_count].name,
            sizeof(state->added_columns[state->added_column_count].name),
            "%s",
            column_name
        );

        if (copied < 0 ||
            (size_t)copied >= sizeof(state->added_columns[state->added_column_count].name)) {
            set_runtime_error(database, "column name exceeds descriptor capacity");
            return MYLITE_ERROR;
        }
        ++state->added_column_count;
    }

    return rc;
}

static int reserve_multi_action_added_columns(
    struct mylite_db *database,
    struct alter_table_multi_action_state *state,
    size_t required_capacity
) {
    struct alter_table_multi_added_column *columns = NULL;

    if (required_capacity <= state->added_column_capacity) {
        return MYLITE_OK;
    }
    if (required_capacity > SIZE_MAX / sizeof(*columns)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    columns = realloc(state->added_columns, required_capacity * sizeof(*columns));
    if (columns == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    state->added_columns = columns;
    state->added_column_capacity = required_capacity;
    return MYLITE_OK;
}

static void alter_table_multi_action_state_deinit(struct alter_table_multi_action_state *state) {
    if (state == NULL) {
        return;
    }

    free(state->added_columns);
    *state = (struct alter_table_multi_action_state){0};
}
