static int plan_alter_table_modify_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_modify_column *out_plan
) {
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_modify_column){0};
    out_plan->unsupported_object_message =
        "ALTER TABLE MODIFY COLUMN supports only persistent base tables";
    out_plan->rowid_alias_message =
        "ALTER TABLE MODIFY COLUMN requires an unshadowed SQLite rowid alias";
    out_plan->integer_support_message =
        "ALTER TABLE MODIFY COLUMN supports only compatible baseline integer, character, "
        "text, binary string, and temporal column replacements";
    out_plan->row_count_overflow_message =
        "too many rows to validate for ALTER TABLE MODIFY COLUMN";
    out_plan->failure_message = "failed to modify table column";

    rc = resolve_writable_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = plan_column(database, child_at(statement, 1U), &out_plan->column);
    }
    if (rc == MYLITE_OK) {
        const struct mylite_sql_ast_node *column_definition = child_at(statement, 1U);

        out_plan->adds_inline_primary_key =
            child_with_kind(column_definition, MYLITE_SQL_AST_INLINE_PRIMARY_KEY) != NULL;
        out_plan->adds_inline_unique_key =
            child_with_kind(column_definition, MYLITE_SQL_AST_INLINE_UNIQUE_KEY) != NULL;
    }
    if (rc == MYLITE_OK && out_plan->column.is_generated) {
        set_unsupported_error(
            database,
            "ALTER TABLE MODIFY COLUMN does not support generated columns"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = reject_auto_increment_column_definition(
            database,
            child_at(statement, 1U),
            "ALTER TABLE MODIFY COLUMN does not support AUTO_INCREMENT"
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_alter_table_modify_column_position(database, child_at(statement, 2U), out_plan);
    }
    if (rc == MYLITE_OK) {
        memcpy(
            out_plan->lookup_column_name,
            out_plan->column.name,
            sizeof(out_plan->lookup_column_name)
        );
        rc = resolve_alter_table_column_replacement_plan(database, out_plan);
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_modify_column_deinit(out_plan);
    }

    return rc;
}

static int plan_alter_table_change_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_modify_column *out_plan
) {
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_modify_column){0};
    out_plan->checks_duplicate_replacement = true;
    out_plan->unsupported_object_message =
        "ALTER TABLE CHANGE COLUMN supports only persistent base tables";
    out_plan->rowid_alias_message =
        "ALTER TABLE CHANGE COLUMN requires an unshadowed SQLite rowid alias";
    out_plan->integer_support_message =
        "ALTER TABLE CHANGE COLUMN supports only compatible baseline integer, character, "
        "text, binary string, and temporal column replacements";
    out_plan->row_count_overflow_message =
        "too many rows to validate for ALTER TABLE CHANGE COLUMN";
    out_plan->failure_message = "failed to change table column";

    rc = resolve_writable_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = copy_identifier_text(
            child_at(statement, 1U),
            out_plan->lookup_column_name,
            sizeof(out_plan->lookup_column_name),
            database
        );
    }
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->lookup_column_name)) {
        set_reserved_name_error(database, "column", out_plan->lookup_column_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = plan_column(database, child_at(statement, 2U), &out_plan->column);
    }
    if (rc == MYLITE_OK) {
        const struct mylite_sql_ast_node *column_definition = child_at(statement, 2U);

        out_plan->adds_inline_primary_key =
            child_with_kind(column_definition, MYLITE_SQL_AST_INLINE_PRIMARY_KEY) != NULL;
        out_plan->adds_inline_unique_key =
            child_with_kind(column_definition, MYLITE_SQL_AST_INLINE_UNIQUE_KEY) != NULL;
    }
    if (rc == MYLITE_OK && out_plan->column.is_generated) {
        set_unsupported_error(
            database,
            "ALTER TABLE CHANGE COLUMN does not support generated columns"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        const struct mylite_sql_ast_node *column_definition = child_at(statement, 2U);

        if (mylite_sql_ast_node_integer_type_is_serial_alias(child_at(column_definition, 1U)) != 0) {
            set_unsupported_error(
                database,
                "ALTER TABLE CHANGE COLUMN does not support AUTO_INCREMENT"
            );
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        rc = plan_alter_table_modify_column_position(database, child_at(statement, 3U), out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = resolve_alter_table_column_replacement_plan(database, out_plan);
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_modify_column_deinit(out_plan);
    }

    return rc;
}

static int plan_alter_table_modify_column_position(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *position,
    struct planned_alter_table_modify_column *out_plan
) {
    if (position == NULL) {
        return MYLITE_OK;
    }

    out_plan->has_position = true;
    if (position->kind == MYLITE_SQL_AST_COLUMN_POSITION_FIRST) {
        out_plan->position_first = true;
        return MYLITE_OK;
    }
    if (position->kind != MYLITE_SQL_AST_COLUMN_POSITION_AFTER) {
        set_runtime_error(database, "invalid ALTER TABLE column position");
        return MYLITE_ERROR;
    }

    if (copy_identifier_text(
            child_at(position, 0U),
            out_plan->after_column_name,
            sizeof(out_plan->after_column_name),
            database
        ) != MYLITE_OK) {
        return MYLITE_ERROR;
    }
    if (mylite_catalog_name_is_reserved(out_plan->after_column_name)) {
        set_reserved_name_error(database, "column", out_plan->after_column_name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}
