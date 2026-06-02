static int plan_alter_table_comment(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_comment *out_plan
) {
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_comment){0};
    rc = resolve_visible_writable_table_reference(
        database,
        child_at(statement, 0U),
        &out_plan->target,
        &out_plan->table
    );
    if (rc == MYLITE_OK && out_plan->table.kind != MYLITE_CATALOG_TABLE_KIND_BASE &&
        out_plan->table.kind != MYLITE_CATALOG_TABLE_KIND_TEMPORARY) {
        set_unsupported_error(database, "ALTER TABLE COMMENT supports only base tables");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = decode_table_comment_option(
            database,
            child_at(statement, 1U),
            out_plan->target.table_name,
            out_plan->comment,
            sizeof(out_plan->comment)
        );
    }

    return rc;
}

static int plan_alter_table_order_by(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_order_by *out_plan
) {
    struct select_source_context source_context = {0};
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_order_by){0};
    rc = resolve_writable_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_read_table_by_name(
            database,
            out_plan->target.schema.schema_id,
            out_plan->target.table_name,
            &out_plan->table
        );
        if (rc != MYLITE_OK) {
            set_table_does_not_exist_error(
                database,
                out_plan->target.schema.name,
                out_plan->target.table_name
            );
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && out_plan->table.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(
            database,
            "ALTER TABLE ORDER BY supports only persistent base tables"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_reject_primary_key_table_alter(
            database,
            out_plan->table.table_id,
            "ALTER TABLE ORDER BY does not yet support primary-key tables"
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_reject_secondary_index_table_alter(
            database,
            out_plan->table.table_id,
            "ALTER TABLE ORDER BY does not yet support secondary-index tables"
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_reject_check_constraint_table_alter(
            database,
            out_plan->table.table_id,
            "ALTER TABLE ORDER BY does not yet support CHECK-constrained tables"
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_columns(
            database,
            out_plan->table.table_id,
            &out_plan->columns,
            &out_plan->column_count
        );
    }
    if (rc == MYLITE_OK) {
        source_context.source = &out_plan->target;
        rc = plan_alter_table_order_by_items(
            database,
            child_at(statement, 1U),
            &source_context,
            out_plan
        );
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_order_by_deinit(out_plan);
    }

    return rc;
}

static int plan_alter_table_order_by_items(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_items,
    const struct select_source_context *source_context,
    struct planned_alter_table_order_by *out_plan
) {
    size_t item_count = mylite_sql_ast_node_child_count(order_items);
    int rc = MYLITE_OK;

    if (order_items == NULL || order_items->kind != MYLITE_SQL_AST_ORDER_BY_ITEM_LIST ||
        item_count == 0U) {
        set_unsupported_error(database, "ALTER TABLE ORDER BY requires descriptor order columns");
        return MYLITE_ERROR;
    }
    if (item_count > SIZE_MAX / sizeof(*out_plan->items)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    out_plan->items = calloc(item_count, sizeof(*out_plan->items));
    if (out_plan->items == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_plan->item_count = item_count;

    for (size_t item_index = 0U; rc == MYLITE_OK && item_index < item_count; ++item_index) {
        rc = plan_alter_table_order_by_item(
            database,
            child_at(order_items, item_index),
            source_context,
            out_plan->columns,
            out_plan->column_count,
            &out_plan->items[item_index]
        );
    }

    return rc;
}

static int plan_alter_table_order_by_item(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *item,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_alter_table_order_by_item *out_item
) {
    const struct mylite_sql_ast_node *direction = NULL;
    size_t source_index = 0U;
    int rc = MYLITE_OK;

    *out_item = (struct planned_alter_table_order_by_item){0};
    if (item == NULL || item->kind != MYLITE_SQL_AST_ORDER_BY_ITEM) {
        set_unsupported_error(database, "ALTER TABLE ORDER BY requires descriptor order columns");
        return MYLITE_ERROR;
    }

    rc = resolve_order_column(
        database,
        child_at(item, 0U),
        source_context,
        table_columns,
        table_column_count,
        &out_item->column,
        &source_index
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    out_item->direction = PLANNED_SELECT_ORDER_ASC;
    direction = child_at(item, 1U);
    if (mylite_sql_ast_node_order_direction(direction) == MYLITE_SQL_AST_ORDER_DIRECTION_DESC) {
        out_item->direction = PLANNED_SELECT_ORDER_DESC;
    }

    return MYLITE_OK;
}

static void planned_alter_table_order_by_deinit(struct planned_alter_table_order_by *plan) {
    if (plan == NULL) {
        return;
    }

    free(plan->columns);
    free(plan->items);
    *plan = (struct planned_alter_table_order_by){0};
}

static int alter_table_order_by_from_plan(
    struct mylite_db *database,
    struct planned_alter_table_order_by *plan
) {
    int rc = execute_physical_alter_table_order_by(database, plan, &plan->affected_rows);

    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
            return rc;
        }
        set_internal_error_if_clear(database, rc, "failed to rebuild table order");
        return rc;
    }

    return MYLITE_OK;
}

static int plan_alter_table_force(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_force *out_plan
) {
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_force){0};
    rc = resolve_writable_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_read_table_by_name(
            database,
            out_plan->target.schema.schema_id,
            out_plan->target.table_name,
            &out_plan->table
        );
        if (rc != MYLITE_OK) {
            set_table_does_not_exist_error(
                database,
                out_plan->target.schema.name,
                out_plan->target.table_name
            );
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && out_plan->table.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(database, "ALTER TABLE FORCE supports only persistent base tables");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_reject_primary_key_table_alter(
            database,
            out_plan->table.table_id,
            "ALTER TABLE FORCE does not yet support primary-key tables"
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_reject_secondary_index_table_alter(
            database,
            out_plan->table.table_id,
            "ALTER TABLE FORCE does not yet support secondary-index tables"
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_reject_check_constraint_table_alter(
            database,
            out_plan->table.table_id,
            "ALTER TABLE FORCE does not yet support CHECK-constrained tables"
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_columns(
            database,
            out_plan->table.table_id,
            &out_plan->columns,
            &out_plan->column_count
        );
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_force_deinit(out_plan);
    }

    return rc;
}

static void planned_alter_table_force_deinit(struct planned_alter_table_force *plan) {
    if (plan == NULL) {
        return;
    }

    free(plan->columns);
    *plan = (struct planned_alter_table_force){0};
}

static int alter_table_force_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_force *plan
) {
    int rc = execute_physical_alter_table_force(database, plan);

    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
            return rc;
        }
        set_internal_error_if_clear(database, rc, "failed to force rebuild table");
        return rc;
    }

    return MYLITE_OK;
}

static int plan_alter_table_key_maintenance(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_key_maintenance *out_plan
) {
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_key_maintenance){0};
    rc = resolve_visible_writable_table_reference(
        database,
        child_at(statement, 0U),
        &out_plan->target,
        &out_plan->table
    );
    if (rc != MYLITE_OK) {
        *out_plan = (struct planned_alter_table_key_maintenance){0};
    }
    return rc;
}
