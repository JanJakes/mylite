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

        if (grouped_aggregate_function_from_expression(unwrap_parenthesized_expression(expression)
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
        } else if (grouped_expression_is_temporal_group_key(expression)) {
            rc = plan_grouped_aggregate_temporal_projection(
                database,
                expression,
                child_at(item, 1U),
                source_context,
                table_columns,
                table_column_count,
                out_plan
            );
        } else if (select_item_is_literal_projection(item)) {
            rc = plan_grouped_aggregate_literal_projection(
                database,
                expression,
                child_at(item, 1U),
                source_context,
                table_columns,
                table_column_count,
                out_plan
            );
        } else if (row_scalar_expression_contains_integer_arithmetic_attempt(expression)) {
            rc = plan_grouped_aggregate_integer_arithmetic_projection(
                database,
                expression,
                child_at(item, 1U),
                source_context,
                table_columns,
                table_column_count,
                out_plan
            );
        } else if (grouped_expression_is_relaxed_row_scalar_projection(database, expression)) {
            rc = plan_grouped_aggregate_relaxed_row_scalar_projection(
                database,
                expression,
                child_at(item, 1U),
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

static int append_grouped_projection_row_scalar_expression(
    struct mylite_db *database,
    struct planned_grouped_aggregate *plan,
    struct planned_row_scalar_expression *row_scalar_expression,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node *alias
) {
    struct planned_grouped_projection *projections = NULL;
    size_t required_count = plan->projection_count + 1U;

    if (required_count > SIZE_MAX / sizeof(*projections)) {
        planned_row_scalar_expression_deinit(row_scalar_expression);
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    projections = realloc(plan->projections, required_count * sizeof(*projections));
    if (projections == NULL) {
        planned_row_scalar_expression_deinit(row_scalar_expression);
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    plan->projections = projections;
    plan->projections[plan->projection_count] = (struct planned_grouped_projection){
        .expression = expression,
        .alias = alias,
        .uses_row_scalar_expression = true,
        .row_scalar_expression = *row_scalar_expression,
    };
    *row_scalar_expression = (struct planned_row_scalar_expression){0};
    plan->projection_count = required_count;
    return MYLITE_OK;
}

static int plan_grouped_aggregate_temporal_projection(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node *alias,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
) {
    struct planned_row_scalar_expression row_scalar_expression = {0};
    int rc = plan_grouped_temporal_row_scalar_expression(
        database,
        expression,
        source_context,
        table_columns,
        table_column_count,
        &row_scalar_expression
    );

    if (rc == MYLITE_OK &&
        !grouped_row_scalar_projection_is_allowed(out_plan, &row_scalar_expression)) {
        set_unsupported_error(
            database,
            "GROUP BY supports selected WEEK(), YEAR(), MONTH(), and DAYOFMONTH() expressions only "
            "when grouped"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = append_grouped_projection_row_scalar_expression(
            database,
            out_plan,
            &row_scalar_expression,
            expression,
            alias
        );
    }
    planned_row_scalar_expression_deinit(&row_scalar_expression);
    return rc;
}

static int plan_grouped_aggregate_literal_projection(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node *alias,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
) {
    struct planned_row_scalar_expression row_scalar_expression = {0};
    int rc = plan_row_scalar_expression(
        database,
        expression,
        true,
        source_context,
        NULL,
        table_columns,
        table_column_count,
        &row_scalar_expression
    );

    if (rc == MYLITE_OK) {
        rc = append_grouped_projection_row_scalar_expression(
            database,
            out_plan,
            &row_scalar_expression,
            expression,
            alias
        );
    }
    planned_row_scalar_expression_deinit(&row_scalar_expression);
    return rc;
}

static int plan_grouped_aggregate_integer_arithmetic_projection(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node *alias,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
) {
    struct planned_row_scalar_expression row_scalar_expression = {0};
    int rc = plan_row_scalar_expression(
        database,
        expression,
        true,
        source_context,
        NULL,
        table_columns,
        table_column_count,
        &row_scalar_expression
    );

    if (rc == MYLITE_OK) {
        rc = validate_grouped_projection_row_scalar_expression(
            database,
            out_plan,
            table_columns,
            table_column_count,
            &row_scalar_expression
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_grouped_projection_row_scalar_expression(
            database,
            out_plan,
            &row_scalar_expression,
            expression,
            alias
        );
    }
    planned_row_scalar_expression_deinit(&row_scalar_expression);
    return rc;
}

static int validate_grouped_projection_row_scalar_expression(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *plan,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    const struct planned_row_scalar_expression *expression
) {
    if (expression == NULL) {
        set_unsupported_error(
            database,
            "GROUP BY supports scalar projections only over grouped or literal operands"
        );
        return MYLITE_ERROR;
    }

    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return validate_grouped_projection_row_scalar_column(
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
                "GROUP BY supports scalar projections only over grouped or literal operands"
            );
            return MYLITE_ERROR;
        }
        for (size_t argument_index = 0U; argument_index < expression->argument_count;
             ++argument_index) {
            int rc = validate_grouped_projection_row_scalar_expression(
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
            "GROUP BY supports scalar projections only over grouped or literal operands"
        );
        return MYLITE_ERROR;
    }
}

static int validate_grouped_projection_row_scalar_column(
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
            &expression->column,
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
        "SELECT list",
        planned_grouped_aggregate_source_resolution(plan, source_index),
        &expression->column
    );
    return MYLITE_ERROR;
}

static int plan_grouped_aggregate_relaxed_row_scalar_projection(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node *alias,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_grouped_aggregate *out_plan
) {
    struct planned_row_scalar_expression row_scalar_expression = {0};
    int rc = plan_row_scalar_expression(
        database,
        expression,
        true,
        source_context,
        NULL,
        table_columns,
        table_column_count,
        &row_scalar_expression
    );

    if (rc == MYLITE_OK &&
        !grouped_relaxed_row_scalar_projection_is_allowed(out_plan, &row_scalar_expression)) {
        set_unsupported_error(
            database,
            "relaxed GROUP BY supports selected DATE_FORMAT() only for weekly archive grouping"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = append_grouped_projection_row_scalar_expression(
            database,
            out_plan,
            &row_scalar_expression,
            expression,
            alias
        );
    }
    planned_row_scalar_expression_deinit(&row_scalar_expression);
    return rc;
}

static bool grouped_expression_is_relaxed_row_scalar_projection(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *expression
) {
    expression = unwrap_parenthesized_expression(expression);
    return !session_sql_mode_has(&database->session, MYLITE_SESSION_SQL_MODE_ONLY_FULL_GROUP_BY) &&
           expression != NULL && expression->kind == MYLITE_SQL_AST_DATE_FORMAT_FUNCTION;
}

static bool grouped_relaxed_row_scalar_projection_is_allowed(
    const struct planned_grouped_aggregate *plan,
    const struct planned_row_scalar_expression *expression
) {
    static const char archive_day_format[] = "%Y-%m-%d";
    const struct planned_row_scalar_expression *value_argument = NULL;
    const struct planned_row_scalar_expression *format_argument = NULL;

    if (plan == NULL || expression == NULL ||
        expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT ||
        expression->argument_count != 3U || expression->arguments == NULL) {
        return false;
    }

    value_argument = &expression->arguments[0];
    format_argument = &expression->arguments[1];
    if (value_argument->kind != PLANNED_ROW_SCALAR_EXPRESSION_COLUMN ||
        format_argument->kind != PLANNED_ROW_SCALAR_EXPRESSION_VALUE ||
        format_argument->value.is_null || !format_argument->value.is_text ||
        format_argument->value.text_length != sizeof(archive_day_format) - 1U ||
        memcmp(format_argument->value.text, archive_day_format, sizeof(archive_day_format) - 1U) !=
            0) {
        return false;
    }

    return grouped_plan_has_temporal_key_on_column(
               plan,
               MYLITE_TEMPORAL_EXTRACT_WEEK,
               value_argument
           ) &&
           grouped_plan_has_temporal_key_on_column(
               plan,
               MYLITE_TEMPORAL_EXTRACT_YEAR,
               value_argument
           );
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

            if (!joined_source_matches_qualified_wildcard_parts(
                    source,
                    (const char(*)[MYLITE_CATALOG_IDENTIFIER_CAPACITY])parts,
                    part_count
                )) {
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
            (const char(*)[MYLITE_CATALOG_IDENTIFIER_CAPACITY])parts,
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

    if (!session_sql_mode_has(&database->session, MYLITE_SESSION_SQL_MODE_ONLY_FULL_GROUP_BY)) {
        return MYLITE_OK;
    }
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
    struct primary_key_info primary_key = mylite_execution_primary_key_info_init();
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

    rc = mylite_execution_load_primary_key_info(
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

    mylite_execution_primary_key_info_deinit(&primary_key);
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

static bool grouped_row_scalar_projection_is_allowed(
    const struct planned_grouped_aggregate *plan,
    const struct planned_row_scalar_expression *expression
) {
    for (size_t group_index = 0U; group_index < plan->group_count; ++group_index) {
        const struct planned_grouped_key *group = &plan->groups[group_index];

        if (group->uses_row_scalar_expression && planned_grouped_row_scalar_expressions_match(
                                                     expression,
                                                     &group->row_scalar_expression
                                                 )) {
            return true;
        }
    }
    return false;
}

static bool planned_grouped_row_scalar_expressions_match(
    const struct planned_row_scalar_expression *left,
    const struct planned_row_scalar_expression *right
) {
    const struct planned_row_scalar_expression *left_argument = NULL;
    const struct planned_row_scalar_expression *right_argument = NULL;

    if (left == NULL || right == NULL ||
        left->kind != PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT ||
        right->kind != PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT ||
        left->temporal_extract_kind != right->temporal_extract_kind ||
        left->argument_count != right->argument_count || left->argument_count == 0U ||
        right->argument_count == 0U || left->arguments == NULL || right->arguments == NULL) {
        return false;
    }

    left_argument = &left->arguments[0];
    right_argument = &right->arguments[0];
    if (left_argument->kind != PLANNED_ROW_SCALAR_EXPRESSION_COLUMN ||
        right_argument->kind != PLANNED_ROW_SCALAR_EXPRESSION_COLUMN) {
        return false;
    }

    return planned_grouped_row_scalar_columns_match(left_argument, right_argument) &&
           (left->argument_count <= 3U || planned_grouped_row_scalar_value_arguments_match(
                                              &left->arguments[3],
                                              &right->arguments[3]
                                          ));
}

static bool grouped_plan_has_temporal_key_on_column(
    const struct planned_grouped_aggregate *plan,
    enum mylite_temporal_extract_kind temporal_extract_kind,
    const struct planned_row_scalar_expression *column_argument
) {
    for (size_t group_index = 0U; plan != NULL && group_index < plan->group_count; ++group_index) {
        const struct planned_grouped_key *group = &plan->groups[group_index];
        const struct planned_row_scalar_expression *expression = &group->row_scalar_expression;

        if (!group->uses_row_scalar_expression ||
            expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT ||
            expression->temporal_extract_kind != temporal_extract_kind ||
            expression->argument_count < 1U || expression->arguments == NULL ||
            !planned_grouped_row_scalar_columns_match(column_argument, &expression->arguments[0])) {
            continue;
        }
        if (temporal_extract_kind == MYLITE_TEMPORAL_EXTRACT_WEEK &&
            !planned_grouped_temporal_key_uses_week_mode_one(expression)) {
            continue;
        }
        return true;
    }
    return false;
}

static bool planned_grouped_temporal_key_uses_week_mode_one(
    const struct planned_row_scalar_expression *expression
) {
    const struct planned_row_scalar_expression *mode = NULL;

    if (expression == NULL || expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT ||
        expression->temporal_extract_kind != MYLITE_TEMPORAL_EXTRACT_WEEK ||
        expression->argument_count <= 3U || expression->arguments == NULL) {
        return false;
    }

    mode = &expression->arguments[3];
    return mode->kind == PLANNED_ROW_SCALAR_EXPRESSION_VALUE && !mode->value.is_null &&
           !mode->value.is_text && !mode->value.is_blob && !mode->value.is_real &&
           mode->value.integer == 1;
}

static bool planned_grouped_row_scalar_columns_match(
    const struct planned_row_scalar_expression *left,
    const struct planned_row_scalar_expression *right
) {
    if (left == NULL || right == NULL || left->kind != PLANNED_ROW_SCALAR_EXPRESSION_COLUMN ||
        right->kind != PLANNED_ROW_SCALAR_EXPRESSION_COLUMN) {
        return false;
    }

    return left->column.column_id == right->column.column_id &&
           left->column_has_source_index == right->column_has_source_index &&
           (!left->column_has_source_index ||
            left->column_source_index == right->column_source_index);
}

static bool planned_grouped_row_scalar_value_arguments_match(
    const struct planned_row_scalar_expression *left,
    const struct planned_row_scalar_expression *right
) {
    if (left->kind != PLANNED_ROW_SCALAR_EXPRESSION_VALUE ||
        right->kind != PLANNED_ROW_SCALAR_EXPRESSION_VALUE ||
        left->value.is_null != right->value.is_null ||
        left->value.is_text != right->value.is_text ||
        left->value.is_blob != right->value.is_blob ||
        left->value.is_real != right->value.is_real) {
        return false;
    }
    if (left->value.is_null) {
        return true;
    }
    if (left->value.is_text || left->value.is_blob) {
        return left->value.text_length == right->value.text_length &&
               (left->value.text_length == 0U ||
                memcmp(left->value.text, right->value.text, left->value.text_length) == 0);
    }
    if (left->value.is_real) {
        return left->value.real == right->value.real;
    }
    return left->value.integer == right->value.integer;
}
