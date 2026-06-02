static int plan_row_scalar_select_items(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct select_source_context *source_context,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_row_scalar_select *out_plan
) {
    const struct mylite_sql_ast_node *item = NULL;

    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        set_unsupported_error(database, "row-scalar SELECT supports only explicit select items");
        return MYLITE_ERROR;
    }
    if (select_list_is_wildcard(select_list)) {
        if (!out_plan->has_source) {
            set_no_tables_used_error(database);
            return MYLITE_ERROR;
        }
        set_unsupported_error(database, "row-scalar SELECT supports only explicit select items");
        return MYLITE_ERROR;
    }

    item = child_at(select_list, 0U);
    while (item != NULL) {
        int rc = append_row_scalar_select_item(
            database,
            out_plan,
            item,
            source_context,
            table,
            table_columns,
            table_column_count
        );

        if (rc != MYLITE_OK) {
            return rc;
        }
        item = item->next_sibling;
    }

    return MYLITE_OK;
}

static int append_row_scalar_select_item(
    struct mylite_db *database,
    struct planned_row_scalar_select *plan,
    const struct mylite_sql_ast_node *select_item,
    const struct select_source_context *source_context,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count
) {
    struct planned_row_scalar_select_item *items = NULL;
    struct planned_row_scalar_select_item item = {0};
    size_t required_count = 0U;
    int rc = MYLITE_OK;

    if (select_item == NULL || select_item->kind != MYLITE_SQL_AST_SELECT_ITEM) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    item.source_expression = child_at(select_item, 0U);
    item.alias = child_at(select_item, 1U);
    rc = plan_row_scalar_expression(
        database,
        item.source_expression,
        plan->has_source,
        source_context,
        table,
        table_columns,
        table_column_count,
        &item.expression
    );
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(&item.expression);
        return rc;
    }

    required_count = plan->item_count + 1U;
    if (required_count > SIZE_MAX / sizeof(*items)) {
        planned_row_scalar_expression_deinit(&item.expression);
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    items = realloc(plan->items, required_count * sizeof(*items));
    if (items == NULL) {
        planned_row_scalar_expression_deinit(&item.expression);
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    plan->items = items;
    plan->items[plan->item_count] = item;
    plan->item_count = required_count;

    return MYLITE_OK;
}
