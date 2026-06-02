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
        rc = mylite_execution_load_table_columns(database, out_plan->table.table_id, out_columns, out_column_count);
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
