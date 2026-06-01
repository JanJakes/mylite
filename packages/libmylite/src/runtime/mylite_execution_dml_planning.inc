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

static int execute_insert_from_plan(
    struct mylite_db *database,
    const struct planned_insert *plan,
    mylite_result *result
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    struct insert_execution_counters counters = {0};
    struct mylite_statement_transaction transaction = {0};
    int rc = build_insert_sql(plan, &sql);

    if (plan->has_auto_increment) {
        counters.auto_increment_next_after_rows = plan->auto_increment_next;
    }
    if (rc == MYLITE_OK) {
        rc = begin_statement_transaction(database, &transaction);
    }
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = execute_insert_plan_rows(database, statement, plan, &counters);
    }
    rc = finalize_sqlite_statement(statement, rc);
    statement = NULL;
    if (rc == MYLITE_OK && plan->has_auto_increment &&
        counters.auto_increment_next_after_rows != plan->auto_increment_next) {
        rc = update_table_auto_increment_next(
            database,
            &plan->table,
            counters.auto_increment_next_after_rows
        );
    }
    if (rc == MYLITE_OK) {
        rc = touch_persistent_table_updated_time(
            database,
            &plan->table,
            counters.affected_rows,
            false
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_child_foreign_keys_after_write(database, plan->table.table_id);
    }
    if (rc == MYLITE_OK) {
        rc = commit_statement_transaction(database, &transaction);
    }
    if (rc != MYLITE_OK) {
        rollback_statement_transaction(database, &transaction);
    }
    free(sql);

    if (rc != MYLITE_OK) {
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

    mylite_result_set_affected_rows(result, counters.affected_rows);
    if (counters.inserted_generated_auto_increment) {
        database->session.last_insert_id =
            (uint64_t)counters.first_inserted_generated_auto_increment;
    }

    return MYLITE_OK;
}

static int execute_load_data_infile_from_plan(
    struct mylite_db *database,
    const struct planned_load_data_infile *plan,
    mylite_result *result
) {
    FILE *file = NULL;
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    struct insert_execution_counters counters = {0};
    struct mylite_statement_transaction transaction = {0};
    int rc = build_insert_sql(&plan->insert, &sql);

    if (plan->insert.has_auto_increment) {
        counters.auto_increment_next_after_rows = plan->insert.auto_increment_next;
    }
    if (rc == MYLITE_OK) {
        errno = 0;
        file = fopen(plan->file_path, "rb");
        if (file == NULL) {
            set_load_data_file_error(database, plan->file_path, errno);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        rc = begin_statement_transaction(database, &transaction);
    }
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = execute_load_data_file_rows(database, file, statement, plan, &counters);
    }
    rc = finalize_sqlite_statement(statement, rc);
    statement = NULL;
    if (file != NULL) {
        if (fclose(file) != 0 && rc == MYLITE_OK) {
            set_load_data_file_error(database, plan->file_path, errno);
            rc = MYLITE_ERROR;
        }
        file = NULL;
    }
    if (rc == MYLITE_OK && plan->insert.has_auto_increment &&
        counters.auto_increment_next_after_rows != plan->insert.auto_increment_next) {
        rc = update_table_auto_increment_next(
            database,
            &plan->insert.table,
            counters.auto_increment_next_after_rows
        );
    }
    if (rc == MYLITE_OK) {
        rc = touch_persistent_table_updated_time(
            database,
            &plan->insert.table,
            counters.affected_rows,
            false
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_child_foreign_keys_after_write(database, plan->insert.table.table_id);
    }
    if (rc == MYLITE_OK) {
        rc = commit_statement_transaction(database, &transaction);
    }
    if (rc != MYLITE_OK) {
        rollback_statement_transaction(database, &transaction);
    }
    free(sql);

    if (rc != MYLITE_OK) {
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

    mylite_result_set_affected_rows(result, counters.affected_rows);
    if (counters.inserted_generated_auto_increment) {
        database->session.last_insert_id =
            (uint64_t)counters.first_inserted_generated_auto_increment;
    }

    return MYLITE_OK;
}

static int execute_load_data_file_rows(
    struct mylite_db *database,
    FILE *file,
    sqlite3_stmt *statement,
    const struct planned_load_data_infile *plan,
    struct insert_execution_counters *counters
) {
    struct mylite_dynamic_string field;
    struct load_data_row row = {0};
    uint64_t physical_row_number = 0U;
    size_t loaded_row_count = 0U;
    bool escape_pending = false;
    bool row_pending = false;
    int rc = MYLITE_OK;
    int byte = 0;

    mylite_dynamic_string_init(&field);
    while (rc == MYLITE_OK && (byte = fgetc(file)) != EOF) {
        row_pending = true;
        if (escape_pending) {
            rc = mylite_dynamic_string_append_char(&field, '\\');
            if (rc == MYLITE_OK) {
                rc = mylite_dynamic_string_append_char(&field, (char)byte);
            }
            escape_pending = false;
            continue;
        }
        if (byte == '\\') {
            escape_pending = true;
            continue;
        }
        if (byte == '\t') {
            rc = finish_load_data_field(database, &row, &field, &escape_pending);
            continue;
        }
        if (byte == '\n') {
            rc = finish_load_data_field(database, &row, &field, &escape_pending);
            if (rc == MYLITE_OK) {
                ++physical_row_number;
                rc = process_load_data_row(
                    database,
                    plan,
                    statement,
                    &row,
                    physical_row_number,
                    &loaded_row_count,
                    counters
                );
            }
            load_data_row_reset(&row);
            row_pending = false;
            continue;
        }
        rc = mylite_dynamic_string_append_char(&field, (char)byte);
    }
    if (rc == MYLITE_OK && ferror(file) != 0) {
        set_load_data_file_error(database, plan->file_path, errno);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && row_pending) {
        rc = finish_load_data_field(database, &row, &field, &escape_pending);
        if (rc == MYLITE_OK) {
            ++physical_row_number;
            rc = process_load_data_row(
                database,
                plan,
                statement,
                &row,
                physical_row_number,
                &loaded_row_count,
                counters
            );
        }
    }

    load_data_row_deinit(&row);
    mylite_dynamic_string_deinit(&field);
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    return rc;
}

static int finish_load_data_field(
    struct mylite_db *database,
    struct load_data_row *row,
    struct mylite_dynamic_string *field,
    bool *escape_pending
) {
    char *raw_text = NULL;
    size_t raw_text_length = 0U;
    int rc = MYLITE_OK;

    if (*escape_pending) {
        rc = mylite_dynamic_string_append_char(field, '\\');
        *escape_pending = false;
    }
    if (rc == MYLITE_OK && field->text == NULL) {
        rc = mylite_dynamic_string_append(field, "");
    }
    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        }
        return rc;
    }

    raw_text_length = field->length;
    raw_text = mylite_dynamic_string_take(field);
    mylite_dynamic_string_init(field);
    if (raw_text == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    return load_data_row_append_field(database, row, raw_text, raw_text_length);
}

static int load_data_row_append_field(
    struct mylite_db *database,
    struct load_data_row *row,
    char *raw_text,
    size_t raw_text_length
) {
    char *text = NULL;
    size_t text_length = 0U;
    bool is_null = false;
    int rc = decode_load_data_field_text(
        database,
        raw_text,
        raw_text_length,
        &text,
        &text_length,
        &is_null
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (row->field_count == row->field_capacity) {
        size_t new_capacity = row->field_capacity == 0U ? 4U : row->field_capacity * 2U;
        struct load_data_field *new_fields = NULL;

        if (new_capacity < row->field_capacity || new_capacity > SIZE_MAX / sizeof(*new_fields)) {
            free(text);
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        new_fields = realloc(row->fields, new_capacity * sizeof(*new_fields));
        if (new_fields == NULL) {
            free(text);
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        row->fields = new_fields;
        row->field_capacity = new_capacity;
    }

    row->fields[row->field_count] = (struct load_data_field){
        .text = text,
        .text_length = text_length,
        .is_null = is_null,
    };
    ++row->field_count;
    return MYLITE_OK;
}

static int decode_load_data_field_text(
    struct mylite_db *database,
    char *raw_text,
    size_t raw_text_length,
    char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    struct mylite_dynamic_string decoded;
    int rc = MYLITE_OK;

    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;
    if (raw_text_length == 2U && raw_text[0] == '\\' && raw_text[1] == 'N') {
        free(raw_text);
        *out_is_null = true;
        return MYLITE_OK;
    }

    mylite_dynamic_string_init(&decoded);
    for (size_t index = 0U; rc == MYLITE_OK && index < raw_text_length; ++index) {
        char byte = raw_text[index];

        if (byte != '\\' || index + 1U >= raw_text_length) {
            rc = mylite_dynamic_string_append_char(&decoded, byte);
            continue;
        }

        ++index;
        switch (raw_text[index]) {
        case '0':
            rc = mylite_dynamic_string_append_char(&decoded, '\0');
            break;
        case 'b':
            rc = mylite_dynamic_string_append_char(&decoded, '\b');
            break;
        case 'n':
            rc = mylite_dynamic_string_append_char(&decoded, '\n');
            break;
        case 'r':
            rc = mylite_dynamic_string_append_char(&decoded, '\r');
            break;
        case 't':
            rc = mylite_dynamic_string_append_char(&decoded, '\t');
            break;
        case 'Z':
            rc = mylite_dynamic_string_append_char(&decoded, '\032');
            break;
        case '\\':
            rc = mylite_dynamic_string_append_char(&decoded, '\\');
            break;
        default:
            rc = mylite_dynamic_string_append_char(&decoded, raw_text[index]);
            break;
        }
    }
    free(raw_text);
    if (rc == MYLITE_OK && decoded.text == NULL) {
        rc = mylite_dynamic_string_append(&decoded, "");
    }
    if (rc == MYLITE_OK) {
        *out_text_length = decoded.length;
        *out_text = mylite_dynamic_string_take(&decoded);
        if (*out_text == NULL) {
            rc = MYLITE_NOMEM;
        }
    }
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    mylite_dynamic_string_deinit(&decoded);
    return rc;
}

static int process_load_data_row(
    struct mylite_db *database,
    const struct planned_load_data_infile *plan,
    sqlite3_stmt *statement,
    const struct load_data_row *row,
    uint64_t physical_row_number,
    size_t *loaded_row_count,
    struct insert_execution_counters *counters
) {
    if (physical_row_number <= plan->ignore_line_count) {
        return MYLITE_OK;
    }
    if (*loaded_row_count == SIZE_MAX) {
        set_runtime_error(database, "LOAD DATA row count is too large");
        return MYLITE_ERROR;
    }
    ++(*loaded_row_count);
    return execute_load_data_import_row(
        database,
        plan,
        statement,
        row,
        *loaded_row_count,
        counters
    );
}

static int execute_load_data_import_row(
    struct mylite_db *database,
    const struct planned_load_data_infile *plan,
    sqlite3_stmt *statement,
    const struct load_data_row *input_row,
    size_t row_number,
    struct insert_execution_counters *counters
) {
    struct planned_insert row_plan = plan->insert;
    struct planned_insert_row row = {0};
    int rc = convert_load_data_row_values(database, plan, input_row, row_number, &row);

    if (rc == MYLITE_OK && insert_allows_implicit_default_adjustment(database, &plan->insert) &&
        row_number == 1U) {
        rc = append_insert_omitted_column_warnings(
            database,
            &plan->insert,
            plan->target_indexes,
            plan->target_count
        );
    }
    if (rc == MYLITE_OK) {
        row_plan.rows = &row;
        row_plan.row_count = 1U;
        row_plan.auto_increment_next = counters->auto_increment_next_after_rows;
        row_plan.auto_increment_next_after_statement = counters->auto_increment_next_after_rows;
        rc = plan_insert_auto_increment_values(database, &row_plan);
    }
    if (rc == MYLITE_OK) {
        rc = plan_insert_string_key_values(database, &row_plan);
    }
    if (rc == MYLITE_OK) {
        rc = execute_insert_plan_rows(database, statement, &row_plan, counters);
    }

    planned_insert_row_deinit(&row, plan->insert.column_count);
    return rc;
}

static int convert_load_data_row_values(
    struct mylite_db *database,
    const struct planned_load_data_infile *plan,
    const struct load_data_row *input_row,
    size_t row_number,
    struct planned_insert_row *out_row
) {
    int rc = MYLITE_OK;

    if (input_row->field_count < plan->target_count) {
        if (session_sql_mode_is_strict(database)) {
            set_load_data_row_missing_error(database, row_number);
            return MYLITE_ERROR;
        }
        rc = append_load_data_row_missing_warnings(
            database,
            (struct load_data_missing_warning_request){
                .row_number = row_number,
                .warning_count = plan->target_count - input_row->field_count,
            }
        );
    } else if (input_row->field_count > plan->target_count) {
        if (session_sql_mode_is_strict(database)) {
            set_load_data_row_truncated_error(database, row_number);
            return MYLITE_ERROR;
        }
        rc = append_load_data_row_truncated_warning(database, row_number);
    }
    if (rc == MYLITE_OK) {
        rc = allocate_insert_row_values(database, &plan->insert, out_row);
    }
    for (size_t target_index = 0U; rc == MYLITE_OK && target_index < plan->target_count;
         ++target_index) {
        size_t column_index = plan->target_indexes[target_index];

        if (target_index >= input_row->field_count) {
            rc = convert_load_data_missing_field_value(
                database,
                &plan->insert.columns[column_index],
                &out_row->values[column_index]
            );
            continue;
        }
        rc = convert_load_data_field_value(
            database,
            &plan->insert.columns[column_index],
            &input_row->fields[target_index],
            row_number,
            &out_row->values[column_index]
        );
    }

    return rc;
}

static int convert_load_data_field_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    const struct load_data_field *field,
    size_t row_number,
    struct planned_value *out_value
) {
    char *text = NULL;
    bool handled_empty_temporal = false;
    int rc = MYLITE_OK;

    planned_value_deinit(out_value);
    *out_value = (struct planned_value){.is_null = true, .is_text = false, .integer = 0};
    if (field->is_null) {
        return convert_load_data_null_field(database, column, row_number, out_value);
    }
    if (strcmp(column->physical_type, "INTEGER") == 0) {
        return convert_load_data_integer_field(database, column, field, row_number, out_value);
    }
    if (column_descriptor_is_char(column) || column_descriptor_is_varchar(column) ||
        column_descriptor_is_text_family(column)) {
        return convert_load_data_text_field(database, column, field, row_number, out_value);
    }

    rc = convert_load_data_empty_temporal_field(
        database,
        column,
        field,
        row_number,
        out_value,
        &handled_empty_temporal
    );
    if (handled_empty_temporal || rc != MYLITE_OK) {
        return rc;
    }

    text = copy_load_data_field_text(database, field);
    if (text == NULL) {
        return MYLITE_NOMEM;
    }
    if (column_descriptor_is_year(column)) {
        return convert_year_string_value(
            database,
            text,
            field->text_length,
            column,
            row_number,
            false,
            out_value
        );
    }
    if (column_descriptor_is_date(column)) {
        return convert_relaxed_date_storage_text(
            database,
            text,
            field->text_length,
            column->name,
            row_number,
            false,
            out_value
        );
    }
    if (column_descriptor_is_time(column)) {
        return canonicalize_time_text(
            database,
            text,
            field->text_length,
            column->name,
            row_number,
            false,
            out_value
        );
    }
    if (column_descriptor_is_datetime(column)) {
        return convert_relaxed_datetime_storage_text(
            database,
            text,
            field->text_length,
            column->name,
            row_number,
            false,
            database == NULL ? 0 : database->session.time_zone_offset_minutes,
            out_value
        );
    }
    if (column_descriptor_is_timestamp(column)) {
        return convert_relaxed_timestamp_storage_text(
            database,
            text,
            field->text_length,
            column->name,
            row_number,
            false,
            0,
            out_value
        );
    }

    free(text);
    set_unsupported_error(
        database,
        "LOAD DATA INFILE supports only baseline integer, character, and temporal targets"
    );
    return MYLITE_ERROR;
}

static int convert_load_data_missing_field_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    planned_value_deinit(out_value);
    *out_value = (struct planned_value){.is_null = true, .is_text = false, .integer = 0};
    if (load_data_missing_field_stores_null(column)) {
        return MYLITE_OK;
    }
    return make_insert_ignore_implicit_value(database, column, out_value);
}

static bool load_data_missing_field_stores_null(
    const struct mylite_catalog_column_descriptor *column
) {
    if (column == NULL) {
        return false;
    }
    return (column->is_nullable &&
            (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_NONE ||
             column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_NULL_EXPRESSION ||
             column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_NO_EXPLICIT)) != 0;
}

static int convert_load_data_empty_temporal_field(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    const struct load_data_field *field,
    size_t row_number,
    struct planned_value *out_value,
    bool *out_handled
) {
    int rc = MYLITE_OK;

    *out_handled = false;
    if (session_sql_mode_is_strict(database) || column == NULL || field == NULL ||
        field->text_length != 0U ||
        (!column_descriptor_is_year(column) && !column_descriptor_is_date(column) &&
         !column_descriptor_is_time(column) && !column_descriptor_is_datetime(column) &&
         !column_descriptor_is_timestamp(column))) {
        return MYLITE_OK;
    }

    if (column_descriptor_is_year(column)) {
        char *text = copy_temporal_text(database, "", 0U);

        *out_handled = true;
        if (text == NULL) {
            return MYLITE_NOMEM;
        }
        return convert_year_string_value(database, text, 0U, column, row_number, true, out_value);
    }
    if (column_descriptor_is_date(column)) {
        *out_handled = true;
        rc = append_out_of_range_warning(database, column->name, row_number);
        if (rc != MYLITE_OK) {
            return rc;
        }
        return make_zero_date_value(database, out_value);
    }
    if (column_descriptor_is_time(column)) {
        *out_handled = true;
        return make_zero_time_value(database, out_value);
    }
    if (column_descriptor_is_datetime(column)) {
        *out_handled = true;
        rc = append_out_of_range_warning(database, column->name, row_number);
        if (rc != MYLITE_OK) {
            return rc;
        }
        return make_zero_datetime_value(database, out_value);
    }
    if (column_descriptor_is_timestamp(column)) {
        *out_handled = true;
        rc = append_out_of_range_warning(database, column->name, row_number);
        if (rc != MYLITE_OK) {
            return rc;
        }
        return make_zero_timestamp_value(database, out_value);
    }

    return MYLITE_OK;
}

static int convert_load_data_null_field(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    struct planned_value *out_value
) {
    if (column_descriptor_is_auto_increment(column) || column->is_nullable) {
        out_value->is_null = true;
        return MYLITE_OK;
    }
    if (session_sql_mode_is_strict(database)) {
        set_load_data_null_to_not_null_error(database, column->name, row_number);
        return MYLITE_ERROR;
    }

    int rc = append_load_data_null_to_not_null_warning(database, column->name, row_number);
    if (rc != MYLITE_OK) {
        return rc;
    }
    return make_insert_ignore_implicit_value(database, column, out_value);
}

static int convert_load_data_integer_field(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    const struct load_data_field *field,
    size_t row_number,
    struct planned_value *out_value
) {
    bool is_negative = false;
    bool strict_mode = session_sql_mode_is_strict(database);
    uint64_t magnitude = 0U;
    enum dml_numeric_string_parse_result parse_result =
        parse_dml_integer_string_text(field->text, field->text_length, &magnitude, &is_negative);
    int rc = MYLITE_OK;

    if (parse_result == DML_NUMERIC_STRING_PARSE_INVALID) {
        if (strict_mode) {
            set_incorrect_integer_value_error(database, field->text, column->name, row_number);
            return MYLITE_ERROR;
        }
        rc =
            append_incorrect_integer_value_warning(database, field->text, column->name, row_number);
        if (rc == MYLITE_OK) {
            *out_value = (struct planned_value){.is_null = false, .is_text = false, .integer = 0};
        }
        return rc;
    }
    if (parse_result == DML_NUMERIC_STRING_PARSE_OVERFLOW) {
        if (strict_mode) {
            set_out_of_range_error(database, column->name, row_number);
            return MYLITE_ERROR;
        }
        out_value->is_null = false;
        return clip_integer_for_column(
            database,
            is_negative,
            column,
            row_number,
            &out_value->integer
        );
    }

    bool exceeds_range = false;
    rc = dml_integer_value_exceeds_column_range(
        database,
        magnitude,
        is_negative,
        column,
        &exceeds_range
    );
    if (rc == MYLITE_OK && parse_result == DML_NUMERIC_STRING_PARSE_TRUNCATED && !exceeds_range) {
        if (strict_mode) {
            set_data_truncated_error(database, column->name, row_number);
            return MYLITE_ERROR;
        }
        rc = append_data_truncated_warning(database, column->name, row_number);
    }
    if (rc == MYLITE_OK) {
        out_value->is_null = false;
        if (strict_mode) {
            rc = convert_integer_for_column_with_policy(
                database,
                magnitude,
                is_negative,
                column,
                row_number,
                false,
                &out_value->integer
            );
        } else {
            rc = convert_integer_for_column_with_policy(
                database,
                magnitude,
                is_negative,
                column,
                row_number,
                true,
                &out_value->integer
            );
        }
    }
    return rc;
}

static int convert_load_data_text_field(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    const struct load_data_field *field,
    size_t row_number,
    struct planned_value *out_value
) {
    char *text = copy_load_data_field_text(database, field);
    struct char_text_conversion conversion = {
        .text = text,
        .text_length = field->text_length,
        .row_number = row_number,
    };
    bool allow_nonspace_truncation = dml_allows_string_truncation_adjustment(database, false);
    int rc = MYLITE_OK;

    if (text == NULL) {
        return MYLITE_NOMEM;
    }
    if (column_descriptor_is_char(column)) {
        rc = convert_char_text(database, column, &conversion, allow_nonspace_truncation);
    } else if (column_descriptor_is_varchar(column)) {
        rc = convert_varchar_text(database, column, &conversion, allow_nonspace_truncation);
    } else {
        rc = convert_text_family_text(
            database,
            column,
            &conversion,
            allow_nonspace_truncation,
            false
        );
    }
    if (rc != MYLITE_OK) {
        free(text);
        return rc;
    }
    return assign_text_value(database, text, conversion.text_length, out_value);
}

static char *copy_load_data_field_text(
    struct mylite_db *database,
    const struct load_data_field *field
) {
    char *copy = malloc(field->text_length + 1U);

    if (copy == NULL) {
        set_nomem_error(database);
        return NULL;
    }
    memcpy(copy, field->text, field->text_length);
    copy[field->text_length] = '\0';
    return copy;
}

static void load_data_row_deinit(struct load_data_row *row) {
    if (row == NULL) {
        return;
    }

    for (size_t index = 0U; index < row->field_count; ++index) {
        free(row->fields[index].text);
    }
    free(row->fields);
    *row = (struct load_data_row){0};
}

static void load_data_row_reset(struct load_data_row *row) {
    if (row == NULL) {
        return;
    }
    for (size_t index = 0U; index < row->field_count; ++index) {
        free(row->fields[index].text);
    }
    row->field_count = 0U;
}

static int execute_insert_plan_rows(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_insert *plan,
    struct insert_execution_counters *counters
) {
    int rc = MYLITE_OK;

    for (size_t row_index = 0U; rc == MYLITE_OK && row_index < plan->row_count; ++row_index) {
        bool skip_row = false;

        if (plan->ignore_errors && plan->foreign_key_count != 0U) {
            rc = validate_insert_ignore_row_foreign_keys(database, plan, row_index, &skip_row);
        }
        if (rc == MYLITE_OK && skip_row) {
            continue;
        }
        if (rc == MYLITE_OK) {
            rc = execute_insert_plan_row_with_retries(
                database,
                statement,
                plan,
                row_index,
                counters
            );
        }
    }

    return rc;
}

static int execute_insert_plan_row_with_retries(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_insert *plan,
    size_t row_index,
    struct insert_execution_counters *counters
) {
    size_t replace_attempt_count = 0U;
    bool row_complete = false;
    int rc = MYLITE_OK;

    while (rc == MYLITE_OK && !row_complete) {
        int sqlite_step_rc = SQLITE_OK;

        rc = execute_insert_plan_row(statement, plan, row_index, &sqlite_step_rc);
        if (rc == MYLITE_OK) {
            row_complete = true;
            rc = finish_successful_insert_plan_row(database, plan, row_index, counters);
        } else if (sqlite_status_is_constraint(sqlite_step_rc)) {
            rc = handle_insert_plan_constraint(
                database,
                sqlite_step_rc,
                statement,
                plan,
                row_index,
                &replace_attempt_count,
                counters,
                &row_complete
            );
        }
    }

    return rc;
}

static int finish_successful_insert_plan_row(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    struct insert_execution_counters *counters
) {
    ++counters->affected_rows;
    record_inserted_generated_auto_increment(plan, row_index, counters);
    if (!plan->has_auto_increment) {
        return MYLITE_OK;
    }

    return advance_auto_increment_after_insert_row(
        database,
        plan,
        row_index,
        &counters->auto_increment_next_after_rows
    );
}

static int handle_insert_plan_constraint(
    struct mylite_db *database,
    int sqlite_step_rc,
    sqlite3_stmt *statement,
    const struct planned_insert *plan,
    size_t row_index,
    size_t *replace_attempt_count,
    struct insert_execution_counters *counters,
    bool *out_row_complete
) {
    int rc = MYLITE_OK;

    *out_row_complete = false;
    if (sqlite_status_is_constraint(sqlite_step_rc)) {
        bool was_generated_range_violation = false;
        bool was_check_violation = false;
        bool was_generated_not_null_violation = false;

        rc = handle_generated_range_constraint_violation(
            database,
            (struct generated_constraint_columns){
                .columns = plan->columns,
                .column_count = plan->column_count,
            },
            row_index + 1U,
            &was_generated_range_violation
        );
        if (rc != MYLITE_OK || was_generated_range_violation) {
            return rc;
        }
        rc = handle_check_constraint_violation(
            database,
            plan->table.table_id,
            plan->ignore_errors,
            &was_check_violation
        );
        if (rc != MYLITE_OK || was_check_violation) {
            if (rc == MYLITE_OK && plan->ignore_errors) {
                rc = reset_insert_statement_after_constraint(statement);
                *out_row_complete = rc == MYLITE_OK;
            }
            return rc;
        }
        rc = handle_generated_not_null_violation(
            database,
            sqlite_step_rc,
            plan->columns,
            plan->column_count,
            &was_generated_not_null_violation
        );
        if (rc != MYLITE_OK || was_generated_not_null_violation) {
            return rc;
        }
    }
    if (plan->duplicate_update.has_clause) {
        rc = handle_insert_duplicate_key_update(
            database,
            sqlite_step_rc,
            statement,
            plan,
            row_index,
            counters
        );
        *out_row_complete = rc == MYLITE_OK;
        if (rc == MYLITE_OK && plan->has_auto_increment) {
            rc = advance_auto_increment_after_duplicate_attempt(
                database,
                plan,
                row_index,
                &counters->auto_increment_next_after_rows
            );
        }
        return rc;
    }
    if (plan->replace_existing_rows) {
        return handle_replace_insert_constraint(
            database,
            sqlite_step_rc,
            statement,
            plan,
            row_index,
            replace_attempt_count,
            counters
        );
    }

    rc = handle_insert_unique_key_conflict(database, sqlite_step_rc, statement, plan, row_index);
    if (rc == MYLITE_OK && plan->has_auto_increment && plan->ignore_errors) {
        rc = advance_auto_increment_after_ignored_duplicate(
            database,
            plan,
            row_index,
            &counters->auto_increment_next_after_rows
        );
    }
    *out_row_complete = rc == MYLITE_OK;
    return rc;
}

static int handle_replace_insert_constraint(
    struct mylite_db *database,
    int sqlite_step_rc,
    sqlite3_stmt *statement,
    const struct planned_insert *plan,
    size_t row_index,
    size_t *replace_attempt_count,
    struct insert_execution_counters *counters
) {
    int rc = MYLITE_OK;

    if (*replace_attempt_count >= plan->index_count) {
        rc = reset_insert_statement_after_constraint(statement);
        if (rc == MYLITE_OK) {
            set_runtime_error(database, "REPLACE conflict retry limit exceeded");
            rc = MYLITE_ERROR;
        }
        return rc;
    }

    ++(*replace_attempt_count);
    return handle_replace_unique_key_conflict(
        database,
        sqlite_step_rc,
        statement,
        plan,
        row_index,
        counters
    );
}

static int validate_insert_ignore_row_foreign_keys(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    bool *out_skip_row
) {
    int rc = MYLITE_OK;

    *out_skip_row = false;
    if (!foreign_key_checks_system_variable_value(database, false)) {
        return MYLITE_OK;
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < plan->foreign_key_count; ++index) {
        bool violates = false;

        rc = validate_insert_ignore_row_foreign_key(
            database,
            plan,
            &plan->foreign_keys[index],
            row_index,
            &violates
        );
        if (rc == MYLITE_OK && violates) {
            rc = append_no_referenced_row_warning(database);
            *out_skip_row = rc == MYLITE_OK;
            break;
        }
    }

    return rc;
}

static int validate_insert_ignore_row_foreign_key(
    struct mylite_db *database,
    const struct planned_insert *plan,
    const struct loaded_foreign_key_info *foreign_key,
    size_t row_index,
    bool *out_violates
) {
    struct loaded_index_part *parent_parts = NULL;
    struct planned_value *parent_values = NULL;
    struct loaded_index_info parent_index = {0};
    bool parent_exists = false;
    int rc = MYLITE_OK;

    *out_violates = false;
    if (foreign_key->part_count == 0U || row_index >= plan->row_count) {
        set_runtime_error(database, "invalid foreign-key insert descriptor");
        return MYLITE_ERROR;
    }
    if (foreign_key->part_count > SIZE_MAX / sizeof(*parent_parts) ||
        foreign_key->part_count > SIZE_MAX / sizeof(*parent_values)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    parent_parts = calloc(foreign_key->part_count, sizeof(*parent_parts));
    parent_values = calloc(foreign_key->part_count, sizeof(*parent_values));
    if (parent_parts == NULL || parent_values == NULL) {
        free(parent_parts);
        free(parent_values);
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < foreign_key->part_count;
         ++part_index) {
        const struct loaded_foreign_key_part *foreign_key_part = &foreign_key->parts[part_index];
        const struct planned_value *child_value = NULL;
        size_t child_column_index = 0U;

        rc = find_column_index_by_id(
            plan->columns,
            plan->column_count,
            &child_column_index,
            foreign_key_part->foreign_key_column.child_column_id
        );
        if (rc != MYLITE_OK) {
            set_runtime_error(database, "foreign-key child column descriptor is stale");
            rc = MYLITE_ERROR;
            break;
        }

        child_value = &plan->rows[row_index].values[child_column_index];
        if (child_value->is_null) {
            free(parent_parts);
            free(parent_values);
            return MYLITE_OK;
        }
        parent_parts[part_index] = (struct loaded_index_part){
            .index_column =
                {
                    .column_id = foreign_key_part->foreign_key_column.parent_column_id,
                    .ordinal_position = (int64_t)part_index + 1,
                    .has_prefix_length = false,
                    .prefix_length = 0,
                    .sort_direction = MYLITE_CATALOG_INDEX_SORT_DIRECTION_ASC,
                },
            .column = foreign_key_part->parent_column,
            .column_index = part_index,
        };
        parent_values[part_index] = *child_value;
    }
    parent_index = (struct loaded_index_info){
        .index = foreign_key->parent_index,
        .parts = parent_parts,
        .part_count = foreign_key->part_count,
    };

    if (rc == MYLITE_OK) {
        rc = unique_key_tuple_exists(
            database,
            foreign_key->parent_table.physical_name,
            &parent_index,
            parent_values,
            foreign_key->part_count,
            &parent_exists
        );
    }
    free(parent_parts);
    free(parent_values);
    if (rc == MYLITE_OK && !parent_exists) {
        *out_violates = true;
    }
    return rc;
}

static int execute_insert_plan_row(
    sqlite3_stmt *statement,
    const struct planned_insert *plan,
    size_t row_index,
    int *out_sqlite_step_rc
) {
    int rc = bind_insert_row(statement, plan, row_index);

    if (rc == MYLITE_OK) {
        rc = step_insert_row(statement, out_sqlite_step_rc);
    }

    return rc;
}

static int advance_auto_increment_after_insert_row(
    const struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    int64_t *auto_increment_next_after_rows
) {
    const struct planned_value *auto_value =
        &plan->rows[row_index].values[plan->auto_increment_column_index];

    if (auto_value->is_null || auto_value->is_text || auto_value->integer <= 0) {
        return MYLITE_OK;
    }

    if (plan->rows[row_index].generated_auto_increment) {
        return next_auto_increment_after_generated_value(
            database,
            auto_value->integer,
            &plan->auto_increment_range,
            auto_increment_next_after_rows
        );
    }

    return next_auto_increment_after_explicit_insert_value(
        *auto_increment_next_after_rows,
        auto_value->integer,
        &plan->auto_increment_range,
        auto_increment_next_after_rows
    );
}

static int advance_auto_increment_after_duplicate_attempt(
    const struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    int64_t *auto_increment_next_after_rows
) {
    if (row_index >= plan->row_count || !plan->rows[row_index].generated_auto_increment) {
        return MYLITE_OK;
    }

    return advance_auto_increment_after_insert_row(
        database,
        plan,
        row_index,
        auto_increment_next_after_rows
    );
}

static int advance_auto_increment_after_ignored_duplicate(
    const struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    int64_t *auto_increment_next_after_rows
) {
    if (row_index >= plan->row_count || plan->rows[row_index].generated_auto_increment) {
        return MYLITE_OK;
    }

    return advance_auto_increment_after_insert_row(
        database,
        plan,
        row_index,
        auto_increment_next_after_rows
    );
}

static void record_inserted_generated_auto_increment(
    const struct planned_insert *plan,
    size_t row_index,
    struct insert_execution_counters *counters
) {
    const struct planned_value *auto_value = NULL;

    if (!plan->has_auto_increment || row_index >= plan->row_count ||
        !plan->rows[row_index].generated_auto_increment ||
        counters->inserted_generated_auto_increment) {
        return;
    }

    auto_value = &plan->rows[row_index].values[plan->auto_increment_column_index];
    counters->first_inserted_generated_auto_increment = auto_value->integer;
    counters->inserted_generated_auto_increment = true;
}

static int handle_insert_unique_key_conflict(
    struct mylite_db *database,
    int sqlite_step_rc,
    sqlite3_stmt *statement,
    const struct planned_insert *plan,
    size_t row_index
) {
    char value_text[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY] = "";
    const struct loaded_index_info *conflicting_index = NULL;
    int rc = find_insert_unique_key_conflict(
        database,
        sqlite_step_rc,
        plan,
        row_index,
        &conflicting_index,
        value_text,
        sizeof(value_text)
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (conflicting_index == NULL) {
        return mylite_sqlite_status_to_mylite(sqlite_step_rc);
    }
    if (!plan->ignore_errors) {
        set_duplicate_key_error(
            database,
            plan->target.table_name,
            conflicting_index->index.name,
            value_text
        );
        return MYLITE_ERROR;
    }

    rc = append_duplicate_key_warning(
        database,
        plan->target.table_name,
        conflicting_index->index.name,
        value_text
    );
    if (rc == MYLITE_OK) {
        int reset_rc = sqlite3_reset(statement);

        if (reset_rc != SQLITE_OK && !sqlite_status_is_constraint(reset_rc)) {
            rc = mylite_sqlite_status_to_mylite(reset_rc);
        }
    }

    return rc;
}

static int handle_insert_duplicate_key_update(
    struct mylite_db *database,
    int sqlite_step_rc,
    sqlite3_stmt *insert_statement,
    const struct planned_insert *plan,
    size_t row_index,
    struct insert_execution_counters *counters
) {
    char value_text[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY] = "";
    const struct loaded_index_info *conflicting_index = NULL;
    int rc = find_insert_unique_key_conflict(
        database,
        sqlite_step_rc,
        plan,
        row_index,
        &conflicting_index,
        value_text,
        sizeof(value_text)
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (conflicting_index == NULL) {
        return mylite_sqlite_status_to_mylite(sqlite_step_rc);
    }

    rc = reset_insert_statement_after_constraint(insert_statement);
    if (rc == MYLITE_OK) {
        rc = apply_insert_duplicate_key_update(
            database,
            plan,
            row_index,
            conflicting_index,
            counters
        );
    }

    return rc;
}

static int handle_replace_unique_key_conflict(
    struct mylite_db *database,
    int sqlite_step_rc,
    sqlite3_stmt *insert_statement,
    const struct planned_insert *plan,
    size_t row_index,
    struct insert_execution_counters *counters
) {
    char value_text[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY] = "";
    sqlite3_int64 deleted_rows = 0;
    const struct loaded_index_info *conflicting_index = NULL;
    bool exact_replacement = false;
    int rc = find_insert_unique_key_conflict(
        database,
        sqlite_step_rc,
        plan,
        row_index,
        &conflicting_index,
        value_text,
        sizeof(value_text)
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (conflicting_index == NULL) {
        return mylite_sqlite_status_to_mylite(sqlite_step_rc);
    }

    rc = reset_insert_statement_after_constraint(insert_statement);
    if (rc == MYLITE_OK) {
        rc = replace_conflicting_row_matches_plan(
            database,
            plan,
            row_index,
            conflicting_index,
            &exact_replacement
        );
    }
    if (rc == MYLITE_OK) {
        rc = delete_replace_conflicting_row(
            database,
            plan,
            conflicting_index,
            row_index,
            &deleted_rows
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_parent_foreign_keys_after_write(database, plan->table.table_id);
    }
    if (rc == MYLITE_OK && !exact_replacement) {
        counters->affected_rows += (int64_t)deleted_rows;
    }

    return rc;
}

static int replace_conflicting_row_matches_plan(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    const struct loaded_index_info *conflicting_index,
    bool *out_matches
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = MYLITE_OK;

    *out_matches = false;
    rc = build_replace_conflicting_row_select_sql(plan, conflicting_index, &sql);
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_unique_key_tuple_parameters(
            statement,
            conflicting_index,
            plan->rows[row_index].values,
            plan->column_count
        );
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = compare_replace_conflicting_row(statement, plan, row_index, out_matches);
        } else if (sqlite_rc == SQLITE_DONE) {
            set_runtime_error(database, "REPLACE conflicting row disappeared before delete");
            rc = MYLITE_ERROR;
        } else {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);

    return rc;
}

static int build_replace_conflicting_row_select_sql(
    const struct planned_insert *plan,
    const struct loaded_index_info *conflicting_index,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    if (conflicting_index == NULL || conflicting_index->part_count == 0U) {
        mylite_dynamic_string_deinit(&string);
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(&string, "SELECT ");
    if (rc == MYLITE_OK) {
        rc = append_insert_column_names(&string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " WHERE ");
    }
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < conflicting_index->part_count;
         ++part_index) {
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(&string, " AND ");
        }
        if (rc == MYLITE_OK) {
            rc = append_loaded_key_part_sql(&string, &conflicting_index->parts[part_index], NULL);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(&string, " = ");
        }
        if (rc == MYLITE_OK) {
            rc = append_loaded_key_part_parameter_sql(
                &string,
                &conflicting_index->parts[part_index],
                part_index + 1U
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " LIMIT 1");
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

static int compare_replace_conflicting_row(
    sqlite3_stmt *statement,
    const struct planned_insert *plan,
    size_t row_index,
    bool *out_matches
) {
    int statement_column_index = 0;

    *out_matches = false;
    if (row_index >= plan->row_count) {
        return MYLITE_ERROR;
    }
    for (size_t column_index = 0U; column_index < plan->column_count; ++column_index) {
        if (plan->columns[column_index].is_generated) {
            continue;
        }
        if (statement_column_index >= INT_MAX) {
            return MYLITE_ERROR;
        }
        if (!sqlite_column_matches_planned_value(
                statement,
                statement_column_index,
                &plan->rows[row_index].values[column_index]
            )) {
            return MYLITE_OK;
        }
        ++statement_column_index;
    }

    *out_matches = true;
    return MYLITE_OK;
}

static bool sqlite_column_matches_planned_value(
    sqlite3_stmt *statement,
    int column_index,
    const struct planned_value *value
) {
    int sqlite_type = sqlite3_column_type(statement, column_index);

    if (value == NULL) {
        return false;
    }
    if (value->is_null) {
        return sqlite_type == SQLITE_NULL;
    }
    if (sqlite_type == SQLITE_NULL) {
        return false;
    }
    if (value->is_blob) {
        int byte_count = sqlite3_column_bytes(statement, column_index);
        const void *bytes = sqlite3_column_blob(statement, column_index);

        if (byte_count < 0 || (size_t)byte_count != value->text_length) {
            return false;
        }
        if (value->text_length != 0U && (bytes == NULL || value->text == NULL)) {
            return false;
        }
        if (value->text_length == 0U) {
            return true;
        }
        return memcmp(bytes, value->text, value->text_length) == 0;
    }
    if (value->is_text) {
        int byte_count = sqlite3_column_bytes(statement, column_index);
        const unsigned char *text = sqlite3_column_text(statement, column_index);

        if (byte_count < 0 || (size_t)byte_count != value->text_length) {
            return false;
        }
        if (value->text_length != 0U && (text == NULL || value->text == NULL)) {
            return false;
        }
        if (value->text_length == 0U) {
            return true;
        }
        return memcmp(text, value->text, value->text_length) == 0;
    }
    if (value->is_real) {
        return sqlite3_column_double(statement, column_index) == value->real;
    }

    return sqlite3_column_int64(statement, column_index) == (sqlite3_int64)value->integer;
}

static int delete_replace_conflicting_row(
    struct mylite_db *database,
    const struct planned_insert *plan,
    const struct loaded_index_info *conflicting_index,
    size_t row_index,
    sqlite3_int64 *out_deleted_rows
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = MYLITE_OK;

    *out_deleted_rows = 0;
    rc = build_replace_conflicting_row_delete_sql(plan, conflicting_index, &sql);
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_unique_key_tuple_parameters(
            statement,
            conflicting_index,
            plan->rows[row_index].values,
            plan->column_count
        );
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            *out_deleted_rows = sqlite3_changes64(database->sqlite);
            if (*out_deleted_rows <= 0) {
                set_runtime_error(database, "REPLACE failed to delete conflicting row");
                rc = MYLITE_ERROR;
            }
        } else {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);

    return rc;
}

static int build_replace_conflicting_row_delete_sql(
    const struct planned_insert *plan,
    const struct loaded_index_info *conflicting_index,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    if (conflicting_index == NULL || conflicting_index->part_count == 0U) {
        mylite_dynamic_string_deinit(&string);
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(&string, "DELETE FROM ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " WHERE ");
    }
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < conflicting_index->part_count;
         ++part_index) {
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(&string, " AND ");
        }
        if (rc == MYLITE_OK) {
            rc = append_loaded_key_part_sql(&string, &conflicting_index->parts[part_index], NULL);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(&string, " = ");
        }
        if (rc == MYLITE_OK) {
            rc = append_loaded_key_part_parameter_sql(
                &string,
                &conflicting_index->parts[part_index],
                part_index + 1U
            );
        }
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

static int plan_update(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_update *out_plan
) {
    if (statement != NULL && statement->kind == MYLITE_SQL_AST_JOINED_UPDATE_STATEMENT) {
        return plan_joined_update(database, statement, out_plan);
    }
    return plan_single_table_update(database, statement, out_plan);
}

static int plan_single_table_update(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_update *out_plan
) {
    const struct mylite_sql_ast_node *target = child_at(statement, 0U);
    const struct mylite_sql_ast_node *assignment_list = child_at(statement, 1U);
    struct update_optional_clauses clauses = {0};
    const struct mylite_sql_ast_node *optional_clause = NULL;
    struct mylite_catalog_column_descriptor *table_columns = NULL;
    struct primary_key_info primary_key = primary_key_info_init();
    size_t table_column_count = 0U;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_update){0};
    optional_clause = child_at(statement, 2U);
    while (optional_clause != NULL) {
        rc = collect_single_table_update_optional_clause(
            database,
            optional_clause,
            out_plan,
            &clauses
        );
        if (rc != MYLITE_OK) {
            return MYLITE_ERROR;
        }
        optional_clause = optional_clause->next_sibling;
    }

    rc = resolve_visible_writable_table_reference(
        database,
        single_table_update_target_name_node(target),
        &out_plan->target,
        &out_plan->table
    );
    if (rc == MYLITE_OK) {
        rc = validate_update_index_hints(database, target, &out_plan->table);
    }
    if (rc == MYLITE_OK) {
        rc = load_table_columns(
            database,
            out_plan->table.table_id,
            &table_columns,
            &table_column_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = load_primary_key_info(
            database,
            out_plan->table.table_id,
            table_columns,
            table_column_count,
            &primary_key
        );
    }
    if (rc == MYLITE_OK && primary_key.has_primary_key) {
        out_plan->has_primary_key = true;
        if (primary_key.part_count == 1U) {
            out_plan->primary_key_column_index = primary_key.parts[0].column_index;
            out_plan->primary_key_column_id = primary_key.parts[0].index_column.column_id;
        }
    }
    if (rc == MYLITE_OK) {
        rc = load_table_index_infos(
            database,
            out_plan->table.table_id,
            table_columns,
            table_column_count,
            &out_plan->indexes,
            &out_plan->index_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_update_assignment(
            database,
            assignment_list,
            table_columns,
            table_column_count,
            out_plan
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_update_ignore_assignment_support(database, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = validate_update_default_function_sources(
            database,
            out_plan,
            table_columns,
            table_column_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_single_table_update_predicate(
            database,
            clauses.where_clause,
            table_columns,
            table_column_count,
            out_plan
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_select_order(
            database,
            clauses.order_clause,
            NULL,
            NULL,
            table_columns,
            table_column_count,
            false,
            false,
            &out_plan->order
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_update_limit(database, clauses.limit_clause, &out_plan->limit);
    }
    if (rc == MYLITE_OK && out_plan->limit.has_limit && out_plan->limit.row_count > 0) {
        rc = choose_sqlite_rowid_alias(
            database,
            table_columns,
            table_column_count,
            "UPDATE LIMIT requires an unshadowed SQLite rowid alias",
            &out_plan->rowid_alias
        );
    }
    if (rc == MYLITE_OK) {
        out_plan->columns = table_columns;
        out_plan->column_count = table_column_count;
        table_columns = NULL;
        table_column_count = 0U;
    }

    free(table_columns);
    primary_key_info_deinit(&primary_key);
    if (rc != MYLITE_OK) {
        planned_update_deinit(out_plan);
    }

    return rc;
}

static int plan_single_table_update_predicate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_update *out_plan
) {
    struct select_source_context source_context = {
        .source = &out_plan->target,
    };
    const struct select_predicate_plan_options predicate_options = {
        .allow_exists = true,
        .allow_in_subquery = true,
    };
    int rc = plan_select_predicate_with_options(
        database,
        where_clause,
        &source_context,
        table_columns,
        table_column_count,
        &predicate_options,
        &out_plan->predicate
    );

    if (rc == MYLITE_OK) {
        rc = validate_update_subquery_predicate_sources(database, out_plan);
    }
    return rc;
}

static int validate_update_subquery_predicate_sources(
    struct mylite_db *database,
    const struct planned_update *plan
) {
    if (plan == NULL || !planned_select_predicate_has_expression(&plan->predicate)) {
        return MYLITE_OK;
    }

    for (size_t node_index = 0U; node_index < plan->predicate.node_count; ++node_index) {
        int rc = validate_update_subquery_predicate_node_source(
            database,
            plan,
            &plan->predicate.nodes[node_index]
        );

        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return MYLITE_OK;
}

static int validate_update_subquery_predicate_node_source(
    struct mylite_db *database,
    const struct planned_update *plan,
    const struct planned_select_predicate_node *node
) {
    if (node == NULL) {
        return MYLITE_OK;
    }
    if (node->kind == PLANNED_SELECT_PREDICATE_EXISTS && node->exists_subquery != NULL &&
        node->exists_subquery->has_table_source &&
        node->exists_subquery->source.table.table_id == plan->table.table_id) {
        set_update_table_used_error(database, plan->target.table_name);
        return MYLITE_ERROR;
    }
    if (node->kind == PLANNED_SELECT_PREDICATE_IN && node->in_subquery != NULL &&
        node->in_subquery->source.table.table_id == plan->table.table_id) {
        set_update_table_used_error(database, plan->target.table_name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int collect_single_table_update_optional_clause(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *optional_clause,
    struct planned_update *plan,
    struct update_optional_clauses *clauses
) {
    if (optional_clause == NULL || plan == NULL || clauses == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    switch (optional_clause->kind) {
    case MYLITE_SQL_AST_WHERE_CLAUSE:
        clauses->where_clause = optional_clause;
        return MYLITE_OK;
    case MYLITE_SQL_AST_ORDER_BY_CLAUSE:
        clauses->order_clause = optional_clause;
        return MYLITE_OK;
    case MYLITE_SQL_AST_LIMIT_CLAUSE:
        clauses->limit_clause = optional_clause;
        return MYLITE_OK;
    case MYLITE_SQL_AST_UPDATE_LOW_PRIORITY_MODIFIER:
        plan->low_priority = true;
        return MYLITE_OK;
    case MYLITE_SQL_AST_UPDATE_IGNORE_MODIFIER:
        plan->ignore_errors = true;
        return MYLITE_OK;
    default:
        set_unsupported_error(
            database,
            "UPDATE supports only modifiers, SET, WHERE, ORDER BY, and LIMIT"
        );
        return MYLITE_ERROR;
    }
}

static const struct mylite_sql_ast_node *single_table_update_target_name_node(
    const struct mylite_sql_ast_node *target
) {
    if (target != NULL && target->kind == MYLITE_SQL_AST_FROM_TABLE) {
        return child_at(target, 0U);
    }
    return target;
}

static int validate_update_index_hints(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *target,
    const struct mylite_catalog_table_descriptor *table
) {
    if (target == NULL || target->kind != MYLITE_SQL_AST_FROM_TABLE) {
        return MYLITE_OK;
    }
    if (from_table_alias_node(target) != NULL) {
        set_unsupported_error(database, "UPDATE does not support aliases");
        return MYLITE_ERROR;
    }
    return validate_select_index_hints(database, target, table);
}

static int plan_joined_update(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_update *out_plan
) {
    const struct mylite_sql_ast_node *from_clause = child_at(statement, 0U);
    const struct mylite_sql_ast_node *assignment_list = child_at(statement, 1U);
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *order_clause = NULL;
    const struct mylite_sql_ast_node *limit_clause = NULL;
    const struct mylite_sql_ast_node *optional_clause = NULL;
    const struct mylite_sql_ast_node *join_condition = NULL;
    struct select_source_context source_context = {0};
    int rc = MYLITE_OK;

    *out_plan = (struct planned_update){.is_joined = true};
    if (from_clause == NULL || from_clause->kind != MYLITE_SQL_AST_FROM_JOIN) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    optional_clause = child_at(statement, 2U);
    while (optional_clause != NULL) {
        if (optional_clause->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
            where_clause = optional_clause;
        } else if (optional_clause->kind == MYLITE_SQL_AST_ORDER_BY_CLAUSE) {
            order_clause = optional_clause;
        } else if (optional_clause->kind == MYLITE_SQL_AST_LIMIT_CLAUSE) {
            limit_clause = optional_clause;
        } else {
            set_unsupported_error(database, "joined UPDATE supports only SET and WHERE");
            return MYLITE_ERROR;
        }
        optional_clause = optional_clause->next_sibling;
    }
    if (order_clause != NULL) {
        set_joined_update_wrong_usage_error(database, "ORDER BY");
        return MYLITE_ERROR;
    }
    if (limit_clause != NULL) {
        set_joined_update_wrong_usage_error(database, "LIMIT");
        return MYLITE_ERROR;
    }

    join_condition = child_at(from_clause, 2U);
    out_plan->join_kind = mylite_sql_ast_node_join_kind(from_clause);
    if (out_plan->join_kind == MYLITE_SQL_AST_JOIN_KIND_RIGHT_OUTER) {
        set_unsupported_error(database, "joined UPDATE does not support RIGHT JOIN");
        return MYLITE_ERROR;
    }
    if (out_plan->join_kind == MYLITE_SQL_AST_JOIN_KIND_LEFT_OUTER && join_condition == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    rc = reject_builtin_schema_joined_update_assignment_target(database, assignment_list);
    if (rc != MYLITE_OK) {
        return rc;
    }

    out_plan->sources = calloc(2U, sizeof(*out_plan->sources));
    if (out_plan->sources == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_plan->source_count = 2U;

    rc = plan_joined_select_source(database, child_at(from_clause, 0U), &out_plan->sources[0]);
    if (rc == MYLITE_OK) {
        rc = plan_joined_select_source(database, child_at(from_clause, 1U), &out_plan->sources[1]);
    }
    if (rc == MYLITE_OK) {
        rc = reject_duplicate_select_source_reference(
            database,
            out_plan->sources,
            out_plan->source_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = init_join_select_source_context(
            out_plan->sources,
            out_plan->source_count,
            &source_context
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_joined_select_condition(
            database,
            join_condition,
            out_plan->join_kind,
            &source_context,
            &out_plan->join_condition
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_select_predicate(
            database,
            where_clause,
            &source_context,
            NULL,
            0U,
            &out_plan->predicate
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_joined_update_assignment(database, assignment_list, &source_context, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = finish_joined_update_target_plan(database, out_plan);
    }

    if (rc != MYLITE_OK) {
        planned_update_deinit(out_plan);
    }
    return rc;
}

static int reject_builtin_schema_joined_update_assignment_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment_list
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    const struct mylite_sql_ast_node *assignment = child_at(assignment_list, 0U);
    const struct mylite_sql_ast_node *target = NULL;
    size_t part_count = 0U;
    int rc = MYLITE_OK;

    if (assignment_list == NULL || assignment_list->kind != MYLITE_SQL_AST_UPDATE_ASSIGNMENT_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_child_count(assignment_list) != 1U) {
        return MYLITE_OK;
    }
    if (assignment == NULL || assignment->kind != MYLITE_SQL_AST_UPDATE_ASSIGNMENT) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    target = child_at(assignment, 0U);
    rc = collect_identifier_parts(target, parts, table_name_part_capacity, &part_count, database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (part_count == 3U && find_builtin_schema_descriptor(parts[0]) != NULL) {
        set_builtin_schema_write_error(database, parts[0]);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int plan_joined_update_assignment(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment_list,
    const struct select_source_context *source_context,
    struct planned_update *out_plan
) {
    const struct mylite_sql_ast_node *assignment = child_at(assignment_list, 0U);
    const struct mylite_sql_ast_node *target = NULL;
    const struct mylite_sql_ast_node *value_node = NULL;
    int rc = MYLITE_OK;

    if (assignment_list == NULL || assignment_list->kind != MYLITE_SQL_AST_UPDATE_ASSIGNMENT_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_child_count(assignment_list) != 1U) {
        set_update_multiple_assignment_unsupported_error(database);
        return MYLITE_ERROR;
    }
    if (assignment == NULL || assignment->kind != MYLITE_SQL_AST_UPDATE_ASSIGNMENT) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    target = child_at(assignment, 0U);
    value_node = child_at(assignment, 1U);
    rc = resolve_descriptor_column_reference_with_source_index(
        database,
        target,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "joined UPDATE supports only descriptor assignment columns",
        NULL,
        0U,
        &out_plan->assignment_column,
        &out_plan->target_source_index
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_plan->assignment_column.is_generated) {
        const struct mylite_sql_ast_node *unwrapped_value =
            unwrap_parenthesized_expression(value_node);

        if (unwrapped_value != NULL && unwrapped_value->kind == MYLITE_SQL_AST_DML_DEFAULT_VALUE) {
            set_unsupported_error(
                database,
                "joined UPDATE does not support generated column DEFAULT assignments"
            );
            return MYLITE_ERROR;
        }
        set_generated_column_value_error(
            database,
            out_plan->assignment_column.name,
            source_context->sources[out_plan->target_source_index].table.name
        );
        return MYLITE_ERROR;
    }
    if (!update_assignment_value_is_multi_constant_supported(value_node)) {
        set_unsupported_error(database, "joined UPDATE supports only constant assignment values");
        return MYLITE_ERROR;
    }

    out_plan->assignment_value_node = value_node;
    return MYLITE_OK;
}

static int finish_joined_update_target_plan(
    struct mylite_db *database,
    struct planned_update *out_plan
) {
    struct primary_key_info primary_key = primary_key_info_init();
    struct planned_select_source *target_source = NULL;
    int rc = MYLITE_OK;

    if (out_plan->target_source_index >= out_plan->source_count) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    target_source = &out_plan->sources[out_plan->target_source_index];
    out_plan->target = target_source->source;
    out_plan->table = target_source->table;

    rc = reject_builtin_schema_joined_update_target(database, out_plan);
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
            out_plan->primary_key_column_id = primary_key.parts[0].index_column.column_id;
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
        rc = validate_update_default_function_sources(
            database,
            out_plan,
            out_plan->columns,
            out_plan->column_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = choose_sqlite_rowid_alias(
            database,
            out_plan->columns,
            out_plan->column_count,
            "joined UPDATE requires an unshadowed SQLite rowid alias",
            &out_plan->rowid_alias
        );
    }

    primary_key_info_deinit(&primary_key);
    return rc;
}

static int reject_builtin_schema_joined_update_target(
    struct mylite_db *database,
    const struct planned_update *plan
) {
    if (plan == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (find_builtin_schema_descriptor(plan->target.schema.name) != NULL) {
        set_builtin_schema_write_error(database, plan->target.schema.name);
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static void set_joined_update_wrong_usage_error(
    struct mylite_db *database,
    const char *clause_name
) {
    set_wrong_usage_error(database, "UPDATE", clause_name);
}

static void planned_update_deinit(struct planned_update *plan) {
    if (plan == NULL) {
        return;
    }

    for (size_t source_index = 0U; source_index < plan->source_count; ++source_index) {
        free(plan->sources[source_index].columns);
    }
    free(plan->sources);
    planned_select_predicate_deinit(&plan->predicate);
    planned_select_deinit(&plan->assignment_subquery);
    planned_value_deinit(&plan->assignment_value);
    planned_value_deinit(&plan->auto_update_value);
    if (plan->assignments != NULL) {
        for (size_t index = 0U; index < plan->assignment_count; ++index) {
            planned_value_deinit(&plan->assignments[index].value);
        }
    }
    free(plan->assignments);
    free(plan->columns);
    planned_select_order_deinit(&plan->order);
    loaded_index_infos_deinit(&plan->indexes, &plan->index_count);
    *plan = (struct planned_update){0};
}

static bool planned_update_has_multiple_assignments(const struct planned_update *plan) {
    if (plan == NULL) {
        return false;
    }
    return plan->assignment_count > 1U;
}

static bool planned_update_assignment_is_noop(const struct planned_update_assignment *assignment) {
    return (assignment != NULL && assignment->generated_default_noop) != 0;
}

static size_t planned_update_executable_assignment_count(const struct planned_update *plan) {
    size_t count = 0U;

    if (plan == NULL || plan->assignments == NULL) {
        return 0U;
    }
    for (size_t index = 0U; index < plan->assignment_count; ++index) {
        if (!planned_update_assignment_is_noop(&plan->assignments[index])) {
            ++count;
        }
    }

    return count;
}

static int copy_update_assignments_for_execution(
    struct mylite_db *database,
    const struct planned_update *plan,
    struct planned_update *executable_plan
) {
    if (!planned_update_has_multiple_assignments(plan)) {
        return MYLITE_OK;
    }

    executable_plan->assignments = calloc(plan->assignment_count, sizeof(*plan->assignments));
    if (executable_plan->assignments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    for (size_t index = 0U; index < plan->assignment_count; ++index) {
        executable_plan->assignments[index].column = plan->assignments[index].column;
        executable_plan->assignments[index].value_node = plan->assignments[index].value_node;
        executable_plan->assignments[index].generated_default_noop =
            plan->assignments[index].generated_default_noop;
    }

    return MYLITE_OK;
}

static void executable_update_deinit(struct planned_update *plan) {
    if (plan == NULL) {
        return;
    }

    planned_value_deinit(&plan->assignment_value);
    planned_value_deinit(&plan->auto_update_value);
    if (planned_update_has_multiple_assignments(plan) && plan->assignments != NULL) {
        for (size_t index = 0U; index < plan->assignment_count; ++index) {
            planned_value_deinit(&plan->assignments[index].value);
        }
        free(plan->assignments);
        plan->assignments = NULL;
    }
}

static int execute_update_from_plan(
    struct mylite_db *database,
    const struct planned_update *plan,
    mylite_result *result
) {
    struct planned_update executable_plan = *plan;
    struct mylite_statement_transaction transaction = {0};
    bool matches_any_row = false;
    uint64_t matched_row_count = 0U;
    int64_t affected_rows = 0;
    int rc = MYLITE_OK;

    if (plan->generated_default_noop) {
        mylite_result_set_affected_rows(result, 0);
        return MYLITE_OK;
    }

    rc = copy_update_assignments_for_execution(database, plan, &executable_plan);

    if (rc == MYLITE_OK) {
        rc = begin_statement_transaction(database, &transaction);
    }
    if (rc == MYLITE_OK) {
        rc = read_limited_update_matched_row_state(
            database,
            plan,
            &matched_row_count,
            &matches_any_row
        );
    }
    if (rc == MYLITE_OK) {
        rc = prepare_executable_update_plan(database, plan, matches_any_row, &executable_plan);
    }
    if (rc == MYLITE_OK) {
        rc = validate_update_date_interval_matched_assignment(database, plan, matches_any_row);
    }
    if (rc == MYLITE_OK && matches_any_row) {
        rc = append_remaining_nonstrict_update_adjustment_warnings(
            database,
            plan,
            matched_row_count
        );
    }
    if (rc == MYLITE_OK && matches_any_row) {
        rc = apply_parent_update_actions(database, &executable_plan);
    }
    if (rc == MYLITE_OK && matches_any_row) {
        rc = execute_matching_update_statement(database, &executable_plan, &affected_rows, plan);
    }
    if (rc == MYLITE_OK && matches_any_row) {
        rc = validate_parent_update_action_child_foreign_keys(database, &executable_plan);
    }
    if (rc == MYLITE_OK) {
        rc = advance_auto_increment_after_update(
            database,
            plan,
            &executable_plan.assignment_value,
            affected_rows
        );
    }
    if (rc == MYLITE_OK) {
        rc = touch_persistent_table_updated_time(database, &plan->table, affected_rows, false);
    }
    if (rc == MYLITE_OK) {
        rc = validate_child_foreign_keys_after_write(database, plan->table.table_id);
    }
    if (rc == MYLITE_OK) {
        rc = validate_parent_foreign_keys_after_write(database, plan->table.table_id);
    }
    if (rc == MYLITE_OK) {
        rc = commit_statement_transaction(database, &transaction);
    }
    if (rc != MYLITE_OK) {
        rollback_statement_transaction(database, &transaction);
    }
    executable_update_deinit(&executable_plan);

    if (rc != MYLITE_OK) {
        return report_update_execution_error(database, rc);
    }

    mylite_result_set_affected_rows(result, affected_rows);

    return MYLITE_OK;
}

static int read_limited_update_matched_row_state(
    struct mylite_db *database,
    const struct planned_update *plan,
    uint64_t *out_row_count,
    bool *out_matches_any_row
) {
    uint64_t matched_row_count = 0U;
    int rc = MYLITE_OK;

    if (out_row_count == NULL || out_matches_any_row == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    *out_row_count = 0U;
    *out_matches_any_row = false;

    rc = read_update_matched_row_count(database, plan, &matched_row_count);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (plan->limit.has_limit && plan->limit.row_count >= 0) {
        uint64_t limit_row_count = (uint64_t)plan->limit.row_count;

        if (matched_row_count > limit_row_count) {
            matched_row_count = limit_row_count;
        }
    }
    *out_row_count = matched_row_count;
    *out_matches_any_row = matched_row_count > 0U;
    return MYLITE_OK;
}

static int validate_update_date_interval_matched_assignment(
    struct mylite_db *database,
    const struct planned_update *plan,
    bool matches_any_row
) {
    if (plan == NULL) {
        return MYLITE_MISUSE;
    }
    if (!matches_any_row ||
        plan->assignment_value_kind != PLANNED_UPDATE_ASSIGNMENT_DATE_INTERVAL) {
        return MYLITE_OK;
    }
    if (plan->date_interval_is_null && !plan->assignment_column.is_nullable) {
        set_bad_null_error(database, plan->assignment_column.name);
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int read_update_matched_row_count(
    struct mylite_db *database,
    const struct planned_update *plan,
    uint64_t *out_row_count
) {
    bool matches_any_row = false;

    if (out_row_count == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    *out_row_count = 0U;
    if (plan == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (plan->limit.has_limit && plan->limit.row_count == 0) {
        return MYLITE_OK;
    }
    if (plan->is_joined) {
        int rc = update_matches_any_row(database, plan, &matches_any_row);

        if (rc == MYLITE_OK && matches_any_row) {
            *out_row_count = 1U;
        }
        return rc;
    }

    return read_single_table_update_matched_row_count(database, plan, out_row_count);
}

static int read_single_table_update_matched_row_count(
    struct mylite_db *database,
    const struct planned_update *plan,
    uint64_t *out_row_count
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = build_single_table_update_matched_count_sql(plan, &sql);

    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_update_matched_count_parameters(statement, plan);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            sqlite3_int64 row_count = sqlite3_column_int64(statement, 0);

            if (row_count < 0) {
                rc = MYLITE_ERROR;
            } else {
                *out_row_count = (uint64_t)row_count;
            }
        } else {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);

    return rc;
}

static int build_single_table_update_matched_count_sql(
    const struct planned_update *plan,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    size_t next_parameter = 1U;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT COUNT(*) FROM ");
    if (rc == MYLITE_OK) {
        rc = append_single_update_matched_from_sql(&string, plan);
    }
    if (rc == MYLITE_OK && planned_update_has_row_filter(plan)) {
        rc = append_update_row_filter_sql(&string, plan, &next_parameter);
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

static int append_remaining_nonstrict_update_adjustment_warnings(
    struct mylite_db *database,
    const struct planned_update *plan,
    uint64_t matched_row_count
) {
    int rc = MYLITE_OK;

    if (matched_row_count <= 1U) {
        return MYLITE_OK;
    }
    if (!planned_update_has_multiple_assignments(plan) &&
        plan->assignment_value_kind == PLANNED_UPDATE_ASSIGNMENT_DATE_INTERVAL) {
        return MYLITE_OK;
    }
    for (uint64_t row_index = 1U; rc == MYLITE_OK && row_index < matched_row_count; ++row_index) {
        if (planned_update_has_multiple_assignments(plan)) {
            for (size_t index = 0U; rc == MYLITE_OK && index < plan->assignment_count; ++index) {
                if (planned_update_assignment_is_noop(&plan->assignments[index])) {
                    continue;
                }
                rc = append_update_assignment_adjustment_warning(
                    database,
                    plan,
                    plan->assignments[index].value_node,
                    &plan->assignments[index].column,
                    plan->ignore_errors,
                    (size_t)row_index + 1U
                );
            }
        } else {
            rc = append_update_assignment_adjustment_warning(
                database,
                plan,
                plan->assignment_value_node,
                &plan->assignment_column,
                plan->ignore_errors,
                (size_t)row_index + 1U
            );
        }
    }

    return rc;
}

static int append_update_assignment_adjustment_warning(
    struct mylite_db *database,
    const struct planned_update *plan,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    bool ignore_errors,
    size_t row_number
) {
    bool is_constant_arithmetic = false;
    int rc = MYLITE_OK;

    value_node = unwrap_parenthesized_expression(value_node);
    if (value_node == NULL || column == NULL) {
        return MYLITE_OK;
    }
    if (value_node->kind == MYLITE_SQL_AST_SCALAR_SUBQUERY) {
        return append_update_scalar_subquery_adjustment_warning(database, plan);
    }
    if (value_node->kind == MYLITE_SQL_AST_DEFAULT_FUNCTION) {
        return append_update_default_function_adjustment_warning(
            database,
            plan,
            value_node,
            column,
            ignore_errors
        );
    }
    if (value_node->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_STRING) {
        if (column != NULL && strcmp(column->physical_type, "INTEGER") == 0) {
            return append_update_integer_string_adjustment_diagnostic(
                database,
                value_node,
                column,
                ignore_errors,
                row_number
            );
        }
        if (column_descriptor_is_decimal(column)) {
            return append_update_decimal_string_adjustment_diagnostic(
                database,
                value_node,
                column,
                ignore_errors,
                row_number
            );
        }
        if (column_descriptor_is_approximate(column)) {
            return append_update_approximate_string_adjustment_diagnostic(
                database,
                value_node,
                column,
                ignore_errors,
                row_number
            );
        }
        if (column_descriptor_is_string_family(column)) {
            return append_update_string_truncation_diagnostic(
                database,
                value_node,
                column,
                ignore_errors,
                row_number
            );
        }
    }
    if (session_sql_mode_is_strict(database) && !ignore_errors) {
        return MYLITE_OK;
    }
    rc = update_value_is_constant_arithmetic_expression(
        database,
        value_node,
        &is_constant_arithmetic
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (is_constant_arithmetic) {
        return append_update_constant_arithmetic_adjustment_warning(database, value_node, column);
    }
    if (value_node->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_NULL) {
        if (!column->is_nullable) {
            return append_bad_null_warning(database, column->name);
        }
        return MYLITE_OK;
    }
    if (value_node->kind != MYLITE_SQL_AST_DML_DEFAULT_VALUE) {
        struct planned_value value = {.is_null = false};

        rc = convert_update_value_for_column(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            &value
        );
        planned_value_deinit(&value);
        return rc;
    }

    return append_update_default_assignment_adjustment_warning(database, column);
}

static int append_update_scalar_subquery_adjustment_warning(
    struct mylite_db *database,
    const struct planned_update *plan
) {
    struct planned_value value = {.is_null = false};
    int rc = MYLITE_OK;

    if (plan == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    rc = materialize_update_scalar_subquery_value(database, plan, &value);
    planned_value_deinit(&value);
    return rc;
}

static int append_update_default_function_adjustment_warning(
    struct mylite_db *database,
    const struct planned_update *plan,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    bool ignore_errors
) {
    struct select_source_context source_context = {0};
    struct planned_value value = {.is_null = false};
    int rc = MYLITE_OK;

    if (plan == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    source_context.source = &plan->target;
    rc = convert_default_function_value_for_target(
        database,
        value_node,
        &source_context,
        plan->columns,
        plan->column_count,
        column,
        ignore_errors,
        &value
    );
    planned_value_deinit(&value);
    return rc;
}

static int append_update_default_assignment_adjustment_warning(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
) {
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_NULL_EXPRESSION) {
        if (!column->is_nullable) {
            return append_bad_null_warning(database, column->name);
        }
        return MYLITE_OK;
    }
    if (column_descriptor_uses_enum_implicit_missing_default(column)) {
        return MYLITE_OK;
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_NO_EXPLICIT) {
        return append_no_default_warning(database, column->name);
    }
    if (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_NONE && !column->is_nullable) {
        return append_no_default_warning(database, column->name);
    }

    return MYLITE_OK;
}

static int append_update_constant_arithmetic_adjustment_warning(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column
) {
    struct scalar_arithmetic_value value = {.is_null = false, .integer = 0};
    int rc = evaluate_scalar_arithmetic_expression(database, value_node, &value);

    if (rc != MYLITE_OK || !value.is_null || column->is_nullable) {
        return rc;
    }

    return append_bad_null_warning(database, column->name);
}

static int append_update_integer_string_adjustment_diagnostic(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    bool ignore_errors,
    size_t row_number
) {
    char *text = NULL;
    size_t text_length = 0U;
    bool is_negative = false;
    bool exceeds_range = false;
    uint64_t magnitude = 0U;
    enum dml_numeric_string_parse_result parse_result = DML_NUMERIC_STRING_PARSE_INVALID;
    int rc = MYLITE_OK;

    if (session_sql_mode_is_strict(database) && !ignore_errors) {
        return MYLITE_OK;
    }

    rc = decode_sql_string_literal(
        database,
        value_node,
        "Integer values support only integer, boolean, NULL, DEFAULT, and exact quoted strings",
        "invalid integer string literal",
        &text,
        &text_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    parse_result = parse_dml_integer_string_text(text, text_length, &magnitude, &is_negative);
    if (parse_result == DML_NUMERIC_STRING_PARSE_INVALID) {
        rc = append_incorrect_integer_value_warning(database, text, column->name, row_number);
    } else if (parse_result == DML_NUMERIC_STRING_PARSE_OVERFLOW) {
        rc = append_out_of_range_warning(database, column->name, row_number);
    } else {
        rc = dml_integer_value_exceeds_column_range(
            database,
            magnitude,
            is_negative,
            column,
            &exceeds_range
        );
        if (rc == MYLITE_OK && exceeds_range) {
            rc = append_out_of_range_warning(database, column->name, row_number);
        } else if (rc == MYLITE_OK && parse_result == DML_NUMERIC_STRING_PARSE_TRUNCATED) {
            rc = append_data_truncated_warning(database, column->name, row_number);
        }
    }

    free(text);
    return rc;
}

static int append_update_string_truncation_diagnostic(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    bool ignore_errors,
    size_t row_number
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (!column_descriptor_is_string_family(column)) {
        return MYLITE_OK;
    }

    rc = decode_sql_string_literal(
        database,
        value_node,
        "String values support only string literals",
        "String values do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc == MYLITE_OK) {
        struct char_text_conversion conversion = {
            .text = text,
            .text_length = text_length,
            .row_number = row_number,
        };
        bool allow_nonspace_truncation =
            dml_allows_string_truncation_adjustment(database, ignore_errors);

        if (column_descriptor_is_char(column)) {
            rc = convert_char_text(database, column, &conversion, allow_nonspace_truncation);
        } else if (column_descriptor_is_varchar(column)) {
            rc = convert_varchar_text(database, column, &conversion, allow_nonspace_truncation);
        } else {
            rc = convert_text_family_text(
                database,
                column,
                &conversion,
                allow_nonspace_truncation,
                false
            );
        }
    }

    free(text);
    return rc;
}

static int validate_parent_update_action_child_foreign_keys(
    struct mylite_db *database,
    const struct planned_update *plan
) {
    struct loaded_foreign_key_info *foreign_keys = NULL;
    size_t foreign_key_count = 0U;
    int rc = MYLITE_OK;

    if (!foreign_key_checks_system_variable_value(database, false)) {
        return MYLITE_OK;
    }
    rc = load_parent_foreign_key_infos(
        database,
        plan->table.table_id,
        &foreign_keys,
        &foreign_key_count
    );
    for (size_t index = 0U; rc == MYLITE_OK && index < foreign_key_count; ++index) {
        const struct loaded_foreign_key_info *foreign_key = &foreign_keys[index];
        size_t updated_part_index = 0U;

        if ((!foreign_key_rule_is_set_null(foreign_key->foreign_key.update_rule) &&
             !foreign_key_rule_is_cascade(foreign_key->foreign_key.update_rule)) ||
            !planned_update_assigns_foreign_key_parent_part(
                plan,
                foreign_key,
                &updated_part_index
            )) {
            continue;
        }
        rc = validate_child_foreign_keys_after_write(database, foreign_key->child_table.table_id);
    }

    loaded_foreign_key_infos_deinit(&foreign_keys, &foreign_key_count);
    return rc;
}

static int apply_parent_update_actions(
    struct mylite_db *database,
    const struct planned_update *plan
) {
    struct loaded_foreign_key_info *foreign_keys = NULL;
    size_t foreign_key_count = 0U;
    int rc = MYLITE_OK;

    if (!foreign_key_checks_system_variable_value(database, false)) {
        return MYLITE_OK;
    }
    rc = load_parent_foreign_key_infos(
        database,
        plan->table.table_id,
        &foreign_keys,
        &foreign_key_count
    );
    for (size_t index = 0U; rc == MYLITE_OK && index < foreign_key_count; ++index) {
        const struct loaded_foreign_key_info *foreign_key = &foreign_keys[index];
        size_t updated_part_index = 0U;

        if (!planned_update_assigns_foreign_key_parent_part(
                plan,
                foreign_key,
                &updated_part_index
            )) {
            continue;
        }
        if (!foreign_key_rule_is_cascade(foreign_key->foreign_key.update_rule) &&
            !foreign_key_rule_is_set_null(foreign_key->foreign_key.update_rule)) {
            continue;
        }
        rc = reject_unsupported_recursive_foreign_key_action(database, foreign_key);
        if (rc == MYLITE_OK) {
            if (foreign_key_rule_is_set_null(foreign_key->foreign_key.update_rule)) {
                rc = execute_parent_update_set_null(database, plan, foreign_key);
            } else {
                rc = execute_parent_update_cascade(database, plan, foreign_key, updated_part_index);
            }
        }
    }

    loaded_foreign_key_infos_deinit(&foreign_keys, &foreign_key_count);
    return rc;
}

static int execute_parent_update_cascade(
    struct mylite_db *database,
    const struct planned_update *plan,
    const struct loaded_foreign_key_info *foreign_key,
    size_t updated_part_index
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int64_t affected_rows = 0;
    int sqlite_rc = SQLITE_OK;
    int rc = build_parent_update_cascade_sql(plan, foreign_key, updated_part_index, &sql);

    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_parent_update_cascade_parameters(statement, plan);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            affected_rows = (int64_t)sqlite3_changes64(database->sqlite);
        } else if (sqlite_status_is_constraint(sqlite_rc)) {
            rc = handle_parent_update_cascade_constraint(
                database,
                plan,
                foreign_key,
                updated_part_index
            );
        } else {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finalize_sqlite_statement(statement, rc);
    if (rc == MYLITE_OK) {
        rc = touch_persistent_table_updated_time(
            database,
            &foreign_key->child_table,
            affected_rows,
            false
        );
    }
    free(sql);
    return rc;
}

static int build_parent_update_cascade_sql(
    const struct planned_update *plan,
    const struct loaded_foreign_key_info *foreign_key,
    size_t updated_part_index,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    bool has_parent_condition = false;
    size_t next_parameter = 2U;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "UPDATE ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(
            &string,
            foreign_key->child_table.physical_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " AS ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, "c");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " SET ");
    }
    if (rc == MYLITE_OK) {
        rc = append_parent_update_cascade_assignment_sql(
            &string,
            plan,
            foreign_key,
            updated_part_index
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " WHERE ");
    }
    if (rc == MYLITE_OK) {
        rc = append_foreign_key_child_not_null_checks(&string, foreign_key);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " AND EXISTS (SELECT 1 FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(
            &string,
            foreign_key->parent_table.physical_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " AS ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, "p");
    }
    if (rc == MYLITE_OK) {
        rc = append_parent_update_target_sql(&string, plan, &next_parameter, &has_parent_condition);
    }
    if (rc == MYLITE_OK) {
        rc = append_parent_child_match_sql(&string, has_parent_condition, foreign_key);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(&string, ')');
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

static int execute_parent_update_set_null(
    struct mylite_db *database,
    const struct planned_update *plan,
    const struct loaded_foreign_key_info *foreign_key
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int64_t affected_rows = 0;
    int sqlite_rc = SQLITE_OK;
    int rc = build_parent_update_set_null_sql(plan, foreign_key, &sql);

    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_parent_update_set_null_parameters(statement, plan);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            affected_rows = (int64_t)sqlite3_changes64(database->sqlite);
        } else if (sqlite_status_is_constraint(sqlite_rc)) {
            rc = handle_foreign_key_action_set_null_constraint(
                database,
                foreign_key->child_table.table_id
            );
        } else {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finalize_sqlite_statement(statement, rc);
    if (rc == MYLITE_OK) {
        rc = touch_persistent_table_updated_time(
            database,
            &foreign_key->child_table,
            affected_rows,
            false
        );
    }
    free(sql);
    return rc;
}

static int build_parent_update_set_null_sql(
    const struct planned_update *plan,
    const struct loaded_foreign_key_info *foreign_key,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    bool has_parent_condition = false;
    size_t next_parameter = 1U;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "UPDATE ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(
            &string,
            foreign_key->child_table.physical_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " AS ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, "c");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " SET ");
    }
    if (rc == MYLITE_OK) {
        rc = append_foreign_key_child_set_null_assignments_sql(&string, foreign_key);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " WHERE ");
    }
    if (rc == MYLITE_OK) {
        rc = append_foreign_key_child_not_null_checks(&string, foreign_key);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " AND EXISTS (SELECT 1 FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(
            &string,
            foreign_key->parent_table.physical_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " AS ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, "p");
    }
    if (rc == MYLITE_OK) {
        rc = append_parent_update_target_sql(&string, plan, &next_parameter, &has_parent_condition);
    }
    if (rc == MYLITE_OK) {
        rc = append_parent_child_match_sql(&string, has_parent_condition, foreign_key);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(&string, ')');
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

static int append_parent_update_cascade_assignment_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    const struct loaded_foreign_key_info *foreign_key,
    size_t updated_part_index
) {
    const struct loaded_foreign_key_part *part = &foreign_key->parts[updated_part_index];
    int rc = mylite_dynamic_string_append_quoted_identifier(string, part->child_column.name);

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " = ");
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (plan->assignment_value_kind != PLANNED_UPDATE_ASSIGNMENT_SAME_COLUMN_ARITHMETIC) {
        return mylite_dynamic_string_append(string, "?1");
    }

    rc = mylite_dynamic_string_append_quoted_identifier(string, part->child_column.name);
    if (rc == MYLITE_OK && plan->arithmetic_operator == MYLITE_SQL_AST_OPERATOR_ADD) {
        rc = mylite_dynamic_string_append(string, " + ?1");
    } else if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " - ?1");
    }
    return rc;
}

static int append_parent_update_target_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter,
    bool *out_has_condition
) {
    int rc = MYLITE_OK;

    *out_has_condition = true;
    if (plan->is_joined) {
        rc = append_joined_parent_update_target_sql(string, plan, next_parameter);
        return rc;
    }
    if (plan->limit.has_limit) {
        rc = append_update_rowid_limited_sql(string, plan, next_parameter);
    } else if (planned_select_predicate_has_expression(&plan->predicate)) {
        rc = append_select_predicate_sql(string, &plan->predicate, next_parameter);
    } else {
        rc = mylite_dynamic_string_append(string, " WHERE ");
    }
    if (rc == MYLITE_OK &&
        (plan->limit.has_limit || planned_select_predicate_has_expression(&plan->predicate))) {
        rc = mylite_dynamic_string_append(string, " AND ");
    }
    if (rc == MYLITE_OK) {
        rc = append_update_changed_condition_sql(string, plan, next_parameter);
    }
    return rc;
}

static int append_joined_parent_update_target_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
) {
    int rc = mylite_dynamic_string_append(string, " WHERE ");

    if (rc == MYLITE_OK) {
        rc = append_foreign_key_column_reference(string, "p", plan->rowid_alias);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " IN (");
    }
    if (rc == MYLITE_OK) {
        rc = append_joined_update_rowid_subquery_sql(string, plan, next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ") AND ");
    }
    if (rc == MYLITE_OK) {
        rc = append_update_changed_condition_sql(string, plan, next_parameter);
    }

    return rc;
}

static int bind_parent_update_cascade_parameters(
    sqlite3_stmt *statement,
    const struct planned_update *plan
) {
    int parameter_index = 1;
    int rc = bind_update_assignment_parameter(statement, parameter_index, plan);

    if (rc == MYLITE_OK) {
        ++parameter_index;
        rc = bind_select_predicate_parameters(statement, &plan->predicate, &parameter_index);
    }
    if (rc == MYLITE_OK && plan->limit.has_limit) {
        rc = bind_int64_parameter(statement, parameter_index, plan->limit.row_count);
        if (rc == MYLITE_OK) {
            ++parameter_index;
        }
    }
    if (rc == MYLITE_OK &&
        (plan->assignment_value_kind == PLANNED_UPDATE_ASSIGNMENT_SAME_COLUMN_ARITHMETIC ||
         !plan->assignment_value.is_null)) {
        rc = bind_update_changed_condition_parameter(statement, parameter_index, plan);
    }
    return rc;
}

static int bind_parent_update_set_null_parameters(
    sqlite3_stmt *statement,
    const struct planned_update *plan
) {
    int parameter_index = 1;
    int rc = bind_select_predicate_parameters(statement, &plan->predicate, &parameter_index);

    if (rc == MYLITE_OK && plan->limit.has_limit) {
        rc = bind_int64_parameter(statement, parameter_index, plan->limit.row_count);
        if (rc == MYLITE_OK) {
            ++parameter_index;
        }
    }
    if (rc == MYLITE_OK &&
        (plan->assignment_value_kind == PLANNED_UPDATE_ASSIGNMENT_SAME_COLUMN_ARITHMETIC ||
         !plan->assignment_value.is_null)) {
        rc = bind_update_changed_condition_parameter(statement, parameter_index, plan);
    }
    return rc;
}

static int handle_foreign_key_action_set_null_constraint(
    struct mylite_db *database,
    int64_t child_table_id
) {
    bool was_check_violation = false;
    int rc =
        handle_check_constraint_violation(database, child_table_id, false, &was_check_violation);

    if (rc != MYLITE_OK || was_check_violation) {
        return rc;
    }
    return mylite_sqlite_status_to_mylite(SQLITE_CONSTRAINT);
}

static int handle_parent_update_cascade_constraint(
    struct mylite_db *database,
    const struct planned_update *plan,
    const struct loaded_foreign_key_info *foreign_key,
    size_t updated_part_index
) {
    bool was_check_violation = false;
    int rc = handle_check_constraint_violation(
        database,
        foreign_key->child_table.table_id,
        false,
        &was_check_violation
    );

    if (rc != MYLITE_OK || was_check_violation) {
        return rc;
    }
    return handle_parent_update_cascade_unique_conflict(
        database,
        plan,
        foreign_key,
        updated_part_index
    );
}

static int handle_parent_update_cascade_unique_conflict(
    struct mylite_db *database,
    const struct planned_update *plan,
    const struct loaded_foreign_key_info *foreign_key,
    size_t updated_part_index
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_index_info *indexes = NULL;
    const struct loaded_index_info *conflicting_index = NULL;
    const int64_t updated_child_column_id =
        foreign_key->parts[updated_part_index].child_column.column_id;
    char record_value[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY] = "";
    size_t column_count = 0U;
    size_t index_count = 0U;
    int rc =
        load_table_columns(database, foreign_key->child_table.table_id, &columns, &column_count);

    if (rc == MYLITE_OK) {
        rc = load_table_index_infos(
            database,
            foreign_key->child_table.table_id,
            columns,
            column_count,
            &indexes,
            &index_count
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < index_count; ++index) {
        if (indexes[index].index.is_unique &&
            loaded_index_contains_column_id(&indexes[index], updated_child_column_id)) {
            conflicting_index = &indexes[index];
            break;
        }
    }
    if (rc == MYLITE_OK && conflicting_index == NULL) {
        rc = mylite_sqlite_status_to_mylite(SQLITE_CONSTRAINT);
    }
    if (rc == MYLITE_OK) {
        rc = format_parent_update_cascade_record_value(plan, record_value, sizeof(record_value));
    }
    if (rc == MYLITE_OK) {
        set_foreign_key_cascade_duplicate_error(
            database,
            foreign_key->parent_table.name,
            record_value,
            foreign_key->child_table.name,
            conflicting_index->index.name
        );
        rc = MYLITE_ERROR;
    }

    loaded_index_infos_deinit(&indexes, &index_count);
    free(columns);
    return rc;
}

static int format_parent_update_cascade_record_value(
    const struct planned_update *plan,
    char *value,
    size_t value_size
) {
    int written = 0;

    if (plan->assignment_value_kind == PLANNED_UPDATE_ASSIGNMENT_SAME_COLUMN_ARITHMETIC) {
        written = snprintf(value, value_size, "%s", "<computed>");
        if (written < 0 || (size_t)written >= value_size) {
            return MYLITE_ERROR;
        }
        return MYLITE_OK;
    }
    return format_key_value(&plan->assignment_value, value, value_size);
}

static bool planned_update_assigns_foreign_key_parent_part(
    const struct planned_update *plan,
    const struct loaded_foreign_key_info *foreign_key,
    size_t *out_part_index
) {
    if (planned_update_has_multiple_assignments(plan)) {
        return false;
    }
    for (size_t part_index = 0U; part_index < foreign_key->part_count; ++part_index) {
        if (foreign_key->parts[part_index].parent_column.column_id ==
            plan->assignment_column.column_id) {
            *out_part_index = part_index;
            return true;
        }
    }
    return false;
}

static int reject_unsupported_recursive_foreign_key_action(
    struct mylite_db *database,
    const struct loaded_foreign_key_info *foreign_key
) {
    struct loaded_foreign_key_info *child_parent_foreign_keys = NULL;
    size_t child_parent_foreign_key_count = 0U;
    int rc = MYLITE_OK;

    if (foreign_key->child_table.table_id == foreign_key->parent_table.table_id) {
        set_unsupported_error(database, "recursive foreign-key action is not supported");
        return MYLITE_ERROR;
    }

    rc = load_parent_foreign_key_infos(
        database,
        foreign_key->child_table.table_id,
        &child_parent_foreign_keys,
        &child_parent_foreign_key_count
    );
    if (rc == MYLITE_OK && child_parent_foreign_key_count != 0U) {
        set_unsupported_error(database, "recursive foreign-key action is not supported");
        rc = MYLITE_ERROR;
    }
    loaded_foreign_key_infos_deinit(&child_parent_foreign_keys, &child_parent_foreign_key_count);
    return rc;
}

static int append_update_decimal_string_adjustment_diagnostic(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    bool ignore_errors,
    size_t row_number
) {
    struct decimal_type_info info = {0};
    struct planned_value value = {
        .is_null = false,
        .is_text = false,
        .is_blob = false,
        .is_real = false,
        .integer = 0,
        .real = 0.0,
        .text = NULL,
        .text_length = 0U,
    };
    int rc = MYLITE_OK;

    if (session_sql_mode_is_strict(database) && !ignore_errors) {
        return MYLITE_OK;
    }
    rc = decimal_type_info_for_logical_type(column->logical_type, &info);
    if (rc != MYLITE_OK) {
        set_unsupported_error(database, "DECIMAL values require baseline decimal descriptors");
        return MYLITE_ERROR;
    }
    rc = convert_decimal_string_literal(
        database,
        value_node,
        column,
        row_number,
        ignore_errors,
        &info,
        &value
    );
    planned_value_deinit(&value);
    return rc;
}

static int append_update_approximate_string_adjustment_diagnostic(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    bool ignore_errors,
    size_t row_number
) {
    struct approximate_type_info info = {
        .type_class = APPROXIMATE_TYPE_DOUBLE,
        .is_unsigned = false,
    };
    struct planned_value value = {
        .is_null = false,
        .is_text = false,
        .is_blob = false,
        .is_real = false,
        .integer = 0,
        .real = 0.0,
        .text = NULL,
        .text_length = 0U,
    };
    int rc = MYLITE_OK;

    if (session_sql_mode_is_strict(database) && !ignore_errors) {
        return MYLITE_OK;
    }
    rc = approximate_type_info_for_logical_type(column->logical_type, &info);
    if (rc != MYLITE_OK) {
        set_unsupported_error(database, "FLOAT and DOUBLE values require baseline descriptors");
        return MYLITE_ERROR;
    }
    rc = convert_approximate_string_literal(
        database,
        value_node,
        column,
        row_number,
        ignore_errors,
        &info,
        &value
    );
    planned_value_deinit(&value);
    return rc;
}

static int prepare_executable_update_plan(
    struct mylite_db *database,
    const struct planned_update *plan,
    bool matches_any_row,
    struct planned_update *executable_plan
) {
    int rc = MYLITE_OK;

    if (!matches_any_row) {
        return MYLITE_OK;
    }

    if (planned_update_has_multiple_assignments(plan)) {
        rc = prepare_update_multiple_assignments(database, plan, executable_plan);
    } else if (plan->assignment_value_kind == PLANNED_UPDATE_ASSIGNMENT_SAME_COLUMN_ARITHMETIC) {
        rc = prepare_update_arithmetic_assignment(database, plan, executable_plan);
    } else if (plan->assignment_value_kind == PLANNED_UPDATE_ASSIGNMENT_DATE_INTERVAL) {
        rc = MYLITE_OK;
    } else {
        rc = convert_update_value(database, plan, &executable_plan->assignment_value);
    }
    if (rc == MYLITE_OK && planned_update_auto_update_column_count(plan) > 0U) {
        rc = make_current_timestamp_value(database, &executable_plan->auto_update_value);
    }
    if (rc == MYLITE_OK && !planned_update_has_multiple_assignments(plan) &&
        plan->assignment_value_kind != PLANNED_UPDATE_ASSIGNMENT_DATE_INTERVAL &&
        plan->assignment_value_kind != PLANNED_UPDATE_ASSIGNMENT_SAME_COLUMN_ARITHMETIC) {
        rc = validate_update_string_key_value(database, executable_plan);
    }
    return rc;
}

static int prepare_update_multiple_assignments(
    struct mylite_db *database,
    const struct planned_update *plan,
    struct planned_update *executable_plan
) {
    int rc = MYLITE_OK;

    if (executable_plan->assignments == NULL) {
        set_runtime_error(database, "invalid UPDATE assignment plan");
        return MYLITE_ERROR;
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < plan->assignment_count; ++index) {
        const struct mylite_sql_ast_node *value_node =
            unwrap_parenthesized_expression(plan->assignments[index].value_node);

        if (planned_update_assignment_is_noop(&plan->assignments[index])) {
            continue;
        }
        if (value_node != NULL && value_node->kind == MYLITE_SQL_AST_DEFAULT_FUNCTION) {
            struct select_source_context source_context = {.source = &plan->target};

            rc = convert_default_function_value_for_target(
                database,
                value_node,
                &source_context,
                plan->columns,
                plan->column_count,
                &plan->assignments[index].column,
                plan->ignore_errors,
                &executable_plan->assignments[index].value
            );
        } else {
            rc = convert_update_value_for_column(
                database,
                value_node,
                &plan->assignments[index].column,
                1U,
                plan->ignore_errors,
                &executable_plan->assignments[index].value
            );
        }
    }

    return rc;
}

static int prepare_update_arithmetic_assignment(
    struct mylite_db *database,
    const struct planned_update *plan,
    struct planned_update *executable_plan
) {
    const uint64_t int64_positive_max = 9223372036854775807ULL;
    uint64_t delta = 0U;
    bool has_non_null_row = false;
    const struct update_arithmetic_range_check any_non_null_check = {
        .condition = UPDATE_ARITHMETIC_RANGE_ANY_NON_NULL,
        .threshold = 0,
        .error_kind = UPDATE_ARITHMETIC_RANGE_ERROR_COLUMN,
    };
    int rc = parse_update_arithmetic_delta(database, plan->arithmetic_delta_node, &delta);

    if (rc != MYLITE_OK || delta > int64_positive_max) {
        rc = update_arithmetic_condition_matches_row(
            database,
            plan,
            &any_non_null_check,
            &has_non_null_row
        );
        if (rc == MYLITE_OK && has_non_null_row) {
            set_update_arithmetic_delta_out_of_range_error(database);
            return MYLITE_ERROR;
        }
        if (rc != MYLITE_OK) {
            return rc;
        }
        delta = 0U;
    }

    executable_plan->arithmetic_delta_magnitude = delta;
    executable_plan->arithmetic_delta = (int64_t)delta;
    if (delta == 0U) {
        return MYLITE_OK;
    }

    return validate_update_arithmetic_range(database, plan, delta);
}

static int parse_update_arithmetic_delta(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    uint64_t *out_delta
) {
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_update_arithmetic_unsupported_error(database);
        return MYLITE_ERROR;
    }

    return parse_unsigned_integer_literal(&literal->span, out_delta);
}

static int validate_update_arithmetic_range(
    struct mylite_db *database,
    const struct planned_update *plan,
    uint64_t delta
) {
    struct integer_column_range range = {0};
    struct update_arithmetic_range_check check = {
        .condition = UPDATE_ARITHMETIC_RANGE_ANY_NON_NULL,
        .threshold = 0,
        .error_kind = update_arithmetic_range_error_kind_for_plan(plan),
    };
    int rc = integer_range_for_column(
        database,
        &plan->assignment_column,
        "UPDATE arithmetic assignment supports only integer columns",
        &range
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (delta > update_arithmetic_domain_width(&range)) {
        return validate_update_arithmetic_threshold_range(database, plan, &check);
    }

    if (plan->arithmetic_operator == MYLITE_SQL_AST_OPERATOR_ADD) {
        check.condition = UPDATE_ARITHMETIC_RANGE_GREATER_THAN;
        check.threshold = update_arithmetic_addition_threshold(&range, delta);
    } else {
        check.condition = UPDATE_ARITHMETIC_RANGE_LESS_THAN;
        check.threshold = update_arithmetic_subtraction_threshold(&range, delta);
        if (update_arithmetic_column_is_unsigned(&plan->assignment_column)) {
            check.error_kind = UPDATE_ARITHMETIC_RANGE_ERROR_UNSIGNED_BIGINT;
        }
    }

    return validate_update_arithmetic_threshold_range(database, plan, &check);
}

static int validate_update_arithmetic_threshold_range(
    struct mylite_db *database,
    const struct planned_update *plan,
    const struct update_arithmetic_range_check *check
) {
    bool has_out_of_range_row = false;
    int rc = update_arithmetic_condition_matches_row(database, plan, check, &has_out_of_range_row);

    if (rc == MYLITE_OK && has_out_of_range_row) {
        set_update_arithmetic_range_error(database, plan, check->error_kind);
        return MYLITE_ERROR;
    }

    return rc;
}

static int update_arithmetic_condition_matches_row(
    struct mylite_db *database,
    const struct planned_update *plan,
    const struct update_arithmetic_range_check *check,
    bool *out_matches
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = MYLITE_OK;

    *out_matches = false;
    rc = build_update_arithmetic_condition_sql(plan, check->condition, &sql);
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_update_arithmetic_condition_parameters(statement, plan, check);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            *out_matches = true;
        } else if (sqlite_rc != SQLITE_DONE) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);

    return rc;
}

static int build_update_arithmetic_condition_sql(
    const struct planned_update *plan,
    enum update_arithmetic_range_condition condition,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    size_t next_parameter = 1U;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT 1 FROM ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = append_update_row_filter_sql(&string, plan, &next_parameter);
    }
    if (rc == MYLITE_OK && planned_update_has_row_filter(plan)) {
        rc = mylite_dynamic_string_append(&string, " AND ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->assignment_column.name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " IS NOT NULL");
    }
    if (rc == MYLITE_OK && condition != UPDATE_ARITHMETIC_RANGE_ANY_NON_NULL) {
        rc = mylite_dynamic_string_append(&string, " AND ");
    }
    if (rc == MYLITE_OK && condition != UPDATE_ARITHMETIC_RANGE_ANY_NON_NULL) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->assignment_column.name);
    }
    if (rc == MYLITE_OK && condition == UPDATE_ARITHMETIC_RANGE_GREATER_THAN) {
        rc = mylite_dynamic_string_append(&string, " > ");
    } else if (rc == MYLITE_OK && condition == UPDATE_ARITHMETIC_RANGE_LESS_THAN) {
        rc = mylite_dynamic_string_append(&string, " < ");
    }
    if (rc == MYLITE_OK && condition != UPDATE_ARITHMETIC_RANGE_ANY_NON_NULL) {
        rc = append_numbered_parameter(&string, next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " LIMIT 1");
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

static int bind_update_arithmetic_condition_parameters(
    sqlite3_stmt *statement,
    const struct planned_update *plan,
    const struct update_arithmetic_range_check *check
) {
    int parameter_index = 1;
    int rc = bind_select_predicate_parameters(statement, &plan->predicate, &parameter_index);

    if (rc == MYLITE_OK && plan->limit.has_limit) {
        rc = bind_int64_parameter(statement, parameter_index, plan->limit.row_count);
        if (rc == MYLITE_OK) {
            ++parameter_index;
        }
    }
    if (rc == MYLITE_OK && check->condition != UPDATE_ARITHMETIC_RANGE_ANY_NON_NULL) {
        rc = bind_int64_parameter(statement, parameter_index, check->threshold);
    }

    return rc;
}

static uint64_t update_arithmetic_domain_width(const struct integer_column_range *range) {
    return range->positive_max + range->negative_abs_max;
}

static int64_t update_arithmetic_addition_threshold(
    const struct integer_column_range *range,
    uint64_t delta
) {
    if (delta <= range->positive_max) {
        return (int64_t)(range->positive_max - delta);
    }

    return int64_from_negative_abs(delta - range->positive_max);
}

static int64_t update_arithmetic_subtraction_threshold(
    const struct integer_column_range *range,
    uint64_t delta
) {
    if (delta <= range->negative_abs_max) {
        return int64_from_negative_abs(range->negative_abs_max - delta);
    }

    return (int64_t)(delta - range->negative_abs_max);
}

static int64_t int64_from_negative_abs(uint64_t negative_abs) {
    const uint64_t int64_negative_abs_max = 9223372036854775808ULL;

    if (negative_abs == 0U) {
        return 0;
    }
    if (negative_abs == int64_negative_abs_max) {
        return INT64_MIN;
    }
    return -(int64_t)negative_abs;
}

static bool update_arithmetic_column_is_unsigned(
    const struct mylite_catalog_column_descriptor *column
) {
    if (column == NULL) {
        return false;
    }
    return strstr(column->logical_type, "UNSIGNED") != NULL;
}

static bool update_arithmetic_column_is_signed_bigint(
    const struct mylite_catalog_column_descriptor *column
) {
    if (column == NULL) {
        return false;
    }
    return strcmp(column->logical_type, "BIGINT") == 0;
}

static enum update_arithmetic_range_error_kind update_arithmetic_range_error_kind_for_plan(
    const struct planned_update *plan
) {
    if (plan->arithmetic_operator == MYLITE_SQL_AST_OPERATOR_SUBTRACT &&
        update_arithmetic_column_is_unsigned(&plan->assignment_column)) {
        return UPDATE_ARITHMETIC_RANGE_ERROR_UNSIGNED_BIGINT;
    }
    if (update_arithmetic_column_is_signed_bigint(&plan->assignment_column)) {
        return UPDATE_ARITHMETIC_RANGE_ERROR_SIGNED_BIGINT;
    }
    return UPDATE_ARITHMETIC_RANGE_ERROR_COLUMN;
}

static void set_update_arithmetic_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "UPDATE arithmetic assignment supports only same-column integer + or - unsigned integer "
        "literals on non-key columns"
    );
}

static void set_update_arithmetic_delta_out_of_range_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "UPDATE arithmetic assignment supports only unsigned integer deltas in signed 64-bit range"
    );
}

static void set_update_arithmetic_range_error(
    struct mylite_db *database,
    const struct planned_update *plan,
    enum update_arithmetic_range_error_kind error_kind
) {
    if (error_kind == UPDATE_ARITHMETIC_RANGE_ERROR_SIGNED_BIGINT) {
        mylite_diagnostics_set_error(
            mylite_connection_diagnostics(database),
            mysql_error_bigint_out_of_range,
            "22003",
            "BIGINT value is out of range in UPDATE arithmetic assignment"
        );
        return;
    }
    if (error_kind == UPDATE_ARITHMETIC_RANGE_ERROR_UNSIGNED_BIGINT) {
        mylite_diagnostics_set_error(
            mylite_connection_diagnostics(database),
            mysql_error_bigint_out_of_range,
            "22003",
            "BIGINT UNSIGNED value is out of range in UPDATE arithmetic assignment"
        );
        return;
    }

    set_out_of_range_error(database, plan->assignment_column.name, 1U);
}

static int execute_matching_update_statement(
    struct mylite_db *database,
    const struct planned_update *executable_plan,
    int64_t *out_affected_rows,
    const struct planned_update *plan
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int rc = build_update_sql(executable_plan, &sql);

    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_update_parameters(statement, executable_plan);
    }
    if (rc == MYLITE_OK) {
        rc = step_update_statement(database, statement, executable_plan, out_affected_rows, plan);
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);
    return rc;
}

static int report_update_execution_error(struct mylite_db *database, int rc) {
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

static int validate_update_string_key_value(
    struct mylite_db *database,
    const struct planned_update *plan
) {
    for (size_t index = 0U; index < plan->index_count; ++index) {
        const struct loaded_index_info *key = &plan->indexes[index];

        if (!key->index.is_unique ||
            !loaded_index_contains_column_id(key, plan->assignment_column.column_id)) {
            continue;
        }
        for (size_t part_index = 0U; part_index < key->part_count; ++part_index) {
            const struct loaded_index_part *part = &key->parts[part_index];

            if (part->index_column.column_id == plan->assignment_column.column_id &&
                loaded_index_part_requires_string_key_validation(part)) {
                return validate_string_key_value(database, &plan->assignment_value);
            }
        }
    }

    return MYLITE_OK;
}

static int advance_auto_increment_after_update(
    struct mylite_db *database,
    const struct planned_update *plan,
    const struct planned_value *assignment_value,
    int64_t affected_rows
) {
    struct integer_column_range range = {0};
    int64_t next_value = plan->table.auto_increment_next;
    int rc = MYLITE_OK;

    if (planned_update_has_multiple_assignments(plan) || affected_rows <= 0 ||
        !column_descriptor_is_auto_increment(&plan->assignment_column) ||
        assignment_value->is_null || assignment_value->is_text || assignment_value->integer <= 0) {
        return MYLITE_OK;
    }
    if (next_value <= 0) {
        next_value = 1;
    }

    rc = integer_range_for_column(
        database,
        &plan->assignment_column,
        "AUTO_INCREMENT supports only baseline integer columns",
        &range
    );
    if (rc == MYLITE_OK) {
        rc = next_auto_increment_after_update_value(
            database,
            next_value,
            assignment_value->integer,
            &range,
            &next_value
        );
    }
    if (rc == MYLITE_OK && next_value != plan->table.auto_increment_next) {
        rc = update_table_auto_increment_next(database, &plan->table, next_value);
    }

    return rc;
}

static int update_table_auto_increment_next(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    int64_t auto_increment_next
) {
    int rc = MYLITE_OK;

    if (table == NULL) {
        set_runtime_error(database, "invalid auto-increment table descriptor");
        return MYLITE_ERROR;
    }
    if (table->table_id < 0) {
        rc = mylite_temporary_catalog_update_table_auto_increment_next(
            &database->session.temporary_catalog,
            table->table_id,
            auto_increment_next
        );
    } else {
        rc = mylite_catalog_update_table_auto_increment_next(
            database,
            table->table_id,
            auto_increment_next
        );
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to update auto-increment counter");
    }
    return rc;
}

static int touch_persistent_table_updated_time(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    int64_t affected_rows,
    bool force
) {
    int rc = MYLITE_OK;

    if (table == NULL) {
        set_runtime_error(database, "invalid table descriptor for status metadata update");
        return MYLITE_ERROR;
    }
    if (table->kind != MYLITE_CATALOG_TABLE_KIND_BASE || table->table_id <= 0) {
        return MYLITE_OK;
    }
    if (!force && affected_rows <= 0) {
        return MYLITE_OK;
    }

    rc = mylite_catalog_update_table_updated_time(
        database,
        table->table_id,
        current_wall_clock_epoch()
    );
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to update table status metadata");
    }
    return rc;
}

static int step_update_statement(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_update *executable_plan,
    int64_t *out_affected_rows,
    const struct planned_update *plan
) {
    int sqlite_rc = sqlite3_step(statement);

    if (sqlite_rc == SQLITE_DONE) {
        *out_affected_rows = (int64_t)sqlite3_changes64(database->sqlite);
        return MYLITE_OK;
    }
    if (sqlite_status_is_constraint(sqlite_rc)) {
        bool was_generated_range_violation = false;
        bool was_check_violation = false;
        bool was_generated_not_null_violation = false;
        int rc = handle_generated_range_constraint_violation(
            database,
            (struct generated_constraint_columns){
                .columns = plan->columns,
                .column_count = plan->column_count,
            },
            1U,
            &was_generated_range_violation
        );

        if (rc != MYLITE_OK || was_generated_range_violation) {
            return rc;
        }
        rc = handle_check_constraint_violation(
            database,
            plan->table.table_id,
            false,
            &was_check_violation
        );
        if (rc != MYLITE_OK || was_check_violation) {
            return rc;
        }
        rc = handle_generated_not_null_violation(
            database,
            sqlite_rc,
            plan->columns,
            plan->column_count,
            &was_generated_not_null_violation
        );
        if (rc != MYLITE_OK || was_generated_not_null_violation) {
            return rc;
        }
        return handle_update_unique_key_conflict(database, executable_plan, plan, sqlite_rc);
    }

    return mylite_sqlite_status_to_mylite(sqlite_rc);
}

static int update_matches_any_row(
    struct mylite_db *database,
    const struct planned_update *plan,
    bool *out_matches
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = MYLITE_OK;

    *out_matches = false;
    if (plan->limit.has_limit && plan->limit.row_count == 0) {
        return MYLITE_OK;
    }

    rc = build_update_matched_sql(plan, &sql);
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_update_matched_parameters(statement, plan);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            *out_matches = true;
        } else if (sqlite_rc != SQLITE_DONE) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);

    return rc;
}

static int init_select_source_context(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    const struct table_name_resolution *source,
    struct select_source_context *out_context
) {
    const struct mylite_sql_ast_node *alias = from_table_alias_node(from_clause);

    *out_context = (struct select_source_context){.source = source};
    if (alias == NULL) {
        return MYLITE_OK;
    }
    if (alias->kind != MYLITE_SQL_AST_IDENTIFIER) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    int rc = copy_identifier_text(alias, out_context->alias, sizeof(out_context->alias), database);

    if (rc != MYLITE_OK) {
        return rc;
    }
    out_context->has_alias = true;

    return MYLITE_OK;
}

static int plan_select(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool allow_order_by_field,
    struct planned_select *out_plan
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *from_clause = child_at(statement, 1U);
    struct select_optional_clauses clauses = {0};
    struct mylite_catalog_column_descriptor *table_columns = NULL;
    struct select_source_context source_context = {0};
    struct select_predicate_plan_options predicate_options = {
        .allow_exists = true,
        .allow_in_subquery = true,
        .allow_date_format_numeric_predicate = true,
    };
    bool allow_field_order = allow_order_by_field;
    size_t table_column_count = 0U;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_select){0};
    out_plan->is_distinct =
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT;
    if (out_plan->is_distinct) {
        allow_field_order = false;
    }
    out_plan->calc_found_rows = mylite_sql_ast_node_select_calc_found_rows(statement) != 0;
    if (out_plan->calc_found_rows && out_plan->is_distinct) {
        set_unsupported_error(
            database,
            "SQL_CALC_FOUND_ROWS supports only non-distinct descriptor-backed table SELECT"
        );
        return MYLITE_ERROR;
    }
    if (from_clause != NULL && from_clause->kind == MYLITE_SQL_AST_FROM_JOIN) {
        const struct joined_select_ast joined_select = {
            .statement = statement,
            .from_clause = from_clause,
        };
        return plan_joined_select(database, &joined_select, out_plan);
    }
    if (from_clause == NULL || from_clause->kind != MYLITE_SQL_AST_FROM_TABLE) {
        set_unsupported_error(database, "SELECT supports only descriptor-backed table reads");
        return MYLITE_ERROR;
    }
    rc = collect_select_optional_clauses(database, statement, &clauses);

    if (rc == MYLITE_OK) {
        rc = resolve_visible_table_reference(
            database,
            child_at(from_clause, 0U),
            &out_plan->source,
            &out_plan->table
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_select_index_hints(database, from_clause, &out_plan->table);
    }
    if (rc == MYLITE_OK) {
        rc = init_select_source_context(database, from_clause, &out_plan->source, &source_context);
    }
    copy_plan_source_alias_if_present(out_plan, &source_context);
    if (rc == MYLITE_OK) {
        rc = load_table_columns(
            database,
            out_plan->table.table_id,
            &table_columns,
            &table_column_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_select_projection(
            database,
            select_list,
            &source_context,
            table_columns,
            table_column_count,
            out_plan
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_select_predicate_with_options(
            database,
            clauses.where_clause,
            &source_context,
            table_columns,
            table_column_count,
            &predicate_options,
            &out_plan->predicate
        );
    }
    if (rc == MYLITE_OK && out_plan->predicate.qualify_column_references) {
        out_plan->requires_source_alias = true;
    }
    if (rc == MYLITE_OK) {
        rc = plan_select_order(
            database,
            clauses.order_clause,
            &source_context,
            out_plan,
            table_columns,
            table_column_count,
            allow_field_order,
            true,
            &out_plan->order
        );
    }
    if (rc == MYLITE_OK && out_plan->requires_source_alias) {
        out_plan->order.qualify_column_reference = true;
    }
    if (rc == MYLITE_OK) {
        rc = validate_select_distinct_order(database, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = plan_select_limit(database, clauses.limit_clause, &out_plan->limit);
    }

    free(table_columns);
    if (rc != MYLITE_OK) {
        planned_select_deinit(out_plan);
    }

    return rc;
}

static int validate_select_distinct_order(
    struct mylite_db *database,
    const struct planned_select *plan
) {
    if (plan == NULL || !plan->is_distinct || !plan->order.has_order) {
        return MYLITE_OK;
    }
    for (size_t item_index = 0U; item_index < plan->order.item_count; ++item_index) {
        const struct planned_select_order_item *item = &plan->order.items[item_index];

        if (item->kind != PLANNED_SELECT_ORDER_ITEM_COLUMN ||
            !select_distinct_order_item_is_selected(plan, item)) {
            set_unsupported_error(
                database,
                "SELECT DISTINCT supports ORDER BY only on selected columns"
            );
            return MYLITE_ERROR;
        }
    }

    return MYLITE_OK;
}

static bool select_distinct_order_item_is_selected(
    const struct planned_select *plan,
    const struct planned_select_order_item *item
) {
    for (size_t column_index = 0U; column_index < plan->column_count; ++column_index) {
        if (item->column_source_index == plan->column_source_indexes[column_index] &&
            item->column.column_id == plan->columns[column_index].column_id) {
            return true;
        }
    }

    return false;
}

static int collect_select_optional_clauses(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct select_optional_clauses *out_clauses
) {
    const struct mylite_sql_ast_node *optional_clause = child_at(statement, 2U);

    *out_clauses = (struct select_optional_clauses){0};
    while (optional_clause != NULL) {
        if (optional_clause->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
            out_clauses->where_clause = optional_clause;
        } else if (optional_clause->kind == MYLITE_SQL_AST_ORDER_BY_CLAUSE) {
            out_clauses->order_clause = optional_clause;
        } else if (optional_clause->kind == MYLITE_SQL_AST_LIMIT_CLAUSE) {
            out_clauses->limit_clause = optional_clause;
        } else {
            set_unsupported_error(database, "SELECT supports only WHERE, ORDER BY, and LIMIT");
            return MYLITE_ERROR;
        }
        optional_clause = optional_clause->next_sibling;
    }

    return MYLITE_OK;
}

static int plan_select_projection(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_select *out_plan
) {
    int rc = plan_select_columns(
        database,
        select_list,
        source_context,
        table_columns,
        table_column_count,
        out_plan
    );

    if (rc == MYLITE_OK && out_plan->is_distinct) {
        rc = validate_select_distinct_columns(database, out_plan);
    }

    return rc;
}

static int validate_select_distinct_columns(
    struct mylite_db *database,
    const struct planned_select *plan
) {
    for (size_t column_index = 0U; column_index < plan->column_count; ++column_index) {
        if (!select_distinct_column_is_supported(&plan->columns[column_index])) {
            set_unsupported_error(
                database,
                "SELECT DISTINCT supports only integer, YEAR, DATE, TIME, DATETIME, TIMESTAMP, or "
                "nonbinary string descriptor columns"
            );
            return MYLITE_ERROR;
        }
    }

    return MYLITE_OK;
}

static bool select_distinct_column_is_supported(
    const struct mylite_catalog_column_descriptor *column
) {
    struct integer_column_range range = {0};

    if (column_descriptor_is_string_family(column) || column_descriptor_is_date(column) ||
        column_descriptor_is_time(column) || column_descriptor_is_datetime(column) ||
        column_descriptor_is_timestamp(column) || column_descriptor_is_year(column)) {
        return true;
    }

    return integer_range_for_column(NULL, column, "", &range) == MYLITE_OK;
}

static void copy_plan_source_alias_if_present(
    struct planned_select *plan,
    const struct select_source_context *context
) {
    if (plan == NULL || context == NULL || !context->has_alias) {
        return;
    }

    memcpy(plan->source_alias, context->alias, sizeof(plan->source_alias));
    plan->source_has_alias = true;
}

static int plan_joined_select(
    struct mylite_db *database,
    const struct joined_select_ast *joined_select,
    struct planned_select *out_plan
) {
    const struct mylite_sql_ast_node *select_list = NULL;
    struct joined_select_temp_nodes temp_nodes = {0};
    struct select_optional_clauses clauses = {0};
    struct select_source_context source_context = {0};
    int rc = MYLITE_OK;

    if (joined_select == NULL || joined_select->statement == NULL ||
        joined_select->from_clause == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    select_list = child_at(joined_select->statement, 0U);
    if (out_plan->is_distinct) {
        set_unsupported_error(database, "joined SELECT does not yet support DISTINCT");
        return MYLITE_ERROR;
    }
    rc = collect_select_optional_clauses(database, joined_select->statement, &clauses);
    if (rc == MYLITE_OK) {
        rc =
            prepare_joined_select_plan(database, joined_select->from_clause, out_plan, &temp_nodes);
    }
    if (rc == MYLITE_OK) {
        rc = reject_duplicate_select_source_reference(
            database,
            out_plan->sources,
            out_plan->source_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = init_join_select_source_context(
            out_plan->sources,
            out_plan->source_count,
            &source_context
        );
    }
    if (rc == MYLITE_OK) {
        rc = reject_unsupported_multi_source_join_edges(database, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = plan_joined_select_conditions(
            database,
            temp_nodes.join_condition_nodes,
            temp_nodes.join_condition_node_count,
            out_plan
        );
    }
    if (rc == MYLITE_OK) {
        out_plan->join_condition = out_plan->join_conditions[0];
    }
    if (rc == MYLITE_OK) {
        rc = plan_select_columns(database, select_list, &source_context, NULL, 0U, out_plan);
    }
    if (rc == MYLITE_OK) {
        rc = plan_joined_select_predicate(
            database,
            clauses.where_clause,
            &source_context,
            &out_plan->predicate
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_select_order(
            database,
            clauses.order_clause,
            &source_context,
            out_plan,
            NULL,
            0U,
            false,
            false,
            &out_plan->order
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_select_limit(database, clauses.limit_clause, &out_plan->limit);
    }

    if (rc != MYLITE_OK) {
        planned_select_deinit(out_plan);
    }
    joined_select_temp_nodes_deinit(&temp_nodes);
    return rc;
}

static int prepare_joined_select_plan(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct planned_select *out_plan,
    struct joined_select_temp_nodes *temp_nodes
) {
    int rc = MYLITE_OK;

    if (temp_nodes == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    out_plan->source_count = joined_select_source_count(from_clause);
    if (out_plan->source_count < 2U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    out_plan->join_count = out_plan->source_count - 1U;
    temp_nodes->source_node_count = out_plan->source_count;
    temp_nodes->join_condition_node_count = out_plan->join_count;
    out_plan->sources = calloc(out_plan->source_count, sizeof(*out_plan->sources));
    out_plan->join_kinds = calloc(out_plan->join_count, sizeof(*out_plan->join_kinds));
    out_plan->join_conditions = calloc(out_plan->join_count, sizeof(*out_plan->join_conditions));
    temp_nodes->source_nodes = (const struct mylite_sql_ast_node **)
        calloc(out_plan->source_count, sizeof(*temp_nodes->source_nodes));
    temp_nodes->join_condition_nodes = (const struct mylite_sql_ast_node **)
        calloc(out_plan->join_count, sizeof(*temp_nodes->join_condition_nodes));

    if (out_plan->sources == NULL || out_plan->join_kinds == NULL ||
        out_plan->join_conditions == NULL || temp_nodes->source_nodes == NULL ||
        temp_nodes->join_condition_nodes == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = collect_joined_select_parts(database, from_clause, out_plan, temp_nodes);
    if (rc == MYLITE_OK) {
        rc = plan_joined_select_sources(database, temp_nodes->source_nodes, out_plan);
    }
    if (rc == MYLITE_OK) {
        out_plan->join_kind = out_plan->join_kinds[0];
    }

    return rc;
}

static void joined_select_temp_nodes_deinit(struct joined_select_temp_nodes *temp_nodes) {
    if (temp_nodes == NULL) {
        return;
    }

    free((void *)temp_nodes->source_nodes);
    free((void *)temp_nodes->join_condition_nodes);
    *temp_nodes = (struct joined_select_temp_nodes){0};
}

static size_t joined_select_source_count(const struct mylite_sql_ast_node *from_clause) {
    const struct mylite_sql_ast_node *node = from_clause;
    size_t source_count = 0U;

    while (node != NULL && node->kind == MYLITE_SQL_AST_FROM_JOIN) {
        ++source_count;
        node = child_at(node, 0U);
    }
    if (node == NULL || node->kind != MYLITE_SQL_AST_FROM_TABLE) {
        return 0U;
    }
    return source_count + 1U;
}

static int collect_joined_select_parts(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct planned_select *out_plan,
    const struct joined_select_temp_nodes *temp_nodes
) {
    const struct mylite_sql_ast_node *node = from_clause;
    size_t join_index = 0U;

    if (node == NULL || out_plan == NULL || temp_nodes == NULL ||
        temp_nodes->source_nodes == NULL || temp_nodes->join_condition_nodes == NULL ||
        temp_nodes->source_node_count != out_plan->source_count ||
        temp_nodes->join_condition_node_count != out_plan->join_count) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    join_index = out_plan->join_count;
    while (node != NULL && node->kind == MYLITE_SQL_AST_FROM_JOIN) {
        const struct mylite_sql_ast_node *right_source = child_at(node, 1U);

        if (join_index == 0U || right_source == NULL ||
            right_source->kind != MYLITE_SQL_AST_FROM_TABLE) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }

        --join_index;
        if (join_index + 1U >= temp_nodes->source_node_count ||
            join_index >= temp_nodes->join_condition_node_count) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        temp_nodes->source_nodes[join_index + 1U] = right_source;
        out_plan->join_kinds[join_index] = mylite_sql_ast_node_join_kind(node);
        temp_nodes->join_condition_nodes[join_index] = child_at(node, 2U);
        node = child_at(node, 0U);
    }

    if (join_index != 0U || node == NULL || node->kind != MYLITE_SQL_AST_FROM_TABLE) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    temp_nodes->source_nodes[0] = node;
    return MYLITE_OK;
}

static int plan_joined_select_sources(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *const *source_nodes,
    struct planned_select *out_plan
) {
    if (source_nodes == NULL || out_plan == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    for (size_t source_index = 0U; source_index < out_plan->source_count; ++source_index) {
        if (source_nodes[source_index] == NULL) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        const int rc = plan_joined_select_source(
            database,
            source_nodes[source_index],
            &out_plan->sources[source_index]
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    return MYLITE_OK;
}

static int reject_unsupported_multi_source_join_edges(
    struct mylite_db *database,
    const struct planned_select *plan
) {
    if (plan == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (plan->source_count <= 2U) {
        return MYLITE_OK;
    }

    for (size_t join_index = 0U; join_index < plan->join_count; ++join_index) {
        if (plan->join_kinds[join_index] == MYLITE_SQL_AST_JOIN_KIND_LEFT_OUTER ||
            plan->join_kinds[join_index] == MYLITE_SQL_AST_JOIN_KIND_RIGHT_OUTER) {
            set_unsupported_error(
                database,
                "multi-source SELECT supports only inner and cartesian join chains"
            );
            return MYLITE_ERROR;
        }
    }
    return MYLITE_OK;
}

static int plan_joined_select_conditions(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *const *join_condition_nodes,
    size_t join_condition_node_count,
    struct planned_select *out_plan
) {
    if (join_condition_nodes == NULL || out_plan == NULL ||
        join_condition_node_count != out_plan->join_count) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    for (size_t join_index = 0U; join_index < join_condition_node_count; ++join_index) {
        struct select_source_context edge_source_context = {0};
        int rc = init_join_select_source_context(
            out_plan->sources,
            join_index + 2U,
            &edge_source_context
        );

        if (rc == MYLITE_OK) {
            rc = plan_joined_select_condition(
                database,
                join_condition_nodes[join_index],
                out_plan->join_kinds[join_index],
                &edge_source_context,
                &out_plan->join_conditions[join_index]
            );
        }
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return MYLITE_OK;
}

static int plan_joined_select_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *source_node,
    struct planned_select_source *out_source
) {
    const struct mylite_sql_ast_node *alias = NULL;
    int rc = MYLITE_OK;

    *out_source = (struct planned_select_source){0};
    if (source_node == NULL || source_node->kind != MYLITE_SQL_AST_FROM_TABLE) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    rc = resolve_visible_table_reference(
        database,
        child_at(source_node, 0U),
        &out_source->source,
        &out_source->table
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = validate_select_index_hints(database, source_node, &out_source->table);
    if (rc != MYLITE_OK) {
        return rc;
    }

    alias = from_table_alias_node(source_node);
    if (alias != NULL) {
        if (alias->kind != MYLITE_SQL_AST_IDENTIFIER) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        rc = copy_identifier_text(alias, out_source->alias, sizeof(out_source->alias), database);
        if (rc != MYLITE_OK) {
            return rc;
        }
        out_source->has_alias = true;
    }

    return load_table_columns(
        database,
        out_source->table.table_id,
        &out_source->columns,
        &out_source->column_count
    );
}

static const struct mylite_sql_ast_node *from_table_alias_node(
    const struct mylite_sql_ast_node *from_table
) {
    const struct mylite_sql_ast_node *child = NULL;

    if (from_table == NULL || from_table->kind != MYLITE_SQL_AST_FROM_TABLE) {
        return NULL;
    }
    child = child_at(from_table, 1U);
    while (child != NULL) {
        if (child->kind == MYLITE_SQL_AST_IDENTIFIER) {
            return child;
        }
        child = child->next_sibling;
    }

    return NULL;
}

static const struct mylite_sql_ast_node *from_table_index_hint_list_node(
    const struct mylite_sql_ast_node *from_table
) {
    const struct mylite_sql_ast_node *child = NULL;

    if (from_table == NULL || from_table->kind != MYLITE_SQL_AST_FROM_TABLE) {
        return NULL;
    }
    child = child_at(from_table, 1U);
    while (child != NULL) {
        if (child->kind == MYLITE_SQL_AST_INDEX_HINT_LIST) {
            return child;
        }
        child = child->next_sibling;
    }

    return NULL;
}

static int validate_select_index_hints(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_table,
    const struct mylite_catalog_table_descriptor *table
) {
    const struct mylite_sql_ast_node *hint_list = from_table_index_hint_list_node(from_table);
    bool has_use = false;
    bool has_force = false;

    if (hint_list == NULL) {
        return MYLITE_OK;
    }
    if (table == NULL || hint_list->kind != MYLITE_SQL_AST_INDEX_HINT_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    for (const struct mylite_sql_ast_node *hint = child_at(hint_list, 0U); hint != NULL;
         hint = hint->next_sibling) {
        int rc = validate_select_index_hint(database, hint, table, &has_use, &has_force);

        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return MYLITE_OK;
}

static int validate_select_index_hint(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *hint,
    const struct mylite_catalog_table_descriptor *table,
    bool *has_use,
    bool *has_force
) {
    const struct mylite_sql_ast_node *name_list = NULL;

    if (hint == NULL || table == NULL || has_use == NULL || has_force == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (hint->kind == MYLITE_SQL_AST_USE_INDEX_HINT) {
        *has_use = true;
    } else if (hint->kind == MYLITE_SQL_AST_FORCE_INDEX_HINT) {
        *has_force = true;
    } else if (hint->kind != MYLITE_SQL_AST_IGNORE_INDEX_HINT) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (*has_use && *has_force) {
        set_index_hint_use_force_error(database);
        return MYLITE_ERROR;
    }

    name_list = index_hint_name_list_node(hint);
    if (name_list == NULL || name_list->kind != MYLITE_SQL_AST_IDENTIFIER_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (hint->kind != MYLITE_SQL_AST_USE_INDEX_HINT && child_at(name_list, 0U) == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    return validate_select_index_hint_names(database, name_list, table);
}

static const struct mylite_sql_ast_node *index_hint_name_list_node(
    const struct mylite_sql_ast_node *hint
) {
    const struct mylite_sql_ast_node *child = NULL;

    if (hint == NULL) {
        return NULL;
    }
    child = child_at(hint, 0U);
    while (child != NULL) {
        if (child->kind == MYLITE_SQL_AST_IDENTIFIER_LIST) {
            return child;
        }
        child = child->next_sibling;
    }

    return NULL;
}

static int validate_select_index_hint_names(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *name_list,
    const struct mylite_catalog_table_descriptor *table
) {
    for (const struct mylite_sql_ast_node *name_node = child_at(name_list, 0U); name_node != NULL;
         name_node = name_node->next_sibling) {
        char index_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
        bool exists = false;
        int rc = copy_identifier_text(name_node, index_name, sizeof(index_name), database);

        if (rc == MYLITE_OK) {
            rc = select_index_hint_name_exists(database, table->table_id, index_name, &exists);
        }
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (!exists) {
            set_key_does_not_exist_in_table_error(database, index_name, table->name);
            return MYLITE_ERROR;
        }
    }

    return MYLITE_OK;
}

static int select_index_hint_name_exists(
    struct mylite_db *database,
    int64_t table_id,
    const char *index_name,
    bool *out_exists
) {
    struct select_index_hint_lookup_context context = {
        .name = index_name,
        .exact_match = false,
        .prefix_match_count = 0U,
    };
    int rc = MYLITE_OK;

    if (out_exists == NULL) {
        return MYLITE_MISUSE;
    }
    *out_exists = false;
    if (table_id < 0) {
        rc = mylite_temporary_catalog_for_each_index_in_table(
            &database->session.temporary_catalog,
            table_id,
            match_select_index_hint_name,
            &context
        );
    } else {
        rc = mylite_catalog_for_each_index_in_table(
            database,
            table_id,
            match_select_index_hint_name,
            &context
        );
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to validate index hints");
        return rc;
    }

    if (context.exact_match || context.prefix_match_count == 1U) {
        *out_exists = true;
    }
    return MYLITE_OK;
}

static int match_select_index_hint_name(
    const struct mylite_catalog_index_descriptor *index,
    void *user_data
) {
    struct select_index_hint_lookup_context *context = user_data;

    if (index != NULL && context != NULL &&
        text_equals_ascii_case_insensitive(index->name, context->name)) {
        context->exact_match = true;
    } else if (
        index != NULL && context != NULL &&
        text_has_ascii_case_insensitive_prefix(index->name, context->name)
    ) {
        ++context->prefix_match_count;
    }
    return MYLITE_OK;
}

static int init_join_select_source_context(
    const struct planned_select_source *sources,
    size_t source_count,
    struct select_source_context *out_context
) {
    *out_context = (struct select_source_context){
        .sources = sources,
        .source_count = source_count,
    };
    return MYLITE_OK;
}

static int reject_duplicate_select_source_reference(
    struct mylite_db *database,
    const struct planned_select_source *sources,
    size_t source_count
) {
    for (size_t left_index = 0U; left_index < source_count; ++left_index) {
        const char *left_name = planned_select_source_reference_name(&sources[left_index]);

        for (size_t right_index = left_index + 1U; right_index < source_count; ++right_index) {
            const char *right_name = planned_select_source_reference_name(&sources[right_index]);

            if (text_equals_ascii_case_insensitive(left_name, right_name)) {
                set_not_unique_table_alias_error(database, right_name);
                return MYLITE_ERROR;
            }
        }
    }

    return MYLITE_OK;
}

static const char *planned_select_source_reference_name(
    const struct planned_select_source *source
) {
    if (source->has_alias) {
        return source->alias;
    }
    return source->source.table_name;
}

static int plan_joined_select_condition(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *condition_node,
    enum mylite_sql_ast_join_kind join_kind,
    const struct select_source_context *source_context,
    struct planned_select_join_condition *out_condition
) {
    int rc = MYLITE_OK;

    *out_condition = (struct planned_select_join_condition){.has_condition = false};
    if (condition_node == NULL) {
        if (join_kind == MYLITE_SQL_AST_JOIN_KIND_LEFT_OUTER ||
            join_kind == MYLITE_SQL_AST_JOIN_KIND_RIGHT_OUTER) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        return MYLITE_OK;
    }
    if (condition_node->kind != MYLITE_SQL_AST_COMPARISON_PREDICATE ||
        mylite_sql_ast_node_operator(condition_node) != MYLITE_SQL_AST_OPERATOR_EQUAL) {
        set_unsupported_error(database, "joined SELECT supports only descriptor equality ON");
        return MYLITE_ERROR;
    }

    rc = resolve_descriptor_column_reference_with_source_index(
        database,
        child_at(condition_node, 0U),
        source_context,
        COLUMN_REFERENCE_ON,
        "joined SELECT supports only descriptor equality ON",
        NULL,
        0U,
        &out_condition->left_column,
        &out_condition->left_source_index
    );
    if (rc == MYLITE_OK) {
        rc = resolve_descriptor_column_reference_with_source_index(
            database,
            child_at(condition_node, 1U),
            source_context,
            COLUMN_REFERENCE_ON,
            "joined SELECT supports only descriptor equality ON",
            NULL,
            0U,
            &out_condition->right_column,
            &out_condition->right_source_index
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!join_condition_columns_are_compatible(
            &out_condition->left_column,
            &out_condition->right_column
        )) {
        set_unsupported_error(
            database,
            "joined SELECT supports only same-family integer or string descriptor equality ON"
        );
        return MYLITE_ERROR;
    }

    out_condition->has_condition = true;
    return MYLITE_OK;
}

static bool join_condition_columns_are_compatible(
    const struct mylite_catalog_column_descriptor *left,
    const struct mylite_catalog_column_descriptor *right
) {
    if (join_condition_column_is_integer_family(left)) {
        return join_condition_column_is_integer_family(right);
    }
    if (column_descriptor_is_string_family(left)) {
        return column_descriptor_is_string_family(right);
    }
    return false;
}

static bool join_condition_column_is_integer_family(
    const struct mylite_catalog_column_descriptor *column
) {
    if (column == NULL || column->physical_type[0] == '\0') {
        return false;
    }
    return strcmp(column->physical_type, "INTEGER") == 0;
}

static void planned_select_order_deinit(struct planned_select_order *order) {
    if (order == NULL) {
        return;
    }

    for (size_t item_index = 0U; item_index < order->item_count; ++item_index) {
        planned_select_order_item_deinit(&order->items[item_index]);
    }
    free(order->items);
    memset(order, 0, sizeof(*order));
}

static void planned_select_deinit(struct planned_select *plan) {
    if (plan == NULL) {
        return;
    }

    for (size_t source_index = 0U; source_index < plan->source_count; ++source_index) {
        free(plan->sources[source_index].columns);
    }
    free(plan->sources);
    free(plan->columns);
    free((void *)plan->column_aliases);
    free(plan->column_source_indexes);
    free(plan->join_kinds);
    free(plan->join_conditions);
    planned_select_predicate_deinit(&plan->predicate);
    planned_select_order_deinit(&plan->order);
    *plan = (struct planned_select){0};
}

static int plan_row_scalar_select(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool allow_order_by_field,
    struct planned_row_scalar_select *out_plan
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *from_clause = child_at(statement, 1U);
    const struct select_source_context *item_source_context = NULL;
    struct row_scalar_select_clauses clauses = {0};
    struct mylite_catalog_column_descriptor *table_columns = NULL;
    struct select_source_context source_context = {0};
    const struct mylite_catalog_table_descriptor *item_table = NULL;
    size_t table_column_count = 0U;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_row_scalar_select){false};
    if (mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT) {
        set_unsupported_error(database, "row-scalar SELECT projection does not support DISTINCT");
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_select_calc_found_rows(statement) != 0) {
        set_unsupported_error(
            database,
            "SQL_CALC_FOUND_ROWS does not support row-scalar SELECT projection"
        );
        return MYLITE_ERROR;
    }

    if (from_clause != NULL && from_clause->kind == MYLITE_SQL_AST_FROM_TABLE) {
        out_plan->has_source = true;
        rc = collect_row_scalar_select_clauses(database, child_at(statement, 2U), &clauses);
        if (rc == MYLITE_OK) {
            rc = plan_row_scalar_select_source(
                database,
                from_clause,
                out_plan,
                &source_context,
                &table_columns,
                &table_column_count
            );
        }
        if (rc == MYLITE_OK) {
            item_source_context = &source_context;
        }
    } else if (from_clause != NULL && from_clause->kind == MYLITE_SQL_AST_FROM_DUAL) {
        rc = collect_row_scalar_select_clauses(database, child_at(statement, 2U), &clauses);
    } else if (from_clause != NULL) {
        set_unsupported_error(
            database,
            "row-scalar SELECT supports only no-source, DUAL, or one descriptor table source"
        );
        return MYLITE_ERROR;
    }

    if (out_plan->has_source) {
        item_table = &out_plan->table;
    }
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_select_items(
            database,
            select_list,
            item_source_context,
            item_table,
            table_columns,
            table_column_count,
            out_plan
        );
    }
    if (rc == MYLITE_OK && out_plan->has_source) {
        rc = plan_row_scalar_select_row_envelope(
            database,
            &clauses,
            &source_context,
            table_columns,
            table_column_count,
            allow_order_by_field,
            out_plan
        );
    }
    if (rc == MYLITE_OK && !out_plan->has_source) {
        rc = plan_row_scalar_select_tableless_filter(database, &clauses, out_plan);
    }

    free(table_columns);
    if (rc != MYLITE_OK) {
        planned_row_scalar_select_deinit(out_plan);
    }

    return rc;
}

static int collect_row_scalar_select_clauses(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *optional_clause,
    struct row_scalar_select_clauses *out_clauses
) {
    *out_clauses = (struct row_scalar_select_clauses){0};
    while (optional_clause != NULL) {
        if (optional_clause->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
            out_clauses->where_clause = optional_clause;
        } else if (optional_clause->kind == MYLITE_SQL_AST_ORDER_BY_CLAUSE) {
            out_clauses->order_clause = optional_clause;
        } else if (optional_clause->kind == MYLITE_SQL_AST_LIMIT_CLAUSE) {
            out_clauses->limit_clause = optional_clause;
        } else {
            set_unsupported_error(
                database,
                "row-scalar SELECT supports only WHERE, ORDER BY, and LIMIT"
            );
            return MYLITE_ERROR;
        }
        optional_clause = optional_clause->next_sibling;
    }

    return MYLITE_OK;
}

static int plan_row_scalar_select_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct planned_row_scalar_select *out_plan,
    struct select_source_context *out_source_context,
    struct mylite_catalog_column_descriptor **out_table_columns,
    size_t *out_table_column_count
) {
    int rc = resolve_visible_table_reference(
        database,
        child_at(from_clause, 0U),
        &out_plan->source,
        &out_plan->table
    );
    if (rc == MYLITE_OK) {
        rc = validate_select_index_hints(database, from_clause, &out_plan->table);
    }
    if (rc == MYLITE_OK) {
        rc = init_select_source_context(
            database,
            from_clause,
            &out_plan->source,
            out_source_context
        );
    }
    if (rc == MYLITE_OK && out_source_context->has_alias) {
        memcpy(out_plan->source_alias, out_source_context->alias, sizeof(out_plan->source_alias));
        out_plan->source_has_alias = true;
    }
    if (rc == MYLITE_OK) {
        rc = load_table_columns(
            database,
            out_plan->table.table_id,
            out_table_columns,
            out_table_column_count
        );
    }

    return rc;
}

static int plan_row_scalar_select_row_envelope(
    struct mylite_db *database,
    const struct row_scalar_select_clauses *clauses,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    bool allow_order_by_field,
    struct planned_row_scalar_select *out_plan
) {
    struct planned_select order_alias_plan = {
        .columns = NULL,
        .column_aliases = NULL,
        .column_count = 0U,
    };
    int rc = plan_select_predicate(
        database,
        clauses->where_clause,
        source_context,
        table_columns,
        table_column_count,
        &out_plan->predicate
    );

    if (rc == MYLITE_OK) {
        rc = plan_select_order(
            database,
            clauses->order_clause,
            source_context,
            &order_alias_plan,
            table_columns,
            table_column_count,
            allow_order_by_field,
            true,
            &out_plan->order
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_select_limit(database, clauses->limit_clause, &out_plan->limit);
    }

    return rc;
}

static int plan_row_scalar_select_tableless_filter(
    struct mylite_db *database,
    const struct row_scalar_select_clauses *clauses,
    struct planned_row_scalar_select *out_plan
) {
    if (clauses->order_clause != NULL || clauses->limit_clause != NULL) {
        set_unsupported_error(
            database,
            "row-scalar SELECT FROM DUAL supports only scalar-literal or EXISTS WHERE filtering"
        );
        return MYLITE_ERROR;
    }
    if (clauses->where_clause == NULL) {
        return MYLITE_OK;
    }
    if (row_scalar_select_where_is_exists_filter(clauses->where_clause)) {
        return plan_row_scalar_select_exists_filter(database, clauses->where_clause, out_plan);
    }

    return plan_row_scalar_select_scalar_filter(database, clauses->where_clause, out_plan);
}

static int plan_row_scalar_select_exists_filter(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    struct planned_row_scalar_select *out_plan
) {
    const struct mylite_sql_ast_node *predicate = NULL;
    bool negate = false;
    int rc = MYLITE_OK;

    if (where_clause == NULL || where_clause->kind != MYLITE_SQL_AST_WHERE_CLAUSE) {
        set_unsupported_error(
            database,
            "row-scalar SELECT FROM DUAL supports only scalar-literal or EXISTS WHERE filtering"
        );
        return MYLITE_ERROR;
    }

    predicate = unwrap_parenthesized_predicate(child_at(where_clause, 0U));
    if (predicate != NULL && predicate->kind == MYLITE_SQL_AST_NOT_PREDICATE) {
        negate = true;
        predicate = unwrap_parenthesized_predicate(child_at(predicate, 0U));
    }
    if (predicate == NULL || predicate->kind != MYLITE_SQL_AST_EXISTS_PREDICATE) {
        set_unsupported_error(
            database,
            "row-scalar SELECT FROM DUAL supports only scalar-literal or EXISTS WHERE filtering"
        );
        return MYLITE_ERROR;
    }

    rc = plan_exists_subquery(
        database,
        child_at(predicate, 0U),
        NULL,
        NULL,
        0U,
        &out_plan->tableless_filter.subquery
    );
    if (rc == MYLITE_OK) {
        out_plan->tableless_filter.has_filter = true;
        out_plan->tableless_filter.negate = negate;
    }

    return rc;
}

static bool row_scalar_select_where_is_exists_filter(
    const struct mylite_sql_ast_node *where_clause
) {
    const struct mylite_sql_ast_node *predicate = NULL;

    if (where_clause == NULL || where_clause->kind != MYLITE_SQL_AST_WHERE_CLAUSE) {
        return false;
    }
    predicate = unwrap_parenthesized_predicate(child_at(where_clause, 0U));
    if (predicate != NULL && predicate->kind == MYLITE_SQL_AST_NOT_PREDICATE) {
        predicate = unwrap_parenthesized_predicate(child_at(predicate, 0U));
    }

    return (predicate != NULL && predicate->kind == MYLITE_SQL_AST_EXISTS_PREDICATE) != 0;
}

static int plan_row_scalar_select_scalar_filter(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause,
    struct planned_row_scalar_select *out_plan
) {
    const struct mylite_sql_ast_node *predicate = NULL;
    int rc = MYLITE_OK;

    if (where_clause == NULL || where_clause->kind != MYLITE_SQL_AST_WHERE_CLAUSE) {
        set_unsupported_error(
            database,
            "row-scalar SELECT FROM DUAL supports only scalar-literal or EXISTS WHERE filtering"
        );
        return MYLITE_ERROR;
    }

    predicate = child_at(where_clause, 0U);
    rc = plan_select_predicate_node_without_exists(
        database,
        predicate,
        NULL,
        NULL,
        0U,
        NULL,
        &out_plan->tableless_filter.predicate
    );
    if (rc == MYLITE_OK &&
        !planned_row_scalar_tableless_filter_is_supported(&out_plan->tableless_filter.predicate)) {
        set_unsupported_error(
            database,
            "row-scalar SELECT FROM DUAL supports only scalar-literal or EXISTS WHERE filtering"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        out_plan->tableless_filter.has_filter = true;
        out_plan->tableless_filter.has_scalar_predicate = true;
    }
    if (rc != MYLITE_OK) {
        planned_select_predicate_deinit_without_exists(&out_plan->tableless_filter.predicate);
    }

    return rc;
}

static bool planned_row_scalar_tableless_filter_is_supported(
    const struct planned_select_predicate *predicate
) {
    if (!planned_select_predicate_has_expression(predicate)) {
        return false;
    }

    for (size_t node_index = 0U; node_index < predicate->node_count; ++node_index) {
        if (!planned_row_scalar_tableless_filter_node_is_supported(&predicate->nodes[node_index])) {
            return false;
        }
    }

    return true;
}

static bool planned_row_scalar_tableless_filter_node_is_supported(
    const struct planned_select_predicate_node *node
) {
    if (node == NULL) {
        return false;
    }

    switch (node->kind) {
    case PLANNED_SELECT_PREDICATE_AND:
        return node->operator_kind == MYLITE_SQL_AST_OPERATOR_LOGICAL_AND;
    case PLANNED_SELECT_PREDICATE_OR:
        return node->operator_kind == MYLITE_SQL_AST_OPERATOR_LOGICAL_OR;
    case PLANNED_SELECT_PREDICATE_XOR:
        return node->operator_kind == MYLITE_SQL_AST_OPERATOR_LOGICAL_XOR;
    case PLANNED_SELECT_PREDICATE_NOT:
        return node->operator_kind == MYLITE_SQL_AST_OPERATOR_LOGICAL_NOT;
    case PLANNED_SELECT_PREDICATE_ROW_SCALAR_TRUTH:
    case PLANNED_SELECT_PREDICATE_ROW_SCALAR_COMPARISON:
    case PLANNED_SELECT_PREDICATE_ROW_SCALAR_IS_NULL:
        return (node->row_scalar_expression != NULL &&
                node->row_scalar_expression->kind == PLANNED_ROW_SCALAR_EXPRESSION_VALUE) != 0;
    default:
        return false;
    }
}

static void planned_row_scalar_select_deinit(struct planned_row_scalar_select *plan) {
    if (plan == NULL) {
        return;
    }

    for (size_t item_index = 0U; item_index < plan->item_count; ++item_index) {
        planned_row_scalar_expression_deinit(&plan->items[item_index].expression);
    }
    free(plan->items);
    if (plan->tableless_filter.has_filter && plan->tableless_filter.has_scalar_predicate) {
        planned_select_predicate_deinit_without_exists(&plan->tableless_filter.predicate);
    } else if (plan->tableless_filter.has_filter) {
        planned_exists_subquery_deinit(&plan->tableless_filter.subquery);
    }
    planned_select_predicate_deinit(&plan->predicate);
    planned_select_order_deinit(&plan->order);
    *plan = (struct planned_row_scalar_select){false};
}

static bool select_statement_has_group_by_clause(const struct mylite_sql_ast_node *statement) {
    const struct mylite_sql_ast_node *optional_clause = child_at(statement, 2U);

    while (optional_clause != NULL) {
        if (optional_clause->kind == MYLITE_SQL_AST_GROUP_BY_CLAUSE) {
            return true;
        }
        optional_clause = optional_clause->next_sibling;
    }

    return false;
}

static int plan_grouped_aggregate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_grouped_aggregate *out_plan
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *from_clause = child_at(statement, 1U);
    struct grouped_aggregate_clauses clauses = {0};
    struct mylite_catalog_column_descriptor *table_columns = NULL;
    struct select_source_context source_context = {0};
    size_t table_column_count = 0U;
    size_t group_key_count = 0U;
    size_t first_aggregate_index = 0U;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_grouped_aggregate){0};
    if (mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT) {
        set_unsupported_error(database, "GROUP BY does not support SELECT DISTINCT");
        return MYLITE_ERROR;
    }
    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        set_unsupported_error(
            database,
            "GROUP BY supports selected descriptor group columns followed by aggregate results"
        );
        return MYLITE_ERROR;
    }
    if (from_clause == NULL || (from_clause->kind != MYLITE_SQL_AST_FROM_TABLE &&
                                from_clause->kind != MYLITE_SQL_AST_FROM_JOIN)) {
        set_unsupported_error(database, "GROUP BY requires descriptor-backed table sources");
        return MYLITE_ERROR;
    }

    rc = collect_grouped_aggregate_clauses(database, statement, &clauses);
    if (rc == MYLITE_OK) {
        group_key_count = grouped_aggregate_group_key_count(clauses.group_clause);
        if (group_key_count == 0U) {
            set_unsupported_error(database, "GROUP BY requires one descriptor group column");
            return MYLITE_ERROR;
        }
        if (group_key_count > grouped_aggregate_max_group_keys) {
            set_unsupported_error(
                database,
                "GROUP BY supports at most four descriptor group columns"
            );
            return MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        rc = plan_grouped_aggregate_source(
            database,
            from_clause,
            out_plan,
            &source_context,
            &table_columns,
            &table_column_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_grouped_projection_references(
            database,
            select_list,
            &source_context,
            table_columns,
            table_column_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_grouped_aggregate_group_columns(
            database,
            select_list,
            clauses.group_clause,
            &source_context,
            table_columns,
            table_column_count,
            out_plan
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_grouped_aggregate_projection_columns(
            database,
            select_list,
            &source_context,
            table_columns,
            table_column_count,
            out_plan,
            &first_aggregate_index
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_grouped_aggregate_functions(
            database,
            select_list,
            first_aggregate_index,
            &source_context,
            table_columns,
            table_column_count,
            out_plan
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_select_predicate(
            database,
            clauses.where_clause,
            &source_context,
            table_columns,
            table_column_count,
            &out_plan->predicate
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_grouped_aggregate_having(
            database,
            clauses.having_clause,
            &source_context,
            table_columns,
            table_column_count,
            out_plan
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_grouped_aggregate_order(
            database,
            clauses.order_clause,
            &source_context,
            table_columns,
            table_column_count,
            out_plan
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_select_limit(database, clauses.limit_clause, &out_plan->limit);
    }

    free(table_columns);
    return rc;
}

static void planned_grouped_aggregate_deinit(struct planned_grouped_aggregate *plan) {
    if (plan == NULL) {
        return;
    }

    for (size_t source_index = 0U; source_index < plan->source_count; ++source_index) {
        free(plan->sources[source_index].columns);
    }
    for (size_t aggregate_index = 0U; aggregate_index < plan->aggregate_count; ++aggregate_index) {
        planned_select_order_deinit(&plan->aggregates[aggregate_index].aggregate_order);
        planned_value_deinit(&plan->aggregates[aggregate_index].separator);
    }
    free(plan->aggregates);
    free(plan->projections);
    free(plan->groups);
    free(plan->sources);
    planned_select_predicate_deinit(&plan->predicate);
    planned_value_deinit(&plan->having.value);
    planned_select_order_deinit(&plan->order);
    *plan = (struct planned_grouped_aggregate){0};
}

static int collect_grouped_aggregate_clauses(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct grouped_aggregate_clauses *out_clauses
) {
    const struct mylite_sql_ast_node *optional_clause = child_at(statement, 2U);

    *out_clauses = (struct grouped_aggregate_clauses){0};
    while (optional_clause != NULL) {
        switch (optional_clause->kind) {
        case MYLITE_SQL_AST_WHERE_CLAUSE:
            out_clauses->where_clause = optional_clause;
            break;
        case MYLITE_SQL_AST_GROUP_BY_CLAUSE:
            out_clauses->group_clause = optional_clause;
            break;
        case MYLITE_SQL_AST_HAVING_CLAUSE:
            out_clauses->having_clause = optional_clause;
            break;
        case MYLITE_SQL_AST_ORDER_BY_CLAUSE:
            out_clauses->order_clause = optional_clause;
            break;
        case MYLITE_SQL_AST_LIMIT_CLAUSE:
            out_clauses->limit_clause = optional_clause;
            break;
        default:
            set_unsupported_error(
                database,
                "GROUP BY supports only WHERE, GROUP BY, HAVING, ORDER BY, and LIMIT"
            );
            return MYLITE_ERROR;
        }
        optional_clause = optional_clause->next_sibling;
    }
    if (out_clauses->group_clause == NULL) {
        set_unsupported_error(database, "GROUP BY requires one descriptor group column");
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int plan_grouped_aggregate_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct planned_grouped_aggregate *out_plan,
    struct select_source_context *out_source_context,
    struct mylite_catalog_column_descriptor **out_columns,
    size_t *out_column_count
) {
    int rc = MYLITE_OK;

    *out_columns = NULL;
    *out_column_count = 0U;
    if (from_clause->kind == MYLITE_SQL_AST_FROM_JOIN) {
        return plan_grouped_aggregate_join_source(
            database,
            from_clause,
            out_plan,
            out_source_context
        );
    }

    rc = resolve_visible_table_reference(
        database,
        child_at(from_clause, 0U),
        &out_plan->source,
        &out_plan->table
    );

    if (rc == MYLITE_OK) {
        rc = validate_select_index_hints(database, from_clause, &out_plan->table);
    }
    if (rc == MYLITE_OK) {
        rc = init_select_source_context(
            database,
            from_clause,
            &out_plan->source,
            out_source_context
        );
    }
    if (rc == MYLITE_OK) {
        rc = load_table_columns(database, out_plan->table.table_id, out_columns, out_column_count);
    }

    return rc;
}

static int plan_grouped_aggregate_join_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    struct planned_grouped_aggregate *out_plan,
    struct select_source_context *out_source_context
) {
    const struct mylite_sql_ast_node *join_condition = child_at(from_clause, 2U);
    int rc = MYLITE_OK;

    out_plan->join_kind = mylite_sql_ast_node_join_kind(from_clause);
    if ((out_plan->join_kind == MYLITE_SQL_AST_JOIN_KIND_LEFT_OUTER ||
         out_plan->join_kind == MYLITE_SQL_AST_JOIN_KIND_RIGHT_OUTER) &&
        join_condition == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (out_plan->join_kind == MYLITE_SQL_AST_JOIN_KIND_RIGHT_OUTER) {
        set_unsupported_error(database, "GROUP BY does not support RIGHT JOIN");
        return MYLITE_ERROR;
    }
    out_plan->sources = calloc(2U, sizeof(*out_plan->sources));
    if (out_plan->sources == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_plan->source_count = 2U;

    rc = plan_joined_select_source(database, child_at(from_clause, 0U), &out_plan->sources[0]);
    if (rc == MYLITE_OK) {
        rc = plan_joined_select_source(database, child_at(from_clause, 1U), &out_plan->sources[1]);
    }
    if (rc == MYLITE_OK) {
        rc = reject_duplicate_select_source_reference(
            database,
            out_plan->sources,
            out_plan->source_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = init_join_select_source_context(
            out_plan->sources,
            out_plan->source_count,
            out_source_context
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_joined_select_condition(
            database,
            join_condition,
            out_plan->join_kind,
            out_source_context,
            &out_plan->join_condition
        );
    }

    return rc;
}

static int validate_grouped_projection_references(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count
) {
    const struct mylite_sql_ast_node *item = NULL;
    int rc = MYLITE_OK;

    if (select_list_is_wildcard(select_list)) {
        return MYLITE_OK;
    }

    item = child_at(select_list, 0U);
    while (item != NULL) {
        const struct mylite_sql_ast_node *expression = child_at(item, 0U);
        const struct mylite_sql_ast_node *column_node = NULL;
        const struct mylite_sql_ast_node *qualified_wildcard = NULL;
        struct mylite_catalog_column_descriptor column = {0};
        size_t source_index = 0U;

        if (grouped_aggregate_function_from_expression(
                unwrap_parenthesized_expression(expression)
            ) != PLANNED_GROUPED_AGGREGATE_NONE) {
            return MYLITE_OK;
        }
        if (expression != NULL && expression->kind == MYLITE_SQL_AST_WILDCARD &&
            child_at(item, 1U) == NULL) {
            item = item->next_sibling;
            continue;
        }
        if (select_item_qualified_wildcard(item, &qualified_wildcard)) {
            rc = validate_grouped_qualified_wildcard_reference(
                database,
                qualified_wildcard,
                source_context
            );
        } else {
            rc = select_item_column_reference(item, &column_node);
            if (rc != MYLITE_OK) {
                set_unsupported_error(
                    database,
                    "GROUP BY supports selected descriptor group columns followed by aggregate "
                    "results"
                );
                return MYLITE_ERROR;
            }
            rc = resolve_descriptor_column_reference_with_source_index(
                database,
                column_node,
                source_context,
                COLUMN_REFERENCE_FIELD,
                "GROUP BY supports selected descriptor group columns followed by aggregate "
                "results",
                table_columns,
                table_column_count,
                &column,
                &source_index
            );
        }
        if (rc != MYLITE_OK) {
            return rc;
        }
        item = item->next_sibling;
    }

    return MYLITE_OK;
}

static int validate_grouped_qualified_wildcard_reference(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *qualified_wildcard,
    const struct select_source_context *source_context
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t part_count = 0U;
    bool matched = false;
    int rc = collect_qualified_wildcard_parts(database, qualified_wildcard, parts, &part_count);

    if (rc != MYLITE_OK) {
        return rc;
    }

    if (select_source_context_is_joined(source_context)) {
        for (size_t index = 0U; index < source_context->source_count; ++index) {
            if (joined_source_matches_qualified_wildcard_parts(
                    &source_context->sources[index],
                    parts,
                    part_count
                )) {
                matched = true;
                break;
            }
        }
    } else {
        matched = select_source_matches_qualified_wildcard_parts(
            source_context,
            (const char (*)[MYLITE_CATALOG_IDENTIFIER_CAPACITY])parts,
            part_count
        );
    }

    if (!matched) {
        return set_unknown_qualified_wildcard_table_error(database, parts, part_count);
    }
    return MYLITE_OK;
}

static int plan_grouped_aggregate_group_columns(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct mylite_sql_ast_node *group_clause,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
) {
    size_t group_count = grouped_aggregate_group_key_count(group_clause);
    int rc = MYLITE_OK;

    if (group_count == 0U) {
        set_unsupported_error(database, "GROUP BY requires one descriptor group column");
        return MYLITE_ERROR;
    }

    out_plan->groups = calloc(group_count, sizeof(*out_plan->groups));
    if (out_plan->groups == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_plan->group_count = group_count;

    for (size_t group_index = 0U; group_index < group_count; ++group_index) {
        rc = resolve_grouped_aggregate_group_column(
            database,
            select_list,
            child_at(group_clause, group_index),
            source_context,
            table_columns,
            table_column_count,
            &out_plan->groups[group_index]
        );
        if (rc != MYLITE_OK) {
            break;
        }
    }

    return rc;
}

static int resolve_grouped_aggregate_group_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct mylite_sql_ast_node *group_key,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_key *out_key
) {
    size_t source_column_match_count = 0U;
    bool matched_alias = false;
    int rc = count_grouped_source_columns_by_unqualified_name(
        database,
        group_key,
        source_context,
        table_columns,
        table_column_count,
        &source_column_match_count
    );

    *out_key = (struct planned_grouped_key){0};
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (source_column_match_count == 0U) {
        const struct grouped_alias_group_resolution alias_resolution = {
            .select_list = select_list,
            .group_key = group_key,
            .source_context = source_context,
            .table_columns = table_columns,
            .table_column_count = table_column_count,
        };

        rc = resolve_grouped_select_alias_group_column(
            database,
            &alias_resolution,
            out_key,
            &matched_alias
        );
        if (rc != MYLITE_OK || matched_alias) {
            return rc;
        }
    }

    rc = resolve_descriptor_column_reference_with_source_index(
        database,
        group_key,
        source_context,
        COLUMN_REFERENCE_GROUP,
        "GROUP BY supports only descriptor group columns",
        table_columns,
        table_column_count,
        &out_key->column,
        &out_key->column_source_index
    );
    if (rc == MYLITE_OK) {
        rc = validate_grouped_aggregate_group_column(database, &out_key->column);
    }
    if (rc == MYLITE_OK) {
        out_key->expression = group_key;
        out_key->alias = NULL;
    }

    return rc;
}

static int resolve_grouped_select_alias_group_column(
    struct mylite_db *database,
    const struct grouped_alias_group_resolution *resolution,
    struct planned_grouped_key *out_key,
    bool *out_matched
) {
    const struct mylite_sql_ast_node *item = NULL;
    const struct mylite_sql_ast_node *matched_item = NULL;
    const struct mylite_sql_ast_node *matched_expression = NULL;
    const struct mylite_sql_ast_node *matched_alias = NULL;
    bool matches_alias = false;
    int rc = MYLITE_OK;

    *out_matched = false;
    if (resolution->group_key == NULL || resolution->group_key->kind != MYLITE_SQL_AST_IDENTIFIER) {
        return MYLITE_OK;
    }

    item = child_at(resolution->select_list, 0U);
    while (item != NULL) {
        rc = order_identifier_matches_alias(
            database,
            resolution->group_key,
            child_at(item, 1U),
            &matches_alias
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (matches_alias) {
            if (matched_item != NULL) {
                char *alias_text = NULL;

                rc = copy_select_item_identifier_alias_text(
                    database,
                    resolution->group_key,
                    &alias_text
                );
                if (rc == MYLITE_OK) {
                    set_ambiguous_column_reference_error(
                        database,
                        COLUMN_REFERENCE_GROUP,
                        alias_text
                    );
                    rc = MYLITE_ERROR;
                }
                free(alias_text);
                return rc;
            }
            matched_item = item;
        }
        item = item->next_sibling;
    }
    if (matched_item == NULL) {
        return MYLITE_OK;
    }

    matched_expression = child_at(matched_item, 0U);
    matched_alias = child_at(matched_item, 1U);
    rc = select_item_column_reference(matched_item, &matched_expression);
    if (rc != MYLITE_OK) {
        set_unsupported_error(
            database,
            "GROUP BY supports selected descriptor-column aliases only"
        );
        return MYLITE_ERROR;
    }

    rc = resolve_descriptor_column_reference_with_source_index(
        database,
        matched_expression,
        resolution->source_context,
        COLUMN_REFERENCE_GROUP,
        "GROUP BY supports selected descriptor-column aliases only",
        resolution->table_columns,
        resolution->table_column_count,
        &out_key->column,
        &out_key->column_source_index
    );
    if (rc == MYLITE_OK) {
        rc = validate_grouped_aggregate_group_column(database, &out_key->column);
    }
    if (rc == MYLITE_OK) {
        out_key->expression = matched_expression;
        out_key->alias = matched_alias;
        *out_matched = true;
    }

    return rc;
}

static int count_grouped_source_columns_by_unqualified_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *group_key,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    size_t *out_match_count
) {
    char group_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t column_index = 0U;
    int rc = MYLITE_OK;

    *out_match_count = 0U;
    if (group_key == NULL || group_key->kind != MYLITE_SQL_AST_IDENTIFIER) {
        return MYLITE_OK;
    }

    rc = copy_identifier_text(group_key, group_name, sizeof(group_name), database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (select_source_context_is_joined(source_context)) {
        for (size_t source_index = 0U; source_index < source_context->source_count;
             ++source_index) {
            const struct planned_select_source *source = &source_context->sources[source_index];

            if (find_column_index(
                    source->columns,
                    source->column_count,
                    group_name,
                    &column_index
                ) == MYLITE_OK) {
                ++(*out_match_count);
            }
        }
        return MYLITE_OK;
    }
    if (find_column_index(table_columns, table_column_count, group_name, &column_index) ==
        MYLITE_OK) {
        *out_match_count = 1U;
    }

    return MYLITE_OK;
}

static size_t grouped_aggregate_group_key_count(const struct mylite_sql_ast_node *group_clause) {
    if (group_clause == NULL) {
        return 0U;
    }
    return mylite_sql_ast_node_child_count(group_clause);
}

static int validate_grouped_aggregate_group_column(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
) {
    struct integer_column_range range = {0};

    if (column_descriptor_is_string_family(column)) {
        return MYLITE_OK;
    }
    return integer_range_for_column(
        database,
        column,
        "GROUP BY supports only integer and nonbinary string descriptor group columns",
        &range
    );
}

static int plan_grouped_aggregate_projection_columns(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan,
    size_t *out_first_aggregate_index
) {
    const struct mylite_sql_ast_node *item = NULL;
    size_t item_index = 0U;
    int rc = MYLITE_OK;

    *out_first_aggregate_index = 0U;
    if (select_list_is_wildcard(select_list)) {
        rc = plan_grouped_aggregate_wildcard_projection_columns(
            database,
            1U,
            source_context,
            table_columns,
            table_column_count,
            out_plan
        );
        if (rc == MYLITE_OK) {
            *out_first_aggregate_index = 1U;
        }
        return rc;
    }

    item = child_at(select_list, 0U);
    while (item != NULL) {
        const struct mylite_sql_ast_node *expression = child_at(item, 0U);
        const struct mylite_sql_ast_node *column_node = NULL;
        const struct mylite_sql_ast_node *qualified_wildcard = NULL;
        struct mylite_catalog_column_descriptor column = {0};
        size_t source_index = 0U;
        size_t expression_index = out_plan->projection_count + 1U;

        if (grouped_aggregate_function_from_expression(
                unwrap_parenthesized_expression(expression)
            ) != PLANNED_GROUPED_AGGREGATE_NONE) {
            *out_first_aggregate_index = item_index;
            return MYLITE_OK;
        }
        if (expression != NULL && expression->kind == MYLITE_SQL_AST_WILDCARD &&
            child_at(item, 1U) == NULL) {
            rc = plan_grouped_aggregate_wildcard_projection_columns(
                database,
                expression_index,
                source_context,
                table_columns,
                table_column_count,
                out_plan
            );
        } else if (select_item_qualified_wildcard(item, &qualified_wildcard)) {
            rc = plan_grouped_aggregate_qualified_wildcard_projection_columns(
                database,
                qualified_wildcard,
                expression_index,
                source_context,
                table_columns,
                table_column_count,
                out_plan
            );
        } else {
            rc = select_item_column_reference(item, &column_node);
            if (rc != MYLITE_OK) {
                set_unsupported_error(
                    database,
                    "GROUP BY supports selected descriptor group columns followed by aggregate "
                    "results"
                );
                return MYLITE_ERROR;
            }
            rc = resolve_descriptor_column_reference_with_source_index(
                database,
                column_node,
                source_context,
                COLUMN_REFERENCE_FIELD,
                "GROUP BY supports selected descriptor group columns followed by aggregate "
                "results",
                table_columns,
                table_column_count,
                &column,
                &source_index
            );
            if (rc == MYLITE_OK) {
                rc = validate_grouped_projection_column(
                    database,
                    out_plan,
                    table_columns,
                    table_column_count,
                    &column,
                    source_index,
                    expression_index
                );
            }
            if (rc == MYLITE_OK) {
                rc = append_grouped_projection_column(
                    database,
                    out_plan,
                    &column,
                    source_index,
                    expression,
                    child_at(item, 1U)
                );
            }
        }
        if (rc != MYLITE_OK) {
            return rc;
        }
        ++item_index;
        item = item->next_sibling;
    }

    *out_first_aggregate_index = item_index;
    return MYLITE_OK;
}

static int append_grouped_projection_column(
    struct mylite_db *database,
    struct planned_grouped_aggregate *plan,
    const struct mylite_catalog_column_descriptor *column,
    size_t source_index,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node *alias
) {
    struct planned_grouped_projection *projections = NULL;
    size_t required_count = plan->projection_count + 1U;

    if (required_count > SIZE_MAX / sizeof(*projections)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    projections = realloc(plan->projections, required_count * sizeof(*projections));
    if (projections == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    plan->projections = projections;
    plan->projections[plan->projection_count] = (struct planned_grouped_projection){
        .expression = expression,
        .alias = alias,
        .column = *column,
        .column_source_index = source_index,
    };
    plan->projection_count = required_count;
    return MYLITE_OK;
}

static int plan_grouped_aggregate_wildcard_projection_columns(
    struct mylite_db *database,
    size_t select_expression_index,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
) {
    (void)select_expression_index;

    if (select_source_context_is_joined(source_context)) {
        for (size_t source_index = 0U; source_index < source_context->source_count;
             ++source_index) {
            const struct planned_select_source *source = &source_context->sources[source_index];
            int rc = append_visible_grouped_projection_columns_from_source(
                database,
                source->columns,
                source->column_count,
                source_index,
                out_plan
            );

            if (rc != MYLITE_OK) {
                return rc;
            }
        }
        return MYLITE_OK;
    }

    return append_visible_grouped_projection_columns_from_source(
        database,
        table_columns,
        table_column_count,
        0U,
        out_plan
    );
}

static int plan_grouped_aggregate_qualified_wildcard_projection_columns(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *qualified_wildcard,
    size_t select_expression_index,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    const struct mylite_catalog_column_descriptor *columns = table_columns;
    size_t column_count = table_column_count;
    size_t source_index = 0U;
    size_t part_count = 0U;
    bool matched = false;
    int rc = collect_qualified_wildcard_parts(database, qualified_wildcard, parts, &part_count);

    (void)select_expression_index;
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (select_source_context_is_joined(source_context)) {
        for (size_t index = 0U; index < source_context->source_count; ++index) {
            const struct planned_select_source *source = &source_context->sources[index];

            if (!joined_source_matches_qualified_wildcard_parts(source, parts, part_count)) {
                continue;
            }
            columns = source->columns;
            column_count = source->column_count;
            source_index = index;
            matched = true;
            break;
        }
    } else {
        matched = select_source_matches_qualified_wildcard_parts(
            source_context,
            (const char (*)[MYLITE_CATALOG_IDENTIFIER_CAPACITY])parts,
            part_count
        );
    }

    if (!matched) {
        return set_unknown_qualified_wildcard_table_error(database, parts, part_count);
    }

    return append_visible_grouped_projection_columns_from_source(
        database,
        columns,
        column_count,
        source_index,
        out_plan
    );
}

static int append_visible_grouped_projection_columns_from_source(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    size_t source_index,
    struct planned_grouped_aggregate *out_plan
) {
    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        size_t expression_index = out_plan->projection_count + 1U;
        int rc = MYLITE_OK;

        if (!columns[column_index].is_visible) {
            continue;
        }
        rc = validate_grouped_projection_column(
            database,
            out_plan,
            columns,
            column_count,
            &columns[column_index],
            source_index,
            expression_index
        );
        if (rc == MYLITE_OK) {
            rc = append_grouped_projection_column(
                database,
                out_plan,
                &columns[column_index],
                source_index,
                NULL,
                NULL
            );
        }
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return MYLITE_OK;
}

static int validate_grouped_projection_column(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct mylite_catalog_column_descriptor *column,
    size_t source_index,
    size_t select_expression_index
) {
    int rc = MYLITE_OK;

    if (grouped_projection_column_is_allowed(
            database,
            plan,
            table_columns,
            table_column_count,
            column,
            source_index,
            &rc
        )) {
        return MYLITE_OK;
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    set_only_full_group_by_error(
        database,
        select_expression_index,
        "SELECT list",
        planned_grouped_aggregate_source_resolution(plan, source_index),
        column
    );
    return MYLITE_ERROR;
}

static int grouped_source_primary_key_is_covered(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    size_t source_index,
    bool *out_covered
) {
    const struct mylite_catalog_column_descriptor *source_columns = NULL;
    size_t source_column_count = 0U;
    int64_t table_id = 0;
    struct primary_key_info primary_key = primary_key_info_init();
    int rc = MYLITE_OK;

    *out_covered = false;
    if (!grouped_source_columns(
            plan,
            source_index,
            table_columns,
            table_column_count,
            &source_columns,
            &source_column_count,
            &table_id
        )) {
        return MYLITE_ERROR;
    }

    rc = load_primary_key_info(
        database,
        table_id,
        source_columns,
        source_column_count,
        &primary_key
    );
    if (rc == MYLITE_OK && primary_key.has_primary_key) {
        *out_covered = true;
        for (size_t part_index = 0U; part_index < primary_key.part_count; ++part_index) {
            size_t group_index = 0U;

            if (!find_matching_grouped_key(
                    plan,
                    &primary_key.parts[part_index].column,
                    source_index,
                    &group_index
                )) {
                *out_covered = false;
                break;
            }
        }
    }

    primary_key_info_deinit(&primary_key);
    return rc;
}

static bool grouped_source_columns(
    const struct planned_grouped_aggregate *plan,
    size_t source_index,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct mylite_catalog_column_descriptor **out_columns,
    size_t *out_column_count,
    int64_t *out_table_id
) {
    if (plan->source_count == 0U) {
        if (source_index != 0U) {
            return false;
        }
        *out_columns = table_columns;
        *out_column_count = table_column_count;
        *out_table_id = plan->table.table_id;
        return true;
    }
    if (source_index >= plan->source_count) {
        return false;
    }

    *out_columns = plan->sources[source_index].columns;
    *out_column_count = plan->sources[source_index].column_count;
    *out_table_id = plan->sources[source_index].table.table_id;
    return true;
}

static bool grouped_projection_column_is_allowed(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct mylite_catalog_column_descriptor *column,
    size_t source_index,
    int *out_rc
) {
    size_t group_index = 0U;
    bool primary_key_covered = false;

    *out_rc = MYLITE_OK;
    if (find_matching_grouped_key(plan, column, source_index, &group_index)) {
        return true;
    }

    *out_rc = grouped_source_primary_key_is_covered(
        database,
        plan,
        table_columns,
        table_column_count,
        source_index,
        &primary_key_covered
    );
    if (*out_rc != MYLITE_OK) {
        return false;
    }
    return primary_key_covered;
}

static int plan_grouped_aggregate_functions(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    size_t first_aggregate_index,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
) {
    size_t select_item_count = mylite_sql_ast_node_child_count(select_list);
    size_t aggregate_count = select_item_count - first_aggregate_index;
    int rc = MYLITE_OK;

    if (aggregate_count > grouped_aggregate_max_results) {
        set_unsupported_error(database, "GROUP BY supports at most sixteen aggregate results");
        return MYLITE_ERROR;
    }
    if (aggregate_count > 0U) {
        out_plan->aggregates = calloc(aggregate_count, sizeof(*out_plan->aggregates));
        if (out_plan->aggregates == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
    }
    out_plan->aggregate_count = aggregate_count;

    for (size_t aggregate_index = 0U; aggregate_index < aggregate_count; ++aggregate_index) {
        rc = plan_grouped_aggregate_item(
            database,
            child_at(select_list, first_aggregate_index + aggregate_index),
            source_context,
            table_columns,
            table_column_count,
            out_plan,
            &out_plan->aggregates[aggregate_index]
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return MYLITE_OK;
}

static int plan_grouped_aggregate_item(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *aggregate_item,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan,
    struct planned_grouped_aggregate_item *out_item
) {
    const struct mylite_sql_ast_node *aggregate_expression =
        unwrap_parenthesized_expression(child_at(aggregate_item, 0U));
    struct mylite_catalog_column_descriptor selected_column = {0};
    size_t selected_source_index = 0U;
    int rc = MYLITE_OK;

    *out_item = (struct planned_grouped_aggregate_item){0};
    out_item->expression = child_at(aggregate_item, 0U);
    out_item->alias = child_at(aggregate_item, 1U);
    out_item->function = grouped_aggregate_function_from_expression(aggregate_expression);
    if (out_item->function == PLANNED_GROUPED_AGGREGATE_NONE) {
        const struct mylite_sql_ast_node *selected_column_node = NULL;

        rc = select_item_column_reference(aggregate_item, &selected_column_node);
        if (rc != MYLITE_OK) {
            set_unsupported_error(
                database,
                "GROUP BY supports selected descriptor group columns followed by aggregate results"
            );
            return MYLITE_ERROR;
        }
        rc = resolve_descriptor_column_reference_with_source_index(
            database,
            selected_column_node,
            source_context,
            COLUMN_REFERENCE_FIELD,
            "GROUP BY supports selected descriptor group columns followed by aggregate results",
            table_columns,
            table_column_count,
            &selected_column,
            &selected_source_index
        );
        if (rc == MYLITE_OK) {
            set_unsupported_error(
                database,
                "GROUP BY supports selected descriptor group columns followed by aggregate results"
            );
            return MYLITE_ERROR;
        }
        return rc;
    }
    if (!grouped_aggregate_function_has_column(out_item->function)) {
        return MYLITE_OK;
    }
    if (out_item->function == PLANNED_GROUPED_AGGREGATE_GROUP_CONCAT) {
        if (out_plan->aggregate_count != 1U) {
            set_unsupported_error(
                database,
                "GROUP_CONCAT(column) does not yet support multiple grouped aggregate results"
            );
            return MYLITE_ERROR;
        }
        if (out_plan->source_count > 0U) {
            set_unsupported_error(
                database,
                "GROUP_CONCAT(column) does not yet support joined GROUP BY sources"
            );
            return MYLITE_ERROR;
        }
        return plan_grouped_group_concat_options(
            database,
            aggregate_expression,
            source_context,
            table_columns,
            table_column_count,
            out_item
        );
    }
    if (out_item->function == PLANNED_GROUPED_AGGREGATE_COUNT_COLUMN) {
        return plan_grouped_count_column(
            database,
            aggregate_expression,
            source_context,
            table_columns,
            table_column_count,
            &out_item->aggregate_column,
            &out_item->aggregate_column_source_index
        );
    }
    if (out_item->function == PLANNED_GROUPED_AGGREGATE_ANY_VALUE) {
        return plan_grouped_any_value_column(
            database,
            aggregate_expression,
            source_context,
            table_columns,
            table_column_count,
            &out_item->aggregate_column,
            &out_item->aggregate_column_source_index
        );
    }

    return plan_grouped_column_aggregate_column(
        database,
        aggregate_expression,
        grouped_column_aggregate_function(out_item->function),
        source_context,
        table_columns,
        table_column_count,
        &out_item->aggregate_column,
        &out_item->aggregate_column_source_index
    );
}

static int plan_grouped_any_value_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column,
    size_t *out_source_index
) {
    const struct mylite_sql_ast_node *column_node = child_at(function, 0U);

    return resolve_descriptor_column_reference_with_source_index(
        database,
        column_node,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "ANY_VALUE(column) supports only descriptor columns",
        table_columns,
        table_column_count,
        out_column,
        out_source_index
    );
}

static int plan_grouped_count_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column,
    size_t *out_source_index
) {
    const struct mylite_sql_ast_node *column_node = child_at(function, 0U);

    return resolve_descriptor_column_reference_with_source_index(
        database,
        column_node,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "COUNT(column) supports only descriptor columns",
        table_columns,
        table_column_count,
        out_column,
        out_source_index
    );
}

static int plan_grouped_column_aggregate_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    enum planned_column_aggregate_function aggregate_function,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column,
    size_t *out_source_index
) {
    const struct mylite_sql_ast_node *column_node = child_at(function, 0U);
    struct integer_column_range range;
    int rc = resolve_descriptor_column_reference_with_source_index(
        database,
        column_node,
        source_context,
        COLUMN_REFERENCE_FIELD,
        column_aggregate_column_error(aggregate_function),
        table_columns,
        table_column_count,
        out_column,
        out_source_index
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    return integer_range_for_column(
        database,
        out_column,
        column_aggregate_integer_error(aggregate_function),
        &range
    );
}

static int plan_grouped_group_concat_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate_item *out_item
) {
    int rc = plan_group_concat_value_column(
        database,
        function,
        source_context,
        table_columns,
        table_column_count,
        &out_item->aggregate_column,
        &out_item->aggregate_column_source_index
    );

    if (rc == MYLITE_OK) {
        rc = plan_group_concat_order_column(
            database,
            function,
            source_context,
            table_columns,
            table_column_count,
            false,
            &out_item->aggregate_order
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_group_concat_separator(
            database,
            function,
            &out_item->has_separator,
            &out_item->separator
        );
    }

    return rc;
}

static int plan_group_concat_value_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column,
    size_t *out_source_index
) {
    const struct mylite_sql_ast_node *column_node = group_concat_value_node(function);
    struct integer_column_range range = {0};
    int rc = resolve_descriptor_column_reference_with_source_index(
        database,
        column_node,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "GROUP_CONCAT(column) supports only descriptor columns",
        table_columns,
        table_column_count,
        out_column,
        out_source_index
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (column_descriptor_is_string_family(out_column)) {
        return MYLITE_OK;
    }
    return integer_range_for_column(
        database,
        out_column,
        "GROUP_CONCAT(column) supports only integer and nonbinary string descriptor columns",
        &range
    );
}

static int plan_group_concat_order_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    bool qualify_column_reference,
    struct planned_select_order *out_order
) {
    const struct mylite_sql_ast_node *order_clause = group_concat_order_node(function);
    const struct mylite_sql_ast_node *direction = NULL;
    struct integer_column_range range = {0};
    int rc = MYLITE_OK;

    *out_order = (struct planned_select_order){
        .has_order = false,
        .direction = PLANNED_SELECT_ORDER_DEFAULT,
        .column_source_index = 0U,
        .qualify_column_reference = qualify_column_reference,
    };
    if (order_clause == NULL) {
        return MYLITE_OK;
    }
    if (order_clause->kind != MYLITE_SQL_AST_ORDER_BY_CLAUSE) {
        set_unsupported_error(
            database,
            "GROUP_CONCAT ORDER BY supports only one descriptor column"
        );
        return MYLITE_ERROR;
    }

    rc = resolve_descriptor_column_reference_with_source_index(
        database,
        child_at(order_clause, 0U),
        source_context,
        COLUMN_REFERENCE_ORDER,
        "GROUP_CONCAT ORDER BY supports only one descriptor column",
        table_columns,
        table_column_count,
        &out_order->column,
        &out_order->column_source_index
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_order->column.is_nullable) {
        set_unsupported_error(
            database,
            "GROUP_CONCAT ORDER BY supports only NOT NULL descriptor columns"
        );
        return MYLITE_ERROR;
    }
    if (!column_descriptor_is_bit(&out_order->column) &&
        !column_descriptor_is_year(&out_order->column) &&
        !column_descriptor_is_date(&out_order->column) &&
        !column_descriptor_is_time(&out_order->column) &&
        !column_descriptor_is_datetime(&out_order->column) &&
        !column_descriptor_is_timestamp(&out_order->column)) {
        rc = integer_range_for_column(
            database,
            &out_order->column,
            "GROUP_CONCAT ORDER BY supports only NOT NULL integer, BIT, YEAR, DATE, TIME, "
            "DATETIME, or TIMESTAMP descriptor columns",
            &range
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    out_order->has_order = true;
    out_order->direction = PLANNED_SELECT_ORDER_ASC;
    direction = child_at(order_clause, 1U);
    if (mylite_sql_ast_node_order_direction(direction) == MYLITE_SQL_AST_ORDER_DIRECTION_DESC) {
        out_order->direction = PLANNED_SELECT_ORDER_DESC;
    }
    return MYLITE_OK;
}

static int plan_group_concat_separator(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    bool *out_has_separator,
    struct planned_value *out_separator
) {
    const struct mylite_sql_ast_node *separator = group_concat_separator_node(function);
    char *text = NULL;
    size_t text_length = 0U;
    size_t character_count = 0U;
    int rc = MYLITE_OK;

    *out_has_separator = false;
    *out_separator = (struct planned_value){.is_null = false};
    if (separator == NULL) {
        return MYLITE_OK;
    }

    rc = decode_sql_string_literal(
        database,
        separator,
        "GROUP_CONCAT SEPARATOR supports only string literals",
        "GROUP_CONCAT SEPARATOR does not support NUL bytes",
        &text,
        &text_length
    );
    if (rc == MYLITE_OK && validate_utf8_text(text, text_length, &character_count) != MYLITE_OK) {
        set_unsupported_error(database, "GROUP_CONCAT SEPARATOR supports only valid UTF-8 text");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = assign_text_value(database, text, text_length, out_separator);
        text = NULL;
        *out_has_separator = true;
    }

    (void)character_count;
    free(text);
    return rc;
}

static const struct mylite_sql_ast_node *group_concat_value_node(
    const struct mylite_sql_ast_node *function
) {
    return child_at(function, 0U);
}

static const struct mylite_sql_ast_node *group_concat_order_node(
    const struct mylite_sql_ast_node *function
) {
    const struct mylite_sql_ast_node *child = child_at(function, 1U);

    while (child != NULL) {
        if (child->kind == MYLITE_SQL_AST_ORDER_BY_CLAUSE) {
            return child;
        }
        child = child->next_sibling;
    }
    return NULL;
}

static const struct mylite_sql_ast_node *group_concat_separator_node(
    const struct mylite_sql_ast_node *function
) {
    const struct mylite_sql_ast_node *child = child_at(function, 1U);

    while (child != NULL) {
        if (child->kind == MYLITE_SQL_AST_LITERAL &&
            mylite_sql_ast_node_literal_kind(child) == MYLITE_SQL_AST_LITERAL_STRING) {
            return child;
        }
        child = child->next_sibling;
    }
    return NULL;
}

static enum planned_grouped_aggregate_function grouped_aggregate_function_from_expression(
    const struct mylite_sql_ast_node *expression
) {
    enum planned_count_function count_function = count_function_from_expression(expression);
    enum planned_column_aggregate_function column_function =
        column_aggregate_function_from_expression(expression);

    if (count_function == PLANNED_COUNT_STAR) {
        return PLANNED_GROUPED_AGGREGATE_COUNT_STAR;
    }
    if (count_function == PLANNED_COUNT_COLUMN) {
        return PLANNED_GROUPED_AGGREGATE_COUNT_COLUMN;
    }
    if (count_function == PLANNED_COUNT_LITERAL ||
        count_function == PLANNED_COUNT_DISTINCT_COLUMN) {
        return PLANNED_GROUPED_AGGREGATE_NONE;
    }

    switch (column_function) {
    case PLANNED_COLUMN_AGGREGATE_MIN:
        return PLANNED_GROUPED_AGGREGATE_MIN;
    case PLANNED_COLUMN_AGGREGATE_MAX:
        return PLANNED_GROUPED_AGGREGATE_MAX;
    case PLANNED_COLUMN_AGGREGATE_SUM:
        return PLANNED_GROUPED_AGGREGATE_SUM;
    case PLANNED_COLUMN_AGGREGATE_AVG:
        return PLANNED_GROUPED_AGGREGATE_AVG;
    case PLANNED_COLUMN_AGGREGATE_BIT_AND:
        return PLANNED_GROUPED_AGGREGATE_BIT_AND;
    case PLANNED_COLUMN_AGGREGATE_BIT_OR:
        return PLANNED_GROUPED_AGGREGATE_BIT_OR;
    case PLANNED_COLUMN_AGGREGATE_BIT_XOR:
        return PLANNED_GROUPED_AGGREGATE_BIT_XOR;
    case PLANNED_COLUMN_AGGREGATE_GROUP_CONCAT:
        return PLANNED_GROUPED_AGGREGATE_GROUP_CONCAT;
    case PLANNED_COLUMN_AGGREGATE_NONE:
        break;
    }
    if (expression != NULL && expression->kind == MYLITE_SQL_AST_ANY_VALUE_FUNCTION) {
        return PLANNED_GROUPED_AGGREGATE_ANY_VALUE;
    }

    return PLANNED_GROUPED_AGGREGATE_NONE;
}

static enum planned_column_aggregate_function grouped_column_aggregate_function(
    enum planned_grouped_aggregate_function function
) {
    switch (function) {
    case PLANNED_GROUPED_AGGREGATE_MIN:
        return PLANNED_COLUMN_AGGREGATE_MIN;
    case PLANNED_GROUPED_AGGREGATE_MAX:
        return PLANNED_COLUMN_AGGREGATE_MAX;
    case PLANNED_GROUPED_AGGREGATE_SUM:
        return PLANNED_COLUMN_AGGREGATE_SUM;
    case PLANNED_GROUPED_AGGREGATE_AVG:
        return PLANNED_COLUMN_AGGREGATE_AVG;
    case PLANNED_GROUPED_AGGREGATE_BIT_AND:
        return PLANNED_COLUMN_AGGREGATE_BIT_AND;
    case PLANNED_GROUPED_AGGREGATE_BIT_OR:
        return PLANNED_COLUMN_AGGREGATE_BIT_OR;
    case PLANNED_GROUPED_AGGREGATE_BIT_XOR:
        return PLANNED_COLUMN_AGGREGATE_BIT_XOR;
    case PLANNED_GROUPED_AGGREGATE_GROUP_CONCAT:
        return PLANNED_COLUMN_AGGREGATE_GROUP_CONCAT;
    case PLANNED_GROUPED_AGGREGATE_NONE:
    case PLANNED_GROUPED_AGGREGATE_COUNT_STAR:
    case PLANNED_GROUPED_AGGREGATE_COUNT_COLUMN:
    case PLANNED_GROUPED_AGGREGATE_ANY_VALUE:
        return PLANNED_COLUMN_AGGREGATE_NONE;
    }

    return PLANNED_COLUMN_AGGREGATE_NONE;
}

static bool grouped_aggregate_function_has_column(
    enum planned_grouped_aggregate_function function
) {
    if (function == PLANNED_GROUPED_AGGREGATE_NONE) {
        return false;
    }
    if (function == PLANNED_GROUPED_AGGREGATE_COUNT_STAR) {
        return false;
    }

    return true;
}

static bool grouped_aggregate_function_is_bitwise(
    enum planned_grouped_aggregate_function function
) {
    if (function == PLANNED_GROUPED_AGGREGATE_BIT_AND) {
        return true;
    }
    if (function == PLANNED_GROUPED_AGGREGATE_BIT_OR) {
        return true;
    }
    if (function == PLANNED_GROUPED_AGGREGATE_BIT_XOR) {
        return true;
    }

    return false;
}

static bool planned_grouped_aggregate_columns_match(
    const struct mylite_catalog_column_descriptor *left_column,
    size_t left_source_index,
    const struct mylite_catalog_column_descriptor *right_column,
    size_t right_source_index
) {
    return (left_column->column_id == right_column->column_id &&
            left_source_index == right_source_index) != 0;
}

static bool find_matching_grouped_key(
    const struct planned_grouped_aggregate *plan,
    const struct mylite_catalog_column_descriptor *column,
    size_t source_index,
    size_t *out_group_index
) {
    for (size_t group_index = 0U; group_index < plan->group_count; ++group_index) {
        if (planned_grouped_aggregate_columns_match(
                column,
                source_index,
                &plan->groups[group_index].column,
                plan->groups[group_index].column_source_index
            )) {
            *out_group_index = group_index;
            return true;
        }
    }

    return false;
}

static bool find_grouped_key_by_unqualified_name(
    const struct planned_grouped_aggregate *plan,
    const char *name,
    size_t *out_group_index,
    bool *out_ambiguous
) {
    bool matched = false;

    *out_ambiguous = false;
    for (size_t group_index = 0U; group_index < plan->group_count; ++group_index) {
        if (text_equals_ascii_case_insensitive(name, plan->groups[group_index].column.name)) {
            if (matched) {
                *out_ambiguous = true;
                return false;
            }
            *out_group_index = group_index;
            matched = true;
        }
    }

    return matched;
}

static bool find_grouped_projection_by_unqualified_name(
    const struct planned_grouped_aggregate *plan,
    const char *name,
    size_t *out_projection_index,
    bool *out_ambiguous
) {
    bool matched = false;

    *out_ambiguous = false;
    for (size_t projection_index = 0U; projection_index < plan->projection_count;
         ++projection_index) {
        if (text_equals_ascii_case_insensitive(
                name,
                plan->projections[projection_index].column.name
            )) {
            if (matched) {
                *out_ambiguous = true;
                return false;
            }
            *out_projection_index = projection_index;
            matched = true;
        }
    }

    return matched;
}

static const struct table_name_resolution *planned_grouped_aggregate_source_resolution(
    const struct planned_grouped_aggregate *plan,
    size_t source_index
) {
    if (plan->source_count == 0U) {
        return &plan->source;
    }
    if (source_index >= plan->source_count) {
        return &plan->sources[0].source;
    }
    return &plan->sources[source_index].source;
}

static int plan_grouped_aggregate_having(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *having_clause,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
) {
    out_plan->having = (struct planned_grouped_having){0};
    if (having_clause == NULL) {
        return MYLITE_OK;
    }
    if (having_clause->kind != MYLITE_SQL_AST_HAVING_CLAUSE) {
        set_unsupported_error(database, "HAVING supports only one grouped predicate");
        return MYLITE_ERROR;
    }

    return plan_grouped_having_node(
        database,
        child_at(having_clause, 0U),
        source_context,
        table_columns,
        table_column_count,
        out_plan
    );
}

static int plan_grouped_having_node(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *having_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
) {
    const struct mylite_sql_ast_node *current = having_node;
    enum planned_grouped_having_operand operand = PLANNED_GROUPED_HAVING_OPERAND_NONE;
    int rc = MYLITE_OK;

    while (current != NULL && current->kind == MYLITE_SQL_AST_PARENTHESIZED_EXPRESSION) {
        current = child_at(current, 0U);
    }
    if (current == NULL) {
        set_unsupported_error(database, "HAVING supports only one grouped predicate");
        return MYLITE_ERROR;
    }
    if (current->kind != MYLITE_SQL_AST_COMPARISON_PREDICATE &&
        current->kind != MYLITE_SQL_AST_IS_NULL_PREDICATE) {
        set_unsupported_error(database, "HAVING supports only one grouped predicate");
        return MYLITE_ERROR;
    }

    rc = plan_grouped_having_operand(
        database,
        child_at(current, 0U),
        source_context,
        table_columns,
        table_column_count,
        out_plan,
        &operand
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    out_plan->having.operand = operand;
    out_plan->having.operator_kind = mylite_sql_ast_node_operator(current);
    if (current->kind == MYLITE_SQL_AST_IS_NULL_PREDICATE) {
        out_plan->having.kind = PLANNED_GROUPED_HAVING_IS_NULL;
        return MYLITE_OK;
    }

    rc = convert_grouped_having_integer_literal(
        database,
        child_at(current, 1U),
        out_plan,
        operand,
        &out_plan->having.value
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    out_plan->having.kind = PLANNED_GROUPED_HAVING_COMPARISON;
    return MYLITE_OK;
}

static int plan_grouped_having_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *operand_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan,
    enum planned_grouped_having_operand *out_operand
) {
    const struct mylite_sql_ast_node *operand = unwrap_parenthesized_expression(operand_node);
    int rc = MYLITE_OK;

    *out_operand = PLANNED_GROUPED_HAVING_OPERAND_NONE;
    if (operand == NULL) {
        set_unsupported_error(database, "HAVING supports only grouped columns or aggregates");
        return MYLITE_ERROR;
    }
    if (operand->kind == MYLITE_SQL_AST_IDENTIFIER ||
        operand->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return plan_grouped_having_identifier_operand(
            database,
            operand,
            source_context,
            table_columns,
            table_column_count,
            out_plan,
            out_operand
        );
    }

    rc = plan_grouped_having_aggregate_operand(
        database,
        operand,
        source_context,
        table_columns,
        table_column_count,
        out_plan
    );
    if (rc == MYLITE_OK) {
        *out_operand = PLANNED_GROUPED_HAVING_OPERAND_AGGREGATE;
    }

    return rc;
}

static int plan_grouped_having_identifier_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *operand_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan,
    enum planned_grouped_having_operand *out_operand
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char column_name[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    size_t part_count = 0U;
    size_t column_index = 0U;
    size_t group_index = 0U;
    bool ambiguous_group_key = false;
    bool matches_alias = false;
    struct mylite_catalog_column_descriptor resolved_column = {0};
    size_t resolved_source_index = 0U;
    int rc = collect_column_reference_parts(database, operand_node, parts, &part_count);

    if (rc == MYLITE_OK) {
        rc = format_column_reference_name(
            database,
            parts,
            part_count,
            column_name,
            sizeof(column_name)
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (part_count > 1U) {
        rc = resolve_descriptor_column_reference_with_source_index(
            database,
            operand_node,
            source_context,
            COLUMN_REFERENCE_HAVING,
            "HAVING supports only grouped columns or selected aggregate aliases",
            table_columns,
            table_column_count,
            &resolved_column,
            &resolved_source_index
        );
        if (rc == MYLITE_OK && find_matching_grouped_key(
                                   out_plan,
                                   &resolved_column,
                                   resolved_source_index,
                                   &group_index
                               )) {
            out_plan->having.group_index = group_index;
            *out_operand = PLANNED_GROUPED_HAVING_OPERAND_GROUP_COLUMN;
            return MYLITE_OK;
        }
        if (rc == MYLITE_OK) {
            set_unknown_having_column_error(database, column_name);
            rc = MYLITE_ERROR;
        }
        return rc;
    }

    if (find_grouped_key_by_unqualified_name(
            out_plan,
            parts[0],
            &group_index,
            &ambiguous_group_key
        )) {
        out_plan->having.group_index = group_index;
        *out_operand = PLANNED_GROUPED_HAVING_OPERAND_GROUP_COLUMN;
        return MYLITE_OK;
    }
    if (ambiguous_group_key) {
        set_ambiguous_column_reference_error(database, COLUMN_REFERENCE_HAVING, parts[0]);
        return MYLITE_ERROR;
    }

    rc = find_grouped_having_aggregate_alias_operand(
        database,
        operand_node,
        out_plan,
        &matches_alias
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (matches_alias) {
        *out_operand = PLANNED_GROUPED_HAVING_OPERAND_AGGREGATE;
        return MYLITE_OK;
    }

    rc = find_grouped_having_projection_alias_operand(
        database,
        operand_node,
        column_name,
        out_plan,
        &matches_alias,
        &group_index
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (matches_alias) {
        out_plan->having.group_index = group_index;
        *out_operand = PLANNED_GROUPED_HAVING_OPERAND_GROUP_COLUMN;
        return MYLITE_OK;
    }

    rc = find_grouped_having_group_alias_operand(
        database,
        operand_node,
        out_plan,
        &matches_alias,
        &group_index
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (matches_alias) {
        out_plan->having.group_index = group_index;
        *out_operand = PLANNED_GROUPED_HAVING_OPERAND_GROUP_COLUMN;
        return MYLITE_OK;
    }

    if (find_column_index(table_columns, table_column_count, parts[0], &column_index) ==
        MYLITE_OK) {
        set_unknown_having_column_error(database, column_name);
        return MYLITE_ERROR;
    }

    set_unknown_having_column_error(database, column_name);
    return MYLITE_ERROR;
}

static int find_grouped_having_projection_alias_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *operand_node,
    const char *column_name,
    const struct planned_grouped_aggregate *plan,
    bool *out_matched,
    size_t *out_group_index
) {
    bool matches_alias = false;
    bool has_match = false;
    size_t matched_group_index = 0U;
    int rc = MYLITE_OK;

    *out_matched = false;
    *out_group_index = 0U;
    for (size_t projection_index = 0U; projection_index < plan->projection_count;
         ++projection_index) {
        const struct planned_grouped_projection *projection = &plan->projections[projection_index];

        rc = order_identifier_matches_alias(
            database,
            operand_node,
            projection->alias,
            &matches_alias
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (!matches_alias) {
            continue;
        }
        if (has_match) {
            set_unsupported_error(database, "HAVING supports only unique selected group aliases");
            return MYLITE_ERROR;
        }
        if (!find_matching_grouped_key(
                plan,
                &projection->column,
                projection->column_source_index,
                &matched_group_index
            )) {
            set_unknown_having_column_error(database, column_name);
            return MYLITE_ERROR;
        }
        has_match = true;
    }
    if (!has_match) {
        return MYLITE_OK;
    }

    *out_matched = true;
    *out_group_index = matched_group_index;
    return MYLITE_OK;
}

static int find_grouped_having_group_alias_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *operand_node,
    const struct planned_grouped_aggregate *plan,
    bool *out_matched,
    size_t *out_group_index
) {
    bool matches_alias = false;
    bool has_match = false;
    size_t matched_group_index = 0U;
    int rc = MYLITE_OK;

    *out_matched = false;
    *out_group_index = 0U;
    for (size_t group_index = 0U; group_index < plan->group_count; ++group_index) {
        rc = order_identifier_matches_alias(
            database,
            operand_node,
            plan->groups[group_index].alias,
            &matches_alias
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (!matches_alias) {
            continue;
        }
        if (has_match) {
            set_unsupported_error(database, "HAVING supports only unique selected group aliases");
            return MYLITE_ERROR;
        }
        matched_group_index = group_index;
        has_match = true;
    }
    if (!has_match) {
        return MYLITE_OK;
    }

    *out_matched = true;
    *out_group_index = matched_group_index;
    return MYLITE_OK;
}

static int plan_grouped_having_aggregate_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *operand_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
) {
    enum planned_grouped_aggregate_function function =
        grouped_aggregate_function_from_expression(operand_node);
    struct mylite_catalog_column_descriptor column = {0};
    struct integer_column_range range = {0};
    size_t source_index = 0U;
    size_t selected_index = SIZE_MAX;
    int rc = MYLITE_OK;

    if (function == PLANNED_GROUPED_AGGREGATE_NONE) {
        set_unsupported_error(database, "HAVING supports only grouped columns or aggregates");
        return MYLITE_ERROR;
    }
    if (grouped_aggregate_function_is_bitwise(function)) {
        set_unsupported_error(database, "HAVING does not support bitwise aggregate predicates");
        return MYLITE_ERROR;
    }
    if (function == PLANNED_GROUPED_AGGREGATE_GROUP_CONCAT) {
        set_unsupported_error(
            database,
            "HAVING does not support GROUP_CONCAT aggregate predicates"
        );
        return MYLITE_ERROR;
    }
    if (!grouped_aggregate_function_has_column(function)) {
        for (size_t aggregate_index = 0U; aggregate_index < out_plan->aggregate_count;
             ++aggregate_index) {
            if (out_plan->aggregates[aggregate_index].function == function) {
                out_plan->having.aggregate_index = aggregate_index;
                return MYLITE_OK;
            }
        }

        set_unsupported_error(database, "HAVING supports only the selected aggregate result");
        return MYLITE_ERROR;
    }

    rc = resolve_descriptor_column_reference_with_source_index(
        database,
        child_at(operand_node, 0U),
        source_context,
        COLUMN_REFERENCE_HAVING,
        "HAVING supports only descriptor aggregate columns",
        table_columns,
        table_column_count,
        &column,
        &source_index
    );
    if (rc == MYLITE_OK && function != PLANNED_GROUPED_AGGREGATE_COUNT_COLUMN) {
        rc = integer_range_for_column(
            database,
            &column,
            "HAVING supports only integer aggregate columns",
            &range
        );
    }
    for (size_t aggregate_index = 0U;
         rc == MYLITE_OK && aggregate_index < out_plan->aggregate_count;
         ++aggregate_index) {
        const struct planned_grouped_aggregate_item *item = &out_plan->aggregates[aggregate_index];

        if (item->function == function && planned_grouped_aggregate_columns_match(
                                              &column,
                                              source_index,
                                              &item->aggregate_column,
                                              item->aggregate_column_source_index
                                          )) {
            selected_index = aggregate_index;
            break;
        }
    }
    if (rc == MYLITE_OK && selected_index == SIZE_MAX) {
        set_unsupported_error(database, "HAVING supports only the selected aggregate result");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        out_plan->having.aggregate_index = selected_index;
    }

    return rc;
}

static int find_grouped_having_aggregate_alias_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *operand_node,
    struct planned_grouped_aggregate *out_plan,
    bool *out_matched
) {
    bool matches_alias = false;
    size_t matched_index = SIZE_MAX;
    int rc = MYLITE_OK;

    *out_matched = false;
    for (size_t aggregate_index = 0U; aggregate_index < out_plan->aggregate_count;
         ++aggregate_index) {
        const struct planned_grouped_aggregate_item *item = &out_plan->aggregates[aggregate_index];

        rc = order_identifier_matches_alias(database, operand_node, item->alias, &matches_alias);
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (!matches_alias) {
            continue;
        }
        if (matched_index != SIZE_MAX) {
            set_unsupported_error(
                database,
                "HAVING supports only unique selected aggregate aliases"
            );
            return MYLITE_ERROR;
        }
        matched_index = aggregate_index;
    }
    if (matched_index == SIZE_MAX) {
        return MYLITE_OK;
    }

    if (grouped_aggregate_function_is_bitwise(out_plan->aggregates[matched_index].function)) {
        set_unsupported_error(database, "HAVING does not support bitwise aggregate predicates");
        return MYLITE_ERROR;
    }
    if (out_plan->aggregates[matched_index].function == PLANNED_GROUPED_AGGREGATE_GROUP_CONCAT) {
        set_unsupported_error(
            database,
            "HAVING does not support GROUP_CONCAT aggregate predicates"
        );
        return MYLITE_ERROR;
    }

    out_plan->having.aggregate_index = matched_index;
    *out_matched = true;
    return MYLITE_OK;
}

static int convert_grouped_having_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct planned_grouped_aggregate *plan,
    enum planned_grouped_having_operand operand,
    struct planned_value *out_value
) {
    const struct mylite_catalog_column_descriptor *group_column =
        plan->having.group_index < plan->group_count
            ? &plan->groups[plan->having.group_index].column
            : NULL;
    const char *operand_name =
        (operand == PLANNED_GROUPED_HAVING_OPERAND_GROUP_COLUMN && group_column != NULL)
            ? group_column->name
            : "aggregate";
    bool is_negative = false;
    uint64_t magnitude = 0U;
    int rc = parse_grouped_having_integer_literal(
        database,
        value_node,
        operand_name,
        &is_negative,
        &magnitude
    );

    *out_value = (struct planned_value){.is_null = false, .integer = 0};
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (operand == PLANNED_GROUPED_HAVING_OPERAND_GROUP_COLUMN) {
        return convert_grouped_having_group_value(
            database,
            magnitude,
            is_negative,
            group_column,
            out_value
        );
    }

    return convert_grouped_having_aggregate_value(
        database,
        magnitude,
        is_negative,
        operand_name,
        out_value
    );
}

static int convert_date_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(
            database,
            "DATE values support only string, NULL, and DEFAULT values"
        );
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        value_node,
        "DATE values support only string literals",
        "DATE values do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    return convert_relaxed_date_storage_text(
        database,
        text,
        text_length,
        column->name,
        row_number,
        ignore_errors,
        out_value
    );
}

static int convert_relaxed_date_storage_text(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    const char *column_name,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    char normalized[date_text_length + 1U];
    char *copy = NULL;
    struct temporal_predicate_normalization_input normalization = {0};
    enum temporal_storage_truncation truncation = TEMPORAL_STORAGE_TRUNCATION_NONE;
    int rc = MYLITE_OK;

    normalization.text = text;
    normalization.text_length = text_length;
    normalization.target_offset_minutes =
        database == NULL ? 0 : database->session.time_zone_offset_minutes;
    if (normalize_date_storage_text(&normalization, normalized, &truncation)) {
        if (truncation == TEMPORAL_STORAGE_TRUNCATION_WARNING && !ignore_errors &&
            temporal_sql_mode_is_strict(database)) {
            set_incorrect_date_value_error(database, text, column_name, row_number);
            free(text);
            return MYLITE_ERROR;
        }
        copy = copy_temporal_text(database, normalized, date_text_length);
        if (copy == NULL) {
            free(text);
            return MYLITE_NOMEM;
        }
        rc = canonicalize_date_text(
            database,
            copy,
            date_text_length,
            column_name,
            row_number,
            ignore_errors,
            out_value
        );
        if (rc != MYLITE_OK) {
            free(text);
            return rc;
        }
        if (truncation == TEMPORAL_STORAGE_TRUNCATION_NOTE) {
            if (temporal_sql_mode_is_strict(database)) {
                rc = append_incorrect_date_value_note(database, text, column_name, row_number);
            } else {
                rc = append_data_truncated_note(database, column_name, row_number);
            }
        } else if (truncation == TEMPORAL_STORAGE_TRUNCATION_WARNING) {
            rc = append_data_truncated_warning(database, column_name, row_number);
        }
        free(text);
        return rc;
    }

    return canonicalize_date_text(
        database,
        text,
        text_length,
        column_name,
        row_number,
        ignore_errors,
        out_value
    );
}

static bool normalize_date_storage_text(
    const struct temporal_predicate_normalization_input *input,
    char *out_text,
    enum temporal_storage_truncation *out_truncation
) {
    char normalized_datetime[datetime_text_length + 1U];
    const char *text = input == NULL ? NULL : input->text;

    if (input == NULL || text == NULL || out_text == NULL || out_truncation == NULL) {
        return false;
    }

    if (input->text_length == datetime_text_length &&
        (text[datetime_date_time_separator_offset] == ' ' ||
         text[datetime_date_time_separator_offset] == 'T')) {
        memcpy(normalized_datetime, text, datetime_text_length);
        normalized_datetime[datetime_date_time_separator_offset] = ' ';
        normalized_datetime[datetime_text_length] = '\0';
        if (!datetime_text_has_canonical_shape(normalized_datetime, datetime_text_length)) {
            return false;
        }
        memcpy(out_text, normalized_datetime, date_text_length);
        out_text[date_text_length] = '\0';
        if (datetime_storage_time_is_midnight(text)) {
            *out_truncation = TEMPORAL_STORAGE_TRUNCATION_NONE;
        } else {
            *out_truncation = TEMPORAL_STORAGE_TRUNCATION_NOTE;
        }
        return date_text_has_canonical_shape(out_text, date_text_length);
    }

    if (normalize_iso_temporal_predicate_text(input, normalized_datetime)) {
        memcpy(out_text, normalized_datetime, date_text_length);
        out_text[date_text_length] = '\0';
        if (datetime_storage_time_is_midnight(text)) {
            *out_truncation = TEMPORAL_STORAGE_TRUNCATION_NONE;
        } else {
            *out_truncation = TEMPORAL_STORAGE_TRUNCATION_NOTE;
        }
        return date_text_has_canonical_shape(out_text, date_text_length);
    }
    if (!normalize_z_temporal_predicate_text(text, input->text_length, normalized_datetime)) {
        return false;
    }

    memcpy(out_text, normalized_datetime, date_text_length);
    out_text[date_text_length] = '\0';
    *out_truncation = TEMPORAL_STORAGE_TRUNCATION_WARNING;
    return date_text_has_canonical_shape(out_text, date_text_length);
}

static bool datetime_storage_time_is_midnight(const char *text) {
    if (text == NULL) {
        return false;
    }
    return (text[datetime_hour_text_offset] == '0' && text[datetime_hour_text_offset + 1U] == '0' &&
            text[datetime_minute_text_offset] == '0' &&
            text[datetime_minute_text_offset + 1U] == '0' &&
            text[datetime_second_text_offset] == '0' &&
            text[datetime_second_text_offset + 1U] == '0') != 0;
}

static int canonicalize_date_text(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    const char *column_name,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    enum temporal_text_kind kind = TEMPORAL_TEXT_INVALID;
    bool strict = false;

    if (text == NULL || column_name == NULL || out_value == NULL) {
        free(text);
        return MYLITE_MISUSE;
    }
    if (!date_text_has_canonical_shape(text, text_length)) {
        set_unsupported_error(database, "DATE values support only canonical YYYY-MM-DD strings");
        free(text);
        return MYLITE_ERROR;
    }

    kind = classify_date_text(text, text_length);
    strict = temporal_sql_mode_is_strict(database);
    switch (kind) {
    case TEMPORAL_TEXT_NORMAL:
        return assign_text_value(database, text, text_length, out_value);
    case TEMPORAL_TEXT_FULL_ZERO:
        if (!temporal_sql_mode_has_no_zero_date(database)) {
            return assign_text_value(database, text, text_length, out_value);
        }
        if (ignore_errors || !strict) {
            return make_date_zero_with_warning(database, text, column_name, row_number, out_value);
        }
        break;
    case TEMPORAL_TEXT_PARTIAL_ZERO:
        if (!temporal_sql_mode_has_no_zero_in_date(database)) {
            return assign_text_value(database, text, text_length, out_value);
        }
        if (ignore_errors || !strict) {
            return make_date_zero_with_warning(database, text, column_name, row_number, out_value);
        }
        break;
    case TEMPORAL_TEXT_ALLOW_INVALID_CANDIDATE:
        if (temporal_sql_mode_has_allow_invalid_dates(database)) {
            return assign_text_value(database, text, text_length, out_value);
        }
        if (ignore_errors || !strict) {
            return make_date_zero_with_warning(database, text, column_name, row_number, out_value);
        }
        break;
    case TEMPORAL_TEXT_INVALID:
        if (ignore_errors || !strict) {
            return make_date_zero_with_warning(database, text, column_name, row_number, out_value);
        }
        break;
    case TEMPORAL_TEXT_DEFERRED:
        break;
    }

    set_incorrect_date_value_error(database, text, column_name, row_number);
    free(text);
    return MYLITE_ERROR;
}

static int convert_time_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(
            database,
            "TIME values support only string, NULL, and DEFAULT values"
        );
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        value_node,
        "TIME values support only string literals",
        "TIME values do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    return canonicalize_time_text(
        database,
        text,
        text_length,
        column->name,
        row_number,
        ignore_errors,
        out_value
    );
}

static int canonicalize_time_text(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    const char *column_name,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    bool is_negative = false;
    bool store_zero = false;
    uint32_t hour = 0U;
    uint32_t minute = 0U;
    uint32_t second = 0U;
    int rc = MYLITE_OK;

    if (text == NULL || column_name == NULL || out_value == NULL) {
        free(text);
        return MYLITE_MISUSE;
    }
    if (time_text_is_canonical_valid(text, text_length)) {
        return assign_text_value(database, text, text_length, out_value);
    }
    if (!time_text_has_canonical_shape(text, text_length)) {
        if (ignore_errors) {
            rc = append_data_truncated_warning(database, column_name, row_number);
            free(text);
            if (rc != MYLITE_OK) {
                return rc;
            }
            return make_zero_time_value(database, out_value);
        }
        set_unsupported_error(database, "TIME values support only canonical [-]HH:MM:SS strings");
        free(text);
        return MYLITE_ERROR;
    }

    if (!ignore_errors) {
        set_incorrect_time_value_error(database, text, column_name, row_number);
        free(text);
        return MYLITE_ERROR;
    }

    if (!time_text_to_components(text, text_length, &is_negative, &hour, &minute, &second)) {
        free(text);
        return MYLITE_ERROR;
    }
    store_zero =
        (!time_text_uses_canonical_hour_width(text, text_length, &hour) ||
         minute > time_maximum_minute_or_second || second > time_maximum_minute_or_second ||
         (hour == 0U && minute == 0U && second == 0U)) != 0;
    rc = append_out_of_range_warning(database, column_name, row_number);
    free(text);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (store_zero) {
        return make_zero_time_value(database, out_value);
    }
    if (is_negative) {
        return copy_text_value(database, "-838:59:59", out_value);
    }
    return copy_text_value(database, "838:59:59", out_value);
}

static int convert_datetime_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(
            database,
            "DATETIME values support only string, NULL, and DEFAULT values"
        );
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        value_node,
        "DATETIME values support only string literals",
        "DATETIME values do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    return convert_relaxed_datetime_storage_text(
        database,
        text,
        text_length,
        column->name,
        row_number,
        ignore_errors,
        database == NULL ? 0 : database->session.time_zone_offset_minutes,
        out_value
    );
}

static int convert_timestamp_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(
            database,
            "TIMESTAMP values support only string, NULL, and DEFAULT values"
        );
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal(
        database,
        value_node,
        "TIMESTAMP values support only string literals",
        "TIMESTAMP values do not support NUL bytes",
        &text,
        &text_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    return convert_relaxed_timestamp_storage_text(
        database,
        text,
        text_length,
        column->name,
        row_number,
        ignore_errors,
        0,
        out_value
    );
}

static int convert_relaxed_datetime_storage_text(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    const char *column_name,
    size_t row_number,
    bool ignore_errors,
    int target_offset_minutes,
    struct planned_value *out_value
) {
    char normalized[datetime_text_length + 1U];
    char *copy = NULL;
    struct temporal_predicate_normalization_input normalization = {0};
    int rc = MYLITE_OK;

    normalization.text = text;
    normalization.text_length = text_length;
    normalization.target_offset_minutes = target_offset_minutes;
    if (normalize_iso_temporal_predicate_text(&normalization, normalized)) {
        free(text);
        copy = copy_temporal_text(database, normalized, datetime_text_length);
        if (copy == NULL) {
            return MYLITE_NOMEM;
        }
        return canonicalize_datetime_text(
            database,
            copy,
            datetime_text_length,
            column_name,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (normalize_z_temporal_predicate_text(text, text_length, normalized)) {
        if (!ignore_errors && temporal_sql_mode_is_strict(database)) {
            set_incorrect_datetime_value_error(database, text, column_name, row_number);
            free(text);
            return MYLITE_ERROR;
        }
        copy = copy_temporal_text(database, normalized, datetime_text_length);
        if (copy == NULL) {
            free(text);
            return MYLITE_NOMEM;
        }
        rc = canonicalize_datetime_text(
            database,
            copy,
            datetime_text_length,
            column_name,
            row_number,
            ignore_errors,
            out_value
        );
        if (rc == MYLITE_OK) {
            rc = append_data_truncated_warning(database, column_name, row_number);
        }
        free(text);
        return rc;
    }

    return canonicalize_datetime_text(
        database,
        text,
        text_length,
        column_name,
        row_number,
        ignore_errors,
        out_value
    );
}

static int canonicalize_timestamp_text(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    const char *column_name,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    if (timestamp_text_is_canonical_valid(text, text_length)) {
        return assign_text_value(database, text, text_length, out_value);
    }
    if (!datetime_text_has_canonical_shape(text, text_length)) {
        set_unsupported_error(
            database,
            "TIMESTAMP values support only canonical YYYY-MM-DD HH:MM:SS strings"
        );
        free(text);
        return MYLITE_ERROR;
    }
    if (datetime_text_is_zero_datetime(text, text_length)) {
        if (!temporal_sql_mode_has_no_zero_date(database)) {
            return assign_text_value(database, text, text_length, out_value);
        }
        if (ignore_errors || !temporal_sql_mode_is_strict(database)) {
            return make_timestamp_zero_with_warning(
                database,
                text,
                column_name,
                row_number,
                out_value
            );
        }
    } else if (ignore_errors || !temporal_sql_mode_is_strict(database)) {
        return make_timestamp_zero_with_warning(database, text, column_name, row_number, out_value);
    }

    set_incorrect_datetime_value_error(database, text, column_name, row_number);
    free(text);
    return MYLITE_ERROR;
}

static int convert_relaxed_timestamp_storage_text(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    const char *column_name,
    size_t row_number,
    bool ignore_errors,
    int target_offset_minutes,
    struct planned_value *out_value
) {
    char normalized[datetime_text_length + 1U];
    char *copy = NULL;
    struct temporal_predicate_normalization_input normalization = {0};
    int rc = MYLITE_OK;

    normalization.text = text;
    normalization.text_length = text_length;
    normalization.target_offset_minutes = target_offset_minutes;
    if (normalize_iso_temporal_predicate_text(&normalization, normalized)) {
        free(text);
        copy = copy_temporal_text(database, normalized, datetime_text_length);
        if (copy == NULL) {
            return MYLITE_NOMEM;
        }
        return canonicalize_timestamp_text(
            database,
            copy,
            datetime_text_length,
            column_name,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (normalize_z_temporal_predicate_text(text, text_length, normalized)) {
        if (!ignore_errors && temporal_sql_mode_is_strict(database)) {
            set_incorrect_datetime_value_error(database, text, column_name, row_number);
            free(text);
            return MYLITE_ERROR;
        }
        copy = copy_temporal_text(database, normalized, datetime_text_length);
        if (copy == NULL) {
            free(text);
            return MYLITE_NOMEM;
        }
        rc = canonicalize_timestamp_text(
            database,
            copy,
            datetime_text_length,
            column_name,
            row_number,
            ignore_errors,
            out_value
        );
        if (rc == MYLITE_OK) {
            rc = append_data_truncated_warning(database, column_name, row_number);
        }
        free(text);
        return rc;
    }

    return canonicalize_timestamp_text(
        database,
        text,
        text_length,
        column_name,
        row_number,
        ignore_errors,
        out_value
    );
}

static char *copy_temporal_text(struct mylite_db *database, const char *text, size_t text_length) {
    char *copy = NULL;

    if (text == NULL) {
        return NULL;
    }
    copy = malloc(text_length + 1U);
    if (copy == NULL) {
        set_nomem_error(database);
        return NULL;
    }
    memcpy(copy, text, text_length);
    copy[text_length] = '\0';
    return copy;
}

static int convert_year_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    const struct mylite_sql_ast_node *literal = unwrap_parenthesized_expression(value_node);
    bool is_negative = false;
    bool parse_overflow = false;
    uint64_t magnitude = 0U;
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (literal == NULL || column == NULL || out_value == NULL) {
        set_unsupported_error(
            database,
            "YEAR values support only integer, boolean, string, NULL, and DEFAULT values"
        );
        return MYLITE_ERROR;
    }
    if (literal->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_STRING) {
        rc = decode_sql_string_literal(
            database,
            literal,
            "YEAR values support only string literals",
            "YEAR values do not support NUL bytes",
            &text,
            &text_length
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        return convert_year_string_value(
            database,
            text,
            text_length,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }

    rc = parse_year_value_magnitude(literal, &magnitude, &is_negative, &parse_overflow);
    if (rc != MYLITE_OK) {
        set_unsupported_error(
            database,
            "YEAR values support only integer, boolean, string, NULL, and DEFAULT values"
        );
        return MYLITE_ERROR;
    }
    if (parse_overflow) {
        if (!ignore_errors) {
            set_out_of_range_error(database, column->name, row_number);
            return MYLITE_ERROR;
        }
        return make_zero_year_value_with_warning(
            database,
            column,
            row_number,
            false,
            NULL,
            out_value
        );
    }

    return convert_year_integer_value(
        database,
        magnitude,
        is_negative,
        column,
        row_number,
        ignore_errors,
        out_value
    );
}

static int convert_year_integer_value(
    struct mylite_db *database,
    uint64_t magnitude,
    bool is_negative,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    uint32_t year = 0U;

    if (!convert_year_numeric_magnitude(magnitude, is_negative, &year)) {
        if (ignore_errors) {
            return make_zero_year_value_with_warning(
                database,
                column,
                row_number,
                false,
                NULL,
                out_value
            );
        }
        set_out_of_range_error(database, column->name, row_number);
        return MYLITE_ERROR;
    }

    return make_year_value(database, year, out_value);
}

static int convert_year_string_value(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    uint32_t year = 0U;
    int rc = convert_year_string_magnitude(text, text_length, &year);

    if (rc == MYLITE_OK) {
        free(text);
        return make_year_value(database, year, out_value);
    }
    if (ignore_errors) {
        bool incorrect_integer = rc == year_conversion_incorrect_integer;

        return make_zero_year_value_with_warning(
            database,
            column,
            row_number,
            incorrect_integer,
            text,
            out_value
        );
    }
    if (rc == year_conversion_incorrect_integer) {
        set_incorrect_integer_value_error(database, text, column->name, row_number);
        free(text);
        return MYLITE_ERROR;
    }

    set_out_of_range_error(database, column->name, row_number);
    free(text);
    return MYLITE_ERROR;
}

static int convert_year_string_magnitude(const char *text, size_t text_length, uint32_t *out_year) {
    uint64_t magnitude = 0U;
    bool overflow = false;

    if (text == NULL || out_year == NULL || text_length == 0U) {
        return year_conversion_incorrect_integer;
    }
    for (size_t index = 0U; index < text_length; ++index) {
        unsigned char byte = (unsigned char)text[index];
        uint64_t digit = 0U;

        if (byte < '0' || byte > '9') {
            return year_conversion_incorrect_integer;
        }
        digit = (uint64_t)(byte - '0');
        if (magnitude > (UINT64_MAX - digit) / decimal_base) {
            overflow = true;
        } else if (!overflow) {
            magnitude = (magnitude * decimal_base) + digit;
        }
    }
    if (overflow) {
        return MYLITE_ERROR;
    }
    if (text_length == 1U || text_length == 2U) {
        if (magnitude == 0U) {
            *out_year = (uint32_t)year_two_digit_high_base;
            return MYLITE_OK;
        }
        if (convert_year_numeric_magnitude(magnitude, false, out_year)) {
            return MYLITE_OK;
        }
        return MYLITE_ERROR;
    }
    if (text_length == year_text_length) {
        if (magnitude == 0U) {
            *out_year = 0U;
            return MYLITE_OK;
        }
        if (magnitude >= year_minimum_normal && magnitude <= year_maximum) {
            *out_year = (uint32_t)magnitude;
            return MYLITE_OK;
        }
    }

    return MYLITE_ERROR;
}

static int convert_year_direct_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    struct planned_value *out_value
) {
    const struct mylite_sql_ast_node *literal = unwrap_parenthesized_expression(value_node);
    bool is_negative = false;
    bool parse_overflow = false;
    uint64_t magnitude = 0U;
    int rc = MYLITE_OK;

    if (literal == NULL) {
        set_unsupported_error(database, "WHERE YEAR predicates support only scalar literals");
        return MYLITE_ERROR;
    }
    if (literal->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_STRING) {
        char *text = NULL;
        size_t text_length = 0U;
        uint32_t represented_year = 0U;

        rc = decode_sql_string_literal(
            database,
            literal,
            "WHERE YEAR predicates support only string literals",
            "WHERE YEAR predicate literals do not support NUL bytes",
            &text,
            &text_length
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        rc = convert_year_direct_string(text, text_length, &represented_year);
        free(text);
        if (rc != MYLITE_OK) {
            return make_year_value(database, 1U, out_value);
        }
        return make_year_value(database, represented_year, out_value);
    }

    rc = parse_year_value_magnitude(literal, &magnitude, &is_negative, &parse_overflow);
    if (rc != MYLITE_OK || parse_overflow || is_negative) {
        return make_year_value(database, 1U, out_value);
    }
    if (magnitude == 0U) {
        return make_year_value(database, 0U, out_value);
    }
    if (magnitude <= year_direct_maximum) {
        return make_year_value(database, (uint32_t)magnitude, out_value);
    }
    return make_year_value(database, 1U, out_value);
}

static int convert_year_direct_string(const char *text, size_t text_length, uint32_t *out_year) {
    uint64_t magnitude = 0U;
    bool overflow = false;

    if (text == NULL || text_length == 0U || out_year == NULL) {
        return MYLITE_ERROR;
    }
    for (size_t index = 0U; index < text_length; ++index) {
        unsigned char byte = (unsigned char)text[index];
        uint64_t digit = 0U;

        if (byte < '0' || byte > '9') {
            return MYLITE_ERROR;
        }
        digit = (uint64_t)(byte - '0');
        if (magnitude > (UINT64_MAX - digit) / decimal_base) {
            overflow = true;
        } else if (!overflow) {
            magnitude = (magnitude * decimal_base) + digit;
        }
    }
    if (overflow || magnitude > year_direct_maximum) {
        return MYLITE_ERROR;
    }

    *out_year = (uint32_t)magnitude;
    return MYLITE_OK;
}

static int parse_year_value_magnitude(
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_magnitude,
    bool *out_is_negative,
    bool *out_parse_overflow
) {
    const struct mylite_sql_ast_node *literal = value_node;

    if (value_node == NULL || out_magnitude == NULL || out_is_negative == NULL ||
        out_parse_overflow == NULL) {
        return MYLITE_MISUSE;
    }

    *out_magnitude = 0U;
    *out_is_negative = false;
    *out_parse_overflow = false;
    if (value_node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(value_node);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            *out_is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            return MYLITE_ERROR;
        }
        literal = unwrap_parenthesized_expression(child_at(value_node, 0U));
    }
    if (boolean_literal_magnitude(literal, out_magnitude)) {
        return MYLITE_OK;
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
        return MYLITE_ERROR;
    }
    if (parse_unsigned_integer_literal(&literal->span, out_magnitude) != MYLITE_OK) {
        *out_parse_overflow = true;
    }
    return MYLITE_OK;
}

static bool convert_year_numeric_magnitude(
    uint64_t magnitude,
    bool is_negative,
    uint32_t *out_year
) {
    if (out_year == NULL) {
        return false;
    }
    if (is_negative && magnitude != 0U) {
        return false;
    }
    if (magnitude == 0U) {
        *out_year = 0U;
        return true;
    }
    if (magnitude <= year_two_digit_high_max) {
        *out_year = (uint32_t)(year_two_digit_high_base + magnitude);
        return true;
    }
    if (magnitude >= year_two_digit_low_min && magnitude <= year_two_digit_max) {
        *out_year = (uint32_t)(year_two_digit_low_base + magnitude);
        return true;
    }
    if (magnitude >= year_minimum_normal && magnitude <= year_maximum) {
        *out_year = (uint32_t)magnitude;
        return true;
    }

    return false;
}

static int make_year_value(
    struct mylite_db *database,
    uint32_t year,
    struct planned_value *out_value
) {
    char text[year_text_length + 1U];
    int written = snprintf(text, sizeof(text), "%04" PRIu32, year);

    if (written != year_text_length) {
        set_runtime_error(database, "failed to format YEAR value");
        return MYLITE_ERROR;
    }
    return copy_text_value(database, text, out_value);
}

static int make_zero_year_value(struct mylite_db *database, struct planned_value *out_value) {
    return make_year_value(database, 0U, out_value);
}

static int make_zero_year_value_with_warning(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool incorrect_integer,
    char *value_text,
    struct planned_value *out_value
) {
    int rc = MYLITE_OK;

    if (incorrect_integer) {
        rc = append_incorrect_integer_value_warning(
            database,
            value_text == NULL ? "" : value_text,
            column->name,
            row_number
        );
    } else {
        rc = append_out_of_range_warning(database, column->name, row_number);
    }
    free(value_text);
    if (rc != MYLITE_OK) {
        return rc;
    }
    return make_zero_year_value(database, out_value);
}

static bool year_text_is_canonical(const char *text, size_t text_length) {
    uint32_t year = 0U;

    if (convert_year_direct_string(text, text_length, &year) != MYLITE_OK) {
        return false;
    }
    if (text_length != year_text_length) {
        return false;
    }
    if (year == 0U || (year >= year_minimum_normal && year <= year_maximum)) {
        return true;
    }
    return false;
}

static int canonicalize_datetime_text(
    struct mylite_db *database,
    char *text,
    size_t text_length,
    const char *column_name,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    enum temporal_text_kind kind = TEMPORAL_TEXT_INVALID;
    bool strict = false;

    if (text == NULL || column_name == NULL || out_value == NULL) {
        free(text);
        return MYLITE_MISUSE;
    }
    if (!datetime_text_has_canonical_shape(text, text_length)) {
        set_unsupported_error(
            database,
            "DATETIME values support only canonical YYYY-MM-DD HH:MM:SS strings"
        );
        free(text);
        return MYLITE_ERROR;
    }

    kind = classify_datetime_text(text, text_length);
    strict = temporal_sql_mode_is_strict(database);
    switch (kind) {
    case TEMPORAL_TEXT_NORMAL:
        return assign_text_value(database, text, text_length, out_value);
    case TEMPORAL_TEXT_FULL_ZERO:
        if (!temporal_sql_mode_has_no_zero_date(database)) {
            return assign_text_value(database, text, text_length, out_value);
        }
        if (ignore_errors || !strict) {
            return make_datetime_zero_with_warning(
                database,
                text,
                column_name,
                row_number,
                out_value
            );
        }
        break;
    case TEMPORAL_TEXT_PARTIAL_ZERO:
        if (!temporal_sql_mode_has_no_zero_in_date(database)) {
            return assign_text_value(database, text, text_length, out_value);
        }
        if (ignore_errors || !strict) {
            return make_datetime_zero_with_warning(
                database,
                text,
                column_name,
                row_number,
                out_value
            );
        }
        break;
    case TEMPORAL_TEXT_ALLOW_INVALID_CANDIDATE:
        if (temporal_sql_mode_has_allow_invalid_dates(database)) {
            return assign_text_value(database, text, text_length, out_value);
        }
        if (ignore_errors || !strict) {
            return make_datetime_zero_with_warning(
                database,
                text,
                column_name,
                row_number,
                out_value
            );
        }
        break;
    case TEMPORAL_TEXT_INVALID:
        if (ignore_errors || !strict) {
            return make_datetime_zero_with_warning(
                database,
                text,
                column_name,
                row_number,
                out_value
            );
        }
        break;
    case TEMPORAL_TEXT_DEFERRED:
        break;
    }

    set_incorrect_datetime_value_error(database, text, column_name, row_number);
    free(text);
    return MYLITE_ERROR;
}

static bool temporal_sql_mode_is_strict(const struct mylite_db *database) {
    return session_sql_mode_is_strict(database);
}

static bool session_sql_mode_is_strict(const struct mylite_db *database) {
    if (database == NULL) {
        return false;
    }
    return (session_sql_mode_has(&database->session, MYLITE_SESSION_SQL_MODE_STRICT_TRANS_TABLES) ||
            session_sql_mode_has(&database->session, MYLITE_SESSION_SQL_MODE_STRICT_ALL_TABLES)) !=
           0;
}

static bool temporal_sql_mode_has_no_zero_date(const struct mylite_db *database) {
    return (database != NULL &&
            session_sql_mode_has(&database->session, MYLITE_SESSION_SQL_MODE_NO_ZERO_DATE)) != 0;
}

static bool temporal_sql_mode_has_no_zero_in_date(const struct mylite_db *database) {
    return (database != NULL &&
            session_sql_mode_has(&database->session, MYLITE_SESSION_SQL_MODE_NO_ZERO_IN_DATE)) != 0;
}

static bool temporal_sql_mode_has_allow_invalid_dates(const struct mylite_db *database) {
    return (
               database != NULL &&
               session_sql_mode_has(&database->session, MYLITE_SESSION_SQL_MODE_ALLOW_INVALID_DATES)
           ) != 0;
}

static enum temporal_text_kind classify_date_text(const char *text, size_t text_length) {
    uint32_t year = 0U;
    uint32_t month = 0U;
    uint32_t day = 0U;

    if (!date_text_has_canonical_shape(text, text_length)) {
        return TEMPORAL_TEXT_INVALID;
    }
    if (date_text_is_zero_date(text, text_length)) {
        return TEMPORAL_TEXT_FULL_ZERO;
    }
    if (!date_component_text_to_u32(text + date_year_text_offset, date_year_text_length, &year) ||
        !date_component_text_to_u32(
            text + date_month_text_offset,
            date_month_text_length,
            &month
        ) ||
        !date_component_text_to_u32(text + date_day_text_offset, date_day_text_length, &day)) {
        return TEMPORAL_TEXT_INVALID;
    }

    return classify_temporal_date_components(year, month, day);
}

static enum temporal_text_kind classify_datetime_text(const char *text, size_t text_length) {
    uint32_t year = 0U;
    uint32_t month = 0U;
    uint32_t day = 0U;
    uint32_t hour = 0U;
    uint32_t minute = 0U;
    uint32_t second = 0U;

    if (!datetime_text_has_canonical_shape(text, text_length)) {
        return TEMPORAL_TEXT_INVALID;
    }
    if (datetime_text_is_zero_datetime(text, text_length)) {
        return TEMPORAL_TEXT_FULL_ZERO;
    }
    if (!date_component_text_to_u32(text + date_year_text_offset, date_year_text_length, &year) ||
        !date_component_text_to_u32(
            text + date_month_text_offset,
            date_month_text_length,
            &month
        ) ||
        !date_component_text_to_u32(text + date_day_text_offset, date_day_text_length, &day) ||
        !date_component_text_to_u32(
            text + datetime_hour_text_offset,
            datetime_hour_text_length,
            &hour
        ) ||
        !date_component_text_to_u32(
            text + datetime_minute_text_offset,
            datetime_minute_text_length,
            &minute
        ) ||
        !date_component_text_to_u32(
            text + datetime_second_text_offset,
            datetime_second_text_length,
            &second
        )) {
        return TEMPORAL_TEXT_INVALID;
    }
    if (!datetime_time_components_valid(hour, minute, second)) {
        return TEMPORAL_TEXT_INVALID;
    }

    return classify_temporal_date_components(year, month, day);
}

static enum temporal_text_kind classify_temporal_date_components(
    uint32_t year,
    uint32_t month,
    uint32_t day
) {
    if (year < date_minimum_year || year > date_maximum_year) {
        return TEMPORAL_TEXT_DEFERRED;
    }
    if (month == 0U || day == 0U) {
        if (month <= date_months_per_year && day <= date_maximum_day_field) {
            return TEMPORAL_TEXT_PARTIAL_ZERO;
        }
        return TEMPORAL_TEXT_INVALID;
    }
    if (date_year_month_day_valid(year, month, day)) {
        return TEMPORAL_TEXT_NORMAL;
    }
    if (month >= date_first_month && month <= date_months_per_year &&
        day <= date_maximum_day_field) {
        return TEMPORAL_TEXT_ALLOW_INVALID_CANDIDATE;
    }
    return TEMPORAL_TEXT_INVALID;
}

static int make_date_zero_with_warning(
    struct mylite_db *database,
    char *text,
    const char *column_name,
    size_t row_number,
    struct planned_value *out_value
) {
    int rc = append_out_of_range_warning(database, column_name, row_number);

    free(text);
    if (rc != MYLITE_OK) {
        return rc;
    }
    return make_zero_date_value(database, out_value);
}

static int make_datetime_zero_with_warning(
    struct mylite_db *database,
    char *text,
    const char *column_name,
    size_t row_number,
    struct planned_value *out_value
) {
    int rc = append_out_of_range_warning(database, column_name, row_number);

    free(text);
    if (rc != MYLITE_OK) {
        return rc;
    }
    return make_zero_datetime_value(database, out_value);
}

static int make_timestamp_zero_with_warning(
    struct mylite_db *database,
    char *text,
    const char *column_name,
    size_t row_number,
    struct planned_value *out_value
) {
    int rc = append_out_of_range_warning(database, column_name, row_number);

    free(text);
    if (rc != MYLITE_OK) {
        return rc;
    }
    return make_zero_timestamp_value(database, out_value);
}

static bool date_text_is_canonical_valid(const char *text, size_t text_length) {
    uint32_t year = 0U;
    uint32_t month = 0U;
    uint32_t day = 0U;

    if (!date_text_has_canonical_shape(text, text_length)) {
        return false;
    }
    if (date_text_is_zero_date(text, text_length)) {
        return false;
    }
    if (!date_component_text_to_u32(text + date_year_text_offset, date_year_text_length, &year) ||
        !date_component_text_to_u32(
            text + date_month_text_offset,
            date_month_text_length,
            &month
        ) ||
        !date_component_text_to_u32(text + date_day_text_offset, date_day_text_length, &day)) {
        return false;
    }

    return date_year_month_day_valid(year, month, day);
}

static bool date_text_has_canonical_shape(const char *text, size_t text_length) {
    if (text == NULL || text_length != date_text_length ||
        text[date_first_separator_offset] != '-' || text[date_second_separator_offset] != '-') {
        return false;
    }
    for (size_t index = 0U; index < text_length; ++index) {
        unsigned char byte = (unsigned char)text[index];

        if (index == date_first_separator_offset || index == date_second_separator_offset) {
            continue;
        }
        if (byte < '0' || byte > '9') {
            return false;
        }
    }
    return true;
}

static bool date_text_is_zero_date(const char *text, size_t text_length) {
    static const char zero_date[] = "0000-00-00";

    return (text != NULL && text_length == date_text_length &&
            memcmp(text, zero_date, date_text_length) == 0) != 0;
}

static bool time_text_is_canonical_valid(const char *text, size_t text_length) {
    int64_t seconds = 0;

    return time_text_to_seconds(text, text_length, &seconds);
}

static bool time_text_has_canonical_shape(const char *text, size_t text_length) {
    bool is_negative = false;
    size_t hour_offset = 0U;
    size_t first_separator = 0U;
    size_t second_separator = 0U;

    if (text == NULL || text_length < time_text_minimum_length ||
        text_length > time_text_maximum_length) {
        return false;
    }
    is_negative = text[0] == '-';
    if (is_negative) {
        hour_offset = 1U;
    }
    if (is_negative && text_length == time_text_minimum_length) {
        return false;
    }
    first_separator = text_length - time_minute_second_suffix_length;
    second_separator = text_length - 3U;
    if (first_separator <= hour_offset || text[first_separator] != ':' ||
        text[second_separator] != ':') {
        return false;
    }
    if (first_separator - hour_offset < 2U || first_separator - hour_offset > 3U) {
        return false;
    }
    for (size_t index = 0U; index < text_length; ++index) {
        unsigned char byte = (unsigned char)text[index];

        if ((index == 0U && is_negative) || index == first_separator || index == second_separator) {
            continue;
        }
        if (byte < '0' || byte > '9') {
            return false;
        }
    }

    return true;
}

static bool time_text_uses_canonical_hour_width(
    const char *text,
    size_t text_length,
    const uint32_t *hour
) {
    bool is_negative = false;
    size_t hour_offset = 0U;
    size_t first_separator = 0U;
    size_t hour_digit_count = 0U;

    if (hour == NULL || !time_text_has_canonical_shape(text, text_length)) {
        return false;
    }

    is_negative = text[0] == '-';
    if (is_negative) {
        hour_offset = 1U;
    }
    first_separator = text_length - time_minute_second_suffix_length;
    hour_digit_count = first_separator - hour_offset;

    return (hour_digit_count == 2U ||
            (hour_digit_count == 3U && *hour >= time_minimum_three_digit_hour)) != 0;
}

static bool time_text_to_seconds(const char *text, size_t text_length, int64_t *out_seconds) {
    bool is_negative = false;
    uint32_t hour = 0U;
    uint32_t minute = 0U;
    uint32_t second = 0U;
    int64_t total = 0;

    if (out_seconds == NULL ||
        !time_text_to_components(text, text_length, &is_negative, &hour, &minute, &second)) {
        return false;
    }
    if (minute > time_maximum_minute_or_second || second > time_maximum_minute_or_second) {
        return false;
    }
    if (!time_text_uses_canonical_hour_width(text, text_length, &hour)) {
        return false;
    }
    if (hour == 0U && minute == 0U && second == 0U && is_negative) {
        return false;
    }
    if (hour > time_maximum_hour ||
        (hour == time_maximum_hour &&
         (minute > time_maximum_minute_or_second || second > time_maximum_minute_or_second))) {
        return false;
    }

    total = ((int64_t)hour * time_second_per_hour) + ((int64_t)minute * time_second_per_minute) +
            (int64_t)second;
    if (is_negative) {
        total = -total;
    }
    *out_seconds = total;
    return true;
}

static bool time_text_to_components(
    const char *text,
    size_t text_length,
    bool *out_is_negative,
    uint32_t *out_hour,
    uint32_t *out_minute,
    uint32_t *out_second
) {
    bool is_negative = false;
    size_t hour_offset = 0U;
    size_t first_separator = 0U;
    size_t second_separator = 0U;

    if (out_is_negative == NULL || out_hour == NULL || out_minute == NULL || out_second == NULL ||
        !time_text_has_canonical_shape(text, text_length)) {
        return false;
    }
    is_negative = text[0] == '-';
    if (is_negative) {
        hour_offset = 1U;
    }
    first_separator = text_length - time_minute_second_suffix_length;
    second_separator = text_length - 3U;
    if (!date_component_text_to_u32(text + hour_offset, first_separator - hour_offset, out_hour) ||
        !date_component_text_to_u32(text + first_separator + 1U, 2U, out_minute) ||
        !date_component_text_to_u32(text + second_separator + 1U, 2U, out_second)) {
        return false;
    }

    *out_is_negative = is_negative;
    return true;
}

static bool datetime_text_is_canonical_valid(const char *text, size_t text_length) {
    uint32_t year = 0U;
    uint32_t month = 0U;
    uint32_t day = 0U;
    uint32_t hour = 0U;
    uint32_t minute = 0U;
    uint32_t second = 0U;

    if (!datetime_text_has_canonical_shape(text, text_length)) {
        return false;
    }
    if (datetime_text_is_zero_datetime(text, text_length)) {
        return false;
    }
    if (!date_component_text_to_u32(text + date_year_text_offset, date_year_text_length, &year) ||
        !date_component_text_to_u32(
            text + date_month_text_offset,
            date_month_text_length,
            &month
        ) ||
        !date_component_text_to_u32(text + date_day_text_offset, date_day_text_length, &day) ||
        !date_component_text_to_u32(
            text + datetime_hour_text_offset,
            datetime_hour_text_length,
            &hour
        ) ||
        !date_component_text_to_u32(
            text + datetime_minute_text_offset,
            datetime_minute_text_length,
            &minute
        ) ||
        !date_component_text_to_u32(
            text + datetime_second_text_offset,
            datetime_second_text_length,
            &second
        )) {
        return false;
    }

    return (date_year_month_day_valid(year, month, day) &&
            datetime_time_components_valid(hour, minute, second)) != 0;
}

static bool timestamp_text_is_canonical_valid(const char *text, size_t text_length) {
    return (datetime_text_is_canonical_valid(text, text_length) &&
            timestamp_text_in_supported_range(text, text_length)) != 0;
}

static bool timestamp_text_in_supported_range(const char *text, size_t text_length) {
    static const char minimum_timestamp[] = "1970-01-01 00:00:01";
    static const char maximum_timestamp[] = "2038-01-19 03:14:07";

    return (text != NULL && text_length == datetime_text_length &&
            memcmp(text, minimum_timestamp, datetime_text_length) >= 0 &&
            memcmp(text, maximum_timestamp, datetime_text_length) <= 0) != 0;
}

static bool datetime_text_has_canonical_shape(const char *text, size_t text_length) {
    if (text == NULL || text_length != datetime_text_length ||
        text[date_first_separator_offset] != '-' || text[date_second_separator_offset] != '-' ||
        text[datetime_date_time_separator_offset] != ' ' ||
        text[datetime_first_time_separator_offset] != ':' ||
        text[datetime_second_time_separator_offset] != ':') {
        return false;
    }
    for (size_t index = 0U; index < text_length; ++index) {
        unsigned char byte = (unsigned char)text[index];

        if (index == date_first_separator_offset || index == date_second_separator_offset ||
            index == datetime_date_time_separator_offset ||
            index == datetime_first_time_separator_offset ||
            index == datetime_second_time_separator_offset) {
            continue;
        }
        if (byte < '0' || byte > '9') {
            return false;
        }
    }
    return true;
}

static bool datetime_text_is_zero_datetime(const char *text, size_t text_length) {
    static const char zero_datetime[] = "0000-00-00 00:00:00";

    return (text != NULL && text_length == datetime_text_length &&
            memcmp(text, zero_datetime, datetime_text_length) == 0) != 0;
}

static bool datetime_time_components_valid(uint32_t hour, uint32_t minute, uint32_t second) {
    return (hour >= datetime_minimum_hour && hour <= datetime_maximum_hour &&
            minute >= datetime_minimum_minute_or_second &&
            minute <= datetime_maximum_minute_or_second &&
            second >= datetime_minimum_minute_or_second &&
            second <= datetime_maximum_minute_or_second) != 0;
}

static bool date_component_text_to_u32(const char *text, size_t length, uint32_t *out_value) {
    uint32_t value = 0U;

    if (text == NULL || length == 0U || out_value == NULL) {
        return false;
    }
    for (size_t index = 0U; index < length; ++index) {
        unsigned char byte = (unsigned char)text[index];

        if (byte < '0' || byte > '9') {
            return false;
        }
        value = (value * date_decimal_base) + (uint32_t)(byte - '0');
    }

    *out_value = value;
    return true;
}

static bool date_year_month_day_valid(uint32_t year, uint32_t month, uint32_t day) {
    static const uint32_t month_days[] = {
        31U,
        28U,
        31U,
        30U,
        31U,
        30U,
        31U,
        31U,
        30U,
        31U,
        30U,
        31U,
    };
    uint32_t maximum_day = 0U;

    if (year < date_minimum_year || year > date_maximum_year || month < date_first_month ||
        month > date_months_per_year || day < date_first_day) {
        return false;
    }
    maximum_day = month_days[month - 1U];
    if (month == date_february && date_year_is_leap(year)) {
        maximum_day = date_leap_day;
    }

    return day <= maximum_day;
}

static bool date_year_is_leap(uint32_t year) {
    if (year % date_leap_year_quadricentennial == 0U) {
        return true;
    }
    if (year % date_leap_year_century == 0U) {
        return false;
    }
    return year % date_leap_year_quadrennial == 0U;
}

static int parse_grouped_having_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const char *operand_name,
    bool *out_is_negative,
    uint64_t *out_magnitude
) {
    const struct mylite_sql_ast_node *literal = value_node;
    int rc = MYLITE_OK;

    *out_is_negative = false;
    *out_magnitude = 0U;
    if (value_node == NULL) {
        set_unsupported_error(database, "HAVING supports only integer or boolean literals");
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(value_node);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            *out_is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_unsupported_error(database, "HAVING supports only integer or boolean literals");
            return MYLITE_ERROR;
        }
        literal = child_at(value_node, 0U);
    }
    if (!boolean_literal_magnitude(literal, out_magnitude)) {
        if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
            mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
            set_unsupported_error(database, "HAVING supports only integer or boolean literals");
            return MYLITE_ERROR;
        }
        rc = parse_unsigned_integer_literal(&literal->span, out_magnitude);
        if (rc != MYLITE_OK) {
            set_having_out_of_range_error(database, operand_name);
            return MYLITE_ERROR;
        }
    }

    return MYLITE_OK;
}

static int convert_grouped_having_group_value(
    struct mylite_db *database,
    uint64_t magnitude,
    bool is_negative,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    const uint64_t int64_negative_abs_max = 9223372036854775808ULL;
    struct integer_column_range range = {0};

    if (column == NULL) {
        return MYLITE_ERROR;
    }

    int rc = integer_range_for_column(
        database,
        column,
        "HAVING supports only integer grouped columns",
        &range
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (is_negative) {
        if ((range.negative_abs_max == 0U && magnitude != 0U) ||
            magnitude > range.negative_abs_max) {
            set_having_out_of_range_error(database, column->name);
            return MYLITE_ERROR;
        }
        if (magnitude == int64_negative_abs_max) {
            out_value->integer = INT64_MIN;
        } else {
            out_value->integer = -(int64_t)magnitude;
        }
        out_value->is_null = false;
        return MYLITE_OK;
    }
    if (magnitude > range.positive_max) {
        set_having_out_of_range_error(database, column->name);
        return MYLITE_ERROR;
    }

    out_value->integer = (int64_t)magnitude;
    out_value->is_null = false;
    return MYLITE_OK;
}

static int convert_grouped_having_aggregate_value(
    struct mylite_db *database,
    uint64_t magnitude,
    bool is_negative,
    const char *operand_name,
    struct planned_value *out_value
) {
    const uint64_t int64_positive_max = 9223372036854775807ULL;
    const uint64_t int64_negative_abs_max = 9223372036854775808ULL;

    if (is_negative && magnitude > int64_negative_abs_max) {
        set_having_out_of_range_error(database, operand_name);
        return MYLITE_ERROR;
    }
    if (!is_negative && magnitude > int64_positive_max) {
        set_having_out_of_range_error(database, operand_name);
        return MYLITE_ERROR;
    }
    if (is_negative && magnitude == int64_negative_abs_max) {
        out_value->integer = INT64_MIN;
    } else if (is_negative) {
        out_value->integer = -(int64_t)magnitude;
    } else {
        out_value->integer = (int64_t)magnitude;
    }
    out_value->is_null = false;

    return MYLITE_OK;
}

static int plan_grouped_aggregate_order(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_clause,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
) {
    const struct mylite_sql_ast_node *order_key = NULL;
    const struct mylite_sql_ast_node *direction = NULL;
    size_t group_order_index = 0U;
    int rc = MYLITE_OK;

    out_plan->order.has_order = false;
    out_plan->order_uses_aggregate = false;
    out_plan->order_aggregate_index = 0U;
    out_plan->order.direction = PLANNED_SELECT_ORDER_DEFAULT;
    out_plan->order.column_source_index = 0U;
    out_plan->order.qualify_column_reference = select_source_context_is_joined(source_context);
    if (order_clause == NULL) {
        return MYLITE_OK;
    }
    if (order_clause->kind != MYLITE_SQL_AST_ORDER_BY_CLAUSE) {
        set_unsupported_error(database, "GROUP BY supports only one grouped ORDER BY column");
        return MYLITE_ERROR;
    }

    order_key = child_at(order_clause, 0U);
    if (grouped_order_clause_uses_item_list(order_clause)) {
        set_unsupported_error(database, "GROUP BY supports only one grouped ORDER BY column");
        return MYLITE_ERROR;
    }
    rc = resolve_grouped_order_key(
        database,
        order_key,
        source_context,
        table_columns,
        table_column_count,
        out_plan
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!out_plan->order_uses_aggregate && !find_matching_grouped_key(
                                               out_plan,
                                               &out_plan->order.column,
                                               out_plan->order.column_source_index,
                                               &group_order_index
                                           )) {
        int dependency_rc = MYLITE_OK;

        if (!grouped_projection_column_is_allowed(
                database,
                out_plan,
                table_columns,
                table_column_count,
                &out_plan->order.column,
                out_plan->order.column_source_index,
                &dependency_rc
            )) {
            if (dependency_rc != MYLITE_OK) {
                return dependency_rc;
            }
            set_only_full_group_by_error(
                database,
                1U,
                "ORDER BY clause",
                planned_grouped_aggregate_source_resolution(
                    out_plan,
                    out_plan->order.column_source_index
                ),
                &out_plan->order.column
            );
            return MYLITE_ERROR;
        }
    }

    out_plan->order.has_order = true;
    out_plan->order.direction = PLANNED_SELECT_ORDER_ASC;
    direction = child_at(order_clause, 1U);
    if (mylite_sql_ast_node_order_direction(direction) == MYLITE_SQL_AST_ORDER_DIRECTION_DESC) {
        out_plan->order.direction = PLANNED_SELECT_ORDER_DESC;
    }

    return MYLITE_OK;
}

static int resolve_grouped_order_key(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
) {
    bool matches_alias = false;
    bool matches_unaliased_column = false;
    size_t matched_index = 0U;
    size_t order_source_index = 0U;
    int rc = find_grouped_order_projection_alias(
        database,
        order_key,
        out_plan,
        &matches_alias,
        &matched_index
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (matches_alias) {
        out_plan->order.column = out_plan->projections[matched_index].column;
        out_plan->order.column_source_index =
            out_plan->projections[matched_index].column_source_index;
        return MYLITE_OK;
    }

    rc = find_grouped_order_group_alias(
        database,
        order_key,
        out_plan,
        &matches_alias,
        &matched_index
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (matches_alias) {
        out_plan->order.column = out_plan->groups[matched_index].column;
        out_plan->order.column_source_index = out_plan->groups[matched_index].column_source_index;
        return MYLITE_OK;
    }

    rc = order_identifier_matches_unaliased_projection_column(
        database,
        order_key,
        out_plan,
        &matches_unaliased_column,
        &matched_index
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (matches_unaliased_column) {
        out_plan->order.column = out_plan->projections[matched_index].column;
        out_plan->order.column_source_index =
            out_plan->projections[matched_index].column_source_index;
        return MYLITE_OK;
    }

    rc = order_identifier_matches_unaliased_group_column(
        database,
        order_key,
        out_plan,
        &matches_unaliased_column,
        &matched_index
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (matches_unaliased_column) {
        out_plan->order.column = out_plan->groups[matched_index].column;
        out_plan->order.column_source_index = out_plan->groups[matched_index].column_source_index;
        return MYLITE_OK;
    }

    rc = find_grouped_order_aggregate_alias(
        database,
        order_key,
        out_plan,
        &matches_alias,
        &matched_index
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (matches_alias) {
        out_plan->order_uses_aggregate = true;
        out_plan->order_aggregate_index = matched_index;
        return MYLITE_OK;
    }

    rc = resolve_descriptor_column_reference_with_source_index(
        database,
        order_key,
        source_context,
        COLUMN_REFERENCE_ORDER,
        "GROUP BY supports ORDER BY only on grouped, primary-key-dependent, "
        "or selected aggregate columns",
        table_columns,
        table_column_count,
        &out_plan->order.column,
        &order_source_index
    );
    if (rc == MYLITE_OK) {
        out_plan->order.column_source_index = order_source_index;
    }

    return rc;
}

static bool grouped_order_clause_uses_item_list(const struct mylite_sql_ast_node *order_clause) {
    const struct mylite_sql_ast_node *order_key = child_at(order_clause, 0U);

    if (order_key == NULL) {
        return false;
    }
    return order_key->kind == MYLITE_SQL_AST_ORDER_BY_ITEM_LIST;
}

static int find_grouped_order_group_alias(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    const struct planned_grouped_aggregate *plan,
    bool *out_matched,
    size_t *out_group_index
) {
    bool matches_alias = false;
    size_t matched_index = SIZE_MAX;
    int rc = MYLITE_OK;

    *out_matched = false;
    *out_group_index = 0U;
    for (size_t group_index = 0U; group_index < plan->group_count; ++group_index) {
        rc = order_identifier_matches_alias(
            database,
            order_key,
            plan->groups[group_index].alias,
            &matches_alias
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (!matches_alias) {
            continue;
        }
        if (matched_index != SIZE_MAX) {
            set_unsupported_error(
                database,
                "GROUP BY supports ORDER BY only on unique selected group aliases"
            );
            return MYLITE_ERROR;
        }
        matched_index = group_index;
    }
    if (matched_index == SIZE_MAX) {
        return MYLITE_OK;
    }

    *out_matched = true;
    *out_group_index = matched_index;
    return MYLITE_OK;
}

static int find_grouped_order_projection_alias(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    const struct planned_grouped_aggregate *plan,
    bool *out_matched,
    size_t *out_projection_index
) {
    bool matches_alias = false;
    size_t matched_index = SIZE_MAX;
    int rc = MYLITE_OK;

    *out_matched = false;
    *out_projection_index = 0U;
    for (size_t projection_index = 0U; projection_index < plan->projection_count;
         ++projection_index) {
        rc = order_identifier_matches_alias(
            database,
            order_key,
            plan->projections[projection_index].alias,
            &matches_alias
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (!matches_alias) {
            continue;
        }
        if (matched_index != SIZE_MAX) {
            set_unsupported_error(
                database,
                "GROUP BY supports ORDER BY only on unique selected group aliases"
            );
            return MYLITE_ERROR;
        }
        matched_index = projection_index;
    }
    if (matched_index == SIZE_MAX) {
        return MYLITE_OK;
    }

    *out_matched = true;
    *out_projection_index = matched_index;
    return MYLITE_OK;
}

static int find_grouped_order_aggregate_alias(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    const struct planned_grouped_aggregate *plan,
    bool *out_matched,
    size_t *out_aggregate_index
) {
    bool matches_alias = false;
    size_t matched_index = SIZE_MAX;
    int rc = MYLITE_OK;

    *out_matched = false;
    *out_aggregate_index = 0U;
    for (size_t aggregate_index = 0U; aggregate_index < plan->aggregate_count; ++aggregate_index) {
        const struct planned_grouped_aggregate_item *item = &plan->aggregates[aggregate_index];

        rc = order_identifier_matches_alias(database, order_key, item->alias, &matches_alias);
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (!matches_alias) {
            continue;
        }
        if (matched_index != SIZE_MAX) {
            set_unsupported_error(
                database,
                "GROUP BY supports ORDER BY only on unique selected aggregate aliases"
            );
            return MYLITE_ERROR;
        }
        matched_index = aggregate_index;
    }
    if (matched_index == SIZE_MAX) {
        return MYLITE_OK;
    }
    if (plan->aggregates[matched_index].function == PLANNED_GROUPED_AGGREGATE_AVG) {
        set_unsupported_error(
            database,
            "GROUP BY does not support ORDER BY on AVG aggregate aliases"
        );
        return MYLITE_ERROR;
    }
    if (grouped_aggregate_function_is_bitwise(plan->aggregates[matched_index].function)) {
        set_unsupported_error(
            database,
            "GROUP BY does not support ORDER BY on bitwise aggregate aliases"
        );
        return MYLITE_ERROR;
    }
    if (plan->aggregates[matched_index].function == PLANNED_GROUPED_AGGREGATE_GROUP_CONCAT) {
        set_unsupported_error(
            database,
            "GROUP BY does not support ORDER BY on GROUP_CONCAT aggregate aliases"
        );
        return MYLITE_ERROR;
    }

    *out_matched = true;
    *out_aggregate_index = matched_index;
    return MYLITE_OK;
}

static int order_identifier_matches_alias(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct mylite_sql_ast_node *alias,
    bool *out_matches
) {
    char *order_name = NULL;
    char *alias_text = NULL;
    int rc = MYLITE_OK;

    *out_matches = false;
    if (column_node == NULL || column_node->kind != MYLITE_SQL_AST_IDENTIFIER || alias == NULL) {
        return MYLITE_OK;
    }

    rc = copy_select_item_identifier_alias_text(database, column_node, &order_name);
    if (rc == MYLITE_OK) {
        rc = copy_select_item_alias_text(database, alias, &alias_text);
    }
    if (rc == MYLITE_OK) {
        *out_matches = text_equals_ascii_case_insensitive(order_name, alias_text);
    }

    free(order_name);
    free(alias_text);
    return rc;
}

static int order_identifier_matches_unaliased_group_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct planned_grouped_aggregate *plan,
    bool *out_matches,
    size_t *out_group_index
) {
    char *order_name = NULL;
    bool ambiguous_group_key = false;
    int rc = MYLITE_OK;

    *out_matches = false;
    *out_group_index = 0U;
    if (column_node == NULL || column_node->kind != MYLITE_SQL_AST_IDENTIFIER) {
        return MYLITE_OK;
    }

    rc = copy_select_item_identifier_alias_text(database, column_node, &order_name);
    if (rc == MYLITE_OK) {
        *out_matches = find_grouped_key_by_unqualified_name(
            plan,
            order_name,
            out_group_index,
            &ambiguous_group_key
        );
    }
    if (rc == MYLITE_OK && ambiguous_group_key) {
        set_ambiguous_column_reference_error(database, COLUMN_REFERENCE_ORDER, order_name);
        rc = MYLITE_ERROR;
    }

    free(order_name);
    return rc;
}

static int order_identifier_matches_unaliased_projection_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct planned_grouped_aggregate *plan,
    bool *out_matches,
    size_t *out_projection_index
) {
    char *order_name = NULL;
    bool ambiguous_projection = false;
    int rc = MYLITE_OK;

    *out_matches = false;
    *out_projection_index = 0U;
    if (column_node == NULL || column_node->kind != MYLITE_SQL_AST_IDENTIFIER) {
        return MYLITE_OK;
    }

    rc = copy_select_item_identifier_alias_text(database, column_node, &order_name);
    if (rc == MYLITE_OK) {
        *out_matches = find_grouped_projection_by_unqualified_name(
            plan,
            order_name,
            out_projection_index,
            &ambiguous_projection
        );
    }
    if (rc == MYLITE_OK && ambiguous_projection) {
        set_ambiguous_column_reference_error(database, COLUMN_REFERENCE_ORDER, order_name);
        rc = MYLITE_ERROR;
    }

    free(order_name);
    return rc;
}

static int execute_select_from_plan(
    struct mylite_db *database,
    const struct planned_select *plan,
    mylite_result **out_result
) {
    sqlite3_stmt *statement = NULL;
    mylite_result *result = NULL;
    char *sql = NULL;
    struct result_column_metadata_context metadata_context = result_column_metadata_context_init();
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = load_result_column_metadata_context(database, plan, &metadata_context);
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        rc = append_select_result_column(database, result, plan, &metadata_context, column_index);
    }
    if (rc == MYLITE_OK) {
        rc = build_select_sql(plan, &sql);
    }
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_select_parameters(statement, plan);
    }
    while (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            break;
        }
        if (sqlite_rc != SQLITE_ROW) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            break;
        }
        rc = append_selected_sqlite_row(
            database,
            statement,
            result,
            plan->columns,
            plan->column_count
        );
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);
    result_column_metadata_context_deinit(&metadata_context);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
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
    rc = set_select_found_row_count(database, plan, result);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }
    if (plan->calc_found_rows) {
        rc = append_sql_calc_found_rows_deprecation_warning(database);
        if (rc != MYLITE_OK) {
            mylite_result_free(result);
            return rc;
        }
    }

    return finish_successful_result(database, result, out_result);
}

static bool select_statement_is_row_scalar_projection_attempt(
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *from_clause = child_at(statement, 1U);
    const struct mylite_sql_ast_node *select_item = NULL;
    bool has_dual_source =
        (from_clause != NULL && from_clause->kind == MYLITE_SQL_AST_FROM_DUAL) != 0;
    bool has_table_source =
        (from_clause != NULL && from_clause->kind == MYLITE_SQL_AST_FROM_TABLE) != 0;

    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST ||
        select_list_is_wildcard(select_list)) {
        return false;
    }
    if (has_dual_source && child_at(statement, 2U) != NULL) {
        return true;
    }

    select_item = child_at(select_list, 0U);
    while (select_item != NULL) {
        if (select_item->kind == MYLITE_SQL_AST_SELECT_ITEM &&
            (row_scalar_expression_contains_row_function(child_at(select_item, 0U)) ||
             (has_table_source &&
              row_scalar_expression_contains_rand_function(child_at(select_item, 0U))) ||
             (has_table_source &&
              row_scalar_expression_contains_any_value_function(child_at(select_item, 0U))) ||
             (has_table_source &&
              row_scalar_expression_contains_conversion_expression(child_at(select_item, 0U))) ||
             (has_table_source &&
              row_scalar_expression_is_concat_operator(child_at(select_item, 0U))) ||
             (has_table_source && row_scalar_expression_contains_date_interval_second_function(
                                      child_at(select_item, 0U)
                                  )) ||
             row_scalar_expression_contains_statement_time_function(child_at(select_item, 0U)) ||
             (has_table_source &&
              row_scalar_expression_contains_control_flow_function(child_at(select_item, 0U))) ||
             (has_table_source && row_scalar_expression_contains_integer_arithmetic_attempt(
                                      child_at(select_item, 0U)
                                  )))) {
            return true;
        }
        select_item = select_item->next_sibling;
    }

    return false;
}

static int execute_row_scalar_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result
) {
    struct planned_row_scalar_select plan = {false};
    int rc = plan_row_scalar_select(database, statement, true, &plan);

    if (rc == MYLITE_OK && apply_sql_select_limit) {
        apply_sql_select_limit_to_plan_limit(database, &plan.limit);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_modifier_warnings(database, statement);
    }
    if (rc == MYLITE_OK) {
        rc = append_session_scalar_select_warnings(database, child_at(statement, 0U));
    }
    if (rc == MYLITE_OK) {
        rc = execute_row_scalar_select_from_plan(database, &plan, out_result);
    }
    planned_row_scalar_select_deinit(&plan);

    return rc;
}

static int execute_row_scalar_select_from_plan(
    struct mylite_db *database,
    const struct planned_row_scalar_select *plan,
    mylite_result **out_result
) {
    sqlite3_stmt *statement = NULL;
    struct result_column_metadata_context metadata_context = result_column_metadata_context_init();
    mylite_result *result = NULL;
    char *sql = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    if (plan->has_source) {
        rc = load_result_column_metadata_context_for_table(
            database,
            plan->table.table_id,
            &metadata_context
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_result_columns(database, result, plan, &metadata_context);
    }
    if (rc == MYLITE_OK) {
        rc = build_row_scalar_select_sql(plan, &sql);
    }
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_row_scalar_select_parameters(statement, plan);
    }
    while (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            break;
        }
        if (sqlite_rc != SQLITE_ROW) {
            rc = row_scalar_select_step_error(database, statement, sqlite_rc);
            break;
        }
        rc = append_selected_sqlite_row(database, statement, result, NULL, 0U);
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);
    result_column_metadata_context_deinit(&metadata_context);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
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
    rc = set_row_scalar_select_found_row_count(database, plan, result);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int row_scalar_select_step_error(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int sqlite_rc
) {
    const char *message = NULL;

    if (sqlite_rc != SQLITE_ERROR || statement == NULL) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    message = sqlite3_errmsg(sqlite3_db_handle(statement));
    if (message == NULL) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }
    if (map_json_extract_row_scalar_step_error(database, message) ||
        map_json_mutation_row_scalar_step_error(database, message) ||
        map_json_search_row_scalar_step_error(database, message) ||
        map_json_type_row_scalar_step_error(database, message) ||
        map_json_constructor_row_scalar_step_error(database, message)) {
        return MYLITE_ERROR;
    }
    if (strcmp(message, MYLITE_INTEGER_ARITHMETIC_OVERFLOW_MESSAGE) == 0) {
        set_scalar_arithmetic_overflow_error(database);
        return MYLITE_ERROR;
    }
    if (strcmp(message, "CONVERT USING latin1 supports only ASCII scalar values") == 0) {
        set_unsupported_error(database, message);
        return MYLITE_ERROR;
    }

    return mylite_sqlite_status_to_mylite(sqlite_rc);
}

static bool map_json_extract_row_scalar_step_error(
    struct mylite_db *database,
    const char *message
) {
    if (strcmp(message, "Invalid data type for JSON data in JSON_EXTRACT()") == 0) {
        set_invalid_json_data_type_error(database, "JSON_EXTRACT");
        return true;
    }
    if (strcmp(message, "Invalid JSON text or JSON path in JSON_EXTRACT()") == 0) {
        set_invalid_json_function_text_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Unsupported JSON path or JSON document in JSON_EXTRACT()") == 0) {
        set_unsupported_error(database, "JSON_EXTRACT() path or document shape is not supported");
        return true;
    }
    if (strcmp(message, "Invalid data type for JSON data in JSON_VALUE()") == 0) {
        set_invalid_json_data_type_error(database, "json_value");
        return true;
    }
    if (strcmp(message, "Invalid JSON text or JSON path in JSON_VALUE()") == 0) {
        set_invalid_json_function_text_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Unsupported JSON path or JSON document in JSON_VALUE()") == 0) {
        set_unsupported_error(database, "JSON_VALUE() path or document shape is not supported");
        return true;
    }
    if (strcmp(message, "Invalid data type for JSON data in JSON_LENGTH()") == 0) {
        set_invalid_json_data_type_error(database, "JSON_LENGTH");
        return true;
    }
    if (strcmp(message, "Invalid JSON path in JSON_LENGTH()") == 0) {
        set_invalid_json_path_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Invalid JSON text or JSON path in JSON_LENGTH()") == 0) {
        set_invalid_json_function_text_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Unsupported JSON path or JSON document in JSON_LENGTH()") == 0) {
        set_unsupported_error(database, "JSON_LENGTH() path or document shape is not supported");
        return true;
    }
    if (strcmp(message, "Invalid data type for JSON data in JSON_KEYS()") == 0) {
        set_invalid_json_data_type_error(database, "JSON_KEYS");
        return true;
    }
    if (strcmp(message, "Invalid JSON path in JSON_KEYS()") == 0) {
        set_invalid_json_path_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Invalid JSON text or JSON path in JSON_KEYS()") == 0) {
        set_invalid_json_function_text_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Unsupported JSON path or JSON document in JSON_KEYS()") == 0) {
        set_unsupported_error(database, "JSON_KEYS() path or document shape is not supported");
        return true;
    }
    return false;
}

static bool map_json_mutation_row_scalar_step_error(
    struct mylite_db *database,
    const char *message
) {
    if (strcmp(message, "Invalid data type for JSON data in JSON_SET()") == 0) {
        set_invalid_json_data_type_error(database, "JSON_SET");
        return true;
    }
    if (strcmp(message, "Invalid JSON path in JSON_SET()") == 0) {
        set_invalid_json_path_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Invalid JSON text or JSON path in JSON_SET()") == 0) {
        set_invalid_json_function_text_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Unsupported JSON path or JSON document in JSON_SET()") == 0) {
        set_unsupported_error(database, "JSON_SET() path or document shape is not supported");
        return true;
    }
    if (strcmp(message, "Invalid data type for JSON data in JSON_INSERT()") == 0) {
        set_invalid_json_data_type_error(database, "JSON_INSERT");
        return true;
    }
    if (strcmp(message, "Invalid JSON path in JSON_INSERT()") == 0) {
        set_invalid_json_path_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Invalid JSON text or JSON path in JSON_INSERT()") == 0) {
        set_invalid_json_function_text_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Unsupported JSON path or JSON document in JSON_INSERT()") == 0) {
        set_unsupported_error(database, "JSON_INSERT() path or document shape is not supported");
        return true;
    }
    if (strcmp(message, "Invalid data type for JSON data in JSON_REPLACE()") == 0) {
        set_invalid_json_data_type_error(database, "JSON_REPLACE");
        return true;
    }
    if (strcmp(message, "Invalid JSON path in JSON_REPLACE()") == 0) {
        set_invalid_json_path_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Invalid JSON text or JSON path in JSON_REPLACE()") == 0) {
        set_invalid_json_function_text_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Unsupported JSON path or JSON document in JSON_REPLACE()") == 0) {
        set_unsupported_error(database, "JSON_REPLACE() path or document shape is not supported");
        return true;
    }
    if (strcmp(message, "Invalid data type for JSON data in JSON_REMOVE()") == 0) {
        set_invalid_json_data_type_error(database, "JSON_REMOVE");
        return true;
    }
    if (strcmp(message, "Invalid JSON path in JSON_REMOVE()") == 0) {
        set_invalid_json_path_error(database, 0U);
        return true;
    }
    if (strcmp(message, "JSON_REMOVE path not allowed") == 0) {
        set_json_path_not_allowed_error(database);
        return true;
    }
    if (strcmp(message, "Invalid JSON text or JSON path in JSON_REMOVE()") == 0) {
        set_invalid_json_function_text_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Unsupported JSON path or JSON document in JSON_REMOVE()") == 0) {
        set_unsupported_error(database, "JSON_REMOVE() path or document shape is not supported");
        return true;
    }
    return false;
}

static bool map_json_search_row_scalar_step_error(struct mylite_db *database, const char *message) {
    if (strcmp(message, "Invalid data type for JSON data in JSON_CONTAINS()") == 0) {
        set_invalid_json_data_type_error(database, "JSON_CONTAINS");
        return true;
    }
    if (strcmp(message, "Invalid JSON path in JSON_CONTAINS()") == 0) {
        set_invalid_json_path_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Invalid JSON text or JSON path in JSON_CONTAINS()") == 0) {
        set_invalid_json_function_text_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Unsupported JSON path or JSON document in JSON_CONTAINS()") == 0) {
        set_unsupported_error(database, "JSON_CONTAINS() path or document shape is not supported");
        return true;
    }
    if (strcmp(message, "Invalid data type for JSON data in JSON_CONTAINS_PATH()") == 0) {
        set_invalid_json_data_type_error(database, "JSON_CONTAINS_PATH");
        return true;
    }
    if (strcmp(message, "Invalid JSON path in JSON_CONTAINS_PATH()") == 0) {
        set_invalid_json_path_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Invalid oneOrAll in JSON_CONTAINS_PATH()") == 0) {
        set_invalid_json_one_or_all_error(database);
        return true;
    }
    if (strcmp(message, "Invalid JSON text or JSON path in JSON_CONTAINS_PATH()") == 0) {
        set_invalid_json_function_text_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Unsupported JSON path or JSON document in JSON_CONTAINS_PATH()") == 0) {
        set_unsupported_error(
            database,
            "JSON_CONTAINS_PATH() path or document shape is not supported"
        );
        return true;
    }
    return false;
}

static bool map_json_type_row_scalar_step_error(struct mylite_db *database, const char *message) {
    if (strcmp(message, "Invalid data type for JSON data in JSON_TYPE()") == 0) {
        set_invalid_json_data_type_error(database, "JSON_TYPE");
        return true;
    }
    if (strcmp(message, "Invalid JSON text in JSON_TYPE()") == 0) {
        set_invalid_json_function_text_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Unsupported JSON document in JSON_TYPE()") == 0) {
        set_unsupported_error(database, "JSON_TYPE() document shape is not supported");
        return true;
    }
    if (strcmp(message, "Incorrect type for argument to JSON_UNQUOTE()") == 0) {
        set_json_unquote_incorrect_type_error(database);
        return true;
    }
    if (strcmp(message, "Incorrect type for argument to JSON_QUOTE()") == 0) {
        set_json_quote_incorrect_type_error(database);
        return true;
    }
    if (strcmp(message, "Invalid JSON text in JSON_UNQUOTE()") == 0) {
        set_invalid_json_function_text_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Unsupported JSON string in JSON_UNQUOTE()") == 0) {
        set_unsupported_error(database, "JSON_UNQUOTE() JSON string shape is not supported");
        return true;
    }
    return false;
}

static bool map_json_constructor_row_scalar_step_error(
    struct mylite_db *database,
    const char *message
) {
    if (strcmp(message, "JSON documents may not contain NULL member names.") == 0) {
        set_json_null_member_name_error(database);
        return true;
    }
    if (strcmp(message, "Invalid JSON text in JSON constructor") == 0) {
        set_invalid_json_function_text_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Unsupported JSON value in JSON constructor") == 0) {
        set_unsupported_error(database, "JSON constructors support only baseline JSON values");
        return true;
    }
    return false;
}

static int append_row_scalar_result_columns(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_row_scalar_select *plan,
    const struct result_column_metadata_context *metadata_context
) {
    int rc = MYLITE_OK;

    for (size_t item_index = 0U; rc == MYLITE_OK && item_index < plan->item_count; ++item_index) {
        rc = append_row_scalar_result_column(
            database,
            result,
            plan,
            metadata_context,
            &plan->items[item_index]
        );
    }

    return rc;
}

static int append_row_scalar_result_column(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_row_scalar_select *plan,
    const struct result_column_metadata_context *metadata_context,
    const struct planned_row_scalar_select_item *item
) {
    char *column_name = NULL;
    struct mylite_result_column_descriptor descriptor;
    int rc = copy_row_scalar_result_column_name(database, item, &column_name);

    if (rc == MYLITE_OK) {
        rc = make_row_scalar_result_column_descriptor(
            database,
            plan,
            metadata_context,
            item,
            column_name,
            &descriptor
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_column_descriptor(result, &descriptor);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    free(column_name);

    return rc;
}

static int make_row_scalar_result_column_descriptor(
    struct mylite_db *database,
    const struct planned_row_scalar_select *plan,
    const struct result_column_metadata_context *metadata_context,
    const struct planned_row_scalar_select_item *item,
    const char *label,
    struct mylite_result_column_descriptor *out_descriptor
) {
    if (out_descriptor == NULL) {
        return MYLITE_MISUSE;
    }
    *out_descriptor = unknown_result_column_descriptor(label);
    if (item == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (item->expression.kind == PLANNED_ROW_SCALAR_EXPRESSION_COLUMN) {
        return make_row_scalar_column_result_column_descriptor(
            database,
            plan,
            metadata_context,
            item,
            label,
            out_descriptor
        );
    }

    return populate_row_scalar_expression_result_column_descriptor(
        database,
        &item->expression,
        out_descriptor
    );
}

static int make_row_scalar_column_result_column_descriptor(
    struct mylite_db *database,
    const struct planned_row_scalar_select *plan,
    const struct result_column_metadata_context *metadata_context,
    const struct planned_row_scalar_select_item *item,
    const char *label,
    struct mylite_result_column_descriptor *out_descriptor
) {
    const char *table_name = NULL;

    if (plan == NULL || item == NULL) {
        return MYLITE_MISUSE;
    }
    *out_descriptor = unknown_result_column_descriptor(label);
    if (metadata_context == NULL || !metadata_context->has_single_source_metadata) {
        return MYLITE_OK;
    }

    table_name = plan->source.table_name;
    if (plan->source_has_alias) {
        table_name = plan->source_alias;
    }
    out_descriptor->schema_name = plan->source.schema.name;
    out_descriptor->table_name = table_name;
    out_descriptor->origin_schema_name = plan->source.schema.name;
    out_descriptor->origin_table_name = plan->table.name;
    out_descriptor->origin_column_name = item->expression.column.name;
    out_descriptor->nullable = item->expression.column.is_nullable;

    return populate_select_result_column_descriptor(
        database,
        &plan->table,
        &item->expression.column,
        metadata_context,
        out_descriptor
    );
}

static int populate_row_scalar_expression_result_column_descriptor(
    struct mylite_db *database,
    const struct planned_row_scalar_expression *expression,
    struct mylite_result_column_descriptor *descriptor
) {
    if (expression == NULL || descriptor == NULL) {
        return MYLITE_MISUSE;
    }

    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
        return populate_window_function_result_column_descriptor(database, expression, descriptor);
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF:
        populate_scalar_binary_numeric_result_column_descriptor(
            descriptor,
            (struct scalar_binary_numeric_result_column_shape){
                .logical_type = MYLITE_RESULT_LOGICAL_TYPE_LONGLONG,
                .display_length = mysql_scalar_bigint_display_length,
                .decimals = 0U,
                .extra_flags = 0U,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE:
        populate_scalar_connection_string_result_column_descriptor(
            database,
            descriptor,
            (struct scalar_connection_string_result_column_shape){
                .display_length = mysql_regexp_string_function_display_length *
                                  scalar_connection_max_bytes_per_character(database),
                .extra_flags = 0U,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
        populate_scalar_binary_numeric_result_column_descriptor(
            descriptor,
            (struct scalar_binary_numeric_result_column_shape){
                .logical_type = MYLITE_RESULT_LOGICAL_TYPE_DOUBLE,
                .display_length = mysql_scalar_double_display_length,
                .decimals = mysql_approximate_decimals,
                .extra_flags = 0U,
                .nullable = false,
            }
        );
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
        populate_uuid_string_result_column_descriptor(descriptor, true);
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
        descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING;
        descriptor->charset_id = mysql_collation_binary_id;
        descriptor->collation_id = mysql_collation_binary_id;
        descriptor->display_length = MYLITE_UUID_BINARY_SIZE;
        descriptor->decimals = mysql_approximate_decimals;
        descriptor->flags = MYLITE_RESULT_COLUMN_FLAG_BINARY;
        descriptor->nullable = true;
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
        populate_scalar_connection_string_result_column_descriptor(
            database,
            descriptor,
            (struct scalar_connection_string_result_column_shape){
                .display_length = MYLITE_UUID_TEXT_SIZE,
                .extra_flags = 0U,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
        populate_scalar_connection_string_result_column_descriptor(
            database,
            descriptor,
            (struct scalar_connection_string_result_column_shape){
                .display_length = mysql_json_type_display_length,
                .extra_flags = MYLITE_RESULT_COLUMN_FLAG_BINARY,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
        populate_scalar_connection_string_result_column_descriptor(
            database,
            descriptor,
            (struct scalar_connection_string_result_column_shape){
                .display_length = mysql_json_document_display_length,
                .extra_flags = MYLITE_RESULT_COLUMN_FLAG_BINARY,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
        populate_scalar_connection_string_result_column_descriptor(
            database,
            descriptor,
            (struct scalar_connection_string_result_column_shape){
                .display_length = mysql_json_value_display_character_length *
                                  scalar_connection_max_bytes_per_character(database),
                .extra_flags = MYLITE_RESULT_COLUMN_FLAG_BINARY,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
        populate_scalar_json_result_column_descriptor(database, descriptor);
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ:
        populate_scalar_temporal_string_result_column_descriptor(database, descriptor);
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
        descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING;
        descriptor->charset_id = mysql_collation_binary_id;
        descriptor->collation_id = mysql_collation_binary_id;
        descriptor->display_length = mysql_scalar_bigint_display_length;
        descriptor->decimals = mysql_approximate_decimals;
        descriptor->flags = MYLITE_RESULT_COLUMN_FLAG_BINARY;
        descriptor->nullable = true;
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
        return populate_row_scalar_date_interval_second_result_column_descriptor(
            database,
            expression,
            descriptor
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
        return populate_row_scalar_conversion_result_column_descriptor(
            database,
            expression,
            descriptor
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT:
        if (mylite_temporal_extract_kind_is_calendar_name(expression->temporal_extract_kind)) {
            populate_calendar_name_result_column_descriptor(database, descriptor);
        }
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_EQUAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIND_IN_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_INSERT:
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_LIKE:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBSTRING_INDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_PADDING:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_BITMASK:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REVERSE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SOUNDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRCMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
        return MYLITE_OK;
    }

    return MYLITE_OK;
}

static int populate_window_function_result_column_descriptor(
    struct mylite_db *database,
    const struct planned_row_scalar_expression *expression,
    struct mylite_result_column_descriptor *descriptor
) {
    if (expression == NULL || descriptor == NULL) {
        return MYLITE_MISUSE;
    }

    switch (expression->window_function_kind) {
    case PLANNED_WINDOW_FUNCTION_ROW_NUMBER:
    case PLANNED_WINDOW_FUNCTION_RANK:
    case PLANNED_WINDOW_FUNCTION_DENSE_RANK:
    case PLANNED_WINDOW_FUNCTION_NTILE:
        populate_scalar_binary_numeric_result_column_descriptor(
            descriptor,
            (struct scalar_binary_numeric_result_column_shape){
                .logical_type = MYLITE_RESULT_LOGICAL_TYPE_LONGLONG,
                .display_length = mysql_scalar_bigint_display_length,
                .decimals = 0U,
                .extra_flags = MYLITE_RESULT_COLUMN_FLAG_UNSIGNED,
                .nullable = false,
            }
        );
        return MYLITE_OK;
    case PLANNED_WINDOW_FUNCTION_PERCENT_RANK:
    case PLANNED_WINDOW_FUNCTION_CUME_DIST:
        populate_scalar_binary_numeric_result_column_descriptor(
            descriptor,
            (struct scalar_binary_numeric_result_column_shape){
                .logical_type = MYLITE_RESULT_LOGICAL_TYPE_DOUBLE,
                .display_length = mysql_scalar_double_display_length,
                .decimals = mysql_approximate_decimals,
                .extra_flags = 0U,
                .nullable = false,
            }
        );
        return MYLITE_OK;
    case PLANNED_WINDOW_FUNCTION_LAG:
    case PLANNED_WINDOW_FUNCTION_LEAD:
    case PLANNED_WINDOW_FUNCTION_FIRST_VALUE:
    case PLANNED_WINDOW_FUNCTION_LAST_VALUE:
    case PLANNED_WINDOW_FUNCTION_NTH_VALUE:
        return populate_window_value_function_result_column_descriptor(
            database,
            expression,
            descriptor
        );
    case PLANNED_WINDOW_FUNCTION_NONE:
        break;
    }

    set_parse_error(database, NULL);
    return MYLITE_ERROR;
}

static int populate_window_value_function_result_column_descriptor(
    struct mylite_db *database,
    const struct planned_row_scalar_expression *expression,
    struct mylite_result_column_descriptor *descriptor
) {
    const struct planned_row_scalar_expression *argument = NULL;

    if (expression == NULL || descriptor == NULL || expression->argument_count == 0U ||
        expression->arguments == NULL) {
        return MYLITE_MISUSE;
    }

    argument = &expression->arguments[0];
    switch (argument->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return populate_window_value_column_result_column_descriptor(
            database,
            &argument->column,
            descriptor
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
        return populate_window_value_literal_result_column_descriptor(
            database,
            &argument->value,
            descriptor
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_EQUAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_PADDING:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_BITMASK:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_INSERT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REVERSE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SOUNDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBSTRING_INDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIND_IN_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRCMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_LIKE:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
        break;
    }

    set_unsupported_error(
        database,
        "window value metadata supports only literal and column values"
    );
    return MYLITE_ERROR;
}

static int populate_window_value_column_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct mylite_result_column_descriptor *descriptor
) {
    bool matched = false;
    int rc = populate_numeric_result_column_descriptor(database, column, descriptor, &matched);

    if (rc == MYLITE_OK && !matched) {
        rc = populate_string_result_column_descriptor(database, NULL, column, descriptor, &matched);
    }
    if (rc == MYLITE_OK && !matched) {
        rc = populate_binary_result_column_descriptor(database, column, descriptor, &matched);
    }
    if (rc == MYLITE_OK && !matched) {
        rc = populate_temporal_result_column_descriptor(column, descriptor, &matched);
    }
    if (rc == MYLITE_OK && !matched) {
        rc = populate_bit_result_column_descriptor(column, descriptor, &matched);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!matched) {
        set_unsupported_error(
            database,
            "window value metadata supports only current baseline column descriptors"
        );
        return MYLITE_ERROR;
    }

    descriptor->nullable = true;
    descriptor->flags &= ~((uint32_t)MYLITE_RESULT_COLUMN_FLAG_NOT_NULL);
    return MYLITE_OK;
}

static int populate_window_value_literal_result_column_descriptor(
    struct mylite_db *database,
    const struct planned_value *value,
    struct mylite_result_column_descriptor *descriptor
) {
    uint64_t display_length = 0U;

    if (database == NULL || value == NULL || descriptor == NULL) {
        return MYLITE_MISUSE;
    }
    if (value->is_null) {
        populate_scalar_binary_numeric_result_column_descriptor(
            descriptor,
            (struct scalar_binary_numeric_result_column_shape){
                .logical_type = MYLITE_RESULT_LOGICAL_TYPE_NULL,
                .display_length = 0U,
                .decimals = 0U,
                .extra_flags = 0U,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    }
    if (value->is_text) {
        if (value->text_length > UINT64_MAX / scalar_connection_max_bytes_per_character(database)) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        display_length =
            (uint64_t)value->text_length * scalar_connection_max_bytes_per_character(database);
        populate_scalar_connection_string_result_column_descriptor(
            database,
            descriptor,
            (struct scalar_connection_string_result_column_shape){
                .display_length = display_length,
                .extra_flags = 0U,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    }
    if (value->is_real) {
        populate_scalar_binary_numeric_result_column_descriptor(
            descriptor,
            (struct scalar_binary_numeric_result_column_shape){
                .logical_type = MYLITE_RESULT_LOGICAL_TYPE_DOUBLE,
                .display_length = mysql_scalar_double_display_length,
                .decimals = mysql_approximate_decimals,
                .extra_flags = 0U,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    }

    populate_scalar_binary_numeric_result_column_descriptor(
        descriptor,
        (struct scalar_binary_numeric_result_column_shape){
            .logical_type = MYLITE_RESULT_LOGICAL_TYPE_LONGLONG,
            .display_length = mysql_scalar_bigint_display_length,
            .decimals = 0U,
            .extra_flags = 0U,
            .nullable = true,
        }
    );
    return MYLITE_OK;
}

static int populate_row_scalar_conversion_result_column_descriptor(
    const struct mylite_db *database,
    const struct planned_row_scalar_expression *expression,
    struct mylite_result_column_descriptor *descriptor
) {
    if (database == NULL || expression == NULL || descriptor == NULL) {
        return MYLITE_MISUSE;
    }

    switch (expression->conversion_kind) {
    case PLANNED_ROW_SCALAR_CONVERSION_BINARY:
    case PLANNED_ROW_SCALAR_CONVERSION_USING_BINARY:
        descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING;
        descriptor->charset_id = mysql_collation_binary_id;
        descriptor->collation_id = mysql_collation_binary_id;
        descriptor->display_length = mysql_scalar_bigint_display_length;
        descriptor->decimals = mysql_approximate_decimals;
        descriptor->flags = MYLITE_RESULT_COLUMN_FLAG_BINARY;
        descriptor->nullable = true;
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_CONVERSION_CHAR:
        populate_scalar_connection_string_result_column_descriptor(
            database,
            descriptor,
            (struct scalar_connection_string_result_column_shape){
                .display_length = mysql_scalar_bigint_display_length *
                                  scalar_connection_max_bytes_per_character(database),
                .extra_flags = 0U,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_CONVERSION_USING_CHARSET:
        descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING;
        descriptor->charset_id = result_metadata_collation_id(expression->conversion_collation);
        descriptor->collation_id = descriptor->charset_id;
        descriptor->display_length = mysql_scalar_bigint_display_length *
                                     scalar_connection_max_bytes_per_character(database);
        descriptor->decimals = mysql_approximate_decimals;
        descriptor->flags = 0U;
        descriptor->nullable = true;
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_CONVERSION_SIGNED:
        populate_scalar_binary_numeric_result_column_descriptor(
            descriptor,
            (struct scalar_binary_numeric_result_column_shape){
                .logical_type = MYLITE_RESULT_LOGICAL_TYPE_LONGLONG,
                .display_length = mysql_scalar_bigint_display_length,
                .decimals = 0U,
                .extra_flags = 0U,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_CONVERSION_UNSIGNED:
        populate_scalar_binary_numeric_result_column_descriptor(
            descriptor,
            (struct scalar_binary_numeric_result_column_shape){
                .logical_type = MYLITE_RESULT_LOGICAL_TYPE_LONGLONG,
                .display_length = mysql_scalar_bigint_display_length,
                .decimals = 0U,
                .extra_flags = MYLITE_RESULT_COLUMN_FLAG_UNSIGNED,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_CONVERSION_NONE:
        break;
    }

    return MYLITE_ERROR;
}

static int populate_row_scalar_date_interval_second_result_column_descriptor(
    struct mylite_db *database,
    const struct planned_row_scalar_expression *expression,
    struct mylite_result_column_descriptor *descriptor
) {
    const struct planned_row_scalar_expression *input_kind_argument = NULL;
    const struct planned_row_scalar_expression *unit_argument = NULL;
    enum mylite_date_interval_second_input_kind input_kind =
        MYLITE_DATE_INTERVAL_SECOND_INPUT_STRING;
    enum mylite_date_interval_unit unit = MYLITE_DATE_INTERVAL_UNIT_SECOND;

    if (expression == NULL || descriptor == NULL) {
        return MYLITE_MISUSE;
    }
    if (expression->argument_count <= 3U || expression->arguments == NULL) {
        populate_scalar_temporal_string_result_column_descriptor(database, descriptor);
        return MYLITE_OK;
    }
    input_kind_argument = &expression->arguments[1];
    unit_argument = &expression->arguments[3];
    if (input_kind_argument->kind != PLANNED_ROW_SCALAR_EXPRESSION_VALUE ||
        input_kind_argument->value.is_null || !input_kind_argument->value.is_text ||
        !mylite_date_interval_second_input_kind_from_name(
            input_kind_argument->value.text,
            input_kind_argument->value.text_length,
            &input_kind
        ) ||
        unit_argument->kind != PLANNED_ROW_SCALAR_EXPRESSION_VALUE ||
        unit_argument->value.is_null || !unit_argument->value.is_text ||
        !mylite_date_interval_unit_from_name(
            unit_argument->value.text,
            unit_argument->value.text_length,
            &unit
        )) {
        populate_scalar_temporal_string_result_column_descriptor(database, descriptor);
        return MYLITE_OK;
    }

    switch (input_kind) {
    case MYLITE_DATE_INTERVAL_SECOND_INPUT_DATE:
        if (mylite_date_interval_unit_has_time_part(unit)) {
            populate_scalar_datetime_result_column_descriptor(descriptor);
        } else {
            populate_scalar_date_result_column_descriptor(descriptor);
        }
        return MYLITE_OK;
    case MYLITE_DATE_INTERVAL_SECOND_INPUT_DATETIME:
    case MYLITE_DATE_INTERVAL_SECOND_INPUT_TIMESTAMP:
        populate_scalar_datetime_result_column_descriptor(descriptor);
        return MYLITE_OK;
    case MYLITE_DATE_INTERVAL_SECOND_INPUT_STRING:
    default:
        populate_scalar_temporal_string_result_column_descriptor(database, descriptor);
        return MYLITE_OK;
    }
}

static int copy_row_scalar_result_column_name(
    struct mylite_db *database,
    const struct planned_row_scalar_select_item *item,
    char **out_name
) {
    const struct mylite_sql_ast_node *source_expression = NULL;

    if (out_name == NULL) {
        return MYLITE_MISUSE;
    }
    *out_name = NULL;
    if (item == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    source_expression = unwrap_parenthesized_expression(item->source_expression);
    if (item->alias != NULL) {
        return copy_select_item_alias_text(database, item->alias, out_name);
    }
    if (source_expression != NULL && source_expression->kind == MYLITE_SQL_AST_ANY_VALUE_FUNCTION) {
        return copy_scalar_projection_column_name(database, item->source_expression, out_name);
    }
    if (item->expression.kind == PLANNED_ROW_SCALAR_EXPRESSION_COLUMN) {
        return duplicate_text(database, item->expression.column.name, out_name);
    }

    return copy_scalar_projection_column_name(database, item->source_expression, out_name);
}

static int set_row_scalar_select_found_row_count(
    struct mylite_db *database,
    const struct planned_row_scalar_select *plan,
    mylite_result *result
) {
    struct planned_select count_plan = {0};
    uint64_t found_row_count = 0U;
    int rc = MYLITE_OK;

    if (!plan->has_source) {
        if (mylite_result_row_count(result) > UINT64_MAX) {
            set_runtime_error(database, "SELECT found-row count is out of range");
            return MYLITE_ERROR;
        }
        mylite_result_set_found_row_count(result, (uint64_t)mylite_result_row_count(result));
        return MYLITE_OK;
    }

    count_plan.table = plan->table;
    count_plan.predicate = plan->predicate;
    count_plan.limit = plan->limit;
    rc = found_row_count_for_select_limit_envelope(
        database,
        &count_plan,
        mylite_result_row_count(result),
        &found_row_count
    );
    if (rc == MYLITE_OK) {
        mylite_result_set_found_row_count(result, found_row_count);
    }
    return rc;
}

static int set_select_found_row_count(
    struct mylite_db *database,
    const struct planned_select *plan,
    mylite_result *result
) {
    int64_t count = 0;
    uint64_t found_row_count = 0U;
    int rc = MYLITE_OK;

    if (plan->calc_found_rows) {
        rc = read_select_found_row_count(database, plan, &count);
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (count < 0) {
            set_runtime_error(database, "invalid SQL_CALC_FOUND_ROWS count");
            return MYLITE_ERROR;
        }
        mylite_result_set_found_row_count(result, (uint64_t)count);
        return MYLITE_OK;
    }

    rc = found_row_count_for_select_limit_envelope(
        database,
        plan,
        mylite_result_row_count(result),
        &found_row_count
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    mylite_result_set_found_row_count(result, found_row_count);
    return MYLITE_OK;
}

static int found_row_count_for_select_limit_envelope(
    struct mylite_db *database,
    const struct planned_select *plan,
    size_t visible_row_count,
    uint64_t *out_found_row_count
) {
    uint64_t visible_rows = (uint64_t)visible_row_count;
    uint64_t offset = 0U;
    int64_t total_count = 0;
    int rc = MYLITE_OK;

    if (out_found_row_count == NULL) {
        return MYLITE_MISUSE;
    }
    *out_found_row_count = 0U;
    if (visible_row_count > UINT64_MAX) {
        set_runtime_error(database, "SELECT found-row count is out of range");
        return MYLITE_ERROR;
    }
    if (!plan->limit.has_limit) {
        *out_found_row_count = visible_rows;
        return MYLITE_OK;
    }
    if (!plan->limit.has_offset) {
        *out_found_row_count = visible_rows;
        return MYLITE_OK;
    }

    offset = (uint64_t)plan->limit.offset;
    if (plan->limit.row_count != 0 && visible_row_count != 0U) {
        if (offset > UINT64_MAX - visible_rows) {
            set_runtime_error(database, "SELECT found-row count is out of range");
            return MYLITE_ERROR;
        }
        *out_found_row_count = offset + visible_rows;
        return MYLITE_OK;
    }
    if (offset == 0U) {
        *out_found_row_count = 0U;
        return MYLITE_OK;
    }

    rc = read_select_found_row_count(database, plan, &total_count);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (total_count < 0) {
        set_runtime_error(database, "invalid SELECT found-row count");
        return MYLITE_ERROR;
    }
    *out_found_row_count = offset < (uint64_t)total_count ? offset : (uint64_t)total_count;
    return MYLITE_OK;
}

static int read_select_found_row_count(
    struct mylite_db *database,
    const struct planned_select *plan,
    int64_t *out_count
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int parameter_index = 1;
    int rc = build_select_found_rows_sql(plan, &sql);

    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_select_predicate_parameters(statement, &plan->predicate, &parameter_index);
    }
    if (rc == MYLITE_OK) {
        rc = step_count_statement(statement, out_count);
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
        return rc;
    }
    if (rc != MYLITE_OK &&
        mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) == MYLITE_OK) {
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }
    return rc;
}

static bool select_statement_has_count_aggregate(const struct mylite_sql_ast_node *statement) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *select_item = NULL;

    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        return false;
    }

    select_item = child_at(select_list, 0U);
    while (select_item != NULL) {
        const struct mylite_sql_ast_node *expression = child_at(select_item, 0U);

        expression = unwrap_parenthesized_expression(expression);
        if (count_function_from_expression(expression) != PLANNED_COUNT_NONE) {
            return true;
        }
        select_item = select_item->next_sibling;
    }

    return false;
}

static int execute_grouped_aggregate_from_plan(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *plan,
    mylite_result **out_result
) {
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = append_grouped_aggregate_result_columns(database, result, plan);
    if (rc == MYLITE_OK) {
        rc = read_grouped_aggregate_from_source(database, plan, result);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return grouped_aggregate_execution_error(database, rc);
    }

    return finish_successful_result(database, result, out_result);
}

static int append_grouped_aggregate_result_columns(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_grouped_aggregate *plan
) {
    int rc = MYLITE_OK;

    for (size_t projection_index = 0U; rc == MYLITE_OK && projection_index < plan->projection_count;
         ++projection_index) {
        const char *projection_column_name = plan->projections[projection_index].column.name;
        char *projection_alias_text = NULL;

        if (plan->projections[projection_index].alias != NULL) {
            rc = copy_select_item_alias_text(
                database,
                plan->projections[projection_index].alias,
                &projection_alias_text
            );
            if (rc != MYLITE_OK) {
                return rc;
            }
            projection_column_name = projection_alias_text;
        }
        rc = mylite_result_append_column(result, projection_column_name);
        free(projection_alias_text);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }

    for (size_t aggregate_index = 0U; rc == MYLITE_OK && aggregate_index < plan->aggregate_count;
         ++aggregate_index) {
        char *aggregate_column_name = NULL;
        const struct planned_grouped_aggregate_item *item = &plan->aggregates[aggregate_index];

        if (item->alias != NULL) {
            rc = copy_select_item_alias_text(database, item->alias, &aggregate_column_name);
        } else {
            rc = copy_aggregate_result_column_name(
                database,
                &item->expression->span,
                &aggregate_column_name
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_result_append_column(result, aggregate_column_name);
            if (rc != MYLITE_OK) {
                set_nomem_error(database);
            }
        }
        free(aggregate_column_name);
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return rc;
}

static int plan_count(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_count *out_plan
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *select_item = child_at(select_list, 0U);
    const struct mylite_sql_ast_node *from_clause = child_at(statement, 1U);
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *optional_clause = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    const struct mylite_sql_ast_node *count_expression = NULL;
    struct planned_count_source_nodes source_nodes = {0};
    int rc = MYLITE_OK;

    *out_plan = (struct planned_count){false};
    expression = child_at(select_item, 0U);
    out_plan->expression = expression;
    out_plan->alias = child_at(select_item, 1U);
    count_expression = unwrap_parenthesized_expression(expression);
    out_plan->function = count_function_from_expression(count_expression);
    if (mylite_sql_ast_node_child_count(select_list) != 1U) {
        set_unsupported_error(database, count_exactly_one_message(out_plan->function));
        return MYLITE_ERROR;
    }
    if (out_plan->function == PLANNED_COUNT_NONE) {
        set_unsupported_error(database, count_exactly_one_message(out_plan->function));
        return MYLITE_ERROR;
    }
    if (out_plan->function == PLANNED_COUNT_LITERAL) {
        rc = plan_count_literal(database, count_expression, &out_plan->count_literal);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    optional_clause = child_at(statement, 2U);
    while (optional_clause != NULL) {
        if (optional_clause->kind != MYLITE_SQL_AST_WHERE_CLAUSE) {
            set_unsupported_error(database, count_supported_clauses_message(out_plan->function));
            return MYLITE_ERROR;
        }
        where_clause = optional_clause;
        optional_clause = optional_clause->next_sibling;
    }

    source_nodes.from_clause = from_clause;
    source_nodes.where_clause = where_clause;
    source_nodes.count_expression = count_expression;
    if (from_clause == NULL || from_clause->kind == MYLITE_SQL_AST_FROM_DUAL) {
        return plan_count_without_source(database, &source_nodes, out_plan);
    }
    if (from_clause->kind != MYLITE_SQL_AST_FROM_TABLE) {
        set_unsupported_error(database, count_descriptor_table_message(out_plan->function));
        return MYLITE_ERROR;
    }

    return plan_count_table_source(database, &source_nodes, out_plan);
}

static void planned_count_deinit(struct planned_count *plan) {
    if (plan == NULL) {
        return;
    }

    planned_select_predicate_deinit(&plan->predicate);
    planned_value_deinit(&plan->count_literal);
    *plan = (struct planned_count){false};
}

static int plan_count_without_source(
    struct mylite_db *database,
    const struct planned_count_source_nodes *nodes,
    struct planned_count *out_plan
) {
    char column_name[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int rc = MYLITE_OK;

    if (out_plan->function == PLANNED_COUNT_COLUMN ||
        out_plan->function == PLANNED_COUNT_DISTINCT_COLUMN) {
        const struct mylite_sql_ast_node *column_node = child_at(nodes->count_expression, 0U);
        char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
        size_t part_count = 0U;

        rc = collect_column_reference_parts(database, column_node, parts, &part_count);
        if (rc == MYLITE_OK) {
            rc = format_column_reference_name(
                database,
                parts,
                part_count,
                column_name,
                sizeof(column_name)
            );
        }
        if (rc == MYLITE_OK) {
            set_unknown_column_error(database, column_name);
            rc = MYLITE_ERROR;
        }
        return rc;
    }
    if (nodes->where_clause != NULL) {
        if (out_plan->function == PLANNED_COUNT_LITERAL) {
            set_unsupported_error(
                database,
                "COUNT(literal) WHERE requires a descriptor-backed table"
            );
        } else {
            set_unsupported_error(database, "COUNT(*) WHERE requires a descriptor-backed table");
        }
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int plan_count_table_source(
    struct mylite_db *database,
    const struct planned_count_source_nodes *nodes,
    struct planned_count *out_plan
) {
    struct mylite_catalog_column_descriptor *table_columns = NULL;
    struct select_source_context source_context = {0};
    size_t table_column_count = 0U;
    int rc = MYLITE_OK;

    out_plan->has_source = true;
    rc = resolve_visible_table_reference(
        database,
        child_at(nodes->from_clause, 0U),
        &out_plan->source,
        &out_plan->table
    );
    if (rc == MYLITE_OK) {
        rc = validate_select_index_hints(database, nodes->from_clause, &out_plan->table);
    }
    if (rc == MYLITE_OK) {
        rc = init_select_source_context(
            database,
            nodes->from_clause,
            &out_plan->source,
            &source_context
        );
    }
    if (rc == MYLITE_OK &&
        (nodes->where_clause != NULL || out_plan->function == PLANNED_COUNT_COLUMN ||
         out_plan->function == PLANNED_COUNT_DISTINCT_COLUMN)) {
        rc = load_table_columns(
            database,
            out_plan->table.table_id,
            &table_columns,
            &table_column_count
        );
    }
    if (rc == MYLITE_OK && (out_plan->function == PLANNED_COUNT_COLUMN ||
                            out_plan->function == PLANNED_COUNT_DISTINCT_COLUMN)) {
        rc = plan_count_column(
            database,
            nodes->count_expression,
            &source_context,
            table_columns,
            table_column_count,
            &out_plan->count_column
        );
    }
    if (rc == MYLITE_OK && out_plan->function == PLANNED_COUNT_DISTINCT_COLUMN) {
        struct integer_column_range range = {0};

        rc = integer_range_for_column(
            database,
            &out_plan->count_column,
            "COUNT(DISTINCT column) supports only integer descriptor columns",
            &range
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_select_predicate(
            database,
            nodes->where_clause,
            &source_context,
            table_columns,
            table_column_count,
            &out_plan->predicate
        );
    }

    free(table_columns);
    return rc;
}

static enum planned_count_function count_function_from_expression(
    const struct mylite_sql_ast_node *expression
) {
    if (expression == NULL) {
        return PLANNED_COUNT_NONE;
    }
    if (expression->kind == MYLITE_SQL_AST_COUNT_STAR_FUNCTION) {
        return PLANNED_COUNT_STAR;
    }
    if (expression->kind == MYLITE_SQL_AST_COUNT_COLUMN_FUNCTION) {
        return PLANNED_COUNT_COLUMN;
    }
    if (expression->kind == MYLITE_SQL_AST_COUNT_LITERAL_FUNCTION) {
        return PLANNED_COUNT_LITERAL;
    }
    if (expression->kind == MYLITE_SQL_AST_COUNT_DISTINCT_COLUMN_FUNCTION) {
        return PLANNED_COUNT_DISTINCT_COLUMN;
    }

    return PLANNED_COUNT_NONE;
}

static const char *count_exactly_one_message(enum planned_count_function function) {
    if (function == PLANNED_COUNT_STAR) {
        return "COUNT(*) supports exactly one aggregate select item";
    }
    if (function == PLANNED_COUNT_COLUMN) {
        return "COUNT(column) supports exactly one aggregate select item";
    }
    if (function == PLANNED_COUNT_LITERAL) {
        return "COUNT(literal) supports exactly one aggregate select item";
    }
    if (function == PLANNED_COUNT_DISTINCT_COLUMN) {
        return "COUNT(DISTINCT column) supports exactly one aggregate select item";
    }

    return "COUNT aggregate supports exactly one aggregate select item";
}

static const char *count_supported_clauses_message(enum planned_count_function function) {
    if (function == PLANNED_COUNT_DISTINCT_COLUMN) {
        return "COUNT(DISTINCT column) supports only WHERE";
    }
    if (function == PLANNED_COUNT_COLUMN) {
        return "COUNT(column) supports only WHERE";
    }
    if (function == PLANNED_COUNT_LITERAL) {
        return "COUNT(literal) supports only WHERE";
    }

    return "COUNT(*) supports only WHERE";
}

static const char *count_descriptor_table_message(enum planned_count_function function) {
    if (function == PLANNED_COUNT_DISTINCT_COLUMN) {
        return "COUNT(DISTINCT column) supports only descriptor-backed table reads";
    }
    if (function == PLANNED_COUNT_COLUMN) {
        return "COUNT(column) supports only descriptor-backed table reads";
    }
    if (function == PLANNED_COUNT_LITERAL) {
        return "COUNT(literal) supports only descriptor-backed table reads";
    }

    return "COUNT(*) supports only descriptor-backed table reads";
}

static int plan_count_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    struct planned_value *out_literal
) {
    const struct mylite_sql_ast_node *argument = child_at(function, 0U);
    const struct mylite_sql_ast_node *literal = argument;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    *out_literal = (struct planned_value){.is_null = false, .integer = 1};
    if (argument == NULL) {
        set_unsupported_error(
            database,
            "COUNT(literal) supports only integer, boolean, and NULL literals"
        );
        return MYLITE_ERROR;
    }
    if (argument->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(argument);

        if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
            operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            set_unsupported_error(
                database,
                "COUNT(literal) supports only integer, boolean, and NULL literals"
            );
            return MYLITE_ERROR;
        }
        literal = child_at(argument, 0U);
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "COUNT(literal) supports only integer, boolean, and NULL literals"
        );
        return MYLITE_ERROR;
    }
    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        out_literal->is_null = true;
        out_literal->integer = 0;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        out_literal->integer = 1;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        out_literal->integer = 0;
        return MYLITE_OK;
    }
    if (literal_kind != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_unsupported_error(
            database,
            "COUNT(literal) supports only integer, boolean, and NULL literals"
        );
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int plan_count_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column
) {
    const struct mylite_sql_ast_node *column_node = child_at(function, 0U);

    return resolve_descriptor_column_reference(
        database,
        column_node,
        source_context,
        COLUMN_REFERENCE_FIELD,
        "COUNT(column) supports only descriptor columns",
        table_columns,
        table_column_count,
        out_column
    );
}

static int execute_count_from_plan(
    struct mylite_db *database,
    const struct planned_count *plan,
    bool apply_sql_select_limit,
    mylite_result **out_result
) {
    mylite_result *result = NULL;
    char count_text[integer_text_capacity];
    int64_t count_value = 1;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }
    if (plan->function == PLANNED_COUNT_LITERAL && plan->count_literal.is_null) {
        count_value = 0;
    }

    rc = append_count_result_column(database, result, plan);
    if (rc == MYLITE_OK && plan->has_source) {
        rc = read_count_from_source(database, plan, &count_value);
    }
    if (rc == MYLITE_OK) {
        rc = format_count_value(database, count_value, count_text, sizeof(count_text));
    }
    if (rc == MYLITE_OK) {
        rc = append_count_value_row(database, result, count_text);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return count_execution_error(database, rc);
    }
    if (apply_sql_select_limit) {
        apply_sql_select_limit_to_result(database, result);
    }

    return finish_successful_result(database, result, out_result);
}

static int append_count_result_column(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_count *plan
) {
    char *column_name = NULL;
    int rc = MYLITE_OK;

    if (plan->alias != NULL) {
        rc = copy_select_item_alias_text(database, plan->alias, &column_name);
    } else {
        rc = copy_aggregate_result_column_name(database, &plan->expression->span, &column_name);
    }

    if (rc == MYLITE_OK) {
        rc = mylite_result_append_column(result, column_name);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }

    free(column_name);
    return rc;
}

static int copy_aggregate_result_column_name(
    struct mylite_db *database,
    const struct mylite_sql_source_span *span,
    char **out_text
) {
    char *text = NULL;
    size_t extra_spaces = 0U;

    if (out_text == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    if (span == NULL || span->text == NULL || span->length == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (span->length == SIZE_MAX) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    extra_spaces = aggregate_label_extra_spaces_after_block_comments(span);
    if (extra_spaces > SIZE_MAX - span->length - 1U) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    text = (char *)malloc(span->length + extra_spaces + 1U);
    if (text == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    copy_aggregate_label_with_spacing(span, text);
    *out_text = text;

    return MYLITE_OK;
}

static size_t aggregate_label_extra_spaces_after_block_comments(
    const struct mylite_sql_source_span *span
) {
    size_t extra_spaces = 0U;
    size_t source_index = 0U;

    while (source_index + 1U < span->length) {
        if (span->text[source_index] == '/' && span->text[source_index + 1U] == '*') {
            source_index += 2U;
            while (source_index + 1U < span->length &&
                   (span->text[source_index] != '*' || span->text[source_index + 1U] != '/')) {
                ++source_index;
            }
            if (source_index + 1U < span->length) {
                source_index += 2U;
                if (source_index < span->length &&
                    aggregate_label_needs_space_after_block_comment(span->text[source_index])) {
                    ++extra_spaces;
                }
            }
        } else {
            ++source_index;
        }
    }

    return extra_spaces;
}

static void copy_aggregate_label_with_spacing(
    const struct mylite_sql_source_span *span,
    char *destination
) {
    size_t destination_index = 0U;
    size_t source_index = 0U;

    while (source_index < span->length) {
        if (source_index + 1U < span->length && span->text[source_index] == '/' &&
            span->text[source_index + 1U] == '*') {
            destination[destination_index] = span->text[source_index];
            destination[destination_index + 1U] = span->text[source_index + 1U];
            destination_index += 2U;
            source_index += 2U;
            while (source_index + 1U < span->length &&
                   (span->text[source_index] != '*' || span->text[source_index + 1U] != '/')) {
                destination[destination_index] = span->text[source_index];
                ++destination_index;
                ++source_index;
            }
            if (source_index + 1U < span->length) {
                destination[destination_index] = span->text[source_index];
                destination[destination_index + 1U] = span->text[source_index + 1U];
                destination_index += 2U;
                source_index += 2U;
                if (source_index < span->length &&
                    aggregate_label_needs_space_after_block_comment(span->text[source_index])) {
                    destination[destination_index] = ' ';
                    ++destination_index;
                }
            }
        } else {
            destination[destination_index] = span->text[source_index];
            ++destination_index;
            ++source_index;
        }
    }

    destination[destination_index] = '\0';
}

static bool aggregate_label_needs_space_after_block_comment(char next_byte) {
    switch (next_byte) {
    case ' ':
    case '\f':
    case '\n':
    case '\r':
    case '\t':
    case '\v':
    case ')':
    case ',':
        return false;
    default:
        return true;
    }
}

static int read_count_from_source(
    struct mylite_db *database,
    const struct planned_count *plan,
    int64_t *out_count
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int rc = build_count_sql(plan, &sql);

    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_count_parameters(statement, plan);
    }
    if (rc == MYLITE_OK) {
        rc = step_count_statement(statement, out_count);
    }

    rc = finalize_sqlite_statement(statement, rc);
    free(sql);

    return rc;
}

static int step_count_statement(sqlite3_stmt *statement, int64_t *out_count) {
    int sqlite_rc = sqlite3_step(statement);

    if (sqlite_rc != SQLITE_ROW) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    *out_count = sqlite3_column_int64(statement, 0);
    sqlite_rc = sqlite3_step(statement);
    if (sqlite_rc != SQLITE_DONE) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    return MYLITE_OK;
}

static int format_count_value(
    struct mylite_db *database,
    int64_t count_value,
    char *buffer,
    size_t buffer_size
) {
    int written = snprintf(buffer, buffer_size, "%" PRId64, count_value);

    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format COUNT(*) value");
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int append_count_value_row(
    struct mylite_db *database,
    mylite_result *result,
    const char *count_text
) {
    const char *values[] = {count_text};
    int rc = mylite_result_append_text_row(result, values);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }

    return rc;
}

static int count_execution_error(struct mylite_db *database, int rc) {
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

static bool select_statement_has_column_aggregate(const struct mylite_sql_ast_node *statement) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *select_item = NULL;

    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        return false;
    }

    select_item = child_at(select_list, 0U);
    while (select_item != NULL) {
        const struct mylite_sql_ast_node *expression = child_at(select_item, 0U);

        expression = unwrap_parenthesized_expression(expression);
        if (column_aggregate_function_from_expression(expression) !=
            PLANNED_COLUMN_AGGREGATE_NONE) {
            return true;
        }
        select_item = select_item->next_sibling;
    }

    return false;
}

static int plan_column_aggregate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_column_aggregate *out_plan
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *select_item = child_at(select_list, 0U);
    const struct mylite_sql_ast_node *from_clause = child_at(statement, 1U);
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *optional_clause = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    struct mylite_catalog_column_descriptor *table_columns = NULL;
    struct select_source_context source_context = {0};
    size_t table_column_count = 0U;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_column_aggregate){0};

    expression = child_at(select_item, 0U);
    out_plan->expression = expression;
    out_plan->alias = child_at(select_item, 1U);
    expression = unwrap_parenthesized_expression(expression);
    out_plan->function = column_aggregate_function_from_expression(expression);
    if (mylite_sql_ast_node_child_count(select_list) != 1U) {
        if (out_plan->function == PLANNED_COLUMN_AGGREGATE_NONE) {
            out_plan->function = select_list_column_aggregate_function(select_list);
        }
        set_unsupported_error(database, column_aggregate_single_item_error(out_plan->function));
        return MYLITE_ERROR;
    }
    if (out_plan->function == PLANNED_COLUMN_AGGREGATE_NONE) {
        set_unsupported_error(database, column_aggregate_single_item_error(out_plan->function));
        return MYLITE_ERROR;
    }

    optional_clause = child_at(statement, 2U);
    while (optional_clause != NULL) {
        if (optional_clause->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
            where_clause = optional_clause;
        } else {
            set_unsupported_error(
                database,
                column_aggregate_optional_clause_error(out_plan->function)
            );
            return MYLITE_ERROR;
        }
        optional_clause = optional_clause->next_sibling;
    }

    if (from_clause == NULL || from_clause->kind == MYLITE_SQL_AST_FROM_DUAL) {
        set_unsupported_error(database, column_aggregate_source_error(out_plan->function));
        return MYLITE_ERROR;
    }
    if (from_clause->kind != MYLITE_SQL_AST_FROM_TABLE) {
        set_unsupported_error(database, column_aggregate_source_error(out_plan->function));
        return MYLITE_ERROR;
    }

    rc = resolve_visible_table_reference(
        database,
        child_at(from_clause, 0U),
        &out_plan->source,
        &out_plan->table
    );
    if (rc == MYLITE_OK) {
        rc = validate_select_index_hints(database, from_clause, &out_plan->table);
    }
    if (rc == MYLITE_OK) {
        rc = init_select_source_context(database, from_clause, &out_plan->source, &source_context);
    }
    if (rc == MYLITE_OK) {
        rc = load_table_columns(
            database,
            out_plan->table.table_id,
            &table_columns,
            &table_column_count
        );
    }
    if (rc == MYLITE_OK) {
        if (out_plan->function == PLANNED_COLUMN_AGGREGATE_GROUP_CONCAT) {
            rc = plan_column_group_concat_options(
                database,
                expression,
                &source_context,
                table_columns,
                table_column_count,
                out_plan
            );
        } else {
            rc = plan_column_aggregate_column(
                database,
                expression,
                out_plan->function,
                &source_context,
                table_columns,
                table_column_count,
                &out_plan->aggregate_column
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = plan_select_predicate(
            database,
            where_clause,
            &source_context,
            table_columns,
            table_column_count,
            &out_plan->predicate
        );
    }

    free(table_columns);
    return rc;
}

static void planned_column_aggregate_deinit(struct planned_column_aggregate *plan) {
    if (plan == NULL) {
        return;
    }

    planned_select_predicate_deinit(&plan->predicate);
    planned_select_order_deinit(&plan->aggregate_order);
    planned_value_deinit(&plan->separator);
    *plan = (struct planned_column_aggregate){0};
}

static enum planned_column_aggregate_function column_aggregate_function_from_expression(
    const struct mylite_sql_ast_node *expression
) {
    if (expression == NULL) {
        return PLANNED_COLUMN_AGGREGATE_NONE;
    }
    if (expression->kind == MYLITE_SQL_AST_MIN_AGGREGATE_FUNCTION) {
        return PLANNED_COLUMN_AGGREGATE_MIN;
    }
    if (expression->kind == MYLITE_SQL_AST_MAX_AGGREGATE_FUNCTION) {
        return PLANNED_COLUMN_AGGREGATE_MAX;
    }
    if (expression->kind == MYLITE_SQL_AST_SUM_AGGREGATE_FUNCTION) {
        return PLANNED_COLUMN_AGGREGATE_SUM;
    }
    if (expression->kind == MYLITE_SQL_AST_AVG_AGGREGATE_FUNCTION) {
        return PLANNED_COLUMN_AGGREGATE_AVG;
    }
    if (expression->kind == MYLITE_SQL_AST_BIT_AND_AGGREGATE_FUNCTION) {
        return PLANNED_COLUMN_AGGREGATE_BIT_AND;
    }
    if (expression->kind == MYLITE_SQL_AST_BIT_OR_AGGREGATE_FUNCTION) {
        return PLANNED_COLUMN_AGGREGATE_BIT_OR;
    }
    if (expression->kind == MYLITE_SQL_AST_BIT_XOR_AGGREGATE_FUNCTION) {
        return PLANNED_COLUMN_AGGREGATE_BIT_XOR;
    }
    if (expression->kind == MYLITE_SQL_AST_GROUP_CONCAT_AGGREGATE_FUNCTION) {
        return PLANNED_COLUMN_AGGREGATE_GROUP_CONCAT;
    }

    return PLANNED_COLUMN_AGGREGATE_NONE;
}

static enum planned_column_aggregate_function select_list_column_aggregate_function(
    const struct mylite_sql_ast_node *select_list
) {
    const struct mylite_sql_ast_node *select_item = child_at(select_list, 0U);

    while (select_item != NULL) {
        const struct mylite_sql_ast_node *expression = child_at(select_item, 0U);
        enum planned_column_aggregate_function function =
            column_aggregate_function_from_expression(unwrap_parenthesized_expression(expression));

        if (function != PLANNED_COLUMN_AGGREGATE_NONE) {
            return function;
        }
        select_item = select_item->next_sibling;
    }

    return PLANNED_COLUMN_AGGREGATE_NONE;
}

static bool column_aggregate_function_is_bitwise(enum planned_column_aggregate_function function) {
    return (function == PLANNED_COLUMN_AGGREGATE_BIT_AND ||
            function == PLANNED_COLUMN_AGGREGATE_BIT_OR ||
            function == PLANNED_COLUMN_AGGREGATE_BIT_XOR) != 0;
}

static const char *column_aggregate_single_item_error(
    enum planned_column_aggregate_function function
) {
    if (column_aggregate_function_is_bitwise(function)) {
        return "BIT_AND/BIT_OR/BIT_XOR(column) supports exactly one aggregate select item";
    }
    if (function == PLANNED_COLUMN_AGGREGATE_GROUP_CONCAT) {
        return "GROUP_CONCAT(column) supports exactly one aggregate select item";
    }
    if (function == PLANNED_COLUMN_AGGREGATE_AVG) {
        return "AVG(column) supports exactly one aggregate select item";
    }
    if (function == PLANNED_COLUMN_AGGREGATE_SUM) {
        return "SUM(column) supports exactly one aggregate select item";
    }

    return "MIN/MAX supports exactly one aggregate select item";
}

static const char *column_aggregate_optional_clause_error(
    enum planned_column_aggregate_function function
) {
    if (column_aggregate_function_is_bitwise(function)) {
        return "BIT_AND/BIT_OR/BIT_XOR(column) supports only WHERE";
    }
    if (function == PLANNED_COLUMN_AGGREGATE_GROUP_CONCAT) {
        return "GROUP_CONCAT(column) supports only WHERE";
    }
    if (function == PLANNED_COLUMN_AGGREGATE_AVG) {
        return "AVG(column) supports only WHERE";
    }
    if (function == PLANNED_COLUMN_AGGREGATE_SUM) {
        return "SUM(column) supports only WHERE";
    }

    return "MIN/MAX supports only WHERE";
}

static const char *column_aggregate_source_error(enum planned_column_aggregate_function function) {
    if (column_aggregate_function_is_bitwise(function)) {
        return "BIT_AND/BIT_OR/BIT_XOR(column) supports only descriptor-backed table reads";
    }
    if (function == PLANNED_COLUMN_AGGREGATE_GROUP_CONCAT) {
        return "GROUP_CONCAT(column) supports only descriptor-backed table reads";
    }
    if (function == PLANNED_COLUMN_AGGREGATE_AVG) {
        return "AVG(column) supports only descriptor-backed table reads";
    }
    if (function == PLANNED_COLUMN_AGGREGATE_SUM) {
        return "SUM(column) supports only descriptor-backed table reads";
    }

    return "MIN/MAX supports only descriptor-backed table reads";
}

static const char *column_aggregate_column_error(enum planned_column_aggregate_function function) {
    if (column_aggregate_function_is_bitwise(function)) {
        return "BIT_AND/BIT_OR/BIT_XOR(column) supports only descriptor columns";
    }
    if (function == PLANNED_COLUMN_AGGREGATE_GROUP_CONCAT) {
        return "GROUP_CONCAT(column) supports only descriptor columns";
    }
    if (function == PLANNED_COLUMN_AGGREGATE_AVG) {
        return "AVG(column) supports only descriptor columns";
    }
    if (function == PLANNED_COLUMN_AGGREGATE_SUM) {
        return "SUM(column) supports only descriptor columns";
    }

    return "MIN/MAX supports only descriptor columns";
}

static const char *column_aggregate_integer_error(enum planned_column_aggregate_function function) {
    if (column_aggregate_function_is_bitwise(function)) {
        return "BIT_AND/BIT_OR/BIT_XOR(column) supports only integer descriptor columns";
    }
    if (function == PLANNED_COLUMN_AGGREGATE_GROUP_CONCAT) {
        return "GROUP_CONCAT(column) supports only integer and nonbinary string descriptor columns";
    }
    if (function == PLANNED_COLUMN_AGGREGATE_AVG) {
        return "AVG(column) supports only integer descriptor columns";
    }
    if (function == PLANNED_COLUMN_AGGREGATE_SUM) {
        return "SUM(column) supports only integer descriptor columns";
    }

    return "MIN/MAX supports only integer descriptor columns";
}

static int plan_column_aggregate_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    enum planned_column_aggregate_function aggregate_function,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct mylite_catalog_column_descriptor *out_column
) {
    const struct mylite_sql_ast_node *column_node = child_at(function, 0U);
    struct integer_column_range range;
    int rc = MYLITE_OK;

    rc = resolve_descriptor_column_reference(
        database,
        column_node,
        source_context,
        COLUMN_REFERENCE_FIELD,
        column_aggregate_column_error(aggregate_function),
        table_columns,
        table_column_count,
        out_column
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = integer_range_for_column(
        database,
        out_column,
        column_aggregate_integer_error(aggregate_function),
        &range
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    return MYLITE_OK;
}

static int plan_column_group_concat_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_column_aggregate *out_plan
) {
    size_t value_source_index = 0U;
    int rc = plan_group_concat_value_column(
        database,
        function,
        source_context,
        table_columns,
        table_column_count,
        &out_plan->aggregate_column,
        &value_source_index
    );

    (void)value_source_index;
    if (rc == MYLITE_OK) {
        rc = plan_group_concat_order_column(
            database,
            function,
            source_context,
            table_columns,
            table_column_count,
            false,
            &out_plan->aggregate_order
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_group_concat_separator(
            database,
            function,
            &out_plan->has_separator,
            &out_plan->separator
        );
    }

    return rc;
}

static int execute_column_aggregate_from_plan(
    struct mylite_db *database,
    const struct planned_column_aggregate *plan,
    bool apply_sql_select_limit,
    mylite_result **out_result
) {
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = append_column_aggregate_result_column(database, result, plan);
    if (rc == MYLITE_OK) {
        rc = read_column_aggregate_from_source(database, plan, result);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return column_aggregate_execution_error(database, rc);
    }
    if (apply_sql_select_limit) {
        apply_sql_select_limit_to_result(database, result);
    }

    return finish_successful_result(database, result, out_result);
}

static int append_column_aggregate_result_column(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_column_aggregate *plan
) {
    char *column_name = NULL;
    int rc = MYLITE_OK;

    if (plan->alias != NULL) {
        rc = copy_select_item_alias_text(database, plan->alias, &column_name);
    } else {
        rc = copy_aggregate_result_column_name(database, &plan->expression->span, &column_name);
    }

    if (rc == MYLITE_OK) {
        rc = mylite_result_append_column(result, column_name);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }

    free(column_name);
    return rc;
}

static int read_column_aggregate_from_source(
    struct mylite_db *database,
    const struct planned_column_aggregate *plan,
    mylite_result *result
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int rc = build_column_aggregate_sql(plan, &sql);

    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_column_aggregate_parameters(statement, plan);
    }
    if (rc == MYLITE_OK && plan->function == PLANNED_COLUMN_AGGREGATE_GROUP_CONCAT) {
        database->session.group_concat_value_ordinal = 0U;
    }
    if (rc == MYLITE_OK) {
        rc = step_column_aggregate_statement(database, statement, plan, result);
    }

    rc = finalize_sqlite_statement(statement, rc);
    free(sql);

    return rc;
}

static int read_grouped_aggregate_from_source(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *plan,
    mylite_result *result
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = build_grouped_aggregate_sql(plan, &sql);

    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_grouped_aggregate_parameters(statement, plan);
    }
    if (rc == MYLITE_OK) {
        for (size_t index = 0U; index < plan->aggregate_count; ++index) {
            if (plan->aggregates[index].function == PLANNED_GROUPED_AGGREGATE_GROUP_CONCAT) {
                database->session.group_concat_value_ordinal = 0U;
                break;
            }
        }
    }
    while (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            break;
        }
        if (sqlite_rc != SQLITE_ROW) {
            rc = grouped_aggregate_step_error(database, statement, plan, sqlite_rc);
            break;
        }
        rc = append_grouped_aggregate_sqlite_row(database, statement, plan, result);
    }

    rc = finalize_sqlite_statement(statement, rc);
    free(sql);

    return rc;
}

static int step_column_aggregate_statement(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_column_aggregate *plan,
    mylite_result *result
) {
    int sqlite_rc = sqlite3_step(statement);
    int rc = MYLITE_OK;

    if (sqlite_rc != SQLITE_ROW) {
        return column_aggregate_step_error(database, statement, plan, sqlite_rc);
    }

    if (plan->function == PLANNED_COLUMN_AGGREGATE_AVG) {
        rc = append_avg_sqlite_row(database, statement, result);
    } else if (column_aggregate_function_is_bitwise(plan->function)) {
        rc = append_bitwise_aggregate_sqlite_row(database, statement, result);
    } else {
        rc = append_selected_sqlite_row(database, statement, result, NULL, 0U);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    sqlite_rc = sqlite3_step(statement);
    if (sqlite_rc != SQLITE_DONE) {
        return column_aggregate_step_error(database, statement, plan, sqlite_rc);
    }

    return MYLITE_OK;
}

static int append_grouped_aggregate_sqlite_row(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_grouped_aggregate *plan,
    mylite_result *result
) {
    struct mylite_result_cell *cells = NULL;
    char (*projection_buffers)[approximate_numeric_text_capacity] = NULL;
    char (*aggregate_buffers)[approximate_numeric_text_capacity] = NULL;
    int sqlite_column_index = 0;
    size_t result_column_count = plan->projection_count + plan->aggregate_count;
    int rc = MYLITE_OK;

    if (plan->projection_count > (size_t)INT_MAX) {
        return MYLITE_ERROR;
    }
    if (result_column_count == 0U) {
        return MYLITE_ERROR;
    }
    if (result_column_count > SIZE_MAX / sizeof(*cells)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    sqlite_column_index = (int)plan->projection_count;

    cells = calloc(result_column_count, sizeof(*cells));
    if (plan->projection_count > 0U) {
        projection_buffers = calloc(plan->projection_count, sizeof(*projection_buffers));
    }
    if (plan->aggregate_count > 0U) {
        aggregate_buffers = calloc(plan->aggregate_count, sizeof(*aggregate_buffers));
    }
    if (cells == NULL || (plan->projection_count > 0U && projection_buffers == NULL) ||
        (plan->aggregate_count > 0U && aggregate_buffers == NULL)) {
        free(cells);
        free(projection_buffers);
        free(aggregate_buffers);
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    for (size_t projection_index = 0U; rc == MYLITE_OK && projection_index < plan->projection_count;
         ++projection_index) {
        rc = append_grouped_aggregate_projection_cell(
            database,
            statement,
            plan,
            projection_index,
            &cells[projection_index],
            projection_buffers[projection_index],
            sizeof(projection_buffers[projection_index])
        );
    }
    for (size_t aggregate_index = 0U; rc == MYLITE_OK && aggregate_index < plan->aggregate_count;
         ++aggregate_index) {
        rc = append_grouped_aggregate_sqlite_cell(
            database,
            statement,
            &plan->aggregates[aggregate_index],
            &sqlite_column_index,
            &cells[plan->projection_count + aggregate_index],
            aggregate_buffers[aggregate_index],
            sizeof(aggregate_buffers[aggregate_index])
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_bytes_row(result, cells);
    }
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }

    free(cells);
    free(projection_buffers);
    free(aggregate_buffers);

    return rc;
}

static int append_grouped_aggregate_projection_cell(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_grouped_aggregate *plan,
    size_t projection_index,
    struct mylite_result_cell *out_cell,
    char *buffer,
    size_t buffer_size
) {
    if (projection_index >= plan->projection_count) {
        return MYLITE_ERROR;
    }
    return append_selected_sqlite_row_value_with_descriptor(
        database,
        statement,
        projection_index,
        &plan->projections[projection_index].column,
        buffer,
        buffer_size,
        out_cell
    );
}

static int append_grouped_aggregate_sqlite_cell(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_grouped_aggregate_item *item,
    int *sqlite_column_index,
    struct mylite_result_cell *out_cell,
    char *buffer,
    size_t buffer_size
) {
    const char *value = NULL;
    int64_t count = 0;
    int rc = MYLITE_OK;

    *out_cell = (struct mylite_result_cell){.is_null = true};
    if (item->function == PLANNED_GROUPED_AGGREGATE_GROUP_CONCAT) {
        rc = append_grouped_group_concat_sqlite_cell(statement, *sqlite_column_index, out_cell);
        ++(*sqlite_column_index);
        return rc;
    }
    if (item->function == PLANNED_GROUPED_AGGREGATE_ANY_VALUE) {
        rc = append_selected_sqlite_row_value_with_descriptor(
            database,
            statement,
            (size_t)*sqlite_column_index,
            &item->aggregate_column,
            buffer,
            buffer_size,
            out_cell
        );
        ++(*sqlite_column_index);
        return rc;
    }
    if (item->function == PLANNED_GROUPED_AGGREGATE_AVG) {
        if (sqlite3_column_type(statement, *sqlite_column_index + 1) != SQLITE_INTEGER) {
            return MYLITE_ERROR;
        }
        count = (int64_t)sqlite3_column_int64(statement, *sqlite_column_index + 1);
        if (count < 0) {
            return MYLITE_ERROR;
        }
        if (count == 0) {
            *sqlite_column_index += 2;
            return MYLITE_OK;
        }
        if (sqlite3_column_type(statement, *sqlite_column_index) != SQLITE_INTEGER) {
            return MYLITE_ERROR;
        }
        rc = format_avg_value(
            database,
            (struct avg_accumulator){
                .sum = (int64_t)sqlite3_column_int64(statement, *sqlite_column_index),
                .count = count,
            },
            buffer,
            buffer_size
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        *out_cell = (struct mylite_result_cell){
            .bytes = buffer,
            .size = strlen(buffer),
            .is_null = false,
        };
        *sqlite_column_index += 2;
        return MYLITE_OK;
    }
    if (grouped_aggregate_function_is_bitwise(item->function)) {
        if (sqlite3_column_type(statement, *sqlite_column_index) != SQLITE_TEXT) {
            return MYLITE_ERROR;
        }
        value = (const char *)sqlite3_column_text(statement, *sqlite_column_index);
        if (value == NULL) {
            return MYLITE_ERROR;
        }
        *out_cell = (struct mylite_result_cell){
            .bytes = value,
            .size = strlen(value),
            .is_null = false,
        };
        ++(*sqlite_column_index);
        return MYLITE_OK;
    }

    rc = sqlite_integer_result_text(statement, *sqlite_column_index, buffer, buffer_size, &value);
    if (rc == MYLITE_OK && value != NULL) {
        *out_cell = (struct mylite_result_cell){
            .bytes = value,
            .size = strlen(value),
            .is_null = false,
        };
    }
    ++(*sqlite_column_index);
    return rc;
}

static int append_grouped_group_concat_sqlite_cell(
    sqlite3_stmt *statement,
    int sqlite_column_index,
    struct mylite_result_cell *out_cell
) {
    int byte_count = sqlite3_column_bytes(statement, sqlite_column_index);
    const unsigned char *text = NULL;
    size_t character_count = 0U;

    *out_cell = (struct mylite_result_cell){.is_null = true};
    if (sqlite3_column_type(statement, sqlite_column_index) == SQLITE_NULL) {
        return MYLITE_OK;
    }
    if (sqlite3_column_type(statement, sqlite_column_index) != SQLITE_TEXT || byte_count < 0) {
        return MYLITE_ERROR;
    }
    text = sqlite3_column_text(statement, sqlite_column_index);
    if (text == NULL || memchr(text, '\0', (size_t)byte_count) != NULL ||
        validate_utf8_text((const char *)text, (size_t)byte_count, &character_count) != MYLITE_OK) {
        return MYLITE_ERROR;
    }
    *out_cell = (struct mylite_result_cell){
        .bytes = text,
        .size = (size_t)byte_count,
        .is_null = false,
    };
    (void)character_count;
    return MYLITE_OK;
}

static int sqlite_integer_result_text(
    sqlite3_stmt *statement,
    int column_index,
    char *buffer,
    size_t buffer_size,
    const char **out_value
) {
    int sqlite_type = sqlite3_column_type(statement, column_index);
    int written = 0;

    *out_value = NULL;
    if (sqlite_type == SQLITE_NULL) {
        return MYLITE_OK;
    }
    if (sqlite_type != SQLITE_INTEGER) {
        return MYLITE_ERROR;
    }

    written = snprintf(
        buffer,
        buffer_size,
        "%" PRId64,
        (int64_t)sqlite3_column_int64(statement, column_index)
    );
    if (written < 0 || (size_t)written >= buffer_size) {
        return MYLITE_ERROR;
    }

    *out_value = buffer;
    return MYLITE_OK;
}

static int append_bitwise_aggregate_sqlite_row(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    mylite_result *result
) {
    const char *values[] = {NULL};
    int rc = MYLITE_OK;

    if (sqlite3_column_type(statement, 0) != SQLITE_TEXT) {
        return MYLITE_ERROR;
    }

    values[0] = (const char *)sqlite3_column_text(statement, 0);
    if (values[0] == NULL) {
        return MYLITE_ERROR;
    }
    rc = mylite_result_append_text_row(result, values);
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }

    return rc;
}

static int append_avg_sqlite_row(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    mylite_result *result
) {
    const char *values[] = {NULL};
    char average_text[integer_text_capacity + sizeof(".0000")];
    int64_t count = 0;
    int rc = MYLITE_OK;

    if (sqlite3_column_type(statement, 1) != SQLITE_INTEGER) {
        return MYLITE_ERROR;
    }

    count = (int64_t)sqlite3_column_int64(statement, 1);
    if (count == 0) {
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
        return rc;
    }
    if (count < 0 || sqlite3_column_type(statement, 0) != SQLITE_INTEGER) {
        return MYLITE_ERROR;
    }

    rc = format_avg_value(
        database,
        (struct avg_accumulator){
            .sum = (int64_t)sqlite3_column_int64(statement, 0),
            .count = count,
        },
        average_text,
        sizeof(average_text)
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    values[0] = average_text;
    rc = mylite_result_append_text_row(result, values);
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }

    return rc;
}

static int format_avg_value(
    struct mylite_db *database,
    struct avg_accumulator accumulator,
    char *buffer,
    size_t buffer_size
) {
    uint64_t magnitude = absolute_int64_magnitude(accumulator.sum);
    uint64_t denominator = (uint64_t)accumulator.count;
    uint64_t integer_part = magnitude / denominator;
    uint64_t remainder = magnitude % denominator;
    unsigned int fraction = 0U;
    int round_digit = 0;
    bool is_negative = accumulator.sum < 0;
    int written = 0;

    for (size_t digit_index = 0U; digit_index < avg_fraction_digits; ++digit_index) {
        int digit = next_decimal_digit(&remainder, denominator);

        if (digit < 0) {
            set_runtime_error(database, "failed to format AVG(column) value");
            return MYLITE_ERROR;
        }
        fraction = (fraction * decimal_base) + (unsigned int)digit;
    }

    round_digit = next_decimal_digit(&remainder, denominator);
    if (round_digit < 0) {
        set_runtime_error(database, "failed to format AVG(column) value");
        return MYLITE_ERROR;
    }
    if (round_digit >= avg_round_half_digit) {
        ++fraction;
        if (fraction == avg_fraction_scale) {
            fraction = 0U;
            ++integer_part;
        }
    }

    written = snprintf(
        buffer,
        buffer_size,
        "%s%" PRIu64 ".%04u",
        is_negative && (integer_part != 0U || fraction != 0U) ? "-" : "",
        integer_part,
        fraction
    );
    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format AVG(column) value");
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static uint64_t absolute_int64_magnitude(int64_t value) {
    const uint64_t int64_negative_abs_max = 9223372036854775808ULL;

    if (value >= 0) {
        return (uint64_t)value;
    }
    if (value == INT64_MIN) {
        return int64_negative_abs_max;
    }

    return (uint64_t)-value;
}

static int next_decimal_digit(uint64_t *remainder, uint64_t denominator) {
    struct uint128_parts product = {0};
    int digit = 0;

    if (remainder == NULL || denominator == 0U || *remainder >= denominator) {
        return -1;
    }

    product = multiply_u64_by_decimal_radix(*remainder);
    while (uint128_ge_u64(&product, denominator)) {
        uint128_subtract_u64(&product, denominator);
        ++digit;
    }

    *remainder = product.low;
    return digit;
}

static struct uint128_parts multiply_u64_by_decimal_radix(uint64_t value) {
    struct uint128_parts product = {0};

    for (unsigned int index = 0U; index < decimal_base; ++index) {
        uint64_t previous_low = product.low;

        product.low += value;
        if (product.low < previous_low) {
            ++product.high;
        }
    }

    return product;
}

static bool uint128_ge_u64(const struct uint128_parts *left, uint64_t right) {
    if (left->high != 0U) {
        return true;
    }

    return left->low >= right;
}

static void uint128_subtract_u64(struct uint128_parts *left, uint64_t right) {
    uint64_t previous_low = left->low;

    left->low -= right;
    if (previous_low < right) {
        --left->high;
    }
}

static int column_aggregate_step_error(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_column_aggregate *plan,
    int sqlite_rc
) {
    const char *message = NULL;

    if (sqlite_rc == SQLITE_ERROR && (plan->function == PLANNED_COLUMN_AGGREGATE_SUM ||
                                      plan->function == PLANNED_COLUMN_AGGREGATE_AVG)) {
        message = sqlite3_errmsg(sqlite3_db_handle(statement));
        if (message != NULL && strcmp(message, "integer overflow") == 0) {
            if (plan->function == PLANNED_COLUMN_AGGREGATE_AVG) {
                set_unsupported_error(
                    database,
                    "AVG(column) intermediate sum exceeds MyLite signed 64-bit range"
                );
            } else {
                set_unsupported_error(
                    database,
                    "SUM(column) result exceeds MyLite signed 64-bit range"
                );
            }
            return MYLITE_ERROR;
        }
    }

    return mylite_sqlite_status_to_mylite(sqlite_rc);
}

static int grouped_aggregate_step_error(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_grouped_aggregate *plan,
    int sqlite_rc
) {
    const char *message = NULL;
    bool has_sum = false;
    bool has_avg = false;

    for (size_t aggregate_index = 0U; aggregate_index < plan->aggregate_count; ++aggregate_index) {
        if (plan->aggregates[aggregate_index].function == PLANNED_GROUPED_AGGREGATE_SUM) {
            has_sum = true;
        }
        if (plan->aggregates[aggregate_index].function == PLANNED_GROUPED_AGGREGATE_AVG) {
            has_avg = true;
        }
    }
    if (sqlite_rc == SQLITE_ERROR && (has_sum || has_avg)) {
        message = sqlite3_errmsg(sqlite3_db_handle(statement));
        if (message != NULL && strcmp(message, "integer overflow") == 0) {
            if (has_avg) {
                set_unsupported_error(
                    database,
                    "AVG(column) intermediate sum exceeds MyLite signed 64-bit range"
                );
            } else {
                set_unsupported_error(
                    database,
                    "SUM(column) result exceeds MyLite signed 64-bit range"
                );
            }
            return MYLITE_ERROR;
        }
    }

    return mylite_sqlite_status_to_mylite(sqlite_rc);
}

static int column_aggregate_execution_error(struct mylite_db *database, int rc) {
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

static int grouped_aggregate_execution_error(struct mylite_db *database, int rc) {
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

static bool select_statement_is_scalar_projection(const struct mylite_sql_ast_node *statement) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *from_clause = child_at(statement, 1U);
    const struct mylite_sql_ast_node *select_item = NULL;

    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        return false;
    }
    if (from_clause != NULL && from_clause->kind != MYLITE_SQL_AST_FROM_DUAL) {
        return false;
    }
    if (child_at(statement, 2U) != NULL) {
        return false;
    }

    select_item = child_at(select_list, 0U);
    if (select_item == NULL) {
        return false;
    }
    while (select_item != NULL) {
        if (select_item->kind != MYLITE_SQL_AST_SELECT_ITEM ||
            !is_scalar_projection_select_item_expression(child_at(select_item, 0U))) {
            return false;
        }
        select_item = select_item->next_sibling;
    }

    return true;
}

static bool is_scalar_projection_select_item_expression(
    const struct mylite_sql_ast_node *expression
) {
    expression = unwrap_any_value_function_expression(expression);

    if (expression == NULL) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(expression) == MYLITE_SQL_AST_LITERAL_STRING) {
        return true;
    }
    return is_scalar_projection_expression(expression);
}

static bool select_statement_is_row_function_scalar_projection(
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *select_item = NULL;
    bool found_row_function = false;

    if (!select_statement_is_scalar_projection(statement)) {
        return false;
    }

    select_item = child_at(select_list, 0U);
    while (select_item != NULL) {
        const struct mylite_sql_ast_node *expression = child_at(select_item, 0U);

        if (is_string_metadata_projection_expression(expression) ||
            is_hex_projection_expression(expression) ||
            is_base64_projection_expression(expression) ||
            is_unhex_projection_expression(expression) ||
            is_uuid_projection_expression(expression) ||
            is_char_projection_expression(expression) ||
            is_json_introspection_projection_expression(expression) ||
            is_json_extract_projection_expression(expression) ||
            is_json_value_projection_expression(expression) ||
            is_json_contains_projection_expression(expression) ||
            is_json_unquote_projection_expression(expression) ||
            is_json_set_projection_expression(expression) ||
            is_json_construction_projection_expression(expression)) {
            found_row_function = true;
        } else if (row_scalar_expression_contains_row_function(expression)) {
            return false;
        }
        select_item = select_item->next_sibling;
    }

    return found_row_function;
}

static bool select_statement_has_no_source_or_dual(const struct mylite_sql_ast_node *statement) {
    const struct mylite_sql_ast_node *from_clause = child_at(statement, 1U);

    if (from_clause == NULL) {
        return true;
    }
    if (from_clause->kind == MYLITE_SQL_AST_FROM_DUAL) {
        return true;
    }
    return false;
}

static int reject_bare_native_function_identifier_lookup_if_needed(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *select_item = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = MYLITE_OK;

    if (!select_statement_has_no_source_or_dual(statement) || child_at(statement, 2U) != NULL) {
        return MYLITE_OK;
    }
    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST ||
        mylite_sql_ast_node_child_count(select_list) != 1U) {
        return MYLITE_OK;
    }

    select_item = child_at(select_list, 0U);
    if (select_item == NULL || select_item->kind != MYLITE_SQL_AST_SELECT_ITEM) {
        return MYLITE_OK;
    }
    expression = unwrap_parenthesized_expression(child_at(select_item, 0U));
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_IDENTIFIER) {
        return MYLITE_OK;
    }

    rc = copy_identifier_text(expression, column_name, sizeof(column_name), database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!text_equals_ascii_case_insensitive(column_name, "pi") &&
        !text_equals_ascii_case_insensitive(column_name, "rand") &&
        !text_equals_ascii_case_insensitive(column_name, "sqrt") &&
        !text_equals_ascii_case_insensitive(column_name, "degrees") &&
        !text_equals_ascii_case_insensitive(column_name, "radians") &&
        !text_equals_ascii_case_insensitive(column_name, "acos") &&
        !text_equals_ascii_case_insensitive(column_name, "asin") &&
        !text_equals_ascii_case_insensitive(column_name, "sin") &&
        !text_equals_ascii_case_insensitive(column_name, "cos") &&
        !text_equals_ascii_case_insensitive(column_name, "tan") &&
        !text_equals_ascii_case_insensitive(column_name, "cot") &&
        !text_equals_ascii_case_insensitive(column_name, "atan") &&
        !text_equals_ascii_case_insensitive(column_name, "atan2") &&
        !text_equals_ascii_case_insensitive(column_name, "exp") &&
        !text_equals_ascii_case_insensitive(column_name, "ln") &&
        !text_equals_ascii_case_insensitive(column_name, "log") &&
        !text_equals_ascii_case_insensitive(column_name, "log10") &&
        !text_equals_ascii_case_insensitive(column_name, "log2") &&
        !text_equals_ascii_case_insensitive(column_name, "pow") &&
        !text_equals_ascii_case_insensitive(column_name, "power") &&
        !text_equals_ascii_case_insensitive(column_name, "uuid")) {
        return MYLITE_OK;
    }

    set_unknown_column_error(database, column_name);
    return MYLITE_ERROR;
}

static bool select_statement_is_scalar_projection_attempt(
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *select_item = NULL;
    bool saw_scalar_projection_expression = false;

    if (!select_statement_has_no_source_or_dual(statement)) {
        return false;
    }
    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST ||
        child_at(statement, 2U) != NULL) {
        return false;
    }

    select_item = child_at(select_list, 0U);
    if (select_item == NULL) {
        return false;
    }
    while (select_item != NULL) {
        if (select_item->kind != MYLITE_SQL_AST_SELECT_ITEM ||
            !is_scalar_projection_attempt_expression(child_at(select_item, 0U))) {
            return false;
        }
        saw_scalar_projection_expression = true;
        select_item = select_item->next_sibling;
    }

    return saw_scalar_projection_expression;
}

static int execute_values_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    const struct mylite_sql_ast_node *rows = child_at(statement, 0U);
    const struct mylite_sql_ast_node *order_clause =
        child_with_kind(statement, MYLITE_SQL_AST_ORDER_BY_CLAUSE);
    const struct mylite_sql_ast_node *limit_clause =
        child_with_kind(statement, MYLITE_SQL_AST_LIMIT_CLAUSE);
    struct planned_select_limit limit = {
        .has_limit = false,
        .row_count = 0,
        .has_offset = false,
        .offset = 0,
    };
    mylite_result *result = NULL;
    size_t column_count = 0U;
    int rc = validate_values_statement_rows(database, rows, &column_count);

    if (rc == MYLITE_OK) {
        rc = validate_values_order_clause(database, order_clause, column_count);
    }
    if (rc == MYLITE_OK) {
        rc = plan_values_limit(database, limit_clause, &limit);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = mylite_result_create(&result);
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }
    rc = append_values_result_columns(database, result, column_count);
    if (rc == MYLITE_OK) {
        rc = append_values_result_rows(database, result, rows, &limit, column_count);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int validate_values_statement_rows(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *rows,
    size_t *out_column_count
) {
    const struct mylite_sql_ast_node *row = NULL;
    size_t expected_column_count = 0U;
    size_t row_number = 1U;
    int rc = MYLITE_OK;

    if (out_column_count == NULL) {
        return MYLITE_MISUSE;
    }
    *out_column_count = 0U;
    if (rows == NULL || rows->kind != MYLITE_SQL_AST_VALUES_ROW_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    row = child_at(rows, 0U);
    if (row == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    expected_column_count = mylite_sql_ast_node_child_count(row);
    if (expected_column_count == 0U) {
        set_values_empty_row_error(database);
        return MYLITE_ERROR;
    }

    while (row != NULL) {
        rc = validate_values_statement_row(
            database,
            row,
            (struct values_statement_row_shape){
                .column_count = expected_column_count,
                .row_number = row_number,
            }
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        row = row->next_sibling;
        ++row_number;
    }

    *out_column_count = expected_column_count;
    return MYLITE_OK;
}

static int validate_values_statement_row(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *row,
    struct values_statement_row_shape shape
) {
    const struct mylite_sql_ast_node *value = NULL;
    size_t column_count = 0U;

    if (row == NULL || row->kind != MYLITE_SQL_AST_VALUES_ROW) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    column_count = mylite_sql_ast_node_child_count(row);
    if (column_count == 0U) {
        set_values_empty_row_error(database);
        return MYLITE_ERROR;
    }
    if (column_count != shape.column_count) {
        set_column_count_mismatch_error(database, shape.row_number);
        return MYLITE_ERROR;
    }

    value = child_at(row, 0U);
    while (value != NULL) {
        int rc = validate_values_statement_value(database, value);

        if (rc != MYLITE_OK) {
            return rc;
        }
        value = value->next_sibling;
    }
    return MYLITE_OK;
}

static int validate_values_statement_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    if (value == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value->kind == MYLITE_SQL_AST_DML_DEFAULT_VALUE) {
        set_values_default_error(database);
        return MYLITE_ERROR;
    }
    if (value->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return validate_values_integer_literal(database, value);
    }
    if (value->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "VALUES supports only integer, string, NULL, TRUE, and FALSE literals"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(value);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER) {
        return validate_values_integer_literal(database, value);
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING ||
        literal_kind == MYLITE_SQL_AST_LITERAL_NULL ||
        literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ||
        literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        return MYLITE_OK;
    }

    set_unsupported_error(
        database,
        "VALUES supports only integer, string, NULL, TRUE, and FALSE literals"
    );
    return MYLITE_ERROR;
}

static int validate_values_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value
) {
    const uint64_t int64_positive_max = 9223372036854775807ULL;
    const uint64_t int64_negative_max_magnitude = 9223372036854775808ULL;
    const struct mylite_sql_ast_node *literal = value;
    uint64_t magnitude = 0U;
    bool is_negative = false;

    if (value == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(value);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_unsupported_error(database, "VALUES supports only signed integer literals");
            return MYLITE_ERROR;
        }
        literal = child_at(value, 0U);
    }
    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_unsupported_error(database, "VALUES supports only signed integer literals");
        return MYLITE_ERROR;
    }
    if (parse_unsigned_integer_literal(&literal->span, &magnitude) != MYLITE_OK) {
        set_values_integer_out_of_range_error(database);
        return MYLITE_ERROR;
    }
    if ((!is_negative && magnitude > int64_positive_max) ||
        (is_negative && magnitude > int64_negative_max_magnitude)) {
        set_values_integer_out_of_range_error(database);
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int validate_values_order_clause(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_clause,
    size_t column_count
) {
    const struct mylite_sql_ast_node *first_child = NULL;

    if (order_clause == NULL) {
        return MYLITE_OK;
    }
    if (order_clause->kind != MYLITE_SQL_AST_ORDER_BY_CLAUSE) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    first_child = child_at(order_clause, 0U);
    if (first_child == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (first_child->kind == MYLITE_SQL_AST_ORDER_BY_ITEM_LIST) {
        const struct mylite_sql_ast_node *item = child_at(first_child, 0U);

        while (item != NULL) {
            int rc = validate_values_order_item(database, item, column_count);

            if (rc != MYLITE_OK) {
                return rc;
            }
            item = item->next_sibling;
        }
        return MYLITE_OK;
    }

    return validate_values_order_key(database, first_child, column_count);
}

static int validate_values_order_item(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *item,
    size_t column_count
) {
    if (item == NULL || item->kind != MYLITE_SQL_AST_ORDER_BY_ITEM) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    return validate_values_order_key(database, child_at(item, 0U), column_count);
}

static int validate_values_order_key(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    size_t column_count
) {
    if (order_key == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (order_key->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(order_key) == MYLITE_SQL_AST_LITERAL_INTEGER) {
        return validate_values_order_ordinal(database, order_key, column_count);
    }
    if (order_key->kind == MYLITE_SQL_AST_IDENTIFIER ||
        order_key->kind == MYLITE_SQL_AST_QUALIFIED_IDENTIFIER) {
        return validate_values_order_identifier(database, order_key, column_count);
    }

    set_unsupported_error(database, "VALUES ORDER BY supports only column names and ordinals");
    return MYLITE_ERROR;
}

static int validate_values_order_ordinal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    size_t column_count
) {
    uint64_t ordinal = 0U;
    char *display_text = NULL;

    if (parse_unsigned_integer_literal(&order_key->span, &ordinal) == MYLITE_OK && ordinal >= 1U &&
        ordinal <= column_count) {
        return MYLITE_OK;
    }

    if (copy_source_span_text(database, &order_key->span, &display_text) != MYLITE_OK) {
        return MYLITE_ERROR;
    }
    set_unknown_order_column_error(database, display_text);
    free(display_text);
    return MYLITE_ERROR;
}

static int validate_values_order_identifier(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    size_t column_count
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char display_text[MYLITE_CATALOG_IDENTIFIER_CAPACITY * table_name_part_capacity];
    size_t part_count = 0U;
    size_t column_index = 0U;
    int rc = collect_column_reference_parts(database, order_key, parts, &part_count);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (part_count == 1U && values_column_name_to_index(parts[0], column_count, &column_index)) {
        return MYLITE_OK;
    }

    rc = format_column_reference_name(
        database,
        parts,
        part_count,
        display_text,
        sizeof(display_text)
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    set_unknown_order_column_error(database, display_text);
    return MYLITE_ERROR;
}

static bool values_column_name_to_index(
    const char *column_name,
    size_t column_count,
    size_t *out_index
) {
    enum { decimal_radix = 10U };

    const char prefix[] = "column_";
    const char *digits = NULL;
    uint64_t value = 0U;

    if (out_index != NULL) {
        *out_index = 0U;
    }
    if (column_name == NULL || !text_has_ascii_case_insensitive_prefix(column_name, prefix)) {
        return false;
    }

    digits = column_name + sizeof(prefix) - 1U;
    if (*digits == '\0' || (digits[0] == '0' && digits[1] != '\0')) {
        return false;
    }
    for (const char *cursor = digits; *cursor != '\0'; ++cursor) {
        unsigned char byte = (unsigned char)*cursor;

        if (byte < '0' || byte > '9') {
            return false;
        }
        if (value > (UINT64_MAX - (uint64_t)(byte - '0')) / decimal_radix) {
            return false;
        }
        value = (value * decimal_radix) + (uint64_t)(byte - '0');
    }
    if (value >= column_count) {
        return false;
    }

    if (out_index != NULL) {
        *out_index = (size_t)value;
    }
    return true;
}

static int plan_values_limit(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *limit_clause,
    struct planned_select_limit *out_limit
) {
    int rc = MYLITE_OK;

    if (out_limit == NULL) {
        return MYLITE_MISUSE;
    }
    *out_limit = (struct planned_select_limit){
        .has_limit = false,
        .row_count = 0,
        .has_offset = false,
        .offset = 0,
    };
    if (limit_clause == NULL) {
        return MYLITE_OK;
    }
    if (limit_clause->kind != MYLITE_SQL_AST_LIMIT_CLAUSE) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    rc = convert_limit_integer_literal(database, child_at(limit_clause, 0U), &out_limit->row_count);
    if (rc != MYLITE_OK) {
        return rc;
    }
    out_limit->has_limit = true;
    if (child_at(limit_clause, 1U) != NULL) {
        rc =
            convert_limit_integer_literal(database, child_at(limit_clause, 1U), &out_limit->offset);
        if (rc != MYLITE_OK) {
            return rc;
        }
        out_limit->has_offset = true;
    }

    return MYLITE_OK;
}

static int append_values_result_columns(
    struct mylite_db *database,
    mylite_result *result,
    size_t column_count
) {
    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        char label[sizeof("column_") + uint64_decimal_digit_capacity];
        int written = snprintf(label, sizeof(label), "column_%zu", column_index);
        int rc = MYLITE_OK;

        if (written < 0 || (size_t)written >= sizeof(label)) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        rc = mylite_result_append_column(result, label);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
            return rc;
        }
    }
    return MYLITE_OK;
}

static int append_values_result_rows(
    struct mylite_db *database,
    mylite_result *result,
    const struct mylite_sql_ast_node *rows,
    const struct planned_select_limit *limit,
    size_t column_count
) {
    const struct mylite_sql_ast_node *row = child_at(rows, 0U);
    uint64_t offset = 0U;
    uint64_t row_count = UINT64_MAX;
    uint64_t row_index = 0U;
    uint64_t emitted = 0U;

    if (limit != NULL && limit->has_offset) {
        offset = (uint64_t)limit->offset;
    }
    if (limit != NULL && limit->has_limit) {
        row_count = (uint64_t)limit->row_count;
    }

    while (row != NULL) {
        int rc = MYLITE_OK;

        if (row_index >= offset && emitted < row_count) {
            rc = append_values_result_row(
                database,
                result,
                row,
                (struct values_statement_row_shape){
                    .column_count = column_count,
                    .row_number = (size_t)(row_index + 1U),
                }
            );
            if (rc != MYLITE_OK) {
                return rc;
            }
            ++emitted;
        }
        if (emitted >= row_count) {
            return MYLITE_OK;
        }
        row = row->next_sibling;
        ++row_index;
    }
    return MYLITE_OK;
}

static int append_values_result_row(
    struct mylite_db *database,
    mylite_result *result,
    const struct mylite_sql_ast_node *row,
    struct values_statement_row_shape shape
) {
    const struct mylite_sql_ast_node *value = child_at(row, 0U);
    struct session_scalar_cell *cells = NULL;
    struct mylite_result_cell *result_cells = NULL;
    size_t column_index = 0U;
    int rc = MYLITE_OK;

    if (shape.column_count > SIZE_MAX / sizeof(*cells) ||
        shape.column_count > SIZE_MAX / sizeof(*result_cells)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    cells = (struct session_scalar_cell *)calloc(shape.column_count, sizeof(*cells));
    if (cells == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    result_cells = (struct mylite_result_cell *)calloc(shape.column_count, sizeof(*result_cells));
    if (result_cells == NULL) {
        session_scalar_cell_array_deinit(cells, shape.column_count);
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    while (value != NULL && column_index < shape.column_count && rc == MYLITE_OK) {
        rc = values_statement_value_cell(database, value, &cells[column_index]);
        if (rc == MYLITE_OK) {
            result_cells[column_index] = session_scalar_cell_result_cell(&cells[column_index]);
        }
        value = value->next_sibling;
        ++column_index;
    }
    if (rc == MYLITE_OK && (value != NULL || column_index != shape.column_count)) {
        set_column_count_mismatch_error(database, shape.row_number);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_bytes_row(result, result_cells);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }

    session_scalar_cell_array_deinit(cells, shape.column_count);
    free(result_cells);
    return rc;
}

static int values_statement_value_cell(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value,
    struct session_scalar_cell *out_cell
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    if (value == NULL || out_cell == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value->kind == MYLITE_SQL_AST_DML_DEFAULT_VALUE) {
        set_values_default_error(database);
        return MYLITE_ERROR;
    }
    if (value->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return values_statement_integer_cell(database, value, out_cell);
    }
    if (value->kind != MYLITE_SQL_AST_LITERAL) {
        set_unsupported_error(
            database,
            "VALUES supports only integer, string, NULL, TRUE, and FALSE literals"
        );
        return MYLITE_ERROR;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(value);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        out_cell->value = NULL;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        out_cell->value = "1";
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        out_cell->value = "0";
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
        int rc = decode_sql_string_literal_with_policy(
            database,
            value,
            "VALUES supports only ordinary string literals",
            "VALUES does not support NUL bytes in string literals",
            true,
            &out_cell->owned_text,
            &out_cell->value_size
        );

        if (rc == MYLITE_OK) {
            out_cell->value = out_cell->owned_text;
            out_cell->has_value_size = true;
        }
        return rc;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER) {
        return values_statement_integer_cell(database, value, out_cell);
    }

    set_unsupported_error(
        database,
        "VALUES supports only integer, string, NULL, TRUE, and FALSE literals"
    );
    return MYLITE_ERROR;
}

static int values_statement_integer_cell(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value,
    struct session_scalar_cell *out_cell
) {
    const struct mylite_sql_ast_node *literal = value;
    bool is_negative = false;
    int rc = validate_values_integer_literal(database, value);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (value->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(value);

        is_negative = operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE;
        literal = child_at(value, 0U);
    }

    rc = normalize_decimal_integer_literal(
        database,
        &literal->span,
        is_negative,
        out_cell->literal_text,
        sizeof(out_cell->literal_text)
    );
    if (rc == MYLITE_OK) {
        out_cell->value = out_cell->literal_text;
    }
    return rc;
}

static int execute_scalar_projection_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *select_item = child_at(select_list, 0U);
    struct session_scalar_cell *cells = NULL;
    struct mylite_result_cell *values = NULL;
    mylite_result *result = NULL;
    size_t column_count = mylite_sql_ast_node_child_count(select_list);
    size_t evaluated_column_count = 0U;
    uint64_t previous_last_insert_id = database->session.last_insert_id;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }
    if (column_count > SIZE_MAX / sizeof(*values) || column_count > SIZE_MAX / sizeof(*cells)) {
        mylite_result_free(result);
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    cells = (struct session_scalar_cell *)calloc(column_count, sizeof(*cells));
    if (cells == NULL) {
        mylite_result_free(result);
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    values = (struct mylite_result_cell *)calloc(column_count, sizeof(*values));
    if (values == NULL) {
        session_scalar_cell_array_deinit(cells, 0U);
        mylite_result_free(result);
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = append_select_modifier_warnings(database, statement);
    if (rc == MYLITE_OK) {
        rc = append_session_scalar_select_warnings(database, select_list);
    }
    if (rc == MYLITE_OK) {
        rc = append_scalar_projection_columns_and_values(
            database,
            select_item,
            result,
            cells,
            values,
            &evaluated_column_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_bytes_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK || evaluated_column_count != 0U) {
        int warning_rc =
            append_session_scalar_cells_warnings(database, cells, evaluated_column_count);

        if (warning_rc != MYLITE_OK) {
            rc = warning_rc;
        }
    }
    free(values);
    session_scalar_cell_array_deinit(cells, evaluated_column_count);
    if (rc != MYLITE_OK) {
        database->session.last_insert_id = previous_last_insert_id;
        mylite_result_free(result);
        return rc;
    }
    if (apply_sql_select_limit) {
        apply_sql_select_limit_to_result(database, result);
    }

    return finish_successful_result(database, result, out_result);
}

static int append_scalar_projection_columns_and_values(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_item,
    mylite_result *result,
    struct session_scalar_cell *cells,
    struct mylite_result_cell *values,
    size_t *out_column_count
) {
    size_t column_index = 0U;
    int rc = MYLITE_OK;

    while (rc == MYLITE_OK && select_item != NULL) {
        const struct mylite_sql_ast_node *expression = child_at(select_item, 0U);
        const struct mylite_sql_ast_node *alias = child_at(select_item, 1U);
        char *column_name = NULL;

        if (alias != NULL) {
            rc = copy_select_item_alias_text(database, alias, &column_name);
        } else {
            rc = copy_scalar_projection_column_name(database, expression, &column_name);
        }
        if (rc == MYLITE_OK) {
            rc = append_scalar_projection_result_column(database, expression, column_name, result);
        }
        free(column_name);
        if (rc == MYLITE_OK) {
            rc = session_scalar_value(database, expression, &cells[column_index]);
        }
        if (rc == MYLITE_OK) {
            values[column_index] = session_scalar_cell_result_cell(&cells[column_index]);
            ++column_index;
            select_item = select_item->next_sibling;
        }
    }

    *out_column_count = column_index;
    return rc;
}

static int append_scalar_projection_result_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *column_name,
    mylite_result *result
) {
    struct mylite_result_column_descriptor descriptor;
    int rc = make_scalar_result_column_descriptor(database, expression, column_name, &descriptor);

    if (rc == MYLITE_OK) {
        rc = mylite_result_append_column_descriptor(result, &descriptor);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    return rc;
}

static int make_scalar_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const char *label,
    struct mylite_result_column_descriptor *out_descriptor
) {
    const struct mylite_sql_ast_node *unwrapped = NULL;
    int rc = MYLITE_OK;

    if (out_descriptor == NULL) {
        return MYLITE_MISUSE;
    }
    *out_descriptor = unknown_result_column_descriptor(label);
    unwrapped = unwrap_any_value_function_expression(expression);
    if (unwrapped == NULL) {
        return MYLITE_OK;
    }
    if (unwrapped->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(unwrapped);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_POSITIVE ||
            operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            const struct mylite_sql_ast_node *operand =
                unwrap_parenthesized_expression(child_at(unwrapped, 0U));

            if (operand != NULL && operand->kind == MYLITE_SQL_AST_LITERAL &&
                mylite_sql_ast_node_literal_kind(operand) == MYLITE_SQL_AST_LITERAL_INTEGER) {
                return populate_scalar_literal_result_column_descriptor(
                    database,
                    unwrapped,
                    out_descriptor
                );
            }
        }
    }
    if (unwrapped->kind == MYLITE_SQL_AST_LITERAL) {
        return populate_scalar_literal_result_column_descriptor(
            database,
            unwrapped,
            out_descriptor
        );
    }

    rc = populate_scalar_function_result_column_descriptor(database, unwrapped, out_descriptor);
    return rc;
}

static int populate_scalar_literal_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_result_column_descriptor *descriptor
) {
    const struct mylite_sql_ast_node *literal = unwrap_parenthesized_expression(expression);
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;

    if (literal == NULL || descriptor == NULL) {
        return MYLITE_MISUSE;
    }
    if (literal->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        literal = unwrap_parenthesized_expression(child_at(literal, 0U));
        if (literal == NULL) {
            return MYLITE_OK;
        }
    }
    if (literal->kind != MYLITE_SQL_AST_LITERAL) {
        return MYLITE_OK;
    }

    literal_kind = mylite_sql_ast_node_literal_kind(literal);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER) {
        struct scalar_binary_numeric_result_column_shape shape = {0};
        int rc = scalar_integer_literal_result_column_shape(expression, &shape);

        if (rc != MYLITE_OK) {
            return MYLITE_OK;
        }
        populate_scalar_binary_numeric_result_column_descriptor(descriptor, shape);
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE ||
        literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        populate_scalar_binary_numeric_result_column_descriptor(
            descriptor,
            (struct scalar_binary_numeric_result_column_shape){
                .logical_type = MYLITE_RESULT_LOGICAL_TYPE_LONGLONG,
                .display_length = mysql_tinyint_bool_display_length,
                .decimals = 0U,
                .extra_flags = 0U,
                .nullable = false,
            }
        );
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
        populate_scalar_binary_numeric_result_column_descriptor(
            descriptor,
            (struct scalar_binary_numeric_result_column_shape){
                .logical_type = MYLITE_RESULT_LOGICAL_TYPE_NULL,
                .display_length = 0U,
                .decimals = 0U,
                .extra_flags = 0U,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
        return populate_scalar_string_literal_result_column_descriptor(
            database,
            literal,
            descriptor
        );
    }

    return MYLITE_OK;
}

static int populate_scalar_function_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_result_column_descriptor *descriptor
) {
    if (expression == NULL || descriptor == NULL) {
        return MYLITE_MISUSE;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_DATABASE_FUNCTION:
    case MYLITE_SQL_AST_SCHEMA_FUNCTION:
        populate_scalar_connection_string_result_column_descriptor(
            database,
            descriptor,
            (struct scalar_connection_string_result_column_shape){
                .display_length = mysql_database_function_display_length,
                .extra_flags = 0U,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case MYLITE_SQL_AST_USER_FUNCTION:
    case MYLITE_SQL_AST_SESSION_USER_FUNCTION:
    case MYLITE_SQL_AST_SYSTEM_USER_FUNCTION:
    case MYLITE_SQL_AST_CURRENT_USER_FUNCTION:
        populate_scalar_connection_string_result_column_descriptor(
            database,
            descriptor,
            (struct scalar_connection_string_result_column_shape){
                .display_length = mysql_user_function_display_length,
                .extra_flags = 0U,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case MYLITE_SQL_AST_VERSION_FUNCTION:
        populate_scalar_connection_string_result_column_descriptor(
            database,
            descriptor,
            (struct scalar_connection_string_result_column_shape){
                .display_length = mysql_version_function_display_length,
                .extra_flags = 0U,
                .nullable = false,
            }
        );
        return MYLITE_OK;
    case MYLITE_SQL_AST_ROW_COUNT_FUNCTION:
        populate_scalar_binary_numeric_result_column_descriptor(
            descriptor,
            (struct scalar_binary_numeric_result_column_shape){
                .logical_type = MYLITE_RESULT_LOGICAL_TYPE_LONGLONG,
                .display_length = mysql_scalar_bigint_display_length,
                .decimals = 0U,
                .extra_flags = 0U,
                .nullable = false,
            }
        );
        return MYLITE_OK;
    case MYLITE_SQL_AST_LAST_INSERT_ID_FUNCTION:
        populate_scalar_binary_numeric_result_column_descriptor(
            descriptor,
            (struct scalar_binary_numeric_result_column_shape){
                .logical_type = MYLITE_RESULT_LOGICAL_TYPE_LONGLONG,
                .display_length = mysql_scalar_bigint_display_length,
                .decimals = 0U,
                .extra_flags = MYLITE_RESULT_COLUMN_FLAG_UNSIGNED,
                .nullable = false,
            }
        );
        return MYLITE_OK;
    case MYLITE_SQL_AST_RAND_FUNCTION:
    case MYLITE_SQL_AST_RAND_SEED_FUNCTION:
        populate_scalar_binary_numeric_result_column_descriptor(
            descriptor,
            (struct scalar_binary_numeric_result_column_shape){
                .logical_type = MYLITE_RESULT_LOGICAL_TYPE_DOUBLE,
                .display_length = mysql_scalar_double_display_length,
                .decimals = mysql_approximate_decimals,
                .extra_flags = 0U,
                .nullable = false,
            }
        );
        return MYLITE_OK;
    case MYLITE_SQL_AST_JSON_VALID_FUNCTION:
    case MYLITE_SQL_AST_JSON_LENGTH_FUNCTION:
    case MYLITE_SQL_AST_JSON_CONTAINS_FUNCTION:
    case MYLITE_SQL_AST_JSON_CONTAINS_PATH_FUNCTION:
    case MYLITE_SQL_AST_IS_UUID_FUNCTION:
    case MYLITE_SQL_AST_REGEXP_INSTR_FUNCTION:
    case MYLITE_SQL_AST_PERIOD_ADD_FUNCTION:
    case MYLITE_SQL_AST_PERIOD_DIFF_FUNCTION:
        populate_scalar_binary_numeric_result_column_descriptor(
            descriptor,
            (struct scalar_binary_numeric_result_column_shape){
                .logical_type = MYLITE_RESULT_LOGICAL_TYPE_LONGLONG,
                .display_length = mysql_scalar_bigint_display_length,
                .decimals = 0U,
                .extra_flags = 0U,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case MYLITE_SQL_AST_REGEXP_SUBSTR_FUNCTION:
    case MYLITE_SQL_AST_REGEXP_REPLACE_FUNCTION:
        populate_scalar_connection_string_result_column_descriptor(
            database,
            descriptor,
            (struct scalar_connection_string_result_column_shape){
                .display_length = mysql_regexp_string_function_display_length *
                                  scalar_connection_max_bytes_per_character(database),
                .extra_flags = 0U,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case MYLITE_SQL_AST_UUID_FUNCTION:
        populate_uuid_string_result_column_descriptor(descriptor, true);
        return MYLITE_OK;
    case MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION:
    case MYLITE_SQL_AST_COLLATE_EXPRESSION:
        return populate_scalar_charset_collation_result_column_descriptor(
            database,
            expression,
            descriptor
        );
    case MYLITE_SQL_AST_UUID_TO_BIN_FUNCTION:
        descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING;
        descriptor->charset_id = mysql_collation_binary_id;
        descriptor->collation_id = mysql_collation_binary_id;
        descriptor->display_length = MYLITE_UUID_BINARY_SIZE;
        descriptor->decimals = mysql_approximate_decimals;
        descriptor->flags = MYLITE_RESULT_COLUMN_FLAG_BINARY;
        descriptor->nullable = true;
        return MYLITE_OK;
    case MYLITE_SQL_AST_BIN_TO_UUID_FUNCTION:
        populate_scalar_connection_string_result_column_descriptor(
            database,
            descriptor,
            (struct scalar_connection_string_result_column_shape){
                .display_length = MYLITE_UUID_TEXT_SIZE,
                .extra_flags = 0U,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case MYLITE_SQL_AST_JSON_TYPE_FUNCTION:
        populate_scalar_connection_string_result_column_descriptor(
            database,
            descriptor,
            (struct scalar_connection_string_result_column_shape){
                .display_length = mysql_json_type_display_length,
                .extra_flags = MYLITE_RESULT_COLUMN_FLAG_BINARY,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case MYLITE_SQL_AST_JSON_QUOTE_FUNCTION:
        populate_scalar_connection_string_result_column_descriptor(
            database,
            descriptor,
            (struct scalar_connection_string_result_column_shape){
                .display_length = mysql_json_document_display_length,
                .extra_flags = MYLITE_RESULT_COLUMN_FLAG_BINARY,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case MYLITE_SQL_AST_JSON_VALUE_FUNCTION:
        populate_scalar_connection_string_result_column_descriptor(
            database,
            descriptor,
            (struct scalar_connection_string_result_column_shape){
                .display_length = mysql_json_value_display_character_length *
                                  scalar_connection_max_bytes_per_character(database),
                .extra_flags = MYLITE_RESULT_COLUMN_FLAG_BINARY,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case MYLITE_SQL_AST_JSON_EXTRACT_FUNCTION:
    case MYLITE_SQL_AST_JSON_KEYS_FUNCTION:
    case MYLITE_SQL_AST_JSON_SET_FUNCTION:
    case MYLITE_SQL_AST_JSON_INSERT_FUNCTION:
    case MYLITE_SQL_AST_JSON_REPLACE_FUNCTION:
    case MYLITE_SQL_AST_JSON_REMOVE_FUNCTION:
    case MYLITE_SQL_AST_JSON_ARRAY_FUNCTION:
    case MYLITE_SQL_AST_JSON_OBJECT_FUNCTION:
        populate_scalar_json_result_column_descriptor(database, descriptor);
        return MYLITE_OK;
    case MYLITE_SQL_AST_DATE_ADD_FUNCTION:
    case MYLITE_SQL_AST_DATE_SUB_FUNCTION:
    case MYLITE_SQL_AST_ADDDATE_FUNCTION:
    case MYLITE_SQL_AST_SUBDATE_FUNCTION:
    case MYLITE_SQL_AST_TIMESTAMPADD_FUNCTION:
    case MYLITE_SQL_AST_TIMESTAMP_FUNCTION:
    case MYLITE_SQL_AST_GET_FORMAT_FUNCTION:
    case MYLITE_SQL_AST_CONVERT_TZ_FUNCTION:
        populate_scalar_temporal_string_result_column_descriptor(database, descriptor);
        return MYLITE_OK;
    case MYLITE_SQL_AST_WEIGHT_STRING_FUNCTION:
    case MYLITE_SQL_AST_WEIGHT_STRING_BINARY_FUNCTION:
        descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING;
        descriptor->charset_id = mysql_collation_binary_id;
        descriptor->collation_id = mysql_collation_binary_id;
        descriptor->display_length = mysql_scalar_bigint_display_length;
        descriptor->decimals = mysql_approximate_decimals;
        descriptor->flags = MYLITE_RESULT_COLUMN_FLAG_BINARY;
        descriptor->nullable = true;
        return MYLITE_OK;
    case MYLITE_SQL_AST_SYSDATE_FUNCTION:
        populate_scalar_datetime_result_column_descriptor(descriptor);
        return MYLITE_OK;
    case MYLITE_SQL_AST_DAYNAME_FUNCTION:
    case MYLITE_SQL_AST_MONTHNAME_FUNCTION:
        populate_calendar_name_result_column_descriptor(database, descriptor);
        return MYLITE_OK;
    default:
        return MYLITE_OK;
    }
}

static void populate_scalar_temporal_string_result_column_descriptor(
    const struct mylite_db *database,
    struct mylite_result_column_descriptor *descriptor
) {
    descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_STRING;
    descriptor->charset_id = result_metadata_collation_id(database->session.collation_connection);
    descriptor->collation_id = descriptor->charset_id;
    descriptor->display_length = mysql_temporal_string_function_display_length *
                                 scalar_connection_max_bytes_per_character(database);
    descriptor->decimals = mysql_approximate_decimals;
    descriptor->flags = 0U;
    descriptor->nullable = true;
}

static void populate_calendar_name_result_column_descriptor(
    const struct mylite_db *database,
    struct mylite_result_column_descriptor *descriptor
) {
    populate_scalar_connection_string_result_column_descriptor(
        database,
        descriptor,
        (struct scalar_connection_string_result_column_shape){
            .display_length = mysql_calendar_name_function_display_length *
                              scalar_connection_max_bytes_per_character(database),
            .extra_flags = 0U,
            .nullable = true,
        }
    );
}

static void populate_scalar_date_result_column_descriptor(
    struct mylite_result_column_descriptor *descriptor
) {
    descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_DATE;
    descriptor->charset_id = mysql_collation_binary_id;
    descriptor->collation_id = mysql_collation_binary_id;
    descriptor->display_length = date_text_length;
    descriptor->decimals = 0U;
    descriptor->flags = MYLITE_RESULT_COLUMN_FLAG_BINARY;
    descriptor->nullable = true;
}

static void populate_scalar_datetime_result_column_descriptor(
    struct mylite_result_column_descriptor *descriptor
) {
    descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_DATETIME;
    descriptor->charset_id = mysql_collation_binary_id;
    descriptor->collation_id = mysql_collation_binary_id;
    descriptor->display_length = datetime_text_length;
    descriptor->decimals = 0U;
    descriptor->flags = MYLITE_RESULT_COLUMN_FLAG_BINARY;
    descriptor->nullable = true;
}

static int populate_scalar_string_literal_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    struct mylite_result_column_descriptor *descriptor
) {
    char *text = NULL;
    size_t text_length = 0U;
    uint64_t max_bytes_per_character = scalar_connection_max_bytes_per_character(database);
    uint64_t display_length = 0U;
    int rc = decode_sql_string_literal_with_policy(
        database,
        literal,
        "scalar metadata supports only ordinary string literals",
        "scalar metadata does not support NUL bytes in string literals",
        true,
        &text,
        &text_length
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    free(text);
    if (text_length > UINT64_MAX / max_bytes_per_character) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    display_length = (uint64_t)text_length * max_bytes_per_character;
    populate_scalar_connection_string_result_column_descriptor(
        database,
        descriptor,
        (struct scalar_connection_string_result_column_shape){
            .display_length = display_length,
            .extra_flags = 0U,
            .nullable = false,
        }
    );
    return MYLITE_OK;
}

static int scalar_integer_literal_result_column_shape(
    const struct mylite_sql_ast_node *expression,
    struct scalar_binary_numeric_result_column_shape *out_shape
) {
    const struct mylite_sql_ast_node *unwrapped = unwrap_parenthesized_expression(expression);
    bool is_negative = false;

    if (unwrapped == NULL || out_shape == NULL) {
        return MYLITE_MISUSE;
    }
    if (unwrapped->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(unwrapped);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        }
        unwrapped = unwrap_parenthesized_expression(child_at(unwrapped, 0U));
    }
    return scalar_unsigned_integer_literal_result_column_shape(unwrapped, is_negative, out_shape);
}

static int scalar_unsigned_integer_literal_result_column_shape(
    const struct mylite_sql_ast_node *literal,
    bool is_negative,
    struct scalar_binary_numeric_result_column_shape *out_shape
) {
    uint64_t magnitude = 0U;
    uint64_t display_length = 0U;
    uint64_t signed_min_magnitude = (uint64_t)INT64_MAX + 1U;

    if (out_shape == NULL || literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
        return MYLITE_ERROR;
    }
    if (parse_unsigned_integer_literal(&literal->span, &magnitude) == MYLITE_OK) {
        if (is_negative && magnitude > signed_min_magnitude) {
            return scalar_newdecimal_integer_literal_result_column_shape(&literal->span, out_shape);
        }
        display_length = (uint64_t)literal->span.length + 1U;
        if (display_length > mysql_bigint_display_length) {
            display_length = mysql_bigint_display_length;
        }
        *out_shape = (struct scalar_binary_numeric_result_column_shape){
            .logical_type = MYLITE_RESULT_LOGICAL_TYPE_LONGLONG,
            .display_length = display_length,
            .decimals = 0U,
            .extra_flags = (!is_negative && magnitude > (uint64_t)INT64_MAX)
                               ? (uint32_t)MYLITE_RESULT_COLUMN_FLAG_UNSIGNED
                               : 0U,
            .nullable = false,
        };
        return MYLITE_OK;
    }

    return scalar_newdecimal_integer_literal_result_column_shape(&literal->span, out_shape);
}

static int scalar_newdecimal_integer_literal_result_column_shape(
    const struct mylite_sql_source_span *span,
    struct scalar_binary_numeric_result_column_shape *out_shape
) {
    size_t significant_digit_count = 0U;

    if (out_shape == NULL) {
        return MYLITE_MISUSE;
    }
    if (scalar_decimal_integer_significant_digit_count(span, &significant_digit_count) !=
            MYLITE_OK ||
        significant_digit_count == 0U ||
        significant_digit_count > literal_projection_max_significant_digits) {
        return MYLITE_ERROR;
    }
    *out_shape = (struct scalar_binary_numeric_result_column_shape){
        .logical_type = MYLITE_RESULT_LOGICAL_TYPE_NEWDECIMAL,
        .display_length = (uint64_t)significant_digit_count + 1U,
        .decimals = 0U,
        .extra_flags = 0U,
        .nullable = false,
    };
    return MYLITE_OK;
}

static int scalar_decimal_integer_significant_digit_count(
    const struct mylite_sql_source_span *span,
    size_t *out_significant_digit_count
) {
    size_t first_significant_digit = 0U;

    if (span == NULL || span->text == NULL || out_significant_digit_count == NULL) {
        return MYLITE_MISUSE;
    }
    first_significant_digit = span->length;
    for (size_t index = 0U; index < span->length; ++index) {
        unsigned char byte = (unsigned char)span->text[index];

        if (!isdigit(byte)) {
            return MYLITE_ERROR;
        }
        if (byte != (unsigned char)'0' && first_significant_digit == span->length) {
            first_significant_digit = index;
        }
    }
    *out_significant_digit_count =
        first_significant_digit == span->length ? 0U : span->length - first_significant_digit;
    return MYLITE_OK;
}

static uint64_t scalar_connection_max_bytes_per_character(const struct mylite_db *database) {
    const char *character_set_name =
        database == NULL ? NULL : database->session.character_set_connection;
    const struct mylite_execution_catalog_character_set *character_set =
        character_set_by_name(character_set_name);
    struct mylite_sql_source_span maxlen_span = {0};
    uint64_t maxlen = 0U;

    if (character_set == NULL) {
        return 4U;
    }
    maxlen_span = (struct mylite_sql_source_span){
        .text = character_set->maxlen,
        .length = strlen(character_set->maxlen),
    };
    if (parse_unsigned_integer_literal(&maxlen_span, &maxlen) != MYLITE_OK || maxlen == 0U) {
        return 4U;
    }
    return maxlen;
}

static void populate_scalar_binary_numeric_result_column_descriptor(
    struct mylite_result_column_descriptor *descriptor,
    struct scalar_binary_numeric_result_column_shape shape
) {
    descriptor->logical_type = shape.logical_type;
    descriptor->charset_id = mysql_collation_binary_id;
    descriptor->collation_id = mysql_collation_binary_id;
    descriptor->display_length = shape.display_length;
    descriptor->decimals = shape.decimals;
    descriptor->flags =
        MYLITE_RESULT_COLUMN_FLAG_BINARY | MYLITE_RESULT_COLUMN_FLAG_NUM | shape.extra_flags;
    descriptor->nullable = shape.nullable;
    if (!shape.nullable) {
        descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_NOT_NULL;
    }
}

static int populate_scalar_charset_collation_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct mylite_result_column_descriptor *descriptor
) {
    const char *charset = NULL;
    const char *collation = NULL;
    struct session_scalar_cell cell = {0};
    uint64_t max_bytes_per_character = 4U;
    uint64_t display_length = 0U;
    size_t value_length = 0U;
    int rc = MYLITE_OK;

    if (descriptor == NULL) {
        return MYLITE_MISUSE;
    }

    rc = scalar_expression_charset_collation_metadata(database, expression, &charset, &collation);
    if (rc == MYLITE_OK) {
        rc = session_scalar_value(database, expression, &cell);
    }
    if (rc != MYLITE_OK) {
        session_scalar_cell_deinit(&cell);
        return rc;
    }

    if (text_equals_ascii_case_insensitive(charset, "latin1") ||
        text_equals_ascii_case_insensitive(charset, "binary")) {
        max_bytes_per_character = 1U;
    } else if (text_equals_ascii_case_insensitive(charset, "utf8mb3")) {
        max_bytes_per_character = 3U;
    }
    value_length = cell.value == NULL ? 0U : strlen(cell.value);
    if (value_length > UINT64_MAX / max_bytes_per_character) {
        session_scalar_cell_deinit(&cell);
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    display_length = (uint64_t)value_length * max_bytes_per_character;

    descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING;
    descriptor->charset_id = result_metadata_collation_id(collation);
    descriptor->collation_id = descriptor->charset_id;
    descriptor->display_length = display_length;
    descriptor->decimals = mysql_approximate_decimals;
    descriptor->flags = 0U;
    descriptor->nullable = true;

    session_scalar_cell_deinit(&cell);
    return MYLITE_OK;
}

static void populate_scalar_connection_string_result_column_descriptor(
    const struct mylite_db *database,
    struct mylite_result_column_descriptor *descriptor,
    struct scalar_connection_string_result_column_shape shape
) {
    descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING;
    descriptor->charset_id = result_metadata_collation_id(database->session.collation_connection);
    descriptor->collation_id = descriptor->charset_id;
    descriptor->display_length = shape.display_length;
    descriptor->decimals = mysql_approximate_decimals;
    descriptor->flags = shape.extra_flags;
    descriptor->nullable = shape.nullable;
    if (!shape.nullable) {
        descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_NOT_NULL;
    }
}

static void populate_uuid_string_result_column_descriptor(
    struct mylite_result_column_descriptor *descriptor,
    bool nullable
) {
    descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING;
    descriptor->charset_id = mysql_collation_utf8mb3_general_ci_id;
    descriptor->collation_id = mysql_collation_utf8mb3_general_ci_id;
    descriptor->display_length = MYLITE_UUID_TEXT_SIZE;
    descriptor->decimals = mysql_approximate_decimals;
    descriptor->flags = 0U;
    descriptor->nullable = nullable;
    if (!nullable) {
        descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_NOT_NULL;
    }
}

static void populate_scalar_json_result_column_descriptor(
    const struct mylite_db *database,
    struct mylite_result_column_descriptor *descriptor
) {
    descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_JSON;
    descriptor->charset_id = result_metadata_collation_id(database->session.collation_connection);
    descriptor->collation_id = descriptor->charset_id;
    descriptor->display_length = mysql_json_document_display_length;
    descriptor->decimals = mysql_approximate_decimals;
    descriptor->flags = MYLITE_RESULT_COLUMN_FLAG_BINARY;
    descriptor->nullable = true;
}

static struct mylite_result_cell session_scalar_cell_result_cell(
    const struct session_scalar_cell *cell
) {
    size_t value_size = 0U;

    if (cell == NULL || cell->value == NULL) {
        return (struct mylite_result_cell){.is_null = true};
    }
    value_size = strlen(cell->value);
    if (cell->has_value_size) {
        value_size = cell->value_size;
    }

    return (struct mylite_result_cell){
        .bytes = cell->value,
        .size = value_size,
        .is_null = false,
    };
}

static int copy_scalar_projection_column_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    char **out_text
) {
    const struct mylite_sql_ast_node *unwrapped = NULL;

    if (out_text == NULL) {
        return MYLITE_MISUSE;
    }
    *out_text = NULL;
    if (expression == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    unwrapped = unwrap_parenthesized_expression(expression);
    if (unwrapped != NULL && unwrapped->kind == MYLITE_SQL_AST_UNARY_EXPRESSION &&
        mylite_sql_ast_node_operator(unwrapped) == MYLITE_SQL_AST_OPERATOR_POSITIVE) {
        const struct mylite_sql_ast_node *literal =
            unwrap_parenthesized_expression(child_at(unwrapped, 0U));

        while (literal != NULL && literal->kind == MYLITE_SQL_AST_UNARY_EXPRESSION &&
               mylite_sql_ast_node_operator(literal) == MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            literal = unwrap_parenthesized_expression(child_at(literal, 0U));
        }
        if (literal != NULL && literal->kind == MYLITE_SQL_AST_LITERAL) {
            enum mylite_sql_ast_literal_kind literal_kind =
                mylite_sql_ast_node_literal_kind(literal);

            if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER ||
                literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
                return copy_source_span_text(database, &literal->span, out_text);
            }
        }
    }
    if (unwrapped != NULL && unwrapped->kind == MYLITE_SQL_AST_LITERAL) {
        enum mylite_sql_ast_literal_kind literal_kind = mylite_sql_ast_node_literal_kind(unwrapped);

        if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER ||
            literal_kind == MYLITE_SQL_AST_LITERAL_NULL) {
            return copy_source_span_text(database, &unwrapped->span, out_text);
        }
    }

    return copy_source_span_text(database, &expression->span, out_text);
}

static bool do_statement_has_only_scalar_projection_expressions(
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *expression_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *expression = NULL;

    if (expression_list == NULL || expression_list->kind != MYLITE_SQL_AST_DO_EXPRESSION_LIST) {
        return false;
    }
    expression = child_at(expression_list, 0U);
    if (expression == NULL) {
        return false;
    }
    while (expression != NULL) {
        if (is_scalar_subquery_projection_expression(expression) ||
            !is_scalar_projection_expression(expression)) {
            return false;
        }
        expression = expression->next_sibling;
    }

    return true;
}

static int append_session_scalar_do_warnings(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *expression_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *expression = child_at(expression_list, 0U);
    int rc = MYLITE_OK;

    while (rc == MYLITE_OK && expression != NULL) {
        rc = append_session_scalar_expression_warnings(database, expression);
        expression = expression->next_sibling;
    }

    return rc;
}

static int append_session_scalar_select_warnings(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list
) {
    const struct mylite_sql_ast_node *select_item = child_at(select_list, 0U);
    int rc = MYLITE_OK;

    while (rc == MYLITE_OK && select_item != NULL) {
        const struct mylite_sql_ast_node *expression =
            unwrap_parenthesized_expression(child_at(select_item, 0U));

        rc = append_session_scalar_expression_warnings(database, expression);
        select_item = select_item->next_sibling;
    }

    return rc;
}

static int append_session_scalar_expression_warnings(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
) {
    struct scalar_arithmetic_node_stack stack = {0};
    int rc = MYLITE_OK;

    if (!scalar_arithmetic_node_stack_push(&stack, expression)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    while (rc == MYLITE_OK && stack.count != 0U) {
        const struct mylite_sql_ast_node *current =
            unwrap_parenthesized_expression(stack.items[--stack.count]);
        size_t child_count = 0U;

        if (current == NULL) {
            continue;
        }
        if (current->kind == MYLITE_SQL_AST_FOUND_ROWS_FUNCTION) {
            rc = append_found_rows_deprecation_warning(database);
        } else if (current->kind == MYLITE_SQL_AST_CONVERT_USING_CHARSET_EXPRESSION) {
            rc = append_convert_using_charset_warning(database, current);
        } else if (current->kind == MYLITE_SQL_AST_SYSTEM_VARIABLE) {
            enum mylite_execution_system_variable_kind variable =
                MYLITE_EXECUTION_SYSTEM_VARIABLE_NONE;

            rc = resolve_session_system_variable(database, current, &variable);
            if (rc == MYLITE_OK &&
                mylite_execution_system_variable_warns_on_scalar_read(variable)) {
                rc = append_system_variable_read_warning(database, variable);
            }
        }

        child_count = mylite_sql_ast_node_child_count(current);
        for (size_t child_index = 0U; rc == MYLITE_OK && child_index < child_count; ++child_index) {
            if (!scalar_arithmetic_node_stack_push(&stack, child_at(current, child_index))) {
                set_nomem_error(database);
                rc = MYLITE_NOMEM;
            }
        }
    }

    scalar_arithmetic_node_stack_deinit(&stack);
    return rc;
}

static int append_select_modifier_warnings(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
) {
    unsigned int options = mylite_sql_ast_node_select_options(statement);

    if ((options & MYLITE_SQL_AST_SELECT_OPTION_SQL_NO_CACHE) == 0U) {
        return MYLITE_OK;
    }

    return append_sql_no_cache_deprecation_warning(database);
}

static int append_found_rows_deprecation_warning(struct mylite_db *database) {
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_found_rows_deprecated,
        "HY000",
        "FOUND_ROWS() is deprecated and will be removed in a future release. Consider using "
        "COUNT(*) instead."
    );
}

static int append_sql_calc_found_rows_deprecation_warning(struct mylite_db *database) {
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_sql_calc_found_rows_deprecated,
        "HY000",
        "SQL_CALC_FOUND_ROWS is deprecated and will be removed in a future release. Consider "
        "using two separate queries instead."
    );
}

static int append_sql_no_cache_deprecation_warning(struct mylite_db *database) {
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_sql_no_cache_deprecated,
        "HY000",
        "'SQL_NO_CACHE' is deprecated and will be removed in a future release."
    );
}

static int append_convert_using_charset_warning(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
) {
    struct scalar_convert_charset_info info = {0};
    int rc = scalar_convert_charset_info_for_expression(database, expression, &info);

    if (rc != MYLITE_OK) {
        return rc;
    }
    switch (info.warning) {
    case SCALAR_CONVERT_CHARSET_WARNING_UTF8_ALIAS:
        return append_utf8_alias_warning(database);
    case SCALAR_CONVERT_CHARSET_WARNING_UTF8MB3_DEPRECATED:
        return append_utf8mb3_deprecation_warning(database);
    case SCALAR_CONVERT_CHARSET_WARNING_NONE:
        return MYLITE_OK;
    }

    return MYLITE_OK;
}

static int append_utf8_alias_warning(struct mylite_db *database) {
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_utf8_alias,
        "HY000",
        "'utf8' is currently an alias for the character set UTF8MB3, but will be an alias "
        "for UTF8MB4 in a future release. Please consider using UTF8MB4 in order to be "
        "unambiguous."
    );
}

static int append_utf8mb3_deprecation_warning(struct mylite_db *database) {
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_utf8mb3_deprecated,
        "HY000",
        "'utf8mb3' is deprecated and will be removed in a future release. Please use "
        "utf8mb4 instead"
    );
}

static int accumulate_staged_division_by_zero_warnings(
    struct mylite_db *database,
    size_t cell_warning_count,
    size_t *total_warning_count
) {
    return accumulate_staged_warning_count(database, cell_warning_count, total_warning_count);
}

static int accumulate_staged_warning_count(
    struct mylite_db *database,
    size_t cell_warning_count,
    size_t *total_warning_count
) {
    if (total_warning_count == NULL) {
        return MYLITE_MISUSE;
    }
    if (*total_warning_count > SIZE_MAX - cell_warning_count) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    *total_warning_count += cell_warning_count;
    return MYLITE_OK;
}

static int append_session_scalar_cell_warnings(
    struct mylite_db *database,
    const struct session_scalar_cell *cell
) {
    int rc = MYLITE_OK;

    if (cell == NULL) {
        return MYLITE_MISUSE;
    }
    if (cell->has_staged_truncated_integer_warning) {
        rc = append_truncated_incorrect_integer_warning(
            database,
            cell->staged_truncated_integer_text
        );
    }
    if (rc == MYLITE_OK && cell->has_staged_truncated_decimal_warning) {
        rc = append_truncated_incorrect_decimal_warning(
            database,
            cell->staged_truncated_decimal_text
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_signed_complement_warnings(
            database,
            cell->staged_signed_complement_warning_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_unsigned_complement_warnings(
            database,
            cell->staged_unsigned_complement_warning_count
        );
    }
    if (rc == MYLITE_OK) {
        rc =
            append_division_by_zero_warnings(database, cell->staged_division_by_zero_warning_count);
    }
    if (rc == MYLITE_OK) {
        rc = append_invalid_logarithm_warnings(
            database,
            cell->staged_invalid_logarithm_warning_count
        );
    }
    if (rc == MYLITE_OK && cell->has_staged_unhex_incorrect_string_warning) {
        rc = append_unhex_incorrect_string_warning(
            database,
            cell->staged_unhex_incorrect_string_text
        );
    }
    return rc;
}

static int append_session_scalar_cells_warnings(
    struct mylite_db *database,
    const struct session_scalar_cell *cells,
    size_t cell_count
) {
    int rc = MYLITE_OK;

    if (cells == NULL && cell_count != 0U) {
        return MYLITE_MISUSE;
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < cell_count; ++index) {
        rc = append_session_scalar_cell_warnings(database, &cells[index]);
    }
    return rc;
}

static int append_division_by_zero_warnings(struct mylite_db *database, size_t warning_count) {
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < warning_count; ++index) {
        rc = mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_division_by_zero,
            "22012",
            "Division by 0"
        );
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        }
    }

    return rc;
}

static int append_invalid_logarithm_warnings(struct mylite_db *database, size_t warning_count) {
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < warning_count; ++index) {
        rc = mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_invalid_argument_for_logarithm,
            "HY000",
            "Invalid argument for logarithm"
        );
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        }
    }

    return rc;
}

static int append_truncated_incorrect_integer_warning(
    struct mylite_db *database,
    const char *value_text
) {
    static const char prefix[] = "Truncated incorrect INTEGER value: '";
    static const char suffix[] = "'";
    char *message = NULL;
    size_t value_length = 0U;
    size_t message_length = 0U;
    int rc = MYLITE_OK;

    if (value_text == NULL) {
        return MYLITE_MISUSE;
    }
    value_length = strlen(value_text);
    if (value_length > SIZE_MAX - sizeof(prefix) - sizeof(suffix)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    message_length = (sizeof(prefix) - 1U) + value_length + (sizeof(suffix) - 1U);
    message = (char *)malloc(message_length + 1U);
    if (message == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    memcpy(message, prefix, sizeof(prefix) - 1U);
    memcpy(message + sizeof(prefix) - 1U, value_text, value_length);
    memcpy(message + sizeof(prefix) - 1U + value_length, suffix, sizeof(suffix));

    rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_truncated_incorrect_integer,
        "22007",
        message
    );
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    free(message);
    return rc;
}

static int append_truncated_incorrect_decimal_warning(
    struct mylite_db *database,
    const char *value_text
) {
    char
        message[sizeof("Truncated incorrect DECIMAL value: ''") + literal_projection_text_capacity];
    int written = 0;
    int rc = MYLITE_OK;

    if (value_text == NULL) {
        return MYLITE_MISUSE;
    }
    written =
        snprintf(message, sizeof(message), "Truncated incorrect DECIMAL value: '%s'", value_text);
    if (written < 0 || (size_t)written >= sizeof(message)) {
        set_runtime_error(database, "failed to format CONV() warning");
        return MYLITE_ERROR;
    }

    rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_truncated_incorrect_decimal,
        "22007",
        message
    );
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    return rc;
}

static int append_signed_complement_warnings(struct mylite_db *database, size_t warning_count) {
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < warning_count; ++index) {
        rc = mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_cast_complement,
            "HY000",
            "Cast to signed converted positive out-of-range integer to its negative complement"
        );
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        }
    }

    return rc;
}

static int append_unsigned_complement_warnings(struct mylite_db *database, size_t warning_count) {
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < warning_count; ++index) {
        rc = mylite_diagnostics_append_warning(
            mylite_connection_diagnostics(database),
            mysql_warning_cast_complement,
            "HY000",
            "Cast to unsigned converted negative integer to its positive complement"
        );
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        }
    }

    return rc;
}

static int append_unhex_incorrect_string_warning(
    struct mylite_db *database,
    const char *value_text
) {
    return mylite_string_unhex_append_incorrect_warning(
        database,
        value_text,
        value_text == NULL ? 0U : strlen(value_text)
    );
}

static const char *do_statement_argument_count_error_function(
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *expression_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *expression = child_at(expression_list, 0U);
    const char *function_name = NULL;

    while (expression != NULL) {
        function_name = argument_count_error_function_name(expression);
        if (function_name != NULL) {
            return function_name;
        }
        expression = expression->next_sibling;
    }

    return NULL;
}

static const char *select_statement_argument_count_error_function(
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *select_item = NULL;
    const char *function_name = NULL;

    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        return NULL;
    }

    select_item = child_at(select_list, 0U);
    while (select_item != NULL) {
        const struct mylite_sql_ast_node *expression = child_at(select_item, 0U);

        expression = unwrap_parenthesized_expression(expression);
        function_name = argument_count_error_function_name(expression);
        if (function_name != NULL) {
            return function_name;
        }
        select_item = select_item->next_sibling;
    }

    return NULL;
}

static const char *argument_count_error_function_name(
    const struct mylite_sql_ast_node *expression
) {
    struct scalar_arithmetic_node_stack stack = {0};
    const char *function_name = NULL;

    if (expression == NULL) {
        return NULL;
    }
    if (!scalar_arithmetic_node_stack_push(&stack, expression)) {
        return NULL;
    }
    while (stack.count != 0U && function_name == NULL) {
        const struct mylite_sql_ast_node *current = stack.items[--stack.count];
        size_t child_count = 0U;

        if (current == NULL) {
            continue;
        }
        function_name = argument_count_error_node_function_name(current);
        if (function_name != NULL) {
            break;
        }

        child_count = mylite_sql_ast_node_child_count(current);
        for (size_t child_index = 0U; child_index < child_count; ++child_index) {
            if (!scalar_arithmetic_node_stack_push(&stack, child_at(current, child_index))) {
                scalar_arithmetic_node_stack_deinit(&stack);
                return NULL;
            }
        }
    }
    scalar_arithmetic_node_stack_deinit(&stack);

    return function_name;
}

static const char *argument_count_error_node_function_name(
    const struct mylite_sql_ast_node *expression
) {
    if (expression == NULL) {
        return NULL;
    }

    switch (expression->kind) {
    case MYLITE_SQL_AST_CONNECTION_ID_ARGUMENT_COUNT_ERROR:
        return "CONNECTION_ID";
    case MYLITE_SQL_AST_VERSION_ARGUMENT_COUNT_ERROR:
        return "VERSION";
    case MYLITE_SQL_AST_PI_ARGUMENT_COUNT_ERROR:
        return "PI";
    case MYLITE_SQL_AST_RAND_ARGUMENT_COUNT_ERROR:
        return "RAND";
    case MYLITE_SQL_AST_SQRT_ARGUMENT_COUNT_ERROR:
        return "SQRT";
    case MYLITE_SQL_AST_DEGREES_ARGUMENT_COUNT_ERROR:
        return "DEGREES";
    case MYLITE_SQL_AST_RADIANS_ARGUMENT_COUNT_ERROR:
        return "RADIANS";
    case MYLITE_SQL_AST_ACOS_ARGUMENT_COUNT_ERROR:
        return "ACOS";
    case MYLITE_SQL_AST_ASIN_ARGUMENT_COUNT_ERROR:
        return "ASIN";
    case MYLITE_SQL_AST_SIN_ARGUMENT_COUNT_ERROR:
        return "SIN";
    case MYLITE_SQL_AST_COS_ARGUMENT_COUNT_ERROR:
        return "COS";
    case MYLITE_SQL_AST_TAN_ARGUMENT_COUNT_ERROR:
        return "TAN";
    case MYLITE_SQL_AST_COT_ARGUMENT_COUNT_ERROR:
        return "COT";
    case MYLITE_SQL_AST_ATAN_ARGUMENT_COUNT_ERROR:
        return "ATAN";
    case MYLITE_SQL_AST_ATAN2_ARGUMENT_COUNT_ERROR:
        return "ATAN2";
    case MYLITE_SQL_AST_EXP_ARGUMENT_COUNT_ERROR:
        return "EXP";
    case MYLITE_SQL_AST_LN_ARGUMENT_COUNT_ERROR:
        return "LN";
    case MYLITE_SQL_AST_LOG10_ARGUMENT_COUNT_ERROR:
        return "LOG10";
    case MYLITE_SQL_AST_LOG2_ARGUMENT_COUNT_ERROR:
        return "LOG2";
    case MYLITE_SQL_AST_POW_ARGUMENT_COUNT_ERROR:
        return "POW";
    case MYLITE_SQL_AST_POWER_ARGUMENT_COUNT_ERROR:
        return "POWER";
    case MYLITE_SQL_AST_ANY_VALUE_ARGUMENT_COUNT_ERROR:
        return "ANY_VALUE";
    case MYLITE_SQL_AST_CURRENT_ROLE_ARGUMENT_COUNT_ERROR:
        return "CURRENT_ROLE";
    case MYLITE_SQL_AST_FOUND_ROWS_ARGUMENT_COUNT_ERROR:
        return "FOUND_ROWS";
    case MYLITE_SQL_AST_LAST_INSERT_ID_ARGUMENT_COUNT_ERROR:
        return "LAST_INSERT_ID";
    case MYLITE_SQL_AST_IFNULL_ARGUMENT_COUNT_ERROR:
        return "IFNULL";
    case MYLITE_SQL_AST_NULLIF_ARGUMENT_COUNT_ERROR:
        return "NULLIF";
    case MYLITE_SQL_AST_ISNULL_ARGUMENT_COUNT_ERROR:
        return "ISNULL";
    case MYLITE_SQL_AST_ABS_ARGUMENT_COUNT_ERROR:
        return "ABS";
    case MYLITE_SQL_AST_SIGN_ARGUMENT_COUNT_ERROR:
        return "SIGN";
    case MYLITE_SQL_AST_CEIL_ARGUMENT_COUNT_ERROR:
        return "CEIL";
    case MYLITE_SQL_AST_CEILING_ARGUMENT_COUNT_ERROR:
        return "CEILING";
    case MYLITE_SQL_AST_FLOOR_ARGUMENT_COUNT_ERROR:
        return "FLOOR";
    case MYLITE_SQL_AST_ROUND_ARGUMENT_COUNT_ERROR:
        return "ROUND";
    case MYLITE_SQL_AST_BIN_ARGUMENT_COUNT_ERROR:
        return "BIN";
    case MYLITE_SQL_AST_OCT_ARGUMENT_COUNT_ERROR:
        return "OCT";
    case MYLITE_SQL_AST_CONV_ARGUMENT_COUNT_ERROR:
        return "CONV";
    case MYLITE_SQL_AST_CONCAT_ARGUMENT_COUNT_ERROR:
        return "CONCAT";
    case MYLITE_SQL_AST_CONCAT_WS_ARGUMENT_COUNT_ERROR:
        return "CONCAT_WS";
    case MYLITE_SQL_AST_ELT_ARGUMENT_COUNT_ERROR:
        return "ELT";
    case MYLITE_SQL_AST_EXPORT_SET_ARGUMENT_COUNT_ERROR:
        return "EXPORT_SET";
    case MYLITE_SQL_AST_FIELD_ARGUMENT_COUNT_ERROR:
        return "FIELD";
    case MYLITE_SQL_AST_MAKE_SET_ARGUMENT_COUNT_ERROR:
        return "MAKE_SET";
    case MYLITE_SQL_AST_GREATEST_ARGUMENT_COUNT_ERROR:
        return "GREATEST";
    case MYLITE_SQL_AST_LEAST_ARGUMENT_COUNT_ERROR:
        return "LEAST";
    case MYLITE_SQL_AST_JSON_VALID_ARGUMENT_COUNT_ERROR:
        return "JSON_VALID";
    case MYLITE_SQL_AST_JSON_EXTRACT_ARGUMENT_COUNT_ERROR:
        return "JSON_EXTRACT";
    case MYLITE_SQL_AST_JSON_CONTAINS_ARGUMENT_COUNT_ERROR:
        return "JSON_CONTAINS";
    case MYLITE_SQL_AST_JSON_CONTAINS_PATH_ARGUMENT_COUNT_ERROR:
        return "JSON_CONTAINS_PATH";
    case MYLITE_SQL_AST_JSON_LENGTH_ARGUMENT_COUNT_ERROR:
        return "JSON_LENGTH";
    case MYLITE_SQL_AST_JSON_KEYS_ARGUMENT_COUNT_ERROR:
        return "JSON_KEYS";
    case MYLITE_SQL_AST_JSON_TYPE_ARGUMENT_COUNT_ERROR:
        return "JSON_TYPE";
    case MYLITE_SQL_AST_JSON_SET_ARGUMENT_COUNT_ERROR:
        return "JSON_SET";
    case MYLITE_SQL_AST_JSON_REPLACE_ARGUMENT_COUNT_ERROR:
        return "JSON_REPLACE";
    case MYLITE_SQL_AST_JSON_REMOVE_ARGUMENT_COUNT_ERROR:
        return "JSON_REMOVE";
    case MYLITE_SQL_AST_JSON_QUOTE_ARGUMENT_COUNT_ERROR:
        return "JSON_QUOTE";
    case MYLITE_SQL_AST_JSON_UNQUOTE_ARGUMENT_COUNT_ERROR:
        return "JSON_UNQUOTE";
    case MYLITE_SQL_AST_FIND_IN_SET_ARGUMENT_COUNT_ERROR:
        return "FIND_IN_SET";
    case MYLITE_SQL_AST_STRCMP_ARGUMENT_COUNT_ERROR:
        return "STRCMP";
    case MYLITE_SQL_AST_SUBSTRING_INDEX_ARGUMENT_COUNT_ERROR:
        return "SUBSTRING_INDEX";
    case MYLITE_SQL_AST_LPAD_ARGUMENT_COUNT_ERROR:
        return "LPAD";
    case MYLITE_SQL_AST_RPAD_ARGUMENT_COUNT_ERROR:
        return "RPAD";
    case MYLITE_SQL_AST_SPACE_ARGUMENT_COUNT_ERROR:
        return "SPACE";
    case MYLITE_SQL_AST_REGEXP_LIKE_ARGUMENT_COUNT_ERROR:
        return "REGEXP_LIKE";
    case MYLITE_SQL_AST_REGEXP_INSTR_ARGUMENT_COUNT_ERROR:
        return "REGEXP_INSTR";
    case MYLITE_SQL_AST_REGEXP_SUBSTR_ARGUMENT_COUNT_ERROR:
        return "REGEXP_SUBSTR";
    case MYLITE_SQL_AST_REGEXP_REPLACE_ARGUMENT_COUNT_ERROR:
        return "REGEXP_REPLACE";
    case MYLITE_SQL_AST_SOUNDEX_ARGUMENT_COUNT_ERROR:
        return "SOUNDEX";
    case MYLITE_SQL_AST_DATE_FORMAT_ARGUMENT_COUNT_ERROR:
        return "DATE_FORMAT";
    case MYLITE_SQL_AST_TIME_FORMAT_ARGUMENT_COUNT_ERROR:
        return "TIME_FORMAT";
    case MYLITE_SQL_AST_STR_TO_DATE_ARGUMENT_COUNT_ERROR:
        return "STR_TO_DATE";
    case MYLITE_SQL_AST_DATEDIFF_ARGUMENT_COUNT_ERROR:
        return "DATEDIFF";
    case MYLITE_SQL_AST_TIMEDIFF_ARGUMENT_COUNT_ERROR:
        return "TIMEDIFF";
    case MYLITE_SQL_AST_TIMESTAMP_ARGUMENT_COUNT_ERROR:
        return "TIMESTAMP";
    case MYLITE_SQL_AST_UNIX_TIMESTAMP_ARGUMENT_COUNT_ERROR:
        return "UNIX_TIMESTAMP";
    case MYLITE_SQL_AST_TIME_TO_SEC_ARGUMENT_COUNT_ERROR:
        return "TIME_TO_SEC";
    case MYLITE_SQL_AST_TO_DAYS_ARGUMENT_COUNT_ERROR:
        return "TO_DAYS";
    case MYLITE_SQL_AST_TO_SECONDS_ARGUMENT_COUNT_ERROR:
        return "TO_SECONDS";
    case MYLITE_SQL_AST_SEC_TO_TIME_ARGUMENT_COUNT_ERROR:
        return "SEC_TO_TIME";
    case MYLITE_SQL_AST_FROM_UNIXTIME_ARGUMENT_COUNT_ERROR:
        return "FROM_UNIXTIME";
    case MYLITE_SQL_AST_FROM_DAYS_ARGUMENT_COUNT_ERROR:
        return "FROM_DAYS";
    case MYLITE_SQL_AST_MAKEDATE_ARGUMENT_COUNT_ERROR:
        return "MAKEDATE";
    case MYLITE_SQL_AST_MAKETIME_ARGUMENT_COUNT_ERROR:
        return "MAKETIME";
    case MYLITE_SQL_AST_DAYNAME_ARGUMENT_COUNT_ERROR:
        return "DAYNAME";
    case MYLITE_SQL_AST_DAYOFMONTH_ARGUMENT_COUNT_ERROR:
        return "DAYOFMONTH";
    case MYLITE_SQL_AST_DAYOFWEEK_ARGUMENT_COUNT_ERROR:
        return "DAYOFWEEK";
    case MYLITE_SQL_AST_DAYOFYEAR_ARGUMENT_COUNT_ERROR:
        return "DAYOFYEAR";
    case MYLITE_SQL_AST_LAST_DAY_ARGUMENT_COUNT_ERROR:
        return "LAST_DAY";
    case MYLITE_SQL_AST_MONTHNAME_ARGUMENT_COUNT_ERROR:
        return "MONTHNAME";
    case MYLITE_SQL_AST_WEEKDAY_ARGUMENT_COUNT_ERROR:
        return "WEEKDAY";
    case MYLITE_SQL_AST_WEEKOFYEAR_ARGUMENT_COUNT_ERROR:
        return "WEEKOFYEAR";
    case MYLITE_SQL_AST_YEARWEEK_ARGUMENT_COUNT_ERROR:
        return "YEARWEEK";
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP_ARGUMENT_COUNT_ERROR:
        return "CURRENT_TIMESTAMP";
    case MYLITE_SQL_AST_SYSDATE_ARGUMENT_COUNT_ERROR:
        return "SYSDATE";
    case MYLITE_SQL_AST_BIT_COUNT_ARGUMENT_COUNT_ERROR:
        return "BIT_COUNT";
    case MYLITE_SQL_AST_CRC32_ARGUMENT_COUNT_ERROR:
        return "CRC32";
    case MYLITE_SQL_AST_HEX_ARGUMENT_COUNT_ERROR:
        return "HEX";
    case MYLITE_SQL_AST_TO_BASE64_ARGUMENT_COUNT_ERROR:
        return "TO_BASE64";
    case MYLITE_SQL_AST_FROM_BASE64_ARGUMENT_COUNT_ERROR:
        return "FROM_BASE64";
    case MYLITE_SQL_AST_UNHEX_ARGUMENT_COUNT_ERROR:
        return "UNHEX";
    case MYLITE_SQL_AST_UUID_ARGUMENT_COUNT_ERROR:
        return "UUID";
    case MYLITE_SQL_AST_IS_UUID_ARGUMENT_COUNT_ERROR:
        return "IS_UUID";
    case MYLITE_SQL_AST_UUID_TO_BIN_ARGUMENT_COUNT_ERROR:
        return "UUID_TO_BIN";
    case MYLITE_SQL_AST_BIN_TO_UUID_ARGUMENT_COUNT_ERROR:
        return "BIN_TO_UUID";
    case MYLITE_SQL_AST_QUOTE_ARGUMENT_COUNT_ERROR:
        return "QUOTE";
    case MYLITE_SQL_AST_FORMAT_ARGUMENT_COUNT_ERROR:
        return "FORMAT";
    case MYLITE_SQL_AST_TRUNCATE_ARGUMENT_COUNT_ERROR:
        return "TRUNCATE";
    case MYLITE_SQL_AST_LENGTH_ARGUMENT_COUNT_ERROR:
        return "LENGTH";
    case MYLITE_SQL_AST_OCTET_LENGTH_ARGUMENT_COUNT_ERROR:
        return "OCTET_LENGTH";
    case MYLITE_SQL_AST_BIT_LENGTH_ARGUMENT_COUNT_ERROR:
        return "BIT_LENGTH";
    case MYLITE_SQL_AST_CHAR_LENGTH_ARGUMENT_COUNT_ERROR:
        return "CHAR_LENGTH";
    case MYLITE_SQL_AST_CHARACTER_LENGTH_ARGUMENT_COUNT_ERROR:
        return "CHARACTER_LENGTH";
    case MYLITE_SQL_AST_LOWER_ARGUMENT_COUNT_ERROR:
        return "LOWER";
    case MYLITE_SQL_AST_LCASE_ARGUMENT_COUNT_ERROR:
        return "LCASE";
    case MYLITE_SQL_AST_UPPER_ARGUMENT_COUNT_ERROR:
        return "UPPER";
    case MYLITE_SQL_AST_UCASE_ARGUMENT_COUNT_ERROR:
        return "UCASE";
    case MYLITE_SQL_AST_LTRIM_ARGUMENT_COUNT_ERROR:
        return "LTRIM";
    case MYLITE_SQL_AST_RTRIM_ARGUMENT_COUNT_ERROR:
        return "RTRIM";
    case MYLITE_SQL_AST_LOCATE_ARGUMENT_COUNT_ERROR:
        return "LOCATE";
    case MYLITE_SQL_AST_INSTR_ARGUMENT_COUNT_ERROR:
        return "INSTR";
    default:
        break;
    }
    return NULL;
}
