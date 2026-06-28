static int plan_update_assignment(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment_list,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_update *out_plan
) {
    const struct mylite_sql_ast_node *assignment = child_at(assignment_list, 0U);
    const struct mylite_sql_ast_node *target = NULL;
    char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t column_index = 0U;
    int rc = MYLITE_OK;

    if (assignment_list == NULL || assignment_list->kind != MYLITE_SQL_AST_UPDATE_ASSIGNMENT_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_child_count(assignment_list) == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_child_count(assignment_list) != 1U) {
        rc = plan_update_multiple_assignments(
            database,
            assignment_list,
            table_columns,
            table_column_count,
            out_plan
        );
        if (rc == MYLITE_OK) {
            rc = validate_update_multiple_auto_update_columns(
                database,
                out_plan,
                table_columns,
                table_column_count
            );
        }
        return rc;
    }
    if (assignment == NULL || assignment->kind != MYLITE_SQL_AST_UPDATE_ASSIGNMENT) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    target = child_at(assignment, 0U);
    if (target == NULL || target->kind != MYLITE_SQL_AST_IDENTIFIER) {
        set_unsupported_error(database, "UPDATE supports only unqualified assignment columns");
        return MYLITE_ERROR;
    }
    rc = copy_identifier_text(target, column_name, sizeof(column_name), database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = find_column_index(table_columns, table_column_count, column_name, &column_index);
    if (rc != MYLITE_OK) {
        set_unknown_column_error(database, column_name);
        return MYLITE_ERROR;
    }

    out_plan->assignment_column = table_columns[column_index];
    out_plan->assignment_value_node = child_at(assignment, 1U);
    if (out_plan->assignment_value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (out_plan->assignment_column.is_generated) {
        const struct mylite_sql_ast_node *value_node =
            unwrap_parenthesized_expression(out_plan->assignment_value_node);

        if (value_node != NULL && value_node->kind == MYLITE_SQL_AST_DML_DEFAULT_VALUE) {
            out_plan->generated_default_noop = true;
            return MYLITE_OK;
        }
        set_generated_column_value_error(
            database,
            out_plan->assignment_column.name,
            out_plan->table.name
        );
        return MYLITE_ERROR;
    }
    rc =
        plan_update_date_interval_assignment(database, table_columns, table_column_count, out_plan);
    if (rc != MYLITE_OK ||
        out_plan->assignment_value_kind == PLANNED_UPDATE_ASSIGNMENT_DATE_INTERVAL) {
        return rc;
    }
    rc = plan_update_arithmetic_assignment(database, table_columns, table_column_count, out_plan);
    if (rc != MYLITE_OK ||
        out_plan->assignment_value_kind == PLANNED_UPDATE_ASSIGNMENT_SAME_COLUMN_ARITHMETIC) {
        return rc;
    }
    rc = plan_update_column_copy_assignment(database, table_columns, table_column_count, out_plan);
    if (rc != MYLITE_OK ||
        out_plan->assignment_value_kind == PLANNED_UPDATE_ASSIGNMENT_COLUMN_COPY) {
        return rc;
    }
    rc = plan_update_row_scalar_assignment(database, table_columns, table_column_count, out_plan);
    if (rc != MYLITE_OK ||
        out_plan->assignment_value_kind == PLANNED_UPDATE_ASSIGNMENT_ROW_SCALAR) {
        return rc;
    }

    return plan_update_scalar_subquery_assignment(database, out_plan);
}

static int plan_update_column_copy_assignment(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_update *out_plan
) {
    const struct mylite_sql_ast_node *value_node = NULL;
    char source_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t source_index = 0U;
    size_t part_count = 0U;
    int rc = MYLITE_OK;

    if (out_plan == NULL) {
        return MYLITE_MISUSE;
    }
    value_node = unwrap_parenthesized_expression(out_plan->assignment_value_node);
    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_IDENTIFIER) {
        return MYLITE_OK;
    }

    rc = collect_identifier_parts(
        value_node,
        parts,
        table_name_part_capacity,
        &part_count,
        database
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (part_count != 1U) {
        set_unsupported_error(
            database,
            "UPDATE column-copy assignment supports only unqualified source columns"
        );
        return MYLITE_ERROR;
    }
    rc = copy_identifier_text(value_node, source_name, sizeof(source_name), database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = find_column_index(table_columns, table_column_count, source_name, &source_index);
    if (rc != MYLITE_OK) {
        set_unknown_column_error(database, source_name);
        return MYLITE_ERROR;
    }
    if (!update_column_copy_source_target_types_are_compatible(
            &table_columns[source_index],
            &out_plan->assignment_column
        )) {
        set_unsupported_error(
            database,
            update_scalar_subquery_implicit_conversion_message(&out_plan->assignment_column)
        );
        return MYLITE_ERROR;
    }

    out_plan->assignment_source_column = table_columns[source_index];
    out_plan->assignment_value_kind = PLANNED_UPDATE_ASSIGNMENT_COLUMN_COPY;
    return MYLITE_OK;
}

static bool update_column_copy_source_target_types_are_compatible(
    const struct mylite_catalog_column_descriptor *source_column,
    const struct mylite_catalog_column_descriptor *target_column
) {
    if (source_column == NULL || target_column == NULL) {
        return false;
    }
    if (column_descriptor_is_approximate(target_column) &&
        strcmp(source_column->physical_type, "INTEGER") == 0) {
        return true;
    }
    if (column_descriptor_is_decimal(target_column) &&
        strcmp(source_column->physical_type, "INTEGER") == 0) {
        return true;
    }
    return update_scalar_subquery_source_target_types_are_compatible(source_column, target_column);
}

static int plan_update_row_scalar_assignment(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_update *out_plan
) {
    const struct mylite_sql_ast_node *value_node = NULL;
    bool planned = false;
    int rc = MYLITE_OK;

    if (out_plan == NULL) {
        return MYLITE_MISUSE;
    }
    value_node = unwrap_parenthesized_expression(out_plan->assignment_value_node);
    rc = plan_update_assignment_row_scalar_expression(
        database,
        value_node,
        &out_plan->assignment_column,
        table_column_count,
        table_columns,
        out_plan,
        &out_plan->assignment_row_scalar_expression,
        &planned
    );
    if (rc != MYLITE_OK || !planned) {
        return rc;
    }

    out_plan->assignment_value_kind = PLANNED_UPDATE_ASSIGNMENT_ROW_SCALAR;
    return MYLITE_OK;
}

static int plan_update_assignment_row_scalar_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t table_column_count,
    const struct mylite_catalog_column_descriptor *table_columns,
    struct planned_update *plan,
    struct planned_row_scalar_expression *out_expression,
    bool *out_planned
) {
    struct select_source_context source_context = {0};
    bool arithmetic_attempt = false;
    int rc = MYLITE_OK;

    if (plan == NULL || target_column == NULL || out_expression == NULL || out_planned == NULL) {
        return MYLITE_MISUSE;
    }
    *out_planned = false;
    value_node = unwrap_parenthesized_expression(value_node);
    if (insert_value_is_unix_timestamp_arithmetic(value_node)) {
        return MYLITE_OK;
    }
    arithmetic_attempt = row_scalar_expression_contains_integer_arithmetic_attempt(value_node);
    if (!row_scalar_expression_is_dml_assignment_context_attempt(value_node, false)) {
        return MYLITE_OK;
    }
    if (arithmetic_attempt) {
        struct integer_column_range range = {0};
        bool is_constant_arithmetic = false;

        rc = update_value_is_constant_arithmetic_expression(
            database,
            value_node,
            &is_constant_arithmetic
        );
        if (rc != MYLITE_OK || is_constant_arithmetic) {
            return rc;
        }

        rc = integer_range_for_column(
            database,
            target_column,
            "UPDATE arithmetic assignment supports only integer columns",
            &range
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    if (column_descriptor_is_auto_increment(target_column)) {
        set_unsupported_error(
            database,
            "UPDATE row-scalar assignment does not support AUTO_INCREMENT targets"
        );
        return MYLITE_ERROR;
    }
    if (update_column_id_is_keyed(plan, target_column->column_id)) {
        set_unsupported_error(
            database,
            "UPDATE row-scalar assignment does not support indexed targets"
        );
        return MYLITE_ERROR;
    }

    source_context.source = &plan->target;
    rc = plan_row_scalar_expression(
        database,
        value_node,
        true,
        &source_context,
        &plan->table,
        table_columns,
        table_column_count,
        out_expression
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    *out_planned = true;
    return MYLITE_OK;
}

static int plan_update_multiple_assignments(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment_list,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_update *out_plan
) {
    size_t assignment_count = mylite_sql_ast_node_child_count(assignment_list);
    int rc = MYLITE_OK;

    out_plan->assignments = calloc(assignment_count, sizeof(*out_plan->assignments));
    if (out_plan->assignments == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_plan->assignment_count = assignment_count;

    for (size_t assignment_index = 0U; rc == MYLITE_OK && assignment_index < assignment_count;
         ++assignment_index) {
        rc = plan_update_multiple_assignment(
            database,
            child_at(assignment_list, assignment_index),
            table_columns,
            table_column_count,
            out_plan,
            assignment_index
        );
    }
    if (rc == MYLITE_OK && planned_update_executable_assignment_count(out_plan) == 0U) {
        out_plan->generated_default_noop = true;
    }

    return rc;
}

static int plan_update_multiple_assignment(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *assignment,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_update *out_plan,
    size_t assignment_index
) {
    const struct mylite_sql_ast_node *target = NULL;
    const struct mylite_sql_ast_node *value_node = NULL;
    char column_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t column_index = 0U;
    int rc = MYLITE_OK;

    if (assignment == NULL || assignment->kind != MYLITE_SQL_AST_UPDATE_ASSIGNMENT) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    target = child_at(assignment, 0U);
    if (target == NULL || target->kind != MYLITE_SQL_AST_IDENTIFIER) {
        set_update_multiple_assignment_unsupported_error(database);
        return MYLITE_ERROR;
    }
    rc = copy_identifier_text(target, column_name, sizeof(column_name), database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = find_column_index(table_columns, table_column_count, column_name, &column_index);
    if (rc != MYLITE_OK) {
        set_unknown_column_error(database, column_name);
        return MYLITE_ERROR;
    }
    for (size_t index = 0U; index < assignment_index; ++index) {
        if (out_plan->assignments[index].column.column_id ==
            table_columns[column_index].column_id) {
            set_update_duplicate_assignment_unsupported_error(database);
            return MYLITE_ERROR;
        }
    }
    if (table_columns[column_index].is_generated) {
        value_node = unwrap_parenthesized_expression(child_at(assignment, 1U));
        if (value_node != NULL && value_node->kind == MYLITE_SQL_AST_DML_DEFAULT_VALUE) {
            out_plan->assignments[assignment_index].column = table_columns[column_index];
            out_plan->assignments[assignment_index].value_node = child_at(assignment, 1U);
            out_plan->assignments[assignment_index].generated_default_noop = true;
            return MYLITE_OK;
        }
        set_generated_column_value_error(
            database,
            table_columns[column_index].name,
            out_plan->table.name
        );
        return MYLITE_ERROR;
    }
    value_node = child_at(assignment, 1U);

    out_plan->assignments[assignment_index].column = table_columns[column_index];
    out_plan->assignments[assignment_index].value_node = value_node;
    if (!update_assignment_value_is_multi_constant_supported(value_node)) {
        bool planned = false;

        rc = plan_update_assignment_row_scalar_expression(
            database,
            value_node,
            &out_plan->assignments[assignment_index].column,
            table_column_count,
            table_columns,
            out_plan,
            &out_plan->assignments[assignment_index].row_scalar_expression,
            &planned
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (planned) {
            out_plan->assignments[assignment_index].value_kind =
                PLANNED_UPDATE_ASSIGNMENT_ROW_SCALAR;
            return MYLITE_OK;
        }
        set_update_multiple_assignment_unsupported_error(database);
        return MYLITE_ERROR;
    }

    out_plan->assignments[assignment_index].value_kind = PLANNED_UPDATE_ASSIGNMENT_VALUE;

    return MYLITE_OK;
}

static int plan_update_date_interval_assignment(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_update *out_plan
) {
    const struct mylite_sql_ast_node *value_node = NULL;
    const char *function_name = NULL;
    int rc = MYLITE_OK;

    if (out_plan == NULL) {
        return MYLITE_MISUSE;
    }
    value_node = unwrap_parenthesized_expression(out_plan->assignment_value_node);
    if (value_node == NULL ||
        !mylite_execution_is_date_interval_second_function_kind(value_node->kind) ||
        value_node->kind == MYLITE_SQL_AST_TIMESTAMPADD_FUNCTION) {
        return MYLITE_OK;
    }

    function_name = mylite_execution_date_interval_second_function_name(value_node->kind);
    rc = mylite_execution_validate_date_interval_second_function_shape(
        database,
        value_node,
        function_name
    );
    if (rc == MYLITE_OK) {
        rc = plan_update_date_interval_source_column(
            database,
            mylite_execution_date_interval_second_temporal_node(value_node),
            table_columns,
            table_column_count,
            &out_plan->assignment_column
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_date_interval_unit_from_ast(
            database,
            mylite_execution_date_interval_unit_node(value_node),
            function_name,
            &out_plan->date_interval_unit
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_date_interval_second_interval_argument(
            database,
            function_name,
            mylite_execution_date_interval_second_interval_node(value_node),
            out_plan->date_interval_unit,
            &out_plan->date_interval_value,
            &out_plan->date_interval_is_null
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_update_date_interval_target(database, out_plan);
    }
    if (rc == MYLITE_OK) {
        out_plan->assignment_value_kind = PLANNED_UPDATE_ASSIGNMENT_DATE_INTERVAL;
        out_plan->date_interval_subtract =
            mylite_execution_date_interval_second_function_subtracts(value_node->kind);
    }
    return rc;
}

static int plan_update_date_interval_source_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *source_node,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct mylite_catalog_column_descriptor *assignment_column
) {
    char source_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t source_index = 0U;
    int rc = MYLITE_OK;

    source_node = unwrap_parenthesized_expression(source_node);
    if (source_node == NULL || source_node->kind != MYLITE_SQL_AST_IDENTIFIER) {
        set_update_date_interval_column_mismatch_error(database);
        return MYLITE_ERROR;
    }
    rc = copy_identifier_text(source_node, source_name, sizeof(source_name), database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = find_column_index(table_columns, table_column_count, source_name, &source_index);
    if (rc != MYLITE_OK) {
        set_unknown_column_error(database, source_name);
        return MYLITE_ERROR;
    }
    if (assignment_column == NULL ||
        table_columns[source_index].column_id != assignment_column->column_id) {
        set_update_date_interval_column_mismatch_error(database);
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int plan_update_date_interval_target(
    struct mylite_db *database,
    struct planned_update *plan
) {
    int rc = MYLITE_OK;

    if (plan == NULL) {
        return MYLITE_MISUSE;
    }
    if (update_assignment_column_is_keyed(plan)) {
        set_update_date_interval_keyed_target_error(database);
        return MYLITE_ERROR;
    }
    if (!update_date_interval_column_family_is_supported(&plan->assignment_column)) {
        set_update_date_interval_target_error(database);
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_date(&plan->assignment_column) &&
        mylite_date_interval_unit_has_time_part(plan->date_interval_unit)) {
        set_update_date_interval_date_time_unit_error(database);
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_char(&plan->assignment_column) ||
        column_descriptor_is_varchar(&plan->assignment_column)) {
        rc = validate_update_date_interval_string_target(database, &plan->assignment_column);
    }
    if (rc == MYLITE_OK) {
        rc = update_date_interval_input_kind_for_column(
            database,
            &plan->assignment_column,
            &plan->date_interval_input_kind
        );
    }
    return rc;
}

static int update_date_interval_input_kind_for_column(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    enum mylite_date_interval_second_input_kind *out_kind
) {
    if (out_kind == NULL) {
        return MYLITE_MISUSE;
    }
    *out_kind = MYLITE_DATE_INTERVAL_SECOND_INPUT_STRING;
    if (column_descriptor_is_date(column)) {
        *out_kind = MYLITE_DATE_INTERVAL_SECOND_INPUT_DATE;
        return MYLITE_OK;
    }
    if (column_descriptor_is_datetime(column)) {
        *out_kind = MYLITE_DATE_INTERVAL_SECOND_INPUT_DATETIME;
        return MYLITE_OK;
    }
    if (column_descriptor_is_string_family(column)) {
        *out_kind = MYLITE_DATE_INTERVAL_SECOND_INPUT_STRING;
        return MYLITE_OK;
    }
    set_update_date_interval_target_error(database);
    return MYLITE_ERROR;
}

static bool update_date_interval_column_family_is_supported(
    const struct mylite_catalog_column_descriptor *column
) {
    return (column_descriptor_is_date(column) || column_descriptor_is_datetime(column) ||
            column_descriptor_is_string_family(column)) != 0;
}

static int validate_update_date_interval_string_target(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
) {
    size_t declared_length = 0U;
    int rc = MYLITE_OK;

    if (column_descriptor_is_char(column)) {
        rc = char_length_for_column(database, column, &declared_length);
    } else if (column_descriptor_is_varchar(column)) {
        rc = varchar_length_for_column(database, column, &declared_length);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (declared_length < datetime_text_length) {
        set_update_date_interval_target_error(database);
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static void set_update_date_interval_column_mismatch_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "UPDATE DATE interval assignment supports only the assigned column as input"
    );
}

static void set_update_date_interval_target_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "UPDATE DATE interval assignment supports only DATE, DATETIME, and nonbinary string targets"
    );
}

static void set_update_date_interval_date_time_unit_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "UPDATE DATE interval assignment does not yet support time units for DATE targets"
    );
}

static void set_update_date_interval_keyed_target_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "UPDATE DATE interval assignment does not yet support key columns"
    );
}

static int plan_update_arithmetic_assignment(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_update *out_plan
) {
    const struct mylite_sql_ast_node *value_node = NULL;
    const struct mylite_sql_ast_node *source = NULL;
    const struct mylite_sql_ast_node *delta = NULL;
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;
    char source_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    size_t source_index = 0U;
    bool is_constant_arithmetic = false;
    int rc = MYLITE_OK;

    value_node = unwrap_parenthesized_expression(out_plan->assignment_value_node);
    if (value_node == NULL) {
        return MYLITE_OK;
    }
    if (insert_value_is_unix_timestamp_arithmetic(value_node)) {
        return validate_update_unix_timestamp_arithmetic_assignment_target(
            database,
            &out_plan->assignment_column
        );
    }
    if (value_node->kind != MYLITE_SQL_AST_BINARY_EXPRESSION) {
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
        return MYLITE_OK;
    }

    operator_kind = mylite_sql_ast_node_operator(value_node);
    if (operator_kind != MYLITE_SQL_AST_OPERATOR_ADD &&
        operator_kind != MYLITE_SQL_AST_OPERATOR_SUBTRACT) {
        return MYLITE_OK;
    }

    source = child_at(value_node, 0U);
    delta = child_at(value_node, 1U);
    if (source == NULL || source->kind != MYLITE_SQL_AST_IDENTIFIER || delta == NULL ||
        delta->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(delta) != MYLITE_SQL_AST_LITERAL_INTEGER) {
        return MYLITE_OK;
    }

    rc = copy_identifier_text(source, source_name, sizeof(source_name), database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = find_column_index(table_columns, table_column_count, source_name, &source_index);
    if (rc != MYLITE_OK) {
        set_unknown_column_error(database, source_name);
        return MYLITE_ERROR;
    }
    if (table_columns[source_index].column_id != out_plan->assignment_column.column_id) {
        return MYLITE_OK;
    }

    out_plan->assignment_value_kind = PLANNED_UPDATE_ASSIGNMENT_SAME_COLUMN_ARITHMETIC;
    out_plan->arithmetic_operator = operator_kind;
    out_plan->arithmetic_delta_node = delta;

    return validate_update_arithmetic_assignment_target(database, out_plan);
}

static int update_value_is_constant_arithmetic_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    bool *out_is_constant
) {
    struct scalar_arithmetic_node_stack stack = {0};
    const struct mylite_sql_ast_node *current = NULL;
    bool admitted = true;
    int rc = MYLITE_OK;

    if (out_is_constant == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    *out_is_constant = false;

    value_node = unwrap_parenthesized_expression(value_node);
    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_BINARY_EXPRESSION) {
        return MYLITE_OK;
    }
    if (!scalar_arithmetic_node_stack_push(&stack, value_node)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    while (stack.count != 0U && admitted) {
        --stack.count;
        current = unwrap_parenthesized_expression(stack.items[stack.count]);
        rc = update_constant_arithmetic_visit_node(database, &stack, current, &admitted);
        if (rc != MYLITE_OK) {
            goto done;
        }
    }

    *out_is_constant = admitted;

done:
    scalar_arithmetic_node_stack_deinit(&stack);
    return rc;
}

static int update_constant_arithmetic_visit_node(
    struct mylite_db *database,
    struct scalar_arithmetic_node_stack *stack,
    const struct mylite_sql_ast_node *value_node,
    bool *in_out_admitted
) {
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;
    int rc = MYLITE_OK;

    if (value_node == NULL) {
        *in_out_admitted = false;
        return MYLITE_OK;
    }

    switch (value_node->kind) {
    case MYLITE_SQL_AST_LITERAL:
        *in_out_admitted = update_constant_arithmetic_literal_is_admitted(
            mylite_sql_ast_node_literal_kind(value_node)
        );
        return MYLITE_OK;
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
        operator_kind = mylite_sql_ast_node_operator(value_node);
        if (!update_constant_arithmetic_unary_operator_is_admitted(operator_kind)) {
            *in_out_admitted = false;
            return MYLITE_OK;
        }
        return update_constant_arithmetic_stack_push(database, stack, child_at(value_node, 0U));
    case MYLITE_SQL_AST_BINARY_EXPRESSION:
        operator_kind = mylite_sql_ast_node_operator(value_node);
        if (!update_constant_arithmetic_binary_operator_is_admitted(operator_kind)) {
            *in_out_admitted = false;
            return MYLITE_OK;
        }
        rc = update_constant_arithmetic_stack_push(database, stack, child_at(value_node, 0U));
        if (rc != MYLITE_OK) {
            return rc;
        }
        return update_constant_arithmetic_stack_push(database, stack, child_at(value_node, 1U));
    default:
        *in_out_admitted = false;
        return MYLITE_OK;
    }
}

static bool update_constant_arithmetic_literal_is_admitted(
    enum mylite_sql_ast_literal_kind literal_kind
) {
    switch (literal_kind) {
    case MYLITE_SQL_AST_LITERAL_INTEGER:
    case MYLITE_SQL_AST_LITERAL_TRUE:
    case MYLITE_SQL_AST_LITERAL_FALSE:
    case MYLITE_SQL_AST_LITERAL_NULL:
        return true;
    default:
        return false;
    }
}

static bool update_constant_arithmetic_unary_operator_is_admitted(
    enum mylite_sql_ast_operator operator_kind
) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_POSITIVE:
    case MYLITE_SQL_AST_OPERATOR_NEGATIVE:
        return true;
    default:
        return false;
    }
}

static bool update_constant_arithmetic_binary_operator_is_admitted(
    enum mylite_sql_ast_operator operator_kind
) {
    switch (operator_kind) {
    case MYLITE_SQL_AST_OPERATOR_ADD:
    case MYLITE_SQL_AST_OPERATOR_SUBTRACT:
    case MYLITE_SQL_AST_OPERATOR_MULTIPLY:
        return true;
    default:
        return false;
    }
}

static int update_constant_arithmetic_stack_push(
    struct mylite_db *database,
    struct scalar_arithmetic_node_stack *stack,
    const struct mylite_sql_ast_node *value_node
) {
    if (!scalar_arithmetic_node_stack_push(stack, value_node)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    return MYLITE_OK;
}

static int validate_update_arithmetic_assignment_target(
    struct mylite_db *database,
    const struct planned_update *plan
) {
    struct integer_column_range range = {0};
    int rc = integer_range_for_column(
        database,
        &plan->assignment_column,
        "UPDATE arithmetic assignment supports only integer columns",
        &range
    );

    (void)range;
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (column_descriptor_is_auto_increment(&plan->assignment_column) ||
        update_assignment_column_is_keyed(plan)) {
        set_update_arithmetic_unsupported_error(database);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_update_unix_timestamp_arithmetic_assignment_target(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
) {
    struct integer_column_range range = {0};
    int rc = MYLITE_OK;

    if (column_descriptor_is_auto_increment(column)) {
        set_unsupported_error(
            database,
            "UPDATE UNIX_TIMESTAMP arithmetic does not yet support AUTO_INCREMENT targets"
        );
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_string_family(column)) {
        return MYLITE_OK;
    }

    rc = integer_range_for_column(
        database,
        column,
        "UPDATE UNIX_TIMESTAMP arithmetic supports only integer and nonbinary string targets",
        &range
    );
    (void)range;
    return rc;
}

static bool update_assignment_column_is_keyed(const struct planned_update *plan) {
    if (plan == NULL) {
        return false;
    }
    return update_column_id_is_keyed(plan, plan->assignment_column.column_id);
}

static bool update_column_id_is_keyed(const struct planned_update *plan, int64_t column_id) {
    if (plan == NULL) {
        return false;
    }
    for (size_t index = 0U; index < plan->index_count; ++index) {
        const struct loaded_index_info *key = &plan->indexes[index];

        if ((key->index.kind == MYLITE_CATALOG_INDEX_KIND_PRIMARY || key->index.is_unique) &&
            loaded_index_contains_column_id(key, column_id)) {
            return true;
        }
    }

    return false;
}

static const struct planned_update_assignment *planned_update_assignment_for_column(
    const struct planned_update *plan,
    int64_t column_id
) {
    if (plan == NULL || !planned_update_has_multiple_assignments(plan)) {
        return NULL;
    }
    for (size_t index = 0U; index < plan->assignment_count; ++index) {
        const struct planned_update_assignment *assignment = &plan->assignments[index];

        if (planned_update_assignment_is_noop(assignment)) {
            continue;
        }
        if (assignment->column.column_id == column_id) {
            return assignment;
        }
    }

    return NULL;
}

static bool planned_update_assigns_auto_increment(const struct planned_update *plan) {
    if (plan == NULL || plan->generated_default_noop) {
        return false;
    }
    if (planned_update_has_multiple_assignments(plan)) {
        return planned_update_auto_increment_assignment(plan) != NULL;
    }
    if (plan->assignment_value_kind == PLANNED_UPDATE_ASSIGNMENT_COLUMN_COPY) {
        return false;
    }
    if (plan->assignment_value_kind == PLANNED_UPDATE_ASSIGNMENT_ROW_SCALAR) {
        return false;
    }
    return column_descriptor_is_auto_increment(&plan->assignment_column);
}

static const struct planned_update_assignment *planned_update_auto_increment_assignment(
    const struct planned_update *plan
) {
    if (plan == NULL || !planned_update_has_multiple_assignments(plan)) {
        return NULL;
    }
    for (size_t index = 0U; index < plan->assignment_count; ++index) {
        const struct planned_update_assignment *assignment = &plan->assignments[index];

        if (planned_update_assignment_is_noop(assignment)) {
            continue;
        }
        if (column_descriptor_is_auto_increment(&assignment->column)) {
            return assignment;
        }
    }

    return NULL;
}

static int validate_update_ignore_assignment_support(
    struct mylite_db *database,
    const struct planned_update *plan
) {
    if (plan == NULL || !plan->ignore_errors || plan->generated_default_noop) {
        return MYLITE_OK;
    }
    if (planned_update_has_multiple_assignments(plan)) {
        for (size_t index = 0U; index < plan->assignment_count; ++index) {
            const struct planned_update_assignment *assignment = &plan->assignments[index];

            if (planned_update_assignment_is_noop(assignment)) {
                continue;
            }
            if (column_descriptor_is_auto_increment(&assignment->column) ||
                update_column_id_is_keyed(plan, assignment->column.column_id)) {
                set_unsupported_error(
                    database,
                    "UPDATE IGNORE does not yet support key assignment conflict demotion"
                );
                return MYLITE_ERROR;
            }
        }
        return MYLITE_OK;
    }
    if (plan->assignment_value_kind == PLANNED_UPDATE_ASSIGNMENT_DATE_INTERVAL) {
        set_unsupported_error(
            database,
            "UPDATE IGNORE does not yet support DATE interval assignments"
        );
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_auto_increment(&plan->assignment_column) ||
        update_assignment_column_is_keyed(plan)) {
        set_unsupported_error(
            database,
            "UPDATE IGNORE does not yet support key assignment conflict demotion"
        );
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static bool update_assignment_value_is_multi_constant_supported(
    const struct mylite_sql_ast_node *value_node
) {
    value_node = unwrap_parenthesized_expression(value_node);
    if (value_node == NULL) {
        return false;
    }

    switch (value_node->kind) {
    case MYLITE_SQL_AST_LITERAL:
    case MYLITE_SQL_AST_DML_DEFAULT_VALUE:
    case MYLITE_SQL_AST_DEFAULT_FUNCTION:
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP_VALUE:
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_SYSDATE_FUNCTION:
    case MYLITE_SQL_AST_SYSDATE_ARGUMENT_COUNT_ERROR:
    case MYLITE_SQL_AST_CURRENT_DATE_VALUE:
    case MYLITE_SQL_AST_CURRENT_TIME_VALUE:
    case MYLITE_SQL_AST_UTC_DATE_VALUE:
    case MYLITE_SQL_AST_UTC_TIME_VALUE:
    case MYLITE_SQL_AST_UTC_TIMESTAMP_VALUE:
        return true;
    case MYLITE_SQL_AST_UNARY_EXPRESSION:
        return update_assignment_unary_value_is_multi_constant_supported(value_node);
    default:
        return dml_value_is_constant_scalar_expression(value_node, false);
    }
}

static bool update_assignment_unary_value_is_multi_constant_supported(
    const struct mylite_sql_ast_node *value_node
) {
    const struct mylite_sql_ast_node *operand = NULL;
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NULL;

    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_UNARY_EXPRESSION) {
        return false;
    }
    operator_kind = mylite_sql_ast_node_operator(value_node);
    if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE &&
        operator_kind != MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
        return false;
    }

    operand = child_at(value_node, 0U);
    if (operand == NULL || operand->kind != MYLITE_SQL_AST_LITERAL) {
        return false;
    }
    literal_kind = mylite_sql_ast_node_literal_kind(operand);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER ||
        literal_kind == MYLITE_SQL_AST_LITERAL_DECIMAL ||
        literal_kind == MYLITE_SQL_AST_LITERAL_FLOAT) {
        return true;
    }
    return false;
}

static int validate_update_multiple_auto_update_columns(
    struct mylite_db *database,
    const struct planned_update *plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count
) {
    for (size_t column_index = 0U; column_index < table_column_count; ++column_index) {
        if (planned_update_column_has_auto_update(plan, &table_columns[column_index]) &&
            update_column_id_is_keyed(plan, table_columns[column_index].column_id)) {
            set_update_multiple_assignment_unsupported_error(database);
            return MYLITE_ERROR;
        }
    }

    return MYLITE_OK;
}

static void set_update_multiple_assignment_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "UPDATE multiple assignments support only distinct unqualified constant assignments in "
        "the current target-column subset"
    );
}

static void set_update_duplicate_assignment_unsupported_error(struct mylite_db *database) {
    set_unsupported_error(database, "UPDATE multiple assignments do not support duplicate targets");
}

static int validate_update_default_function_sources(
    struct mylite_db *database,
    const struct planned_update *plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count
) {
    if (plan == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (planned_update_has_multiple_assignments(plan)) {
        int rc = MYLITE_OK;

        for (size_t index = 0U; rc == MYLITE_OK && index < plan->assignment_count; ++index) {
            rc = validate_update_default_function_source(
                database,
                plan->assignments[index].value_node,
                plan,
                table_columns,
                table_column_count
            );
        }
        return rc;
    }

    return validate_update_default_function_source(
        database,
        plan->assignment_value_node,
        plan,
        table_columns,
        table_column_count
    );
}

static int validate_update_default_function_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct planned_update *plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count
) {
    struct select_source_context source_context = {0};

    value_node = unwrap_parenthesized_expression(value_node);
    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_DEFAULT_FUNCTION) {
        return MYLITE_OK;
    }

    source_context.source = &plan->target;
    return validate_default_function_source_value(
        database,
        value_node,
        &source_context,
        table_columns,
        table_column_count
    );
}

static int plan_update_scalar_subquery_assignment(
    struct mylite_db *database,
    struct planned_update *out_plan
) {
    const struct mylite_sql_ast_node *value_node = NULL;
    const struct mylite_sql_ast_node *statement = NULL;
    bool handled = false;
    int rc = MYLITE_OK;

    if (out_plan == NULL) {
        return MYLITE_MISUSE;
    }

    value_node = unwrap_parenthesized_expression(out_plan->assignment_value_node);
    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_SCALAR_SUBQUERY) {
        return MYLITE_OK;
    }
    if (mylite_sql_ast_node_child_count(value_node) != 1U) {
        set_unsupported_error(
            database,
            "UPDATE scalar subquery assignment supports only parenthesized SELECT"
        );
        return MYLITE_ERROR;
    }

    statement = child_at(value_node, 0U);
    rc = plan_update_scalar_aggregate_subquery_assignment(database, statement, out_plan, &handled);
    if (rc != MYLITE_OK || handled) {
        return rc;
    }
    rc = validate_update_scalar_subquery_select(database, statement);
    if (rc == MYLITE_OK) {
        rc = plan_select(database, statement, false, &out_plan->assignment_subquery);
    }
    if (rc == MYLITE_OK && out_plan->assignment_subquery.column_count != 1U) {
        set_scalar_subquery_column_count_error(database);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK &&
        out_plan->assignment_subquery.table.table_id == out_plan->table.table_id) {
        set_update_table_used_error(database, out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        out_plan->assignment_value_is_scalar_subquery = true;
    }

    return rc;
}

static int plan_update_scalar_aggregate_subquery_assignment(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_update *out_plan,
    bool *out_handled
) {
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *select_item = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    const struct mylite_sql_ast_node *aggregate_expression = NULL;
    const struct mylite_sql_ast_node *rhs_expression = NULL;
    enum planned_column_aggregate_function function = PLANNED_COLUMN_AGGREGATE_NONE;
    int rc = MYLITE_OK;

    if (out_handled == NULL) {
        return MYLITE_MISUSE;
    }
    *out_handled = false;
    if (statement == NULL || statement->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
        return MYLITE_OK;
    }
    if (mylite_sql_ast_node_select_modifier(statement) != MYLITE_SQL_AST_SELECT_MODIFIER_DEFAULT ||
        mylite_sql_ast_node_select_options(statement) != 0U ||
        mylite_sql_ast_node_select_calc_found_rows(statement) != 0 ||
        mylite_sql_ast_node_select_locking_clause(statement) !=
            MYLITE_SQL_AST_SELECT_LOCKING_CLAUSE_NONE) {
        return MYLITE_OK;
    }
    select_list = child_at(statement, 0U);
    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST ||
        mylite_sql_ast_node_child_count(select_list) != 1U) {
        return MYLITE_OK;
    }
    select_item = child_at(select_list, 0U);
    if (select_item == NULL || select_item->kind != MYLITE_SQL_AST_SELECT_ITEM) {
        return MYLITE_OK;
    }

    expression = unwrap_parenthesized_expression(child_at(select_item, 0U));
    rc = plan_update_scalar_aggregate_subquery_expression(
        database,
        expression,
        out_plan,
        &function,
        &aggregate_expression,
        &rhs_expression
    );
    if (rc != MYLITE_OK || function == PLANNED_COLUMN_AGGREGATE_NONE) {
        return rc;
    }
    rc = plan_update_scalar_aggregate_subquery_source(
        database,
        statement,
        function,
        aggregate_expression,
        out_plan
    );
    if (rc == MYLITE_OK && rhs_expression != NULL) {
        rc = plan_update_scalar_aggregate_rhs_expression(database, rhs_expression, out_plan);
    }
    if (rc == MYLITE_OK) {
        out_plan->assignment_value_is_scalar_subquery = true;
        out_plan->assignment_value_is_scalar_aggregate_expression_subquery = true;
        *out_handled = true;
    }

    return rc;
}

static int plan_update_scalar_aggregate_subquery_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    struct planned_update *out_plan,
    enum planned_column_aggregate_function *out_function,
    const struct mylite_sql_ast_node **out_aggregate_expression,
    const struct mylite_sql_ast_node **out_rhs_expression
) {
    enum planned_column_aggregate_function function = PLANNED_COLUMN_AGGREGATE_NONE;
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;
    const struct mylite_sql_ast_node *left = NULL;
    const struct mylite_sql_ast_node *right = NULL;

    (void)database;
    if (out_function == NULL || out_aggregate_expression == NULL || out_rhs_expression == NULL) {
        return MYLITE_MISUSE;
    }
    *out_function = PLANNED_COLUMN_AGGREGATE_NONE;
    *out_aggregate_expression = NULL;
    *out_rhs_expression = NULL;
    if (expression == NULL) {
        return MYLITE_OK;
    }

    function = column_aggregate_function_from_expression(expression);
    if (function != PLANNED_COLUMN_AGGREGATE_NONE) {
        *out_function = function;
        *out_aggregate_expression = expression;
        return MYLITE_OK;
    }
    if (expression->kind != MYLITE_SQL_AST_BINARY_EXPRESSION) {
        return MYLITE_OK;
    }

    operator_kind = mylite_sql_ast_node_operator(expression);
    if (operator_kind != MYLITE_SQL_AST_OPERATOR_ADD &&
        operator_kind != MYLITE_SQL_AST_OPERATOR_SUBTRACT) {
        return MYLITE_OK;
    }
    left = unwrap_parenthesized_expression(child_at(expression, 0U));
    right = unwrap_parenthesized_expression(child_at(expression, 1U));
    function = column_aggregate_function_from_expression(left);
    if (function == PLANNED_COLUMN_AGGREGATE_NONE) {
        return MYLITE_OK;
    }

    out_plan->assignment_scalar_aggregate_has_rhs = true;
    out_plan->assignment_scalar_aggregate_operator = operator_kind;
    *out_function = function;
    *out_aggregate_expression = left;
    *out_rhs_expression = right;
    return MYLITE_OK;
}

static int plan_update_scalar_aggregate_rhs_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *rhs_expression,
    struct planned_update *out_plan
) {
    return plan_row_scalar_expression(
        database,
        rhs_expression,
        false,
        NULL,
        NULL,
        NULL,
        0U,
        &out_plan->assignment_scalar_aggregate_rhs
    );
}

static int plan_update_scalar_aggregate_subquery_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    enum planned_column_aggregate_function function,
    const struct mylite_sql_ast_node *aggregate_expression,
    struct planned_update *out_plan
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *select_item = child_at(select_list, 0U);
    const struct mylite_sql_ast_node *from_clause = child_at(statement, 1U);
    const struct mylite_sql_ast_node *optional_clause = child_at(statement, 2U);
    const struct mylite_sql_ast_node *where_clause = NULL;
    struct mylite_catalog_column_descriptor *table_columns = NULL;
    struct select_source_context source_context = {0};
    size_t table_column_count = 0U;
    int rc = MYLITE_OK;

    if (function != PLANNED_COLUMN_AGGREGATE_MIN && function != PLANNED_COLUMN_AGGREGATE_MAX &&
        function != PLANNED_COLUMN_AGGREGATE_SUM) {
        set_unsupported_error(
            database,
            "UPDATE scalar aggregate subquery assignment supports only MIN(), MAX(), and SUM()"
        );
        return MYLITE_ERROR;
    }
    while (optional_clause != NULL) {
        if (optional_clause->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
            where_clause = optional_clause;
        } else {
            set_unsupported_error(
                database,
                "UPDATE scalar aggregate subquery assignment supports only WHERE"
            );
            return MYLITE_ERROR;
        }
        optional_clause = optional_clause->next_sibling;
    }
    if (from_clause == NULL || from_clause->kind != MYLITE_SQL_AST_FROM_TABLE) {
        set_unsupported_error(
            database,
            "UPDATE scalar aggregate subquery assignment supports only one descriptor table source"
        );
        return MYLITE_ERROR;
    }

    out_plan->assignment_scalar_aggregate.expression = child_at(select_item, 0U);
    out_plan->assignment_scalar_aggregate.alias = child_at(select_item, 1U);
    out_plan->assignment_scalar_aggregate.function = function;
    rc = resolve_visible_table_reference(
        database,
        child_at(from_clause, 0U),
        &out_plan->assignment_scalar_aggregate.source,
        &out_plan->assignment_scalar_aggregate.table
    );
    if (rc == MYLITE_OK) {
        rc = validate_select_index_hints(
            database,
            from_clause,
            &out_plan->assignment_scalar_aggregate.table
        );
    }
    if (rc == MYLITE_OK &&
        out_plan->assignment_scalar_aggregate.table.table_id == out_plan->table.table_id) {
        set_update_table_used_error(database, out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = init_select_source_context(
            database,
            from_clause,
            &out_plan->assignment_scalar_aggregate.source,
            &source_context
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_columns(
            database,
            out_plan->assignment_scalar_aggregate.table.table_id,
            &table_columns,
            &table_column_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_column_aggregate_column(
            database,
            aggregate_expression,
            function,
            &source_context,
            table_columns,
            table_column_count,
            &out_plan->assignment_scalar_aggregate.aggregate_column
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_select_predicate(
            database,
            where_clause,
            &source_context,
            table_columns,
            table_column_count,
            &out_plan->assignment_scalar_aggregate.predicate
        );
    }

    free(table_columns);
    return rc;
}

static int validate_update_scalar_subquery_select(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *select_list = NULL;
    const struct mylite_sql_ast_node *from_clause = NULL;

    if (statement == NULL || statement->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
        set_unsupported_error(
            database,
            "UPDATE scalar subquery assignment supports only parenthesized SELECT"
        );
        return MYLITE_ERROR;
    }
    if (mylite_sql_ast_node_select_modifier(statement) != MYLITE_SQL_AST_SELECT_MODIFIER_DEFAULT ||
        mylite_sql_ast_node_select_options(statement) != 0U ||
        mylite_sql_ast_node_select_calc_found_rows(statement) != 0 ||
        mylite_sql_ast_node_select_locking_clause(statement) !=
            MYLITE_SQL_AST_SELECT_LOCKING_CLAUSE_NONE) {
        set_unsupported_error(
            database,
            "UPDATE scalar subquery assignment supports only plain inner SELECT"
        );
        return MYLITE_ERROR;
    }

    select_list = child_at(statement, 0U);
    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST ||
        select_list_is_wildcard(select_list)) {
        set_unsupported_error(
            database,
            "UPDATE scalar subquery assignment supports only one explicit source column"
        );
        return MYLITE_ERROR;
    }

    from_clause = child_at(statement, 1U);
    if (from_clause == NULL || from_clause->kind != MYLITE_SQL_AST_FROM_TABLE) {
        set_unsupported_error(
            database,
            "UPDATE scalar subquery assignment supports only one descriptor table source"
        );
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int convert_update_value(
    struct mylite_db *database,
    const struct planned_update *plan,
    struct planned_value *out_value
) {
    const struct mylite_sql_ast_node *value_node = NULL;
    const struct mylite_catalog_column_descriptor *column = NULL;

    if (plan == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    value_node = unwrap_parenthesized_expression(plan->assignment_value_node);
    column = &plan->assignment_column;
    if (value_node == NULL || column == NULL || out_value == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    if (value_node->kind == MYLITE_SQL_AST_SCALAR_SUBQUERY) {
        planned_value_deinit(out_value);
        *out_value = (struct planned_value){.is_null = true, .is_text = false, .integer = 0};
        return materialize_update_scalar_subquery_value(database, plan, out_value);
    }
    if (value_node->kind == MYLITE_SQL_AST_DEFAULT_FUNCTION) {
        struct select_source_context source_context = {.source = &plan->target};

        return convert_default_function_value_for_target(
            database,
            value_node,
            &source_context,
            plan->columns,
            plan->column_count,
            column,
            plan->ignore_errors,
            out_value
        );
    }

    return convert_update_value_for_column(
        database,
        value_node,
        column,
        1U,
        plan->ignore_errors,
        out_value
    );
}

static int convert_update_value_for_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    bool handled = false;
    bool adjust_missing_default = dml_allows_missing_default_adjustment(database, ignore_errors);
    bool is_constant_arithmetic = false;
    int rc = MYLITE_OK;

    value_node = unwrap_parenthesized_expression(value_node);
    if (value_node == NULL || column == NULL || out_value == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    planned_value_deinit(out_value);
    *out_value = (struct planned_value){.is_null = true, .is_text = false, .integer = 0};
    if (value_node->kind == MYLITE_SQL_AST_DML_DEFAULT_VALUE) {
        return materialize_dml_default_value(database, column, adjust_missing_default, out_value);
    }
    rc = convert_statement_time_value_for_column(database, value_node, column, out_value, &handled);
    if (handled) {
        return rc;
    }
    if (value_node->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_NULL) {
        if (!column->is_nullable) {
            if (!adjust_missing_default) {
                if (column_descriptor_is_spatial(column)) {
                    set_spatial_bad_null_error(database, column->name);
                    return MYLITE_ERROR;
                }
                set_bad_null_error(database, column->name);
                return MYLITE_ERROR;
            }
            return convert_null_insert_value(database, column, true, out_value);
        }
        return MYLITE_OK;
    }
    rc = convert_dml_rand_value(
        database,
        value_node,
        column,
        row_number,
        dml_allows_string_truncation_adjustment(database, ignore_errors),
        true,
        &handled,
        out_value
    );
    if (handled) {
        return rc;
    }
    rc = convert_dml_constant_scalar_value(
        database,
        value_node,
        column,
        NULL,
        row_number,
        ignore_errors,
        dml_allows_string_truncation_adjustment(database, ignore_errors),
        true,
        false,
        false,
        &handled,
        out_value
    );
    if (handled) {
        return rc;
    }
    if (insert_value_is_unix_timestamp_arithmetic(value_node)) {
        return convert_update_unix_timestamp_arithmetic_value(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
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
        return convert_update_constant_arithmetic_value(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }

    return convert_update_column_value(
        database,
        value_node,
        column,
        row_number,
        ignore_errors,
        out_value
    );
}

static int convert_update_constant_arithmetic_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    static const uint64_t int64_min_magnitude = 9223372036854775808ULL;
    struct scalar_arithmetic_value value = {.is_null = false, .integer = 0};
    bool is_negative = false;
    uint64_t magnitude = 0U;
    bool is_constant_arithmetic = false;
    bool adjust_missing_default = dml_allows_missing_default_adjustment(database, ignore_errors);
    bool adjust_errors = dml_allows_missing_default_adjustment(database, ignore_errors);
    int rc = MYLITE_OK;

    rc = update_value_is_constant_arithmetic_expression(
        database,
        value_node,
        &is_constant_arithmetic
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!is_constant_arithmetic) {
        set_unsupported_error(
            database,
            "UPDATE supports only integer, boolean, NULL, and DEFAULT assignment values"
        );
        return MYLITE_ERROR;
    }

    rc = validate_update_constant_arithmetic_assignment_target(database, column);
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = evaluate_scalar_arithmetic_expression(database, value_node, &value);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (value.division_by_zero_warning_count != 0U) {
        set_unsupported_error(
            database,
            "UPDATE constant arithmetic assignment does not support division"
        );
        return MYLITE_ERROR;
    }
    if (value.is_null) {
        if (!column->is_nullable) {
            if (!adjust_missing_default) {
                set_bad_null_error(database, column->name);
                return MYLITE_ERROR;
            }
            rc = append_bad_null_warning(database, column->name);
            if (rc != MYLITE_OK) {
                return rc;
            }
            return make_insert_ignore_implicit_value(database, column, out_value);
        }
        *out_value = (struct planned_value){.is_null = true};
        return MYLITE_OK;
    }

    if (value.integer < 0) {
        is_negative = true;
        if (value.integer == INT64_MIN) {
            magnitude = int64_min_magnitude;
        } else {
            magnitude = (uint64_t)-value.integer;
        }
    } else {
        magnitude = (uint64_t)value.integer;
    }

    out_value->is_null = false;
    return convert_integer_for_column_with_policy(
        database,
        magnitude,
        is_negative,
        column,
        row_number,
        adjust_errors,
        &out_value->integer
    );
}

static int convert_update_unix_timestamp_arithmetic_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    static const struct unix_timestamp_arithmetic_messages messages = {
        .unsupported =
            "UPDATE UNIX_TIMESTAMP arithmetic supports only UNIX_TIMESTAMP(), "
            "UNIX_TIMESTAMP() +/- signed integer literals, and UNIX_TIMESTAMP() +/- NULL",
        .delta_range =
            "UPDATE UNIX_TIMESTAMP arithmetic supports only signed 64-bit integer deltas",
    };
    static const uint64_t int64_min_magnitude = 9223372036854775808ULL;
    struct scalar_arithmetic_value value = {.is_null = false, .integer = 0};
    bool adjust_missing_default = dml_allows_missing_default_adjustment(database, ignore_errors);
    bool is_negative = false;
    uint64_t magnitude = 0U;
    int rc = validate_update_unix_timestamp_arithmetic_assignment_target(database, column);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = evaluate_insert_unix_timestamp_arithmetic_value(database, value_node, &messages, &value);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (value.is_null) {
        return convert_null_insert_value(database, column, adjust_missing_default, out_value);
    }

    if (column_descriptor_is_string_family(column)) {
        return finish_insert_unix_timestamp_text_value(
            database,
            value.integer,
            column,
            row_number,
            dml_allows_string_truncation_adjustment(database, ignore_errors),
            out_value
        );
    }

    if (value.integer < 0) {
        is_negative = true;
        if (value.integer == INT64_MIN) {
            magnitude = int64_min_magnitude;
        } else {
            magnitude = (uint64_t)-value.integer;
        }
    } else {
        magnitude = (uint64_t)value.integer;
    }

    out_value->is_null = false;
    return convert_integer_for_column_with_policy(
        database,
        magnitude,
        is_negative,
        column,
        row_number,
        ignore_errors,
        &out_value->integer
    );
}

static int validate_update_constant_arithmetic_assignment_target(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
) {
    struct integer_column_range range = {0};
    int rc = integer_range_for_column(
        database,
        column,
        "UPDATE constant arithmetic assignment supports only integer columns",
        &range
    );

    (void)range;
    return rc;
}

static int convert_statement_time_value_for_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value,
    bool *out_handled
) {
    if (out_handled == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    *out_handled = true;

    switch (value_node->kind) {
    case MYLITE_SQL_AST_CURRENT_DATE_VALUE:
        if (column_descriptor_is_date(column)) {
            return make_current_date_value(database, out_value);
        }
        set_unsupported_error(database, "CURRENT_DATE values are supported only for DATE columns");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_CURRENT_TIME_VALUE:
        if (column_descriptor_is_time(column)) {
            if (mylite_execution_validate_temporal_fractional_precision(
                    database,
                    value_node,
                    (struct mylite_execution_temporal_fractional_precision_context){
                        .subject_name = "curtime",
                        .unsupported_message =
                            "fractional CURRENT_TIME precision is not supported",
                    }
                ) != MYLITE_OK) {
                return MYLITE_ERROR;
            }
            return make_current_time_value(database, out_value);
        }
        set_unsupported_error(database, "CURRENT_TIME values are supported only for TIME columns");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP_VALUE:
        if (column_descriptor_is_datetime(column) || column_descriptor_is_timestamp(column)) {
            return make_current_timestamp_value(database, out_value);
        }
        set_unsupported_error(
            database,
            "CURRENT_TIMESTAMP values are supported only for DATETIME and TIMESTAMP columns"
        );
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_SYSDATE_FUNCTION:
        if (column_descriptor_is_datetime(column) || column_descriptor_is_timestamp(column)) {
            return make_sysdate_value(database, out_value);
        }
        set_unsupported_error(
            database,
            "SYSDATE values are supported only for DATETIME and TIMESTAMP columns"
        );
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_UTC_DATE_VALUE:
        if (column_descriptor_is_date(column)) {
            return make_utc_date_value(database, out_value);
        }
        set_unsupported_error(database, "UTC_DATE values are supported only for DATE columns");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_UTC_TIME_VALUE:
        if (column_descriptor_is_time(column)) {
            if (mylite_execution_validate_temporal_fractional_precision(
                    database,
                    value_node,
                    (struct mylite_execution_temporal_fractional_precision_context){
                        .subject_name = "utc_time",
                        .unsupported_message =
                            "fractional UTC_TIME precision is not supported",
                    }
                ) != MYLITE_OK) {
                return MYLITE_ERROR;
            }
            return make_utc_time_value(database, out_value);
        }
        set_unsupported_error(database, "UTC_TIME values are supported only for TIME columns");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_UTC_TIMESTAMP_VALUE:
        if (column_descriptor_is_datetime(column) || column_descriptor_is_timestamp(column)) {
            if (mylite_execution_validate_temporal_fractional_precision(
                    database,
                    value_node,
                    (struct mylite_execution_temporal_fractional_precision_context){
                        .subject_name = "utc_timestamp",
                        .unsupported_message =
                            "fractional UTC_TIMESTAMP precision is not supported",
                    }
                ) != MYLITE_OK) {
                return MYLITE_ERROR;
            }
            return make_utc_timestamp_value(database, out_value);
        }
        set_unsupported_error(
            database,
            "UTC_TIMESTAMP values are supported only for DATETIME and TIMESTAMP columns"
        );
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_CURRENT_TIMESTAMP_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "CURRENT_TIMESTAMP");
        return MYLITE_ERROR;
    case MYLITE_SQL_AST_SYSDATE_ARGUMENT_COUNT_ERROR:
        set_native_function_parameter_count_error(database, "SYSDATE");
        return MYLITE_ERROR;
    default:
        *out_handled = false;
        return MYLITE_OK;
    }
}

static int convert_update_column_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    if (column_descriptor_is_char(column)) {
        return convert_char_literal(
            database,
            value_node,
            column,
            row_number,
            dml_allows_string_truncation_adjustment(database, ignore_errors),
            out_value
        );
    }
    if (column_descriptor_is_varchar(column)) {
        return convert_varchar_literal(
            database,
            value_node,
            column,
            row_number,
            dml_allows_string_truncation_adjustment(database, ignore_errors),
            out_value
        );
    }
    if (column_descriptor_is_text_family(column)) {
        return convert_text_family_literal(
            database,
            value_node,
            column,
            row_number,
            dml_allows_string_truncation_adjustment(database, ignore_errors),
            false,
            out_value
        );
    }
    if (column_descriptor_is_json(column)) {
        return convert_json_literal(database, value_node, column, NULL, out_value);
    }
    if (column_descriptor_is_enum(column)) {
        return convert_enum_literal(database, value_node, column, 1U, out_value);
    }
    if (column_descriptor_is_set(column)) {
        return convert_set_literal(database, value_node, column, 1U, out_value);
    }
    if (column_descriptor_is_binary_string_family(column)) {
        return convert_binary_string_literal(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (column_descriptor_is_bit(column)) {
        return convert_bit_literal(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (column_descriptor_is_year(column)) {
        return convert_year_literal(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (column_descriptor_is_decimal(column)) {
        return convert_decimal_literal(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (column_descriptor_is_approximate(column)) {
        return convert_approximate_literal(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (column_descriptor_is_date(column)) {
        return convert_update_date_literal(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (column_descriptor_is_time(column)) {
        return convert_time_literal(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (column_descriptor_is_datetime(column)) {
        return convert_update_datetime_literal(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (column_descriptor_is_timestamp(column)) {
        return convert_update_timestamp_literal(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (column_descriptor_is_spatial(column)) {
        set_cannot_get_geometry_object_error(database);
        return MYLITE_ERROR;
    }

    return convert_update_integer_literal(
        database,
        value_node,
        column,
        row_number,
        ignore_errors,
        out_value
    );
}

static int convert_update_date_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    char normalized[date_text_length + 1U];
    char *copy = NULL;
    char *text = NULL;
    size_t text_length = 0U;
    struct temporal_predicate_normalization_input normalization = {0};
    enum temporal_storage_truncation truncation = TEMPORAL_STORAGE_TRUNCATION_NONE;
    int rc = MYLITE_OK;

    if (!ignore_errors || value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        return convert_date_literal(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
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

    normalization.text = text;
    normalization.text_length = text_length;
    normalization.target_offset_minutes =
        database == NULL ? 0 : database->session.time_zone_offset_minutes;
    if (normalize_date_storage_text(&normalization, normalized, &truncation)) {
        copy = copy_temporal_text(database, normalized, date_text_length);
        if (copy == NULL) {
            free(text);
            return MYLITE_NOMEM;
        }
        rc = canonicalize_date_text(
            database,
            copy,
            date_text_length,
            column->name,
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
                rc = append_incorrect_date_value_note(database, text, column->name, row_number);
            } else {
                rc = append_data_truncated_note(database, column->name, row_number);
            }
        } else if (truncation == TEMPORAL_STORAGE_TRUNCATION_WARNING) {
            rc = append_data_truncated_warning(database, column->name, row_number);
        }
        free(text);
        return rc;
    }
    if (!date_text_has_canonical_shape(text, text_length)) {
        return make_date_zero_with_warning(database, text, column->name, row_number, out_value);
    }

    return canonicalize_date_text(
        database,
        text,
        text_length,
        column->name,
        row_number,
        ignore_errors,
        out_value
    );
}

static int convert_update_datetime_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    char normalized[datetime_text_length + 1U];
    char *copy = NULL;
    char *text = NULL;
    size_t text_length = 0U;
    struct temporal_predicate_normalization_input normalization = {0};
    int rc = MYLITE_OK;

    if (!ignore_errors || value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        return convert_datetime_literal(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
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

    normalization.text = text;
    normalization.text_length = text_length;
    normalization.target_offset_minutes =
        database == NULL ? 0 : database->session.time_zone_offset_minutes;
    if (normalize_single_digit_hour_datetime_storage_text(&normalization, normalized)) {
        free(text);
        copy = copy_temporal_text(database, normalized, datetime_text_length);
        if (copy == NULL) {
            return MYLITE_NOMEM;
        }
        return canonicalize_datetime_text(
            database,
            copy,
            datetime_text_length,
            column->name,
            row_number,
            ignore_errors,
            out_value
        );
    }
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
            column->name,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (normalize_z_temporal_predicate_text(text, text_length, normalized)) {
        copy = copy_temporal_text(database, normalized, datetime_text_length);
        if (copy == NULL) {
            free(text);
            return MYLITE_NOMEM;
        }
        rc = canonicalize_datetime_text(
            database,
            copy,
            datetime_text_length,
            column->name,
            row_number,
            ignore_errors,
            out_value
        );
        if (rc == MYLITE_OK) {
            rc = append_data_truncated_warning(database, column->name, row_number);
        }
        free(text);
        return rc;
    }
    if (!datetime_text_has_canonical_shape(text, text_length)) {
        return make_datetime_zero_with_warning(database, text, column->name, row_number, out_value);
    }

    return canonicalize_datetime_text(
        database,
        text,
        text_length,
        column->name,
        row_number,
        ignore_errors,
        out_value
    );
}

static int convert_update_timestamp_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    char normalized[datetime_text_length + 1U];
    char *copy = NULL;
    char *text = NULL;
    size_t text_length = 0U;
    struct temporal_predicate_normalization_input normalization = {0};
    int rc = MYLITE_OK;

    if (!ignore_errors || value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        return convert_timestamp_literal(
            database,
            value_node,
            column,
            row_number,
            ignore_errors,
            out_value
        );
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

    normalization.text = text;
    normalization.text_length = text_length;
    normalization.target_offset_minutes = 0;
    if (normalize_single_digit_hour_datetime_storage_text(&normalization, normalized)) {
        free(text);
        copy = copy_temporal_text(database, normalized, datetime_text_length);
        if (copy == NULL) {
            return MYLITE_NOMEM;
        }
        return canonicalize_timestamp_text(
            database,
            copy,
            datetime_text_length,
            column->name,
            row_number,
            ignore_errors,
            out_value
        );
    }
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
            column->name,
            row_number,
            ignore_errors,
            out_value
        );
    }
    if (normalize_z_temporal_predicate_text(text, text_length, normalized)) {
        copy = copy_temporal_text(database, normalized, datetime_text_length);
        if (copy == NULL) {
            free(text);
            return MYLITE_NOMEM;
        }
        rc = canonicalize_timestamp_text(
            database,
            copy,
            datetime_text_length,
            column->name,
            row_number,
            ignore_errors,
            out_value
        );
        if (rc == MYLITE_OK) {
            rc = append_data_truncated_warning(database, column->name, row_number);
        }
        free(text);
        return rc;
    }
    if (!datetime_text_has_canonical_shape(text, text_length)) {
        return make_timestamp_zero_with_warning(
            database,
            text,
            column->name,
            row_number,
            out_value
        );
    }

    return canonicalize_timestamp_text(
        database,
        text,
        text_length,
        column->name,
        row_number,
        ignore_errors,
        out_value
    );
}

static int materialize_update_scalar_subquery_value(
    struct mylite_db *database,
    const struct planned_update *plan,
    struct planned_value *out_value
) {
    sqlite3_stmt *statement = NULL;
    struct planned_select scalar_plan = plan->assignment_subquery;
    char *sql = NULL;
    int rc = MYLITE_OK;

    if (plan->assignment_value_is_scalar_aggregate_expression_subquery) {
        return materialize_update_scalar_aggregate_subquery_value(database, plan, out_value);
    }
    if (!plan->assignment_value_is_scalar_subquery ||
        plan->assignment_subquery.column_count != 1U) {
        set_unsupported_error(
            database,
            "UPDATE scalar subquery assignment supports only one descriptor source column"
        );
        return MYLITE_ERROR;
    }

    cap_update_scalar_subquery_limit(&scalar_plan.limit);
    rc = build_select_sql(&scalar_plan, &sql);
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_select_parameters(statement, &scalar_plan);
    }
    if (rc == MYLITE_OK) {
        rc = step_update_scalar_subquery_value(database, statement, plan, out_value);
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

static int materialize_update_scalar_aggregate_subquery_value(
    struct mylite_db *database,
    const struct planned_update *plan,
    struct planned_value *out_value
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int rc = build_update_scalar_aggregate_assignment_sql(plan, &sql);

    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_update_scalar_aggregate_assignment_parameters(statement, plan);
    }
    if (rc == MYLITE_OK) {
        rc = step_update_scalar_subquery_value(database, statement, plan, out_value);
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

static int build_update_scalar_aggregate_assignment_sql(
    const struct planned_update *plan,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    size_t next_parameter = 1U;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT ");
    if (rc == MYLITE_OK) {
        rc = append_update_scalar_aggregate_assignment_sql(&string, plan, &next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(
            &string,
            plan->assignment_scalar_aggregate.table.physical_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_select_predicate_sql(
            &string,
            &plan->assignment_scalar_aggregate.predicate,
            &next_parameter
        );
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

static int append_update_scalar_aggregate_assignment_sql(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
) {
    int rc = append_column_aggregate_select_item_sql(
        string,
        &plan->assignment_scalar_aggregate,
        next_parameter
    );

    if (rc == MYLITE_OK && plan->assignment_scalar_aggregate_has_rhs) {
        if (plan->assignment_scalar_aggregate_operator == MYLITE_SQL_AST_OPERATOR_ADD) {
            rc = mylite_dynamic_string_append(string, " + ");
        } else {
            rc = mylite_dynamic_string_append(string, " - ");
        }
    }
    if (rc == MYLITE_OK && plan->assignment_scalar_aggregate_has_rhs) {
        rc = append_row_scalar_expression_sql(
            string,
            &plan->assignment_scalar_aggregate_rhs,
            next_parameter
        );
    }

    return rc;
}

static int bind_update_scalar_aggregate_assignment_parameters(
    sqlite3_stmt *statement,
    const struct planned_update *plan
) {
    int parameter_index = 1;
    int rc = MYLITE_OK;

    if (plan->assignment_scalar_aggregate_has_rhs) {
        rc = bind_row_scalar_expression_parameters(
            statement,
            &plan->assignment_scalar_aggregate_rhs,
            &parameter_index
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_select_predicate_parameters(
            statement,
            &plan->assignment_scalar_aggregate.predicate,
            &parameter_index
        );
    }

    return rc;
}

static void cap_update_scalar_subquery_limit(struct planned_select_limit *limit) {
    if (limit == NULL) {
        return;
    }
    if (!limit->has_limit) {
        *limit = (struct planned_select_limit){
            .has_limit = true,
            .row_count = 2,
            .has_offset = false,
            .offset = 0,
        };
        return;
    }
    if (limit->row_count > 2) {
        limit->row_count = 2;
    }
}

static int step_update_scalar_subquery_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_update *plan,
    struct planned_value *out_value
) {
    int sqlite_rc = sqlite3_step(statement);
    int rc = MYLITE_OK;

    if (sqlite_rc == SQLITE_DONE) {
        return convert_null_insert_value(
            database,
            &plan->assignment_column,
            plan->ignore_errors,
            out_value
        );
    }
    if (sqlite_rc != SQLITE_ROW) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    if (plan->assignment_value_is_scalar_aggregate_expression_subquery) {
        rc = materialize_update_scalar_aggregate_sqlite_value(database, statement, plan, out_value);
    } else {
        rc = materialize_update_scalar_subquery_sqlite_value(database, statement, plan, out_value);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    sqlite_rc = sqlite3_step(statement);
    if (sqlite_rc == SQLITE_ROW) {
        set_scalar_subquery_row_count_error(database);
        return MYLITE_ERROR;
    }
    if (sqlite_rc != SQLITE_DONE) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    return MYLITE_OK;
}

static int materialize_update_scalar_aggregate_sqlite_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_update *plan,
    struct planned_value *out_value
) {
    const struct mylite_catalog_column_descriptor *target_column = &plan->assignment_column;
    struct integer_column_range range = {0};
    int sqlite_type = sqlite3_column_type(statement, 0);
    int rc = MYLITE_OK;

    if (sqlite_type == SQLITE_NULL) {
        return convert_null_insert_value(database, target_column, plan->ignore_errors, out_value);
    }
    rc = integer_range_for_column(
        database,
        target_column,
        "UPDATE scalar aggregate subquery assignment supports only integer targets",
        &range
    );
    (void)range;
    if (rc != MYLITE_OK) {
        return rc;
    }
    return materialize_update_scalar_subquery_integer_value(
        database,
        statement,
        sqlite_type,
        target_column,
        out_value
    );
}

static int materialize_update_scalar_subquery_sqlite_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_update *plan,
    struct planned_value *out_value
) {
    const struct mylite_catalog_column_descriptor *source_column =
        &plan->assignment_subquery.columns[0];
    const struct mylite_catalog_column_descriptor *target_column = &plan->assignment_column;
    int sqlite_type = sqlite3_column_type(statement, 0);

    if (sqlite_type == SQLITE_NULL) {
        return convert_null_insert_value(database, target_column, plan->ignore_errors, out_value);
    }
    if (!update_scalar_subquery_source_target_types_are_compatible(source_column, target_column)) {
        set_unsupported_error(
            database,
            update_scalar_subquery_implicit_conversion_message(target_column)
        );
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_approximate(target_column)) {
        return materialize_update_scalar_subquery_approximate_value(
            database,
            statement,
            target_column,
            out_value
        );
    }
    if (update_scalar_subquery_target_uses_text_storage(target_column)) {
        return materialize_update_scalar_subquery_text_storage_value(
            database,
            statement,
            target_column,
            out_value
        );
    }
    if (update_scalar_subquery_target_uses_blob_storage(target_column)) {
        return materialize_update_scalar_subquery_blob_storage_value(
            database,
            statement,
            target_column,
            out_value
        );
    }

    return materialize_update_scalar_subquery_integer_value(
        database,
        statement,
        sqlite_type,
        target_column,
        out_value
    );
}

static bool update_scalar_subquery_target_uses_text_storage(
    const struct mylite_catalog_column_descriptor *target_column
) {
    if (column_descriptor_is_string_family(target_column)) {
        return true;
    }
    if (column_descriptor_is_json(target_column)) {
        return true;
    }
    if (column_descriptor_is_decimal(target_column)) {
        return true;
    }
    if (column_descriptor_is_date(target_column)) {
        return true;
    }
    if (column_descriptor_is_time(target_column)) {
        return true;
    }
    if (column_descriptor_is_datetime(target_column)) {
        return true;
    }
    if (column_descriptor_is_timestamp(target_column)) {
        return true;
    }
    if (column_descriptor_is_year(target_column)) {
        return true;
    }
    return false;
}

static bool update_scalar_subquery_target_uses_blob_storage(
    const struct mylite_catalog_column_descriptor *target_column
) {
    return (column_descriptor_is_binary_string_family(target_column) ||
            column_descriptor_is_bit(target_column)) != 0;
}

static int materialize_update_scalar_subquery_text_storage_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct mylite_catalog_column_descriptor *target_column,
    struct planned_value *out_value
) {
    int rc = validate_update_scalar_subquery_text_storage_value(database, statement, target_column);

    if (rc == MYLITE_OK) {
        rc = copy_update_scalar_subquery_text_value(database, statement, 0, out_value);
    }
    return rc;
}

static int materialize_update_scalar_subquery_blob_storage_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct mylite_catalog_column_descriptor *target_column,
    struct planned_value *out_value
) {
    if (column_descriptor_is_bit(target_column)) {
        return materialize_sqlite_bit_value(database, statement, 0, target_column, 1U, out_value);
    }
    return materialize_sqlite_binary_string_value(
        database,
        statement,
        0,
        target_column,
        1U,
        out_value
    );
}

static int validate_update_scalar_subquery_text_storage_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct mylite_catalog_column_descriptor *target_column
) {
    if (column_descriptor_is_string_family(target_column)) {
        return validate_insert_select_string_value(database, statement, 0, target_column, 1U);
    }
    if (column_descriptor_is_json(target_column)) {
        return validate_insert_select_json_value(database, statement, 0, target_column);
    }
    if (column_descriptor_is_decimal(target_column)) {
        return validate_insert_select_decimal_value(database, statement, 0, target_column, 1U);
    }
    if (column_descriptor_is_date(target_column)) {
        return validate_insert_select_date_value(database, statement, 0, target_column, 1U);
    }
    if (column_descriptor_is_time(target_column)) {
        return validate_insert_select_time_value(database, statement, 0, target_column, 1U);
    }
    if (column_descriptor_is_datetime(target_column)) {
        return validate_insert_select_datetime_value(database, statement, 0, target_column, 1U);
    }
    if (column_descriptor_is_year(target_column)) {
        return validate_insert_select_year_value(database, statement, 0, target_column, 1U);
    }
    return validate_insert_select_timestamp_value(database, statement, 0, target_column, 1U);
}

static int materialize_update_scalar_subquery_approximate_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct mylite_catalog_column_descriptor *target_column,
    struct planned_value *out_value
) {
    int rc = validate_insert_select_approximate_value(database, statement, 0, target_column, 1U);

    if (rc == MYLITE_OK) {
        *out_value = (struct planned_value){
            .is_null = false,
            .is_real = true,
            .real = sqlite3_column_double(statement, 0),
        };
    }
    return rc;
}

static int materialize_update_scalar_subquery_integer_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int sqlite_type,
    const struct mylite_catalog_column_descriptor *target_column,
    struct planned_value *out_value
) {
    int rc = MYLITE_OK;

    if (sqlite_type != SQLITE_INTEGER) {
        set_unsupported_error(
            database,
            "UPDATE scalar subquery assignment does not support implicit integer conversion"
        );
        return MYLITE_ERROR;
    }

    rc = validate_insert_select_integer_value(database, statement, 0, target_column, 1U, false);
    if (rc == MYLITE_OK) {
        *out_value = (struct planned_value){
            .is_null = false,
            .integer = (int64_t)sqlite3_column_int64(statement, 0),
        };
    }
    return rc;
}

static int copy_update_scalar_subquery_text_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    struct planned_value *out_value
) {
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

static bool update_scalar_subquery_source_target_types_are_compatible(
    const struct mylite_catalog_column_descriptor *source_column,
    const struct mylite_catalog_column_descriptor *target_column
) {
    return insert_select_source_target_types_are_compatible(source_column, target_column);
}

static const char *update_scalar_subquery_implicit_conversion_message(
    const struct mylite_catalog_column_descriptor *target_column
) {
    if (column_descriptor_is_string_family(target_column)) {
        return "UPDATE scalar subquery assignment does not support implicit string conversion";
    }
    if (column_descriptor_is_json(target_column)) {
        return "UPDATE scalar subquery assignment does not support implicit JSON conversion";
    }
    if (column_descriptor_is_enum(target_column)) {
        return "UPDATE scalar subquery assignment does not support implicit ENUM conversion";
    }
    if (column_descriptor_is_set(target_column)) {
        return "UPDATE scalar subquery assignment does not support implicit SET conversion";
    }
    if (column_descriptor_is_binary_string_family(target_column)) {
        return "UPDATE scalar subquery assignment does not support implicit binary string "
               "conversion";
    }
    if (column_descriptor_is_bit(target_column)) {
        return "UPDATE scalar subquery assignment does not support implicit BIT conversion";
    }
    if (column_descriptor_is_decimal(target_column)) {
        return "UPDATE scalar subquery assignment does not support implicit DECIMAL conversion";
    }
    if (column_descriptor_is_approximate(target_column)) {
        return "UPDATE scalar subquery assignment does not support implicit FLOAT or DOUBLE "
               "conversion";
    }
    if (column_descriptor_is_date(target_column)) {
        return "UPDATE scalar subquery assignment does not support implicit DATE conversion";
    }
    if (column_descriptor_is_time(target_column)) {
        return "UPDATE scalar subquery assignment does not support implicit TIME conversion";
    }
    if (column_descriptor_is_datetime(target_column)) {
        return "UPDATE scalar subquery assignment does not support implicit DATETIME conversion";
    }
    if (column_descriptor_is_timestamp(target_column)) {
        return "UPDATE scalar subquery assignment does not support implicit TIMESTAMP conversion";
    }
    if (column_descriptor_is_year(target_column)) {
        return "UPDATE scalar subquery assignment does not support implicit YEAR conversion";
    }

    return "UPDATE scalar subquery assignment does not support implicit integer conversion";
}

static int convert_update_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    bool ignore_errors,
    struct planned_value *out_value
) {
    const struct mylite_sql_ast_node *literal = value_node;
    bool is_negative = false;
    bool adjust_errors = dml_allows_missing_default_adjustment(database, ignore_errors);
    uint64_t magnitude = 0U;
    int rc = MYLITE_OK;

    if (value_node->kind == MYLITE_SQL_AST_UNARY_EXPRESSION) {
        enum mylite_sql_ast_operator operator_kind = mylite_sql_ast_node_operator(value_node);

        if (operator_kind == MYLITE_SQL_AST_OPERATOR_NEGATIVE) {
            is_negative = true;
        } else if (operator_kind != MYLITE_SQL_AST_OPERATOR_POSITIVE) {
            set_unsupported_error(
                database,
                "UPDATE supports only integer, boolean, NULL, and DEFAULT assignment values"
            );
            return MYLITE_ERROR;
        }
        literal = child_at(value_node, 0U);
    }
    if (!boolean_literal_magnitude(literal, &magnitude)) {
        if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
            (mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER &&
             mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_STRING &&
             mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_HEX)) {
            set_unsupported_error(
                database,
                "UPDATE supports only integer, boolean, NULL, and DEFAULT assignment values"
            );
            return MYLITE_ERROR;
        }
        if (mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_STRING) {
            return convert_integer_string_literal(
                database,
                literal,
                column,
                row_number,
                ignore_errors,
                out_value
            );
        }
        if (mylite_sql_ast_node_literal_kind(literal) == MYLITE_SQL_AST_LITERAL_HEX) {
            return convert_integer_hex_literal(
                database,
                literal,
                column,
                row_number,
                ignore_errors,
                is_negative,
                out_value
            );
        }

        rc = parse_unsigned_integer_literal(&literal->span, &magnitude);
        if (rc != MYLITE_OK) {
            if (!adjust_errors) {
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
    }

    out_value->is_null = false;
    rc = convert_integer_for_column_with_policy(
        database,
        magnitude,
        is_negative,
        column,
        row_number,
        adjust_errors,
        &out_value->integer
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    return MYLITE_OK;
}

static int plan_update_limit(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *limit_clause,
    struct planned_select_limit *out_limit
) {
    int rc = MYLITE_OK;

    out_limit->has_limit = false;
    out_limit->row_count = 0;
    out_limit->has_offset = false;
    out_limit->offset = 0;
    if (limit_clause == NULL) {
        return MYLITE_OK;
    }
    if (limit_clause->kind != MYLITE_SQL_AST_LIMIT_CLAUSE) {
        set_unsupported_error(database, "UPDATE supports only LIMIT row_count");
        return MYLITE_ERROR;
    }
    if (child_at(limit_clause, 1U) != NULL) {
        set_unsupported_error(database, "UPDATE supports only LIMIT row_count");
        return MYLITE_ERROR;
    }

    rc = convert_limit_integer_literal(database, child_at(limit_clause, 0U), &out_limit->row_count);
    if (rc != MYLITE_OK) {
        return rc;
    }
    out_limit->has_limit = true;

    return MYLITE_OK;
}

static int convert_limit_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    int64_t *out_value
) {
    const uint64_t sqlite_int64_positive_max = 9223372036854775807ULL;
    uint64_t magnitude = 0U;
    int rc = MYLITE_OK;

    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_unsupported_error(database, "LIMIT supports only unsigned decimal integer literals");
        return MYLITE_ERROR;
    }

    rc = parse_unsigned_integer_literal(&value_node->span, &magnitude);
    if (rc != MYLITE_OK || magnitude > sqlite_int64_positive_max) {
        set_limit_out_of_range_error(database);
        return MYLITE_ERROR;
    }

    *out_value = (int64_t)magnitude;
    return MYLITE_OK;
}

static int integer_range_for_column(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    const char *unsupported_message,
    struct integer_column_range *out_range
) {
    return integer_range_for_logical_type(
        database,
        (struct integer_logical_type_range_request){
            .logical_type = column->logical_type,
            .unsupported_message = unsupported_message,
        },
        out_range
    );
}

static int integer_range_for_logical_type(
    struct mylite_db *database,
    struct integer_logical_type_range_request request,
    struct integer_column_range *out_range
) {
    const uint64_t int_signed_positive_max = 2147483647ULL;
    const uint64_t int_signed_negative_abs_max = 2147483648ULL;
    const uint64_t int_unsigned_max = 4294967295ULL;
    const uint64_t mediumint_signed_positive_max = 8388607ULL;
    const uint64_t mediumint_signed_negative_abs_max = 8388608ULL;
    const uint64_t mediumint_unsigned_max = 16777215ULL;
    const uint64_t smallint_signed_positive_max = 32767ULL;
    const uint64_t smallint_signed_negative_abs_max = 32768ULL;
    const uint64_t smallint_unsigned_max = 65535ULL;
    const uint64_t tinyint_signed_positive_max = 127ULL;
    const uint64_t tinyint_signed_negative_abs_max = 128ULL;
    const uint64_t tinyint_unsigned_max = 255ULL;
    const uint64_t bigint_signed_positive_max = 9223372036854775807ULL;
    const uint64_t bigint_signed_negative_abs_max = 9223372036854775808ULL;

    if (strcmp(request.logical_type, "TINYINT") == 0) {
        *out_range = (struct integer_column_range){
            .positive_max = tinyint_signed_positive_max,
            .negative_abs_max = tinyint_signed_negative_abs_max,
        };
        return MYLITE_OK;
    }
    if (strcmp(request.logical_type, "TINYINT(1)") == 0) {
        *out_range = (struct integer_column_range){
            .positive_max = tinyint_signed_positive_max,
            .negative_abs_max = tinyint_signed_negative_abs_max,
        };
        return MYLITE_OK;
    }
    if (strcmp(request.logical_type, "TINYINT UNSIGNED") == 0) {
        *out_range = (struct integer_column_range){
            .positive_max = tinyint_unsigned_max,
            .negative_abs_max = 0U,
        };
        return MYLITE_OK;
    }
    if (strcmp(request.logical_type, "SMALLINT") == 0) {
        *out_range = (struct integer_column_range){
            .positive_max = smallint_signed_positive_max,
            .negative_abs_max = smallint_signed_negative_abs_max,
        };
        return MYLITE_OK;
    }
    if (strcmp(request.logical_type, "SMALLINT UNSIGNED") == 0) {
        *out_range = (struct integer_column_range){
            .positive_max = smallint_unsigned_max,
            .negative_abs_max = 0U,
        };
        return MYLITE_OK;
    }
    if (strcmp(request.logical_type, "MEDIUMINT") == 0) {
        *out_range = (struct integer_column_range){
            .positive_max = mediumint_signed_positive_max,
            .negative_abs_max = mediumint_signed_negative_abs_max,
        };
        return MYLITE_OK;
    }
    if (strcmp(request.logical_type, "MEDIUMINT UNSIGNED") == 0) {
        *out_range = (struct integer_column_range){
            .positive_max = mediumint_unsigned_max,
            .negative_abs_max = 0U,
        };
        return MYLITE_OK;
    }
    if (strcmp(request.logical_type, "INT") == 0) {
        *out_range = (struct integer_column_range){
            .positive_max = int_signed_positive_max,
            .negative_abs_max = int_signed_negative_abs_max,
        };
        return MYLITE_OK;
    }
    if (strcmp(request.logical_type, "INT UNSIGNED") == 0) {
        *out_range = (struct integer_column_range){
            .positive_max = int_unsigned_max,
            .negative_abs_max = 0U,
        };
        return MYLITE_OK;
    }
    if (strcmp(request.logical_type, "BIGINT") == 0) {
        *out_range = (struct integer_column_range){
            .positive_max = bigint_signed_positive_max,
            .negative_abs_max = bigint_signed_negative_abs_max,
        };
        return MYLITE_OK;
    }
    if (strcmp(request.logical_type, "BIGINT UNSIGNED") == 0) {
        *out_range = (struct integer_column_range){
            .positive_max = bigint_signed_positive_max,
            .negative_abs_max = 0U,
        };
        return MYLITE_OK;
    }

    if (database != NULL) {
        set_unsupported_error(database, request.unsupported_message);
    }
    return MYLITE_ERROR;
}
