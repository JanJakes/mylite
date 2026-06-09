static int plan_create_table(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool is_temporary_target,
    struct planned_create_table *out_plan
) {
    const struct mylite_sql_ast_node *item_list = child_at(statement, 1U);
    int rc = MYLITE_OK;

    *out_plan = (struct planned_create_table){0};
    out_plan->auto_increment_next = 1;
    out_plan->pack_keys = -1;
    out_plan->stats_persistent = -1;
    out_plan->stats_auto_recalc = -1;
    out_plan->delay_key_write = -1;
    memcpy(
        out_plan->default_charset,
        MYLITE_CATALOG_DEFAULT_TABLE_CHARSET,
        sizeof(MYLITE_CATALOG_DEFAULT_TABLE_CHARSET)
    );
    memcpy(
        out_plan->default_collation,
        MYLITE_CATALOG_DEFAULT_TABLE_COLLATION,
        sizeof(MYLITE_CATALOG_DEFAULT_TABLE_COLLATION)
    );
    rc = resolve_writable_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc != MYLITE_OK) {
        return rc;
    }
    memcpy(
        out_plan->default_charset,
        out_plan->target.schema.default_charset,
        strlen(out_plan->target.schema.default_charset) + 1U
    );
    memcpy(
        out_plan->default_collation,
        out_plan->target.schema.default_collation,
        strlen(out_plan->target.schema.default_collation) + 1U
    );
    if (mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        return MYLITE_ERROR;
    }
    rc = validate_create_table_options(
        database,
        create_table_options_node(statement),
        out_plan->target.table_name,
        is_temporary_target
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = apply_create_table_charset_collation_options(
        database,
        create_table_options_node(statement),
        out_plan
    );
    if (rc == MYLITE_OK) {
        rc = apply_create_table_comment_option(
            database,
            create_table_options_node(statement),
            out_plan
        );
    }
    if (rc == MYLITE_OK) {
        rc = apply_create_table_storage_statistics_options(
            database,
            create_table_options_node(statement),
            out_plan
        );
    }
    if (rc != MYLITE_OK) {
        planned_create_table_deinit(out_plan);
        return rc;
    }

    rc = plan_create_table_items(database, item_list, out_plan);
    if (rc != MYLITE_OK) {
        planned_create_table_deinit(out_plan);
        return rc;
    }
    rc = apply_create_table_auto_increment_option(
        database,
        create_table_options_node(statement),
        out_plan
    );
    if (rc == MYLITE_OK) {
        rc = validate_create_table_auto_increment_columns(database, out_plan, true);
    }
    if (rc == MYLITE_OK) {
        rc = validate_create_table_visible_columns(database, out_plan);
    }
    if (rc != MYLITE_OK) {
        planned_create_table_deinit(out_plan);
        return rc;
    }

    return MYLITE_OK;
}

static int validate_create_table_visible_columns(
    struct mylite_db *database,
    const struct planned_create_table *plan
) {
    size_t visible_count = 0U;

    for (size_t column_index = 0U; column_index < plan->column_count; ++column_index) {
        if (plan->columns[column_index].is_visible) {
            ++visible_count;
        }
    }
    if (visible_count == 0U) {
        set_must_have_visible_column_error(database);
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int plan_create_table_items(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *item_list,
    struct planned_create_table *out_plan
) {
    const struct mylite_sql_ast_node *item = NULL;
    const struct mylite_sql_ast_node *primary_key = NULL;
    size_t column_index = 0U;
    int rc = count_create_table_column_items(database, item_list, &out_plan->column_count);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_plan->column_count > SIZE_MAX / sizeof(*out_plan->columns)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    out_plan->columns = calloc(out_plan->column_count, sizeof(*out_plan->columns));
    if (out_plan->columns == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    item = child_at(item_list, 0U);
    while (rc == MYLITE_OK && item != NULL) {
        rc = plan_create_table_item(database, item, out_plan, &column_index, &primary_key);
        item = item->next_sibling;
    }
    if (rc == MYLITE_OK) {
        rc = finalize_planned_generated_columns(database, out_plan);
    }
    if (rc == MYLITE_OK && primary_key != NULL) {
        rc = apply_create_table_primary_key_definition(database, out_plan, primary_key);
    }
    if (rc == MYLITE_OK) {
        rc = apply_create_table_inline_unique_indexes(database, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = apply_create_table_secondary_index_definitions(database, item_list, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = apply_create_table_foreign_key_definitions(database, item_list, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = apply_create_table_check_constraint_definitions(database, item_list, out_plan);
    }
    if (rc != MYLITE_OK) {
        planned_create_table_deinit(out_plan);
    }

    return rc;
}

static int plan_create_table_item(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *item,
    struct planned_create_table *out_plan,
    size_t *column_index,
    const struct mylite_sql_ast_node **primary_key
) {
    int rc = MYLITE_OK;

    if (item->kind == MYLITE_SQL_AST_COLUMN_DEFINITION) {
        size_t current_column_index = *column_index;

        if (current_column_index >= out_plan->column_count) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        rc = plan_column(database, item, &out_plan->columns[current_column_index]);
        if (rc == MYLITE_OK) {
            rc = normalize_text_length_argument_for_default_charset(
                database,
                &out_plan->columns[current_column_index],
                out_plan->default_charset
            );
        }
        if (rc == MYLITE_OK) {
            rc = apply_column_binary_collation_shorthand(
                database,
                out_plan->default_charset,
                &out_plan->columns[current_column_index]
            );
        }
        if (rc == MYLITE_OK) {
            rc = apply_create_table_default_binary_charset_to_column(
                database,
                out_plan,
                item,
                &out_plan->columns[current_column_index]
            );
        }
        if (rc == MYLITE_OK) {
            rc = validate_column_default(
                database,
                out_plan->columns[current_column_index].default_node,
                &out_plan->columns[current_column_index]
            );
        }
        if (rc == MYLITE_OK && child_with_kind(item, MYLITE_SQL_AST_INLINE_PRIMARY_KEY) != NULL) {
            rc = apply_create_table_inline_primary_key(
                database,
                out_plan,
                current_column_index,
                item
            );
        }
        if (rc == MYLITE_OK && child_with_kind(item, MYLITE_SQL_AST_INLINE_UNIQUE_KEY) != NULL) {
            out_plan->columns[current_column_index].is_unique_key = true;
        }
        *column_index = current_column_index + 1U;
        return rc;
    }

    if (item->kind == MYLITE_SQL_AST_PRIMARY_KEY_DEFINITION) {
        if (*primary_key != NULL) {
            set_multiple_primary_key_error(database);
            return MYLITE_ERROR;
        }
        *primary_key = item;
        return MYLITE_OK;
    }

    if (item->kind == MYLITE_SQL_AST_SECONDARY_INDEX_DEFINITION ||
        item->kind == MYLITE_SQL_AST_UNIQUE_INDEX_DEFINITION ||
        item->kind == MYLITE_SQL_AST_FULLTEXT_INDEX_DEFINITION ||
        item->kind == MYLITE_SQL_AST_SPATIAL_INDEX_DEFINITION ||
        item->kind == MYLITE_SQL_AST_FOREIGN_KEY_DEFINITION ||
        item->kind == MYLITE_SQL_AST_CHECK_CONSTRAINT_DEFINITION) {
        return MYLITE_OK;
    }

    set_parse_error(database, NULL);
    return MYLITE_ERROR;
}
