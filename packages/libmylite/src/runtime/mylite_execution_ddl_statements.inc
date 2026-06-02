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
    if (database->session.user_transaction_active) {
        set_unsupported_error(
            database,
            "Temporary table DDL inside an active transaction is not supported"
        );
        mylite_result_free(result);
        return MYLITE_ERROR;
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
    if (database->session.user_transaction_active) {
        set_unsupported_error(
            database,
            "Temporary table DDL inside an active transaction is not supported"
        );
        mylite_result_free(result);
        return MYLITE_ERROR;
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
    if (database->session.user_transaction_active) {
        set_unsupported_error(
            database,
            "Temporary table DDL inside an active transaction is not supported"
        );
        mylite_result_free(result);
        return MYLITE_ERROR;
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

static int execute_create_view_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_create_view plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_create_view(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = create_view_from_plan(database, &plan);
    }
    planned_create_view_deinit(&plan);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int plan_create_view(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_create_view *out_plan
) {
    const struct mylite_sql_ast_node *select_statement = child_at(statement, 1U);
    int rc = MYLITE_OK;

    *out_plan = (struct planned_create_view){0};
    rc = resolve_writable_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = validate_create_view_select_subset(database, select_statement);
    }
    if (rc == MYLITE_OK) {
        rc = plan_select(database, select_statement, false, &out_plan->source);
    }
    if (rc == MYLITE_OK && out_plan->source.table.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(database, "CREATE VIEW supports only persistent base-table sources");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = copy_create_view_column_names(database, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = build_create_view_definition_sql(database, out_plan, &out_plan->view_definition);
    }
    if (rc == MYLITE_OK) {
        rc = build_create_view_show_create_sql(database, out_plan, &out_plan->show_create_sql);
    }
    if (rc != MYLITE_OK) {
        planned_create_view_deinit(out_plan);
    }

    return rc;
}

static int create_view_from_plan(
    struct mylite_db *database,
    const struct planned_create_view *plan
) {
    struct mylite_catalog_table_descriptor existing_table = {0};
    struct mylite_catalog_table_descriptor view_table = {0};
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    char physical_name[MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY];
    bool existing_table_found = false;
    int64_t table_id = 0;
    int64_t created_time_utc_epoch = current_timestamp_epoch(database);
    int rc = mylite_catalog_try_read_table_by_name(
        database,
        plan->target.schema.schema_id,
        plan->target.table_name,
        &existing_table,
        &existing_table_found
    );

    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to read table descriptor");
        return rc;
    }
    if (existing_table_found) {
        set_table_exists_error(database, plan->target.table_name);
        return MYLITE_ERROR;
    }

    rc = mylite_catalog_begin_mutation(database, &mutation);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_allocate_table_id_in_mutation(database, &mutation, &table_id);
    }
    if (rc == MYLITE_OK) {
        rc = build_physical_view_name(table_id, physical_name, sizeof(physical_name));
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_insert_table_in_mutation(
            database,
            &mutation,
            table_id,
            plan->target.schema.schema_id,
            plan->target.table_name,
            physical_name,
            MYLITE_CATALOG_TABLE_KIND_VIEW,
            1,
            plan->target.schema.default_charset,
            plan->target.schema.default_collation,
            "",
            "",
            0,
            0,
            0,
            0,
            0,
            0,
            created_time_utc_epoch,
            created_time_utc_epoch,
            &view_table
        );
    }
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        const struct mylite_catalog_column_descriptor *source_column =
            &plan->source.columns[column_index];

        rc = mylite_catalog_insert_column_in_mutation(
            database,
            &mutation,
            table_id,
            (int64_t)column_index + 1,
            plan->column_names[column_index],
            source_column->logical_type,
            source_column->physical_type,
            source_column->is_nullable,
            source_column->is_visible,
            false,
            MYLITE_CATALOG_COLUMN_DEFAULT_NONE,
            0,
            NULL,
            false,
            source_column->character_set_name,
            source_column->collation_name,
            "",
            false,
            MYLITE_CATALOG_GENERATED_COLUMN_INVALID,
            "",
            "",
            NULL
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_insert_view_in_mutation(
            database,
            &mutation,
            table_id,
            plan->view_definition,
            plan->show_create_sql,
            "NONE",
            "NO",
            "root@%",
            "DEFINER",
            database->session.character_set_client,
            database->session.collation_connection,
            plan->source.source.schema.schema_id,
            plan->source.table.table_id,
            plan->source.source.schema.name,
            plan->source.source.table_name,
            NULL
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        mylite_catalog_rollback_mutation(database, &mutation);
        set_internal_error_if_clear(database, rc, "failed to create view descriptor");
        return rc;
    }

    (void)view_table;
    return MYLITE_OK;
}

static void planned_create_view_deinit(struct planned_create_view *plan) {
    if (plan == NULL) {
        return;
    }

    for (size_t column_index = 0U; column_index < plan->column_count; ++column_index) {
        free(plan->column_names[column_index]);
    }
    free((void *)plan->column_names);
    free(plan->view_definition);
    free(plan->show_create_sql);
    planned_select_deinit(&plan->source);
    *plan = (struct planned_create_view){0};
}

static int validate_create_view_select_subset(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_statement
) {
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *select_item = NULL;
    const struct mylite_sql_ast_node *from_clause = NULL;
    const struct mylite_sql_ast_node *optional_clause = NULL;

    if (select_statement == NULL || select_statement->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
        set_unsupported_error(database, "CREATE VIEW supports only a single SELECT statement");
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_select_modifier(select_statement) !=
            MYLITE_SQL_AST_SELECT_MODIFIER_DEFAULT ||
        mylite_sql_ast_node_select_calc_found_rows(select_statement) != 0) {
        set_unsupported_error(database, "CREATE VIEW supports only direct non-distinct SELECT");
        return MYLITE_ERROR;
    }

    select_list = child_at(select_statement, 0U);
    if (select_list == NULL) {
        set_unsupported_error(database, "CREATE VIEW requires a direct projection list");
        return MYLITE_ERROR;
    }
    select_item = child_at(select_list, 0U);
    while (select_item != NULL) {
        const struct mylite_sql_ast_node *expression = child_at(select_item, 0U);
        const struct mylite_sql_ast_node *alias = child_at(select_item, 1U);

        if (select_item->kind != MYLITE_SQL_AST_SELECT_ITEM || expression == NULL) {
            set_unsupported_error(database, "CREATE VIEW requires direct projection items");
            return MYLITE_ERROR;
        }
        if (expression->kind == MYLITE_SQL_AST_WILDCARD ||
            expression->kind == MYLITE_SQL_AST_QUALIFIED_WILDCARD) {
            if (alias != NULL) {
                set_unsupported_error(database, "CREATE VIEW wildcard items cannot have aliases");
                return MYLITE_ERROR;
            }
        } else if (
            expression->kind != MYLITE_SQL_AST_IDENTIFIER &&
            expression->kind != MYLITE_SQL_AST_QUALIFIED_IDENTIFIER
        ) {
            set_unsupported_error(database, "CREATE VIEW supports only direct column projection");
            return MYLITE_ERROR;
        }
        select_item = select_item->next_sibling;
    }

    from_clause = child_at(select_statement, 1U);
    if (from_clause == NULL || from_clause->kind != MYLITE_SQL_AST_FROM_TABLE) {
        set_unsupported_error(database, "CREATE VIEW supports only one base-table source");
        return MYLITE_ERROR;
    }
    if (from_table_index_hint_list_node(from_clause) != NULL) {
        set_unsupported_error(database, "CREATE VIEW does not support index hints");
        return MYLITE_ERROR;
    }

    optional_clause = child_at(select_statement, 2U);
    if (optional_clause != NULL) {
        set_unsupported_error(database, "CREATE VIEW supports only direct projection SELECT");
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int copy_create_view_column_names(
    struct mylite_db *database,
    struct planned_create_view *plan
) {
    if (plan->source.column_count > SIZE_MAX / sizeof(*plan->column_names)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    plan->column_names = (char **)calloc(plan->source.column_count, sizeof(*plan->column_names));
    if (plan->column_names == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    plan->column_count = plan->source.column_count;

    for (size_t column_index = 0U; column_index < plan->column_count; ++column_index) {
        const struct mylite_sql_ast_node *alias = plan->source.column_aliases[column_index];
        int rc = MYLITE_OK;

        if (alias != NULL) {
            rc = copy_select_item_alias_text(database, alias, &plan->column_names[column_index]);
        } else {
            rc = duplicate_text(
                database,
                plan->source.columns[column_index].name,
                &plan->column_names[column_index]
            );
        }
        if (rc != MYLITE_OK) {
            return rc;
        }
        for (size_t prior_index = 0U; prior_index < column_index; ++prior_index) {
            if (text_equals_ascii_case_insensitive(
                    plan->column_names[prior_index],
                    plan->column_names[column_index]
                )) {
                set_duplicate_column_error(database, plan->column_names[column_index]);
                return MYLITE_ERROR;
            }
        }
    }

    return MYLITE_OK;
}

static int build_create_view_definition_sql(
    struct mylite_db *database,
    const struct planned_create_view *plan,
    char **out_sql
) {
    return build_create_view_select_sql(database, plan, true, out_sql);
}

static int build_create_view_select_sql(
    struct mylite_db *database,
    const struct planned_create_view *plan,
    bool qualify_source_schema,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "select ");
    if (rc == MYLITE_OK) {
        rc = append_create_view_projection_sql(&string, plan, qualify_source_schema);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " from ");
    }
    if (rc == MYLITE_OK) {
        rc = append_create_view_from_sql(&string, plan, qualify_source_schema);
    }
    if (rc == MYLITE_OK) {
        *out_sql = mylite_dynamic_string_take(&string);
        return MYLITE_OK;
    }

    mylite_dynamic_string_deinit(&string);
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    return rc;
}

static int build_create_view_show_create_sql(
    struct mylite_db *database,
    const struct planned_create_view *plan,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    char *select_sql = NULL;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = build_create_view_select_sql(database, plan, false, &select_sql);
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(
            &string,
            "CREATE ALGORITHM=UNDEFINED DEFINER=`root`@`%` SQL SECURITY DEFINER VIEW "
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_mysql_quoted_identifier(&string, plan->target.table_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " AS ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, select_sql);
    }
    if (rc == MYLITE_OK) {
        *out_sql = mylite_dynamic_string_take(&string);
        free(select_sql);
        return MYLITE_OK;
    }

    mylite_dynamic_string_deinit(&string);
    free(select_sql);
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    return rc;
}

static int append_create_view_projection_sql(
    struct mylite_dynamic_string *string,
    const struct planned_create_view *plan,
    bool qualify_source_schema
) {
    const char *source_qualifier = plan->source.source.table_name;
    int rc = MYLITE_OK;

    if (plan->source.source_has_alias) {
        source_qualifier = plan->source.source_alias;
    }

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        if (column_index > 0U) {
            rc = mylite_dynamic_string_append_char(string, ',');
        }
        if (rc == MYLITE_OK && qualify_source_schema && !plan->source.source_has_alias) {
            rc = mylite_dynamic_string_append_mysql_quoted_identifier(
                string,
                plan->source.source.schema.name
            );
            if (rc == MYLITE_OK) {
                rc = mylite_dynamic_string_append_char(string, '.');
            }
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_mysql_quoted_identifier(string, source_qualifier);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(string, '.');
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_mysql_quoted_identifier(
                string,
                plan->source.columns[column_index].name
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " AS ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_mysql_quoted_identifier(
                string,
                plan->column_names[column_index]
            );
        }
    }
    return rc;
}

static int append_create_view_from_sql(
    struct mylite_dynamic_string *string,
    const struct planned_create_view *plan,
    bool qualify_source_schema
) {
    int rc = MYLITE_OK;

    if (qualify_source_schema) {
        rc = mylite_dynamic_string_append_mysql_quoted_identifier(
            string,
            plan->source.source.schema.name
        );
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(string, '.');
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_mysql_quoted_identifier(
            string,
            plan->source.source.table_name
        );
    }

    if (rc == MYLITE_OK && plan->source.source_has_alias) {
        rc = mylite_dynamic_string_append_char(string, ' ');
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_mysql_quoted_identifier(
                string,
                plan->source.source_alias
            );
        }
    }
    return rc;
}

static int execute_create_index_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_add_index plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_create_index(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = validate_sql_require_primary_key_for_table_descriptor(database, &plan.table);
    }
    if (rc == MYLITE_OK) {
        rc = add_secondary_index_from_plan(database, &plan);
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_add_index_deinit(&plan);
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, plan.affected_rows);
    planned_alter_table_add_index_deinit(&plan);
    return finish_successful_result(database, result, out_result);
}

static int execute_create_schema_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    mylite_result *result = NULL;
    bool finished = false;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = validate_schema_default_charset_collation_options(
        database,
        child_with_kind(statement, MYLITE_SQL_AST_TABLE_OPTION_LIST)
    );
    if (rc == MYLITE_OK) {
        rc = maybe_finish_create_schema_if_not_exists_noop(database, statement, result, &finished);
    }
    if (rc == MYLITE_OK && finished) {
        return finish_successful_result(database, result, out_result);
    }
    if (rc == MYLITE_OK) {
        rc = create_schema_from_statement(database, statement, result);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int maybe_finish_create_schema_if_not_exists_noop(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result *result,
    bool *out_finished
) {
    char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    struct mylite_catalog_schema_descriptor schema = {0};
    bool found = false;
    int rc = MYLITE_OK;

    *out_finished = false;
    if (!create_schema_has_if_not_exists(statement)) {
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
    if (rc != MYLITE_OK || !found) {
        return rc;
    }

    rc = append_database_exists_note(database, schema_name);
    if (rc == MYLITE_OK) {
        mylite_result_set_affected_rows(result, 1);
        *out_finished = true;
    }
    return rc;
}

static bool create_schema_has_if_not_exists(const struct mylite_sql_ast_node *statement) {
    return child_with_kind(statement, MYLITE_SQL_AST_CREATE_SCHEMA_IF_NOT_EXISTS_CLAUSE) != NULL;
}

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
    if (rc == MYLITE_OK && plan.temporary_existing_count != 0U &&
        database->session.user_transaction_active) {
        set_unsupported_error(
            database,
            "Temporary table DDL inside an active transaction is not supported"
        );
        rc = MYLITE_ERROR;
    }
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
    if (database->session.user_transaction_active) {
        set_unsupported_error(
            database,
            "Temporary table DDL inside an active transaction is not supported"
        );
        mylite_result_free(result);
        return MYLITE_ERROR;
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
        mylite_catalog_rollback_mutation(database, &mutation);
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
    bool has_temporary_targets = planned_drop_table_has_temporary_targets(plan);
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
    if (has_temporary_targets && database->session.user_transaction_active) {
        set_unsupported_error(
            database,
            "Temporary table DDL inside an active transaction is not supported"
        );
        return MYLITE_ERROR;
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
            mylite_catalog_rollback_mutation(database, &mutation);
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

static bool planned_drop_table_has_temporary_targets(const struct planned_drop_table *plan) {
    if (plan == NULL) {
        return false;
    }
    if (plan->temporary_existing_count != 0U) {
        return true;
    }
    return false;
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
        mylite_catalog_rollback_mutation(database, &mutation);
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
    if (rc == MYLITE_OK) {
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
    if (action_count < 2U) {
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
    rc = plan_alter_table_add_column(database, &view.statement, &plan);
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

static int execute_alter_table_add_primary_key_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_add_primary_key plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_add_primary_key(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = alter_table_add_primary_key_from_plan(database, &plan);
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_add_primary_key_deinit(&plan);
        mylite_result_free(result);
        return rc;
    }

    planned_alter_table_add_primary_key_deinit(&plan);
    return finish_successful_result(database, result, out_result);
}

static int execute_alter_table_add_index_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_add_index plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_add_index(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = validate_sql_require_primary_key_for_table_descriptor(database, &plan.table);
    }
    if (rc == MYLITE_OK) {
        rc = add_secondary_index_from_plan(database, &plan);
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_add_index_deinit(&plan);
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, plan.affected_rows);
    planned_alter_table_add_index_deinit(&plan);
    return finish_successful_result(database, result, out_result);
}

static int execute_alter_table_add_foreign_key_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_add_foreign_key plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_add_foreign_key(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = validate_sql_require_primary_key_for_table_descriptor(database, &plan.table);
    }
    if (rc == MYLITE_OK) {
        rc = alter_table_add_foreign_key_from_plan(database, &plan);
    }
    planned_alter_table_add_foreign_key_deinit(&plan);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_alter_table_drop_foreign_key_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_drop_foreign_key plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_drop_foreign_key(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = validate_sql_require_primary_key_for_table_descriptor(database, &plan.table);
    }
    if (rc == MYLITE_OK) {
        rc = alter_table_drop_foreign_key_from_plan(database, &plan);
    }
    planned_alter_table_drop_foreign_key_deinit(&plan);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int execute_alter_table_drop_constraint_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_drop_constraint plan = {0};
    mylite_result *result = NULL;
    int64_t affected_rows = 0;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_drop_constraint(database, statement, &plan);
    if (rc == MYLITE_OK) {
        switch (plan.kind) {
        case PLANNED_ALTER_TABLE_DROP_CONSTRAINT_PRIMARY_KEY:
            if (sql_require_primary_key_session_enabled(database)) {
                set_sql_require_primary_key_error(database);
                rc = MYLITE_ERROR;
            } else {
                rc = alter_table_drop_primary_key_from_plan(database, &plan.primary_key);
                affected_rows = plan.primary_key.affected_rows;
            }
            break;
        case PLANNED_ALTER_TABLE_DROP_CONSTRAINT_UNIQUE_INDEX:
            rc = validate_sql_require_primary_key_for_table_descriptor(
                database,
                &plan.unique_index.table
            );
            if (rc == MYLITE_OK) {
                rc = drop_index_from_plan(database, &plan.unique_index);
            }
            affected_rows = plan.unique_index.affected_rows;
            break;
        case PLANNED_ALTER_TABLE_DROP_CONSTRAINT_FOREIGN_KEY:
            rc = validate_sql_require_primary_key_for_table_descriptor(
                database,
                &plan.foreign_key.table
            );
            if (rc == MYLITE_OK) {
                rc = alter_table_drop_foreign_key_from_plan(database, &plan.foreign_key);
            }
            break;
        case PLANNED_ALTER_TABLE_DROP_CONSTRAINT_CHECK:
            rc = validate_sql_require_primary_key_for_table_descriptor(
                database,
                &plan.check_constraint.table
            );
            if (rc == MYLITE_OK) {
                rc = alter_table_check_constraint_from_plan(database, &plan.check_constraint);
            }
            affected_rows = plan.check_constraint.affected_rows;
            break;
        case PLANNED_ALTER_TABLE_DROP_CONSTRAINT_NONE:
            set_runtime_error(database, "invalid DROP CONSTRAINT plan");
            rc = MYLITE_ERROR;
            break;
        }
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_drop_constraint_deinit(&plan);
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, affected_rows);
    planned_alter_table_drop_constraint_deinit(&plan);
    return finish_successful_result(database, result, out_result);
}

static int execute_alter_table_drop_index_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_drop_index plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_drop_index(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = validate_sql_require_primary_key_for_table_descriptor(database, &plan.table);
    }
    if (rc == MYLITE_OK) {
        rc = drop_index_from_plan(database, &plan);
    }
    if (rc != MYLITE_OK) {
        planned_drop_index_deinit(&plan);
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, plan.affected_rows);
    planned_drop_index_deinit(&plan);
    return finish_successful_result(database, result, out_result);
}

static int execute_alter_table_rename_index_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_rename_index plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_rename_index(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = validate_sql_require_primary_key_for_table_descriptor(database, &plan.table);
    }
    if (rc == MYLITE_OK) {
        rc = rename_index_from_plan(database, &plan);
    }
    if (rc != MYLITE_OK) {
        planned_rename_index_deinit(&plan);
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    planned_rename_index_deinit(&plan);
    return finish_successful_result(database, result, out_result);
}

static int execute_alter_table_add_check_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_check_constraint plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_add_check(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = validate_sql_require_primary_key_for_table_descriptor(database, &plan.table);
    }
    if (rc == MYLITE_OK) {
        rc = alter_table_check_constraint_from_plan(database, &plan);
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_check_constraint_deinit(&plan);
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, plan.affected_rows);
    planned_alter_table_check_constraint_deinit(&plan);
    return finish_successful_result(database, result, out_result);
}

static int execute_alter_table_drop_check_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_check_constraint plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_drop_check(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = validate_sql_require_primary_key_for_table_descriptor(database, &plan.table);
    }
    if (rc == MYLITE_OK) {
        rc = alter_table_check_constraint_from_plan(database, &plan);
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_check_constraint_deinit(&plan);
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, plan.affected_rows);
    planned_alter_table_check_constraint_deinit(&plan);
    return finish_successful_result(database, result, out_result);
}

static int execute_alter_table_alter_check_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_check_constraint plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_alter_check(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = validate_sql_require_primary_key_for_table_descriptor(database, &plan.table);
    }
    if (rc == MYLITE_OK) {
        rc = alter_table_check_constraint_from_plan(database, &plan);
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_check_constraint_deinit(&plan);
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, plan.affected_rows);
    planned_alter_table_check_constraint_deinit(&plan);
    return finish_successful_result(database, result, out_result);
}

static int execute_drop_index_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_drop_index plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_drop_index(
        database,
        (struct drop_index_nodes){
            .table_name_node = child_at(statement, 1U),
            .index_name_node = child_at(statement, 0U),
        },
        "DROP INDEX",
        &plan
    );
    if (rc == MYLITE_OK) {
        rc = validate_sql_require_primary_key_for_table_descriptor(database, &plan.table);
    }
    if (rc == MYLITE_OK) {
        rc = drop_index_from_plan(database, &plan);
    }
    if (rc != MYLITE_OK) {
        planned_drop_index_deinit(&plan);
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, plan.affected_rows);
    planned_drop_index_deinit(&plan);
    return finish_successful_result(database, result, out_result);
}

static int execute_alter_table_drop_primary_key_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_drop_primary_key plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_drop_primary_key(database, statement, &plan);
    if (rc == MYLITE_OK && sql_require_primary_key_session_enabled(database)) {
        set_sql_require_primary_key_error(database);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = alter_table_drop_primary_key_from_plan(database, &plan);
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_drop_primary_key_deinit(&plan);
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, plan.affected_rows);
    planned_alter_table_drop_primary_key_deinit(&plan);
    return finish_successful_result(database, result, out_result);
}

static int execute_alter_table_auto_increment_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_auto_increment plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_auto_increment(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = validate_sql_require_primary_key_for_table_descriptor(database, &plan.table);
    }
    if (rc == MYLITE_OK) {
        rc = alter_table_auto_increment_from_plan(database, &plan);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

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
    if (rc == MYLITE_OK) {
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
    if (rc == MYLITE_OK) {
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

static int execute_alter_table_order_by_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_order_by plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_order_by(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = validate_sql_require_primary_key_for_table_descriptor(database, &plan.table);
    }
    if (rc == MYLITE_OK) {
        rc = alter_table_order_by_from_plan(database, &plan);
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_order_by_deinit(&plan);
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, plan.affected_rows);
    planned_alter_table_order_by_deinit(&plan);
    return finish_successful_result(database, result, out_result);
}

static int execute_alter_table_force_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_force plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_force(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = validate_sql_require_primary_key_for_table_descriptor(database, &plan.table);
    }
    if (rc == MYLITE_OK) {
        rc = alter_table_force_from_plan(database, &plan);
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_force_deinit(&plan);
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    planned_alter_table_force_deinit(&plan);
    return finish_successful_result(database, result, out_result);
}

static int execute_alter_table_disable_keys_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    return execute_alter_table_key_maintenance_statement(database, statement, out_result);
}

static int execute_alter_table_enable_keys_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    return execute_alter_table_key_maintenance_statement(database, statement, out_result);
}

static int execute_alter_table_key_maintenance_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_key_maintenance plan = {0};
    enum mylite_sql_ast_alter_algorithm algorithm = mylite_sql_ast_node_alter_algorithm(statement);
    enum mylite_sql_ast_alter_lock lock = mylite_sql_ast_node_alter_lock(statement);
    const char *note_table_name = NULL;
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_key_maintenance(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = validate_alter_table_key_maintenance_algorithm_lock_options(database, algorithm, lock);
    }
    if (rc == MYLITE_OK && algorithm == MYLITE_SQL_AST_ALTER_ALGORITHM_COPY) {
        rc = read_show_table_status_row_count(database, &plan.table, &plan.affected_rows);
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        } else if (rc != MYLITE_OK) {
            set_runtime_error(database, "failed to read key maintenance row count");
        }
    }
    if (rc == MYLITE_OK) {
        note_table_name = algorithm == MYLITE_SQL_AST_ALTER_ALGORITHM_COPY ? "#sql-mylite-copy"
                                                                           : plan.target.table_name;
        rc = append_table_storage_engine_option_note(database, note_table_name);
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, plan.affected_rows);
    return finish_successful_result(database, result, out_result);
}

static int validate_alter_table_algorithm_lock_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
) {
    enum mylite_sql_ast_alter_algorithm algorithm = MYLITE_SQL_AST_ALTER_ALGORITHM_UNSPECIFIED;
    enum mylite_sql_ast_alter_lock lock = MYLITE_SQL_AST_ALTER_LOCK_UNSPECIFIED;
    int rc = MYLITE_OK;

    if (statement == NULL) {
        return MYLITE_OK;
    }
    if (!alter_table_statement_accepts_algorithm_lock_options(statement->kind)) {
        return MYLITE_OK;
    }

    algorithm = mylite_sql_ast_node_alter_algorithm(statement);
    lock = mylite_sql_ast_node_alter_lock(statement);
    if (algorithm == MYLITE_SQL_AST_ALTER_ALGORITHM_UNSPECIFIED &&
        lock == MYLITE_SQL_AST_ALTER_LOCK_UNSPECIFIED) {
        return MYLITE_OK;
    }

    if ((statement->kind == MYLITE_SQL_AST_ALTER_TABLE_DISABLE_KEYS_STATEMENT ||
         statement->kind == MYLITE_SQL_AST_ALTER_TABLE_ENABLE_KEYS_STATEMENT) &&
        algorithm == MYLITE_SQL_AST_ALTER_ALGORITHM_INSTANT &&
        (lock == MYLITE_SQL_AST_ALTER_LOCK_NONE || lock == MYLITE_SQL_AST_ALTER_LOCK_SHARED ||
         lock == MYLITE_SQL_AST_ALTER_LOCK_EXCLUSIVE)) {
        rc = reject_builtin_schema_write_target(database, child_at(statement, 0U));
        if (rc != MYLITE_OK) {
            return rc;
        }
        rc = require_selected_schema_for_unqualified_table_name(database, child_at(statement, 0U));
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    rc = validate_alter_table_algorithm_lock_option_values(database, algorithm, lock);
    if (rc != MYLITE_OK) {
        return rc;
    }

    switch (statement->kind) {
    case MYLITE_SQL_AST_ALTER_TABLE_ADD_COLUMN_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_COLUMN_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_RENAME_COLUMN_STATEMENT:
        return validate_alter_table_column_algorithm_options(database, algorithm);

    case MYLITE_SQL_AST_ALTER_TABLE_ADD_INDEX_STATEMENT:
        return validate_alter_table_add_index_algorithm_lock_options(
            database,
            statement,
            algorithm,
            lock
        );

    case MYLITE_SQL_AST_ALTER_TABLE_DROP_INDEX_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_RENAME_INDEX_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_INDEX_VISIBILITY_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_FOREIGN_KEY_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_COMMENT_STATEMENT:
        return validate_alter_table_online_metadata_algorithm_options(database, algorithm);

    case MYLITE_SQL_AST_ALTER_TABLE_ADD_FOREIGN_KEY_STATEMENT:
        return validate_alter_table_add_foreign_key_algorithm_lock_options(
            database,
            algorithm,
            lock
        );

    case MYLITE_SQL_AST_ALTER_TABLE_ADD_PRIMARY_KEY_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_PRIMARY_KEY_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_FORCE_STATEMENT:
        return validate_alter_table_rebuild_algorithm_options(database, algorithm);

    default:
        return MYLITE_OK;
    }
}

static int validate_alter_table_algorithm_lock_option_values(
    struct mylite_db *database,
    enum mylite_sql_ast_alter_algorithm algorithm,
    enum mylite_sql_ast_alter_lock lock
) {
    if (algorithm == MYLITE_SQL_AST_ALTER_ALGORITHM_UNKNOWN ||
        lock == MYLITE_SQL_AST_ALTER_LOCK_UNKNOWN) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (algorithm == MYLITE_SQL_AST_ALTER_ALGORITHM_INSTANT &&
        (lock == MYLITE_SQL_AST_ALTER_LOCK_NONE || lock == MYLITE_SQL_AST_ALTER_LOCK_SHARED ||
         lock == MYLITE_SQL_AST_ALTER_LOCK_EXCLUSIVE)) {
        set_alter_table_instant_lock_error(database);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_alter_table_column_algorithm_options(
    struct mylite_db *database,
    enum mylite_sql_ast_alter_algorithm algorithm
) {
    if (algorithm == MYLITE_SQL_AST_ALTER_ALGORITHM_INPLACE) {
        set_unsupported_error(database, "ALTER TABLE action does not support ALGORITHM=INPLACE");
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_alter_table_add_index_algorithm_lock_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    enum mylite_sql_ast_alter_algorithm algorithm,
    enum mylite_sql_ast_alter_lock lock
) {
    if (alter_table_add_index_statement_is_fulltext(statement)) {
        if (algorithm == MYLITE_SQL_AST_ALTER_ALGORITHM_INSTANT) {
            set_alter_table_add_fulltext_instant_error(database);
            return MYLITE_ERROR;
        }
        if (lock == MYLITE_SQL_AST_ALTER_LOCK_NONE) {
            if (algorithm == MYLITE_SQL_AST_ALTER_ALGORITHM_COPY) {
                set_alter_table_copy_lock_none_error(database);
            } else {
                set_alter_table_add_fulltext_lock_none_error(database);
            }
            return MYLITE_ERROR;
        }
        return MYLITE_OK;
    }
    return validate_alter_table_online_metadata_algorithm_options(database, algorithm);
}

static int validate_alter_table_online_metadata_algorithm_options(
    struct mylite_db *database,
    enum mylite_sql_ast_alter_algorithm algorithm
) {
    if (algorithm == MYLITE_SQL_AST_ALTER_ALGORITHM_INSTANT) {
        set_alter_table_instant_algorithm_error(database);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_alter_table_add_foreign_key_algorithm_lock_options(
    struct mylite_db *database,
    enum mylite_sql_ast_alter_algorithm algorithm,
    enum mylite_sql_ast_alter_lock lock
) {
    if (algorithm == MYLITE_SQL_AST_ALTER_ALGORITHM_INSTANT) {
        set_alter_table_add_foreign_key_instant_error(database);
        return MYLITE_ERROR;
    }
    if (algorithm == MYLITE_SQL_AST_ALTER_ALGORITHM_INPLACE) {
        set_alter_table_add_foreign_key_inplace_error(database);
        return MYLITE_ERROR;
    }
    if (lock == MYLITE_SQL_AST_ALTER_LOCK_NONE) {
        set_alter_table_add_foreign_key_lock_none_error(database, algorithm);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_alter_table_rebuild_algorithm_options(
    struct mylite_db *database,
    enum mylite_sql_ast_alter_algorithm algorithm
) {
    if (algorithm == MYLITE_SQL_AST_ALTER_ALGORITHM_INSTANT) {
        set_unsupported_error(database, "ALTER TABLE action does not support ALGORITHM=INSTANT");
        return MYLITE_ERROR;
    }
    if (algorithm == MYLITE_SQL_AST_ALTER_ALGORITHM_INPLACE) {
        set_unsupported_error(database, "ALTER TABLE action does not support ALGORITHM=INPLACE");
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_alter_table_key_maintenance_algorithm_lock_options(
    struct mylite_db *database,
    enum mylite_sql_ast_alter_algorithm algorithm,
    enum mylite_sql_ast_alter_lock lock
) {
    if (lock == MYLITE_SQL_AST_ALTER_LOCK_NONE) {
        if (algorithm == MYLITE_SQL_AST_ALTER_ALGORITHM_COPY) {
            set_alter_table_copy_lock_none_error(database);
        } else {
            set_alter_table_key_maintenance_lock_error(database);
        }
        return MYLITE_ERROR;
    }
    if (lock == MYLITE_SQL_AST_ALTER_LOCK_SHARED &&
        algorithm != MYLITE_SQL_AST_ALTER_ALGORITHM_COPY) {
        set_alter_table_key_maintenance_lock_error(database);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static bool alter_table_statement_accepts_algorithm_lock_options(
    enum mylite_sql_ast_node_kind kind
) {
    switch (kind) {
    case MYLITE_SQL_AST_ALTER_TABLE_ADD_COLUMN_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_COLUMN_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_RENAME_COLUMN_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_ADD_PRIMARY_KEY_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_PRIMARY_KEY_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_ADD_INDEX_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_INDEX_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_RENAME_INDEX_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_INDEX_VISIBILITY_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_ADD_FOREIGN_KEY_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_FOREIGN_KEY_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_CONSTRAINT_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_COMMENT_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_FORCE_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_DISABLE_KEYS_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_ENABLE_KEYS_STATEMENT:
        return true;
    default:
        return false;
    }
}

static bool alter_table_add_index_statement_is_fulltext(
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *index_definition = child_at(statement, 1U);

    if (index_definition == NULL) {
        return false;
    }
    switch (index_definition->kind) {
    case MYLITE_SQL_AST_FULLTEXT_INDEX_DEFINITION:
        return true;
    default:
        return false;
    }
}
