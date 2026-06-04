static int plan_alter_table_add_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_add_column *out_plan
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    size_t column_count = 0U;
    size_t duplicate_index = 0U;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_add_column){0};
    rc = resolve_visible_writable_table_reference(
        database,
        child_at(statement, 0U),
        &out_plan->target,
        &out_plan->table
    );
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
    if (rc == MYLITE_OK) {
        rc = reject_unsupported_alter_add_column_planned_column(database, &out_plan->column);
    }
    if (rc == MYLITE_OK) {
        rc = reject_inline_primary_key_column_definition(
            database,
            child_at(statement, 1U),
            "ALTER TABLE ADD COLUMN does not support PRIMARY KEY"
        );
    }
    if (rc == MYLITE_OK) {
        rc = reject_auto_increment_column_definition(
            database,
            child_at(statement, 1U),
            "ALTER TABLE ADD COLUMN does not support AUTO_INCREMENT"
        );
    }
    if (rc == MYLITE_OK && out_plan->table.kind != MYLITE_CATALOG_TABLE_KIND_BASE &&
        out_plan->table.kind != MYLITE_CATALOG_TABLE_KIND_TEMPORARY) {
        set_unsupported_error(
            database,
            "ALTER TABLE ADD COLUMN supports only persistent base tables or temporary tables"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = normalize_text_length_argument_for_default_charset(
            database,
            &out_plan->column,
            out_plan->table.default_charset
        );
    }
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
    if (rc == MYLITE_OK) {
        rc = reject_nonempty_temporal_add_column_without_default(
            database,
            &out_plan->table,
            &out_plan->column
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_columns(database, out_plan->table.table_id, &columns, &column_count);
    }
    if (rc == MYLITE_OK &&
        find_column_index(columns, column_count, out_plan->column.name, &duplicate_index) ==
            MYLITE_OK) {
        set_duplicate_column_error(database, out_plan->column.name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = validate_added_column_row_size(database, columns, column_count, &out_plan->column);
    }
    if (rc == MYLITE_OK && column_count >= (size_t)INT64_MAX) {
        set_runtime_error(database, "too many columns in table descriptor");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = finish_alter_table_add_column_plan_columns(
            database,
            child_at(statement, 2U),
            out_plan,
            &columns,
            column_count
        );
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_add_column_deinit(out_plan);
    }

    free(columns);

    return rc;
}

static int reject_unsupported_alter_add_column_planned_column(
    struct mylite_db *database,
    const struct planned_column *column
) {
    if (column->is_generated) {
        set_unsupported_error(
            database,
            "ALTER TABLE ADD COLUMN does not support generated columns"
        );
        return MYLITE_ERROR;
    }
    if (planned_column_is_spatial(column)) {
        set_unsupported_error(
            database,
            "ALTER TABLE ADD COLUMN does not yet support spatial columns"
        );
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int finish_alter_table_add_column_plan_columns(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *position,
    struct planned_alter_table_add_column *out_plan,
    struct mylite_catalog_column_descriptor **columns,
    size_t column_count
) {
    int rc = collect_no_zero_date_default_warnings(
        database,
        *columns,
        column_count,
        (struct zero_temporal_default_warning_filter){.skipped_column_id = -1},
        &out_plan->zero_temporal_default_warnings
    );

    if (rc != MYLITE_OK) {
        return rc;
    }

    out_plan->columns = *columns;
    out_plan->column_count = column_count;
    *columns = NULL;
    return plan_alter_table_add_column_position(database, position, out_plan);
}

static int plan_alter_table_add_column_position(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *position,
    struct planned_alter_table_add_column *out_plan
) {
    size_t after_column_index = 0U;
    int rc = MYLITE_OK;

    out_plan->target_column_index = out_plan->column_count;
    if (position == NULL) {
        return MYLITE_OK;
    }

    if (position->kind == MYLITE_SQL_AST_COLUMN_POSITION_FIRST) {
        out_plan->target_column_index = 0U;
        out_plan->changes_position = out_plan->target_column_index != out_plan->column_count;
        return MYLITE_OK;
    }
    if (position->kind != MYLITE_SQL_AST_COLUMN_POSITION_AFTER) {
        set_runtime_error(database, "invalid ALTER TABLE ADD COLUMN position");
        return MYLITE_ERROR;
    }

    rc = copy_identifier_text(
        child_at(position, 0U),
        out_plan->after_column_name,
        sizeof(out_plan->after_column_name),
        database
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (mylite_catalog_name_is_reserved(out_plan->after_column_name)) {
        set_reserved_name_error(database, "column", out_plan->after_column_name);
        return MYLITE_ERROR;
    }

    rc = find_column_index(
        out_plan->columns,
        out_plan->column_count,
        out_plan->after_column_name,
        &after_column_index
    );
    if (rc != MYLITE_OK) {
        set_unknown_column_in_table_error(
            database,
            out_plan->after_column_name,
            out_plan->table.name
        );
        return MYLITE_ERROR;
    }

    out_plan->target_column_index = after_column_index + 1U;
    out_plan->changes_position = out_plan->target_column_index != out_plan->column_count;
    return MYLITE_OK;
}

static int reject_nonempty_temporal_add_column_without_default(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const struct planned_column *column
) {
    int64_t row_count = 0;
    int rc = MYLITE_OK;

    if ((!planned_column_is_date(column) && !planned_column_is_datetime(column)) ||
        column->is_nullable || column->default_kind != MYLITE_CATALOG_COLUMN_DEFAULT_NONE) {
        return MYLITE_OK;
    }

    rc = read_show_table_status_row_count(database, table, &row_count);
    if (rc != MYLITE_OK || row_count <= 0) {
        return rc;
    }

    if (planned_column_is_date(column)) {
        set_incorrect_date_value_error(database, "0000-00-00", column->name, 1U);
    } else {
        set_incorrect_datetime_value_error(database, "0000-00-00 00:00:00", column->name, 1U);
    }

    return MYLITE_ERROR;
}

static int collect_no_zero_date_default_warnings(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    struct zero_temporal_default_warning_filter filter,
    struct zero_temporal_default_warnings *out_warnings
) {
    int rc = MYLITE_OK;

    if (!temporal_sql_mode_has_no_zero_date(database) || temporal_sql_mode_is_strict(database)) {
        return MYLITE_OK;
    }
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < column_count; ++column_index) {
        const struct mylite_catalog_column_descriptor *column = &columns[column_index];

        if (column->column_id == filter.skipped_column_id ||
            !column_descriptor_has_full_zero_temporal_default(column)) {
            continue;
        }
        rc = append_zero_temporal_default_warning_name(database, out_warnings, column->name);
    }

    return rc;
}

static int reserve_zero_temporal_default_warnings(
    struct mylite_db *database,
    const struct zero_temporal_default_warnings *warnings
) {
    if (warnings == NULL || warnings->count == 0U) {
        return MYLITE_OK;
    }
    return reserve_additional_warning_capacity(database, warnings->count);
}

static int append_zero_temporal_default_warnings(
    struct mylite_db *database,
    const struct zero_temporal_default_warnings *warnings
) {
    int rc = MYLITE_OK;

    if (warnings == NULL) {
        return MYLITE_OK;
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < warnings->count; ++index) {
        rc = append_zero_temporal_default_warning(database, warnings->items[index].column_name);
    }

    return rc;
}

static int append_zero_temporal_default_warning(
    struct mylite_db *database,
    const char *column_name
) {
    return append_out_of_range_warning(database, column_name, 1U);
}

static bool column_descriptor_has_full_zero_temporal_default(
    const struct mylite_catalog_column_descriptor *column
) {
    if (column == NULL || column->default_kind != MYLITE_CATALOG_COLUMN_DEFAULT_TEXT) {
        return false;
    }
    if (column_descriptor_is_date(column)) {
        return strcmp(column->default_text, "0000-00-00") == 0;
    }
    if (column_descriptor_is_datetime(column) || column_descriptor_is_timestamp(column)) {
        return strcmp(column->default_text, "0000-00-00 00:00:00") == 0;
    }
    return false;
}

static int append_zero_temporal_default_warning_name(
    struct mylite_db *database,
    struct zero_temporal_default_warnings *warnings,
    const char *column_name
) {
    struct zero_temporal_default_warning *items = NULL;

    if (warnings == NULL || column_name == NULL) {
        return MYLITE_MISUSE;
    }
    if (warnings->count == warnings->capacity) {
        size_t new_capacity = warnings->capacity == 0U ? 4U : warnings->capacity * 2U;

        if (new_capacity < warnings->capacity ||
            new_capacity > SIZE_MAX / sizeof(warnings->items[0])) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        items = realloc(warnings->items, new_capacity * sizeof(warnings->items[0]));
        if (items == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        warnings->items = items;
        warnings->capacity = new_capacity;
    }

    snprintf(
        warnings->items[warnings->count].column_name,
        sizeof(warnings->items[warnings->count].column_name),
        "%s",
        column_name
    );
    ++warnings->count;
    return MYLITE_OK;
}

static void zero_temporal_default_warnings_deinit(struct zero_temporal_default_warnings *warnings) {
    if (warnings == NULL) {
        return;
    }

    free(warnings->items);
    *warnings = (struct zero_temporal_default_warnings){0};
}

static void planned_alter_table_add_column_deinit(struct planned_alter_table_add_column *plan) {
    if (plan == NULL) {
        return;
    }

    free(plan->columns);
    zero_temporal_default_warnings_deinit(&plan->zero_temporal_default_warnings);
    *plan = (struct planned_alter_table_add_column){0};
}

static int alter_table_add_column_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_add_column *plan
) {
    if (plan != NULL && plan->table.kind == MYLITE_CATALOG_TABLE_KIND_TEMPORARY) {
        return add_temporary_column_from_plan(database, plan);
    }

    return add_persistent_column_from_plan(database, plan);
}

static int add_persistent_column_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_add_column *plan
) {
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    int rc = MYLITE_OK;

    if (plan == NULL || plan->table.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_runtime_error(database, "invalid persistent add-column plan");
        return MYLITE_ERROR;
    }
    rc = mylite_catalog_begin_mutation(database, &mutation);
    if (rc == MYLITE_OK) {
        rc = alter_table_add_column_in_mutation(database, &mutation, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to add table column");
        mylite_catalog_rollback_mutation(database, &mutation);
        return rc;
    }

    ++database->session.sqlite_schema_generation;

    return MYLITE_OK;
}

static int add_temporary_column_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_add_column *plan
) {
    struct mylite_catalog_column_descriptor column = {0};
    bool descriptor_appended = false;
    int rc = build_temporary_added_column_descriptor(database, plan, &column);

    if (rc == MYLITE_OK) {
        rc = mylite_temporary_catalog_append_column(
            &database->session.temporary_catalog,
            plan->table.table_id,
            &column,
            plan->target_column_index
        );
        descriptor_appended = rc == MYLITE_OK;
    }
    if (rc == MYLITE_OK) {
        rc = execute_physical_alter_table_add_column(database, plan);
    }
    if (rc != MYLITE_OK) {
        if (descriptor_appended) {
            (void)mylite_temporary_catalog_remove_column_by_id(
                &database->session.temporary_catalog,
                plan->table.table_id,
                column.column_id
            );
        }
        set_internal_error_if_clear(database, rc, "failed to add temporary table column");
        return rc;
    }

    ++database->session.sqlite_schema_generation;
    return MYLITE_OK;
}

static int build_temporary_added_column_descriptor(
    struct mylite_db *database,
    const struct planned_alter_table_add_column *plan,
    struct mylite_catalog_column_descriptor *out_column
) {
    int64_t column_id = 0;
    int rc = MYLITE_OK;

    if (plan == NULL || out_column == NULL ||
        plan->table.kind != MYLITE_CATALOG_TABLE_KIND_TEMPORARY ||
        plan->target_column_index > plan->column_count || plan->target_column_index >= INT64_MAX) {
        set_runtime_error(database, "invalid temporary add-column plan");
        return MYLITE_ERROR;
    }
    if (plan->adds_inline_primary_key || plan->adds_inline_unique_key) {
        set_runtime_error(database, "invalid temporary add-column inline key plan");
        return MYLITE_ERROR;
    }

    *out_column = (struct mylite_catalog_column_descriptor){0};
    rc = mylite_temporary_catalog_allocate_column_id(
        &database->session.temporary_catalog,
        &column_id
    );
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to allocate temporary column id");
        return rc;
    }

    *out_column = (struct mylite_catalog_column_descriptor){
        .column_id = column_id,
        .table_id = plan->table.table_id,
        .ordinal_position = (int64_t)plan->target_column_index + 1,
        .is_nullable = plan->column.is_nullable,
        .is_visible = true,
        .is_auto_increment = false,
        .default_kind = plan->column.default_kind,
        .default_integer = plan->column.default_integer,
        .on_update_current_timestamp = plan->column.on_update_current_timestamp,
        .is_generated = plan->column.is_generated,
        .generated_kind = plan->column.generated_kind,
    };
    snprintf(out_column->name, sizeof(out_column->name), "%s", plan->column.name);
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
    snprintf(out_column->comment, sizeof(out_column->comment), "%s", plan->column.comment);
    snprintf(
        out_column->generation_expression,
        sizeof(out_column->generation_expression),
        "%s",
        plan->column.generation_expression
    );
    snprintf(
        out_column->sqlite_generation_expression,
        sizeof(out_column->sqlite_generation_expression),
        "%s",
        plan->column.sqlite_generation_expression
    );
    return MYLITE_OK;
}

static int alter_table_add_column_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    const struct planned_alter_table_add_column *plan
) {
    struct mylite_catalog_column_descriptor inserted_column = {0};
    int rc = MYLITE_OK;

    if (mutation == NULL || plan == NULL) {
        set_runtime_error(database, "invalid add-column mutation plan");
        return MYLITE_ERROR;
    }

    rc = mylite_catalog_insert_column_in_mutation(
        database,
        mutation,
        plan->table.table_id,
        (int64_t)plan->column_count + 1,
        plan->column.name,
        plan->column.logical_type,
        plan->column.physical_type,
        plan->column.is_nullable,
        true,
        false,
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
        plan->column.sqlite_generation_expression,
        &inserted_column
    );
    if (rc == MYLITE_OK) {
        rc = reorder_catalog_columns_after_add(database, mutation, plan, &inserted_column);
    }
    if (rc == MYLITE_OK) {
        rc = execute_physical_alter_table_add_column(database, plan);
    }
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

static int reorder_catalog_columns_after_add(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    const struct planned_alter_table_add_column *plan,
    const struct mylite_catalog_column_descriptor *inserted_column
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    size_t new_column_count = 0U;
    size_t source_index = 0U;
    int rc = MYLITE_OK;

    if (!plan->changes_position) {
        return MYLITE_OK;
    }
    if (plan->column_count == SIZE_MAX) {
        set_runtime_error(database, "failed to plan add-column reorder");
        return MYLITE_ERROR;
    }

    new_column_count = plan->column_count + 1U;
    if (inserted_column == NULL || inserted_column->column_id <= 0 ||
        plan->target_column_index >= new_column_count ||
        new_column_count > SIZE_MAX / sizeof(*columns)) {
        set_runtime_error(database, "failed to plan add-column reorder");
        return MYLITE_ERROR;
    }

    columns = calloc(new_column_count, sizeof(*columns));
    if (columns == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    for (size_t index = 0U; index < new_column_count; ++index) {
        if (index == plan->target_column_index) {
            columns[index] = *inserted_column;
            continue;
        }
        if (source_index >= plan->column_count) {
            free(columns);
            set_runtime_error(database, "failed to build add-column reorder");
            return MYLITE_ERROR;
        }
        columns[index] = plan->columns[source_index];
        ++source_index;
    }

    const struct mylite_catalog_column_reorder reorder = {
        .table_id = plan->table.table_id,
        .columns = columns,
        .column_count = new_column_count,
        .metadata_replaced_column_id = inserted_column->column_id,
    };

    rc = mylite_catalog_reorder_columns_in_mutation(database, mutation, &reorder);
    free(columns);
    return rc;
}

static int execute_physical_alter_table_add_column(
    struct mylite_db *database,
    const struct planned_alter_table_add_column *plan
) {
    char *sql = NULL;
    int rc = build_alter_table_add_column_sql(database, plan, &sql);

    if (rc == MYLITE_OK) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);

    return rc;
}

static int plan_alter_table_add_primary_key(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_add_primary_key *out_plan
) {
    const struct mylite_sql_ast_node *primary_key = child_at(statement, 1U);
    struct primary_key_definition_nodes nodes = {0};
    const struct mylite_sql_ast_node *part_list = NULL;
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct primary_key_info existing_primary_key = mylite_execution_primary_key_info_init();
    size_t column_count = 0U;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_add_primary_key){0};
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
            "ALTER TABLE ADD PRIMARY KEY supports only persistent base tables"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_columns(database, out_plan->table.table_id, &columns, &column_count);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_primary_key_info(
            database,
            out_plan->table.table_id,
            columns,
            column_count,
            &existing_primary_key
        );
    }
    if (rc == MYLITE_OK && existing_primary_key.has_primary_key) {
        set_multiple_primary_key_error(database);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = extract_primary_key_definition_nodes(database, primary_key, &nodes);
    }
    if (rc == MYLITE_OK) {
        part_list = nodes.part_list;
        rc = apply_index_options_to_alter_primary_key_plan(database, nodes.options, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc =
            validate_hash_index_key_part_order(database, out_plan->uses_hash_index_type, part_list);
    }
    if (rc == MYLITE_OK) {
        rc = choose_sqlite_rowid_alias(
            database,
            columns,
            column_count,
            "ALTER TABLE ADD PRIMARY KEY requires an unshadowed SQLite rowid alias",
            &out_plan->rowid_alias
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_alter_table_primary_key_parts_from_ast(
            database,
            out_plan,
            part_list,
            columns,
            column_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_alter_table_primary_key_key_length(database, out_plan);
    }

    mylite_execution_primary_key_info_deinit(&existing_primary_key);
    free(columns);
    return rc;
}

static int append_alter_table_primary_key_parts_from_ast(
    struct mylite_db *database,
    struct planned_alter_table_add_primary_key *plan,
    const struct mylite_sql_ast_node *part_list,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count
) {
    const struct mylite_sql_ast_node *part = child_at(part_list, 0U);
    int rc = MYLITE_OK;

    while (rc == MYLITE_OK && part != NULL) {
        const struct mylite_sql_ast_node *column_node = secondary_index_part_column_node(part);
        enum mylite_catalog_index_sort_direction sort_direction = index_part_sort_direction(part);
        char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];

        if (column_node == NULL || column_node->kind != MYLITE_SQL_AST_IDENTIFIER) {
            set_unsupported_error(
                database,
                index_part_unsupported_message(
                    part,
                    "PRIMARY KEY supports only unqualified key columns"
                )
            );
            rc = MYLITE_ERROR;
            break;
        }
        rc = copy_identifier_text(column_node, column_name, sizeof(column_name), database);
        if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(column_name)) {
            set_reserved_name_error(database, "column", column_name);
            rc = MYLITE_ERROR;
        }
        if (rc == MYLITE_OK) {
            rc = append_alter_table_primary_key_part(
                database,
                plan,
                columns,
                column_count,
                column_name,
                sort_direction
            );
        }
        part = part->next_sibling;
    }

    return rc;
}

static void planned_alter_table_add_primary_key_deinit(
    struct planned_alter_table_add_primary_key *plan
) {
    if (plan == NULL) {
        return;
    }
    free(plan->parts);
    *plan = (struct planned_alter_table_add_primary_key){0};
}

static int validate_alter_table_primary_key_key_length(
    struct mylite_db *database,
    const struct planned_alter_table_add_primary_key *plan
) {
    uint64_t total = 0U;

    if (plan == NULL || plan->parts == NULL || plan->part_count == 0U) {
        set_runtime_error(database, "invalid primary-key key parts");
        return MYLITE_ERROR;
    }
    for (size_t part_index = 0U; part_index < plan->part_count; ++part_index) {
        const struct loaded_index_part *part = &plan->parts[part_index];
        uint64_t part_bytes = 0U;
        int rc = column_descriptor_primary_key_part_key_bytes(database, &part->column, &part_bytes);

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

static int append_alter_table_primary_key_part(
    struct mylite_db *database,
    struct planned_alter_table_add_primary_key *plan,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const char *column_name,
    enum mylite_catalog_index_sort_direction sort_direction
) {
    size_t column_index = 0U;
    int rc = find_column_index(columns, column_count, column_name, &column_index);

    if (rc != MYLITE_OK) {
        set_key_column_missing_error(database, column_name);
        return MYLITE_ERROR;
    }
    if (alter_table_primary_key_contains_column(plan, column_index)) {
        set_duplicate_column_error(database, columns[column_index].name);
        return MYLITE_ERROR;
    }

    if (column_descriptor_is_char_or_varchar(&columns[column_index])) {
        rc = validate_descriptor_string_key_column(database, &columns[column_index]);
    } else if (column_descriptor_is_json(&columns[column_index])) {
        set_json_key_error(database, columns[column_index].name);
        rc = MYLITE_ERROR;
    } else {
        struct integer_column_range range = {0};

        rc = integer_range_for_column(
            database,
            &columns[column_index],
            "PRIMARY KEY supports only integer columns",
            &range
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = reserve_alter_table_primary_key_parts(database, plan, plan->part_count + 1U);
    if (rc != MYLITE_OK) {
        return rc;
    }

    plan->parts[plan->part_count] = (struct loaded_index_part){
        .index_column =
            {
                .column_id = columns[column_index].column_id,
                .ordinal_position = (int64_t)plan->part_count + 1,
                .sort_direction = sort_direction,
            },
        .column = columns[column_index],
        .column_index = column_index,
    };
    ++plan->part_count;

    return MYLITE_OK;
}

static int reserve_alter_table_primary_key_parts(
    struct mylite_db *database,
    struct planned_alter_table_add_primary_key *plan,
    size_t required_capacity
) {
    enum { initial_primary_key_part_capacity = 4 };

    struct loaded_index_part *parts = NULL;
    size_t capacity = 0U;

    if (required_capacity <= plan->part_capacity) {
        return MYLITE_OK;
    }
    capacity = plan->part_capacity == 0U ? initial_primary_key_part_capacity : plan->part_capacity;
    while (capacity < required_capacity) {
        if (capacity > SIZE_MAX / 2U) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        capacity *= 2U;
    }
    if (capacity > SIZE_MAX / sizeof(*parts)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    parts = realloc(plan->parts, capacity * sizeof(*parts));
    if (parts == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    plan->parts = parts;
    plan->part_capacity = capacity;
    return MYLITE_OK;
}

static bool alter_table_primary_key_contains_column(
    const struct planned_alter_table_add_primary_key *plan,
    size_t column_index
) {
    for (size_t part_index = 0U; part_index < plan->part_count; ++part_index) {
        if (plan->parts[part_index].column_index == column_index) {
            return true;
        }
    }

    return false;
}

static int alter_table_add_primary_key_from_plan(
    struct mylite_db *database,
    const struct planned_alter_table_add_primary_key *plan
) {
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    int rc = MYLITE_OK;

    if (plan->part_count == 0U) {
        set_runtime_error(database, "invalid primary-key plan");
        return MYLITE_ERROR;
    }
    rc = validate_alter_table_add_primary_key_existing_rows(database, plan);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = reserve_hash_index_warning_if_needed(database, plan->uses_hash_index_type);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_begin_mutation(database, &mutation);
    }
    if (rc == MYLITE_OK) {
        rc = alter_table_add_primary_key_in_mutation(database, &mutation, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to add primary key");
        mylite_catalog_rollback_mutation(database, &mutation);
        return rc;
    }

    ++database->session.sqlite_schema_generation;

    return append_hash_index_warning_if_needed(database, plan->uses_hash_index_type);
}

static int alter_table_add_primary_key_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    const struct planned_alter_table_add_primary_key *plan
) {
    char primary_key_physical_name[MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY];
    int64_t primary_key_index_id = 0;
    int rc = MYLITE_OK;

    if (mutation == NULL || plan == NULL || plan->part_count == 0U) {
        set_runtime_error(database, "invalid primary-key mutation plan");
        return MYLITE_ERROR;
    }

    rc = mylite_catalog_allocate_index_id_in_mutation(database, mutation, &primary_key_index_id);
    if (rc == MYLITE_OK) {
        rc = build_physical_index_name(
            primary_key_index_id,
            primary_key_physical_name,
            sizeof(primary_key_physical_name)
        );
    }
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < plan->part_count; ++part_index) {
        const struct mylite_catalog_column_descriptor *column = &plan->parts[part_index].column;
        enum mylite_catalog_column_default_kind default_kind =
            alter_table_primary_key_default_kind(column);
        int64_t default_integer = 0;
        const char *default_text = NULL;

        if (column_default_kind_has_integer_value(default_kind)) {
            default_integer = column->default_integer;
        }
        if (column_default_kind_has_text_value(default_kind)) {
            default_text = column->default_text;
        }

        rc = mylite_catalog_replace_column_in_mutation(
            database,
            mutation,
            plan->table.table_id,
            column->column_id,
            column->name,
            column->logical_type,
            column->physical_type,
            false,
            column->is_visible,
            column->is_auto_increment,
            default_kind,
            default_integer,
            default_text,
            column->on_update_current_timestamp,
            column->character_set_name,
            column->collation_name,
            column->comment,
            column->is_generated,
            column->generated_kind,
            column->generation_expression,
            column->sqlite_generation_expression
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_insert_index_in_mutation(
            database,
            mutation,
            primary_key_index_id,
            plan->table.table_id,
            "PRIMARY",
            primary_key_physical_name,
            MYLITE_CATALOG_INDEX_KIND_PRIMARY,
            true,
            true,
            plan->comment,
            plan->show_create_explicit_btree,
            NULL
        );
    }
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < plan->part_count; ++part_index) {
        rc = mylite_catalog_insert_index_column_in_mutation(
            database,
            mutation,
            primary_key_index_id,
            plan->table.table_id,
            plan->parts[part_index].column.column_id,
            (int64_t)part_index + 1,
            NULL,
            plan->parts[part_index].index_column.sort_direction,
            NULL
        );
    }
    if (rc == MYLITE_OK) {
        rc =
            execute_physical_alter_table_add_primary_key(database, plan, primary_key_physical_name);
    }
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

static int validate_alter_table_add_primary_key_existing_rows(
    struct mylite_db *database,
    const struct planned_alter_table_add_primary_key *plan
) {
    int rc = validate_alter_table_add_primary_key_nulls(database, plan);

    if (rc == MYLITE_OK) {
        rc = validate_alter_table_add_primary_key_string_values(database, plan);
    }
    if (rc == MYLITE_OK) {
        rc = validate_alter_table_add_primary_key_duplicates(database, plan);
    }
    return rc;
}

static int validate_alter_table_add_primary_key_nulls(
    struct mylite_db *database,
    const struct planned_alter_table_add_primary_key *plan
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = build_alter_table_add_primary_key_null_validation_sql(plan, &sql);

    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            set_invalid_use_of_null_error(database);
            rc = MYLITE_ERROR;
        } else if (sqlite_rc != SQLITE_DONE) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);

    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    } else if (
        rc != MYLITE_OK &&
        mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) == MYLITE_OK
    ) {
        set_physical_sqlite_row_error(database);
        rc = MYLITE_ERROR;
    }

    return rc;
}

static int validate_alter_table_add_primary_key_string_values(
    struct mylite_db *database,
    const struct planned_alter_table_add_primary_key *plan
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    size_t string_part_count = 0U;
    int sqlite_rc = SQLITE_OK;
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; part_index < plan->part_count; ++part_index) {
        if (column_descriptor_is_char_or_varchar(&plan->parts[part_index].column)) {
            ++string_part_count;
        }
    }
    if (string_part_count == 0U) {
        return MYLITE_OK;
    }

    rc = build_alter_table_add_primary_key_string_validation_sql(plan, &sql);
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    while (rc == MYLITE_OK && (sqlite_rc = sqlite3_step(statement)) == SQLITE_ROW) {
        for (size_t column_index = 0U; column_index < string_part_count; ++column_index) {
            const unsigned char *text = NULL;
            int text_length = 0;

            if (column_index > (size_t)INT_MAX) {
                set_runtime_error(database, "invalid string key validation column");
                rc = MYLITE_ERROR;
                break;
            }
            if (sqlite3_column_type(statement, (int)column_index) == SQLITE_NULL) {
                continue;
            }

            text = sqlite3_column_text(statement, (int)column_index);
            text_length = sqlite3_column_bytes(statement, (int)column_index);
            if (text_length < 0 ||
                !text_value_is_supported_string_key((const char *)text, (size_t)text_length)) {
                set_non_ascii_string_key_error(database);
                rc = MYLITE_ERROR;
                break;
            }
        }
    }
    if (rc == MYLITE_OK && sqlite_rc != SQLITE_DONE) {
        rc = mylite_sqlite_status_to_mylite(sqlite_rc);
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);

    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    } else if (
        rc != MYLITE_OK &&
        mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) == MYLITE_OK
    ) {
        set_physical_sqlite_row_error(database);
        rc = MYLITE_ERROR;
    }

    return rc;
}

static int validate_alter_table_add_primary_key_duplicates(
    struct mylite_db *database,
    const struct planned_alter_table_add_primary_key *plan
) {
    sqlite3_stmt *statement = NULL;
    char duplicate_value[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    char *sql = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = build_alter_table_add_primary_key_duplicate_validation_sql(plan, &sql);

    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = format_sqlite_key_tuple(
                statement,
                NULL,
                plan->part_count,
                duplicate_value,
                sizeof(duplicate_value)
            );
            if (rc == MYLITE_OK) {
                set_duplicate_key_error(database, plan->table.name, "PRIMARY", duplicate_value);
                rc = MYLITE_ERROR;
            }
        } else if (sqlite_rc != SQLITE_DONE) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);

    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    } else if (
        rc != MYLITE_OK &&
        mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) == MYLITE_OK
    ) {
        set_physical_sqlite_row_error(database);
        rc = MYLITE_ERROR;
    }

    return rc;
}

static enum mylite_catalog_column_default_kind alter_table_primary_key_default_kind(
    const struct mylite_catalog_column_descriptor *column
) {
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER) {
        return MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER;
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER_EXPRESSION) {
        return MYLITE_CATALOG_COLUMN_DEFAULT_INTEGER_EXPRESSION;
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT) {
        return MYLITE_CATALOG_COLUMN_DEFAULT_TEXT;
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_TEXT_EXPRESSION) {
        return MYLITE_CATALOG_COLUMN_DEFAULT_TEXT_EXPRESSION;
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_BINARY) {
        return MYLITE_CATALOG_COLUMN_DEFAULT_BINARY;
    }

    return MYLITE_CATALOG_COLUMN_DEFAULT_NO_EXPLICIT;
}

static int execute_physical_alter_table_add_primary_key(
    struct mylite_db *database,
    const struct planned_alter_table_add_primary_key *plan,
    const char *index_physical_name
) {
    char *sql = NULL;
    int rc = build_alter_table_add_primary_key_index_sql(plan, index_physical_name, &sql);

    if (rc == MYLITE_OK) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);

    return rc;
}
