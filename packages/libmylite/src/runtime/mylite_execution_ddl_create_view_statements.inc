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
            0,
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
