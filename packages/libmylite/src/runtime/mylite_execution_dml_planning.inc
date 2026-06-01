static int plan_insert(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_insert *out_plan
) {
    const struct mylite_sql_ast_node *column_list = child_at(statement, 1U);
    const struct mylite_sql_ast_node *row_list = child_at(statement, 2U);
    size_t *target_indexes = NULL;
    size_t target_count = 0U;
    struct primary_key_info primary_key = primary_key_info_init();
    int rc = MYLITE_OK;

    *out_plan = (struct planned_insert){0};
    out_plan->ignore_errors =
        child_with_kind(statement, MYLITE_SQL_AST_INSERT_IGNORE_MODIFIER) != NULL;
    out_plan->replace_existing_rows = statement->kind == MYLITE_SQL_AST_REPLACE_VALUES_STATEMENT;
    rc = resolve_visible_writable_table_reference(
        database,
        child_at(statement, 0U),
        &out_plan->target,
        &out_plan->table
    );
    if (rc == MYLITE_OK) {
        rc = load_table_columns(
            database,
            out_plan->table.table_id,
            &out_plan->columns,
            &out_plan->column_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = load_primary_key_info(
            database,
            out_plan->table.table_id,
            out_plan->columns,
            out_plan->column_count,
            &primary_key
        );
    }
    if (rc == MYLITE_OK && primary_key.has_primary_key) {
        out_plan->has_primary_key = true;
        if (primary_key.part_count == 1U) {
            out_plan->primary_key_column_index = primary_key.parts[0].column_index;
        }
    }
    if (rc == MYLITE_OK) {
        rc = load_table_index_infos(
            database,
            out_plan->table.table_id,
            out_plan->columns,
            out_plan->column_count,
            &out_plan->indexes,
            &out_plan->index_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = load_insert_target_foreign_key_infos(database, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = initialize_insert_auto_increment_plan(database, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = collect_insert_target_indexes(
            database,
            column_list,
            out_plan,
            row_list,
            &target_indexes,
            &target_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = check_insert_target_duplicate(
            database,
            out_plan->columns,
            target_indexes,
            target_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_insert_row_shapes(database, row_list, target_count);
    }
    if (rc == MYLITE_OK) {
        rc = plan_insert_rows(database, row_list, target_indexes, target_count, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = check_insert_omitted_columns(database, out_plan, target_indexes, target_count);
    }
    if (rc == MYLITE_OK) {
        rc = plan_insert_auto_increment_values(database, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = plan_insert_string_key_values(database, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = plan_insert_duplicate_update(database, statement, out_plan);
    }

    free(target_indexes);
    primary_key_info_deinit(&primary_key);
    if (rc != MYLITE_OK) {
        planned_insert_deinit(out_plan);
    }

    return rc;
}

static int plan_insert_set(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_insert *out_plan
) {
    const struct mylite_sql_ast_node *assignment_list = child_at(statement, 1U);
    size_t *target_indexes = NULL;
    size_t target_count = 0U;
    struct primary_key_info primary_key = primary_key_info_init();
    int rc = MYLITE_OK;

    *out_plan = (struct planned_insert){0};
    out_plan->ignore_errors =
        child_with_kind(statement, MYLITE_SQL_AST_INSERT_IGNORE_MODIFIER) != NULL;
    out_plan->replace_existing_rows = statement->kind == MYLITE_SQL_AST_REPLACE_SET_STATEMENT;
    rc = resolve_visible_writable_table_reference(
        database,
        child_at(statement, 0U),
        &out_plan->target,
        &out_plan->table
    );
    if (rc == MYLITE_OK) {
        rc = load_table_columns(
            database,
            out_plan->table.table_id,
            &out_plan->columns,
            &out_plan->column_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = load_primary_key_info(
            database,
            out_plan->table.table_id,
            out_plan->columns,
            out_plan->column_count,
            &primary_key
        );
    }
    if (rc == MYLITE_OK && primary_key.has_primary_key) {
        out_plan->has_primary_key = true;
        if (primary_key.part_count == 1U) {
            out_plan->primary_key_column_index = primary_key.parts[0].column_index;
        }
    }
    if (rc == MYLITE_OK) {
        rc = load_table_index_infos(
            database,
            out_plan->table.table_id,
            out_plan->columns,
            out_plan->column_count,
            &out_plan->indexes,
            &out_plan->index_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = load_insert_target_foreign_key_infos(database, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = initialize_insert_auto_increment_plan(database, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = collect_insert_set_target_indexes(
            database,
            assignment_list,
            out_plan,
            statement->kind == MYLITE_SQL_AST_REPLACE_SET_STATEMENT
                ? "REPLACE ... SET supports only unqualified assignment columns"
                : "INSERT ... SET supports only unqualified assignment columns",
            &target_indexes,
            &target_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = check_insert_target_duplicate(
            database,
            out_plan->columns,
            target_indexes,
            target_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_insert_set_row(database, assignment_list, target_indexes, target_count, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = check_insert_omitted_columns(database, out_plan, target_indexes, target_count);
    }
    if (rc == MYLITE_OK) {
        rc = plan_insert_auto_increment_values(database, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = plan_insert_string_key_values(database, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = plan_insert_duplicate_update(database, statement, out_plan);
    }

    free(target_indexes);
    primary_key_info_deinit(&primary_key);
    if (rc != MYLITE_OK) {
        planned_insert_deinit(out_plan);
    }

    return rc;
}

static int load_insert_target_foreign_key_infos(
    struct mylite_db *database,
    struct planned_insert *plan
) {
    return load_table_foreign_key_infos(
        database,
        plan->table.table_id,
        plan->columns,
        plan->column_count,
        &plan->foreign_keys,
        &plan->foreign_key_count
    );
}

static int initialize_insert_auto_increment_plan(
    struct mylite_db *database,
    struct planned_insert *plan
) {
    size_t auto_increment_count = 0U;

    if (plan == NULL) {
        set_runtime_error(database, "invalid auto-increment insert plan");
        return MYLITE_ERROR;
    }

    plan->auto_increment_next =
        plan->table.auto_increment_next > 0 ? plan->table.auto_increment_next : 1;
    plan->auto_increment_next_after_statement = plan->auto_increment_next;
    plan->no_auto_value_on_zero =
        session_sql_mode_has(&database->session, MYLITE_SESSION_SQL_MODE_NO_AUTO_VALUE_ON_ZERO);
    for (size_t column_index = 0U; column_index < plan->column_count; ++column_index) {
        if (!column_descriptor_is_auto_increment(&plan->columns[column_index])) {
            continue;
        }
        ++auto_increment_count;
        plan->has_auto_increment = true;
        plan->auto_increment_column_index = column_index;
    }
    if (auto_increment_count == 0U) {
        return MYLITE_OK;
    }
    if (auto_increment_count > 1U || !insert_auto_increment_column_is_indexed(plan)) {
        set_unsupported_error(database, "INSERT supports only indexed AUTO_INCREMENT tables");
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int initialize_insert_auto_increment_range(
    struct mylite_db *database,
    struct planned_insert *plan
) {
    if (plan == NULL) {
        set_runtime_error(database, "invalid auto-increment insert plan");
        return MYLITE_ERROR;
    }
    if (!plan->has_auto_increment) {
        return MYLITE_OK;
    }
    if (plan->auto_increment_column_index >= plan->column_count) {
        set_runtime_error(database, "invalid auto-increment column index");
        return MYLITE_ERROR;
    }

    return integer_range_for_column(
        database,
        &plan->columns[plan->auto_increment_column_index],
        "AUTO_INCREMENT supports only baseline integer columns",
        &plan->auto_increment_range
    );
}

static bool insert_auto_increment_column_is_indexed(const struct planned_insert *plan) {
    if (plan->has_primary_key &&
        plan->auto_increment_column_index == plan->primary_key_column_index) {
        return true;
    }

    for (size_t index = 0U; index < plan->index_count; ++index) {
        if (plan->indexes[index].index.kind == MYLITE_CATALOG_INDEX_KIND_PRIMARY ||
            plan->indexes[index].part_count == 0U) {
            continue;
        }
        if (plan->indexes[index].parts[0].column_index == plan->auto_increment_column_index) {
            return true;
        }
    }

    return false;
}

static int plan_insert_auto_increment_values(
    struct mylite_db *database,
    struct planned_insert *plan
) {
    struct insert_auto_increment_mode_counts mode_counts = {0};
    int64_t next_value = 0;
    int rc = MYLITE_OK;

    if (plan == NULL || !plan->has_auto_increment) {
        return MYLITE_OK;
    }

    rc = initialize_insert_auto_increment_range(database, plan);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = count_insert_auto_increment_modes(plan, &mode_counts);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (plan->row_count > 1U && mode_counts.generated_count != 0U &&
        mode_counts.explicit_count != 0U) {
        set_unsupported_error(
            database,
            "mixed explicit and generated AUTO_INCREMENT values are not supported"
        );
        return MYLITE_ERROR;
    }

    next_value = plan->auto_increment_next;
    for (size_t row_index = 0U; row_index < plan->row_count; ++row_index) {
        rc = plan_insert_auto_increment_row_value(
            database,
            plan,
            &plan->auto_increment_range,
            row_index,
            &next_value
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    plan->auto_increment_next_after_statement = next_value;
    return MYLITE_OK;
}

static int plan_insert_string_key_values(
    struct mylite_db *database,
    const struct planned_insert *plan
) {
    for (size_t index = 0U; index < plan->index_count; ++index) {
        const struct loaded_index_info *key = &plan->indexes[index];

        if (!key->index.is_unique) {
            continue;
        }
        for (size_t part_index = 0U; part_index < key->part_count; ++part_index) {
            const struct loaded_index_part *part = &key->parts[part_index];

            if (!loaded_index_part_requires_string_key_validation(part)) {
                continue;
            }
            if (part->column_index >= plan->column_count) {
                set_runtime_error(database, "invalid string key descriptor");
                return MYLITE_ERROR;
            }
            for (size_t row_index = 0U; row_index < plan->row_count; ++row_index) {
                int rc = validate_string_key_value(
                    database,
                    &plan->rows[row_index].values[part->column_index]
                );

                if (rc != MYLITE_OK) {
                    return rc;
                }
            }
        }
    }

    return MYLITE_OK;
}

static int plan_insert_duplicate_update(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_insert *plan
) {
    const struct mylite_sql_ast_node *clause =
        child_with_kind(statement, MYLITE_SQL_AST_INSERT_DUPLICATE_UPDATE_CLAUSE);
    const struct mylite_sql_ast_node *assignment_list = NULL;
    size_t assignment_count = 0U;
    int rc = MYLITE_OK;

    if (clause == NULL) {
        return MYLITE_OK;
    }
    if (plan->ignore_errors) {
        set_unsupported_error(
            database,
            "INSERT IGNORE ... ON DUPLICATE KEY UPDATE is not supported"
        );
        return MYLITE_ERROR;
    }

    assignment_list = child_at(clause, 0U);
    if (assignment_list == NULL ||
        assignment_list->kind != MYLITE_SQL_AST_INSERT_DUPLICATE_ASSIGNMENT_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    assignment_count = mylite_sql_ast_node_child_count(assignment_list);
    if (assignment_count == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    plan->duplicate_update = (struct planned_insert_duplicate_update){
        .has_clause = true,
        .assignment_count = assignment_count,
    };

    plan->duplicate_update.assignments =
        calloc(assignment_count, sizeof(*plan->duplicate_update.assignments));
    if (plan->duplicate_update.assignments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    for (size_t index = 0U; rc == MYLITE_OK && index < assignment_count; ++index) {
        rc = plan_insert_duplicate_assignment(
            database,
            child_at(assignment_list, index),
            plan,
            index
        );
    }
    if (rc == MYLITE_OK && statement->kind == MYLITE_SQL_AST_INSERT_SELECT_STATEMENT &&
        insert_duplicate_update_has_same_column_arithmetic(plan)) {
        set_unsupported_error(
            database,
            "INSERT ... SELECT ... ON DUPLICATE KEY UPDATE arithmetic assignment is not supported"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = plan_insert_duplicate_update_key(database, plan);
    }

    return rc;
}

static int plan_insert_duplicate_assignment(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment,
    struct planned_insert *plan,
    size_t assignment_index
) {
    struct planned_insert_duplicate_update_assignment *duplicate_assignment = NULL;
    const struct mylite_sql_ast_node *target = NULL;
    const struct mylite_sql_ast_node *value = NULL;
    char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = MYLITE_OK;

    if (plan == NULL || assignment_index >= plan->duplicate_update.assignment_count ||
        plan->duplicate_update.assignments == NULL) {
        set_runtime_error(database, "invalid insert duplicate assignment plan");
        return MYLITE_ERROR;
    }
    duplicate_assignment = &plan->duplicate_update.assignments[assignment_index];
    duplicate_assignment->value_kind = PLANNED_INSERT_DUPLICATE_UPDATE_VALUE_LITERAL;

    if (assignment == NULL || assignment->kind != MYLITE_SQL_AST_INSERT_DUPLICATE_ASSIGNMENT) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    target = child_at(assignment, 0U);
    if (target == NULL || target->kind != MYLITE_SQL_AST_IDENTIFIER) {
        set_unsupported_error(
            database,
            "INSERT ... ON DUPLICATE KEY UPDATE supports only unqualified assignment columns"
        );
        return MYLITE_ERROR;
    }
    rc = copy_identifier_text(target, column_name, sizeof(column_name), database);
    if (rc == MYLITE_OK) {
        rc = find_column_index(
            plan->columns,
            plan->column_count,
            column_name,
            &duplicate_assignment->assignment_column_index
        );
        if (rc != MYLITE_OK) {
            set_unknown_column_error(database, column_name);
            return MYLITE_ERROR;
        }
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < assignment_index; ++index) {
        if (plan->duplicate_update.assignments[index].assignment_column_index ==
            duplicate_assignment->assignment_column_index) {
            set_unsupported_error(
                database,
                "INSERT ... ON DUPLICATE KEY UPDATE does not support duplicate assignment targets"
            );
            return MYLITE_ERROR;
        }
    }

    value = child_at(assignment, 1U);
    if (rc == MYLITE_OK && value == NULL) {
        set_parse_error(database, NULL);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK &&
        plan->columns[duplicate_assignment->assignment_column_index].is_generated) {
        const struct mylite_sql_ast_node *unwrapped_value = unwrap_parenthesized_expression(value);

        if (unwrapped_value != NULL && unwrapped_value->kind == MYLITE_SQL_AST_DML_DEFAULT_VALUE) {
            duplicate_assignment->assignment_value_node = value;
            duplicate_assignment->value_kind = PLANNED_INSERT_DUPLICATE_UPDATE_VALUE_LITERAL;
            duplicate_assignment->generated_default_noop = true;
            return MYLITE_OK;
        }
        set_generated_column_value_error(
            database,
            plan->columns[duplicate_assignment->assignment_column_index].name,
            plan->table.name
        );
        return MYLITE_ERROR;
    }

    return plan_insert_duplicate_assignment_value(database, value, plan, duplicate_assignment);
}

static int plan_insert_duplicate_assignment_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value,
    struct planned_insert *plan,
    struct planned_insert_duplicate_update_assignment *duplicate_assignment
) {
    const struct mylite_sql_ast_node *default_value = NULL;
    bool planned_arithmetic = false;
    int rc = MYLITE_OK;

    if (value == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value->kind == MYLITE_SQL_AST_INSERT_VALUES_REFERENCE) {
        rc = plan_insert_duplicate_values_reference(database, value, plan, duplicate_assignment);
    } else {
        rc = plan_insert_duplicate_same_column_arithmetic(
            database,
            value,
            plan,
            duplicate_assignment,
            &planned_arithmetic
        );
        if (rc == MYLITE_OK && !planned_arithmetic) {
            duplicate_assignment->assignment_value_node = value;
            duplicate_assignment->value_kind = PLANNED_INSERT_DUPLICATE_UPDATE_VALUE_LITERAL;
        }
    }

    default_value = unwrap_parenthesized_expression(duplicate_assignment->assignment_value_node);
    if (rc == MYLITE_OK && default_value != NULL &&
        default_value->kind == MYLITE_SQL_AST_DEFAULT_FUNCTION) {
        struct select_source_context source_context = {.source = &plan->target};

        rc = validate_default_function_source_value(
            database,
            default_value,
            &source_context,
            plan->columns,
            plan->column_count
        );
    }

    return rc;
}

static int plan_insert_duplicate_values_reference(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    struct planned_insert *plan,
    struct planned_insert_duplicate_update_assignment *duplicate_assignment
) {
    const struct mylite_sql_ast_node *column = child_at(value_node, 0U);
    char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = MYLITE_OK;

    if (column == NULL || column->kind != MYLITE_SQL_AST_IDENTIFIER) {
        set_unsupported_error(
            database,
            "VALUES() in ON DUPLICATE KEY UPDATE supports only unqualified columns"
        );
        return MYLITE_ERROR;
    }
    rc = copy_identifier_text(column, column_name, sizeof(column_name), database);
    if (rc == MYLITE_OK) {
        rc = find_column_index(
            plan->columns,
            plan->column_count,
            column_name,
            &duplicate_assignment->values_column_index
        );
        if (rc != MYLITE_OK) {
            set_unknown_column_error(database, column_name);
            return MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && duplicate_assignment->values_column_index !=
                               duplicate_assignment->assignment_column_index) {
        set_unsupported_error(
            database,
            "VALUES() in ON DUPLICATE KEY UPDATE supports only the assignment column"
        );
        return MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && !insert_select_source_target_types_are_compatible(
                               &plan->columns[duplicate_assignment->values_column_index],
                               &plan->columns[duplicate_assignment->assignment_column_index]
                           )) {
        set_unsupported_error(
            database,
            "INSERT ... ON DUPLICATE KEY UPDATE does not support implicit VALUES() conversion"
        );
        return MYLITE_ERROR;
    }

    duplicate_assignment->assignment_value_node = value_node;
    duplicate_assignment->value_kind = PLANNED_INSERT_DUPLICATE_UPDATE_VALUE_VALUES_REFERENCE;
    return rc;
}

static int plan_insert_duplicate_same_column_arithmetic(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    struct planned_insert *plan,
    struct planned_insert_duplicate_update_assignment *duplicate_assignment,
    bool *out_planned
) {
    const struct mylite_sql_ast_node *source = NULL;
    const struct mylite_sql_ast_node *delta = NULL;
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;
    char source_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t source_index = 0U;
    int rc = MYLITE_OK;

    *out_planned = false;
    value_node = unwrap_parenthesized_expression(value_node);
    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_BINARY_EXPRESSION ||
        insert_value_is_unix_timestamp_arithmetic(value_node)) {
        return MYLITE_OK;
    }

    operator_kind = mylite_sql_ast_node_operator(value_node);
    if (operator_kind != MYLITE_SQL_AST_OPERATOR_ADD &&
        operator_kind != MYLITE_SQL_AST_OPERATOR_SUBTRACT) {
        set_insert_duplicate_arithmetic_unsupported_error(database);
        return MYLITE_ERROR;
    }

    source = child_at(value_node, 0U);
    delta = child_at(value_node, 1U);
    if (source == NULL || source->kind != MYLITE_SQL_AST_IDENTIFIER || delta == NULL ||
        delta->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(delta) != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_insert_duplicate_arithmetic_unsupported_error(database);
        return MYLITE_ERROR;
    }

    rc = copy_identifier_text(source, source_name, sizeof(source_name), database);
    if (rc == MYLITE_OK) {
        rc = find_column_index(plan->columns, plan->column_count, source_name, &source_index);
        if (rc != MYLITE_OK) {
            set_unknown_column_error(database, source_name);
            return MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && source_index != duplicate_assignment->assignment_column_index) {
        set_insert_duplicate_arithmetic_unsupported_error(database);
        return MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = validate_insert_duplicate_arithmetic_assignment_target(
            database,
            plan,
            duplicate_assignment->assignment_column_index
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    duplicate_assignment->assignment_value_node = value_node;
    duplicate_assignment->value_kind = PLANNED_INSERT_DUPLICATE_UPDATE_VALUE_SAME_COLUMN_ARITHMETIC;
    duplicate_assignment->arithmetic_operator = operator_kind;
    duplicate_assignment->arithmetic_delta_node = delta;
    *out_planned = true;
    return MYLITE_OK;
}

static int validate_insert_duplicate_arithmetic_assignment_target(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t column_index
) {
    struct integer_column_range range = {0};
    const struct mylite_catalog_column_descriptor *column = NULL;
    int rc = MYLITE_OK;

    if (plan == NULL || column_index >= plan->column_count) {
        set_runtime_error(database, "invalid insert duplicate arithmetic assignment plan");
        return MYLITE_ERROR;
    }
    column = &plan->columns[column_index];
    rc = integer_range_for_column(
        database,
        column,
        "INSERT ... ON DUPLICATE KEY UPDATE arithmetic assignment supports only integer columns",
        &range
    );
    (void)range;
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (column_descriptor_is_auto_increment(column) ||
        insert_duplicate_column_index_is_keyed(plan, column_index)) {
        set_insert_duplicate_arithmetic_unsupported_error(database);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int plan_insert_duplicate_update_key(
    struct mylite_db *database,
    struct planned_insert *plan
) {
    for (size_t index = 0U; index < plan->index_count; ++index) {
        const struct loaded_index_info *candidate = &plan->indexes[index];

        if (!candidate->index.is_unique) {
            continue;
        }
        if (candidate->part_count == 0U) {
            set_unsupported_error(
                database,
                "INSERT ... ON DUPLICATE KEY UPDATE supports only descriptor key parts"
            );
            return MYLITE_ERROR;
        }
        if (!plan->duplicate_update.has_key) {
            plan->duplicate_update.has_key = true;
            plan->duplicate_update.key_index = index;
        }
    }
    if (insert_duplicate_assignment_targets_auto_increment(plan)) {
        set_unsupported_error(
            database,
            "INSERT ... ON DUPLICATE KEY UPDATE does not support auto-increment assignments"
        );
        return MYLITE_ERROR;
    }
    if (insert_duplicate_assignment_targets_key(plan)) {
        return reject_insert_duplicate_parent_foreign_key_assignments(database, plan);
    }

    return MYLITE_OK;
}

static int reject_insert_duplicate_parent_foreign_key_assignments(
    struct mylite_db *database,
    const struct planned_insert *plan
) {
    struct loaded_foreign_key_info *foreign_keys = NULL;
    size_t foreign_key_count = 0U;
    int rc = load_parent_foreign_key_infos(
        database,
        plan->table.table_id,
        &foreign_keys,
        &foreign_key_count
    );

    for (size_t index = 0U; rc == MYLITE_OK && index < foreign_key_count; ++index) {
        const struct loaded_foreign_key_info *foreign_key = &foreign_keys[index];

        for (size_t part_index = 0U; part_index < foreign_key->part_count; ++part_index) {
            if (insert_duplicate_assignment_targets_column_id(
                    plan,
                    foreign_key->parts[part_index].parent_column.column_id
                )) {
                set_unsupported_error(
                    database,
                    "INSERT ... ON DUPLICATE KEY UPDATE does not support parent foreign-key "
                    "key assignments"
                );
                rc = MYLITE_ERROR;
                break;
            }
        }
    }

    loaded_foreign_key_infos_deinit(&foreign_keys, &foreign_key_count);
    return rc;
}

static bool insert_duplicate_assignment_targets_key(const struct planned_insert *plan) {
    if (plan == NULL) {
        return false;
    }

    for (size_t index = 0U; index < plan->index_count; ++index) {
        const struct loaded_index_info *key = &plan->indexes[index];

        if (!key->index.is_unique) {
            continue;
        }
        for (size_t part_index = 0U; part_index < key->part_count; ++part_index) {
            for (size_t assignment_index = 0U;
                 assignment_index < plan->duplicate_update.assignment_count;
                 ++assignment_index) {
                if (insert_duplicate_assignment_is_noop(
                        &plan->duplicate_update.assignments[assignment_index]
                    )) {
                    continue;
                }
                if (key->parts[part_index].column_index ==
                    plan->duplicate_update.assignments[assignment_index].assignment_column_index) {
                    return true;
                }
            }
        }
    }

    return false;
}

static bool insert_duplicate_assignment_targets_auto_increment(const struct planned_insert *plan) {
    if (plan == NULL || !plan->has_auto_increment) {
        return false;
    }

    for (size_t index = 0U; index < plan->duplicate_update.assignment_count; ++index) {
        if (insert_duplicate_assignment_is_noop(&plan->duplicate_update.assignments[index])) {
            continue;
        }
        if (plan->duplicate_update.assignments[index].assignment_column_index ==
            plan->auto_increment_column_index) {
            return true;
        }
    }

    return false;
}

static bool insert_duplicate_assignment_targets_column_id(
    const struct planned_insert *plan,
    int64_t column_id
) {
    if (plan == NULL || column_id <= 0) {
        return false;
    }

    for (size_t index = 0U; index < plan->duplicate_update.assignment_count; ++index) {
        if (insert_duplicate_assignment_is_noop(&plan->duplicate_update.assignments[index])) {
            continue;
        }
        size_t column_index = plan->duplicate_update.assignments[index].assignment_column_index;

        if (column_index < plan->column_count &&
            plan->columns[column_index].column_id == column_id) {
            return true;
        }
    }

    return false;
}

static bool insert_duplicate_column_index_is_keyed(
    const struct planned_insert *plan,
    size_t column_index
) {
    if (plan == NULL || column_index >= plan->column_count) {
        return false;
    }
    for (size_t index = 0U; index < plan->index_count; ++index) {
        const struct loaded_index_info *key = &plan->indexes[index];

        if ((key->index.kind == MYLITE_CATALOG_INDEX_KIND_PRIMARY || key->index.is_unique) &&
            loaded_index_contains_column_id(key, plan->columns[column_index].column_id)) {
            return true;
        }
    }

    return false;
}

static bool insert_duplicate_assignment_is_noop(
    const struct planned_insert_duplicate_update_assignment *assignment
) {
    return (assignment != NULL && assignment->generated_default_noop) != 0;
}

static size_t count_executable_insert_duplicate_assignments(const struct planned_insert *plan) {
    size_t count = 0U;

    if (plan == NULL) {
        return 0U;
    }
    for (size_t index = 0U; index < plan->duplicate_update.assignment_count; ++index) {
        if (!insert_duplicate_assignment_is_noop(&plan->duplicate_update.assignments[index])) {
            ++count;
        }
    }
    return count;
}

static void set_insert_duplicate_arithmetic_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "INSERT ... ON DUPLICATE KEY UPDATE arithmetic assignment supports only same-column "
        "integer + or - unsigned integer literals on non-key columns"
    );
}

static int count_insert_auto_increment_modes(
    const struct planned_insert *plan,
    struct insert_auto_increment_mode_counts *out_counts
) {
    int rc = MYLITE_OK;

    *out_counts = (struct insert_auto_increment_mode_counts){0};
    for (size_t row_index = 0U; row_index < plan->row_count; ++row_index) {
        bool generated = false;

        rc = generated_auto_increment_value_for_row(plan, row_index, &generated);
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (generated) {
            ++out_counts->generated_count;
        } else {
            ++out_counts->explicit_count;
        }
    }

    return MYLITE_OK;
}

static int plan_insert_auto_increment_row_value(
    struct mylite_db *database,
    struct planned_insert *plan,
    const struct integer_column_range *range,
    size_t row_index,
    int64_t *next_value
) {
    struct planned_value *value = &plan->rows[row_index].values[plan->auto_increment_column_index];
    bool generated = false;
    int rc = generated_auto_increment_value_for_row(plan, row_index, &generated);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (generated) {
        int64_t generated_value = 0;

        rc = generated_auto_increment_value_for_lower_bound(
            database,
            *next_value,
            range,
            &generated_value
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (generated_value <= 0 || (uint64_t)generated_value > range->positive_max) {
            set_failed_read_auto_increment_error(database);
            return MYLITE_ERROR;
        }
        rc = assign_generated_auto_increment_value(database, plan, row_index, generated_value);
        if (rc == MYLITE_OK) {
            plan->rows[row_index].generated_auto_increment = true;
        }
        if (rc == MYLITE_OK && !plan->generated_auto_increment) {
            plan->first_generated_auto_increment = generated_value;
            plan->generated_auto_increment = true;
        }
        if (rc == MYLITE_OK) {
            rc = next_auto_increment_after_generated_value(
                database,
                generated_value,
                range,
                next_value
            );
        }
    } else if (!value->is_text && value->integer > 0) {
        rc = next_auto_increment_after_explicit_insert_value(
            *next_value,
            value->integer,
            range,
            next_value
        );
    }
    if (rc != MYLITE_OK) {
        set_unsupported_error(database, "AUTO_INCREMENT counter is out of range");
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int generated_auto_increment_value_for_row(
    const struct planned_insert *plan,
    size_t row_index,
    bool *out_generated
) {
    const struct planned_value *value = NULL;

    if (plan == NULL || out_generated == NULL || row_index >= plan->row_count ||
        plan->auto_increment_column_index >= plan->column_count ||
        plan->rows[row_index].values == NULL) {
        return MYLITE_ERROR;
    }

    value = &plan->rows[row_index].values[plan->auto_increment_column_index];
    *out_generated = (value->is_null || (!plan->no_auto_value_on_zero && !value->is_text &&
                                         value->integer == 0)) != 0;
    return MYLITE_OK;
}

static int assign_generated_auto_increment_value(
    struct mylite_db *database,
    struct planned_insert *plan,
    size_t row_index,
    int64_t generated_value
) {
    struct planned_value *value = NULL;

    if (plan == NULL || row_index >= plan->row_count ||
        plan->auto_increment_column_index >= plan->column_count ||
        plan->rows[row_index].values == NULL || generated_value <= 0) {
        set_runtime_error(database, "invalid generated auto-increment value");
        return MYLITE_ERROR;
    }

    value = &plan->rows[row_index].values[plan->auto_increment_column_index];
    planned_value_deinit(value);
    *value = (struct planned_value){
        .is_null = false,
        .is_text = false,
        .integer = generated_value,
    };
    return MYLITE_OK;
}

static int generated_auto_increment_value_for_lower_bound(
    struct mylite_db *database,
    int64_t lower_bound,
    const struct integer_column_range *range,
    int64_t *out_value
) {
    uint64_t increment = 1U;
    uint64_t offset = 1U;
    uint64_t candidate = 0U;

    if (out_value == NULL || range == NULL || lower_bound <= 0) {
        return MYLITE_ERROR;
    }
    *out_value = 0;
    increment = database->session.auto_increment_increment;
    offset = database->session.auto_increment_offset;
    if (offset > increment) {
        set_unsupported_error(
            database,
            "AUTO_INCREMENT allocation with offset greater than increment is not supported"
        );
        return MYLITE_ERROR;
    }

    if ((uint64_t)lower_bound <= offset) {
        candidate = offset;
    } else {
        uint64_t delta = (uint64_t)lower_bound - offset;
        uint64_t steps = delta / increment;

        if (delta % increment != 0U) {
            ++steps;
        }
        if (steps > (UINT64_MAX - offset) / increment) {
            return MYLITE_ERROR;
        }
        candidate = offset + (steps * increment);
    }
    if (candidate > (uint64_t)INT64_MAX) {
        return MYLITE_ERROR;
    }

    *out_value = (int64_t)candidate;
    return MYLITE_OK;
}

static int next_auto_increment_after_generated_value(
    const struct mylite_db *database,
    int64_t value,
    const struct integer_column_range *range,
    int64_t *out_next
) {
    uint64_t increment = 1U;

    if (out_next == NULL || database == NULL || range == NULL || value <= 0 ||
        (uint64_t)value > range->positive_max) {
        return MYLITE_ERROR;
    }
    if ((uint64_t)value == range->positive_max) {
        *out_next = value;
        return MYLITE_OK;
    }

    increment = database->session.auto_increment_increment;
    if (range->positive_max - (uint64_t)value < increment) {
        if (range->positive_max > (uint64_t)INT64_MAX) {
            return MYLITE_ERROR;
        }
        *out_next = (int64_t)range->positive_max;
        return MYLITE_OK;
    }
    *out_next = value + (int64_t)increment;
    return MYLITE_OK;
}

static int next_auto_increment_after_explicit_insert_value(
    int64_t current_next,
    int64_t value,
    const struct integer_column_range *range,
    int64_t *out_next
) {
    if (out_next == NULL || range == NULL || current_next <= 0 || value <= 0) {
        return MYLITE_ERROR;
    }
    *out_next = current_next;
    if (value < current_next) {
        return MYLITE_OK;
    }
    if ((uint64_t)value > range->positive_max) {
        return MYLITE_ERROR;
    }
    if ((uint64_t)value == range->positive_max) {
        *out_next = value;
        return MYLITE_OK;
    }
    *out_next = value + 1;
    return MYLITE_OK;
}

static int next_auto_increment_after_update_value(
    struct mylite_db *database,
    int64_t current_next,
    int64_t value,
    const struct integer_column_range *range,
    int64_t *out_next
) {
    int64_t lower_bound = 0;

    if (out_next == NULL || range == NULL || current_next <= 0 || value <= 0) {
        return MYLITE_ERROR;
    }
    *out_next = current_next;
    if (value < current_next) {
        return MYLITE_OK;
    }
    if ((uint64_t)value > range->positive_max) {
        return MYLITE_ERROR;
    }
    if ((uint64_t)value == range->positive_max) {
        *out_next = value;
        return MYLITE_OK;
    }
    lower_bound = value + 1;
    return generated_auto_increment_value_for_lower_bound(database, lower_bound, range, out_next);
}

static void planned_insert_deinit(struct planned_insert *plan) {
    if (plan == NULL) {
        return;
    }

    if (plan->rows != NULL) {
        for (size_t row_index = 0U; row_index < plan->row_count; ++row_index) {
            for (size_t column_index = 0U;
                 column_index < plan->column_count && plan->rows[row_index].values != NULL;
                 ++column_index) {
                planned_value_deinit(&plan->rows[row_index].values[column_index]);
            }
            free(plan->rows[row_index].values);
        }
    }
    free(plan->rows);
    free(plan->duplicate_update.assignments);
    loaded_index_infos_deinit(&plan->indexes, &plan->index_count);
    loaded_foreign_key_infos_deinit(&plan->foreign_keys, &plan->foreign_key_count);
    free(plan->columns);
    *plan = (struct planned_insert){0};
}

static void planned_value_deinit(struct planned_value *value) {
    if (value == NULL) {
        return;
    }

    free(value->text);
    *value = (struct planned_value){
        .is_null = false,
        .is_text = false,
        .integer = 0,
        .text = NULL,
        .text_length = 0U,
    };
}
