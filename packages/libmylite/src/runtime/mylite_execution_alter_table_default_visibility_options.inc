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
        rc = mylite_execution_load_table_columns(database, out_plan->table.table_id, &columns, &column_count);
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
        rc = mylite_execution_load_table_columns(database, out_plan->table.table_id, &columns, &column_count);
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
        rc = mylite_execution_load_table_columns(database, out_plan->table.table_id, &columns, &column_count);
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
        rc = mylite_execution_load_table_columns(database, out_plan->table.table_id, &columns, &column_count);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_index_infos(
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

    mylite_execution_loaded_index_infos_deinit(&indexes, &index_count);
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
    mylite_execution_loaded_index_info_deinit(&plan->index);
    *plan = (struct planned_alter_table_index_visibility){0};
}
