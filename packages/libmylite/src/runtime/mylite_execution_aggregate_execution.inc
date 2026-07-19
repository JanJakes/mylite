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
        if (count_function_from_expression(expression) != PLANNED_COUNT_NONE &&
            !aggregate_function_has_window_clause(expression)) {
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
    if (rc == MYLITE_OK && plan->calc_found_rows) {
        rc = set_grouped_aggregate_found_row_count(database, plan, result);
    }
    if (rc == MYLITE_OK && plan->calc_found_rows) {
        rc = append_sql_calc_found_rows_deprecation_warning(database);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return grouped_aggregate_execution_error(database, rc);
    }

    return finish_successful_result(database, result, out_result);
}

static int set_grouped_aggregate_found_row_count(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *plan,
    mylite_result *result
) {
    int64_t count = 0;
    int rc = read_grouped_aggregate_found_row_count(database, plan, &count);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (count < 0) {
        set_runtime_error(database, "invalid SQL_CALC_FOUND_ROWS group count");
        return MYLITE_ERROR;
    }
    mylite_result_set_found_row_count(result, (uint64_t)count);
    return MYLITE_OK;
}

static int read_grouped_aggregate_found_row_count(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *plan,
    int64_t *out_count
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int rc = build_grouped_aggregate_found_rows_sql(plan, &sql);

    if (rc == MYLITE_OK) {
        rc = prepare_cached_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_grouped_aggregate_count_parameters(statement, plan);
    }
    if (rc == MYLITE_OK) {
        rc = step_count_statement(statement, out_count);
    }
    rc = finish_cached_sqlite_statement(database, statement, rc);
    free(sql);
    return rc;
}

static int append_grouped_aggregate_result_columns(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_grouped_aggregate *plan
) {
    int rc = MYLITE_OK;

    for (size_t projection_index = 0U; rc == MYLITE_OK && projection_index < plan->projection_count;
         ++projection_index) {
        const struct planned_grouped_projection *projection = &plan->projections[projection_index];
        rc = mylite_result_append_column(result, projection->result_label);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }

    for (size_t aggregate_index = 0U; rc == MYLITE_OK && aggregate_index < plan->aggregate_count;
         ++aggregate_index) {
        const struct planned_grouped_aggregate_item *item = &plan->aggregates[aggregate_index];

        if (item->is_hidden_order_key) {
            continue;
        }
        if (rc == MYLITE_OK && item->function == PLANNED_GROUPED_AGGREGATE_ST_COLLECT) {
            rc = append_spatial_collect_result_column(
                database,
                result,
                item->result_label,
                item->has_spatial_wrapper,
                item->spatial_wrapper_kind
            );
        } else if (rc == MYLITE_OK) {
            rc = mylite_result_append_column(result, item->result_label);
            if (rc != MYLITE_OK) {
                set_nomem_error(database);
            }
        }
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
    const struct mylite_sql_ast_node *from_clause = select_statement_source_clause(statement);
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *order_clause = NULL;
    const struct mylite_sql_ast_node *limit_clause = NULL;
    const struct mylite_sql_ast_node *optional_clause = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    const struct mylite_sql_ast_node *count_expression = NULL;
    const struct mylite_sql_ast_node *alias = NULL;
    struct planned_count_source_nodes source_nodes = {0};
    int rc = MYLITE_OK;

    *out_plan = (struct planned_count){false};
    out_plan->calc_found_rows = mylite_sql_ast_node_select_calc_found_rows(statement) != 0;
    expression = child_at(select_item, 0U);
    alias = child_at(select_item, 1U);
    count_expression = unwrap_parenthesized_expression(expression);
    out_plan->function = count_function_from_expression(count_expression);
    out_plan->count_is_distinct = aggregate_function_is_distinct(count_expression);
    if (mylite_sql_ast_node_child_count(select_list) != 1U) {
        set_unsupported_error(database, count_exactly_one_message(out_plan->function));
        return MYLITE_ERROR;
    }
    if (out_plan->function == PLANNED_COUNT_NONE) {
        set_unsupported_error(database, count_exactly_one_message(out_plan->function));
        return MYLITE_ERROR;
    }
    rc = copy_planned_aggregate_result_label(
        database,
        &(const struct aggregate_result_label_source){.expression = expression, .alias = alias},
        &out_plan->result_label
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_plan->function == PLANNED_COUNT_LITERAL) {
        rc = plan_count_literal(database, count_expression, &out_plan->count_literal);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    optional_clause = select_statement_optional_clause_start(statement);
    while (optional_clause != NULL) {
        if (optional_clause->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
            where_clause = optional_clause;
        } else if (optional_clause->kind == MYLITE_SQL_AST_ORDER_BY_CLAUSE) {
            order_clause = optional_clause;
        } else if (optional_clause->kind == MYLITE_SQL_AST_LIMIT_CLAUSE) {
            limit_clause = optional_clause;
        } else if (optional_clause->kind == MYLITE_SQL_AST_SELECT_INTO_LIST) {
            /* SELECT ... INTO is handled after normal SELECT execution. */
        } else {
            set_unsupported_error(database, count_supported_clauses_message(out_plan->function));
            return MYLITE_ERROR;
        }
        optional_clause = optional_clause->next_sibling;
    }
    rc = plan_select_limit(database, limit_clause, &out_plan->limit);
    if (rc != MYLITE_OK) {
        return rc;
    }

    source_nodes.from_clause = from_clause;
    source_nodes.where_clause = where_clause;
    source_nodes.order_clause = order_clause;
    source_nodes.count_expression = count_expression;
    if (from_clause == NULL || from_clause->kind == MYLITE_SQL_AST_FROM_DUAL) {
        return plan_count_without_source(database, &source_nodes, out_plan);
    }
    if (from_clause->kind == MYLITE_SQL_AST_FROM_DERIVED) {
        return plan_count_derived_table_source(database, &source_nodes, out_plan);
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

    if (plan->source_kind == PLANNED_COUNT_SOURCE_DERIVED_SELECT) {
        planned_select_deinit(&plan->derived_select);
    } else if (plan->source_kind == PLANNED_COUNT_SOURCE_DERIVED_ROW_SCALAR) {
        planned_row_scalar_select_deinit(&plan->derived_row_scalar_select);
    } else if (plan->source_kind == PLANNED_COUNT_SOURCE_DERIVED_GROUPED_AGGREGATE) {
        planned_grouped_aggregate_deinit(plan->derived_grouped_aggregate);
        free(plan->derived_grouped_aggregate);
    } else if (plan->source_kind == PLANNED_COUNT_SOURCE_DERIVED_COMPOUND) {
        planned_count_derived_compound_deinit(&plan->derived_compound);
    }
    planned_select_predicate_deinit(&plan->predicate);
    planned_value_deinit(&plan->count_literal);
    free(plan->result_label);
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
    if (nodes->order_clause != NULL) {
        set_unsupported_error(database, count_supported_clauses_message(out_plan->function));
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int plan_count_derived_table_source(
    struct mylite_db *database,
    const struct planned_count_source_nodes *nodes,
    struct planned_count *out_plan
) {
    const struct mylite_sql_ast_node *source = nodes->from_clause;
    const struct mylite_sql_ast_node *statement = NULL;

    if (out_plan->function == PLANNED_COUNT_COLUMN ||
        out_plan->function == PLANNED_COUNT_DISTINCT_COLUMN) {
        set_unsupported_error(database, count_descriptor_table_message(out_plan->function));
        return MYLITE_ERROR;
    }
    if (nodes->where_clause != NULL || nodes->order_clause != NULL) {
        set_unsupported_error(database, "COUNT derived table supports only inner query clauses");
        return MYLITE_ERROR;
    }
    if (source == NULL || source->kind != MYLITE_SQL_AST_FROM_DERIVED) {
        set_unsupported_error(database, "COUNT derived table requires an inner query");
        return MYLITE_ERROR;
    }
    statement = child_at(source, 0U);
    if (statement == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    out_plan->has_source = true;
    if (statement->kind == MYLITE_SQL_AST_SELECT_STATEMENT) {
        return plan_count_derived_select_statement(database, statement, out_plan);
    }
    if (statement->kind == MYLITE_SQL_AST_COMPOUND_SELECT_STATEMENT) {
        return plan_count_derived_compound_statement(database, statement, out_plan);
    }

    set_unsupported_error(database, "COUNT derived table supports only SELECT and UNION sources");
    return MYLITE_ERROR;
}

static int plan_count_derived_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_count *out_plan
) {
    if (select_statement_has_group_by_clause(statement)) {
        out_plan->source_kind = PLANNED_COUNT_SOURCE_DERIVED_GROUPED_AGGREGATE;
        out_plan->derived_grouped_aggregate =
            calloc(1U, sizeof(*out_plan->derived_grouped_aggregate));
        if (out_plan->derived_grouped_aggregate == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        return plan_grouped_aggregate(database, statement, out_plan->derived_grouped_aggregate);
    }
    if (select_statement_is_row_scalar_projection_attempt(statement)) {
        out_plan->source_kind = PLANNED_COUNT_SOURCE_DERIVED_ROW_SCALAR;
        return plan_row_scalar_select(
            database,
            statement,
            false,
            &out_plan->derived_row_scalar_select
        );
    }

    out_plan->source_kind = PLANNED_COUNT_SOURCE_DERIVED_SELECT;
    return plan_select(database, statement, false, &out_plan->derived_select);
}

static int plan_count_derived_compound_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_count *out_plan
) {
    const struct mylite_sql_ast_node *terms = child_at(statement, 1U);
    const struct mylite_sql_ast_node *term = NULL;
    const struct mylite_sql_ast_node *branch = NULL;
    size_t branch_index = 0U;
    int rc = MYLITE_OK;

    if (terms == NULL || terms->kind != MYLITE_SQL_AST_UNION_TERM_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    out_plan->source_kind = PLANNED_COUNT_SOURCE_DERIVED_COMPOUND;
    out_plan->derived_compound.branch_count = compound_select_branch_count(statement);
    if (out_plan->derived_compound.branch_count == 0U ||
        out_plan->derived_compound.branch_count >
            SIZE_MAX / sizeof(*out_plan->derived_compound.branches)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_plan->derived_compound.branches = calloc(
        out_plan->derived_compound.branch_count,
        sizeof(*out_plan->derived_compound.branches)
    );
    if (out_plan->derived_compound.branches == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    branch = child_at(statement, 0U);
    rc = plan_count_derived_compound_branch(
        database,
        branch,
        &out_plan->derived_compound.branches[branch_index]
    );
    term = child_at(terms, 0U);
    while (rc == MYLITE_OK && term != NULL) {
        ++branch_index;
        branch = child_at(term, 0U);
        if (branch_index >= out_plan->derived_compound.branch_count) {
            set_parse_error(database, NULL);
            rc = MYLITE_ERROR;
            break;
        }
        out_plan->derived_compound.branches[branch_index].modifier =
            mylite_sql_ast_node_union_modifier(term);
        rc = plan_count_derived_compound_branch(
            database,
            branch,
            &out_plan->derived_compound.branches[branch_index]
        );
        term = term->next_sibling;
    }
    if (rc == MYLITE_OK && branch_index + 1U != out_plan->derived_compound.branch_count) {
        set_parse_error(database, NULL);
        rc = MYLITE_ERROR;
    }
    if (rc != MYLITE_OK) {
        planned_count_derived_compound_deinit(&out_plan->derived_compound);
    }

    return rc;
}

static int plan_count_derived_compound_branch(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *branch,
    struct planned_count_derived_compound_branch *out_branch
) {
    if (branch == NULL || branch->kind != MYLITE_SQL_AST_SELECT_STATEMENT) {
        set_unsupported_error(database, "COUNT UNION derived table supports only SELECT branches");
        return MYLITE_ERROR;
    }
    if (select_statement_has_group_by_clause(branch)) {
        out_branch->kind = PLANNED_COUNT_SOURCE_DERIVED_GROUPED_AGGREGATE;
        out_branch->grouped_aggregate = calloc(1U, sizeof(*out_branch->grouped_aggregate));
        if (out_branch->grouped_aggregate == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        return plan_grouped_aggregate(database, branch, out_branch->grouped_aggregate);
    }
    if (select_statement_is_row_scalar_projection_attempt(branch)) {
        out_branch->kind = PLANNED_COUNT_SOURCE_DERIVED_ROW_SCALAR;
        return plan_row_scalar_select(database, branch, false, &out_branch->row_scalar_select);
    }

    out_branch->kind = PLANNED_COUNT_SOURCE_DERIVED_SELECT;
    return plan_select(database, branch, false, &out_branch->select);
}

static void planned_count_derived_compound_deinit(struct planned_count_derived_compound *compound) {
    if (compound == NULL) {
        return;
    }
    for (size_t branch_index = 0U; branch_index < compound->branch_count; ++branch_index) {
        planned_count_derived_compound_branch_deinit(&compound->branches[branch_index]);
    }
    free(compound->branches);
    *compound = (struct planned_count_derived_compound){0};
}

static void planned_count_derived_compound_branch_deinit(
    struct planned_count_derived_compound_branch *branch
) {
    if (branch == NULL) {
        return;
    }
    if (branch->kind == PLANNED_COUNT_SOURCE_DERIVED_SELECT) {
        planned_select_deinit(&branch->select);
    } else if (branch->kind == PLANNED_COUNT_SOURCE_DERIVED_ROW_SCALAR) {
        planned_row_scalar_select_deinit(&branch->row_scalar_select);
    } else if (branch->kind == PLANNED_COUNT_SOURCE_DERIVED_GROUPED_AGGREGATE) {
        planned_grouped_aggregate_deinit(branch->grouped_aggregate);
        free(branch->grouped_aggregate);
    }
    *branch = (struct planned_count_derived_compound_branch){0};
}

static int plan_count_table_source(
    struct mylite_db *database,
    const struct planned_count_source_nodes *nodes,
    struct planned_count *out_plan
) {
    struct mylite_catalog_column_descriptor *table_columns = NULL;
    struct loaded_table_columns_cache_entry *table_columns_cache_entry = NULL;
    struct select_source_context source_context = {0};
    size_t table_column_count = 0U;
    int rc = MYLITE_OK;

    out_plan->has_source = true;
    out_plan->source_kind = PLANNED_COUNT_SOURCE_TABLE;
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
    if (rc == MYLITE_OK && (nodes->where_clause != NULL || nodes->order_clause != NULL ||
                            out_plan->function == PLANNED_COUNT_COLUMN ||
                            out_plan->function == PLANNED_COUNT_DISTINCT_COLUMN)) {
        rc = mylite_execution_acquire_table_columns(
            database,
            out_plan->table.table_id,
            &table_columns,
            &table_column_count,
            &table_columns_cache_entry
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
        rc = validate_count_distinct_column_descriptor(database, &out_plan->count_column);
    }
    if (rc == MYLITE_OK) {
        rc = validate_count_order_clause(
            database,
            nodes->order_clause,
            &source_context,
            table_columns,
            table_column_count
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

    mylite_execution_release_table_columns(table_columns_cache_entry, table_columns);
    return rc;
}

static int validate_count_order_clause(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_clause,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count
) {
    const struct mylite_sql_ast_node *order_items = NULL;
    const struct mylite_sql_ast_node *item = NULL;

    if (order_clause == NULL) {
        return MYLITE_OK;
    }
    if (order_clause->kind != MYLITE_SQL_AST_ORDER_BY_CLAUSE) {
        set_unsupported_error(
            database,
            "COUNT aggregate supports only descriptor ORDER BY columns"
        );
        return MYLITE_ERROR;
    }

    order_items = child_at(order_clause, 0U);
    if (order_items != NULL && order_items->kind == MYLITE_SQL_AST_ORDER_BY_ITEM_LIST) {
        item = child_at(order_items, 0U);
        while (item != NULL) {
            if (item->kind != MYLITE_SQL_AST_ORDER_BY_ITEM) {
                set_unsupported_error(
                    database,
                    "COUNT aggregate supports only descriptor ORDER BY columns"
                );
                return MYLITE_ERROR;
            }
            int rc = validate_count_order_item(
                database,
                child_at(item, 0U),
                source_context,
                table_columns,
                table_column_count
            );

            if (rc != MYLITE_OK) {
                return rc;
            }
            item = item->next_sibling;
        }
        return MYLITE_OK;
    }

    return validate_count_order_item(
        database,
        child_at(order_clause, 0U),
        source_context,
        table_columns,
        table_column_count
    );
}

static int validate_count_order_item(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *order_key,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count
) {
    struct mylite_catalog_column_descriptor column = {0};
    size_t source_index = 0U;

    if (order_key != NULL && order_key->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(order_key) == MYLITE_SQL_AST_LITERAL_INTEGER) {
        return MYLITE_OK;
    }
    return resolve_descriptor_column_reference_with_source_index(
        database,
        order_key,
        source_context,
        COLUMN_REFERENCE_ORDER,
        "COUNT aggregate supports only descriptor ORDER BY columns",
        table_columns,
        table_column_count,
        &column,
        &source_index
    );
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
    if (rc == MYLITE_OK && plan->calc_found_rows) {
        mylite_result_set_found_row_count(result, 1U);
    }
    if (rc == MYLITE_OK) {
        apply_count_limit_to_result(result, plan);
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

static void apply_count_limit_to_result(mylite_result *result, const struct planned_count *plan) {
    if (result == NULL || plan == NULL || !plan->limit.has_limit) {
        return;
    }
    if (plan->limit.row_count == 0 || (plan->limit.has_offset && plan->limit.offset > 0)) {
        mylite_result_truncate_rows(result, 0U);
    }
}

static int append_count_result_column(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_count *plan
) {
    int rc = mylite_result_append_column(result, plan->result_label);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }
    return rc;
}

static int copy_planned_aggregate_result_label(
    struct mylite_db *database,
    const struct aggregate_result_label_source *source,
    char **out_label
) {
    if (source == NULL) {
        return MYLITE_MISUSE;
    }
    if (source->alias != NULL) {
        return copy_select_item_alias_text(database, source->alias, out_label);
    }
    return copy_aggregate_result_column_name(
        database,
        source->expression == NULL ? NULL : &source->expression->span,
        out_label
    );
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
        rc = prepare_cached_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_count_parameters(statement, plan);
    }
    if (rc == MYLITE_OK) {
        rc = step_count_statement(statement, out_count);
    }

    rc = finish_cached_sqlite_statement(database, statement, rc);
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
        const struct mylite_sql_ast_node *aggregate_expression = NULL;
        const struct mylite_sql_ast_node *rhs_expression = NULL;
        enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;

        expression = unwrap_parenthesized_expression(expression);
        if (column_aggregate_function_from_projection_expression(
                expression,
                &aggregate_expression,
                &operator_kind,
                &rhs_expression
            ) != PLANNED_COLUMN_AGGREGATE_NONE &&
            !aggregate_function_has_window_clause(aggregate_expression)) {
            return true;
        }
        (void)aggregate_expression;
        (void)rhs_expression;
        (void)operator_kind;
        select_item = select_item->next_sibling;
    }

    return false;
}

static bool select_statement_has_multi_count_aggregate(
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *from_clause = select_statement_source_clause(statement);
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *select_item = NULL;
    bool has_count = false;

    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST ||
        mylite_sql_ast_node_child_count(select_list) <= 1U) {
        return false;
    }
    if (from_clause != NULL && from_clause->kind == MYLITE_SQL_AST_FROM_JOIN) {
        return false;
    }

    select_item = child_at(select_list, 0U);
    while (select_item != NULL) {
        const struct mylite_sql_ast_node *expression = child_at(select_item, 0U);

        expression = unwrap_parenthesized_expression(expression);
        if (expression != NULL && expression->kind == MYLITE_SQL_AST_COUNT_EXPRESSION_FUNCTION &&
            !aggregate_function_has_window_clause(expression)) {
            return false;
        }
        if (count_function_from_expression(expression) != PLANNED_COUNT_NONE &&
            !aggregate_function_has_window_clause(expression)) {
            has_count = true;
        }
        select_item = select_item->next_sibling;
    }

    return has_count;
}

static int plan_column_aggregate_select(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_column_aggregate_select *out_plan
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *select_item = NULL;
    const struct mylite_sql_ast_node *from_clause = select_statement_source_clause(statement);
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *limit_clause = NULL;
    const struct mylite_sql_ast_node *optional_clause = NULL;
    struct mylite_catalog_column_descriptor *table_columns = NULL;
    struct select_source_context source_context = {0};
    size_t table_column_count = 0U;
    size_t select_count = 0U;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_column_aggregate_select){0};
    if (select_list == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    select_count = mylite_sql_ast_node_child_count(select_list);
    if (select_count == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (select_count > SIZE_MAX / sizeof(*out_plan->items)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    out_plan->items = calloc(select_count, sizeof(*out_plan->items));
    if (out_plan->items == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_plan->item_count = select_count;

    select_item = child_at(select_list, 0U);
    if (select_item == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    optional_clause = select_statement_optional_clause_start(statement);
    while (optional_clause != NULL) {
        if (optional_clause->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
            where_clause = optional_clause;
        } else if (optional_clause->kind == MYLITE_SQL_AST_LIMIT_CLAUSE) {
            limit_clause = optional_clause;
        } else if (optional_clause->kind == MYLITE_SQL_AST_SELECT_INTO_LIST) {
            /* SELECT ... INTO is handled after normal SELECT execution. */
        } else {
            set_unsupported_error(database, "aggregate SELECT supports only WHERE and LIMIT");
            return MYLITE_ERROR;
        }
        optional_clause = optional_clause->next_sibling;
    }
    rc = plan_select_limit(database, limit_clause, &out_plan->limit);
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (from_clause == NULL || from_clause->kind == MYLITE_SQL_AST_FROM_DUAL) {
        for (size_t item_index = 0U; rc == MYLITE_OK && item_index < out_plan->item_count;
             ++item_index) {
            const struct column_aggregate_select_item_plan_input input = {
                .select_item = select_item,
                .from_clause = from_clause,
                .where_clause = where_clause,
                .source_context = NULL,
                .table = NULL,
                .table_columns = NULL,
                .table_column_count = 0U
            };

            rc = plan_column_aggregate_select_item(database, &input, &out_plan->items[item_index]);
            select_item = select_item->next_sibling;
        }
        return rc;
    }
    if (from_clause->kind != MYLITE_SQL_AST_FROM_TABLE) {
        set_unsupported_error(
            database,
            "aggregate SELECT supports only descriptor-backed table reads"
        );
        return MYLITE_ERROR;
    }
    out_plan->has_source = true;

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
        rc = mylite_execution_load_table_columns(
            database,
            out_plan->table.table_id,
            &table_columns,
            &table_column_count
        );
    }
    if (rc == MYLITE_OK) {
        for (size_t item_index = 0U; rc == MYLITE_OK && item_index < out_plan->item_count;
             ++item_index) {
            const struct column_aggregate_select_item_plan_input input = {
                .select_item = select_item,
                .from_clause = from_clause,
                .where_clause = where_clause,
                .source_context = &source_context,
                .table = &out_plan->table,
                .table_columns = table_columns,
                .table_column_count = table_column_count
            };

            rc = plan_column_aggregate_select_item(database, &input, &out_plan->items[item_index]);
            select_item = select_item->next_sibling;
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

static int plan_column_aggregate_select_item(
    struct mylite_db *database,
    const struct column_aggregate_select_item_plan_input *input,
    struct planned_column_aggregate *out_item
) {
    const struct mylite_sql_ast_node *select_item = NULL;
    const struct mylite_sql_ast_node *from_clause = NULL;
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    const struct mylite_sql_ast_node *source_expression = NULL;
    const struct mylite_sql_ast_node *alias = NULL;
    const struct mylite_sql_ast_node *aggregate_expression = NULL;
    const struct mylite_sql_ast_node *rhs_expression = NULL;
    enum mylite_sql_ast_operator projection_operator = MYLITE_SQL_AST_OPERATOR_NONE;
    struct mylite_catalog_column_descriptor aggregate_column = {0};
    int rc = MYLITE_OK;

    *out_item = (struct planned_column_aggregate){0};
    if (input == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    select_item = input->select_item;
    from_clause = input->from_clause;
    where_clause = input->where_clause;
    if (select_item == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    source_expression = child_at(select_item, 0U);
    alias = child_at(select_item, 1U);
    expression = unwrap_parenthesized_expression(source_expression);
    if (spatial_collect_wrapper_from_expression(
            expression,
            &out_item->spatial_wrapper_kind,
            NULL
        )) {
        out_item->has_spatial_wrapper = true;
    }
    out_item->function = column_or_count_aggregate_function_from_projection_expression(
        expression,
        &aggregate_expression,
        &projection_operator,
        &rhs_expression
    );
    if (out_item->function == PLANNED_COLUMN_AGGREGATE_NONE) {
        set_unsupported_error(database, "aggregate SELECT supports only aggregate select items");
        return MYLITE_ERROR;
    }
    rc = copy_planned_aggregate_result_label(
        database,
        &(const struct aggregate_result_label_source){
            .expression = source_expression,
            .alias = alias,
        },
        &out_item->result_label
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    out_item->aggregate_is_distinct = aggregate_function_is_distinct(aggregate_expression);

    if (out_item->function == PLANNED_COLUMN_AGGREGATE_COUNT_LITERAL) {
        rc = plan_count_literal(database, aggregate_expression, &out_item->count_literal);
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    if (column_aggregate_function_is_count(out_item->function)) {
        if (from_clause == NULL || from_clause->kind == MYLITE_SQL_AST_FROM_DUAL) {
            if (out_item->function == PLANNED_COLUMN_AGGREGATE_COUNT_COLUMN ||
                out_item->function == PLANNED_COLUMN_AGGREGATE_COUNT_DISTINCT_COLUMN) {
                const struct mylite_sql_ast_node *column_node = child_at(aggregate_expression, 0U);
                char column_name[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
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
            if (where_clause != NULL) {
                if (out_item->function == PLANNED_COLUMN_AGGREGATE_COUNT_LITERAL) {
                    set_unsupported_error(
                        database,
                        "COUNT(literal) WHERE requires a descriptor-backed table"
                    );
                } else {
                    set_unsupported_error(
                        database,
                        "COUNT(*) WHERE requires a descriptor-backed table"
                    );
                }
                return MYLITE_ERROR;
            }
            return MYLITE_OK;
        }
        if (out_item->function == PLANNED_COLUMN_AGGREGATE_COUNT_COLUMN ||
            out_item->function == PLANNED_COLUMN_AGGREGATE_COUNT_DISTINCT_COLUMN) {
            rc = plan_count_column(
                database,
                aggregate_expression,
                input->source_context,
                input->table_columns,
                input->table_column_count,
                &aggregate_column
            );
        }
        if (rc == MYLITE_OK &&
            out_item->function == PLANNED_COLUMN_AGGREGATE_COUNT_DISTINCT_COLUMN) {
            rc = validate_count_distinct_column_descriptor(database, &aggregate_column);
        }
        if (rc == MYLITE_OK &&
            (out_item->function == PLANNED_COLUMN_AGGREGATE_COUNT_COLUMN ||
             out_item->function == PLANNED_COLUMN_AGGREGATE_COUNT_DISTINCT_COLUMN)) {
            rc = copy_planned_column_descriptor(
                database,
                &aggregate_column,
                &out_item->aggregate_column
            );
        }
        return rc;
    }

    if (from_clause == NULL || from_clause->kind == MYLITE_SQL_AST_FROM_DUAL) {
        rc = plan_column_aggregate_without_source(
            database,
            aggregate_expression,
            out_item->function,
            where_clause,
            out_item
        );
        if (rc == MYLITE_OK && rhs_expression != NULL) {
            rc = plan_row_scalar_expression(
                database,
                rhs_expression,
                false,
                NULL,
                NULL,
                NULL,
                0U,
                &out_item->aggregate_projection_rhs
            );
            if (rc == MYLITE_OK) {
                out_item->aggregate_has_projection_rhs = true;
                out_item->aggregate_projection_operator = projection_operator;
            }
        }
        return rc;
    }

    if (out_item->function == PLANNED_COLUMN_AGGREGATE_GROUP_CONCAT) {
        rc = plan_column_group_concat_options(
            database,
            expression,
            input->source_context,
            input->table,
            input->table_columns,
            input->table_column_count,
            out_item
        );
    } else if (column_aggregate_function_is_json(out_item->function)) {
        rc = plan_column_json_aggregate_options(
            database,
            aggregate_expression,
            out_item->function,
            true,
            input->source_context,
            input->table_columns,
            input->table_column_count,
            out_item
        );
    } else if (
        out_item->function != PLANNED_COLUMN_AGGREGATE_ANY_VALUE &&
        out_item->function != PLANNED_COLUMN_AGGREGATE_ST_COLLECT &&
        column_aggregate_argument_is_row_scalar_expression(aggregate_expression)
    ) {
        rc = plan_column_aggregate_row_scalar_expression(
            database,
            aggregate_expression,
            input->source_context,
            input->table,
            input->table_columns,
            input->table_column_count,
            out_item
        );
    } else {
        rc = plan_column_aggregate_column(
            database,
            aggregate_expression,
            out_item->function,
            input->source_context,
            input->table_columns,
            input->table_column_count,
            &aggregate_column
        );
        if (rc == MYLITE_OK) {
            rc = copy_planned_column_descriptor(
                database,
                &aggregate_column,
                &out_item->aggregate_column
            );
        }
    }
    if (rc == MYLITE_OK && rhs_expression != NULL) {
        rc = plan_row_scalar_expression(
            database,
            rhs_expression,
            false,
            NULL,
            NULL,
            NULL,
            0U,
            &out_item->aggregate_projection_rhs
        );
        if (rc == MYLITE_OK) {
            out_item->aggregate_has_projection_rhs = true;
            out_item->aggregate_projection_operator = projection_operator;
        }
    }
    return rc;
}

static int plan_column_aggregate_without_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *aggregate_expression,
    enum planned_column_aggregate_function function,
    const struct mylite_sql_ast_node *where_clause,
    struct planned_column_aggregate *out_plan
) {
    const struct mylite_sql_ast_node *argument = child_at(aggregate_expression, 0U);
    int rc = MYLITE_OK;

    if (where_clause != NULL || argument == NULL ||
        (!column_aggregate_function_is_json(function) && function != PLANNED_COLUMN_AGGREGATE_MIN &&
         function != PLANNED_COLUMN_AGGREGATE_MAX &&
         !column_aggregate_function_is_statistical(function)) ||
        (!column_aggregate_function_is_json(function) &&
         !column_aggregate_argument_is_row_scalar_expression(aggregate_expression))) {
        set_unsupported_error(database, column_aggregate_source_error(function));
        return MYLITE_ERROR;
    }
    if (column_aggregate_function_is_json(function)) {
        if (where_clause != NULL) {
            set_unsupported_error(database, column_aggregate_source_error(function));
            return MYLITE_ERROR;
        }
        return plan_column_json_aggregate_options(
            database,
            aggregate_expression,
            function,
            false,
            NULL,
            NULL,
            0U,
            out_plan
        );
    }

    rc = plan_row_scalar_expression(
        database,
        argument,
        false,
        NULL,
        NULL,
        NULL,
        0U,
        &out_plan->aggregate_row_scalar_expression
    );
    if (rc == MYLITE_OK) {
        out_plan->aggregate_uses_row_scalar_expression = true;
    }
    return rc;
}

static void planned_column_aggregate_select_deinit(struct planned_column_aggregate_select *plan) {
    if (plan == NULL) {
        return;
    }

    for (size_t item_index = 0U; item_index < plan->item_count; ++item_index) {
        planned_column_aggregate_deinit(&plan->items[item_index]);
    }
    free(plan->items);
    planned_select_predicate_deinit(&plan->predicate);
    *plan = (struct planned_column_aggregate_select){0};
}

static void planned_column_aggregate_deinit(struct planned_column_aggregate *plan) {
    if (plan == NULL) {
        return;
    }

    free(plan->aggregate_column);
    planned_select_predicate_deinit(&plan->predicate);
    planned_select_order_deinit(&plan->aggregate_order);
    planned_value_deinit(&plan->separator);
    planned_value_deinit(&plan->count_literal);
    planned_row_scalar_expression_deinit(&plan->aggregate_key_row_scalar_expression);
    planned_row_scalar_expression_deinit(&plan->aggregate_row_scalar_expression);
    planned_row_scalar_expression_deinit(&plan->aggregate_projection_rhs);
    free(plan->result_label);
    *plan = (struct planned_column_aggregate){0};
}

static enum planned_column_aggregate_function column_or_count_aggregate_function_from_expression(
    const struct mylite_sql_ast_node *expression
) {
    enum planned_count_function count_function = PLANNED_COUNT_NONE;
    enum planned_column_aggregate_function column_function =
        column_aggregate_function_from_expression(expression);

    if (column_function != PLANNED_COLUMN_AGGREGATE_NONE) {
        return column_function;
    }
    if (expression != NULL && expression->kind == MYLITE_SQL_AST_ANY_VALUE_FUNCTION) {
        return PLANNED_COLUMN_AGGREGATE_ANY_VALUE;
    }

    count_function = count_function_from_expression(expression);
    if (count_function == PLANNED_COUNT_STAR) {
        return PLANNED_COLUMN_AGGREGATE_COUNT_STAR;
    }
    if (count_function == PLANNED_COUNT_COLUMN) {
        return PLANNED_COLUMN_AGGREGATE_COUNT_COLUMN;
    }
    if (count_function == PLANNED_COUNT_LITERAL) {
        return PLANNED_COLUMN_AGGREGATE_COUNT_LITERAL;
    }
    if (count_function == PLANNED_COUNT_DISTINCT_COLUMN) {
        return PLANNED_COLUMN_AGGREGATE_COUNT_DISTINCT_COLUMN;
    }
    return PLANNED_COLUMN_AGGREGATE_NONE;
}

static enum planned_column_aggregate_function column_or_count_aggregate_function_from_projection_expression(
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node **out_aggregate_expression,
    enum mylite_sql_ast_operator *out_operator,
    const struct mylite_sql_ast_node **out_rhs_expression
) {
    enum planned_column_aggregate_function function =
        column_aggregate_function_from_projection_expression(
            expression,
            out_aggregate_expression,
            out_operator,
            out_rhs_expression
        );

    if (function != PLANNED_COLUMN_AGGREGATE_NONE) {
        return function;
    }

    if (out_aggregate_expression != NULL) {
        *out_aggregate_expression = NULL;
    }
    if (out_rhs_expression != NULL) {
        *out_rhs_expression = NULL;
    }
    if (out_operator != NULL) {
        *out_operator = MYLITE_SQL_AST_OPERATOR_NONE;
    }

    expression = unwrap_parenthesized_expression(expression);
    function = column_or_count_aggregate_function_from_expression(expression);
    if (function != PLANNED_COLUMN_AGGREGATE_NONE && out_aggregate_expression != NULL) {
        *out_aggregate_expression = expression;
    }
    return function;
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
    if (expression->kind == MYLITE_SQL_AST_STD_AGGREGATE_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_STDDEV_AGGREGATE_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_STDDEV_POP_AGGREGATE_FUNCTION) {
        return PLANNED_COLUMN_AGGREGATE_STDDEV_POP;
    }
    if (expression->kind == MYLITE_SQL_AST_STDDEV_SAMP_AGGREGATE_FUNCTION) {
        return PLANNED_COLUMN_AGGREGATE_STDDEV_SAMP;
    }
    if (expression->kind == MYLITE_SQL_AST_VAR_POP_AGGREGATE_FUNCTION ||
        expression->kind == MYLITE_SQL_AST_VARIANCE_AGGREGATE_FUNCTION) {
        return PLANNED_COLUMN_AGGREGATE_VAR_POP;
    }
    if (expression->kind == MYLITE_SQL_AST_VAR_SAMP_AGGREGATE_FUNCTION) {
        return PLANNED_COLUMN_AGGREGATE_VAR_SAMP;
    }
    if (expression->kind == MYLITE_SQL_AST_ST_COLLECT_AGGREGATE_FUNCTION) {
        return PLANNED_COLUMN_AGGREGATE_ST_COLLECT;
    }
    if (generic_function_name_equals(expression, "JSON_ARRAYAGG")) {
        return PLANNED_COLUMN_AGGREGATE_JSON_ARRAYAGG;
    }
    if (generic_function_name_equals(expression, "JSON_OBJECTAGG")) {
        return PLANNED_COLUMN_AGGREGATE_JSON_OBJECTAGG;
    }

    return PLANNED_COLUMN_AGGREGATE_NONE;
}

static bool spatial_collect_wrapper_from_expression(
    const struct mylite_sql_ast_node *expression,
    enum mylite_spatial_function_kind *out_kind,
    const struct mylite_sql_ast_node **out_collect_expression
) {
    const struct mylite_sql_ast_node *arguments = NULL;
    const struct mylite_sql_ast_node *argument = NULL;
    enum mylite_spatial_function_kind kind = MYLITE_SPATIAL_FUNCTION_NONE;

    if (out_kind != NULL) {
        *out_kind = MYLITE_SPATIAL_FUNCTION_NONE;
    }
    if (out_collect_expression != NULL) {
        *out_collect_expression = NULL;
    }
    expression = unwrap_parenthesized_expression(expression);
    if (!generic_spatial_function_kind(expression, &kind)) {
        return false;
    }
    if (kind != MYLITE_SPATIAL_FUNCTION_ST_ASTEXT && kind != MYLITE_SPATIAL_FUNCTION_ST_ASWKT &&
        kind != MYLITE_SPATIAL_FUNCTION_ST_GEOMETRYTYPE &&
        kind != MYLITE_SPATIAL_FUNCTION_ST_SRID) {
        return false;
    }
    arguments = generic_system_function_arguments(expression);
    if (arguments == NULL || mylite_sql_ast_node_child_count(arguments) != 1U) {
        return false;
    }
    argument = unwrap_parenthesized_expression(child_at(arguments, 0U));
    if (argument == NULL || argument->kind != MYLITE_SQL_AST_ST_COLLECT_AGGREGATE_FUNCTION) {
        return false;
    }
    if (out_kind != NULL) {
        *out_kind = kind;
    }
    if (out_collect_expression != NULL) {
        *out_collect_expression = argument;
    }
    return true;
}

static enum planned_column_aggregate_function column_aggregate_function_from_projection_expression(
    const struct mylite_sql_ast_node *expression,
    const struct mylite_sql_ast_node **out_aggregate_expression,
    enum mylite_sql_ast_operator *out_operator,
    const struct mylite_sql_ast_node **out_rhs_expression
) {
    enum planned_column_aggregate_function function = PLANNED_COLUMN_AGGREGATE_NONE;
    enum mylite_sql_ast_operator operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;
    const struct mylite_sql_ast_node *left = NULL;
    const struct mylite_sql_ast_node *right = NULL;

    if (out_aggregate_expression != NULL) {
        *out_aggregate_expression = NULL;
    }
    if (out_rhs_expression != NULL) {
        *out_rhs_expression = NULL;
    }
    if (out_operator != NULL) {
        *out_operator = MYLITE_SQL_AST_OPERATOR_NONE;
    }
    expression = unwrap_parenthesized_expression(expression);
    if (spatial_collect_wrapper_from_expression(expression, NULL, out_aggregate_expression)) {
        return PLANNED_COLUMN_AGGREGATE_ST_COLLECT;
    }
    function = column_aggregate_function_from_expression(expression);
    if (function != PLANNED_COLUMN_AGGREGATE_NONE) {
        if (out_aggregate_expression != NULL) {
            *out_aggregate_expression = expression;
        }
        return function;
    }
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_BINARY_EXPRESSION) {
        return PLANNED_COLUMN_AGGREGATE_NONE;
    }

    operator_kind = mylite_sql_ast_node_operator(expression);
    if (operator_kind != MYLITE_SQL_AST_OPERATOR_ADD &&
        operator_kind != MYLITE_SQL_AST_OPERATOR_SUBTRACT) {
        return PLANNED_COLUMN_AGGREGATE_NONE;
    }
    left = unwrap_parenthesized_expression(child_at(expression, 0U));
    right = unwrap_parenthesized_expression(child_at(expression, 1U));
    function = column_aggregate_function_from_expression(left);
    if (function == PLANNED_COLUMN_AGGREGATE_NONE ||
        (function != PLANNED_COLUMN_AGGREGATE_MIN && function != PLANNED_COLUMN_AGGREGATE_MAX &&
         function != PLANNED_COLUMN_AGGREGATE_SUM)) {
        return PLANNED_COLUMN_AGGREGATE_NONE;
    }
    if (out_aggregate_expression != NULL) {
        *out_aggregate_expression = left;
    }
    if (out_rhs_expression != NULL) {
        *out_rhs_expression = right;
    }
    if (out_operator != NULL) {
        *out_operator = operator_kind;
    }
    return function;
}

static bool column_aggregate_argument_is_row_scalar_expression(
    const struct mylite_sql_ast_node *function
) {
    return row_scalar_expression_is_aggregate_argument_context_attempt(function);
}

static bool aggregate_function_is_distinct(const struct mylite_sql_ast_node *function) {
    const struct mylite_sql_ast_node *child = child_at(function, 1U);

    while (child != NULL) {
        if (child->kind == MYLITE_SQL_AST_AGGREGATE_DISTINCT_MODIFIER) {
            return true;
        }
        child = child->next_sibling;
    }
    return false;
}

static bool column_aggregate_function_is_count(enum planned_column_aggregate_function function) {
    return (function == PLANNED_COLUMN_AGGREGATE_COUNT_STAR ||
            function == PLANNED_COLUMN_AGGREGATE_COUNT_COLUMN ||
            function == PLANNED_COLUMN_AGGREGATE_COUNT_LITERAL ||
            function == PLANNED_COLUMN_AGGREGATE_COUNT_DISTINCT_COLUMN) != 0;
}

static bool column_aggregate_function_is_bitwise(enum planned_column_aggregate_function function) {
    return (function == PLANNED_COLUMN_AGGREGATE_BIT_AND ||
            function == PLANNED_COLUMN_AGGREGATE_BIT_OR ||
            function == PLANNED_COLUMN_AGGREGATE_BIT_XOR) != 0;
}

static bool column_aggregate_function_is_min_max(enum planned_column_aggregate_function function) {
    return (function == PLANNED_COLUMN_AGGREGATE_MIN || function == PLANNED_COLUMN_AGGREGATE_MAX) !=
           0;
}

static bool column_aggregate_function_is_statistical(
    enum planned_column_aggregate_function function
) {
    return (function == PLANNED_COLUMN_AGGREGATE_STDDEV_POP ||
            function == PLANNED_COLUMN_AGGREGATE_STDDEV_SAMP ||
            function == PLANNED_COLUMN_AGGREGATE_VAR_POP ||
            function == PLANNED_COLUMN_AGGREGATE_VAR_SAMP) != 0;
}

static bool column_aggregate_function_is_json(enum planned_column_aggregate_function function) {
    return (function == PLANNED_COLUMN_AGGREGATE_JSON_ARRAYAGG ||
            function == PLANNED_COLUMN_AGGREGATE_JSON_OBJECTAGG) != 0;
}

static const char *column_aggregate_source_error(enum planned_column_aggregate_function function) {
    if (column_aggregate_function_is_bitwise(function)) {
        return "BIT_AND/BIT_OR/BIT_XOR(column) supports only descriptor-backed table reads";
    }
    if (function == PLANNED_COLUMN_AGGREGATE_GROUP_CONCAT) {
        return "GROUP_CONCAT(column) supports only descriptor-backed table reads";
    }
    if (function == PLANNED_COLUMN_AGGREGATE_ANY_VALUE) {
        return "ANY_VALUE(column) supports only descriptor-backed table reads";
    }
    if (function == PLANNED_COLUMN_AGGREGATE_AVG) {
        return "AVG(column) supports only descriptor-backed table reads";
    }
    if (function == PLANNED_COLUMN_AGGREGATE_SUM) {
        return "SUM(column) supports only descriptor-backed table reads";
    }
    if (column_aggregate_function_is_statistical(function)) {
        return "statistical aggregate supports only descriptor-backed table reads";
    }
    if (function == PLANNED_COLUMN_AGGREGATE_ST_COLLECT) {
        return "ST_Collect(column) supports only descriptor-backed table reads";
    }
    if (column_aggregate_function_is_json(function)) {
        return "JSON aggregates with WHERE require descriptor-backed table reads";
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
    if (function == PLANNED_COLUMN_AGGREGATE_ANY_VALUE) {
        return "ANY_VALUE(column) supports only descriptor columns";
    }
    if (function == PLANNED_COLUMN_AGGREGATE_AVG) {
        return "AVG(column) supports only descriptor columns";
    }
    if (function == PLANNED_COLUMN_AGGREGATE_SUM) {
        return "SUM(column) supports only descriptor columns";
    }
    if (column_aggregate_function_is_statistical(function)) {
        return "statistical aggregate supports only descriptor columns";
    }
    if (function == PLANNED_COLUMN_AGGREGATE_ST_COLLECT) {
        return "ST_Collect(column) supports only descriptor columns";
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
    if (column_aggregate_function_is_statistical(function)) {
        return "statistical aggregate supports only integer descriptor columns";
    }
    if (function == PLANNED_COLUMN_AGGREGATE_ST_COLLECT) {
        return "ST_Collect(column) supports only spatial descriptor columns";
    }

    return "MIN/MAX supports only integer and nonbinary string descriptor columns";
}

static int validate_column_aggregate_descriptor_column(
    struct mylite_db *database,
    enum planned_column_aggregate_function aggregate_function,
    const struct mylite_catalog_column_descriptor *column
) {
    struct integer_column_range range = {0};

    if (aggregate_function == PLANNED_COLUMN_AGGREGATE_ANY_VALUE) {
        return MYLITE_OK;
    }
    if (column_aggregate_function_is_min_max(aggregate_function) &&
        column_descriptor_is_string_family(column)) {
        return MYLITE_OK;
    }
    if (aggregate_function == PLANNED_COLUMN_AGGREGATE_ST_COLLECT &&
        column_descriptor_is_spatial(column)) {
        return MYLITE_OK;
    }
    return integer_range_for_column(
        database,
        column,
        column_aggregate_integer_error(aggregate_function),
        &range
    );
}

static int validate_count_distinct_column_descriptor(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column
) {
    struct integer_column_range range = {0};

    if (column_descriptor_uses_string_key_collation(column, true) ||
        column_descriptor_is_binary_string_family(column)) {
        return MYLITE_OK;
    }
    return integer_range_for_column(
        database,
        column,
        "COUNT(DISTINCT column) supports only integer, string, and binary string descriptor "
        "columns",
        &range
    );
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
    rc = validate_column_aggregate_descriptor_column(database, aggregate_function, out_column);
    if (rc != MYLITE_OK) {
        return rc;
    }

    return MYLITE_OK;
}

static int plan_column_aggregate_row_scalar_expression(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    const struct select_source_context *source_context,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_column_aggregate *out_plan
) {
    const struct mylite_sql_ast_node *argument = child_at(function, 0U);
    int rc = MYLITE_OK;

    if (row_scalar_expression_contains_fractional_division_attempt(argument)) {
        set_unsupported_error(database, "aggregate row-scalar arguments do not support / division");
        return MYLITE_ERROR;
    }
    rc = plan_row_scalar_expression(
        database,
        argument,
        true,
        source_context,
        table,
        table_columns,
        table_column_count,
        &out_plan->aggregate_row_scalar_expression
    );
    if (rc == MYLITE_OK) {
        out_plan->aggregate_uses_row_scalar_expression = true;
    }
    return rc;
}

static int plan_column_group_concat_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    const struct select_source_context *source_context,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_column_aggregate *out_plan
) {
    struct mylite_catalog_column_descriptor aggregate_column = {0};
    size_t value_source_index = 0U;
    int rc = MYLITE_OK;

    function = unwrap_parenthesized_expression(function);
    if (function == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    out_plan->aggregate_is_distinct = group_concat_is_distinct(function);
    if (column_aggregate_argument_is_row_scalar_expression(function)) {
        rc = plan_column_aggregate_row_scalar_expression(
            database,
            function,
            source_context,
            table,
            table_columns,
            table_column_count,
            out_plan
        );
    } else {
        rc = plan_group_concat_value_column(
            database,
            function,
            source_context,
            table_columns,
            table_column_count,
            &aggregate_column,
            &value_source_index
        );
        if (rc == MYLITE_OK) {
            rc = copy_planned_column_descriptor(
                database,
                &aggregate_column,
                &out_plan->aggregate_column
            );
        }
    }

    if (rc == MYLITE_OK) {
        rc = populate_group_concat_collation_name(
            database,
            source_context,
            table,
            out_plan->aggregate_uses_row_scalar_expression ? NULL : out_plan->aggregate_column,
            value_source_index,
            out_plan->aggregate_uses_row_scalar_expression
                ? &out_plan->aggregate_row_scalar_expression
                : NULL,
            out_plan->aggregate_collation_name
        );
    }
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

static bool aggregate_function_has_window_clause(const struct mylite_sql_ast_node *function) {
    const struct mylite_sql_ast_node *child = NULL;

    if (function == NULL) {
        return false;
    }
    child = function->first_child;
    while (child != NULL) {
        if (child->kind == MYLITE_SQL_AST_WINDOW_SPEC ||
            child->kind == MYLITE_SQL_AST_WINDOW_REFERENCE) {
            return true;
        }
        child = child->next_sibling;
    }
    return false;
}

static const char *column_json_aggregate_function_name(
    enum planned_column_aggregate_function function
) {
    if (function == PLANNED_COLUMN_AGGREGATE_JSON_ARRAYAGG) {
        return "JSON_ARRAYAGG";
    }
    if (function == PLANNED_COLUMN_AGGREGATE_JSON_OBJECTAGG) {
        return "JSON_OBJECTAGG";
    }
    return "JSON aggregate";
}

static int plan_column_json_aggregate_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *function,
    enum planned_column_aggregate_function aggregate_function,
    bool has_source,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_column_aggregate *out_plan
) {
    const struct mylite_sql_ast_node *arguments = generic_system_function_arguments(function);
    const struct mylite_sql_ast_node *value = NULL;
    size_t expected_argument_count =
        aggregate_function == PLANNED_COLUMN_AGGREGATE_JSON_OBJECTAGG ? 2U : 1U;
    int rc = MYLITE_OK;

    if (aggregate_function_has_window_clause(function)) {
        set_unsupported_error(database, "JSON aggregate window functions are not yet supported");
        return MYLITE_ERROR;
    }
    if (arguments == NULL ||
        mylite_sql_ast_node_child_count(arguments) != expected_argument_count) {
        set_native_function_parameter_count_error(
            database,
            column_json_aggregate_function_name(aggregate_function)
        );
        return MYLITE_ERROR;
    }

    if (aggregate_function == PLANNED_COLUMN_AGGREGATE_JSON_OBJECTAGG) {
        rc = plan_row_scalar_json_constructor_argument(
            database,
            child_at(arguments, 0U),
            true,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            COLUMN_REFERENCE_FIELD,
            &out_plan->aggregate_key_row_scalar_expression
        );
        value = child_at(arguments, 1U);
    } else {
        value = child_at(arguments, 0U);
    }
    if (rc == MYLITE_OK) {
        rc = plan_row_scalar_json_constructor_argument(
            database,
            value,
            false,
            has_source,
            source_context,
            table_columns,
            table_column_count,
            COLUMN_REFERENCE_FIELD,
            &out_plan->aggregate_row_scalar_expression
        );
    }
    if (rc == MYLITE_OK) {
        out_plan->aggregate_uses_row_scalar_expression = true;
    }
    return rc;
}

static int execute_column_aggregate_from_plan(
    struct mylite_db *database,
    const struct planned_column_aggregate_select *plan,
    bool apply_sql_select_limit,
    mylite_result **out_result
) {
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    for (size_t item_index = 0U; rc == MYLITE_OK && item_index < plan->item_count; ++item_index) {
        rc = append_column_aggregate_result_column(database, result, &plan->items[item_index]);
    }
    if (rc == MYLITE_OK) {
        rc = read_column_aggregate_from_source(database, plan, result);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return column_aggregate_execution_error(database, rc);
    }
    apply_column_aggregate_limit_to_result(result, plan);
    if (apply_sql_select_limit) {
        apply_sql_select_limit_to_result(database, result);
    }

    return finish_successful_result(database, result, out_result);
}

static void apply_column_aggregate_limit_to_result(
    mylite_result *result,
    const struct planned_column_aggregate_select *plan
) {
    if (result == NULL || plan == NULL || !plan->limit.has_limit) {
        return;
    }
    if (plan->limit.row_count == 0 || (plan->limit.has_offset && plan->limit.offset > 0)) {
        mylite_result_truncate_rows(result, 0U);
    }
}

static int append_column_aggregate_result_column(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_column_aggregate *plan
) {
    int rc = MYLITE_OK;

    if (rc == MYLITE_OK && plan->function == PLANNED_COLUMN_AGGREGATE_ST_COLLECT) {
        rc = append_spatial_collect_result_column(
            database,
            result,
            plan->result_label,
            plan->has_spatial_wrapper,
            plan->spatial_wrapper_kind
        );
    } else if (rc == MYLITE_OK) {
        rc = mylite_result_append_column(result, plan->result_label);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }

    return rc;
}

static int append_spatial_collect_result_column(
    struct mylite_db *database,
    mylite_result *result,
    const char *column_name,
    bool has_wrapper,
    enum mylite_spatial_function_kind wrapper_kind
) {
    struct mylite_result_column_descriptor descriptor = {
        .label = column_name,
        .schema_name = "",
        .table_name = "",
        .origin_schema_name = "",
        .origin_table_name = "",
        .origin_column_name = "",
        .logical_type = MYLITE_RESULT_LOGICAL_TYPE_GEOMETRY,
        .flags = MYLITE_RESULT_COLUMN_FLAG_BLOB | MYLITE_RESULT_COLUMN_FLAG_BINARY,
        .charset_id = mysql_collation_binary_id,
        .collation_id = mysql_collation_binary_id,
        .display_length = mysql_json_document_display_length,
        .decimals = mysql_approximate_decimals,
        .nullable = true,
    };
    int rc = MYLITE_OK;

    if (has_wrapper) {
        rc =
            populate_spatial_function_result_column_descriptor(database, wrapper_kind, &descriptor);
        descriptor.label = column_name;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_column_descriptor(result, &descriptor);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    return rc;
}

static int read_column_aggregate_from_source(
    struct mylite_db *database,
    const struct planned_column_aggregate_select *plan,
    mylite_result *result
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int rc = build_column_aggregate_sql(plan, &sql);

    if (rc == MYLITE_OK) {
        rc = prepare_cached_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_column_aggregate_parameters(statement, plan);
    }
    if (rc == MYLITE_OK) {
        for (size_t item_index = 0U; item_index < plan->item_count; ++item_index) {
            if (plan->items[item_index].function == PLANNED_COLUMN_AGGREGATE_GROUP_CONCAT) {
                database->session.group_concat_value_ordinal = 0U;
                break;
            }
        }
    }
    if (rc == MYLITE_OK) {
        rc = step_column_aggregate_statement(database, statement, plan, result);
    }

    rc = finish_cached_sqlite_statement(database, statement, rc);
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
    size_t initial_row_count = mylite_result_row_count(result);
    int sqlite_rc = SQLITE_OK;
    int rc = build_grouped_aggregate_sql(plan, &sql);

    if (rc == MYLITE_OK) {
        rc = prepare_cached_sqlite_statement(database, sql, &statement);
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

    rc = finish_cached_sqlite_statement(database, statement, rc);
    free(sql);
    if (rc == MYLITE_OK && plan->with_rollup &&
        mylite_result_row_count(result) > initial_row_count) {
        rc = append_grouped_aggregate_rollup_row(database, plan, result);
    }

    return rc;
}

static int append_grouped_aggregate_rollup_row(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *plan,
    mylite_result *result
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    size_t visible_aggregate_count = 0U;
    int sqlite_rc = SQLITE_OK;
    int rc = MYLITE_OK;

    for (size_t aggregate_index = 0U; aggregate_index < plan->aggregate_count; ++aggregate_index) {
        if (!plan->aggregates[aggregate_index].is_hidden_order_key) {
            ++visible_aggregate_count;
        }
    }
    if (visible_aggregate_count == 0U) {
        return append_grouped_aggregate_null_rollup_row(database, plan, result);
    }

    rc = build_grouped_aggregate_rollup_sql(plan, &sql);
    if (rc == MYLITE_OK) {
        rc = prepare_cached_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_grouped_aggregate_rollup_parameters(statement, plan);
    }
    if (rc == MYLITE_OK) {
        for (size_t index = 0U; index < plan->aggregate_count; ++index) {
            if (plan->aggregates[index].function == PLANNED_GROUPED_AGGREGATE_GROUP_CONCAT) {
                database->session.group_concat_value_ordinal = 0U;
                break;
            }
        }
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = append_grouped_aggregate_rollup_sqlite_row(database, statement, plan, result);
        } else {
            rc = grouped_aggregate_step_error(database, statement, plan, sqlite_rc);
        }
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc != SQLITE_DONE) {
            rc = grouped_aggregate_step_error(database, statement, plan, sqlite_rc);
        }
    }

    rc = finish_cached_sqlite_statement(database, statement, rc);
    free(sql);
    return rc;
}

static int append_grouped_aggregate_null_rollup_row(
    struct mylite_db *database,
    const struct planned_grouped_aggregate *plan,
    mylite_result *result
) {
    struct mylite_result_cell *cells = NULL;
    size_t result_column_count = plan->projection_count;
    int rc = MYLITE_OK;

    if (result_column_count == 0U) {
        return MYLITE_ERROR;
    }
    if (result_column_count > SIZE_MAX / sizeof(*cells)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    cells = calloc(result_column_count, sizeof(*cells));
    if (cells == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    for (size_t column_index = 0U; column_index < result_column_count; ++column_index) {
        append_grouped_aggregate_rollup_projection_cell(plan, column_index, &cells[column_index]);
    }
    rc = mylite_result_append_bytes_row(result, cells);
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    free(cells);
    return rc;
}

static int step_column_aggregate_statement(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_column_aggregate_select *plan,
    mylite_result *result
) {
    int sqlite_rc = sqlite3_step(statement);
    int rc = MYLITE_OK;

    if (sqlite_rc != SQLITE_ROW) {
        return column_aggregate_select_step_error(database, statement, plan, sqlite_rc);
    }

    rc = append_column_aggregate_sqlite_row(database, statement, plan, result);
    if (rc != MYLITE_OK) {
        return rc;
    }
    sqlite_rc = sqlite3_step(statement);
    if (sqlite_rc != SQLITE_DONE) {
        return column_aggregate_select_step_error(database, statement, plan, sqlite_rc);
    }

    return MYLITE_OK;
}

static int append_column_aggregate_sqlite_row(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_column_aggregate_select *plan,
    mylite_result *result
) {
    struct mylite_result_cell *cells = NULL;
    char (*buffers)[approximate_numeric_text_capacity] = NULL;
    int sqlite_column_index = 0;
    int rc = MYLITE_OK;

    if (plan->item_count > (size_t)INT_MAX) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (plan->item_count > SIZE_MAX / sizeof(*cells) ||
        plan->item_count > SIZE_MAX / sizeof(*buffers)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    cells = calloc(plan->item_count, sizeof(*cells));
    buffers = calloc(plan->item_count, sizeof(*buffers));
    if (cells == NULL || buffers == NULL) {
        free(cells);
        free(buffers);
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    for (size_t item_index = 0U; rc == MYLITE_OK && item_index < plan->item_count; ++item_index) {
        rc = append_column_aggregate_sqlite_cell(
            database,
            statement,
            &plan->items[item_index],
            &sqlite_column_index,
            &cells[item_index],
            buffers[item_index],
            sizeof(buffers[item_index])
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_bytes_row(result, cells);
    }
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }

    free(cells);
    free(buffers);
    return rc;
}

static int append_column_aggregate_sqlite_cell(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_column_aggregate *item,
    int *sqlite_column_index,
    struct mylite_result_cell *out_cell,
    char *buffer,
    size_t buffer_size
) {
    int rc = MYLITE_OK;

    if (item->function == PLANNED_COLUMN_AGGREGATE_AVG) {
        return append_avg_sqlite_cell(
            database,
            statement,
            sqlite_column_index,
            out_cell,
            buffer,
            buffer_size
        );
    }
    if (column_aggregate_function_is_bitwise(item->function)) {
        rc = append_bitwise_aggregate_sqlite_cell(statement, *sqlite_column_index, out_cell);
        if (rc == MYLITE_OK) {
            ++(*sqlite_column_index);
        }
        return rc;
    }
    if (item->function == PLANNED_COLUMN_AGGREGATE_GROUP_CONCAT) {
        rc = append_grouped_group_concat_sqlite_cell(statement, *sqlite_column_index, out_cell);
        if (rc == MYLITE_OK) {
            ++(*sqlite_column_index);
        }
        return rc;
    }
    if (item->function == PLANNED_COLUMN_AGGREGATE_JSON_ARRAYAGG ||
        item->function == PLANNED_COLUMN_AGGREGATE_JSON_OBJECTAGG) {
        rc = append_grouped_group_concat_sqlite_cell(statement, *sqlite_column_index, out_cell);
        if (rc == MYLITE_OK) {
            ++(*sqlite_column_index);
        }
        return rc;
    }
    if (item->function == PLANNED_COLUMN_AGGREGATE_ST_COLLECT) {
        rc = mylite_execution_read_sqlite_result_cell(
            database,
            statement,
            (size_t)*sqlite_column_index,
            item->has_spatial_wrapper ? NULL : item->aggregate_column,
            buffer,
            buffer_size,
            out_cell
        );
        if (rc == MYLITE_OK) {
            ++(*sqlite_column_index);
        }
        return rc;
    }
    if (item->function == PLANNED_COLUMN_AGGREGATE_ANY_VALUE) {
        rc = mylite_execution_read_sqlite_result_cell(
            database,
            statement,
            (size_t)*sqlite_column_index,
            item->aggregate_column,
            buffer,
            buffer_size,
            out_cell
        );
        if (rc == MYLITE_OK) {
            ++(*sqlite_column_index);
        }
        return rc;
    }

    rc = mylite_execution_read_sqlite_result_cell(
        database,
        statement,
        (size_t)*sqlite_column_index,
        NULL,
        buffer,
        buffer_size,
        out_cell
    );
    if (rc == MYLITE_OK) {
        ++(*sqlite_column_index);
    }
    return rc;
}

static int append_avg_sqlite_cell(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int *sqlite_column_index,
    struct mylite_result_cell *out_cell,
    char *buffer,
    size_t buffer_size
) {
    int64_t count = 0;
    int rc = MYLITE_OK;

    *out_cell = (struct mylite_result_cell){.bytes = NULL, .size = 0U, .is_null = true};
    if (sqlite3_column_type(statement, *sqlite_column_index + 1) != SQLITE_INTEGER) {
        return MYLITE_ERROR;
    }

    count = (int64_t)sqlite3_column_int64(statement, *sqlite_column_index + 1);
    if (count == 0) {
        *sqlite_column_index += 2;
        return MYLITE_OK;
    }
    if (count < 0 || sqlite3_column_type(statement, *sqlite_column_index) != SQLITE_INTEGER) {
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

static int append_bitwise_aggregate_sqlite_cell(
    sqlite3_stmt *statement,
    int sqlite_column_index,
    struct mylite_result_cell *out_cell
) {
    const unsigned char *text = NULL;
    int byte_count = 0;

    if (sqlite3_column_type(statement, sqlite_column_index) != SQLITE_TEXT) {
        return MYLITE_ERROR;
    }
    text = sqlite3_column_text(statement, sqlite_column_index);
    byte_count = sqlite3_column_bytes(statement, sqlite_column_index);
    if (text == NULL || byte_count < 0) {
        return MYLITE_ERROR;
    }

    *out_cell = (struct mylite_result_cell){
        .bytes = text,
        .size = (size_t)byte_count,
        .is_null = false,
    };
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
    size_t visible_aggregate_count = 0U;
    size_t result_column_count = 0U;
    size_t result_aggregate_index = 0U;
    int rc = MYLITE_OK;

    for (size_t aggregate_index = 0U; aggregate_index < plan->aggregate_count; ++aggregate_index) {
        if (!plan->aggregates[aggregate_index].is_hidden_order_key) {
            ++visible_aggregate_count;
        }
    }
    result_column_count = plan->projection_count + visible_aggregate_count;
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
        if (plan->aggregates[aggregate_index].is_hidden_order_key) {
            continue;
        }
        rc = append_grouped_aggregate_sqlite_cell(
            database,
            statement,
            &plan->aggregates[aggregate_index],
            &sqlite_column_index,
            &cells[plan->projection_count + result_aggregate_index],
            aggregate_buffers[aggregate_index],
            sizeof(aggregate_buffers[aggregate_index])
        );
        if (rc == MYLITE_OK) {
            ++result_aggregate_index;
        }
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

static int append_grouped_aggregate_rollup_sqlite_row(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_grouped_aggregate *plan,
    mylite_result *result
) {
    struct mylite_result_cell *cells = NULL;
    char (*aggregate_buffers)[approximate_numeric_text_capacity] = NULL;
    int sqlite_column_index = 0;
    size_t visible_aggregate_count = 0U;
    size_t result_column_count = 0U;
    size_t result_aggregate_index = 0U;
    int rc = MYLITE_OK;

    for (size_t aggregate_index = 0U; aggregate_index < plan->aggregate_count; ++aggregate_index) {
        if (!plan->aggregates[aggregate_index].is_hidden_order_key) {
            ++visible_aggregate_count;
        }
    }
    result_column_count = plan->projection_count + visible_aggregate_count;
    if (result_column_count == 0U) {
        return MYLITE_ERROR;
    }
    if (result_column_count > SIZE_MAX / sizeof(*cells) ||
        plan->aggregate_count > SIZE_MAX / sizeof(*aggregate_buffers)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    cells = calloc(result_column_count, sizeof(*cells));
    if (plan->aggregate_count > 0U) {
        aggregate_buffers = calloc(plan->aggregate_count, sizeof(*aggregate_buffers));
    }
    if (cells == NULL || (plan->aggregate_count > 0U && aggregate_buffers == NULL)) {
        free(cells);
        free(aggregate_buffers);
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    for (size_t projection_index = 0U; projection_index < plan->projection_count;
         ++projection_index) {
        append_grouped_aggregate_rollup_projection_cell(
            plan,
            projection_index,
            &cells[projection_index]
        );
    }
    for (size_t aggregate_index = 0U; rc == MYLITE_OK && aggregate_index < plan->aggregate_count;
         ++aggregate_index) {
        if (plan->aggregates[aggregate_index].is_hidden_order_key) {
            continue;
        }
        rc = append_grouped_aggregate_sqlite_cell(
            database,
            statement,
            &plan->aggregates[aggregate_index],
            &sqlite_column_index,
            &cells[plan->projection_count + result_aggregate_index],
            aggregate_buffers[aggregate_index],
            sizeof(aggregate_buffers[aggregate_index])
        );
        if (rc == MYLITE_OK) {
            ++result_aggregate_index;
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_bytes_row(result, cells);
    }
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }

    free(cells);
    free(aggregate_buffers);

    return rc;
}

static void append_grouped_aggregate_rollup_projection_cell(
    const struct planned_grouped_aggregate *plan,
    size_t projection_index,
    struct mylite_result_cell *out_cell
) {
    static const unsigned char grouping_marker[] = "1";

    *out_cell = (struct mylite_result_cell){.is_null = true};
    if (projection_index >= plan->projection_count) {
        return;
    }
    if (plan->projections[projection_index].is_grouping_marker) {
        *out_cell = (struct mylite_result_cell){
            .bytes = grouping_marker,
            .size = sizeof(grouping_marker) - 1U,
            .is_null = false,
        };
    }
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
    if (plan->projections[projection_index].is_grouping_marker ||
        plan->projections[projection_index].uses_row_scalar_expression) {
        return mylite_execution_read_sqlite_result_cell(
            database,
            statement,
            projection_index,
            NULL,
            buffer,
            buffer_size,
            out_cell
        );
    }
    return mylite_execution_read_sqlite_result_cell(
        database,
        statement,
        projection_index,
        plan->projections[projection_index].column,
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
    if (item->function == PLANNED_GROUPED_AGGREGATE_JSON_ARRAYAGG ||
        item->function == PLANNED_GROUPED_AGGREGATE_JSON_OBJECTAGG) {
        rc = append_grouped_group_concat_sqlite_cell(statement, *sqlite_column_index, out_cell);
        ++(*sqlite_column_index);
        return rc;
    }
    if (item->function == PLANNED_GROUPED_AGGREGATE_ST_COLLECT) {
        rc = mylite_execution_read_sqlite_result_cell(
            database,
            statement,
            (size_t)*sqlite_column_index,
            item->has_spatial_wrapper ? NULL : item->aggregate_column,
            buffer,
            buffer_size,
            out_cell
        );
        if (rc == MYLITE_OK) {
            ++(*sqlite_column_index);
        }
        return rc;
    }
    if (item->function == PLANNED_GROUPED_AGGREGATE_ANY_VALUE) {
        rc = mylite_execution_read_sqlite_result_cell(
            database,
            statement,
            (size_t)*sqlite_column_index,
            item->aggregate_column,
            buffer,
            buffer_size,
            out_cell
        );
        ++(*sqlite_column_index);
        return rc;
    }
    if ((item->function == PLANNED_GROUPED_AGGREGATE_MIN ||
         item->function == PLANNED_GROUPED_AGGREGATE_MAX) &&
        column_descriptor_is_string_family(item->aggregate_column)) {
        rc = mylite_execution_read_sqlite_result_cell(
            database,
            statement,
            (size_t)*sqlite_column_index,
            item->aggregate_column,
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
    if (grouped_aggregate_function_is_statistical(item->function)) {
        rc = mylite_execution_read_sqlite_result_cell(
            database,
            statement,
            (size_t)*sqlite_column_index,
            NULL,
            buffer,
            buffer_size,
            out_cell
        );
        ++(*sqlite_column_index);
        return rc;
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

static int column_aggregate_select_step_error(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_column_aggregate_select *plan,
    int sqlite_rc
) {
    const char *message = NULL;
    bool has_sum = false;
    bool has_avg = false;
    bool has_json = false;

    for (size_t item_index = 0U; item_index < plan->item_count; ++item_index) {
        if (plan->items[item_index].function == PLANNED_COLUMN_AGGREGATE_SUM) {
            has_sum = true;
        }
        if (plan->items[item_index].function == PLANNED_COLUMN_AGGREGATE_AVG) {
            has_avg = true;
        }
        if (column_aggregate_function_is_json(plan->items[item_index].function)) {
            has_json = true;
        }
    }
    if (map_json_aggregate_step_error(database, statement, sqlite_rc, has_json) != MYLITE_OK) {
        return MYLITE_ERROR;
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

static int grouped_aggregate_step_error(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_grouped_aggregate *plan,
    int sqlite_rc
) {
    const char *message = NULL;
    bool has_sum = false;
    bool has_avg = false;
    bool has_json = false;

    for (size_t aggregate_index = 0U; aggregate_index < plan->aggregate_count; ++aggregate_index) {
        if (plan->aggregates[aggregate_index].function == PLANNED_GROUPED_AGGREGATE_SUM) {
            has_sum = true;
        }
        if (plan->aggregates[aggregate_index].function == PLANNED_GROUPED_AGGREGATE_AVG) {
            has_avg = true;
        }
        if (plan->aggregates[aggregate_index].function == PLANNED_GROUPED_AGGREGATE_JSON_ARRAYAGG ||
            plan->aggregates[aggregate_index].function ==
                PLANNED_GROUPED_AGGREGATE_JSON_OBJECTAGG) {
            has_json = true;
        }
    }
    if (map_json_aggregate_step_error(database, statement, sqlite_rc, has_json) != MYLITE_OK) {
        return MYLITE_ERROR;
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

static int map_json_aggregate_step_error(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int sqlite_rc,
    bool has_json_aggregate
) {
    const char *message = NULL;

    if (!has_json_aggregate || sqlite_rc != SQLITE_ERROR) {
        return MYLITE_OK;
    }

    message = sqlite3_errmsg(sqlite3_db_handle(statement));
    if (message == NULL) {
        return MYLITE_OK;
    }
    if (strcmp(message, "JSON documents may not contain NULL member names.") == 0) {
        set_json_null_member_name_error(database);
        return MYLITE_ERROR;
    }
    if (strcmp(message, "Invalid JSON text in JSON constructor") == 0) {
        set_invalid_json_function_text_error(database, 0U);
        return MYLITE_ERROR;
    }
    if (strcmp(message, "Unsupported JSON value in JSON constructor") == 0) {
        set_unsupported_error(database, "JSON constructors support only baseline JSON values");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
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
