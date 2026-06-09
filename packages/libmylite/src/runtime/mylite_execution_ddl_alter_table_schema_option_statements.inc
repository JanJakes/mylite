static int execute_alter_schema_default_charset_collation_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_schema_default_charset_collation plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_schema_default_charset_collation(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = alter_schema_default_charset_collation_from_plan(database, &plan);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 1);
    return finish_successful_result(database, result, out_result);
}

static int execute_alter_table_default_charset_collation_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_default_charset_collation plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_default_charset_collation(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = validate_sql_require_primary_key_for_table_descriptor(database, &plan.table);
    }
    if (rc == MYLITE_OK) {
        rc = alter_table_default_charset_collation_from_plan(database, &plan);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int alter_table_default_charset_collation_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_default_charset_collation *plan
) {
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    int rc = MYLITE_OK;

    if (!plan->changes_descriptor) {
        return MYLITE_OK;
    }

    rc = mylite_catalog_begin_mutation(database, &mutation);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_update_table_default_charset_collation_in_mutation(
            database,
            &mutation,
            plan->table.table_id,
            plan->default_charset,
            plan->default_collation,
            NULL
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to update table charset/collation");
        mylite_catalog_rollback_mutation(database, &mutation);
        return rc;
    }

    return MYLITE_OK;
}

static int execute_alter_table_convert_character_set_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_convert_character_set plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_convert_character_set(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = validate_sql_require_primary_key_for_table_descriptor(database, &plan.table);
    }
    if (rc == MYLITE_OK) {
        rc = alter_table_convert_character_set_from_plan(database, &plan);
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_convert_character_set_deinit(&plan);
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    planned_alter_table_convert_character_set_deinit(&plan);
    return finish_successful_result(database, result, out_result);
}

static int alter_table_convert_character_set_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_convert_character_set *plan
) {
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    int rc = MYLITE_OK;

    if (!plan->changes_descriptor) {
        return MYLITE_OK;
    }

    rc = mylite_catalog_begin_mutation(database, &mutation);
    if (rc == MYLITE_OK && (strcmp(plan->default_charset, plan->table.default_charset) != 0 ||
                            strcmp(plan->default_collation, plan->table.default_collation) != 0)) {
        rc = mylite_catalog_update_table_default_charset_collation_in_mutation(
            database,
            &mutation,
            plan->table.table_id,
            plan->default_charset,
            plan->default_collation,
            NULL
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < plan->column_count; ++index) {
        const struct mylite_catalog_column_descriptor *column = &plan->columns[index];

        if (!alter_table_convert_column_needs_explicit_metadata_update(column, plan)) {
            continue;
        }
        rc = mylite_catalog_replace_column_in_mutation(
            database,
            &mutation,
            plan->table.table_id,
            column->column_id,
            column->name,
            column->logical_type,
            column->physical_type,
            column->is_nullable,
            column->is_visible,
            column->is_auto_increment,
            column->default_kind,
            column->default_integer,
            column->default_text,
            column->on_update_current_timestamp,
            plan->default_charset,
            plan->default_collation,
            column->comment,
            column->is_generated,
            column->generated_kind,
            column->generation_expression,
            column->sqlite_generation_expression
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to convert table character set metadata");
        mylite_catalog_rollback_mutation(database, &mutation);
        return rc;
    }

    return MYLITE_OK;
}

static int execute_alter_table_comment_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_comment plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_comment(database, statement, &plan);
    if (rc == MYLITE_OK && plan.table.kind == MYLITE_CATALOG_TABLE_KIND_TEMPORARY &&
        database->session.user_transaction_active) {
        set_unsupported_error(
            database,
            "Temporary table DDL inside an active transaction is not supported"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = validate_sql_require_primary_key_for_table_descriptor(database, &plan.table);
    }
    if (rc == MYLITE_OK) {
        rc = alter_table_comment_from_plan(database, &plan);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int alter_table_comment_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_comment *plan
) {
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    int rc = MYLITE_OK;

    if (plan->table.kind == MYLITE_CATALOG_TABLE_KIND_TEMPORARY) {
        rc = mylite_temporary_catalog_update_table_comment(
            &database->session.temporary_catalog,
            plan->table.table_id,
            plan->comment
        );
        if (rc != MYLITE_OK) {
            set_internal_error_if_clear(database, rc, "failed to update temporary table comment");
        }
        return rc;
    }

    rc = mylite_catalog_begin_mutation(database, &mutation);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_update_table_comment_in_mutation(
            database,
            &mutation,
            plan->table.table_id,
            plan->comment,
            NULL
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to update table comment");
        mylite_catalog_rollback_mutation(database, &mutation);
        return rc;
    }

    return MYLITE_OK;
}

static int execute_alter_table_storage_statistics_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_storage_statistics plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_storage_statistics(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = validate_sql_require_primary_key_for_table_descriptor(database, &plan.table);
    }
    if (rc == MYLITE_OK) {
        rc = alter_table_storage_statistics_from_plan(database, &plan);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int alter_table_storage_statistics_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_storage_statistics *plan
) {
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    int rc = MYLITE_OK;

    if (!plan->changes_descriptor) {
        return MYLITE_OK;
    }

    rc = mylite_catalog_begin_mutation(database, &mutation);
    if (rc == MYLITE_OK) {
        rc = alter_table_storage_statistics_in_mutation(database, &mutation, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to update table storage options");
        mylite_catalog_rollback_mutation(database, &mutation);
        return rc;
    }

    return MYLITE_OK;
}

static int alter_table_storage_statistics_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    const struct planned_alter_table_storage_statistics *plan
) {
    if (plan == NULL) {
        return MYLITE_MISUSE;
    }
    if (!plan->changes_descriptor) {
        return MYLITE_OK;
    }
    return mylite_catalog_update_table_storage_statistics_in_mutation(
        database,
        mutation,
        &plan->table,
        NULL
    );
}

static int plan_alter_schema_default_charset_collation(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_schema_default_charset_collation *out_plan
) {
    const struct mylite_sql_ast_node *first_child = child_at(statement, 0U);
    const struct mylite_sql_ast_node *schema_options =
        child_with_kind(statement, MYLITE_SQL_AST_TABLE_OPTION_LIST);
    const struct mylite_sql_ast_node *schema_node =
        first_child == schema_options ? NULL : first_child;
    char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    bool found = false;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_schema_default_charset_collation){0};
    if (schema_node == NULL) {
        if (database == NULL || !database->session.has_selected_schema) {
            set_no_database_error(database);
            return MYLITE_ERROR;
        }
        snprintf(schema_name, sizeof(schema_name), "%s", database->session.selected_schema);
    } else {
        rc = copy_identifier_text(schema_node, schema_name, sizeof(schema_name), database);
    }
    if (rc == MYLITE_OK) {
        rc = reject_builtin_schema_schema_write_name(database, schema_name);
    }
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(schema_name)) {
        set_reserved_name_error(database, "database", schema_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_try_read_schema_by_name(
            database,
            schema_name,
            &out_plan->schema,
            &found
        );
        if (rc != MYLITE_OK) {
            set_internal_error_if_clear(database, rc, "failed to read schema descriptor");
        } else if (!found) {
            set_database_does_not_exist_error(database, schema_name);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        memcpy(
            out_plan->default_charset,
            out_plan->schema.default_charset,
            strlen(out_plan->schema.default_charset) + 1U
        );
        memcpy(
            out_plan->default_collation,
            out_plan->schema.default_collation,
            strlen(out_plan->schema.default_collation) + 1U
        );
        rc = apply_schema_default_charset_collation_options(
            database,
            schema_options,
            out_plan->default_charset,
            sizeof(out_plan->default_charset),
            out_plan->default_collation,
            sizeof(out_plan->default_collation)
        );
    }
    if (rc == MYLITE_OK) {
        out_plan->changes_descriptor = false;
        if (strcmp(out_plan->default_charset, out_plan->schema.default_charset) != 0) {
            out_plan->changes_descriptor = true;
        }
        if (strcmp(out_plan->default_collation, out_plan->schema.default_collation) != 0) {
            out_plan->changes_descriptor = true;
        }
    }

    return rc;
}

static int alter_schema_default_charset_collation_from_plan(
    struct mylite_db *database,
    const struct planned_alter_schema_default_charset_collation *plan
) {
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    int rc = MYLITE_OK;

    if (!plan->changes_descriptor) {
        return MYLITE_OK;
    }

    rc = mylite_catalog_begin_mutation(database, &mutation);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_update_schema_default_charset_collation_in_mutation(
            database,
            &mutation,
            plan->schema.schema_id,
            plan->default_charset,
            plan->default_collation,
            NULL
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to update schema charset/collation");
        mylite_catalog_rollback_mutation(database, &mutation);
        return rc;
    }

    return MYLITE_OK;
}
