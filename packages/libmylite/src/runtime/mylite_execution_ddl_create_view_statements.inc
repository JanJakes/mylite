static int execute_create_view_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    return execute_create_or_alter_view_statement(database, statement, out_result);
}

static int execute_alter_view_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    return execute_create_or_alter_view_statement(database, statement, out_result);
}

static int execute_create_or_alter_view_statement(
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
    out_plan->statement_kind = PLANNED_VIEW_CREATE;
    if (statement->kind == MYLITE_SQL_AST_ALTER_VIEW_STATEMENT) {
        out_plan->statement_kind = PLANNED_VIEW_ALTER;
    } else if (child_with_kind(statement, MYLITE_SQL_AST_CREATE_OR_REPLACE_CLAUSE) != NULL) {
        out_plan->statement_kind = PLANNED_VIEW_CREATE_OR_REPLACE;
    }
    snprintf(out_plan->algorithm, sizeof(out_plan->algorithm), "UNDEFINED");
    snprintf(out_plan->check_option, sizeof(out_plan->check_option), "NONE");
    snprintf(out_plan->is_updatable, sizeof(out_plan->is_updatable), "YES");
    snprintf(
        out_plan->definer,
        sizeof(out_plan->definer),
        "%s",
        database->session.current_user_identity
    );
    snprintf(out_plan->security_type, sizeof(out_plan->security_type), "DEFINER");

    rc = resolve_writable_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = plan_create_view_options(database, statement, out_plan);
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
    if (rc == MYLITE_OK && strcmp(out_plan->algorithm, "TEMPTABLE") == 0) {
        snprintf(out_plan->is_updatable, sizeof(out_plan->is_updatable), "NO");
    }
    if (rc == MYLITE_OK && strcmp(out_plan->is_updatable, "NO") == 0 &&
        strcmp(out_plan->check_option, "NONE") != 0) {
        set_view_check_option_non_updatable_error(database, out_plan);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = copy_create_view_column_names(database, statement, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = copy_create_view_select_column_names(database, out_plan);
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

static int plan_create_view_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_create_view *out_plan
) {
    const struct mylite_sql_ast_node *option_list =
        child_with_kind(statement, MYLITE_SQL_AST_VIEW_OPTION_LIST);
    const struct mylite_sql_ast_node *option =
        option_list == NULL ? NULL : child_at(option_list, 0U);
    int rc = MYLITE_OK;

    while (rc == MYLITE_OK && option != NULL) {
        rc = apply_create_view_option(database, option, out_plan);
        option = option->next_sibling;
    }
    if (rc == MYLITE_OK) {
        copy_view_check_option(
            child_with_kind(statement, MYLITE_SQL_AST_VIEW_CHECK_OPTION),
            out_plan->check_option,
            sizeof(out_plan->check_option)
        );
    }
    return rc;
}

static int apply_create_view_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *option,
    struct planned_create_view *plan
) {
    if (option == NULL) {
        return MYLITE_OK;
    }

    switch (option->kind) {
    case MYLITE_SQL_AST_VIEW_ALGORITHM_OPTION:
        return copy_view_option_keyword(
            database,
            child_at(option, 0U),
            plan->algorithm,
            sizeof(plan->algorithm)
        );
    case MYLITE_SQL_AST_VIEW_DEFINER_OPTION:
        return copy_view_definer_account(
            database,
            child_at(option, 0U),
            plan->definer,
            sizeof(plan->definer)
        );
    case MYLITE_SQL_AST_VIEW_SECURITY_OPTION:
        return copy_view_option_keyword(
            database,
            child_at(option, 0U),
            plan->security_type,
            sizeof(plan->security_type)
        );
    default:
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
}

static int copy_view_option_keyword(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    char *destination,
    size_t destination_size
) {
    if (node == NULL || node->span.text == NULL || node->span.length == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (node->span.length >= destination_size) {
        set_identifier_too_long_error(database, "view option");
        return MYLITE_ERROR;
    }

    for (size_t index = 0U; index < node->span.length; ++index) {
        destination[index] = (char)toupper((unsigned char)node->span.text[index]);
    }
    destination[node->span.length] = '\0';
    return MYLITE_OK;
}

static int copy_view_definer_account(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *account,
    char *destination,
    size_t destination_size
) {
    const struct mylite_sql_ast_node *user_node = NULL;
    const struct mylite_sql_ast_node *host_node = NULL;
    char user[MYLITE_CATALOG_DEFINER_CAPACITY];
    char host[MYLITE_CATALOG_DEFINER_CAPACITY];
    int rc = MYLITE_OK;

    if (account == NULL) {
        snprintf(destination, destination_size, "%s", database->session.current_user_identity);
        return MYLITE_OK;
    }
    user_node = child_at(account, 0U);
    host_node = child_at(account, 1U);
    if (user_node == NULL) {
        snprintf(destination, destination_size, "%s", database->session.current_user_identity);
        return MYLITE_OK;
    }

    user[0] = '\0';
    host[0] = '\0';
    rc = copy_view_definer_user(database, user_node, user, sizeof(user));
    if (rc == MYLITE_OK) {
        rc = copy_view_definer_host(database, host_node, host, sizeof(host));
    }
    if (rc == MYLITE_OK) {
        rc = append_view_account(database, user, host, destination, destination_size);
    }
    return rc;
}

static int copy_view_definer_user(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    char *destination,
    size_t destination_size
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (node->kind != MYLITE_SQL_AST_LITERAL) {
        return copy_identifier_text(node, destination, destination_size, database);
    }

    rc = decode_sql_string_literal(
        database,
        node,
        "CREATE VIEW DEFINER supports only string definer accounts",
        "CREATE VIEW DEFINER account literals do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc == MYLITE_OK) {
        rc = copy_view_account_component_span(
            database,
            text,
            text_length,
            destination,
            destination_size
        );
    }
    free(text);
    return rc;
}

static int copy_view_definer_host(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    char *destination,
    size_t destination_size
) {
    const char *text = NULL;
    size_t text_size = 0U;

    if (node == NULL) {
        snprintf(destination, destination_size, "%%");
        return MYLITE_OK;
    }
    if (node->span.text == NULL || node->span.length == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    text = node->span.text;
    text_size = node->span.length;
    if (text_size != 0U && text[0] == '@') {
        ++text;
        --text_size;
    }
    return copy_view_account_component_span(database, text, text_size, destination, destination_size);
}

static int copy_view_account_component_span(
    struct mylite_db *database,
    const char *text,
    size_t text_size,
    char *destination,
    size_t destination_size
) {
    char quote = '\0';
    size_t begin = 0U;
    size_t end = text_size;
    size_t copied = 0U;

    if (text == NULL || destination_size == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (text_size >= 2U &&
        (text[0] == '\'' || text[0] == '"' || text[0] == '`') &&
        text[text_size - 1U] == text[0]) {
        quote = text[0];
        begin = 1U;
        end = text_size - 1U;
    }

    for (size_t index = begin; index < end; ++index) {
        char byte = text[index];

        if (quote != '\0' && byte == quote && index + 1U < end && text[index + 1U] == quote) {
            ++index;
        } else if (quote != '`' && byte == '\\' && index + 1U < end) {
            ++index;
            byte = text[index];
        }
        if (copied + 1U >= destination_size) {
            set_identifier_too_long_error(database, "definer");
            return MYLITE_ERROR;
        }
        destination[copied] = byte;
        ++copied;
    }
    destination[copied] = '\0';
    return MYLITE_OK;
}

static int append_view_account(
    struct mylite_db *database,
    const char *user,
    const char *host,
    char *destination,
    size_t destination_size
) {
    int written = snprintf(destination, destination_size, "%s@%s", user, host);

    if (written < 0 || (size_t)written >= destination_size) {
        set_identifier_too_long_error(database, "definer");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static void copy_view_check_option(
    const struct mylite_sql_ast_node *check_option_node,
    char *destination,
    size_t destination_size
) {
    const char *value = "NONE";

    if (check_option_node != NULL) {
        value = view_check_option_span_has_keyword(check_option_node, "LOCAL") ? "LOCAL"
                                                                               : "CASCADED";
    }
    snprintf(destination, destination_size, "%s", value);
}

static bool view_check_option_span_has_keyword(
    const struct mylite_sql_ast_node *check_option_node,
    const char *keyword
) {
    size_t keyword_length = strlen(keyword);

    if (check_option_node == NULL || check_option_node->span.text == NULL) {
        return false;
    }
    if (keyword_length == 0U || keyword_length > check_option_node->span.length) {
        return false;
    }

    for (size_t offset = 0U; offset + keyword_length <= check_option_node->span.length; ++offset) {
        bool matched = true;

        for (size_t index = 0U; index < keyword_length; ++index) {
            char left =
                (char)toupper((unsigned char)check_option_node->span.text[offset + index]);

            if (left != keyword[index]) {
                matched = false;
                break;
            }
        }
        if (matched) {
            return true;
        }
    }
    return false;
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
    if (existing_table_found && plan->statement_kind == PLANNED_VIEW_CREATE) {
        set_table_exists_error(database, plan->target.table_name);
        return MYLITE_ERROR;
    }
    if (existing_table_found && existing_table.kind != MYLITE_CATALOG_TABLE_KIND_VIEW) {
        if (plan->statement_kind == PLANNED_VIEW_ALTER) {
            set_not_view_error(database, plan->target.schema.name, plan->target.table_name);
        } else {
            set_table_exists_error(database, plan->target.table_name);
        }
        return MYLITE_ERROR;
    }
    if (!existing_table_found && plan->statement_kind == PLANNED_VIEW_ALTER) {
        set_table_does_not_exist_error(database, plan->target.schema.name, plan->target.table_name);
        return MYLITE_ERROR;
    }

    rc = mylite_catalog_begin_mutation(database, &mutation);
    if (rc == MYLITE_OK && existing_table_found) {
        rc = mylite_catalog_delete_table_in_mutation(database, &mutation, existing_table.table_id);
    }
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
            0,
            0,
            0,
            -1,
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
            plan->check_option,
            plan->is_updatable,
            plan->definer,
            plan->security_type,
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
        if (plan->column_names != NULL) {
            free(plan->column_names[column_index]);
        }
        if (plan->select_column_names != NULL) {
            free(plan->select_column_names[column_index]);
        }
    }
    free((void *)plan->column_names);
    free((void *)plan->select_column_names);
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
    const struct mylite_sql_ast_node *statement,
    struct planned_create_view *plan
) {
    const struct mylite_sql_ast_node *column_list =
        child_with_kind(statement, MYLITE_SQL_AST_IDENTIFIER_LIST);

    if (column_list == NULL) {
        return copy_create_view_inferred_column_names(database, plan);
    }
    plan->has_explicit_column_names = true;
    return copy_create_view_explicit_column_names(database, column_list, plan);
}

static int copy_create_view_inferred_column_names(
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
        int rc = copy_create_view_inferred_column_name(
            database,
            plan,
            column_index,
            &plan->column_names[column_index]
        );

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

static int copy_create_view_explicit_column_names(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_list,
    struct planned_create_view *plan
) {
    const struct mylite_sql_ast_node *column_node = child_at(column_list, 0U);
    size_t column_count = create_view_explicit_column_count(column_list);

    if (column_count != plan->source.column_count) {
        set_view_column_count_error(database);
        return MYLITE_ERROR;
    }
    if (column_count > SIZE_MAX / sizeof(*plan->column_names)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    plan->column_names = (char **)calloc(column_count, sizeof(*plan->column_names));
    if (plan->column_names == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    plan->column_count = column_count;

    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
        int rc = copy_identifier_text(column_node, column_name, sizeof(column_name), database);

        if (rc == MYLITE_OK) {
            rc = duplicate_text(database, column_name, &plan->column_names[column_index]);
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
        column_node = column_node->next_sibling;
    }
    return MYLITE_OK;
}

static size_t create_view_explicit_column_count(const struct mylite_sql_ast_node *column_list) {
    const struct mylite_sql_ast_node *column_node = child_at(column_list, 0U);
    size_t column_count = 0U;

    while (column_node != NULL) {
        ++column_count;
        column_node = column_node->next_sibling;
    }
    return column_count;
}

static int copy_create_view_select_column_names(
    struct mylite_db *database,
    struct planned_create_view *plan
) {
    if (plan->column_count > SIZE_MAX / sizeof(*plan->select_column_names)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    plan->select_column_names =
        (char **)calloc(plan->column_count, sizeof(*plan->select_column_names));
    if (plan->select_column_names == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    for (size_t column_index = 0U; column_index < plan->column_count; ++column_index) {
        int rc = copy_create_view_inferred_column_name(
            database,
            plan,
            column_index,
            &plan->select_column_names[column_index]
        );

        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    return MYLITE_OK;
}

static int copy_create_view_inferred_column_name(
    struct mylite_db *database,
    struct planned_create_view *plan,
    size_t column_index,
    char **out_name
) {
    const struct mylite_sql_ast_node *alias = plan->source.column_aliases[column_index];

    if (alias != NULL) {
        return copy_select_item_alias_text(database, alias, out_name);
    }
    return duplicate_text(database, plan->source.columns[column_index].name, out_name);
}

static void set_view_column_count_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_view_wrong_list,
        "HY000",
        "View's SELECT and view's field list have different column counts"
    );
}

static void set_view_check_option_non_updatable_error(
    struct mylite_db *database,
    const struct planned_create_view *plan
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "CHECK OPTION on non-updatable view '%s.%s'",
        plan->target.schema.name,
        plan->target.table_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_check_option_on_non_updatable_view,
        "HY000",
        message
    );
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
        rc = mylite_dynamic_string_append(&string, "CREATE ALGORITHM=");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, plan->algorithm);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " DEFINER=");
    }
    if (rc == MYLITE_OK) {
        rc = append_show_create_definer(&string, plan->definer);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " SQL SECURITY ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, plan->security_type);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " VIEW ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_mysql_quoted_identifier(&string, plan->target.table_name);
    }
    if (rc == MYLITE_OK && plan->has_explicit_column_names) {
        rc = append_create_view_column_list_sql(&string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " AS ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, select_sql);
    }
    if (rc == MYLITE_OK) {
        rc = append_create_view_check_option_sql(&string, plan);
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
                plan->select_column_names[column_index]
            );
        }
    }
    return rc;
}

static int append_create_view_column_list_sql(
    struct mylite_dynamic_string *string,
    const struct planned_create_view *plan
) {
    int rc = mylite_dynamic_string_append(string, " (");

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        if (column_index > 0U) {
            rc = mylite_dynamic_string_append_char(string, ',');
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_mysql_quoted_identifier(
                string,
                plan->column_names[column_index]
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    return rc;
}

static int append_create_view_check_option_sql(
    struct mylite_dynamic_string *string,
    const struct planned_create_view *plan
) {
    int rc = MYLITE_OK;

    if (strcmp(plan->check_option, "NONE") == 0) {
        return MYLITE_OK;
    }

    rc = mylite_dynamic_string_append(string, " WITH ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, plan->check_option);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " CHECK OPTION");
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
