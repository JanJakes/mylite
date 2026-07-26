static int plan_insert_select(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_insert_select *out_plan
) {
    const struct mylite_sql_ast_node *column_list = child_at(statement, 1U);
    const struct mylite_sql_ast_node *source_statement = child_at(statement, 2U);
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
        return MYLITE_ERROR;
    }
    rc = plan_insert_select_target(database, statement, out_plan);
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
    if (rc == MYLITE_OK) {
        rc = plan_insert_select_source(database, statement, source_kind, out_plan);
    }

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
    struct planned_insert_select *out_plan
) {
    int rc = resolve_visible_writable_table_reference(
        database,
        child_at(statement, 0U),
        &out_plan->target.target,
        &out_plan->target.table
    );

    if (rc == MYLITE_OK) {
        rc = mylite_execution_acquire_table_columns(
            database,
            out_plan->target.table.table_id,
            &out_plan->target.columns,
            &out_plan->target.column_count,
            &out_plan->target.columns_cache_entry
        );
    }
    if (rc == MYLITE_OK) {
        rc = load_insert_table_key_metadata(database, &out_plan->target);
    }
    if (rc == MYLITE_OK) {
        rc = load_insert_target_foreign_key_infos(database, &out_plan->target);
    }

    return rc;
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
        MYLITE_SQL_AST_SET_OPERATOR_UNION,
        false
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
                source->columns[target_position],
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
