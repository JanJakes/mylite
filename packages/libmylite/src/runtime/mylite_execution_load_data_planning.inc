static int plan_load_data_infile(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_load_data_infile *out_plan
) {
    const struct mylite_sql_ast_node *file_name = child_at(statement, 0U);
    const struct mylite_sql_ast_node *table_name = child_at(statement, 1U);
    const struct mylite_sql_ast_node *ignore_lines = NULL;
    const struct mylite_sql_ast_node *column_list = NULL;
    struct primary_key_info primary_key = mylite_execution_primary_key_info_init();
    int rc = MYLITE_OK;

    *out_plan = (struct planned_load_data_infile){0};
    for (size_t child_index = 2U; child_index < mylite_sql_ast_node_child_count(statement);
         ++child_index) {
        const struct mylite_sql_ast_node *child = child_at(statement, child_index);

        if (child->kind == MYLITE_SQL_AST_LITERAL &&
            mylite_sql_ast_node_literal_kind(child) == MYLITE_SQL_AST_LITERAL_INTEGER) {
            ignore_lines = child;
        } else if (child->kind == MYLITE_SQL_AST_IDENTIFIER_LIST) {
            column_list = child;
        }
    }

    rc = decode_sql_string_literal(
        database,
        file_name,
        "LOAD DATA INFILE supports only string literal file names",
        "LOAD DATA INFILE file names do not support NUL bytes",
        &out_plan->file_path,
        &out_plan->file_path_length
    );
    if (rc == MYLITE_OK) {
        rc =
            parse_load_data_ignore_line_count(database, ignore_lines, &out_plan->ignore_line_count);
    }
    if (rc == MYLITE_OK) {
        rc = resolve_visible_writable_table_reference(
            database,
            table_name,
            &out_plan->insert.target,
            &out_plan->insert.table
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_columns(
            database,
            out_plan->insert.table.table_id,
            &out_plan->insert.columns,
            &out_plan->insert.column_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_primary_key_info(
            database,
            out_plan->insert.table.table_id,
            out_plan->insert.columns,
            out_plan->insert.column_count,
            &primary_key
        );
    }
    if (rc == MYLITE_OK && primary_key.has_primary_key) {
        out_plan->insert.has_primary_key = true;
        if (primary_key.part_count == 1U) {
            out_plan->insert.primary_key_column_index = primary_key.parts[0].column_index;
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_index_infos(
            database,
            out_plan->insert.table.table_id,
            out_plan->insert.columns,
            out_plan->insert.column_count,
            &out_plan->insert.indexes,
            &out_plan->insert.index_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = load_insert_target_foreign_key_infos(database, &out_plan->insert);
    }
    if (rc == MYLITE_OK) {
        rc = initialize_insert_auto_increment_plan(database, &out_plan->insert);
    }
    if (rc == MYLITE_OK) {
        rc = collect_load_data_target_indexes(
            database,
            column_list,
            &out_plan->insert,
            &out_plan->target_indexes,
            &out_plan->target_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = check_insert_target_duplicate(
            database,
            out_plan->insert.columns,
            out_plan->target_indexes,
            out_plan->target_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_load_data_target_columns(
            database,
            &out_plan->insert,
            out_plan->target_indexes,
            out_plan->target_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = check_insert_omitted_columns(
            database,
            &out_plan->insert,
            out_plan->target_indexes,
            out_plan->target_count
        );
    }

    mylite_execution_primary_key_info_deinit(&primary_key);
    return rc;
}

static void planned_load_data_infile_deinit(struct planned_load_data_infile *plan) {
    if (plan == NULL) {
        return;
    }

    planned_insert_deinit(&plan->insert);
    free(plan->file_path);
    free(plan->target_indexes);
    *plan = (struct planned_load_data_infile){0};
}

static int collect_load_data_target_indexes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_list,
    const struct planned_insert *plan,
    size_t **out_indexes,
    size_t *out_index_count
) {
    size_t explicit_column_count = mylite_sql_ast_node_child_count(column_list);
    size_t column_count = explicit_column_count;
    bool omitted_column_list = false;
    size_t *indexes = NULL;

    *out_indexes = NULL;
    *out_index_count = 0U;
    if (column_list != NULL && column_list->kind != MYLITE_SQL_AST_IDENTIFIER_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    omitted_column_list = insert_column_list_is_omitted(column_list);
    if (explicit_column_count == 0U) {
        if (!omitted_column_list) {
            return MYLITE_OK;
        }
        column_count = count_visible_insert_target_columns(plan);
    }
    if (column_count > SIZE_MAX / sizeof(*indexes)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (column_count == 0U) {
        return MYLITE_OK;
    }

    indexes = calloc(column_count, sizeof(*indexes));
    if (indexes == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    if (explicit_column_count == 0U) {
        collect_visible_insert_target_indexes(plan, indexes);
    } else {
        int rc = collect_explicit_insert_target_indexes(
            database,
            column_list,
            plan,
            indexes,
            column_count
        );

        if (rc != MYLITE_OK) {
            free(indexes);
            return rc;
        }
    }

    *out_indexes = indexes;
    *out_index_count = column_count;
    return MYLITE_OK;
}

static int validate_load_data_target_columns(
    struct mylite_db *database,
    const struct planned_insert *plan,
    const size_t *target_indexes,
    size_t target_count
) {
    for (size_t target_index = 0U; target_index < target_count; ++target_index) {
        const struct mylite_catalog_column_descriptor *column = NULL;
        size_t column_index = target_indexes[target_index];

        if (column_index >= plan->column_count) {
            set_runtime_error(database, "invalid LOAD DATA target column index");
            return MYLITE_ERROR;
        }
        column = &plan->columns[column_index];
        if (column->is_generated) {
            set_generated_column_value_error(database, column->name, plan->table.name);
            return MYLITE_ERROR;
        }
        if (strcmp(column->physical_type, "INTEGER") == 0 || column_descriptor_is_char(column) ||
            column_descriptor_is_varchar(column) || column_descriptor_is_text_family(column) ||
            column_descriptor_is_year(column) || column_descriptor_is_date(column) ||
            column_descriptor_is_time(column) || column_descriptor_is_datetime(column) ||
            column_descriptor_is_timestamp(column)) {
            continue;
        }
        set_unsupported_error(
            database,
            "LOAD DATA INFILE supports only baseline integer, character, and temporal targets"
        );
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int parse_load_data_ignore_line_count(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    uint64_t *out_value
) {
    *out_value = 0U;
    if (literal == NULL) {
        return MYLITE_OK;
    }
    if (literal->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER ||
        parse_unsigned_integer_literal(&literal->span, out_value) != MYLITE_OK) {
        set_unsupported_error(
            database,
            "LOAD DATA IGNORE LINES supports only unsigned integer literals"
        );
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}
