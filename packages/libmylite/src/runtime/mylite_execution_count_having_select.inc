static bool select_statement_has_count_having_clause(const struct mylite_sql_ast_node *statement) {
    const struct mylite_sql_ast_node *optional_clause =
        select_statement_optional_clause_start(statement);

    if (select_statement_has_count_aggregate(statement) ||
        select_statement_has_column_aggregate(statement)) {
        return false;
    }

    while (optional_clause != NULL) {
        if (optional_clause->kind == MYLITE_SQL_AST_HAVING_CLAUSE &&
            count_having_comparison_uses_count_star(
                count_having_comparison_node(optional_clause)
            )) {
            return true;
        }
        optional_clause = optional_clause->next_sibling;
    }

    return false;
}

static int execute_count_having_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result
) {
    struct planned_count_having_select plan = {0};
    int rc = plan_count_having_select(database, statement, &plan);

    if (rc == MYLITE_OK) {
        rc = append_select_modifier_warnings(database, statement);
    }
    if (rc == MYLITE_OK) {
        rc = execute_count_having_select_from_plan(
            database,
            &plan,
            apply_sql_select_limit,
            out_result
        );
    }
    planned_count_having_select_deinit(&plan);

    return rc;
}

static int plan_count_having_select(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_count_having_select *out_plan
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *from_clause = select_statement_source_clause(statement);
    struct select_optional_clauses clauses = {0};
    struct table_name_resolution source = {0};
    struct mylite_catalog_table_descriptor table = {0};
    struct select_source_context source_context = {0};
    struct mylite_catalog_column_descriptor *table_columns = NULL;
    size_t table_column_count = 0U;
    int rc = MYLITE_OK;

    *out_plan = (struct planned_count_having_select){0};
    if (mylite_sql_ast_node_select_calc_found_rows(statement) != 0) {
        set_unsupported_error(
            database,
            "SQL_CALC_FOUND_ROWS does not support aggregate HAVING without GROUP BY"
        );
        return MYLITE_ERROR;
    }
    if (from_clause == NULL || from_clause->kind != MYLITE_SQL_AST_FROM_TABLE) {
        set_unsupported_error(
            database,
            "aggregate HAVING without GROUP BY supports only descriptor table reads"
        );
        return MYLITE_ERROR;
    }

    rc = collect_count_having_select_clauses(database, statement, &clauses);
    if (rc == MYLITE_OK) {
        rc = resolve_visible_table_reference(database, child_at(from_clause, 0U), &source, &table);
    }
    if (rc == MYLITE_OK) {
        rc = validate_select_index_hints(database, from_clause, &table);
    }
    if (rc == MYLITE_OK) {
        rc = init_select_source_context(database, from_clause, &source, &source_context);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_columns(
            database,
            table.table_id,
            &table_columns,
            &table_column_count
        );
    }
    if (rc == MYLITE_OK) {
        out_plan->descriptor_projection.source = source;
        out_plan->descriptor_projection.table = table;
        out_plan->descriptor_projection.is_distinct =
            mylite_sql_ast_node_select_modifier(statement) ==
            MYLITE_SQL_AST_SELECT_MODIFIER_DISTINCT;
        copy_plan_source_alias_if_present(&out_plan->descriptor_projection, &source_context);

        out_plan->row_projection.has_source = true;
        out_plan->row_projection.source = source;
        out_plan->row_projection.table = table;
        if (source_context.has_alias) {
            memcpy(
                out_plan->row_projection.source_alias,
                source_context.alias,
                sizeof(out_plan->row_projection.source_alias)
            );
            out_plan->row_projection.source_has_alias = true;
        }
    }
    if (rc == MYLITE_OK) {
        rc = plan_count_having_select_projection(
            database,
            select_list,
            &clauses,
            &source_context,
            table_columns,
            table_column_count,
            out_plan
        );
    }
    if (rc == MYLITE_OK) {
        rc = plan_count_having_clause(
            database,
            clauses.having_clause,
            &out_plan->operator_kind,
            &out_plan->value
        );
    }

    free(table_columns);
    if (rc != MYLITE_OK) {
        planned_count_having_select_deinit(out_plan);
    }

    return rc;
}

static int collect_count_having_select_clauses(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct select_optional_clauses *out_clauses
) {
    const struct mylite_sql_ast_node *optional_clause =
        select_statement_optional_clause_start(statement);

    *out_clauses = (struct select_optional_clauses){0};
    while (optional_clause != NULL) {
        if (optional_clause->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
            out_clauses->where_clause = optional_clause;
        } else if (optional_clause->kind == MYLITE_SQL_AST_HAVING_CLAUSE) {
            out_clauses->having_clause = optional_clause;
        } else if (optional_clause->kind == MYLITE_SQL_AST_SELECT_INTO_LIST) {
            /* SELECT ... INTO is handled after normal SELECT execution. */
        } else {
            set_unsupported_error(
                database,
                "aggregate HAVING without GROUP BY supports only WHERE and HAVING"
            );
            return MYLITE_ERROR;
        }
        optional_clause = optional_clause->next_sibling;
    }
    if (out_clauses->having_clause == NULL) {
        set_unsupported_error(database, "aggregate HAVING without GROUP BY requires HAVING");
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int plan_count_having_select_projection(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct select_optional_clauses *clauses,
    const struct select_source_context *source_context,
    struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_count_having_select *out_plan
) {
    if (select_list_is_descriptor_projection(select_list)) {
        return plan_count_having_descriptor_projection(
            database,
            select_list,
            clauses,
            source_context,
            table_columns,
            table_column_count,
            out_plan
        );
    }

    return plan_count_having_row_scalar_projection(
        database,
        select_list,
        clauses,
        source_context,
        table_columns,
        table_column_count,
        out_plan
    );
}

static bool select_list_is_descriptor_projection(const struct mylite_sql_ast_node *select_list) {
    const struct mylite_sql_ast_node *item = NULL;

    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST) {
        return false;
    }
    if (select_list_is_wildcard(select_list)) {
        return true;
    }

    item = child_at(select_list, 0U);
    while (item != NULL) {
        const struct mylite_sql_ast_node *column_node = NULL;
        const struct mylite_sql_ast_node *qualified_wildcard = NULL;

        if (select_item_qualified_wildcard(item, &qualified_wildcard)) {
            item = item->next_sibling;
            continue;
        }
        if (select_item_column_reference(item, &column_node) != MYLITE_OK) {
            return false;
        }
        item = item->next_sibling;
    }

    return true;
}

static int plan_count_having_descriptor_projection(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct select_optional_clauses *clauses,
    const struct select_source_context *source_context,
    struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_count_having_select *out_plan
) {
    int rc = plan_select_projection(
        database,
        select_list,
        source_context,
        table_columns,
        table_column_count,
        &out_plan->descriptor_projection
    );

    if (rc == MYLITE_OK) {
        rc = plan_select_predicate(
            database,
            clauses->where_clause,
            source_context,
            table_columns,
            table_column_count,
            &out_plan->descriptor_projection.predicate
        );
    }
    if (rc == MYLITE_OK) {
        out_plan->projection_kind = PLANNED_COUNT_HAVING_SELECT_DESCRIPTOR;
    }

    return rc;
}

static int plan_count_having_row_scalar_projection(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *select_list,
    const struct select_optional_clauses *clauses,
    const struct select_source_context *source_context,
    struct mylite_catalog_column_descriptor *table_columns,
    size_t table_column_count,
    struct planned_count_having_select *out_plan
) {
    int rc = MYLITE_OK;

    if (out_plan->descriptor_projection.is_distinct) {
        set_unsupported_error(
            database,
            "aggregate HAVING without GROUP BY does not support DISTINCT row-scalar projection"
        );
        return MYLITE_ERROR;
    }

    rc = plan_row_scalar_select_items(
        database,
        select_list,
        NULL,
        source_context,
        &out_plan->row_projection.table,
        table_columns,
        table_column_count,
        &out_plan->row_projection
    );
    if (rc == MYLITE_OK) {
        rc = plan_select_predicate(
            database,
            clauses->where_clause,
            source_context,
            table_columns,
            table_column_count,
            &out_plan->row_projection.predicate
        );
    }
    if (rc == MYLITE_OK) {
        out_plan->projection_kind = PLANNED_COUNT_HAVING_SELECT_ROW_SCALAR;
    }

    return rc;
}

static int plan_count_having_clause(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *having_clause,
    enum mylite_sql_ast_operator *out_operator_kind,
    struct planned_value *out_value
) {
    const struct mylite_sql_ast_node *comparison = count_having_comparison_node(having_clause);
    struct planned_grouped_aggregate aggregate = {0};
    int rc = MYLITE_OK;

    *out_operator_kind = MYLITE_SQL_AST_OPERATOR_NONE;
    *out_value = (struct planned_value){0};
    if (!count_having_comparison_uses_count_star(comparison)) {
        set_unsupported_error(
            database,
            "aggregate HAVING without GROUP BY supports only COUNT(*) comparisons"
        );
        return MYLITE_ERROR;
    }

    *out_operator_kind = mylite_sql_ast_node_operator(comparison);
    rc = convert_grouped_having_comparison_value(
        database,
        child_at(comparison, 1U),
        &aggregate,
        PLANNED_GROUPED_HAVING_OPERAND_AGGREGATE,
        out_value
    );

    return rc;
}

static const struct mylite_sql_ast_node *count_having_comparison_node(
    const struct mylite_sql_ast_node *having_clause
) {
    const struct mylite_sql_ast_node *predicate = NULL;

    if (having_clause == NULL || having_clause->kind != MYLITE_SQL_AST_HAVING_CLAUSE) {
        return NULL;
    }
    predicate = unwrap_parenthesized_predicate(child_at(having_clause, 0U));
    if (predicate == NULL || predicate->kind != MYLITE_SQL_AST_COMPARISON_PREDICATE) {
        return NULL;
    }

    return predicate;
}

static bool count_having_comparison_uses_count_star(const struct mylite_sql_ast_node *comparison) {
    const struct mylite_sql_ast_node *operand = NULL;

    if (comparison == NULL) {
        return false;
    }
    operand = unwrap_parenthesized_expression(child_at(comparison, 0U));
    return (operand != NULL && operand->kind == MYLITE_SQL_AST_COUNT_STAR_FUNCTION) != 0;
}

static void planned_count_having_select_deinit(struct planned_count_having_select *plan) {
    if (plan == NULL) {
        return;
    }

    planned_select_deinit(&plan->descriptor_projection);
    planned_row_scalar_select_deinit(&plan->row_projection);
    planned_value_deinit(&plan->value);
    *plan = (struct planned_count_having_select){0};
}

static int execute_count_having_select_from_plan(
    struct mylite_db *database,
    const struct planned_count_having_select *plan,
    bool apply_sql_select_limit,
    mylite_result **out_result
) {
    sqlite3_stmt *statement = NULL;
    struct result_column_metadata_context metadata_context = result_column_metadata_context_init();
    struct sqlite_result_row_storage row_storage = {0};
    mylite_result *result = NULL;
    char *sql = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }
    if (plan->projection_kind == PLANNED_COUNT_HAVING_SELECT_DESCRIPTOR) {
        rc = load_result_column_metadata_context_for_table(
            database,
            plan->descriptor_projection.table.table_id,
            &metadata_context
        );
    } else {
        rc = load_result_column_metadata_context_for_table(
            database,
            plan->row_projection.table.table_id,
            &metadata_context
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_count_having_select_result_columns(database, result, plan, &metadata_context);
    }
    if (rc == MYLITE_OK) {
        rc = build_count_having_select_sql(plan, &sql);
    }
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_count_having_select_parameters(statement, plan);
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
        if (plan->projection_kind == PLANNED_COUNT_HAVING_SELECT_DESCRIPTOR) {
            rc = append_selected_sqlite_row(
                database,
                statement,
                result,
                plan->descriptor_projection.columns,
                plan->descriptor_projection.column_count,
                &row_storage
            );
        } else {
            rc = append_selected_sqlite_row(database, statement, result, NULL, 0U, &row_storage);
        }
    }
    rc = finalize_sqlite_statement(statement, rc);
    sqlite_result_row_storage_deinit(&row_storage);
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
    if (apply_sql_select_limit) {
        apply_sql_select_limit_to_result(database, result);
    }

    return finish_successful_result(database, result, out_result);
}

static int append_count_having_select_result_columns(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_count_having_select *plan,
    const struct result_column_metadata_context *metadata_context
) {
    int rc = MYLITE_OK;

    if (plan->projection_kind == PLANNED_COUNT_HAVING_SELECT_DESCRIPTOR) {
        for (size_t column_index = 0U;
             rc == MYLITE_OK && column_index < plan->descriptor_projection.column_count;
             ++column_index) {
            rc = append_select_result_column(
                database,
                result,
                &plan->descriptor_projection,
                metadata_context,
                column_index
            );
        }
        return rc;
    }

    return append_count_having_row_scalar_result_columns(database, result, plan, metadata_context);
}

static int append_count_having_row_scalar_result_columns(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_count_having_select *plan,
    const struct result_column_metadata_context *metadata_context
) {
    int rc = MYLITE_OK;

    for (size_t item_index = 0U; rc == MYLITE_OK && item_index < plan->row_projection.item_count;
         ++item_index) {
        const struct planned_row_scalar_select_item *item = &plan->row_projection.items[item_index];
        const char *literal_label = count_having_row_scalar_literal_label(item);

        if (literal_label != NULL) {
            struct mylite_result_column_descriptor descriptor;

            rc = make_row_scalar_result_column_descriptor(
                database,
                &plan->row_projection,
                metadata_context,
                item,
                literal_label,
                &descriptor
            );
            if (rc == MYLITE_OK) {
                rc = mylite_result_append_column_descriptor(result, &descriptor);
                if (rc != MYLITE_OK) {
                    set_nomem_error(database);
                }
            }
            continue;
        }

        rc = append_row_scalar_result_column(
            database,
            result,
            &plan->row_projection,
            metadata_context,
            item
        );
    }

    return rc;
}

static const char *count_having_row_scalar_literal_label(
    const struct planned_row_scalar_select_item *item
) {
    if (item == NULL || item->expression.kind != PLANNED_ROW_SCALAR_EXPRESSION_VALUE ||
        item->expression.value.is_null || !item->expression.value.is_text) {
        return NULL;
    }

    return item->expression.value.text;
}

static int build_count_having_select_sql(
    const struct planned_count_having_select *plan,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    size_t next_parameter = 1U;
    const struct planned_select_predicate *inner_predicate = NULL;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    if (plan->projection_kind == PLANNED_COUNT_HAVING_SELECT_DESCRIPTOR &&
        plan->descriptor_projection.is_distinct) {
        rc = mylite_dynamic_string_append(&string, "SELECT DISTINCT ");
    } else {
        rc = mylite_dynamic_string_append(&string, "SELECT ");
    }
    if (rc == MYLITE_OK) {
        rc = append_count_having_outer_select_list_sql(&string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " FROM (SELECT ");
    }
    if (rc == MYLITE_OK) {
        rc = append_count_having_inner_select_list_sql(&string, plan, &next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, ", COUNT(*) AS ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, "_mylite_count_having_count");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " FROM ");
    }
    if (rc == MYLITE_OK && plan->projection_kind == PLANNED_COUNT_HAVING_SELECT_DESCRIPTOR) {
        rc = mylite_dynamic_string_append_quoted_identifier(
            &string,
            plan->descriptor_projection.table.physical_name
        );
    } else if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(
            &string,
            plan->row_projection.table.physical_name
        );
    }
    inner_predicate = plan->projection_kind == PLANNED_COUNT_HAVING_SELECT_DESCRIPTOR
                          ? &plan->descriptor_projection.predicate
                          : &plan->row_projection.predicate;
    if (rc == MYLITE_OK && inner_predicate->qualify_column_references) {
        rc = mylite_dynamic_string_append(&string, " AS ");
    }
    if (rc == MYLITE_OK && inner_predicate->qualify_column_references) {
        rc = append_select_source_alias(&string, 0U);
    }
    if (rc == MYLITE_OK && plan->projection_kind == PLANNED_COUNT_HAVING_SELECT_DESCRIPTOR) {
        rc = append_select_predicate_sql(
            &string,
            &plan->descriptor_projection.predicate,
            &next_parameter
        );
    } else if (rc == MYLITE_OK) {
        rc = append_select_predicate_sql(&string, &plan->row_projection.predicate, &next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, ") ");
    }
    if (rc == MYLITE_OK) {
        rc = append_count_having_sql(&string, plan, &next_parameter);
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

static int append_count_having_outer_select_list_sql(
    struct mylite_dynamic_string *string,
    const struct planned_count_having_select *plan
) {
    size_t projection_count = plan->projection_kind == PLANNED_COUNT_HAVING_SELECT_DESCRIPTOR
                                  ? plan->descriptor_projection.column_count
                                  : plan->row_projection.item_count;
    int rc = MYLITE_OK;

    if (projection_count == 0U) {
        return MYLITE_ERROR;
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < projection_count; ++index) {
        if (index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_count_having_projection_alias(string, index);
        }
        if (rc == MYLITE_OK && plan->projection_kind == PLANNED_COUNT_HAVING_SELECT_DESCRIPTOR &&
            plan->descriptor_projection.is_distinct &&
            column_descriptor_uses_string_key_collation(
                &plan->descriptor_projection.columns[index],
                true
            )) {
            rc = append_string_key_collation_sql(string);
        }
    }

    return rc;
}

static int append_count_having_inner_select_list_sql(
    struct mylite_dynamic_string *string,
    const struct planned_count_having_select *plan,
    size_t *next_parameter
) {
    size_t projection_count = plan->projection_kind == PLANNED_COUNT_HAVING_SELECT_DESCRIPTOR
                                  ? plan->descriptor_projection.column_count
                                  : plan->row_projection.item_count;
    int rc = MYLITE_OK;

    if (projection_count == 0U) {
        return MYLITE_ERROR;
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < projection_count; ++index) {
        if (index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK && plan->projection_kind == PLANNED_COUNT_HAVING_SELECT_DESCRIPTOR) {
            rc = append_select_projection_column_sql(string, &plan->descriptor_projection, index);
        } else if (rc == MYLITE_OK) {
            rc = append_row_scalar_expression_sql(
                string,
                &plan->row_projection.items[index].expression,
                next_parameter
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " AS ");
        }
        if (rc == MYLITE_OK) {
            rc = append_count_having_projection_alias(string, index);
        }
    }

    return rc;
}

static int append_count_having_projection_alias(
    struct mylite_dynamic_string *string,
    size_t projection_index
) {
    enum { count_having_projection_alias_capacity = 64U };

    char alias[count_having_projection_alias_capacity];
    int written =
        snprintf(alias, sizeof(alias), "_mylite_count_having_projection_%zu", projection_index);

    if (written < 0 || (size_t)written >= sizeof(alias)) {
        return MYLITE_NOMEM;
    }

    return mylite_dynamic_string_append_quoted_identifier(string, alias);
}

static int append_count_having_sql(
    struct mylite_dynamic_string *string,
    const struct planned_count_having_select *plan,
    size_t *next_parameter
) {
    int rc = mylite_dynamic_string_append(string, "WHERE ");

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, "_mylite_count_having_count");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ' ');
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, comparison_operator_sql(plan->operator_kind));
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ' ');
    }
    if (rc == MYLITE_OK) {
        rc = append_numbered_parameter(string, *next_parameter);
        if (rc == MYLITE_OK) {
            ++(*next_parameter);
        }
    }

    return rc;
}

static int bind_count_having_select_parameters(
    sqlite3_stmt *statement,
    const struct planned_count_having_select *plan
) {
    int parameter_index = 1;
    int rc = MYLITE_OK;

    if (plan->projection_kind == PLANNED_COUNT_HAVING_SELECT_ROW_SCALAR) {
        for (size_t index = 0U; rc == MYLITE_OK && index < plan->row_projection.item_count;
             ++index) {
            rc = bind_row_scalar_expression_parameters(
                statement,
                &plan->row_projection.items[index].expression,
                &parameter_index
            );
        }
    }
    if (rc == MYLITE_OK && plan->projection_kind == PLANNED_COUNT_HAVING_SELECT_DESCRIPTOR) {
        rc = bind_select_predicate_parameters(
            statement,
            &plan->descriptor_projection.predicate,
            &parameter_index
        );
    } else if (rc == MYLITE_OK) {
        rc = bind_select_predicate_parameters(
            statement,
            &plan->row_projection.predicate,
            &parameter_index
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_int64_parameter(statement, parameter_index, plan->value.integer);
    }

    return rc;
}
