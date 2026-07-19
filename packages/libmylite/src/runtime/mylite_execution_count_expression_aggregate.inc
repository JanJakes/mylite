static int collect_count_expression_aggregate_clauses(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct select_optional_clauses *out_clauses
);
static int plan_count_expression_aggregate_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct select_optional_clauses *clauses,
    struct select_source_context *out_source_context,
    struct planned_count_expression_aggregate *out_plan
);
static int plan_single_source_count_expression_aggregate_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    const struct select_optional_clauses *clauses,
    struct select_source_context *out_source_context,
    struct planned_count_expression_aggregate *out_plan
);
static int plan_joined_count_expression_aggregate_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    const struct select_optional_clauses *clauses,
    struct select_source_context *out_source_context,
    struct planned_count_expression_aggregate *out_plan
);
static int plan_count_expression_aggregate_items(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_count_expression_aggregate *out_plan
);
static int append_count_expression_aggregate_item(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_item,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_count_expression_aggregate *out_plan
);
static const struct mylite_sql_ast_node *count_expression_aggregate_predicate_node(
    const struct mylite_sql_ast_node *expression
);
static bool count_expression_aggregate_select_item_is_supported(
    const struct mylite_sql_ast_node *select_item,
    bool *inout_has_expression_count,
    bool *inout_has_count_star
);
static const struct mylite_sql_ast_node *count_expression_aggregate_argument(
    const struct mylite_sql_ast_node *expression
);
static int append_count_expression_aggregate_result_columns(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_count_expression_aggregate *plan
);
static int build_count_expression_aggregate_sql(
    const struct planned_count_expression_aggregate *plan,
    char **out_sql
);
static int append_count_expression_aggregate_select_item_sql(
    struct mylite_dynamic_string *string,
    const struct planned_count_expression_aggregate_item *item,
    size_t *next_parameter
);
static int bind_count_expression_aggregate_parameters(
    sqlite3_stmt *statement,
    const struct planned_count_expression_aggregate *plan
);
static int append_count_expression_aggregate_row(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    mylite_result *result,
    const struct planned_count_expression_aggregate *plan
);
static bool count_expression_aggregate_limit_excludes_result(
    const struct planned_count_expression_aggregate *plan
);
static bool select_statement_has_joined_count_expression_source(
    const struct mylite_sql_ast_node *statement
);

static bool select_statement_has_count_expression_aggregate(
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *select_item = NULL;
    bool has_expression_count = false;
    bool has_count_star = false;

    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST ||
        select_list_is_wildcard(select_list)) {
        return false;
    }

    select_item = child_at(select_list, 0U);
    while (select_item != NULL) {
        if (!count_expression_aggregate_select_item_is_supported(
                select_item,
                &has_expression_count,
                &has_count_star
            )) {
            return false;
        }
        select_item = select_item->next_sibling;
    }

    return has_expression_count ||
           (has_count_star && select_statement_has_joined_count_expression_source(statement));
}

static bool select_statement_has_joined_count_expression_source(
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *from_clause = select_statement_source_clause(statement);

    return from_clause != NULL && from_clause->kind == MYLITE_SQL_AST_FROM_JOIN;
}

static int execute_count_expression_aggregate_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result
) {
    struct planned_count_expression_aggregate plan = {0};
    int rc = plan_count_expression_aggregate(database, statement, &plan);

    if (rc == MYLITE_OK) {
        rc = append_select_modifier_warnings(database, statement);
    }
    if (rc == MYLITE_OK && mylite_sql_ast_node_select_calc_found_rows(statement) != 0) {
        rc = append_sql_calc_found_rows_deprecation_warning(database);
    }
    if (rc == MYLITE_OK) {
        rc = execute_count_expression_aggregate_from_plan(
            database,
            &plan,
            apply_sql_select_limit,
            out_result
        );
    }
    planned_count_expression_aggregate_deinit(&plan);

    return rc;
}

static int plan_count_expression_aggregate(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_count_expression_aggregate *out_plan
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    struct select_optional_clauses clauses = {0};
    struct select_source_context source_context = {0};
    int rc = MYLITE_OK;

    *out_plan = (struct planned_count_expression_aggregate){0};
    out_plan->is_distinct =
        mylite_sql_ast_node_select_modifier(statement) == MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT;
    out_plan->calc_found_rows = mylite_sql_ast_node_select_calc_found_rows(statement) != 0;

    rc = collect_count_expression_aggregate_clauses(database, statement, &clauses);
    if (rc == MYLITE_OK) {
        rc = plan_count_expression_aggregate_source(
            database,
            statement,
            &clauses,
            &source_context,
            out_plan
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_count_expression_aggregate_items(
            database,
            select_list,
            &source_context,
            out_plan->source.source_columns,
            out_plan->source.source_column_count,
            out_plan
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_plan_select_limit(database, clauses.limit_clause, &out_plan->limit);
    }

    if (rc != MYLITE_OK) {
        planned_count_expression_aggregate_deinit(out_plan);
    }
    return rc;
}

static int collect_count_expression_aggregate_clauses(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct select_optional_clauses *out_clauses
) {
    int rc = collect_select_optional_clauses(database, statement, out_clauses);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (out_clauses->having_clause != NULL || out_clauses->order_clause != NULL) {
        set_unsupported_error(database, "COUNT expression aggregate supports only WHERE and LIMIT");
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int plan_count_expression_aggregate_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    const struct select_optional_clauses *clauses,
    struct select_source_context *out_source_context,
    struct planned_count_expression_aggregate *out_plan
) {
    const struct mylite_sql_ast_node *from_clause = select_statement_source_clause(statement);

    if (from_clause == NULL || from_clause->kind == MYLITE_SQL_AST_FROM_DUAL) {
        set_unsupported_error(database, "COUNT expression aggregate requires a table source");
        return MYLITE_ERROR;
    }
    if (from_clause->kind == MYLITE_SQL_AST_FROM_TABLE) {
        return plan_single_source_count_expression_aggregate_source(
            database,
            from_clause,
            clauses,
            out_source_context,
            out_plan
        );
    }
    if (from_clause->kind == MYLITE_SQL_AST_FROM_JOIN) {
        return plan_joined_count_expression_aggregate_source(
            database,
            from_clause,
            clauses,
            out_source_context,
            out_plan
        );
    }

    set_unsupported_error(
        database,
        "COUNT expression aggregate supports only table and joined sources"
    );
    return MYLITE_ERROR;
}

static int plan_single_source_count_expression_aggregate_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    const struct select_optional_clauses *clauses,
    struct select_source_context *out_source_context,
    struct planned_count_expression_aggregate *out_plan
) {
    const struct select_predicate_plan_options predicate_options = {
        .allow_exists = false,
        .allow_scalar_subquery_in_literal = true,
        .allow_sql_auto_is_null_lookup = true,
    };
    int rc = resolve_visible_table_reference(
        database,
        child_at(from_clause, 0U),
        &out_plan->source.source,
        &out_plan->source.table
    );

    if (rc == MYLITE_OK) {
        rc = validate_select_index_hints(database, from_clause, &out_plan->source.table);
    }
    if (rc == MYLITE_OK) {
        rc = init_select_source_context(
            database,
            from_clause,
            &out_plan->source.source,
            out_source_context
        );
    }
    if (rc == MYLITE_OK) {
        copy_plan_source_alias_if_present(&out_plan->source, out_source_context);
        rc = mylite_execution_acquire_table_columns(
            database,
            out_plan->source.table.table_id,
            &out_plan->source.source_columns,
            &out_plan->source.source_column_count,
            &out_plan->source.source_columns_cache_entry
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_select_predicate_with_options(
            database,
            clauses->where_clause,
            out_source_context,
            out_plan->source.source_columns,
            out_plan->source.source_column_count,
            &predicate_options,
            &out_plan->source.predicate
        );
    }

    return rc;
}

static int plan_joined_count_expression_aggregate_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *from_clause,
    const struct select_optional_clauses *clauses,
    struct select_source_context *out_source_context,
    struct planned_count_expression_aggregate *out_plan
) {
    struct joined_select_temp_nodes temp_nodes = {0};
    int rc = prepare_joined_select_plan(database, from_clause, &out_plan->source, &temp_nodes);

    if (rc == MYLITE_OK) {
        rc = reject_duplicate_select_source_reference(
            database,
            out_plan->source.sources,
            out_plan->source.source_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = init_join_select_source_context(
            out_plan->source.sources,
            out_plan->source.source_count,
            out_source_context
        );
    }
    if (rc == MYLITE_OK) {
        rc = reject_unsupported_multi_source_join_edges(database, &out_plan->source);
    }
    if (rc == MYLITE_OK) {
        rc = plan_joined_select_conditions(
            database,
            temp_nodes.join_condition_nodes,
            temp_nodes.join_condition_node_count,
            &out_plan->source
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_joined_select_predicate(
            database,
            clauses->where_clause,
            out_source_context,
            &out_plan->source.predicate
        );
    }

    joined_select_temp_nodes_deinit(&temp_nodes);
    return rc;
}

static int plan_count_expression_aggregate_items(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_count_expression_aggregate *out_plan
) {
    const struct mylite_sql_ast_node *select_item = NULL;

    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST ||
        select_list_is_wildcard(select_list)) {
        set_unsupported_error(
            database,
            "COUNT expression aggregate supports explicit count select items"
        );
        return MYLITE_ERROR;
    }

    select_item = child_at(select_list, 0U);
    while (select_item != NULL) {
        int rc = append_count_expression_aggregate_item(
            database,
            select_item,
            source_context,
            table_columns,
            table_column_count,
            out_plan
        );

        if (rc != MYLITE_OK) {
            return rc;
        }
        select_item = select_item->next_sibling;
    }

    return MYLITE_OK;
}

static int append_count_expression_aggregate_item(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_item,
    const struct select_source_context *source_context,
    const struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_count_expression_aggregate *out_plan
) {
    const struct mylite_sql_ast_node *source_expression = NULL;
    const struct mylite_sql_ast_node *expression = NULL;
    const struct mylite_sql_ast_node *alias = NULL;
    const struct mylite_sql_ast_node *predicate_node = NULL;
    const struct select_predicate_plan_options options = {
        .allow_exists = false,
        .allow_same_scope_column_reference_rhs = true,
        .allow_date_format_numeric_predicate = true,
    };
    struct planned_count_expression_aggregate_item item = {0};
    struct planned_count_expression_aggregate_item *items = NULL;
    size_t required_count = 0U;
    int rc = MYLITE_OK;

    if (select_item == NULL || select_item->kind != MYLITE_SQL_AST_SELECT_ITEM) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    source_expression = child_at(select_item, 0U);
    alias = child_at(select_item, 1U);
    expression = unwrap_parenthesized_expression(source_expression);
    if (expression == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    item.is_distinct = aggregate_function_is_distinct(expression);
    if (expression->kind == MYLITE_SQL_AST_COUNT_STAR_FUNCTION) {
        item.kind = PLANNED_COUNT_EXPRESSION_AGGREGATE_STAR;
    } else {
        predicate_node = count_expression_aggregate_predicate_node(expression);
        if (predicate_node != NULL) {
            item.kind = PLANNED_COUNT_EXPRESSION_AGGREGATE_NULLIF_PREDICATE;
            item.predicate.qualify_column_references =
                select_source_context_is_joined(source_context);
            rc = plan_select_predicate_node_without_exists(
                database,
                predicate_node,
                source_context,
                table_columns,
                table_column_count,
                &options,
                &item.predicate
            );
            if (rc != MYLITE_OK) {
                planned_select_predicate_deinit(&item.predicate);
                return rc;
            }
        } else if (expression->kind == MYLITE_SQL_AST_COUNT_EXPRESSION_FUNCTION &&
                   count_expression_aggregate_argument(expression) != NULL) {
            const struct mylite_catalog_table_descriptor *table =
                select_source_context_is_joined(source_context) ? NULL : &out_plan->source.table;
            const struct mylite_sql_ast_node *argument =
                count_expression_aggregate_argument(expression);

            if (row_scalar_expression_contains_fractional_division_attempt(argument)) {
                set_unsupported_error(
                    database,
                    "aggregate row-scalar arguments do not support / division"
                );
                return MYLITE_ERROR;
            }
            item.kind = PLANNED_COUNT_EXPRESSION_AGGREGATE_ROW_SCALAR;
            rc = plan_row_scalar_expression(
                database,
                argument,
                true,
                source_context,
                table,
                table_columns,
                table_column_count,
                &item.row_scalar_expression
            );
            if (rc != MYLITE_OK) {
                planned_row_scalar_expression_deinit(&item.row_scalar_expression);
                return rc;
            }
        } else {
            set_unsupported_error(
                database,
                "COUNT expression aggregate supports COUNT(*), "
                "COUNT(NULLIF(column predicate,FALSE)), and COUNT(row-scalar expression) "
                "select items"
            );
            return MYLITE_ERROR;
        }
    }

    rc = copy_planned_aggregate_result_label(
        database,
        &(const struct aggregate_result_label_source){
            .expression = source_expression,
            .alias = alias,
        },
        &item.result_label
    );
    if (rc != MYLITE_OK) {
        planned_select_predicate_deinit(&item.predicate);
        planned_row_scalar_expression_deinit(&item.row_scalar_expression);
        return rc;
    }
    required_count = out_plan->item_count + 1U;
    if (required_count > SIZE_MAX / sizeof(*items)) {
        planned_select_predicate_deinit(&item.predicate);
        planned_row_scalar_expression_deinit(&item.row_scalar_expression);
        free(item.result_label);
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    items = realloc(out_plan->items, required_count * sizeof(*items));
    if (items == NULL) {
        planned_select_predicate_deinit(&item.predicate);
        planned_row_scalar_expression_deinit(&item.row_scalar_expression);
        free(item.result_label);
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    out_plan->items = items;
    out_plan->items[out_plan->item_count] = item;
    out_plan->item_count = required_count;
    return MYLITE_OK;
}

static const struct mylite_sql_ast_node *count_expression_aggregate_predicate_node(
    const struct mylite_sql_ast_node *expression
) {
    const struct mylite_sql_ast_node *nullif_expression = NULL;
    const struct mylite_sql_ast_node *false_argument = NULL;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_COUNT_EXPRESSION_FUNCTION ||
        count_expression_aggregate_argument(expression) == NULL) {
        return NULL;
    }
    nullif_expression = count_expression_aggregate_argument(expression);
    if (nullif_expression == NULL || nullif_expression->kind != MYLITE_SQL_AST_NULLIF_FUNCTION ||
        mylite_sql_ast_node_child_count(nullif_expression) != 2U) {
        return NULL;
    }
    false_argument = child_at(nullif_expression, 1U);
    if (false_argument == NULL || false_argument->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(false_argument) != MYLITE_SQL_AST_LITERAL_FALSE) {
        return NULL;
    }

    return child_at(nullif_expression, 0U);
}

static bool count_expression_aggregate_select_item_is_supported(
    const struct mylite_sql_ast_node *select_item,
    bool *inout_has_expression_count, // NOLINT(bugprone-easily-swappable-parameters): paired COUNT
                                      // flags.
    bool *inout_has_count_star
) {
    const struct mylite_sql_ast_node *expression = NULL;

    if (select_item == NULL || select_item->kind != MYLITE_SQL_AST_SELECT_ITEM) {
        return false;
    }
    expression = unwrap_parenthesized_expression(child_at(select_item, 0U));
    if (expression == NULL) {
        return false;
    }
    if (expression->kind == MYLITE_SQL_AST_COUNT_STAR_FUNCTION &&
        !aggregate_function_has_window_clause(expression)) {
        *inout_has_count_star = true;
        return true;
    }
    if (expression->kind == MYLITE_SQL_AST_COUNT_EXPRESSION_FUNCTION &&
        !aggregate_function_has_window_clause(expression) &&
        count_expression_aggregate_argument(expression) != NULL) {
        *inout_has_expression_count = true;
        return true;
    }

    return false;
}

static const struct mylite_sql_ast_node *count_expression_aggregate_argument(
    const struct mylite_sql_ast_node *expression
) {
    const struct mylite_sql_ast_node *argument = NULL;
    const struct mylite_sql_ast_node *child = NULL;

    expression = unwrap_parenthesized_expression(expression);
    if (expression == NULL || expression->kind != MYLITE_SQL_AST_COUNT_EXPRESSION_FUNCTION) {
        return NULL;
    }
    child = child_at(expression, 0U);
    while (child != NULL) {
        if (child->kind != MYLITE_SQL_AST_AGGREGATE_DISTINCT_MODIFIER &&
            child->kind != MYLITE_SQL_AST_WINDOW_SPEC &&
            child->kind != MYLITE_SQL_AST_WINDOW_REFERENCE) {
            if (argument != NULL) {
                return NULL;
            }
            argument = child;
        }
        child = child->next_sibling;
    }

    return argument;
}

static void planned_count_expression_aggregate_deinit(
    struct planned_count_expression_aggregate *plan
) {
    if (plan == NULL) {
        return;
    }

    for (size_t item_index = 0U; item_index < plan->item_count; ++item_index) {
        planned_select_predicate_deinit(&plan->items[item_index].predicate);
        planned_row_scalar_expression_deinit(&plan->items[item_index].row_scalar_expression);
        free(plan->items[item_index].result_label);
    }
    free(plan->items);
    planned_select_deinit(&plan->source);
    *plan = (struct planned_count_expression_aggregate){0};
}

static int execute_count_expression_aggregate_from_plan(
    struct mylite_db *database,
    const struct planned_count_expression_aggregate *plan,
    bool apply_sql_select_limit,
    mylite_result **out_result
) {
    sqlite3_stmt *statement = NULL;
    mylite_result *result = NULL;
    char *sql = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }
    rc = append_count_expression_aggregate_result_columns(database, result, plan);
    if (rc == MYLITE_OK) {
        rc = build_count_expression_aggregate_sql(plan, &sql);
    }
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_count_expression_aggregate_parameters(statement, plan);
    }
    if (rc == MYLITE_OK) {
        rc = append_count_expression_aggregate_row(database, statement, result, plan);
    }

    rc = finalize_sqlite_statement(statement, rc);
    free(sql);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return count_execution_error(database, rc);
    }
    if (plan->calc_found_rows) {
        mylite_result_set_found_row_count(result, 1U);
    }
    if (apply_sql_select_limit) {
        apply_sql_select_limit_to_result(database, result);
    }

    return finish_successful_result(database, result, out_result);
}

static int append_count_expression_aggregate_result_columns(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_count_expression_aggregate *plan
) {
    int rc = MYLITE_OK;

    for (size_t item_index = 0U; rc == MYLITE_OK && item_index < plan->item_count; ++item_index) {
        const struct planned_count_expression_aggregate_item *item = &plan->items[item_index];

        rc = mylite_result_append_column(result, item->result_label);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }

    return rc;
}

static int build_count_expression_aggregate_sql(
    const struct planned_count_expression_aggregate *plan,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    size_t next_parameter = 1U;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    if (plan->is_distinct) {
        rc = mylite_dynamic_string_append(&string, "SELECT DISTINCT ");
    } else {
        rc = mylite_dynamic_string_append(&string, "SELECT ");
    }
    for (size_t item_index = 0U; rc == MYLITE_OK && item_index < plan->item_count; ++item_index) {
        if (item_index != 0U) {
            rc = mylite_dynamic_string_append(&string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_count_expression_aggregate_select_item_sql(
                &string,
                &plan->items[item_index],
                &next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = append_select_from_sql(&string, &plan->source, &next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_predicate_sql(&string, &plan->source.predicate, &next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_limit_sql(&string, &plan->limit);
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

static int append_count_expression_aggregate_select_item_sql(
    struct mylite_dynamic_string *string,
    const struct planned_count_expression_aggregate_item *item,
    size_t *next_parameter
) {
    int rc = mylite_dynamic_string_append(string, "COUNT(");

    if (rc == MYLITE_OK && item->kind == PLANNED_COUNT_EXPRESSION_AGGREGATE_STAR) {
        rc = mylite_dynamic_string_append(string, "*");
    } else if (rc == MYLITE_OK &&
               item->kind == PLANNED_COUNT_EXPRESSION_AGGREGATE_NULLIF_PREDICATE) {
        if (item->is_distinct) {
            rc = mylite_dynamic_string_append(string, "DISTINCT ");
        }
        if (rc != MYLITE_OK) {
            return rc;
        }
        rc = mylite_dynamic_string_append(string, "NULLIF((");
        if (rc == MYLITE_OK) {
            rc = append_select_predicate_expression_sql_without_exists(
                string,
                &item->predicate,
                next_parameter
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, "), 0)");
        }
    } else if (rc == MYLITE_OK && item->kind == PLANNED_COUNT_EXPRESSION_AGGREGATE_ROW_SCALAR) {
        if (item->is_distinct) {
            rc = mylite_dynamic_string_append(string, "DISTINCT ");
        }
        if (rc != MYLITE_OK) {
            return rc;
        }
        rc = append_row_scalar_expression_sql(string, &item->row_scalar_expression, next_parameter);
        if (rc == MYLITE_OK &&
            row_scalar_expression_uses_string_collation(&item->row_scalar_expression)) {
            rc = append_string_key_collation_sql(string);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }

    return rc;
}

static int bind_count_expression_aggregate_parameters(
    sqlite3_stmt *statement,
    const struct planned_count_expression_aggregate *plan
) {
    int parameter_index = 1;
    int rc = MYLITE_OK;

    for (size_t item_index = 0U; rc == MYLITE_OK && item_index < plan->item_count; ++item_index) {
        const struct planned_count_expression_aggregate_item *item = &plan->items[item_index];

        if (item->kind == PLANNED_COUNT_EXPRESSION_AGGREGATE_NULLIF_PREDICATE) {
            rc = bind_select_predicate_parameters_without_exists(
                statement,
                &item->predicate,
                &parameter_index
            );
        } else if (item->kind == PLANNED_COUNT_EXPRESSION_AGGREGATE_ROW_SCALAR) {
            rc = bind_row_scalar_expression_parameters(
                statement,
                &item->row_scalar_expression,
                &parameter_index
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = bind_select_parameters_at(statement, &plan->source, &parameter_index);
    }
    return rc;
}

static int append_count_expression_aggregate_row(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    mylite_result *result,
    const struct planned_count_expression_aggregate *plan
) {
    int sqlite_rc = sqlite3_step(statement);

    if (sqlite_rc == SQLITE_DONE && count_expression_aggregate_limit_excludes_result(plan)) {
        return MYLITE_OK;
    }
    if (sqlite_rc != SQLITE_ROW) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }
    struct mylite_execution_result_row_storage row_storage = {0};
    int rc = mylite_execution_append_sqlite_result_row(
        database,
        statement,
        result,
        NULL,
        0U,
        &row_storage
    );

    mylite_execution_result_row_storage_deinit(&row_storage);

    if (rc != MYLITE_OK) {
        return rc;
    }
    sqlite_rc = sqlite3_step(statement);
    if (sqlite_rc != SQLITE_DONE) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    return MYLITE_OK;
}

static bool count_expression_aggregate_limit_excludes_result(
    const struct planned_count_expression_aggregate *plan
) {
    if (plan == NULL || !plan->limit.has_limit) {
        return false;
    }
    return plan->limit.row_count == 0 || (plan->limit.has_offset && plan->limit.offset > 0);
}
