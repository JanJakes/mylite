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
        current->kind != MYLITE_SQL_AST_IS_NULL_PREDICATE &&
        current->kind != MYLITE_SQL_AST_IN_PREDICATE) {
        set_unsupported_error(database, "HAVING supports only one grouped predicate");
        return MYLITE_ERROR;
    }

    rc = plan_grouped_having_operand(
        database,
        child_at(current, 0U),
        current->kind,
        source_context,
        table_columns,
        table_column_count,
        out_plan,
        &operand
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (current->kind == MYLITE_SQL_AST_IN_PREDICATE &&
        operand != PLANNED_GROUPED_HAVING_OPERAND_GROUP_COLUMN) {
        set_unsupported_error(database, "HAVING IN supports only grouped columns");
        return MYLITE_ERROR;
    }

    if (current->kind == MYLITE_SQL_AST_COMPARISON_PREDICATE &&
        operand == PLANNED_GROUPED_HAVING_OPERAND_AGGREGATE &&
        out_plan->having.aggregate_index < out_plan->aggregate_count &&
        out_plan->aggregates[out_plan->having.aggregate_index].function ==
            PLANNED_GROUPED_AGGREGATE_GROUP_CONCAT) {
        set_unsupported_error(
            database,
            "HAVING does not support GROUP_CONCAT aggregate predicates"
        );
        return MYLITE_ERROR;
    }

    out_plan->having.operand = operand;
    out_plan->having.operator_kind = mylite_sql_ast_node_operator(current);
    if (current->kind == MYLITE_SQL_AST_IS_NULL_PREDICATE) {
        out_plan->having.kind = PLANNED_GROUPED_HAVING_IS_NULL;
        return MYLITE_OK;
    }
    if (current->kind == MYLITE_SQL_AST_IN_PREDICATE) {
        rc = convert_grouped_having_in_values(
            database,
            child_at(current, 1U),
            out_plan,
            operand,
            &out_plan->having.values,
            &out_plan->having.value_count
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        out_plan->having.kind = PLANNED_GROUPED_HAVING_IN;
        return MYLITE_OK;
    }

    rc = convert_grouped_having_comparison_value(
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
    enum mylite_sql_ast_node_kind predicate_kind,
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
    if (operand->kind == MYLITE_SQL_AST_BINARY_EXPRESSION) {
        return plan_grouped_having_row_scalar_operand(
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
        predicate_kind,
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

static int plan_grouped_having_row_scalar_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *operand_node,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan,
    enum planned_grouped_having_operand *out_operand
) {
    struct planned_row_scalar_expression expression = {0};
    int rc = plan_row_scalar_expression(
        database,
        operand_node,
        true,
        source_context,
        NULL,
        table_columns,
        table_column_count,
        &expression
    );

    if (rc == MYLITE_OK) {
        rc = validate_grouped_having_row_scalar_expression(
            database,
            out_plan,
            table_columns,
            table_column_count,
            &expression
        );
    }
    if (rc == MYLITE_OK) {
        out_plan->having.row_scalar_expression =
            calloc(1U, sizeof(*out_plan->having.row_scalar_expression));
        if (out_plan->having.row_scalar_expression == NULL) {
            set_nomem_error(database);
            rc = MYLITE_NOMEM;
        }
    }
    if (rc == MYLITE_OK) {
        *out_plan->having.row_scalar_expression = expression;
        expression = (struct planned_row_scalar_expression){0};
        *out_operand = PLANNED_GROUPED_HAVING_OPERAND_ROW_SCALAR;
    }
    planned_row_scalar_expression_deinit(&expression);
    return rc;
}

static int validate_grouped_having_row_scalar_expression(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct planned_row_scalar_expression *expression
) {
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "HAVING supports scalar expressions only over grouped or literal operands"
        );
        return MYLITE_ERROR;
    }

    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return validate_grouped_having_row_scalar_column(
            database,
            plan,
            table_columns,
            table_column_count,
            expression
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
        if (expression->argument_count != 2U || expression->arguments == NULL) {
            set_unsupported_error(
                database,
                "HAVING supports scalar expressions only over grouped or literal operands"
            );
            return MYLITE_ERROR;
        }
        for (size_t argument_index = 0U; argument_index < expression->argument_count;
             ++argument_index) {
            int rc = validate_grouped_having_row_scalar_expression(
                database,
                plan,
                table_columns,
                table_column_count,
                &expression->arguments[argument_index]
            );

            if (rc != MYLITE_OK) {
                return rc;
            }
        }
        return MYLITE_OK;
    default:
        set_unsupported_error(
            database,
            "HAVING supports scalar expressions only over grouped or literal operands"
        );
        return MYLITE_ERROR;
    }
}

static int validate_grouped_having_row_scalar_column(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct planned_row_scalar_expression *expression
) {
    int dependency_rc = MYLITE_OK;
    size_t source_index =
        expression->column_has_source_index ? expression->column_source_index : 0U;

    if (grouped_projection_column_is_allowed(
            database,
            plan,
            table_columns,
            table_column_count,
            expression->column,
            source_index,
            &dependency_rc
        )) {
        return MYLITE_OK;
    }
    if (dependency_rc != MYLITE_OK) {
        return dependency_rc;
    }

    set_only_full_group_by_error(
        database,
        1U,
        "HAVING clause",
        planned_grouped_aggregate_source_resolution(plan, source_index),
        expression->column
    );
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
    enum mylite_sql_ast_node_kind predicate_kind,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
) {
    enum planned_grouped_aggregate_function function =
        grouped_aggregate_function_from_expression(operand_node);
    struct mylite_catalog_column_descriptor column = {0};
    struct integer_column_range range = {0};
    struct planned_grouped_aggregate_item having_item = {0};
    size_t source_index = 0U;
    size_t selected_index = SIZE_MAX;
    int rc = MYLITE_OK;

    if (function == PLANNED_GROUPED_AGGREGATE_NONE) {
        set_unsupported_error(database, "HAVING supports only grouped columns or aggregates");
        return MYLITE_ERROR;
    }
    if (function == PLANNED_GROUPED_AGGREGATE_ANY_VALUE) {
        return reject_grouped_having_any_value_operand(database, operand_node);
    }
    if (function == PLANNED_GROUPED_AGGREGATE_GROUP_CONCAT) {
        if (predicate_kind != MYLITE_SQL_AST_IS_NULL_PREDICATE) {
            set_unsupported_error(
                database,
                "HAVING does not support GROUP_CONCAT aggregate predicates"
            );
            return MYLITE_ERROR;
        }
        rc = plan_grouped_aggregate_expression(
            database,
            operand_node,
            source_context,
            table_columns,
            table_column_count,
            out_plan,
            NULL,
            &having_item
        );
        if (rc != MYLITE_OK) {
            goto cleanup;
        }
        for (size_t aggregate_index = 0U; aggregate_index < out_plan->aggregate_count;
             ++aggregate_index) {
            if (!grouped_order_aggregate_items_match(
                    &having_item,
                    &out_plan->aggregates[aggregate_index]
                )) {
                continue;
            }
            if (selected_index != SIZE_MAX) {
                set_unsupported_error(
                    database,
                    "HAVING supports only unique selected aggregate expressions"
                );
                rc = MYLITE_ERROR;
                goto cleanup;
            }
            selected_index = aggregate_index;
        }
        if (selected_index == SIZE_MAX) {
            set_unsupported_error(database, "HAVING supports only the selected aggregate result");
            rc = MYLITE_ERROR;
            goto cleanup;
        }

        out_plan->having.aggregate_index = selected_index;
        goto cleanup;
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
    if (column_aggregate_argument_is_row_scalar_expression(operand_node)) {
        rc = plan_grouped_aggregate_expression(
            database,
            operand_node,
            source_context,
            table_columns,
            table_column_count,
            out_plan,
            NULL,
            &having_item
        );
        if (rc != MYLITE_OK) {
            goto cleanup;
        }
        for (size_t aggregate_index = 0U; aggregate_index < out_plan->aggregate_count;
             ++aggregate_index) {
            if (!grouped_order_aggregate_items_match(
                    &having_item,
                    &out_plan->aggregates[aggregate_index]
                )) {
                continue;
            }
            if (selected_index != SIZE_MAX) {
                set_unsupported_error(
                    database,
                    "HAVING supports only unique selected aggregate expressions"
                );
                rc = MYLITE_ERROR;
                goto cleanup;
            }
            selected_index = aggregate_index;
        }
        if (selected_index == SIZE_MAX) {
            set_unsupported_error(database, "HAVING supports only the selected aggregate result");
            rc = MYLITE_ERROR;
            goto cleanup;
        }

        out_plan->having.aggregate_index = selected_index;
        goto cleanup;
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
    if (rc == MYLITE_OK && function != PLANNED_GROUPED_AGGREGATE_COUNT_COLUMN &&
        function != PLANNED_GROUPED_AGGREGATE_COUNT_DISTINCT_COLUMN) {
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

cleanup:
    planned_row_scalar_expression_deinit(&having_item.aggregate_row_scalar_expression);
    planned_select_order_deinit(&having_item.aggregate_order);
    planned_value_deinit(&having_item.separator);
    return rc;
}

static int reject_grouped_having_any_value_operand(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *operand_node
) {
    const struct mylite_sql_ast_node *argument = child_at(operand_node, 0U);
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char column_name[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    size_t part_count = 0U;
    int rc = MYLITE_OK;

    if (argument == NULL) {
        set_unsupported_error(database, "HAVING supports only grouped columns or aggregates");
        return MYLITE_ERROR;
    }
    rc = collect_column_reference_parts(database, argument, parts, &part_count);
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

    set_unknown_having_column_error(database, column_name);
    return MYLITE_ERROR;
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

    out_plan->having.aggregate_index = matched_index;
    *out_matched = true;
    return MYLITE_OK;
}

static int convert_grouped_having_comparison_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    struct planned_grouped_aggregate *plan,
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
    bool handled = false;
    uint64_t magnitude = 0U;
    int rc = MYLITE_OK;

    *out_value = (struct planned_value){.is_null = false, .integer = 0};
    rc = convert_grouped_having_any_value_alias_value(
        database,
        value_node,
        plan,
        operand,
        out_value,
        &handled
    );
    if (rc != MYLITE_OK || handled) {
        return rc;
    }

    if (operand == PLANNED_GROUPED_HAVING_OPERAND_GROUP_COLUMN && group_column != NULL &&
        column_descriptor_is_string_family(group_column) && value_node != NULL &&
        value_node->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_STRING) {
        return convert_grouped_having_string_value(database, value_node, out_value);
    }

    rc = parse_grouped_having_integer_literal(
        database,
        value_node,
        operand_name,
        &is_negative,
        &magnitude
    );
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
    if (grouped_having_operand_uses_uint64_order_key(plan, operand)) {
        return convert_grouped_having_bitwise_value(
            database,
            magnitude,
            is_negative,
            operand_name,
            plan,
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

static int convert_grouped_having_in_values(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_list,
    struct planned_grouped_aggregate *plan,
    enum planned_grouped_having_operand operand,
    struct planned_value **out_values,
    size_t *out_value_count
) {
    const struct mylite_sql_ast_node *value_node = NULL;
    struct planned_value *values = NULL;
    size_t value_count = 0U;
    int rc = MYLITE_OK;

    *out_values = NULL;
    *out_value_count = 0U;
    if (operand != PLANNED_GROUPED_HAVING_OPERAND_GROUP_COLUMN) {
        set_unsupported_error(database, "HAVING IN supports only grouped columns");
        return MYLITE_ERROR;
    }
    if (value_list == NULL || value_list->kind != MYLITE_SQL_AST_PREDICATE_VALUE_LIST ||
        mylite_sql_ast_node_child_count(value_list) == 0U) {
        set_unsupported_error(database, "HAVING IN supports only nonempty literal lists");
        return MYLITE_ERROR;
    }

    value_count = mylite_sql_ast_node_child_count(value_list);
    if (value_count > SIZE_MAX / sizeof(*values)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    values = calloc(value_count, sizeof(*values));
    if (values == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    value_node = value_list->first_child;

    for (size_t value_index = 0U; rc == MYLITE_OK && value_index < value_count;
         ++value_index, value_node = value_node->next_sibling) {
        if (value_node != NULL && value_node->kind == MYLITE_SQL_AST_LITERAL &&
            mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_NULL) {
            values[value_index] = (struct planned_value){.is_null = true};
            continue;
        }

        rc = convert_grouped_having_comparison_value(
            database,
            value_node,
            plan,
            operand,
            &values[value_index]
        );
    }
    if (rc != MYLITE_OK) {
        planned_value_array_deinit(values, value_count);
        free(values);
        return rc;
    }

    *out_values = values;
    *out_value_count = value_count;
    return MYLITE_OK;
}

static int convert_grouped_having_any_value_alias_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    struct planned_grouped_aggregate *plan,
    enum planned_grouped_having_operand operand,
    struct planned_value *out_value,
    bool *out_handled
) {
    const struct planned_grouped_aggregate_item *item = NULL;
    bool is_negative = false;
    uint64_t magnitude = 0U;
    int rc = MYLITE_OK;

    *out_handled = false;
    if (plan == NULL || operand != PLANNED_GROUPED_HAVING_OPERAND_AGGREGATE ||
        plan->having.aggregate_index >= plan->aggregate_count) {
        return MYLITE_OK;
    }

    item = &plan->aggregates[plan->having.aggregate_index];
    if (item->function != PLANNED_GROUPED_AGGREGATE_ANY_VALUE) {
        return MYLITE_OK;
    }

    *out_handled = true;
    if (column_descriptor_is_string_family(&item->aggregate_column)) {
        return convert_grouped_having_string_value(database, value_node, out_value);
    }

    rc = parse_grouped_having_integer_literal(
        database,
        value_node,
        item->aggregate_column.name,
        &is_negative,
        &magnitude
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    return convert_grouped_having_group_value(
        database,
        magnitude,
        is_negative,
        &item->aggregate_column,
        out_value
    );
}

static int convert_grouped_having_string_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    struct planned_value *out_value
) {
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_unsupported_error(database, "HAVING string comparisons support only string literals");
        return MYLITE_ERROR;
    }

    rc = decode_sql_string_literal_with_policy(
        database,
        value_node,
        "HAVING string comparisons support only string literals",
        "HAVING string comparison literals do not support NUL bytes",
        true,
        &text,
        &text_length
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    return assign_text_value(database, text, text_length, out_value);
}

static bool grouped_having_operand_uses_uint64_order_key(
    const struct planned_grouped_aggregate *plan,
    enum planned_grouped_having_operand operand
) {
    if (plan == NULL || operand != PLANNED_GROUPED_HAVING_OPERAND_AGGREGATE ||
        plan->having.aggregate_index >= plan->aggregate_count) {
        return false;
    }

    return grouped_aggregate_function_is_bitwise(
        plan->aggregates[plan->having.aggregate_index].function
    );
}

static int convert_grouped_having_bitwise_value(
    struct mylite_db *database,
    uint64_t magnitude,
    bool is_negative,
    const char *operand_name,
    struct planned_grouped_aggregate *plan,
    struct planned_value *out_value
) {
    int written = 0;

    if (plan == NULL || out_value == NULL) {
        return MYLITE_ERROR;
    }
    if (is_negative) {
        set_having_out_of_range_error(database, operand_name);
        return MYLITE_ERROR;
    }

    written = snprintf(
        plan->having.uint64_order_key,
        sizeof(plan->having.uint64_order_key),
        "%020" PRIu64,
        magnitude
    );
    if (written < 0 || (size_t)written >= sizeof(plan->having.uint64_order_key)) {
        set_runtime_error(database, "failed to format bitwise HAVING comparison key");
        return MYLITE_ERROR;
    }

    plan->having.value_uses_uint64_order_key = true;
    out_value->is_null = false;
    out_value->integer = 0;
    return MYLITE_OK;
}
