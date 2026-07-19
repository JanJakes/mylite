static int execute_drop_table_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_drop_table plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_drop_table(database, statement, false, &plan);
    if (rc == MYLITE_OK && drop_table_plan_requires_implicit_commit(&plan)) {
        rc = commit_active_user_transaction_for_ddl(database);
    }
    if (rc == MYLITE_OK && plan.missing_count != 0U && !drop_table_has_if_exists(statement)) {
        rc = finish_drop_table_missing_targets(database, &plan);
    }
    if (rc == MYLITE_OK) {
        rc = append_drop_table_missing_notes(database, &plan);
    }
    if (rc == MYLITE_OK) {
        rc = execute_drop_table_from_plan(database, &plan);
    }
    planned_drop_table_deinit(&plan);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_drop_temporary_table_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_drop_table plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }
    rc = plan_drop_table(database, statement, true, &plan);
    if (rc == MYLITE_OK && plan.missing_count != 0U && !drop_table_has_if_exists(statement)) {
        rc = finish_drop_table_missing_targets(database, &plan);
    }
    if (rc == MYLITE_OK) {
        rc = append_drop_table_missing_notes(database, &plan);
    }
    if (rc == MYLITE_OK) {
        rc = execute_drop_table_from_plan(database, &plan);
    }
    planned_drop_table_deinit(&plan);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_drop_view_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_drop_table plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_drop_view(database, statement, &plan);
    if (rc == MYLITE_OK && plan.missing_count != 0U && !drop_table_has_if_exists(statement)) {
        rc = finish_drop_table_missing_targets(database, &plan);
    }
    if (rc == MYLITE_OK && (plan.existing_count != 0U || plan.missing_count != 0U)) {
        rc = commit_active_user_transaction_for_ddl(database);
    }
    if (rc == MYLITE_OK) {
        rc = append_drop_table_missing_notes(database, &plan);
    }
    if (rc == MYLITE_OK) {
        rc = execute_drop_view_from_plan(database, &plan);
    }
    planned_drop_table_deinit(&plan);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int plan_drop_table(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool temporary_only,
    struct planned_drop_table *out_plan
) {
    const struct mylite_sql_ast_node *target_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *target_node = NULL;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_drop_table){.temporary_only = temporary_only};
    if (target_list == NULL || target_list->kind != MYLITE_SQL_AST_TABLE_NAME_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    out_plan->target_count = mylite_sql_ast_node_child_count(target_list);
    if (out_plan->target_count == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (out_plan->target_count > SIZE_MAX / sizeof(*out_plan->targets)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_plan->targets = calloc(out_plan->target_count, sizeof(*out_plan->targets));
    if (out_plan->targets == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    target_node = child_at(target_list, 0U);
    for (size_t target_index = 0U; rc == MYLITE_OK && target_index < out_plan->target_count;
         ++target_index) {
        rc = plan_drop_table_target(database, target_node, out_plan, target_index);
        if (target_node != NULL) {
            target_node = target_node->next_sibling;
        }
    }
    if (rc != MYLITE_OK) {
        planned_drop_table_deinit(out_plan);
    }

    return rc;
}

static int plan_drop_table_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *target_node,
    struct planned_drop_table *out_plan,
    size_t target_index
) {
    struct planned_drop_table_target *target = &out_plan->targets[target_index];
    bool missing_schema = false;
    bool found = false;
    int rc = MYLITE_OK;

    rc = resolve_writable_table_name_allow_missing_schema(
        database,
        target_node,
        &target->target,
        &missing_schema
    );
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(target->target.table_name)) {
        set_reserved_name_error(database, "table", target->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = check_drop_table_duplicate_targets(database, out_plan, target_index);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_temporary_catalog_try_read_table_by_name(
        &database->session.temporary_catalog,
        target->target.schema.name,
        target->target.table_name,
        &target->table,
        &found
    );
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to read temporary table descriptor");
        return rc;
    }
    if (found) {
        target->is_temporary = true;
        ++out_plan->existing_count;
        ++out_plan->temporary_existing_count;
        return MYLITE_OK;
    }
    if (out_plan->temporary_only || missing_schema) {
        target->missing = true;
        ++out_plan->missing_count;
        return MYLITE_OK;
    }

    rc = mylite_catalog_try_read_table_by_name(
        database,
        target->target.schema.schema_id,
        target->target.table_name,
        &target->table,
        &found
    );
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to read table descriptor");
        return rc;
    }
    if (!found) {
        target->missing = true;
        ++out_plan->missing_count;
        return MYLITE_OK;
    }
    if (target->table.kind == MYLITE_CATALOG_TABLE_KIND_VIEW) {
        target->missing = true;
        ++out_plan->missing_count;
        return MYLITE_OK;
    }

    ++out_plan->existing_count;
    ++out_plan->persistent_existing_count;
    return MYLITE_OK;
}

static int plan_drop_view(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_drop_table *out_plan
) {
    const struct mylite_sql_ast_node *target_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *target_node = NULL;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_drop_table){0};
    if (target_list == NULL || target_list->kind != MYLITE_SQL_AST_TABLE_NAME_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    out_plan->target_count = mylite_sql_ast_node_child_count(target_list);
    if (out_plan->target_count == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (out_plan->target_count > SIZE_MAX / sizeof(*out_plan->targets)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_plan->targets = calloc(out_plan->target_count, sizeof(*out_plan->targets));
    if (out_plan->targets == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    target_node = child_at(target_list, 0U);
    for (size_t target_index = 0U; rc == MYLITE_OK && target_index < out_plan->target_count;
         ++target_index) {
        struct planned_drop_table_target *target = &out_plan->targets[target_index];
        bool missing_schema = false;
        bool found = false;

        rc = resolve_writable_table_name_allow_missing_schema(
            database,
            target_node,
            &target->target,
            &missing_schema
        );
        if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(target->target.table_name)) {
            set_reserved_name_error(database, "table", target->target.table_name);
            rc = MYLITE_ERROR;
        }
        if (rc == MYLITE_OK) {
            rc = check_drop_table_duplicate_targets(database, out_plan, target_index);
        }
        if (rc == MYLITE_OK && missing_schema) {
            target->missing = true;
            ++out_plan->missing_count;
        } else if (rc == MYLITE_OK) {
            rc = mylite_catalog_try_read_table_by_name(
                database,
                target->target.schema.schema_id,
                target->target.table_name,
                &target->table,
                &found
            );
            if (rc != MYLITE_OK) {
                set_internal_error_if_clear(database, rc, "failed to read table descriptor");
            } else if (!found) {
                target->missing = true;
                ++out_plan->missing_count;
            } else if (target->table.kind != MYLITE_CATALOG_TABLE_KIND_VIEW) {
                set_not_view_error(database, target->target.schema.name, target->target.table_name);
                rc = MYLITE_ERROR;
            } else {
                ++out_plan->existing_count;
                ++out_plan->persistent_existing_count;
            }
        }
        if (target_node != NULL) {
            target_node = target_node->next_sibling;
        }
    }
    if (rc != MYLITE_OK) {
        planned_drop_table_deinit(out_plan);
    }

    return rc;
}

static int execute_drop_view_from_plan(
    struct mylite_db *database,
    const struct planned_drop_table *plan
) {
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    int rc = MYLITE_OK;

    if (plan->existing_count == 0U) {
        return MYLITE_OK;
    }
    rc = mylite_catalog_begin_mutation(database, &mutation);
    for (size_t target_index = 0U; rc == MYLITE_OK && target_index < plan->target_count;
         ++target_index) {
        const struct planned_drop_table_target *target = &plan->targets[target_index];

        if (!target->missing) {
            rc = mylite_catalog_delete_table_in_mutation(
                database,
                &mutation,
                target->table.table_id
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        rc = mylite_catalog_rollback_mutation(database, &mutation, rc);
        set_internal_error_if_clear(database, rc, "failed to drop view descriptor");
    }
    return rc;
}

static int check_drop_table_duplicate_targets(
    struct mylite_db *database,
    const struct planned_drop_table *plan,
    size_t target_index
) {
    const struct planned_drop_table_target *target = &plan->targets[target_index];

    for (size_t previous_index = 0U; previous_index < target_index; ++previous_index) {
        if (drop_table_targets_match(&plan->targets[previous_index], target)) {
            set_duplicate_table_alias_error(database, target->target.table_name);
            return MYLITE_ERROR;
        }
    }

    return MYLITE_OK;
}

static bool drop_table_targets_match(
    const struct planned_drop_table_target *left,
    const struct planned_drop_table_target *right
) {
    if (strcmp(left->target.schema.name, right->target.schema.name) != 0) {
        return false;
    }
    return strcmp(left->target.table_name, right->target.table_name) == 0;
}

static int finish_drop_table_missing_targets(
    struct mylite_db *database,
    const struct planned_drop_table *plan
) {
    if (plan->missing_count == 1U) {
        for (size_t target_index = 0U; target_index < plan->target_count; ++target_index) {
            const struct planned_drop_table_target *target = &plan->targets[target_index];

            if (target->missing) {
                set_unknown_table_error(
                    database,
                    target->target.schema.name,
                    target->target.table_name
                );
                return MYLITE_ERROR;
            }
        }
    }

    return set_unknown_drop_tables_error(database, plan);
}

static bool drop_table_plan_requires_implicit_commit(const struct planned_drop_table *plan) {
    if (plan == NULL) {
        return false;
    }
    if (plan->persistent_existing_count != 0U) {
        return true;
    }
    if (plan->missing_count != 0U) {
        return true;
    }
    return false;
}

static int append_drop_table_missing_notes(
    struct mylite_db *database,
    const struct planned_drop_table *plan
) {
    int rc = MYLITE_OK;

    for (size_t target_index = 0U; rc == MYLITE_OK && target_index < plan->target_count;
         ++target_index) {
        const struct planned_drop_table_target *target = &plan->targets[target_index];

        if (target->missing) {
            rc = append_unknown_table_note(
                database,
                target->target.schema.name,
                target->target.table_name
            );
        }
    }

    return rc;
}

static void planned_drop_table_deinit(struct planned_drop_table *plan) {
    if (plan == NULL) {
        return;
    }

    free(plan->targets);
    *plan = (struct planned_drop_table){0};
}

static int execute_drop_table_from_plan(
    struct mylite_db *database,
    const struct planned_drop_table *plan
) {
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    bool has_persistent_targets = planned_drop_table_has_persistent_targets(plan);
    int rc = MYLITE_OK;

    if (plan->existing_count == 0U) {
        return MYLITE_OK;
    }
    for (size_t target_index = 0U; rc == MYLITE_OK && target_index < plan->target_count;
         ++target_index) {
        const struct planned_drop_table_target *target = &plan->targets[target_index];

        if (!target->missing && !target->is_temporary) {
            rc = reject_table_referenced_by_foreign_key(database, target->table.table_id);
        }
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (has_persistent_targets) {
        rc = commit_active_user_transaction_for_ddl(database);
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    if (has_persistent_targets) {
        rc = mylite_catalog_begin_mutation(database, &mutation);
    }
    if (rc == MYLITE_OK && has_persistent_targets) {
        rc = delete_drop_table_persistent_catalog_rows(database, plan, &mutation);
    }
    if (rc == MYLITE_OK) {
        rc = drop_existing_physical_tables(database, plan);
    }
    if (rc == MYLITE_OK && has_persistent_targets) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        if (has_persistent_targets) {
            rc = mylite_catalog_rollback_mutation(database, &mutation, rc);
        }
        return rc;
    }
    rc = remove_drop_table_temporary_descriptors(database, plan);
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to remove temporary table descriptor");
        return rc;
    }
    ++database->session.sqlite_schema_generation;

    return MYLITE_OK;
}

static bool planned_drop_table_has_persistent_targets(const struct planned_drop_table *plan) {
    if (plan == NULL) {
        return false;
    }
    if (plan->persistent_existing_count != 0U) {
        return true;
    }
    return false;
}

static int delete_drop_table_persistent_catalog_rows(
    struct mylite_db *database,
    const struct planned_drop_table *plan,
    struct mylite_catalog_mutation *mutation
) {
    int rc = MYLITE_OK;

    for (size_t target_index = 0U; rc == MYLITE_OK && target_index < plan->target_count;
         ++target_index) {
        const struct planned_drop_table_target *target = &plan->targets[target_index];

        if (!target->missing && !target->is_temporary) {
            rc =
                mylite_catalog_delete_table_in_mutation(database, mutation, target->table.table_id);
        }
    }

    return rc;
}

static int drop_existing_physical_tables(
    struct mylite_db *database,
    const struct planned_drop_table *plan
) {
    int rc = MYLITE_OK;

    for (size_t target_index = 0U; rc == MYLITE_OK && target_index < plan->target_count;
         ++target_index) {
        const struct planned_drop_table_target *target = &plan->targets[target_index];

        if (!target->missing) {
            rc = execute_physical_drop_table(database, target->table.physical_name);
        }
    }

    return rc;
}

static int remove_drop_table_temporary_descriptors(
    struct mylite_db *database,
    const struct planned_drop_table *plan
) {
    int rc = MYLITE_OK;

    for (size_t target_index = 0U; rc == MYLITE_OK && target_index < plan->target_count;
         ++target_index) {
        const struct planned_drop_table_target *target = &plan->targets[target_index];

        if (!target->missing && target->is_temporary) {
            rc = mylite_temporary_catalog_remove_table_by_id(
                &database->session.temporary_catalog,
                target->table.table_id
            );
        }
    }

    return rc;
}

static int maybe_finish_create_if_not_exists_noop(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct planned_create_table *plan,
    bool *out_finished
) {
    struct mylite_catalog_table_descriptor table = {0};
    bool found = false;
    int rc = MYLITE_OK;

    *out_finished = false;
    if (!create_table_has_if_not_exists(statement)) {
        return MYLITE_OK;
    }

    rc = mylite_catalog_try_read_table_by_name(
        database,
        plan->target.schema.schema_id,
        plan->target.table_name,
        &table,
        &found
    );
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to read table descriptor");
        return rc;
    }
    if (!found) {
        return MYLITE_OK;
    }

    rc = append_table_exists_note(database, plan->target.table_name);
    if (rc == MYLITE_OK) {
        *out_finished = true;
    }
    return rc;
}

static int maybe_finish_create_temporary_if_not_exists_noop(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct planned_create_table *plan,
    bool *out_finished
) {
    struct mylite_catalog_table_descriptor table = {0};
    bool found = false;
    int rc = MYLITE_OK;

    *out_finished = false;
    if (!create_table_has_if_not_exists(statement)) {
        return MYLITE_OK;
    }

    rc = mylite_temporary_catalog_try_read_table_by_name(
        &database->session.temporary_catalog,
        plan->target.schema.name,
        plan->target.table_name,
        &table,
        &found
    );
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to read temporary table descriptor");
        return rc;
    }
    if (!found) {
        return MYLITE_OK;
    }

    rc = append_table_exists_note(database, plan->target.table_name);
    if (rc == MYLITE_OK) {
        *out_finished = true;
    }
    return rc;
}

static bool create_table_has_if_not_exists(const struct mylite_sql_ast_node *statement) {
    return child_with_kind(statement, MYLITE_SQL_AST_CREATE_IF_NOT_EXISTS_CLAUSE) != NULL;
}

static const struct mylite_sql_ast_node *create_table_options_node(
    const struct mylite_sql_ast_node *statement
) {
    return child_with_kind(statement, MYLITE_SQL_AST_TABLE_OPTION_LIST);
}

static bool drop_table_has_if_exists(const struct mylite_sql_ast_node *statement) {
    return child_with_kind(statement, MYLITE_SQL_AST_DROP_IF_EXISTS_CLAUSE) != NULL;
}

static int execute_drop_schema_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_drop_schema plan = {0};
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    mylite_result *result = NULL;
    bool finished = false;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = maybe_finish_drop_schema_if_exists_noop(database, statement, &finished);
    if (rc == MYLITE_OK && finished) {
        return finish_successful_result_with_warning_count(result, 1U, out_result);
    }
    if (rc == MYLITE_OK) {
        rc = plan_drop_schema(database, statement, &mutation, &plan);
    }
    if (rc == MYLITE_OK) {
        rc = drop_schema_from_plan(database, &mutation, &plan, result);
    }
    if (rc != MYLITE_OK) {
        rc = mylite_catalog_rollback_mutation(database, &mutation, rc);
    }
    planned_drop_schema_deinit(&plan);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int maybe_finish_drop_schema_if_exists_noop(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool *out_finished
) {
    char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    struct mylite_catalog_schema_descriptor schema = {0};
    bool found = false;
    int rc = MYLITE_OK;

    *out_finished = false;
    if (!drop_schema_has_if_exists(statement)) {
        return MYLITE_OK;
    }

    rc = copy_identifier_text(child_at(statement, 0U), schema_name, sizeof(schema_name), database);
    if (rc == MYLITE_OK) {
        rc = reject_builtin_schema_schema_write_name(database, schema_name);
    }
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(schema_name)) {
        set_reserved_name_error(database, "database", schema_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_try_read_schema_by_name(database, schema_name, &schema, &found);
        if (rc != MYLITE_OK) {
            set_internal_error_if_clear(database, rc, "failed to read schema descriptor");
        }
    }
    if (rc != MYLITE_OK || found) {
        return rc;
    }

    *out_finished = true;
    return MYLITE_OK;
}

static bool drop_schema_has_if_exists(const struct mylite_sql_ast_node *statement) {
    return child_with_kind(statement, MYLITE_SQL_AST_DROP_SCHEMA_IF_EXISTS_CLAUSE) != NULL;
}
