static int plan_insert_select(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_insert_select *out_plan
) {
    const struct mylite_sql_ast_node *column_list = child_at(statement, 1U);
    const struct mylite_sql_ast_node *source_statement = child_at(statement, 2U);
    struct primary_key_info primary_key = primary_key_info_init();
    bool ignore = child_with_kind(statement, MYLITE_SQL_AST_INSERT_IGNORE_MODIFIER) != NULL;
    enum planned_insert_select_source_kind source_kind =
        insert_select_source_kind(source_statement);
    bool uses_auto_increment = (statement->kind == MYLITE_SQL_AST_INSERT_SELECT_STATEMENT ||
                                statement->kind == MYLITE_SQL_AST_REPLACE_SELECT_STATEMENT) != 0;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_insert_select){0};
    out_plan->target.ignore_errors = ignore;
    out_plan->target.replace_existing_rows =
        statement->kind == MYLITE_SQL_AST_REPLACE_SELECT_STATEMENT;
    out_plan->source_kind = source_kind;
    if (source_kind == PLANNED_INSERT_SELECT_SOURCE_COMPOUND &&
        statement->kind == MYLITE_SQL_AST_REPLACE_SELECT_STATEMENT) {
        set_unsupported_error(database, "REPLACE ... SELECT does not support UNION sources");
        primary_key_info_deinit(&primary_key);
        return MYLITE_ERROR;
    }
    rc = plan_insert_select_target(database, statement, out_plan, &primary_key);
    if (rc == MYLITE_OK && uses_auto_increment) {
        rc = initialize_insert_auto_increment_plan(database, &out_plan->target);
    }
    if (rc == MYLITE_OK && uses_auto_increment) {
        rc = initialize_insert_auto_increment_range(database, &out_plan->target);
    }
    if (rc == MYLITE_OK) {
        rc = collect_insert_target_indexes(
            database,
            column_list,
            &out_plan->target,
            NULL,
            &out_plan->target_indexes,
            &out_plan->target_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = check_insert_target_duplicate(
            database,
            out_plan->target.columns,
            out_plan->target_indexes,
            out_plan->target_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = reject_insert_select_generated_targets(database, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = plan_insert_duplicate_update(database, statement, &out_plan->target);
    }
    if (rc == MYLITE_OK && out_plan->target.duplicate_update.has_clause &&
        out_plan->target.has_auto_increment) {
        set_unsupported_error(
            database,
            "INSERT ... SELECT ... ON DUPLICATE KEY UPDATE does not support AUTO_INCREMENT targets"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && ignore && source_kind == PLANNED_INSERT_SELECT_SOURCE_ROW_SCALAR) {
        set_unsupported_error(
            database,
            "INSERT IGNORE ... SELECT does not support row-scalar sources"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && ignore && source_kind == PLANNED_INSERT_SELECT_SOURCE_COMPOUND) {
        set_unsupported_error(database, "INSERT IGNORE ... SELECT does not support UNION sources");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = plan_insert_select_source(database, statement, source_kind, out_plan);
    }

    primary_key_info_deinit(&primary_key);
    return rc;
}

static int reject_insert_select_generated_targets(
    struct mylite_db *database,
    const struct planned_insert_select *plan
) {
    if (plan == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    for (size_t target_position = 0U; target_position < plan->target_count; ++target_position) {
        size_t column_index = plan->target_indexes[target_position];

        if (column_index >= plan->target.column_count) {
            set_runtime_error(database, "invalid INSERT ... SELECT target descriptor");
            return MYLITE_ERROR;
        }
        if (plan->target.columns[column_index].is_generated) {
            set_generated_column_value_error(
                database,
                plan->target.columns[column_index].name,
                plan->target.table.name
            );
            return MYLITE_ERROR;
        }
    }

    return MYLITE_OK;
}

static enum planned_insert_select_source_kind insert_select_source_kind(
    const struct mylite_sql_ast_node *source_statement
) {
    if (source_statement != NULL &&
        source_statement->kind == MYLITE_SQL_AST_COMPOUND_SELECT_STATEMENT) {
        return PLANNED_INSERT_SELECT_SOURCE_COMPOUND;
    }
    if (insert_select_source_is_row_scalar(source_statement)) {
        return PLANNED_INSERT_SELECT_SOURCE_ROW_SCALAR;
    }

    return PLANNED_INSERT_SELECT_SOURCE_TABLE;
}

static bool insert_select_source_is_row_scalar(const struct mylite_sql_ast_node *select_statement) {
    const struct mylite_sql_ast_node *from_clause = NULL;

    if (select_statement == NULL || select_statement->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
        return false;
    }

    from_clause = child_at(select_statement, 1U);
    return (from_clause == NULL || from_clause->kind == MYLITE_SQL_AST_FROM_DUAL) != 0;
}

static int plan_insert_select_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_insert_select *out_plan,
    struct primary_key_info *primary_key
) {
    int rc = resolve_visible_writable_table_reference(
        database,
        child_at(statement, 0U),
        &out_plan->target.target,
        &out_plan->target.table
    );

    if (rc == MYLITE_OK) {
        rc = load_table_columns(
            database,
            out_plan->target.table.table_id,
            &out_plan->target.columns,
            &out_plan->target.column_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = load_primary_key_info(
            database,
            out_plan->target.table.table_id,
            out_plan->target.columns,
            out_plan->target.column_count,
            primary_key
        );
    }
    if (rc == MYLITE_OK) {
        apply_insert_select_primary_key_info(primary_key, &out_plan->target);
        rc = load_table_index_infos(
            database,
            out_plan->target.table.table_id,
            out_plan->target.columns,
            out_plan->target.column_count,
            &out_plan->target.indexes,
            &out_plan->target.index_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = load_insert_target_foreign_key_infos(database, &out_plan->target);
    }

    return rc;
}

static void apply_insert_select_primary_key_info(
    const struct primary_key_info *primary_key,
    struct planned_insert *target
) {
    if (!primary_key->has_primary_key) {
        return;
    }

    target->has_primary_key = true;
    if (primary_key->part_count == 1U) {
        target->primary_key_column_index = primary_key->parts[0].column_index;
    }
}

static int plan_insert_select_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    enum planned_insert_select_source_kind source_kind,
    struct planned_insert_select *out_plan
) {
    const struct mylite_sql_ast_node *select_statement = child_at(statement, 2U);

    if (source_kind == PLANNED_INSERT_SELECT_SOURCE_ROW_SCALAR) {
        return plan_insert_select_row_scalar_source(database, select_statement, out_plan);
    }
    if (source_kind == PLANNED_INSERT_SELECT_SOURCE_COMPOUND) {
        return plan_insert_select_compound_source(database, statement, out_plan);
    }

    return plan_insert_select_table_source(database, statement, out_plan);
}

static int plan_insert_select_row_scalar_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_statement,
    struct planned_insert_select *out_plan
) {
    int rc = plan_row_scalar_select(database, select_statement, false, &out_plan->row_source);

    if (rc == MYLITE_OK && out_plan->row_source.item_count != out_plan->target_count) {
        set_column_count_mismatch_error(database, 1U);
        rc = MYLITE_ERROR;
    }

    return rc;
}

static int plan_insert_select_table_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_insert_select *out_plan
) {
    int rc = plan_select(database, child_at(statement, 2U), false, &out_plan->source);

    if (rc == MYLITE_OK && out_plan->source.calc_found_rows) {
        if (statement->kind == MYLITE_SQL_AST_REPLACE_SELECT_STATEMENT) {
            set_unsupported_error(
                database,
                "REPLACE ... SELECT does not support SQL_CALC_FOUND_ROWS"
            );
        } else {
            set_unsupported_error(
                database,
                "INSERT ... SELECT does not support SQL_CALC_FOUND_ROWS"
            );
        }
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && out_plan->source.source_count > 0U) {
        if (statement->kind == MYLITE_SQL_AST_REPLACE_SELECT_STATEMENT) {
            set_unsupported_error(database, "REPLACE ... SELECT does not support joined SELECT");
        } else {
            set_unsupported_error(database, "INSERT ... SELECT does not support joined SELECT");
        }
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && out_plan->source.column_count != out_plan->target_count) {
        set_column_count_mismatch_error(database, 1U);
        rc = MYLITE_ERROR;
    }

    return rc;
}

static int plan_insert_select_compound_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_insert_select *out_plan
) {
    const struct mylite_sql_ast_node *compound_statement = child_at(statement, 2U);
    const struct mylite_sql_ast_node *terms = child_at(compound_statement, 1U);
    const struct mylite_sql_ast_node *term = NULL;
    size_t branch_count = mylite_sql_ast_node_child_count(terms);
    size_t first_column_count = 0U;
    int rc = MYLITE_OK;

    if (compound_statement == NULL ||
        compound_statement->kind != MYLITE_SQL_AST_COMPOUND_SELECT_STATEMENT ||
        branch_count == SIZE_MAX) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    ++branch_count;
    out_plan->compound_source.branches =
        calloc(branch_count, sizeof(*out_plan->compound_source.branches));
    if (out_plan->compound_source.branches == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_plan->compound_source.branch_count = branch_count;

    rc = plan_insert_select_compound_branch(
        database,
        child_at(compound_statement, 0U),
        MYLITE_SQL_AST_UNION_MODIFIER_DISTINCT,
        &out_plan->compound_source.branches[0],
        &first_column_count
    );

    term = child_at(terms, 0U);
    for (size_t branch_index = 1U; rc == MYLITE_OK && branch_index < branch_count; ++branch_index) {
        size_t branch_column_count = 0U;

        if (mylite_sql_ast_node_set_operator_kind(term) != MYLITE_SQL_AST_SET_OPERATOR_UNION) {
            set_unsupported_error(
                database,
                "INSERT ... SELECT compound sources support only UNION"
            );
            rc = MYLITE_ERROR;
            break;
        }
        rc = plan_insert_select_compound_branch(
            database,
            child_at(term, 0U),
            mylite_sql_ast_node_union_modifier(term),
            &out_plan->compound_source.branches[branch_index],
            &branch_column_count
        );
        if (rc == MYLITE_OK && branch_column_count != first_column_count) {
            set_union_column_count_mismatch_error(database);
            rc = MYLITE_ERROR;
        }
        term = term->next_sibling;
    }
    if (rc == MYLITE_OK && first_column_count != out_plan->target_count) {
        set_column_count_mismatch_error(database, 1U);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = validate_insert_select_compound_target_compatibility(database, out_plan);
    }

    if (rc != MYLITE_OK) {
        planned_insert_select_compound_source_deinit(&out_plan->compound_source);
    }

    return rc;
}

static int plan_insert_select_compound_branch(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *branch_statement,
    enum mylite_sql_ast_union_modifier modifier,
    struct planned_insert_select_compound_branch *out_branch,
    size_t *out_column_count
) {
    int rc = reject_compound_select_branch_shape(
        database,
        branch_statement,
        MYLITE_SQL_AST_SET_OPERATOR_UNION
    );

    *out_branch = (struct planned_insert_select_compound_branch){.modifier = modifier};
    *out_column_count = 0U;
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (insert_select_source_is_row_scalar(branch_statement)) {
        out_branch->kind = PLANNED_INSERT_SELECT_COMPOUND_BRANCH_ROW_SCALAR;
        return plan_insert_select_compound_row_scalar_branch(
            database,
            branch_statement,
            out_branch,
            out_column_count
        );
    }

    out_branch->kind = PLANNED_INSERT_SELECT_COMPOUND_BRANCH_TABLE;
    return plan_insert_select_compound_table_branch(
        database,
        branch_statement,
        out_branch,
        out_column_count
    );
}

static int plan_insert_select_compound_table_branch(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *branch_statement,
    struct planned_insert_select_compound_branch *out_branch,
    size_t *out_column_count
) {
    int rc = plan_select(database, branch_statement, false, &out_branch->source);

    if (rc == MYLITE_OK && out_branch->source.source_count > 0U) {
        set_unsupported_error(database, "INSERT ... SELECT UNION does not support joined SELECT");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        *out_column_count = out_branch->source.column_count;
    }
    if (rc != MYLITE_OK) {
        planned_select_deinit(&out_branch->source);
    }

    return rc;
}

static int plan_insert_select_compound_row_scalar_branch(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *branch_statement,
    struct planned_insert_select_compound_branch *out_branch,
    size_t *out_column_count
) {
    int rc = plan_row_scalar_select(database, branch_statement, false, &out_branch->row_source);

    if (rc == MYLITE_OK) {
        *out_column_count = out_branch->row_source.item_count;
    }
    if (rc != MYLITE_OK) {
        planned_row_scalar_select_deinit(&out_branch->row_source);
    }

    return rc;
}

static int validate_insert_select_compound_branch_target_compatibility(
    struct mylite_db *database,
    const struct planned_insert_select *plan,
    const struct planned_select *source
) {
    for (size_t target_position = 0U; target_position < plan->target_count; ++target_position) {
        size_t column_index = plan->target_indexes[target_position];

        if (!insert_select_source_target_types_are_compatible(
                &source->columns[target_position],
                &plan->target.columns[column_index]
            )) {
            set_unsupported_error(
                database,
                insert_select_implicit_conversion_message(&plan->target.columns[column_index])
            );
            return MYLITE_ERROR;
        }
    }

    return MYLITE_OK;
}

static int validate_insert_select_compound_target_compatibility(
    struct mylite_db *database,
    const struct planned_insert_select *plan
) {
    int rc = MYLITE_OK;

    for (size_t branch_index = 0U;
         rc == MYLITE_OK && branch_index < plan->compound_source.branch_count;
         ++branch_index) {
        const struct planned_insert_select_compound_branch *branch =
            &plan->compound_source.branches[branch_index];

        if (branch->kind == PLANNED_INSERT_SELECT_COMPOUND_BRANCH_TABLE) {
            rc = validate_insert_select_compound_branch_target_compatibility(
                database,
                plan,
                &branch->source
            );
        }
    }

    return rc;
}

static void planned_insert_select_compound_source_deinit(
    struct planned_insert_select_compound_source *source
) {
    if (source == NULL) {
        return;
    }
    for (size_t branch_index = 0U; branch_index < source->branch_count; ++branch_index) {
        planned_select_deinit(&source->branches[branch_index].source);
        planned_row_scalar_select_deinit(&source->branches[branch_index].row_source);
    }
    free(source->branches);
    *source = (struct planned_insert_select_compound_source){0};
}

static void planned_insert_select_deinit(struct planned_insert_select *plan) {
    if (plan == NULL) {
        return;
    }

    planned_select_deinit(&plan->source);
    planned_row_scalar_select_deinit(&plan->row_source);
    planned_insert_select_compound_source_deinit(&plan->compound_source);
    planned_insert_deinit(&plan->target);
    free(plan->target_indexes);
    *plan = (struct planned_insert_select){0};
}

static int execute_insert_select_from_plan(
    struct mylite_db *database,
    const struct planned_insert_select *plan,
    mylite_result *result
) {
    if (plan->source_kind == PLANNED_INSERT_SELECT_SOURCE_ROW_SCALAR) {
        return execute_insert_select_row_scalar_from_plan(database, plan, result);
    }

    return execute_insert_select_table_from_plan(database, plan, result);
}

static int execute_insert_select_table_from_plan(
    struct mylite_db *database,
    const struct planned_insert_select *plan,
    mylite_result *result
) {
    struct insert_select_table_execution execution = {0};
    int rc = prepare_insert_select_table_execution(database, plan, &execution);

    if (rc == MYLITE_OK) {
        rc = execute_insert_select_table(database, plan, &execution);
    }
    if (rc == MYLITE_OK) {
        rc = finish_insert_select_table_result(database, &execution, result);
    }
    cleanup_insert_select_table_execution(database, &execution, rc);

    return normalize_insert_select_table_result(database, rc);
}

static int prepare_insert_select_table_execution(
    struct mylite_db *database,
    const struct planned_insert_select *plan,
    struct insert_select_table_execution *execution
) {
    int rc = build_insert_select_temp_table_name(
        database,
        execution->temporary_table_name,
        sizeof(execution->temporary_table_name)
    );

    if (rc == MYLITE_OK) {
        rc = build_insert_select_materialize_sql(
            plan,
            execution->temporary_table_name,
            &execution->materialize_sql
        );
    }
    if (rc == MYLITE_OK) {
        rc = build_insert_select_validation_sql(
            plan,
            execution->temporary_table_name,
            &execution->validation_sql
        );
    }
    if (rc == MYLITE_OK) {
        rc = build_insert_sql(&plan->target, &execution->insert_sql);
    }
    if (rc == MYLITE_OK) {
        rc = build_drop_temp_table_sql(execution->temporary_table_name, &execution->drop_sql);
    }

    return rc;
}

static int execute_insert_select_table(
    struct mylite_db *database,
    const struct planned_insert_select *plan,
    struct insert_select_table_execution *execution
) {
    int rc = begin_statement_transaction(database, &execution->transaction);

    if (rc == MYLITE_OK) {
        rc = execute_sqlite_control_sql(database, execution->drop_sql);
    }
    if (rc == MYLITE_OK) {
        rc = execute_insert_select_materialize(
            database,
            execution->materialize_sql,
            plan,
            &execution->temporary_table_created
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_insert_select_table_rows(database, plan, execution);
    }
    if (rc == MYLITE_OK) {
        rc = execute_insert_select_insert(
            database,
            execution->validation_sql,
            execution->insert_sql,
            plan,
            &execution->counters
        );
    }
    if (rc == MYLITE_OK) {
        rc = finish_insert_select_table_side_effects(database, plan, execution);
    }
    if (rc == MYLITE_OK) {
        rc = execute_sqlite_control_sql(database, execution->drop_sql);
    }
    if (rc == MYLITE_OK) {
        execution->temporary_table_created = false;
        rc = commit_statement_transaction(database, &execution->transaction);
    }
    if (rc != MYLITE_OK) {
        rollback_statement_transaction(database, &execution->transaction);
    }

    return rc;
}

static int validate_insert_select_table_rows(
    struct mylite_db *database,
    const struct planned_insert_select *plan,
    const struct insert_select_table_execution *execution
) {
    sqlite3_stmt *validation_statement = NULL;
    int rc = prepare_sqlite_statement(database, execution->validation_sql, &validation_statement);

    if (rc == MYLITE_OK) {
        rc = validate_insert_select_rows(database, validation_statement, plan);
    }

    return finalize_sqlite_statement(validation_statement, rc);
}

static int finish_insert_select_table_side_effects(
    struct mylite_db *database,
    const struct planned_insert_select *plan,
    struct insert_select_table_execution *execution
) {
    int rc = MYLITE_OK;

    if (plan->target.has_auto_increment &&
        execution->counters.auto_increment_next_after_rows != plan->target.auto_increment_next) {
        rc = update_table_auto_increment_next(
            database,
            &plan->target.table,
            execution->counters.auto_increment_next_after_rows
        );
    }
    if (rc == MYLITE_OK) {
        rc = touch_persistent_table_updated_time(
            database,
            &plan->target.table,
            execution->counters.affected_rows,
            false
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_child_foreign_keys_after_write(database, plan->target.table.table_id);
    }

    return rc;
}

static void cleanup_insert_select_table_execution(
    struct mylite_db *database,
    struct insert_select_table_execution *execution,
    int rc
) {
    if (rc != MYLITE_OK && execution->temporary_table_created) {
        (void)execute_sqlite_control_sql(database, execution->drop_sql);
    }
    free(execution->materialize_sql);
    free(execution->validation_sql);
    free(execution->insert_sql);
    free(execution->drop_sql);
    *execution = (struct insert_select_table_execution){0};
}

static int normalize_insert_select_table_result(struct mylite_db *database, int rc) {
    if (rc == MYLITE_OK) {
        return MYLITE_OK;
    }
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
        return rc;
    }
    if (mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) != MYLITE_OK) {
        return rc;
    }

    set_physical_sqlite_row_error(database);
    return MYLITE_ERROR;
}

static int finish_insert_select_table_result(
    struct mylite_db *database,
    const struct insert_select_table_execution *execution,
    mylite_result *result
) {
    mylite_result_set_affected_rows(result, execution->counters.affected_rows);
    if (execution->counters.inserted_generated_auto_increment) {
        database->session.last_insert_id =
            (uint64_t)execution->counters.first_inserted_generated_auto_increment;
    }

    return MYLITE_OK;
}

static int execute_insert_select_row_scalar_from_plan(
    struct mylite_db *database,
    const struct planned_insert_select *plan,
    mylite_result *result
) {
    sqlite3_stmt *statement = NULL;
    struct planned_insert_row row = {0};
    char *sql = NULL;
    bool has_row = false;
    int rc = prepare_insert_select_row_scalar_source(database, plan, &statement, &sql, &has_row);

    if (rc == MYLITE_OK && has_row) {
        rc = materialize_insert_select_row_scalar_row(database, statement, plan, &row);
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);
    if (rc == MYLITE_OK) {
        rc = finish_insert_select_row_scalar_result(database, plan, has_row, &row, result);
    }

    planned_insert_row_deinit(&row, plan->target.column_count);
    return normalize_insert_select_row_scalar_result(database, rc);
}

static int prepare_insert_select_row_scalar_source(
    struct mylite_db *database,
    const struct planned_insert_select *plan,
    sqlite3_stmt **out_statement,
    char **out_sql,
    bool *out_has_row
) {
    int sqlite_rc = SQLITE_OK;
    int rc = MYLITE_OK;

    *out_statement = NULL;
    *out_sql = NULL;
    *out_has_row = false;

    rc = build_row_scalar_select_sql(&plan->row_source, out_sql);
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, *out_sql, out_statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_row_scalar_select_parameters(*out_statement, &plan->row_source);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    sqlite_rc = sqlite3_step(*out_statement);
    if (sqlite_rc == SQLITE_ROW) {
        *out_has_row = true;
        return MYLITE_OK;
    }
    if (sqlite_rc == SQLITE_DONE) {
        return MYLITE_OK;
    }

    return mylite_sqlite_status_to_mylite(sqlite_rc);
}

static int materialize_insert_select_row_scalar_row(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_insert_select *plan,
    struct planned_insert_row *row
) {
    int sqlite_rc = SQLITE_OK;
    int rc = MYLITE_OK;

    if (plan->target.column_count > SIZE_MAX / sizeof(*row->values)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    row->values = calloc(plan->target.column_count, sizeof(*row->values));
    if (row->values == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = check_insert_omitted_columns(
        database,
        &plan->target,
        plan->target_indexes,
        plan->target_count
    );
    if (rc == MYLITE_OK && insert_select_allows_adjustment(database, plan)) {
        rc = append_insert_omitted_column_warnings(
            database,
            &plan->target,
            plan->target_indexes,
            plan->target_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_insert_select_row_scalar_row(database, statement, plan, 1U);
    }
    if (rc == MYLITE_OK) {
        rc = materialize_insert_select_row_values(database, statement, plan, 1U, row->values);
    }

    if (rc != MYLITE_OK) {
        return rc;
    }
    sqlite_rc = sqlite3_step(statement);
    if (sqlite_rc == SQLITE_ROW) {
        set_unsupported_error(
            database,
            "INSERT ... SELECT row-scalar source returned too many rows"
        );
        return MYLITE_ERROR;
    }
    if (sqlite_rc != SQLITE_DONE) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    return MYLITE_OK;
}

static int finish_insert_select_row_scalar_result(
    struct mylite_db *database,
    const struct planned_insert_select *plan,
    bool has_row,
    struct planned_insert_row *row,
    mylite_result *result
) {
    if (has_row) {
        return execute_insert_select_row_scalar_insert(database, plan, row, result);
    }

    mylite_result_set_affected_rows(result, 0);
    return MYLITE_OK;
}

static void planned_insert_row_deinit(struct planned_insert_row *row, size_t column_count) {
    if (row == NULL || row->values == NULL) {
        return;
    }

    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        planned_value_deinit(&row->values[column_index]);
    }
    free(row->values);
    *row = (struct planned_insert_row){0};
}

static int normalize_insert_select_row_scalar_result(struct mylite_db *database, int rc) {
    if (rc == MYLITE_OK) {
        return MYLITE_OK;
    }
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
        return rc;
    }
    if (mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) != MYLITE_OK) {
        return rc;
    }

    set_physical_sqlite_row_error(database);
    return MYLITE_ERROR;
}

static bool insert_select_allows_adjustment(
    const struct mylite_db *database,
    const struct planned_insert_select *plan
) {
    if (plan == NULL) {
        return false;
    }
    return dml_allows_missing_default_adjustment(database, plan->target.ignore_errors);
}

static int validate_insert_select_row_scalar_row(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_insert_select *plan,
    size_t row_number
) {
    bool adjust_value = insert_select_allows_adjustment(database, plan);

    if (plan->target_count > (size_t)INT_MAX) {
        return MYLITE_ERROR;
    }
    for (size_t target_position = 0U; target_position < plan->target_count; ++target_position) {
        size_t column_index = plan->target_indexes[target_position];
        const struct mylite_catalog_column_descriptor *target_column =
            &plan->target.columns[column_index];
        int rc = MYLITE_OK;

        if (column_descriptor_is_auto_increment(target_column) &&
            sqlite3_column_type(statement, (int)target_position) == SQLITE_NULL) {
            continue;
        }
        rc = validate_insert_select_value(
            database,
            statement,
            (int)target_position,
            NULL,
            target_column,
            row_number,
            adjust_value
        );

        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return MYLITE_OK;
}

static int execute_insert_select_row_scalar_insert(
    struct mylite_db *database,
    const struct planned_insert_select *plan,
    struct planned_insert_row *row,
    mylite_result *result
) {
    struct planned_insert row_plan = plan->target;
    int rc = MYLITE_OK;

    row_plan.rows = row;
    row_plan.row_count = 1U;
    rc = plan_insert_auto_increment_values(database, &row_plan);
    if (rc == MYLITE_OK) {
        rc = plan_insert_string_key_values(database, &row_plan);
    }
    if (rc == MYLITE_OK) {
        rc = execute_insert_from_plan(database, &row_plan, result);
    }

    return rc;
}

static int execute_insert_select_materialize(
    struct mylite_db *database,
    const char *materialize_sql,
    const struct planned_insert_select *plan,
    bool *out_temporary_table_created
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = MYLITE_OK;

    *out_temporary_table_created = false;
    rc = prepare_sqlite_statement(database, materialize_sql, &statement);
    if (rc == MYLITE_OK) {
        rc = bind_insert_select_materialize_parameters(statement, plan);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            *out_temporary_table_created = true;
        } else {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }

    return finalize_sqlite_statement(statement, rc);
}

static int execute_insert_select_insert(
    struct mylite_db *database,
    const char *validation_sql,
    const char *insert_sql,
    const struct planned_insert_select *plan,
    struct insert_execution_counters *counters
) {
    struct insert_select_execution_statements statements = {0};
    int rc = MYLITE_OK;

    *counters = (struct insert_execution_counters){0};
    if (plan->target.has_auto_increment) {
        counters->auto_increment_next_after_rows = plan->target.auto_increment_next;
    }
    rc = prepare_sqlite_statement(database, validation_sql, &statements.select_statement);
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, insert_sql, &statements.insert_statement);
    }
    if (rc == MYLITE_OK) {
        rc = execute_insert_select_insert_rows(database, &statements, plan, counters);
    }
    rc = finalize_sqlite_statement(statements.insert_statement, rc);
    statements.insert_statement = NULL;

    return finalize_sqlite_statement(statements.select_statement, rc);
}

static int execute_insert_select_insert_rows(
    struct mylite_db *database,
    struct insert_select_execution_statements *statements,
    const struct planned_insert_select *plan,
    struct insert_execution_counters *counters
) {
    size_t row_number = 0U;
    int sqlite_rc = SQLITE_OK;
    int rc = MYLITE_OK;

    while ((sqlite_rc = sqlite3_step(statements->select_statement)) == SQLITE_ROW) {
        if (row_number == SIZE_MAX) {
            set_unsupported_error(database, "INSERT ... SELECT selected too many rows");
            return MYLITE_ERROR;
        }
        ++row_number;
        rc = execute_insert_select_insert_row(database, statements, plan, row_number, counters);
        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    if (sqlite_rc != SQLITE_DONE) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    return MYLITE_OK;
}

static int execute_insert_select_insert_row(
    struct mylite_db *database,
    struct insert_select_execution_statements *statements,
    const struct planned_insert_select *plan,
    size_t row_number,
    struct insert_execution_counters *counters
) {
    struct planned_insert row_plan = plan->target;
    struct planned_insert_row row = {0};
    int rc = MYLITE_OK;

    row.values = calloc(plan->target.column_count, sizeof(*row.values));
    if (row.values == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = materialize_insert_select_row_values(
        database,
        statements->select_statement,
        plan,
        row_number,
        row.values
    );
    if (rc == MYLITE_OK) {
        row_plan.rows = &row;
        row_plan.row_count = 1U;
        rc = plan_insert_select_auto_increment_row_value(database, &row_plan, counters);
    }
    if (rc == MYLITE_OK) {
        rc = plan_insert_string_key_values(database, &row_plan);
    }
    if (rc == MYLITE_OK) {
        rc = execute_insert_plan_rows(database, statements->insert_statement, &row_plan, counters);
    }
    for (size_t column_index = 0U; column_index < plan->target.column_count; ++column_index) {
        planned_value_deinit(&row.values[column_index]);
    }
    free(row.values);

    return rc;
}

static int plan_insert_select_auto_increment_row_value(
    struct mylite_db *database,
    struct planned_insert *row_plan,
    struct insert_execution_counters *counters
) {
    int64_t next_value = 0;

    if (row_plan == NULL || counters == NULL || !row_plan->has_auto_increment) {
        return MYLITE_OK;
    }

    next_value = counters->auto_increment_next_after_rows;
    return plan_insert_auto_increment_row_value(
        database,
        row_plan,
        &row_plan->auto_increment_range,
        0U,
        &next_value
    );
}

static int materialize_insert_select_row_values(
    struct mylite_db *database,
    sqlite3_stmt *select_statement,
    const struct planned_insert_select *plan,
    size_t row_number,
    struct planned_value *values
) {
    bool adjust_value = insert_select_allows_adjustment(database, plan);
    int rc = MYLITE_OK;

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->target.column_count;
         ++column_index) {
        size_t target_position = 0U;

        if (find_insert_select_target_position(plan, column_index, &target_position)) {
            const struct mylite_catalog_column_descriptor *source_column = NULL;

            if (target_position >= (size_t)INT_MAX) {
                return MYLITE_ERROR;
            }
            rc = insert_select_source_column_for_position(
                database,
                select_statement,
                plan,
                target_position,
                &source_column
            );
            if (rc == MYLITE_OK) {
                rc = materialize_insert_select_selected_value(
                    database,
                    select_statement,
                    (int)target_position,
                    source_column,
                    &plan->target.columns[column_index],
                    row_number,
                    adjust_value,
                    &values[column_index]
                );
            }
        } else {
            rc = materialize_insert_select_omitted_value(
                database,
                plan,
                column_index,
                &values[column_index]
            );
        }
    }

    return rc;
}

static int materialize_insert_select_selected_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *source_column,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number,
    bool adjust_value,
    struct planned_value *out_value
) {
    if (sqlite3_column_type(statement, selected_column_index) == SQLITE_NULL) {
        if (column_descriptor_is_auto_increment(target_column)) {
            *out_value = (struct planned_value){.is_null = true};
            return MYLITE_OK;
        }
        if (adjust_value && !target_column->is_nullable) {
            return convert_null_insert_value(database, target_column, true, out_value);
        }
        *out_value = (struct planned_value){.is_null = true};
        return MYLITE_OK;
    }
    if (sqlite3_column_type(statement, selected_column_index) == SQLITE_INTEGER &&
        target_column->physical_type[0] != '\0' &&
        strcmp(target_column->physical_type, "INTEGER") == 0) {
        return materialize_insert_select_integer_value(
            database,
            statement,
            selected_column_index,
            target_column,
            row_number,
            adjust_value,
            out_value
        );
    }
    if (column_descriptor_is_binary_string_family(target_column)) {
        return materialize_sqlite_binary_string_value(
            database,
            statement,
            selected_column_index,
            target_column,
            row_number,
            out_value
        );
    }
    if (column_descriptor_is_string_family(target_column)) {
        return materialize_insert_select_string_value(
            database,
            statement,
            selected_column_index,
            source_column,
            target_column,
            row_number,
            adjust_value,
            out_value
        );
    }
    if (column_descriptor_is_bit(target_column)) {
        return materialize_sqlite_bit_value(
            database,
            statement,
            selected_column_index,
            target_column,
            row_number,
            out_value
        );
    }

    return materialize_insert_select_raw_value(
        database,
        statement,
        selected_column_index,
        out_value
    );
}

static int materialize_insert_select_integer_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number,
    bool adjust_value,
    struct planned_value *out_value
) {
    const uint64_t int64_negative_abs_max = 9223372036854775808ULL;
    int64_t selected_value = (int64_t)sqlite3_column_int64(statement, selected_column_index);
    bool is_negative = selected_value < 0;
    uint64_t magnitude = (uint64_t)selected_value;

    if (is_negative) {
        if (selected_value == INT64_MIN) {
            magnitude = int64_negative_abs_max;
        } else {
            magnitude = (uint64_t)(-selected_value);
        }
    }

    out_value->is_null = false;
    out_value->is_text = false;
    out_value->is_blob = false;
    out_value->is_real = false;
    return convert_integer_for_column_with_policy(
        database,
        magnitude,
        is_negative,
        target_column,
        row_number,
        adjust_value,
        &out_value->integer
    );
}

static int materialize_insert_select_string_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *source_column,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number,
    bool adjust_value,
    struct planned_value *out_value
) {
    const unsigned char *text = NULL;
    int byte_count = 0;
    char *copy = NULL;
    size_t text_length = 0U;
    struct char_text_conversion conversion = {0};
    int rc = MYLITE_OK;

    if (sqlite3_column_type(statement, selected_column_index) != SQLITE_TEXT) {
        set_unsupported_error(
            database,
            "INSERT ... SELECT does not support implicit string conversion"
        );
        return MYLITE_ERROR;
    }

    text = sqlite3_column_text(statement, selected_column_index);
    byte_count = sqlite3_column_bytes(statement, selected_column_index);
    if (text == NULL || byte_count < 0) {
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }
    text_length = (size_t)byte_count;
    copy = malloc(text_length + 1U);
    if (copy == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(copy, text, text_length);
    copy[text_length] = '\0';

    conversion = (struct char_text_conversion){
        .text = copy,
        .text_length = text_length,
        .row_number = row_number,
    };
    if (column_descriptor_is_char(target_column)) {
        rc = convert_char_text(database, target_column, &conversion, adjust_value);
    } else if (column_descriptor_is_varchar(target_column)) {
        rc = convert_varchar_text(database, target_column, &conversion, adjust_value);
    } else {
        rc = convert_text_family_text(
            database,
            target_column,
            &conversion,
            adjust_value,
            source_column != NULL
        );
    }
    if (rc != MYLITE_OK) {
        free(copy);
        return rc;
    }

    return assign_text_value(database, copy, conversion.text_length, out_value);
}

static int materialize_sqlite_binary_string_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number,
    struct planned_value *out_value
) {
    const void *bytes = sqlite3_column_blob(statement, selected_column_index);
    int byte_count = sqlite3_column_bytes(statement, selected_column_index);
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
    rc = convert_binary_bytes_for_column(
        database,
        target_column,
        row_number,
        false,
        &copy,
        &copy_length
    );
    if (rc != MYLITE_OK) {
        free(copy);
        return rc;
    }

    return assign_blob_value(database, copy, copy_length, out_value);
}

static int materialize_sqlite_bit_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number,
    struct planned_value *out_value
) {
    const void *bytes = sqlite3_column_blob(statement, selected_column_index);
    int byte_count = sqlite3_column_bytes(statement, selected_column_index);
    int rc = validate_insert_select_bit_value(
        database,
        statement,
        selected_column_index,
        target_column,
        row_number
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    return copy_blob_value(database, bytes, (size_t)byte_count, out_value);
}

static int materialize_insert_select_raw_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    struct planned_value *out_value
) {
    int sqlite_type = sqlite3_column_type(statement, selected_column_index);

    switch (sqlite_type) {
    case SQLITE_INTEGER:
        *out_value = (struct planned_value){
            .is_null = false,
            .is_text = false,
            .integer = (int64_t)sqlite3_column_int64(statement, selected_column_index),
        };
        return MYLITE_OK;
    case SQLITE_FLOAT:
        *out_value = (struct planned_value){
            .is_null = false,
            .is_text = false,
            .is_real = true,
            .real = sqlite3_column_double(statement, selected_column_index),
        };
        return MYLITE_OK;
    case SQLITE_TEXT: {
        const unsigned char *text = sqlite3_column_text(statement, selected_column_index);
        int byte_count = sqlite3_column_bytes(statement, selected_column_index);
        char *copy = NULL;

        if (text == NULL || byte_count < 0) {
            set_physical_sqlite_row_error(database);
            return MYLITE_ERROR;
        }
        copy = malloc((size_t)byte_count + 1U);
        if (copy == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        memcpy(copy, text, (size_t)byte_count);
        copy[(size_t)byte_count] = '\0';
        return assign_text_value(database, copy, (size_t)byte_count, out_value);
    }
    case SQLITE_BLOB: {
        const void *bytes = sqlite3_column_blob(statement, selected_column_index);
        int byte_count = sqlite3_column_bytes(statement, selected_column_index);

        if ((bytes == NULL && byte_count != 0) || byte_count < 0) {
            set_physical_sqlite_row_error(database);
            return MYLITE_ERROR;
        }
        return copy_blob_value(database, bytes, (size_t)byte_count, out_value);
    }
    case SQLITE_NULL:
        *out_value = (struct planned_value){.is_null = true};
        return MYLITE_OK;
    default:
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }
}

static int materialize_insert_select_omitted_value(
    struct mylite_db *database,
    const struct planned_insert_select *plan,
    size_t column_index,
    struct planned_value *out_value
) {
    const struct mylite_catalog_column_descriptor *target_column = NULL;

    if (plan == NULL || column_index >= plan->target.column_count) {
        set_runtime_error(database, "invalid INSERT ... SELECT omitted target column");
        return MYLITE_ERROR;
    }
    target_column = &plan->target.columns[column_index];
    if (column_descriptor_is_auto_increment(target_column)) {
        *out_value = (struct planned_value){.is_null = true};
        return MYLITE_OK;
    }
    if (insert_select_allows_adjustment(database, plan)) {
        return allocate_insert_column_value(database, &plan->target, column_index, out_value);
    }

    return materialize_dml_default_value(database, target_column, false, out_value);
}

static int validate_insert_select_rows(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_insert_select *plan
) {
    bool checked_omitted_columns = false;
    size_t row_number = 0U;
    int sqlite_rc = SQLITE_OK;
    int rc = MYLITE_OK;

    while ((sqlite_rc = sqlite3_step(statement)) == SQLITE_ROW) {
        if (!checked_omitted_columns) {
            rc = check_insert_omitted_columns(
                database,
                &plan->target,
                plan->target_indexes,
                plan->target_count
            );
            checked_omitted_columns = true;
            if (rc == MYLITE_OK && insert_select_allows_adjustment(database, plan)) {
                rc = append_insert_omitted_column_warnings(
                    database,
                    &plan->target,
                    plan->target_indexes,
                    plan->target_count
                );
            }
        }
        if (rc == MYLITE_OK && row_number == SIZE_MAX) {
            set_unsupported_error(database, "INSERT ... SELECT selected too many rows");
            rc = MYLITE_ERROR;
        }
        if (rc == MYLITE_OK) {
            ++row_number;
            rc = validate_insert_select_row(database, statement, plan, row_number);
        }
        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    if (sqlite_rc != SQLITE_DONE) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    return MYLITE_OK;
}

static int validate_insert_select_row(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_insert_select *plan,
    size_t row_number
) {
    bool adjust_value = insert_select_allows_adjustment(database, plan);

    if (plan->target_count > (size_t)INT_MAX) {
        return MYLITE_ERROR;
    }
    for (size_t target_position = 0U; target_position < plan->target_count; ++target_position) {
        size_t column_index = plan->target_indexes[target_position];
        int rc = MYLITE_OK;

        if (column_descriptor_is_auto_increment(&plan->target.columns[column_index]) &&
            sqlite3_column_type(statement, (int)target_position) == SQLITE_NULL) {
            continue;
        }
        const struct mylite_catalog_column_descriptor *source_column = NULL;

        rc = insert_select_source_column_for_position(
            database,
            statement,
            plan,
            target_position,
            &source_column
        );
        if (rc == MYLITE_OK) {
            rc = validate_insert_select_value(
                database,
                statement,
                (int)target_position,
                source_column,
                &plan->target.columns[column_index],
                row_number,
                adjust_value
            );
        }

        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return MYLITE_OK;
}

static int insert_select_source_column_for_position(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_insert_select *plan,
    size_t target_position,
    const struct mylite_catalog_column_descriptor **out_source_column
) {
    sqlite3_int64 branch_index = 0;
    const struct planned_insert_select_compound_branch *branch = NULL;

    *out_source_column = NULL;
    if (plan == NULL || statement == NULL) {
        set_runtime_error(database, "invalid INSERT ... SELECT source validation");
        return MYLITE_ERROR;
    }
    if (plan->source_kind == PLANNED_INSERT_SELECT_SOURCE_TABLE) {
        if (target_position >= plan->source.column_count) {
            set_runtime_error(database, "invalid INSERT ... SELECT source column");
            return MYLITE_ERROR;
        }
        *out_source_column = &plan->source.columns[target_position];
        return MYLITE_OK;
    }
    if (plan->source_kind != PLANNED_INSERT_SELECT_SOURCE_COMPOUND) {
        return MYLITE_OK;
    }
    if (plan->target_count > (size_t)INT_MAX ||
        sqlite3_column_type(statement, (int)plan->target_count) != SQLITE_INTEGER) {
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }

    branch_index = sqlite3_column_int64(statement, (int)plan->target_count);
    if (branch_index < 0 ||
        (uint64_t)branch_index >= (uint64_t)plan->compound_source.branch_count) {
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }
    branch = &plan->compound_source.branches[(size_t)branch_index];
    if (branch->kind != PLANNED_INSERT_SELECT_COMPOUND_BRANCH_TABLE) {
        return MYLITE_OK;
    }
    if (target_position >= branch->source.column_count) {
        set_runtime_error(database, "invalid INSERT ... SELECT UNION source column");
        return MYLITE_ERROR;
    }

    *out_source_column = &branch->source.columns[target_position];
    return MYLITE_OK;
}

static int validate_insert_select_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *source_column,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number,
    bool adjust_value
) {
    int sqlite_type = sqlite3_column_type(statement, selected_column_index);

    if (source_column != NULL &&
        !insert_select_source_target_types_are_compatible(source_column, target_column)) {
        set_unsupported_error(database, insert_select_implicit_conversion_message(target_column));
        return MYLITE_ERROR;
    }
    if (sqlite_type == SQLITE_NULL) {
        if (!target_column->is_nullable) {
            if (adjust_value) {
                return MYLITE_OK;
            }
            if (column_descriptor_is_spatial(target_column)) {
                set_spatial_bad_null_error(database, target_column->name);
                return MYLITE_ERROR;
            }
            set_bad_null_error(database, target_column->name);
            return MYLITE_ERROR;
        }
        return MYLITE_OK;
    }
    if (column_descriptor_is_string_family(target_column)) {
        return validate_insert_select_convertible_string_value(
            database,
            statement,
            selected_column_index,
            (struct insert_select_string_validation){
                .source_column = source_column,
                .target_column = target_column,
                .row_number = row_number,
                .adjust_value = adjust_value,
            }
        );
    }
    if (column_descriptor_is_json(target_column)) {
        (void)row_number;
        return validate_insert_select_json_value(
            database,
            statement,
            selected_column_index,
            target_column
        );
    }
    if (column_descriptor_is_binary_string_family(target_column)) {
        return validate_insert_select_binary_string_value(
            database,
            statement,
            selected_column_index,
            target_column,
            row_number
        );
    }
    if (column_descriptor_is_bit(target_column)) {
        return validate_insert_select_bit_value(
            database,
            statement,
            selected_column_index,
            target_column,
            row_number
        );
    }
    if (column_descriptor_is_year(target_column)) {
        return validate_insert_select_year_value(
            database,
            statement,
            selected_column_index,
            target_column,
            row_number
        );
    }
    if (column_descriptor_is_decimal(target_column)) {
        return validate_insert_select_decimal_value(
            database,
            statement,
            selected_column_index,
            target_column,
            row_number
        );
    }
    if (column_descriptor_is_approximate(target_column)) {
        return validate_insert_select_approximate_value(
            database,
            statement,
            selected_column_index,
            target_column,
            row_number
        );
    }
    if (column_descriptor_is_date(target_column)) {
        return validate_insert_select_date_value(
            database,
            statement,
            selected_column_index,
            target_column,
            row_number
        );
    }
    if (column_descriptor_is_time(target_column)) {
        return validate_insert_select_time_value(
            database,
            statement,
            selected_column_index,
            target_column,
            row_number
        );
    }
    if (column_descriptor_is_datetime(target_column)) {
        return validate_insert_select_datetime_value(
            database,
            statement,
            selected_column_index,
            target_column,
            row_number
        );
    }
    if (column_descriptor_is_timestamp(target_column)) {
        return validate_insert_select_timestamp_value(
            database,
            statement,
            selected_column_index,
            target_column,
            row_number
        );
    }

    return validate_insert_select_integer_value(
        database,
        statement,
        selected_column_index,
        target_column,
        row_number,
        adjust_value
    );
}

static bool insert_select_source_target_types_are_compatible(
    const struct mylite_catalog_column_descriptor *source_column,
    const struct mylite_catalog_column_descriptor *target_column
) {
    if (source_column == NULL || target_column == NULL) {
        return false;
    }
    if (column_descriptor_is_string_family(target_column)) {
        return column_descriptor_is_string_family(source_column);
    }
    if (column_descriptor_is_json(target_column)) {
        return column_descriptor_is_json(source_column);
    }
    if (column_descriptor_is_enum(target_column)) {
        return false;
    }
    if (column_descriptor_is_set(target_column)) {
        return false;
    }
    if (column_descriptor_is_binary_string_family(target_column)) {
        return column_descriptor_is_binary_string_family(source_column);
    }
    if (column_descriptor_is_bit(target_column)) {
        return (column_descriptor_is_bit(source_column) &&
                strcmp(source_column->logical_type, target_column->logical_type) == 0) != 0;
    }
    if (column_descriptor_is_year(target_column)) {
        return column_descriptor_is_year(source_column);
    }
    if (column_descriptor_is_decimal(target_column)) {
        return column_descriptor_is_decimal(source_column);
    }
    if (column_descriptor_is_approximate(target_column)) {
        return (column_descriptor_is_approximate(source_column) &&
                strcmp(source_column->logical_type, target_column->logical_type) == 0) != 0;
    }
    if (column_descriptor_is_date(target_column)) {
        return column_descriptor_is_date(source_column);
    }
    if (column_descriptor_is_time(target_column)) {
        return column_descriptor_is_time(source_column);
    }
    if (column_descriptor_is_datetime(target_column)) {
        return column_descriptor_is_datetime(source_column);
    }
    if (column_descriptor_is_timestamp(target_column)) {
        return column_descriptor_is_timestamp(source_column);
    }

    return (source_column->physical_type[0] != '\0' &&
            strcmp(source_column->physical_type, "INTEGER") == 0) != 0;
}

static const char *insert_select_implicit_conversion_message(
    const struct mylite_catalog_column_descriptor *target_column
) {
    if (column_descriptor_is_string_family(target_column)) {
        return "INSERT ... SELECT does not support implicit string conversion";
    }
    if (column_descriptor_is_json(target_column)) {
        return "INSERT ... SELECT does not support implicit JSON conversion";
    }
    if (column_descriptor_is_enum(target_column)) {
        return "INSERT ... SELECT does not support implicit ENUM conversion";
    }
    if (column_descriptor_is_set(target_column)) {
        return "INSERT ... SELECT does not support implicit SET conversion";
    }
    if (column_descriptor_is_binary_string_family(target_column)) {
        return "INSERT ... SELECT does not support implicit binary string conversion";
    }
    if (column_descriptor_is_bit(target_column)) {
        return "INSERT ... SELECT does not support implicit BIT conversion";
    }
    if (column_descriptor_is_year(target_column)) {
        return "INSERT ... SELECT does not support implicit YEAR conversion";
    }
    if (column_descriptor_is_decimal(target_column)) {
        return "INSERT ... SELECT does not support implicit DECIMAL conversion";
    }
    if (column_descriptor_is_approximate(target_column)) {
        return "INSERT ... SELECT does not support implicit FLOAT or DOUBLE conversion";
    }
    if (column_descriptor_is_date(target_column)) {
        return "INSERT ... SELECT does not support implicit DATE conversion";
    }
    if (column_descriptor_is_time(target_column)) {
        return "INSERT ... SELECT does not support implicit TIME conversion";
    }
    if (column_descriptor_is_datetime(target_column)) {
        return "INSERT ... SELECT does not support implicit DATETIME conversion";
    }
    if (column_descriptor_is_timestamp(target_column)) {
        return "INSERT ... SELECT does not support implicit TIMESTAMP conversion";
    }

    return "INSERT ... SELECT does not support implicit integer conversion";
}

static int validate_insert_select_convertible_string_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    struct insert_select_string_validation validation
) {
    const unsigned char *text = NULL;
    int byte_count = 0;
    size_t text_length = 0U;

    if (sqlite3_column_type(statement, selected_column_index) != SQLITE_TEXT) {
        set_unsupported_error(
            database,
            "INSERT ... SELECT does not support implicit string conversion"
        );
        return MYLITE_ERROR;
    }
    text = sqlite3_column_text(statement, selected_column_index);
    byte_count = sqlite3_column_bytes(statement, selected_column_index);
    if (text == NULL || byte_count < 0) {
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }
    text_length = (size_t)byte_count;
    if (memchr(text, '\0', text_length) != NULL) {
        set_unsupported_error(database, "string values do not support NUL bytes");
        return MYLITE_ERROR;
    }

    if (column_descriptor_is_varchar(validation.target_column)) {
        return validate_insert_select_convertible_varchar_value(
            database,
            (const char *)text,
            text_length,
            validation
        );
    }
    if (column_descriptor_is_char(validation.target_column)) {
        return validate_insert_select_convertible_char_value(
            database,
            (const char *)text,
            text_length,
            validation
        );
    }
    if (column_descriptor_is_text_family(validation.target_column)) {
        return validate_insert_select_convertible_text_family_value(
            database,
            (const char *)text,
            text_length,
            validation
        );
    }

    set_unsupported_error(
        database,
        "INSERT ... SELECT does not support implicit string conversion"
    );
    return MYLITE_ERROR;
}

static int validate_insert_select_convertible_varchar_value(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    struct insert_select_string_validation validation
) {
    size_t declared_length = 0U;
    size_t character_count = 0U;
    size_t truncated_length = 0U;
    int rc = MYLITE_OK;

    rc = varchar_length_for_column(database, validation.target_column, &declared_length);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = utf8_prefix_byte_length_for_character_count(
        (struct utf8_prefix_request){
            .text = text,
            .text_length = text_length,
            .character_limit = declared_length,
        },
        &truncated_length,
        &character_count
    );
    if (rc != MYLITE_OK) {
        set_unsupported_error(database, "VARCHAR values must be valid UTF-8");
        return MYLITE_ERROR;
    }
    if (character_count <= declared_length || validation.adjust_value) {
        return MYLITE_OK;
    }
    if (text_is_ascii_spaces(text + truncated_length, text_length - truncated_length)) {
        if (validation.source_column == NULL ||
            !column_descriptor_is_varchar(validation.source_column)) {
            return MYLITE_OK;
        }
        set_data_truncated_error(database, validation.target_column->name, validation.row_number);
        return MYLITE_ERROR;
    }
    if (validation.source_column != NULL &&
        column_descriptor_is_varchar(validation.source_column)) {
        set_data_truncated_error(database, validation.target_column->name, validation.row_number);
    } else {
        set_data_too_long_error(database, validation.target_column->name, validation.row_number);
    }

    return MYLITE_ERROR;
}

static int validate_insert_select_convertible_char_value(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    struct insert_select_string_validation validation
) {
    size_t declared_length = 0U;
    size_t character_count = 0U;
    size_t trimmed_length = 0U;
    size_t trailing_space_count = 0U;
    int rc = MYLITE_OK;

    rc = char_length_for_column(database, validation.target_column, &declared_length);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_utf8_text(text, text_length, &character_count);
    if (rc != MYLITE_OK) {
        set_unsupported_error(database, "CHAR values must be valid UTF-8");
        return MYLITE_ERROR;
    }

    trimmed_length = text_length;
    while (trimmed_length > 0U && text[trimmed_length - 1U] == ' ') {
        --trimmed_length;
        ++trailing_space_count;
    }
    if (trailing_space_count <= character_count &&
        character_count - trailing_space_count <= declared_length) {
        return MYLITE_OK;
    }
    if (validation.adjust_value) {
        return MYLITE_OK;
    }
    set_data_too_long_error(database, validation.target_column->name, validation.row_number);
    return MYLITE_ERROR;
}

static int validate_insert_select_convertible_text_family_value(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    struct insert_select_string_validation validation
) {
    const struct text_family_type_info *info = NULL;
    size_t character_count = 0U;
    size_t truncated_length = 0U;
    int rc = MYLITE_OK;

    rc = validate_utf8_text(text, text_length, &character_count);
    if (rc != MYLITE_OK) {
        set_unsupported_error(database, "TEXT values must be valid UTF-8");
        return MYLITE_ERROR;
    }
    (void)character_count;
    info = text_family_type_info_for_logical_type(validation.target_column->logical_type);
    if (info == NULL) {
        set_unsupported_error(database, "statement supports only baseline TEXT descriptors");
        return MYLITE_ERROR;
    }
    if (text_length <= info->maximum_length || validation.adjust_value) {
        return MYLITE_OK;
    }
    if (validation.source_column != NULL) {
        if (column_descriptor_is_text_family(validation.source_column)) {
            set_data_too_long_error(
                database,
                validation.target_column->name,
                validation.row_number
            );
            return MYLITE_ERROR;
        }

        return validate_text_family_text(
            database,
            validation.target_column,
            (struct varchar_text_validation){
                .text = text,
                .text_length = text_length,
                .row_number = validation.row_number,
            }
        );
    }

    rc = utf8_prefix_byte_length_for_byte_limit(
        (struct utf8_byte_prefix_request){
            .text = text,
            .text_length = text_length,
            .byte_limit = (size_t)info->maximum_length,
        },
        &truncated_length
    );
    if (rc != MYLITE_OK) {
        set_unsupported_error(database, "TEXT values must be valid UTF-8");
        return MYLITE_ERROR;
    }
    if (text_is_ascii_spaces(text + truncated_length, text_length - truncated_length)) {
        return MYLITE_OK;
    }

    set_data_too_long_error(database, validation.target_column->name, validation.row_number);
    return MYLITE_ERROR;
}

static int validate_insert_select_string_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number
) {
    const unsigned char *text = NULL;
    int byte_count = 0;

    if (sqlite3_column_type(statement, selected_column_index) != SQLITE_TEXT) {
        set_unsupported_error(
            database,
            "INSERT ... SELECT does not support implicit string conversion"
        );
        return MYLITE_ERROR;
    }
    text = sqlite3_column_text(statement, selected_column_index);
    byte_count = sqlite3_column_bytes(statement, selected_column_index);
    if (text == NULL || byte_count < 0) {
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_char(target_column)) {
        return validate_canonical_char_text(
            database,
            target_column,
            (struct varchar_text_validation){
                .text = (const char *)text,
                .text_length = (size_t)byte_count,
                .row_number = row_number,
            }
        );
    }
    if (column_descriptor_is_varchar(target_column)) {
        return validate_varchar_text(
            database,
            target_column,
            (struct varchar_text_validation){
                .text = (const char *)text,
                .text_length = (size_t)byte_count,
                .row_number = row_number,
            }
        );
    }
    return validate_text_family_text(
        database,
        target_column,
        (struct varchar_text_validation){
            .text = (const char *)text,
            .text_length = (size_t)byte_count,
            .row_number = row_number,
        }
    );
}

static int validate_insert_select_json_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *target_column
) {
    const unsigned char *text = NULL;
    int byte_count = 0;
    char *normalized = NULL;
    size_t normalized_length = 0U;
    struct mylite_json_normalize_result result = {0};
    int rc = MYLITE_OK;

    if (sqlite3_column_type(statement, selected_column_index) != SQLITE_TEXT) {
        set_unsupported_error(
            database,
            "INSERT ... SELECT does not support implicit JSON conversion"
        );
        return MYLITE_ERROR;
    }
    text = sqlite3_column_text(statement, selected_column_index);
    byte_count = sqlite3_column_bytes(statement, selected_column_index);
    if (text == NULL || byte_count < 0) {
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }
    rc = mylite_json_normalize(
        (const char *)text,
        (size_t)byte_count,
        &normalized,
        &normalized_length,
        &result
    );
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
        return rc;
    }
    if (rc != MYLITE_OK) {
        if (result.status == MYLITE_JSON_NORMALIZE_UNSUPPORTED) {
            set_unsupported_error(database, "INSERT ... SELECT does not support this JSON value");
        } else {
            set_invalid_json_text_error(database, result.position, target_column->name);
        }
        return MYLITE_ERROR;
    }
    free(normalized);
    (void)normalized_length;
    return MYLITE_OK;
}

static int validate_insert_select_binary_string_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number
) {
    const void *bytes = NULL;
    int byte_count = 0;
    char *copy = NULL;
    size_t copy_length = 0U;
    int rc = MYLITE_OK;

    if (sqlite3_column_type(statement, selected_column_index) != SQLITE_BLOB) {
        set_unsupported_error(
            database,
            "INSERT ... SELECT does not support implicit binary string conversion"
        );
        return MYLITE_ERROR;
    }
    bytes = sqlite3_column_blob(statement, selected_column_index);
    byte_count = sqlite3_column_bytes(statement, selected_column_index);
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
    rc = convert_binary_bytes_for_column(
        database,
        target_column,
        row_number,
        false,
        &copy,
        &copy_length
    );
    free(copy);
    return rc;
}

static int validate_insert_select_bit_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number
) {
    const void *bytes = NULL;
    int byte_count = 0;
    uint64_t width = 0U;
    size_t expected_byte_count = 0U;
    unsigned char unused_mask = 0U;
    int rc = bit_width_for_logical_type(target_column->logical_type, &width);

    if (rc != MYLITE_OK || sqlite3_column_type(statement, selected_column_index) != SQLITE_BLOB) {
        set_unsupported_error(
            database,
            "INSERT ... SELECT does not support implicit BIT conversion"
        );
        return MYLITE_ERROR;
    }
    bytes = sqlite3_column_blob(statement, selected_column_index);
    byte_count = sqlite3_column_bytes(statement, selected_column_index);
    if ((bytes == NULL && byte_count != 0) || byte_count < 0) {
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }
    expected_byte_count = bit_byte_width(width);
    if ((size_t)byte_count != expected_byte_count) {
        set_data_too_long_error(database, target_column->name, row_number);
        return MYLITE_ERROR;
    }
    if ((width % byte_bit_count) != 0U && expected_byte_count > 0U) {
        unused_mask = (unsigned char)(byte_all_bits_mask << (width % byte_bit_count));
        if ((((const unsigned char *)bytes)[0] & unused_mask) != 0U) {
            set_data_too_long_error(database, target_column->name, row_number);
            return MYLITE_ERROR;
        }
    }
    return MYLITE_OK;
}

static int validate_insert_select_decimal_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number
) {
    const unsigned char *text = NULL;
    int byte_count = 0;

    if (sqlite3_column_type(statement, selected_column_index) != SQLITE_TEXT) {
        set_unsupported_error(
            database,
            "INSERT ... SELECT does not support implicit DECIMAL conversion"
        );
        return MYLITE_ERROR;
    }
    text = sqlite3_column_text(statement, selected_column_index);
    byte_count = sqlite3_column_bytes(statement, selected_column_index);
    if (text == NULL || byte_count < 0) {
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }
    return validate_canonical_decimal_text(
        database,
        target_column,
        (const char *)text,
        (size_t)byte_count,
        row_number
    );
}

static int validate_insert_select_approximate_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number
) {
    struct approximate_type_info info = {
        .type_class = APPROXIMATE_TYPE_DOUBLE,
        .is_unsigned = false,
    };
    int sqlite_type = sqlite3_column_type(statement, selected_column_index);
    double value = 0.0;
    int rc = approximate_type_info_for_logical_type(target_column->logical_type, &info);

    if (rc != MYLITE_OK || (sqlite_type != SQLITE_FLOAT && sqlite_type != SQLITE_INTEGER)) {
        set_unsupported_error(
            database,
            "INSERT ... SELECT does not support implicit FLOAT or DOUBLE conversion"
        );
        return MYLITE_ERROR;
    }
    value = sqlite3_column_double(statement, selected_column_index);
    if (!isfinite(value) || (info.is_unsigned && value < 0.0) ||
        (info.type_class == APPROXIMATE_TYPE_FLOAT && fabs(value) > FLT_MAX)) {
        set_out_of_range_error(database, target_column->name, row_number);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_insert_select_date_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number
) {
    const unsigned char *text = NULL;
    int byte_count = 0;

    if (sqlite3_column_type(statement, selected_column_index) != SQLITE_TEXT) {
        set_unsupported_error(
            database,
            "INSERT ... SELECT does not support implicit DATE conversion"
        );
        return MYLITE_ERROR;
    }
    text = sqlite3_column_text(statement, selected_column_index);
    byte_count = sqlite3_column_bytes(statement, selected_column_index);
    if (text == NULL || byte_count < 0) {
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }
    if (!predicate_date_text_admitted(database, (const char *)text, (size_t)byte_count)) {
        set_incorrect_date_value_error(
            database,
            (const char *)text,
            target_column->name,
            row_number
        );
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_insert_select_time_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number
) {
    const unsigned char *text = NULL;
    int byte_count = 0;

    if (sqlite3_column_type(statement, selected_column_index) != SQLITE_TEXT) {
        set_unsupported_error(
            database,
            "INSERT ... SELECT does not support implicit TIME conversion"
        );
        return MYLITE_ERROR;
    }
    text = sqlite3_column_text(statement, selected_column_index);
    byte_count = sqlite3_column_bytes(statement, selected_column_index);
    if (text == NULL || byte_count < 0) {
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }
    if (!time_text_is_canonical_valid((const char *)text, (size_t)byte_count)) {
        set_incorrect_time_value_error(
            database,
            (const char *)text,
            target_column->name,
            row_number
        );
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_insert_select_datetime_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number
) {
    const unsigned char *text = NULL;
    int byte_count = 0;

    if (sqlite3_column_type(statement, selected_column_index) != SQLITE_TEXT) {
        set_unsupported_error(
            database,
            "INSERT ... SELECT does not support implicit DATETIME conversion"
        );
        return MYLITE_ERROR;
    }
    text = sqlite3_column_text(statement, selected_column_index);
    byte_count = sqlite3_column_bytes(statement, selected_column_index);
    if (text == NULL || byte_count < 0) {
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }
    if (!predicate_datetime_text_admitted(database, (const char *)text, (size_t)byte_count)) {
        set_incorrect_datetime_value_error(
            database,
            (const char *)text,
            target_column->name,
            row_number
        );
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_insert_select_timestamp_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number
) {
    const unsigned char *text = NULL;
    int byte_count = 0;

    if (sqlite3_column_type(statement, selected_column_index) != SQLITE_TEXT) {
        set_unsupported_error(
            database,
            "INSERT ... SELECT does not support implicit TIMESTAMP conversion"
        );
        return MYLITE_ERROR;
    }
    text = sqlite3_column_text(statement, selected_column_index);
    byte_count = sqlite3_column_bytes(statement, selected_column_index);
    if (text == NULL || byte_count < 0) {
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }
    if (!predicate_timestamp_text_admitted(database, (const char *)text, (size_t)byte_count)) {
        set_incorrect_datetime_value_error(
            database,
            (const char *)text,
            target_column->name,
            row_number
        );
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_insert_select_year_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number
) {
    const unsigned char *text = NULL;
    int byte_count = 0;

    if (sqlite3_column_type(statement, selected_column_index) != SQLITE_TEXT) {
        set_unsupported_error(
            database,
            "INSERT ... SELECT does not support implicit YEAR conversion"
        );
        return MYLITE_ERROR;
    }
    text = sqlite3_column_text(statement, selected_column_index);
    byte_count = sqlite3_column_bytes(statement, selected_column_index);
    if (text == NULL || byte_count < 0) {
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }
    if (!year_text_is_canonical((const char *)text, (size_t)byte_count)) {
        set_out_of_range_error(database, target_column->name, row_number);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_insert_select_integer_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number,
    bool adjust_value
) {
    const uint64_t int64_negative_abs_max = 9223372036854775808ULL;
    struct integer_column_range range = {0};
    int64_t selected_value = 0;
    bool is_negative = false;
    uint64_t magnitude = 0U;
    int64_t converted_value = 0;

    if (sqlite3_column_type(statement, selected_column_index) != SQLITE_INTEGER) {
        set_unsupported_error(
            database,
            "INSERT ... SELECT does not support implicit integer conversion"
        );
        return MYLITE_ERROR;
    }
    if (adjust_value) {
        return integer_range_for_column(
            database,
            target_column,
            "INSERT supports only baseline integer columns",
            &range
        );
    }

    selected_value = (int64_t)sqlite3_column_int64(statement, selected_column_index);
    is_negative = selected_value < 0;
    if (is_negative) {
        magnitude =
            selected_value == INT64_MIN ? int64_negative_abs_max : (uint64_t)(-selected_value);
    } else {
        magnitude = (uint64_t)selected_value;
    }

    return convert_integer_for_column(
        database,
        magnitude,
        is_negative,
        target_column,
        row_number,
        &converted_value
    );
}
