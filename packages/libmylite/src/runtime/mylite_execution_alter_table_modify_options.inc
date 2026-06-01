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
    if (rc == MYLITE_OK && out_plan->column.is_generated) {
        set_unsupported_error(
            database,
            "ALTER TABLE MODIFY COLUMN does not support generated columns"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = reject_inline_primary_key_column_definition(
            database,
            child_at(statement, 1U),
            "ALTER TABLE MODIFY COLUMN does not support PRIMARY KEY"
        );
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
    if (rc == MYLITE_OK && out_plan->column.is_generated) {
        set_unsupported_error(
            database,
            "ALTER TABLE CHANGE COLUMN does not support generated columns"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = reject_inline_primary_key_column_definition(
            database,
            child_at(statement, 2U),
            "ALTER TABLE CHANGE COLUMN does not support PRIMARY KEY"
        );
    }
    if (rc == MYLITE_OK) {
        rc = reject_auto_increment_column_definition(
            database,
            child_at(statement, 2U),
            "ALTER TABLE CHANGE COLUMN does not support AUTO_INCREMENT"
        );
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

static int plan_alter_table_set_default(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_set_default *out_plan
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t column_count = 0U;
    size_t column_index = 0U;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_set_default){0};
    rc = resolve_writable_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = copy_identifier_text(
            child_at(statement, 1U),
            column_name,
            sizeof(column_name),
            database
        );
    }
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(column_name)) {
        set_reserved_name_error(database, "column", column_name);
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
            "ALTER TABLE ALTER COLUMN SET DEFAULT supports only persistent base tables"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = load_table_columns(database, out_plan->table.table_id, &columns, &column_count);
    }
    if (rc == MYLITE_OK) {
        rc = find_column_index(columns, column_count, column_name, &column_index);
        if (rc != MYLITE_OK) {
            set_unknown_column_in_table_error(database, column_name, out_plan->table.name);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        out_plan->original_column = columns[column_index];
        planned_column_from_catalog_descriptor(
            &out_plan->original_column,
            child_at(statement, 2U),
            &out_plan->column
        );
        if (column_descriptor_is_text_family(&out_plan->original_column) ||
            column_descriptor_is_binary_blob_family(&out_plan->original_column)) {
            set_invalid_default_error(database, out_plan->original_column.name);
            rc = MYLITE_ERROR;
        } else {
            rc =
                validate_column_default(database, out_plan->column.default_node, &out_plan->column);
        }
    }
    if (rc == MYLITE_OK && planned_column_is_char_or_varchar(&out_plan->column) &&
        column_default_value_is_parenthesized_expression(out_plan->column.default_node)) {
        rc = finalize_planned_column_parenthesized_string_literal_default(
            database,
            &out_plan->column
        );
    } else if (rc == MYLITE_OK) {
        rc = finalize_planned_column_default(database, &out_plan->column);
    }
    if (rc == MYLITE_OK) {
        rc = collect_no_zero_date_default_warnings(
            database,
            columns,
            column_count,
            (struct zero_temporal_default_warning_filter){
                .skipped_column_id = out_plan->original_column.column_id,
            },
            &out_plan->zero_temporal_default_warnings
        );
    }

    free(columns);

    return rc;
}

static void planned_alter_table_set_default_deinit(struct planned_alter_table_set_default *plan) {
    if (plan == NULL) {
        return;
    }

    zero_temporal_default_warnings_deinit(&plan->zero_temporal_default_warnings);
    *plan = (struct planned_alter_table_set_default){0};
}

static int alter_table_set_default_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_set_default *plan
) {
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    int rc = mylite_catalog_begin_mutation(database, &mutation);

    if (rc == MYLITE_OK) {
        rc = alter_table_set_default_in_mutation(database, &mutation, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to set column default");
        mylite_catalog_rollback_mutation(database, &mutation);
        return rc;
    }

    return MYLITE_OK;
}

static int alter_table_set_default_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    const struct planned_alter_table_set_default *plan
) {
    int rc = mylite_catalog_replace_column_in_mutation(
        database,
        mutation,
        plan->table.table_id,
        plan->original_column.column_id,
        plan->original_column.name,
        plan->original_column.logical_type,
        plan->original_column.physical_type,
        plan->original_column.is_nullable,
        plan->original_column.is_visible,
        plan->original_column.is_auto_increment,
        plan->column.default_kind,
        plan->column.default_integer,
        plan->column.default_text,
        plan->original_column.on_update_current_timestamp,
        plan->original_column.character_set_name,
        plan->original_column.collation_name,
        plan->original_column.comment,
        plan->original_column.is_generated,
        plan->original_column.generated_kind,
        plan->original_column.generation_expression,
        plan->original_column.sqlite_generation_expression
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_update_table_identity_in_mutation(
            database,
            mutation,
            plan->table.table_id,
            plan->table.schema_id,
            plan->table.name,
            NULL
        );
    }
    return rc;
}

static int plan_alter_table_drop_default(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_drop_default *out_plan
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t column_count = 0U;
    size_t column_index = 0U;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_drop_default){0};
    rc = resolve_writable_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = copy_identifier_text(
            child_at(statement, 1U),
            column_name,
            sizeof(column_name),
            database
        );
    }
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(column_name)) {
        set_reserved_name_error(database, "column", column_name);
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
            "ALTER TABLE ALTER COLUMN DROP DEFAULT supports only persistent base tables"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = load_table_columns(database, out_plan->table.table_id, &columns, &column_count);
    }
    if (rc == MYLITE_OK) {
        rc = find_column_index(columns, column_count, column_name, &column_index);
        if (rc != MYLITE_OK) {
            set_unknown_column_in_table_error(database, column_name, out_plan->table.name);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        out_plan->column = columns[column_index];
    }

    free(columns);

    return rc;
}

static int alter_table_drop_default_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_drop_default *plan
) {
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    int rc = mylite_catalog_begin_mutation(database, &mutation);

    if (rc == MYLITE_OK) {
        rc = alter_table_drop_default_in_mutation(database, &mutation, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to drop column default");
        mylite_catalog_rollback_mutation(database, &mutation);
        return rc;
    }

    return MYLITE_OK;
}

static int alter_table_drop_default_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    const struct planned_alter_table_drop_default *plan
) {
    int rc = mylite_catalog_replace_column_in_mutation(
        database,
        mutation,
        plan->table.table_id,
        plan->column.column_id,
        plan->column.name,
        plan->column.logical_type,
        plan->column.physical_type,
        plan->column.is_nullable,
        plan->column.is_visible,
        plan->column.is_auto_increment,
        MYLITE_CATALOG_COLUMN_DEFAULT_NO_EXPLICIT,
        0,
        NULL,
        plan->column.on_update_current_timestamp,
        plan->column.character_set_name,
        plan->column.collation_name,
        plan->column.comment,
        plan->column.is_generated,
        plan->column.generated_kind,
        plan->column.generation_expression,
        plan->column.sqlite_generation_expression
    );

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_update_table_identity_in_mutation(
            database,
            mutation,
            plan->table.table_id,
            plan->table.schema_id,
            plan->table.name,
            NULL
        );
    }
    return rc;
}

static int plan_alter_table_column_visibility(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_column_visibility *out_plan
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t column_count = 0U;
    size_t column_index = 0U;
    size_t visible_count = 0U;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_column_visibility){0};
    out_plan->is_visible = mylite_sql_ast_node_column_visibility(statement) ==
                           MYLITE_SQL_AST_COLUMN_VISIBILITY_VISIBLE;
    rc = resolve_writable_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = copy_identifier_text(
            child_at(statement, 1U),
            column_name,
            sizeof(column_name),
            database
        );
    }
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(column_name)) {
        set_reserved_name_error(database, "column", column_name);
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
            "ALTER TABLE ALTER COLUMN SET VISIBLE/INVISIBLE supports only persistent base tables"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = load_table_columns(database, out_plan->table.table_id, &columns, &column_count);
    }
    if (rc == MYLITE_OK) {
        rc = find_column_index(columns, column_count, column_name, &column_index);
        if (rc != MYLITE_OK) {
            set_unknown_column_in_table_error(database, column_name, out_plan->table.name);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        visible_count = count_visible_columns(columns, column_count);
        if (!out_plan->is_visible && columns[column_index].is_visible && visible_count <= 1U) {
            set_must_have_visible_column_error(database);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        out_plan->column = columns[column_index];
    }

    free(columns);

    return rc;
}

static int alter_table_column_visibility_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_column_visibility *plan
) {
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    int rc = mylite_catalog_begin_mutation(database, &mutation);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_set_column_visibility_in_mutation(
            database,
            &mutation,
            plan->table.table_id,
            plan->column.column_id,
            plan->is_visible
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_update_table_identity_in_mutation(
            database,
            &mutation,
            plan->table.table_id,
            plan->table.schema_id,
            plan->table.name,
            NULL
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to set column visibility");
        mylite_catalog_rollback_mutation(database, &mutation);
        return rc;
    }

    return MYLITE_OK;
}

static int plan_alter_table_index_visibility(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_index_visibility *out_plan
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_index_info *indexes = NULL;
    char index_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t column_count = 0U;
    size_t index_count = 0U;
    size_t resolved_index = 0U;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_index_visibility){0};
    out_plan->is_visible = mylite_sql_ast_node_column_visibility(statement) ==
                           MYLITE_SQL_AST_COLUMN_VISIBILITY_VISIBLE;
    rc = resolve_writable_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc =
            copy_identifier_text(child_at(statement, 1U), index_name, sizeof(index_name), database);
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
            "ALTER TABLE ALTER INDEX supports only persistent base tables"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = load_table_columns(database, out_plan->table.table_id, &columns, &column_count);
    }
    if (rc == MYLITE_OK) {
        rc = load_table_index_infos(
            database,
            out_plan->table.table_id,
            columns,
            column_count,
            &indexes,
            &index_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = find_loaded_index_by_name(indexes, index_count, index_name, &resolved_index);
        if (rc != MYLITE_OK) {
            set_key_does_not_exist_in_table_error(
                database,
                index_name,
                out_plan->target.table_name
            );
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK &&
        indexes[resolved_index].index.kind == MYLITE_CATALOG_INDEX_KIND_PRIMARY) {
        set_primary_key_index_invisible_error(database);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK &&
        !loaded_index_kind_is_secondary_or_fulltext(indexes[resolved_index].index.kind)) {
        set_unsupported_error(database, "ALTER TABLE ALTER INDEX supports only secondary indexes");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        out_plan->index = indexes[resolved_index];
        indexes[resolved_index] = (struct loaded_index_info){0};
    }
    if (rc == MYLITE_OK &&
        mylite_sql_ast_node_alter_algorithm(statement) == MYLITE_SQL_AST_ALTER_ALGORITHM_COPY) {
        rc = read_show_table_status_row_count(database, &out_plan->table, &out_plan->affected_rows);
    }

    loaded_index_infos_deinit(&indexes, &index_count);
    free(columns);
    return rc;
}

static int alter_table_index_visibility_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_index_visibility *plan
) {
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    int rc = mylite_catalog_begin_mutation(database, &mutation);

    if (rc == MYLITE_OK) {
        rc = mylite_catalog_set_index_visibility_in_mutation(
            database,
            &mutation,
            plan->table.table_id,
            plan->index.index.index_id,
            plan->is_visible
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_update_table_identity_in_mutation(
            database,
            &mutation,
            plan->table.table_id,
            plan->table.schema_id,
            plan->table.name,
            NULL
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to set index visibility");
        mylite_catalog_rollback_mutation(database, &mutation);
        return rc;
    }

    return MYLITE_OK;
}

static void planned_alter_table_index_visibility_deinit(
    struct planned_alter_table_index_visibility *plan
) {
    if (plan == NULL) {
        return;
    }
    loaded_index_info_deinit(&plan->index);
    *plan = (struct planned_alter_table_index_visibility){0};
}

static int plan_alter_table_default_charset_collation(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_default_charset_collation *out_plan
) {
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_default_charset_collation){0};
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
            "ALTER TABLE DEFAULT CHARSET/COLLATE supports only persistent base tables"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        memcpy(
            out_plan->default_charset,
            out_plan->table.default_charset,
            strlen(out_plan->table.default_charset) + 1U
        );
        memcpy(
            out_plan->default_collation,
            out_plan->table.default_collation,
            strlen(out_plan->table.default_collation) + 1U
        );
        rc = validate_alter_table_default_charset_collation_options(
            database,
            child_at(statement, 1U)
        );
    }
    if (rc == MYLITE_OK) {
        rc = apply_alter_table_default_charset_collation_options(
            database,
            child_at(statement, 1U),
            out_plan
        );
    }
    if (rc == MYLITE_OK) {
        out_plan->changes_descriptor = false;
        if (strcmp(out_plan->default_charset, out_plan->table.default_charset) != 0) {
            out_plan->changes_descriptor = true;
        }
        if (strcmp(out_plan->default_collation, out_plan->table.default_collation) != 0) {
            out_plan->changes_descriptor = true;
        }
    }

    return rc;
}

static int apply_alter_table_default_charset_collation_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_options,
    struct planned_alter_table_default_charset_collation *plan
) {
    const struct mylite_sql_ast_node *table_option = NULL;
    bool has_charset_option = false;
    bool has_collation_option = false;
    int rc = MYLITE_OK;

    if (table_options == NULL || table_options->kind != MYLITE_SQL_AST_TABLE_OPTION_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    table_option = child_at(table_options, 0U);
    while (table_option != NULL) {
        if (table_option->kind == MYLITE_SQL_AST_TABLE_CHARSET_OPTION) {
            has_charset_option = true;
        } else if (table_option->kind == MYLITE_SQL_AST_TABLE_COLLATION_OPTION) {
            has_collation_option = true;
        }
        table_option = table_option->next_sibling;
    }
    table_option = child_at(table_options, 0U);
    while (rc == MYLITE_OK && table_option != NULL) {
        if (table_option->kind == MYLITE_SQL_AST_TABLE_CHARSET_OPTION) {
            rc = apply_table_charset_option(
                database,
                table_option,
                false,
                plan->default_charset,
                sizeof(plan->default_charset)
            );
        } else if (table_option->kind == MYLITE_SQL_AST_TABLE_COLLATION_OPTION) {
            rc = apply_table_collation_option(
                database,
                table_option,
                false,
                plan->default_collation,
                sizeof(plan->default_collation)
            );
        }
        table_option = table_option->next_sibling;
    }
    if (rc == MYLITE_OK && has_charset_option && !has_collation_option) {
        rc = copy_default_collation_for_charset(
            database,
            plan->default_charset,
            plan->default_collation,
            sizeof(plan->default_collation)
        );
    }

    return rc;
}

static int plan_alter_table_convert_character_set(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_convert_character_set *out_plan
) {
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_convert_character_set){0};
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
            "ALTER TABLE CONVERT TO CHARACTER SET supports only persistent base tables"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        memcpy(
            out_plan->default_charset,
            out_plan->table.default_charset,
            strlen(out_plan->table.default_charset) + 1U
        );
        memcpy(
            out_plan->default_collation,
            out_plan->table.default_collation,
            strlen(out_plan->table.default_collation) + 1U
        );
        rc = apply_alter_table_convert_character_set_options(
            database,
            child_at(statement, 1U),
            out_plan
        );
    }
    if (rc == MYLITE_OK) {
        rc = load_table_columns(
            database,
            out_plan->table.table_id,
            &out_plan->columns,
            &out_plan->column_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_alter_table_convert_character_set_columns(database, out_plan);
    }
    if (rc == MYLITE_OK) {
        out_plan->changes_descriptor =
            (strcmp(out_plan->default_charset, out_plan->table.default_charset) != 0 ||
             strcmp(out_plan->default_collation, out_plan->table.default_collation) != 0) != 0;
        for (size_t index = 0U; !out_plan->changes_descriptor && index < out_plan->column_count;
             ++index) {
            if (alter_table_convert_column_needs_explicit_metadata_update(
                    &out_plan->columns[index],
                    out_plan
                )) {
                out_plan->changes_descriptor = true;
            }
        }
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_convert_character_set_deinit(out_plan);
    }

    return rc;
}

static int apply_alter_table_convert_character_set_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_options,
    struct planned_alter_table_convert_character_set *plan
) {
    const struct mylite_sql_ast_node *charset_option = NULL;
    const struct mylite_sql_ast_node *collation_option = NULL;
    char charset_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char collation_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = MYLITE_OK;

    if (table_options == NULL || table_options->kind != MYLITE_SQL_AST_TABLE_OPTION_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    charset_option = child_at(table_options, 0U);
    collation_option = child_at(table_options, 1U);
    if (charset_option == NULL || charset_option->kind != MYLITE_SQL_AST_TABLE_CHARSET_OPTION ||
        child_at(table_options, 2U) != NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (table_charset_option_is_default_keyword(charset_option)) {
        if (collation_option != NULL) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        rc = apply_alter_table_convert_character_set_default(database, plan);
    } else {
        rc = copy_table_charset_option_name(
            database,
            charset_option,
            charset_name,
            sizeof(charset_name)
        );
        if (rc == MYLITE_OK) {
            rc = copy_normalized_charset_name(
                database,
                charset_name,
                plan->default_charset,
                sizeof(plan->default_charset)
            );
        }
        if (rc == MYLITE_OK && collation_option == NULL) {
            rc = copy_default_collation_for_charset(
                database,
                plan->default_charset,
                plan->default_collation,
                sizeof(plan->default_collation)
            );
        }
    }
    if (rc == MYLITE_OK && collation_option != NULL) {
        rc = copy_table_collation_option_name(
            database,
            collation_option,
            collation_name,
            sizeof(collation_name)
        );
        if (rc == MYLITE_OK) {
            rc = copy_normalized_collation_name(
                database,
                collation_name,
                plan->default_collation,
                sizeof(plan->default_collation)
            );
        }
    }
    if (rc == MYLITE_OK && !text_equals_ascii_case_insensitive(plan->default_charset, "utf8mb4")) {
        set_unsupported_error(
            database,
            "ALTER TABLE CONVERT TO CHARACTER SET supports only utf8mb4 target character set"
        );
        return MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = validate_table_charset_collation_option_values_with_binary(
            database,
            true,
            plan->default_charset,
            (collation_option != NULL || table_charset_option_is_default_keyword(charset_option)) !=
                0,
            plan->default_collation,
            true
        );
    }

    return rc;
}

static int apply_alter_table_convert_character_set_default(
    struct mylite_db *database,
    struct planned_alter_table_convert_character_set *plan
) {
    char charset_buffer[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char collation_buffer[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    const char *charset_name = NULL;
    const char *collation_name = NULL;
    int rc = database_character_set_system_variable_value(
        database,
        false,
        charset_buffer,
        sizeof(charset_buffer),
        &charset_name
    );

    if (rc == MYLITE_OK) {
        rc = database_collation_system_variable_value(
            database,
            false,
            collation_buffer,
            sizeof(collation_buffer),
            &collation_name
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (charset_name == NULL || strlen(charset_name) >= sizeof(plan->default_charset) ||
        collation_name == NULL || strlen(collation_name) >= sizeof(plan->default_collation)) {
        set_runtime_error(database, "database charset descriptor is too small");
        return MYLITE_ERROR;
    }

    memcpy(plan->default_charset, charset_name, strlen(charset_name) + 1U);
    memcpy(plan->default_collation, collation_name, strlen(collation_name) + 1U);
    return MYLITE_OK;
}

static int validate_alter_table_convert_character_set_columns(
    struct mylite_db *database,
    const struct planned_alter_table_convert_character_set *plan
) {
    for (size_t index = 0U; index < plan->column_count; ++index) {
        const struct mylite_catalog_column_descriptor *column = &plan->columns[index];
        const char *effective_charset = NULL;

        if (column_descriptor_is_national_char_or_varchar(column)) {
            set_unsupported_error(
                database,
                "ALTER TABLE CONVERT TO CHARACTER SET does not yet support national character "
                "columns"
            );
            return MYLITE_ERROR;
        }
        if (column_descriptor_is_enum(column) || column_descriptor_is_set(column)) {
            set_unsupported_error(
                database,
                "ALTER TABLE CONVERT TO CHARACTER SET does not yet support ENUM or SET columns"
            );
            return MYLITE_ERROR;
        }
        if (column_descriptor_is_binary_string_family(column) ||
            !alter_table_convert_column_participates(column)) {
            continue;
        }

        effective_charset = column_effective_character_set_name(&plan->table, column);
        if (effective_charset == NULL ||
            !text_equals_ascii_case_insensitive(effective_charset, "utf8mb4")) {
            set_unsupported_error(
                database,
                "ALTER TABLE CONVERT TO CHARACTER SET does not yet support cross-character-set "
                "conversion"
            );
            return MYLITE_ERROR;
        }
    }

    return MYLITE_OK;
}

static bool table_charset_option_is_default_keyword(
    const struct mylite_sql_ast_node *charset_option
) {
    const struct mylite_sql_ast_node *name_node = child_at(charset_option, 0U);
    char keyword[sizeof("DEFAULT")];

    if (name_node == NULL || name_node->kind != MYLITE_SQL_AST_IDENTIFIER ||
        name_node->span.text == NULL || name_node->span.length != sizeof("DEFAULT") - 1U) {
        return false;
    }

    memcpy(keyword, name_node->span.text, name_node->span.length);
    keyword[name_node->span.length] = '\0';
    return text_equals_ascii_case_insensitive(keyword, "DEFAULT");
}

static bool alter_table_convert_column_participates(
    const struct mylite_catalog_column_descriptor *column
) {
    return column_descriptor_is_string_family(column);
}

static bool alter_table_convert_column_needs_explicit_metadata_update(
    const struct mylite_catalog_column_descriptor *column,
    const struct planned_alter_table_convert_character_set *plan
) {
    if (!alter_table_convert_column_participates(column)) {
        return false;
    }
    if (column->character_set_name[0] == '\0' && column->collation_name[0] == '\0') {
        return false;
    }
    return (strcmp(column->character_set_name, plan->default_charset) != 0 ||
            strcmp(column->collation_name, plan->default_collation) != 0) != 0;
}

static void planned_alter_table_convert_character_set_deinit(
    struct planned_alter_table_convert_character_set *plan
) {
    if (plan == NULL) {
        return;
    }

    free(plan->columns);
    *plan = (struct planned_alter_table_convert_character_set){0};
}

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
        rc = reject_primary_key_table_alter(
            database,
            out_plan->table.table_id,
            "ALTER TABLE ORDER BY does not yet support primary-key tables"
        );
    }
    if (rc == MYLITE_OK) {
        rc = reject_secondary_index_table_alter(
            database,
            out_plan->table.table_id,
            "ALTER TABLE ORDER BY does not yet support secondary-index tables"
        );
    }
    if (rc == MYLITE_OK) {
        rc = reject_check_constraint_table_alter(
            database,
            out_plan->table.table_id,
            "ALTER TABLE ORDER BY does not yet support CHECK-constrained tables"
        );
    }
    if (rc == MYLITE_OK) {
        rc = load_table_columns(
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
        rc = reject_primary_key_table_alter(
            database,
            out_plan->table.table_id,
            "ALTER TABLE FORCE does not yet support primary-key tables"
        );
    }
    if (rc == MYLITE_OK) {
        rc = reject_secondary_index_table_alter(
            database,
            out_plan->table.table_id,
            "ALTER TABLE FORCE does not yet support secondary-index tables"
        );
    }
    if (rc == MYLITE_OK) {
        rc = reject_check_constraint_table_alter(
            database,
            out_plan->table.table_id,
            "ALTER TABLE FORCE does not yet support CHECK-constrained tables"
        );
    }
    if (rc == MYLITE_OK) {
        rc = load_table_columns(
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

static void planned_column_from_catalog_descriptor(
    const struct mylite_catalog_column_descriptor *descriptor,
    const struct mylite_sql_ast_node *default_node,
    struct planned_column *out_column
) {
    *out_column = (struct planned_column){0};
    snprintf(out_column->name, sizeof(out_column->name), "%s", descriptor->name);
    snprintf(
        out_column->logical_type_storage,
        sizeof(out_column->logical_type_storage),
        "%s",
        descriptor->logical_type
    );
    snprintf(
        out_column->physical_type_storage,
        sizeof(out_column->physical_type_storage),
        "%s",
        descriptor->physical_type
    );
    out_column->logical_type = out_column->logical_type_storage;
    out_column->physical_type = out_column->physical_type_storage;
    out_column->is_nullable = descriptor->is_nullable;
    out_column->is_visible = descriptor->is_visible;
    out_column->is_auto_increment = descriptor->is_auto_increment;
    out_column->on_update_current_timestamp = descriptor->on_update_current_timestamp;
    out_column->default_node = default_node;
    out_column->default_kind = descriptor->default_kind;
    out_column->default_integer = descriptor->default_integer;
    snprintf(
        out_column->default_text,
        sizeof(out_column->default_text),
        "%s",
        descriptor->default_text
    );
    snprintf(
        out_column->character_set_name,
        sizeof(out_column->character_set_name),
        "%s",
        descriptor->character_set_name
    );
    snprintf(
        out_column->collation_name,
        sizeof(out_column->collation_name),
        "%s",
        descriptor->collation_name
    );
    snprintf(out_column->comment, sizeof(out_column->comment), "%s", descriptor->comment);
    out_column->is_generated = descriptor->is_generated;
    out_column->generated_kind = descriptor->generated_kind;
    snprintf(
        out_column->generation_expression,
        sizeof(out_column->generation_expression),
        "%s",
        descriptor->generation_expression
    );
    snprintf(
        out_column->sqlite_generation_expression,
        sizeof(out_column->sqlite_generation_expression),
        "%s",
        descriptor->sqlite_generation_expression
    );
}

static int resolve_alter_table_column_replacement_plan(
    struct mylite_db *database,
    struct planned_alter_table_modify_column *out_plan
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    size_t column_count = 0U;
    size_t column_index = 0U;
    int rc = mylite_catalog_read_table_by_name(
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
        return MYLITE_ERROR;
    }
    if (out_plan->table.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(database, out_plan->unsupported_object_message);
        return MYLITE_ERROR;
    }
    rc = normalize_text_length_argument_for_default_charset(
        database,
        &out_plan->column,
        out_plan->table.default_charset
    );
    if (rc == MYLITE_OK) {
        rc = apply_default_binary_charset_to_column(
            database,
            out_plan->table.default_charset,
            &out_plan->column
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_column_default(database, out_plan->column.default_node, &out_plan->column);
    }
    if (rc == MYLITE_OK) {
        rc = finalize_planned_column_default(database, &out_plan->column);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = reject_check_constraint_table_alter(
        database,
        out_plan->table.table_id,
        "ALTER TABLE column replacement does not yet support CHECK-constrained tables"
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = reject_foreign_key_table_alter(
        database,
        out_plan->table.table_id,
        "ALTER TABLE column replacement does not yet support foreign-key constrained tables"
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = load_table_columns(database, out_plan->table.table_id, &columns, &column_count);
    if (rc == MYLITE_OK) {
        rc = find_column_index(columns, column_count, out_plan->lookup_column_name, &column_index);
        if (rc != MYLITE_OK) {
            set_unknown_column_in_table_error(
                database,
                out_plan->lookup_column_name,
                out_plan->table.name
            );
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && out_plan->checks_duplicate_replacement) {
        size_t replacement_index = 0U;

        if (find_column_index(columns, column_count, out_plan->column.name, &replacement_index) ==
                MYLITE_OK &&
            replacement_index != column_index) {
            set_duplicate_column_error(database, columns[replacement_index].name);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        out_plan->original_column = columns[column_index];
        out_plan->column_index = column_index;
        out_plan->column_count = column_count;
        rc = resolve_alter_table_modify_column_position(database, columns, column_count, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = collect_no_zero_date_default_warnings(
            database,
            columns,
            column_count,
            (struct zero_temporal_default_warning_filter){
                .skipped_column_id = out_plan->original_column.column_id,
            },
            &out_plan->zero_temporal_default_warnings
        );
    }
    if (rc == MYLITE_OK) {
        rc = load_table_index_infos(
            database,
            out_plan->table.table_id,
            columns,
            column_count,
            &out_plan->indexes,
            &out_plan->index_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = apply_alter_table_modify_primary_key_nullability(database, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = validate_alter_table_modify_indexes(database, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = complete_alter_table_modify_column_plan(database, columns, out_plan);
    }

    free(columns);

    return rc;
}

static int apply_alter_table_modify_primary_key_nullability(
    struct mylite_db *database,
    struct planned_alter_table_modify_column *out_plan
) {
    for (size_t index = 0U; index < out_plan->index_count; ++index) {
        const struct loaded_index_info *key = &out_plan->indexes[index];

        if (key->index.kind != MYLITE_CATALOG_INDEX_KIND_PRIMARY ||
            !loaded_index_contains_column_id(key, out_plan->original_column.column_id)) {
            continue;
        }
        if (out_plan->column.nullability == MYLITE_SQL_AST_NULLABILITY_NULL) {
            set_primary_key_part_null_error(database);
            return MYLITE_ERROR;
        }
        if (out_plan->column.nullability == MYLITE_SQL_AST_NULLABILITY_UNSPECIFIED) {
            out_plan->column.is_nullable = false;
        }
        return MYLITE_OK;
    }

    return MYLITE_OK;
}

static int validate_alter_table_modify_indexes(
    struct mylite_db *database,
    struct planned_alter_table_modify_column *plan
) {
    int rc = refresh_alter_table_modify_index_parts(plan);

    for (size_t index = 0U; rc == MYLITE_OK && index < plan->index_count; ++index) {
        const struct loaded_index_info *key = &plan->indexes[index];

        if (key->index.kind == MYLITE_CATALOG_INDEX_KIND_PRIMARY) {
            rc = validate_alter_table_modify_primary_index(database, key);
        } else if (key->index.kind == MYLITE_CATALOG_INDEX_KIND_SECONDARY) {
            rc = validate_loaded_index_part_list(
                database,
                key->parts,
                key->part_count,
                key->index.is_unique
            );
        } else if (key->index.kind == MYLITE_CATALOG_INDEX_KIND_FULLTEXT) {
            rc = validate_alter_table_modify_fulltext_index(database, key);
        } else if (key->index.kind == MYLITE_CATALOG_INDEX_KIND_SPATIAL) {
            rc = validate_alter_table_modify_spatial_index(database, key);
        }
    }

    return rc;
}

static int refresh_alter_table_modify_index_parts(struct planned_alter_table_modify_column *plan) {
    for (size_t index = 0U; index < plan->index_count; ++index) {
        struct loaded_index_info *key = &plan->indexes[index];

        for (size_t part_index = 0U; part_index < key->part_count; ++part_index) {
            struct loaded_index_part *part = &key->parts[part_index];

            if (part->index_column.column_id == plan->original_column.column_id) {
                make_modify_target_descriptor(plan, &part->column);
            }
        }
    }

    return MYLITE_OK;
}

static int validate_alter_table_modify_primary_index(
    struct mylite_db *database,
    const struct loaded_index_info *index
) {
    uint64_t total = 0U;

    if (index == NULL || index->part_count == 0U) {
        set_runtime_error(database, "invalid primary-key descriptor");
        return MYLITE_ERROR;
    }
    for (size_t part_index = 0U; part_index < index->part_count; ++part_index) {
        const struct loaded_index_part *part = &index->parts[part_index];
        uint64_t part_bytes = 0U;
        int rc = MYLITE_OK;

        if (part->column.is_nullable) {
            set_primary_key_part_null_error(database);
            return MYLITE_ERROR;
        }
        rc = column_descriptor_primary_key_part_key_bytes(database, &part->column, &part_bytes);
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (total > UINT64_MAX - part_bytes) {
            set_key_too_long_error(database, innodb_max_key_length_bytes);
            return MYLITE_ERROR;
        }
        total += part_bytes;
        if (total > innodb_max_key_length_bytes) {
            set_key_too_long_error(database, innodb_max_key_length_bytes);
            return MYLITE_ERROR;
        }
    }

    return MYLITE_OK;
}

static int validate_alter_table_modify_fulltext_index(
    struct mylite_db *database,
    const struct loaded_index_info *index
) {
    if (index == NULL || index->part_count == 0U) {
        set_runtime_error(database, "invalid FULLTEXT index descriptor");
        return MYLITE_ERROR;
    }
    for (size_t part_index = 0U; part_index < index->part_count; ++part_index) {
        struct planned_column column = {0};

        planned_column_from_catalog_descriptor(&index->parts[part_index].column, NULL, &column);
        int rc = validate_fulltext_index_column(database, &column);
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return MYLITE_OK;
}

static int validate_alter_table_modify_spatial_index(
    struct mylite_db *database,
    const struct loaded_index_info *index
) {
    if (index == NULL || index->part_count != 1U) {
        set_runtime_error(database, "invalid SPATIAL index descriptor");
        return MYLITE_ERROR;
    }

    return validate_alter_table_add_spatial_column(database, &index->parts[0].column);
}

static int resolve_alter_table_modify_column_position(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    struct planned_alter_table_modify_column *out_plan
) {
    size_t after_column_index = 0U;
    int rc = MYLITE_OK;

    out_plan->target_column_index = out_plan->column_index;
    if (!out_plan->has_position) {
        return MYLITE_OK;
    }
    if (out_plan->position_first) {
        out_plan->target_column_index = 0U;
        out_plan->changes_position = out_plan->target_column_index != out_plan->column_index;
        return MYLITE_OK;
    }

    rc = find_column_index(columns, column_count, out_plan->after_column_name, &after_column_index);
    if (rc != MYLITE_OK || after_column_index == out_plan->column_index) {
        set_unknown_column_in_table_error(
            database,
            out_plan->after_column_name,
            out_plan->table.name
        );
        return MYLITE_ERROR;
    }

    if (after_column_index < out_plan->column_index) {
        out_plan->target_column_index = after_column_index + 1U;
    } else {
        out_plan->target_column_index = after_column_index;
    }
    out_plan->changes_position = out_plan->target_column_index != out_plan->column_index;
    return MYLITE_OK;
}

static int complete_alter_table_modify_column_plan(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    struct planned_alter_table_modify_column *out_plan
) {
    bool supports_char_varchar_replacement = modify_column_char_varchar_replacement_supported(
        &out_plan->original_column,
        &out_plan->column
    );
    bool supports_text_family_replacement = modify_column_text_family_replacement_supported(
        &out_plan->original_column,
        &out_plan->column
    );
    bool supports_binary_string_replacement = modify_column_binary_string_replacement_supported(
        &out_plan->original_column,
        &out_plan->column
    );
    bool supports_temporal_replacement =
        modify_column_temporal_replacement_supported(&out_plan->original_column, &out_plan->column);
    int rc = MYLITE_OK;

    if (!supports_char_varchar_replacement && !supports_text_family_replacement &&
        !supports_binary_string_replacement && !supports_temporal_replacement &&
        (column_descriptor_is_string_family(&out_plan->original_column) ||
         column_descriptor_is_binary_string_family(&out_plan->original_column) ||
         column_descriptor_is_json(&out_plan->original_column) ||
         column_descriptor_is_bit(&out_plan->original_column) ||
         column_descriptor_is_decimal(&out_plan->original_column) ||
         column_descriptor_is_approximate(&out_plan->original_column) ||
         column_descriptor_is_date(&out_plan->original_column) ||
         column_descriptor_is_time(&out_plan->original_column) ||
         column_descriptor_is_datetime(&out_plan->original_column) ||
         column_descriptor_is_timestamp(&out_plan->original_column) ||
         column_descriptor_is_year(&out_plan->original_column) ||
         column_descriptor_is_enum(&out_plan->original_column) ||
         column_descriptor_is_set(&out_plan->original_column) ||
         column_descriptor_is_spatial(&out_plan->original_column) ||
         planned_column_is_string_family(&out_plan->column) ||
         planned_column_is_binary_string_family(&out_plan->column) ||
         planned_column_is_json(&out_plan->column) || planned_column_is_bit(&out_plan->column) ||
         planned_column_is_enum(&out_plan->column) || planned_column_is_set(&out_plan->column) ||
         planned_column_is_decimal(&out_plan->column) ||
         planned_column_is_approximate(&out_plan->column) ||
         planned_column_is_date(&out_plan->column) || planned_column_is_time(&out_plan->column) ||
         planned_column_is_datetime(&out_plan->column) ||
         planned_column_is_timestamp(&out_plan->column) ||
         planned_column_is_year(&out_plan->column) ||
         planned_column_is_spatial(&out_plan->column))) {
        set_unsupported_error(database, out_plan->integer_support_message);
        return MYLITE_ERROR;
    }
    if (modify_column_definition_matches(&out_plan->original_column, &out_plan->column)) {
        if (modify_column_name_matches(&out_plan->original_column, &out_plan->column)) {
            if (out_plan->changes_position) {
                goto rebuild_column;
            }
            if (out_plan->original_column.is_visible) {
                out_plan->is_noop = true;
            } else {
                out_plan->is_metadata_only = true;
            }
            return MYLITE_OK;
        }
        if (out_plan->changes_position) {
            goto rebuild_column;
        }
        out_plan->is_metadata_only = true;
        return MYLITE_OK;
    }
    if (!out_plan->changes_position &&
        modify_column_metadata_only_replacement(&out_plan->original_column, &out_plan->column)) {
        out_plan->is_metadata_only = true;
        return MYLITE_OK;
    }

rebuild_column:
    if (!modify_column_type_matches(&out_plan->original_column, &out_plan->column)) {
        out_plan->reports_rebuild_row_count = true;
    }
    rc = choose_sqlite_rowid_alias(
        database,
        columns,
        out_plan->column_count,
        out_plan->rowid_alias_message,
        &out_plan->rowid_alias
    );
    if (rc == MYLITE_OK) {
        rc = collect_modify_column_rebuild_columns(
            database,
            columns,
            out_plan->column_count,
            out_plan
        );
    }

    return rc;
}

static bool modify_column_definition_matches(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
) {
    if (strcmp(original_column->logical_type, replacement_column->logical_type) != 0) {
        return false;
    }
    if (strcmp(original_column->physical_type, replacement_column->physical_type) != 0) {
        return false;
    }
    if (original_column->is_nullable != replacement_column->is_nullable) {
        return false;
    }
    if (original_column->on_update_current_timestamp !=
        replacement_column->on_update_current_timestamp) {
        return false;
    }
    if (strcmp(original_column->character_set_name, replacement_column->character_set_name) != 0) {
        return false;
    }
    if (strcmp(original_column->collation_name, replacement_column->collation_name) != 0) {
        return false;
    }
    if (strcmp(original_column->comment, replacement_column->comment) != 0) {
        return false;
    }
    if (original_column->default_kind != replacement_column->default_kind) {
        return false;
    }
    if (original_column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER &&
        original_column->default_integer != replacement_column->default_integer) {
        return false;
    }
    if (original_column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER_EXPRESSION &&
        original_column->default_integer != replacement_column->default_integer) {
        return false;
    }
    if ((original_column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_DECIMAL ||
         original_column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT ||
         original_column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT_EXPRESSION ||
         original_column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_BINARY ||
         column_default_kind_is_expression(original_column->default_kind)) &&
        strcmp(original_column->default_text, replacement_column->default_text) != 0) {
        return false;
    }

    return true;
}

static bool modify_column_type_matches(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
) {
    if (strcmp(original_column->logical_type, replacement_column->logical_type) != 0) {
        return false;
    }
    if (strcmp(original_column->physical_type, replacement_column->physical_type) != 0) {
        return false;
    }

    return true;
}

static bool modify_column_metadata_only_replacement(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
) {
    if (strcmp(original_column->physical_type, replacement_column->physical_type) != 0) {
        return false;
    }
    if (original_column->is_nullable != replacement_column->is_nullable) {
        return false;
    }
    if (!modify_column_integer_value_domain_matches(original_column, replacement_column)) {
        return false;
    }

    return true;
}

static bool modify_column_name_matches(
    const struct mylite_catalog_column_descriptor *original_column,
    const struct planned_column *replacement_column
) {
    if (strcmp(original_column->name, replacement_column->name) != 0) {
        return false;
    }

    return true;
}

static void planned_alter_table_modify_column_deinit(
    struct planned_alter_table_modify_column *plan
) {
    if (plan == NULL) {
        return;
    }

    free(plan->columns);
    zero_temporal_default_warnings_deinit(&plan->zero_temporal_default_warnings);
    loaded_index_infos_deinit(&plan->indexes, &plan->index_count);
    *plan = (struct planned_alter_table_modify_column){0};
}

static int alter_table_modify_column_from_plan(
    struct mylite_db *database,
    struct planned_alter_table_modify_column *plan
) {
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    int64_t validated_row_count = 0;
    int rc = MYLITE_OK;

    if (plan->is_noop) {
        return MYLITE_OK;
    }

    rc = mylite_catalog_begin_mutation(database, &mutation);
    if (rc == MYLITE_OK && !plan->is_metadata_only) {
        rc = validate_modify_column_existing_rows(database, plan, &validated_row_count);
        if (rc == MYLITE_OK && plan->reports_rebuild_row_count) {
            plan->affected_rows = validated_row_count;
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_replace_column_in_mutation(
            database,
            &mutation,
            plan->table.table_id,
            plan->original_column.column_id,
            plan->column.name,
            plan->column.logical_type,
            plan->column.physical_type,
            plan->column.is_nullable,
            true,
            plan->column.is_auto_increment,
            plan->column.default_kind,
            plan->column.default_integer,
            plan->column.default_text,
            plan->column.on_update_current_timestamp,
            plan->column.character_set_name,
            plan->column.collation_name,
            plan->column.comment,
            plan->column.is_generated,
            plan->column.generated_kind,
            plan->column.generation_expression,
            plan->column.sqlite_generation_expression
        );
    }
    if (rc == MYLITE_OK && plan->changes_position) {
        const struct mylite_catalog_column_reorder reorder = {
            .table_id = plan->table.table_id,
            .columns = plan->columns,
            .column_count = plan->column_count,
            .metadata_replaced_column_id = plan->original_column.column_id,
        };

        rc = mylite_catalog_reorder_columns_in_mutation(database, &mutation, &reorder);
    }
    if (rc == MYLITE_OK && plan->is_metadata_only) {
        if (!modify_column_name_matches(&plan->original_column, &plan->column)) {
            struct planned_alter_table_rename_column rename_plan = {
                .target = plan->target,
                .table = plan->table,
                .column = plan->original_column,
                .is_noop = false,
            };

            memcpy(
                rename_plan.new_column_name,
                plan->column.name,
                sizeof(rename_plan.new_column_name)
            );
            rc = execute_physical_alter_table_rename_column(database, &rename_plan);
        }
    } else if (rc == MYLITE_OK) {
        rc = execute_physical_alter_table_modify_column(database, plan, &mutation);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_update_table_identity_in_mutation(
            database,
            &mutation,
            plan->table.table_id,
            plan->table.schema_id,
            plan->table.name,
            NULL
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, plan->failure_message);
        mylite_catalog_rollback_mutation(database, &mutation);
        return rc;
    }

    if (!plan->is_metadata_only ||
        !modify_column_name_matches(&plan->original_column, &plan->column)) {
        ++database->session.sqlite_schema_generation;
    }

    return MYLITE_OK;
}

static int collect_modify_column_rebuild_columns(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    struct planned_alter_table_modify_column *out_plan
) {
    struct mylite_catalog_column_descriptor *rebuild_columns = NULL;

    if (column_count > SIZE_MAX / sizeof(*rebuild_columns)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rebuild_columns = calloc(column_count, sizeof(*rebuild_columns));
    if (rebuild_columns == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    if (!out_plan->changes_position) {
        for (size_t index = 0U; index < column_count; ++index) {
            rebuild_columns[index] = columns[index];
            if (index == out_plan->column_index) {
                make_modify_target_descriptor(out_plan, &rebuild_columns[index]);
            }
        }
    } else {
        size_t source_index = 0U;

        for (size_t index = 0U; index < column_count; ++index) {
            if (index == out_plan->target_column_index) {
                make_modify_target_descriptor(out_plan, &rebuild_columns[index]);
            } else {
                while (source_index < column_count && source_index == out_plan->column_index) {
                    ++source_index;
                }
                if (source_index >= column_count) {
                    free(rebuild_columns);
                    set_runtime_error(database, "failed to plan column reorder");
                    return MYLITE_ERROR;
                }
                rebuild_columns[index] = columns[source_index];
                ++source_index;
            }
        }
    }

    out_plan->columns = rebuild_columns;
    return MYLITE_OK;
}

static int validate_modify_column_existing_rows(
    struct mylite_db *database,
    const struct planned_alter_table_modify_column *plan,
    int64_t *out_row_count
) {
    sqlite3_stmt *statement = NULL;
    struct mylite_catalog_column_descriptor target_column = {0};
    char *sql = NULL;
    size_t row_number = 0U;
    int sqlite_rc = SQLITE_OK;
    int rc = build_alter_table_modify_validation_sql(plan, &sql);

    *out_row_count = 0;
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    make_modify_target_descriptor(plan, &target_column);
    while (rc == MYLITE_OK && (sqlite_rc = sqlite3_step(statement)) == SQLITE_ROW) {
        int sqlite_type = sqlite3_column_type(statement, 0);

        if (row_number >= (size_t)INT64_MAX) {
            set_runtime_error(database, plan->row_count_overflow_message);
            rc = MYLITE_ERROR;
            break;
        }
        ++row_number;
        if (sqlite_type == SQLITE_NULL) {
            rc = validate_modify_column_null_value(database, &target_column, row_number);
        } else if (sqlite_type == SQLITE_INTEGER) {
            rc = validate_existing_integer_for_column(
                database,
                (int64_t)sqlite3_column_int64(statement, 0),
                &target_column,
                row_number,
                plan->integer_support_message
            );
        } else if (
            column_descriptor_is_binary_string_family(&target_column) &&
            (sqlite_type == SQLITE_TEXT || sqlite_type == SQLITE_BLOB)
        ) {
            rc = validate_existing_binary_bytes_for_column(
                database,
                sqlite3_column_blob(statement, 0),
                sqlite3_column_bytes(statement, 0),
                &target_column,
                row_number
            );
        } else if (sqlite_type == SQLITE_TEXT) {
            rc = validate_existing_text_for_column(
                database,
                sqlite3_column_text(statement, 0),
                sqlite3_column_bytes(statement, 0),
                &target_column,
                row_number
            );
        } else {
            set_physical_sqlite_row_error(database);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && sqlite_rc != SQLITE_DONE) {
        rc = mylite_sqlite_status_to_mylite(sqlite_rc);
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);

    if (rc == MYLITE_OK) {
        *out_row_count = (int64_t)row_number;
    } else if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    } else if (mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) == MYLITE_OK) {
        set_physical_sqlite_row_error(database);
        rc = MYLITE_ERROR;
    }

    return rc;
}

static int validate_modify_column_null_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number
) {
    if (target_column->is_nullable) {
        return MYLITE_OK;
    }
    if (column_descriptor_is_text_family(target_column)) {
        set_data_truncated_error(database, target_column->name, row_number);
    } else {
        set_invalid_use_of_null_error(database);
    }

    return MYLITE_ERROR;
}

static int validate_existing_integer_for_column(
    struct mylite_db *database,
    int64_t value,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    const char *unsupported_message
) {
    struct integer_column_range range = {0};
    int rc = integer_range_for_column(database, column, unsupported_message, &range);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (value < 0) {
        const uint64_t int64_min_magnitude = (uint64_t)INT64_MAX + 1U;
        uint64_t magnitude = value == INT64_MIN ? int64_min_magnitude : (uint64_t)(-value);

        if (range.negative_abs_max == 0U || magnitude > range.negative_abs_max) {
            set_out_of_range_error(database, column->name, row_number);
            return MYLITE_ERROR;
        }
        return MYLITE_OK;
    }

    if ((uint64_t)value > range.positive_max) {
        set_out_of_range_error(database, column->name, row_number);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_existing_text_for_column(
    struct mylite_db *database,
    const unsigned char *text,
    int byte_count,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number
) {
    if (text == NULL || byte_count < 0) {
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_varchar(column)) {
        return validate_varchar_text(
            database,
            column,
            (struct varchar_text_validation){
                .text = (const char *)text,
                .text_length = (size_t)byte_count,
                .row_number = row_number,
            }
        );
    }
    if (column_descriptor_is_char(column)) {
        return validate_canonical_char_text(
            database,
            column,
            (struct varchar_text_validation){
                .text = (const char *)text,
                .text_length = (size_t)byte_count,
                .row_number = row_number,
            }
        );
    }
    if (column_descriptor_is_text_family(column)) {
        return validate_text_family_text(
            database,
            column,
            (struct varchar_text_validation){
                .text = (const char *)text,
                .text_length = (size_t)byte_count,
                .row_number = row_number,
            }
        );
    }
    if (column_descriptor_is_datetime(column)) {
        if (predicate_datetime_text_admitted(database, (const char *)text, (size_t)byte_count)) {
            return MYLITE_OK;
        }
        set_incorrect_datetime_value_error(database, (const char *)text, column->name, row_number);
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_timestamp(column)) {
        if (predicate_timestamp_text_admitted(database, (const char *)text, (size_t)byte_count)) {
            return MYLITE_OK;
        }
        set_incorrect_datetime_value_error(database, (const char *)text, column->name, row_number);
        return MYLITE_ERROR;
    }

    set_physical_sqlite_row_error(database);
    return MYLITE_ERROR;
}

static int validate_existing_binary_bytes_for_column(
    struct mylite_db *database,
    const void *bytes,
    int byte_count,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number
) {
    char *copy = NULL;
    size_t copy_length = 0U;
    int rc = MYLITE_OK;

    if ((bytes == NULL && byte_count != 0) || byte_count < 0) {
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }
    copy_length = (size_t)byte_count;
    copy = malloc(copy_length + 1U);
    if (copy == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (copy_length != 0U) {
        memcpy(copy, bytes, copy_length);
    }
    copy[copy_length] = '\0';
    rc = convert_binary_bytes_for_column(database, column, row_number, false, &copy, &copy_length);
    free(copy);
    return rc;
}

static void make_modify_target_descriptor(
    const struct planned_alter_table_modify_column *plan,
    struct mylite_catalog_column_descriptor *out_column
) {
    *out_column = plan->original_column;
    memcpy(out_column->name, plan->column.name, sizeof(out_column->name));
    snprintf(
        out_column->logical_type,
        sizeof(out_column->logical_type),
        "%s",
        plan->column.logical_type
    );
    snprintf(
        out_column->physical_type,
        sizeof(out_column->physical_type),
        "%s",
        plan->column.physical_type
    );
    out_column->is_nullable = plan->column.is_nullable;
    out_column->on_update_current_timestamp = plan->column.on_update_current_timestamp;
    out_column->default_kind = plan->column.default_kind;
    out_column->default_integer = plan->column.default_integer;
    snprintf(
        out_column->default_text,
        sizeof(out_column->default_text),
        "%s",
        plan->column.default_text
    );
    snprintf(
        out_column->character_set_name,
        sizeof(out_column->character_set_name),
        "%s",
        plan->column.character_set_name
    );
    snprintf(
        out_column->collation_name,
        sizeof(out_column->collation_name),
        "%s",
        plan->column.collation_name
    );
}

static int execute_physical_alter_table_modify_column(
    struct mylite_db *database,
    const struct planned_alter_table_modify_column *plan,
    const struct mylite_catalog_mutation *mutation
) {
    char temporary_physical_name[MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY];
    char *sql = NULL;
    int rc = build_modify_temporary_physical_name(
        plan,
        mutation,
        temporary_physical_name,
        sizeof(temporary_physical_name)
    );

    if (rc == MYLITE_OK) {
        rc = build_alter_table_modify_create_sql(plan, temporary_physical_name, &sql);
    }
    if (rc == MYLITE_OK) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);
    sql = NULL;

    if (rc == MYLITE_OK) {
        if (alter_table_modify_needs_binary_copy_materialization(plan)) {
            rc = execute_physical_alter_table_modify_copy_rows(
                database,
                plan,
                temporary_physical_name
            );
        } else {
            rc = build_alter_table_modify_copy_sql(plan, temporary_physical_name, &sql);
            if (rc == MYLITE_OK) {
                rc = execute_sqlite_schema_sql(database, sql);
            }
        }
    }
    free(sql);
    sql = NULL;

    if (rc == MYLITE_OK) {
        rc = execute_physical_drop_table(database, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = build_alter_table_rename_physical_table_sql(
            temporary_physical_name,
            plan->table.physical_name,
            &sql
        );
    }
    if (rc == MYLITE_OK) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);
    sql = NULL;

    if (rc == MYLITE_OK) {
        rc = build_alter_table_modify_indexes_sql(plan, &sql);
    }
    if (rc == MYLITE_OK && sql != NULL) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);

    return rc;
}

static int execute_physical_alter_table_order_by(
    struct mylite_db *database,
    const struct planned_alter_table_order_by *plan,
    int64_t *out_affected_rows
) {
    sqlite3_stmt *statement = NULL;
    char temporary_physical_name[MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY];
    char *sql = NULL;
    bool transaction_started = false;
    bool copy_failed = false;
    int64_t affected_rows = 0;
    int sqlite_rc = SQLITE_OK;
    int rc = build_alter_table_order_temporary_physical_name(
        plan,
        database->session.sqlite_schema_generation + 1U,
        temporary_physical_name,
        sizeof(temporary_physical_name)
    );

    *out_affected_rows = 0;
    if (rc == MYLITE_OK) {
        rc = execute_sqlite_control_sql(database, "BEGIN IMMEDIATE");
    }
    if (rc == MYLITE_OK) {
        transaction_started = true;
        rc = build_alter_table_order_create_sql(plan, temporary_physical_name, &sql);
    }
    if (rc == MYLITE_OK) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);
    sql = NULL;

    if (rc == MYLITE_OK) {
        rc = build_alter_table_order_copy_sql(plan, temporary_physical_name, &sql);
    }
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            affected_rows = (int64_t)sqlite3_changes64(database->sqlite);
        } else {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finalize_sqlite_statement(statement, rc);
    statement = NULL;
    if (rc != MYLITE_OK) {
        copy_failed = true;
    }
    free(sql);
    sql = NULL;

    if (rc == MYLITE_OK) {
        rc = execute_physical_drop_table(database, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = build_alter_table_rename_physical_table_sql(
            temporary_physical_name,
            plan->table.physical_name,
            &sql
        );
    }
    if (rc == MYLITE_OK) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);
    sql = NULL;

    if (rc == MYLITE_OK) {
        rc = execute_sqlite_control_sql(database, "COMMIT");
        if (rc == MYLITE_OK) {
            transaction_started = false;
        }
    }
    if (rc != MYLITE_OK && transaction_started) {
        (void)execute_sqlite_control_sql(database, "ROLLBACK");
    }
    if (rc != MYLITE_OK && rc != MYLITE_NOMEM && copy_failed) {
        set_physical_sqlite_row_error(database);
    }
    if (rc == MYLITE_OK) {
        ++database->session.sqlite_schema_generation;
        *out_affected_rows = affected_rows;
    }

    return rc;
}

static int execute_physical_alter_table_force(
    struct mylite_db *database,
    const struct planned_alter_table_force *plan
) {
    sqlite3_stmt *statement = NULL;
    char temporary_physical_name[MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY];
    char *sql = NULL;
    bool transaction_started = false;
    bool copy_failed = false;
    int sqlite_rc = SQLITE_OK;
    int rc = build_alter_table_force_temporary_physical_name(
        plan,
        database->session.sqlite_schema_generation + 1U,
        temporary_physical_name,
        sizeof(temporary_physical_name)
    );

    if (rc == MYLITE_OK) {
        rc = execute_sqlite_control_sql(database, "BEGIN IMMEDIATE");
    }
    if (rc == MYLITE_OK) {
        transaction_started = true;
        rc = build_alter_table_force_create_sql(plan, temporary_physical_name, &sql);
    }
    if (rc == MYLITE_OK) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);
    sql = NULL;

    if (rc == MYLITE_OK) {
        rc = build_alter_table_force_copy_sql(plan, temporary_physical_name, &sql);
    }
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc != SQLITE_DONE) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finalize_sqlite_statement(statement, rc);
    statement = NULL;
    if (rc != MYLITE_OK) {
        copy_failed = true;
    }
    free(sql);
    sql = NULL;

    if (rc == MYLITE_OK) {
        rc = execute_physical_drop_table(database, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = build_alter_table_rename_physical_table_sql(
            temporary_physical_name,
            plan->table.physical_name,
            &sql
        );
    }
    if (rc == MYLITE_OK) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);
    sql = NULL;

    if (rc == MYLITE_OK) {
        rc = execute_sqlite_control_sql(database, "COMMIT");
        if (rc == MYLITE_OK) {
            transaction_started = false;
        }
    }
    if (rc != MYLITE_OK && transaction_started) {
        (void)execute_sqlite_control_sql(database, "ROLLBACK");
    }
    if (rc != MYLITE_OK && rc != MYLITE_NOMEM && copy_failed) {
        set_physical_sqlite_row_error(database);
    }
    if (rc == MYLITE_OK) {
        ++database->session.sqlite_schema_generation;
    }

    return rc;
}

static int build_alter_table_check_temporary_physical_name(
    const struct planned_alter_table_check_constraint *plan,
    uint64_t sqlite_schema_generation,
    char *destination,
    size_t destination_size
) {
    int written = 0;

    if (plan == NULL || destination == NULL || destination_size == 0U) {
        return MYLITE_MISUSE;
    }

    written = snprintf(
        destination,
        destination_size,
        "_mylite_user_table_%" PRId64 "_check_%" PRIu64,
        plan->table.table_id,
        sqlite_schema_generation
    );
    if (written < 0 || (size_t)written >= destination_size) {
        return MYLITE_NOMEM;
    }

    return MYLITE_OK;
}

static int build_alter_table_check_create_sql(
    const struct planned_alter_table_check_constraint *plan,
    const char *temporary_physical_name,
    char **out_sql
) {
    return build_create_table_definition_sql(
        &plan->rebuild,
        temporary_physical_name,
        false,
        out_sql
    );
}

static int build_alter_table_check_copy_sql(
    const struct planned_alter_table_check_constraint *plan,
    const char *temporary_physical_name,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "INSERT INTO ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, temporary_physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " (");
    }
    if (rc == MYLITE_OK) {
        rc = append_alter_table_check_column_list(&string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, ") SELECT ");
    }
    if (rc == MYLITE_OK) {
        rc = append_alter_table_check_column_list(&string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        *out_sql = mylite_dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    mylite_dynamic_string_deinit(&string);
    return rc;
}

static int append_alter_table_check_column_list(
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_check_constraint *plan
) {
    int rc = MYLITE_OK;

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->rebuild.column_count;
         ++column_index) {
        if (column_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(
                string,
                plan->rebuild.columns[column_index].name
            );
        }
    }

    return rc;
}

static int rename_table_from_plan_with_policy(
    struct mylite_db *database,
    const struct planned_rename_table *plan,
    bool allow_same_object_noop,
    const char *unsupported_object_message
) {
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    struct mylite_catalog_table_descriptor source = {0};
    int rc = MYLITE_OK;

    if (allow_same_object_noop) {
        rc = mylite_catalog_read_table_by_name(
            database,
            plan->source.schema.schema_id,
            plan->source.table_name,
            &source
        );
        if (rc != MYLITE_OK) {
            set_table_does_not_exist_error(
                database,
                plan->source.schema.name,
                plan->source.table_name
            );
            return MYLITE_ERROR;
        }
        if (source.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
            set_unsupported_error(database, unsupported_object_message);
            return MYLITE_ERROR;
        }
        if (source.schema_id == plan->target.schema.schema_id &&
            text_equals_ascii_case_insensitive(source.name, plan->target.table_name)) {
            return MYLITE_OK;
        }
    }

    rc = mylite_catalog_begin_mutation(database, &mutation);
    if (rc == MYLITE_OK) {
        rc = rename_table_pair_in_mutation(
            database,
            &mutation,
            plan,
            allow_same_object_noop,
            unsupported_object_message
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to rename table descriptor");
        mylite_catalog_rollback_mutation(database, &mutation);
        return rc;
    }

    return MYLITE_OK;
}

static int rename_table_pair_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    const struct planned_rename_table *plan,
    bool allow_same_object_noop,
    const char *unsupported_object_message
) {
    struct mylite_catalog_table_descriptor source = {0};
    struct mylite_catalog_table_descriptor target = {0};
    bool found = false;
    int rc = mylite_catalog_read_table_by_name(
        database,
        plan->source.schema.schema_id,
        plan->source.table_name,
        &source
    );

    if (rc != MYLITE_OK) {
        set_table_does_not_exist_error(database, plan->source.schema.name, plan->source.table_name);
        return MYLITE_ERROR;
    }
    if (source.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(database, unsupported_object_message);
        return MYLITE_ERROR;
    }
    if (allow_same_object_noop && source.schema_id == plan->target.schema.schema_id &&
        text_equals_ascii_case_insensitive(source.name, plan->target.table_name)) {
        return MYLITE_OK;
    }

    rc = mylite_catalog_try_read_table_by_name(
        database,
        plan->target.schema.schema_id,
        plan->target.table_name,
        &target,
        &found
    );
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to read target table descriptor");
        return rc;
    }
    if (found) {
        set_table_exists_error(database, plan->target.table_name);
        return MYLITE_ERROR;
    }
    rc = reject_rename_table_check_constraint_collisions(database, &source, plan);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_catalog_update_table_identity_in_mutation(
        database,
        mutation,
        source.table_id,
        plan->target.schema.schema_id,
        plan->target.table_name,
        NULL
    );
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_rename_generated_check_constraints_in_mutation(
            database,
            mutation,
            source.table_id,
            plan->target.table_name
        );
    }

    return rc;
}

static int reject_rename_table_check_constraint_collisions(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *source,
    const struct planned_rename_table *plan
) {
    struct loaded_check_constraint_info *checks = NULL;
    size_t check_count = 0U;
    char check_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = load_table_check_constraint_infos(database, source->table_id, &checks, &check_count);

    for (size_t index = 0U; rc == MYLITE_OK && index < check_count; ++index) {
        rc = build_renamed_check_constraint_name(
            database,
            plan,
            &checks[index].check_constraint,
            check_name,
            sizeof(check_name)
        );
        for (size_t previous = 0U; rc == MYLITE_OK && previous < index; ++previous) {
            char previous_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];

            rc = build_renamed_check_constraint_name(
                database,
                plan,
                &checks[previous].check_constraint,
                previous_name,
                sizeof(previous_name)
            );
            if (rc == MYLITE_OK && text_equals_ascii_case_insensitive(check_name, previous_name)) {
                set_duplicate_check_constraint_error(database, check_name);
                rc = MYLITE_ERROR;
            }
        }
        if (rc == MYLITE_OK) {
            rc = reject_rename_table_check_constraint_schema_collision(
                database,
                source,
                plan,
                check_name
            );
        }
    }

    loaded_check_constraint_infos_deinit(&checks, &check_count);
    return rc;
}

static int build_renamed_check_constraint_name(
    struct mylite_db *database,
    const struct planned_rename_table *plan,
    const struct mylite_catalog_check_constraint_descriptor *check_constraint,
    char *destination,
    size_t destination_size
) {
    int written = 0;

    if (!check_constraint->name_is_generated) {
        written = snprintf(destination, destination_size, "%s", check_constraint->name);
    } else {
        written = snprintf(
            destination,
            destination_size,
            "%s_chk_%" PRId64,
            plan->target.table_name,
            check_constraint->generated_ordinal
        );
    }
    if (written < 0 || (size_t)written >= destination_size) {
        set_runtime_error(database, "renamed CHECK constraint name is too long");
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int reject_rename_table_check_constraint_schema_collision(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *source,
    const struct planned_rename_table *plan,
    const char *check_constraint_name
) {
    struct check_constraint_name_collision_context context = {
        .name = check_constraint_name,
        .excluded_table_id = source->table_id,
        .found = false,
    };
    int rc = mylite_catalog_for_each_check_constraint_in_schema(
        database,
        plan->target.schema.schema_id,
        reject_rename_table_check_constraint_schema_collision_callback,
        &context
    );

    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to read CHECK constraint descriptors");
        return rc;
    }
    if (context.found) {
        set_duplicate_check_constraint_error(database, check_constraint_name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int reject_rename_table_check_constraint_schema_collision_callback(
    const struct mylite_catalog_check_constraint_descriptor *check_constraint,
    void *user_data
) {
    struct check_constraint_name_collision_context *context = user_data;

    if (check_constraint == NULL || context == NULL || context->name == NULL) {
        return MYLITE_MISUSE;
    }
    if (check_constraint->table_id != context->excluded_table_id &&
        text_equals_ascii_case_insensitive(check_constraint->name, context->name)) {
        context->found = true;
    }

    return MYLITE_OK;
}
