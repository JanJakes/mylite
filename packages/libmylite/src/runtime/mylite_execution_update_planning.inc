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
