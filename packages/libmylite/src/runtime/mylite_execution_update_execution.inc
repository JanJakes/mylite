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
    enum mylite_execution_dml_numeric_parse_result parse_result =
        MYLITE_EXECUTION_DML_NUMERIC_PARSE_INVALID;
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

    parse_result = mylite_execution_parse_dml_integer_string_text(
        text,
        text_length,
        &magnitude,
        &is_negative
    );
    if (parse_result == MYLITE_EXECUTION_DML_NUMERIC_PARSE_INVALID) {
        rc = append_incorrect_integer_value_warning(database, text, column->name, row_number);
    } else if (parse_result == MYLITE_EXECUTION_DML_NUMERIC_PARSE_OVERFLOW) {
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
        } else if (
            rc == MYLITE_OK && parse_result == MYLITE_EXECUTION_DML_NUMERIC_PARSE_TRUNCATED
        ) {
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
    rc = mylite_execution_load_parent_foreign_key_infos(
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

    mylite_execution_loaded_foreign_key_infos_deinit(&foreign_keys, &foreign_key_count);
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
    rc = mylite_execution_load_parent_foreign_key_infos(
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

    mylite_execution_loaded_foreign_key_infos_deinit(&foreign_keys, &foreign_key_count);
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
        if (plan->is_joined) {
            rc = bind_joined_update_source_parameters(statement, &parameter_index, plan);
        }
    }
    if (rc == MYLITE_OK && plan->is_joined) {
        rc = bind_select_join_condition_parameters(
            statement,
            &plan->join_condition,
            &parameter_index
        );
    }
    if (rc == MYLITE_OK) {
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
    int rc = MYLITE_OK;

    if (plan->is_joined) {
        rc = bind_joined_update_source_parameters(statement, &parameter_index, plan);
    }
    if (rc == MYLITE_OK && plan->is_joined) {
        rc = bind_select_join_condition_parameters(
            statement,
            &plan->join_condition,
            &parameter_index
        );
    }
    if (rc == MYLITE_OK) {
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
        mylite_execution_load_table_columns(database, foreign_key->child_table.table_id, &columns, &column_count);

    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_index_infos(
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

    mylite_execution_loaded_index_infos_deinit(&indexes, &index_count);
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

    rc = mylite_execution_load_parent_foreign_key_infos(
        database,
        foreign_key->child_table.table_id,
        &child_parent_foreign_keys,
        &child_parent_foreign_key_count
    );
    if (rc == MYLITE_OK && child_parent_foreign_key_count != 0U) {
        set_unsupported_error(database, "recursive foreign-key action is not supported");
        rc = MYLITE_ERROR;
    }
    mylite_execution_loaded_foreign_key_infos_deinit(&child_parent_foreign_keys, &child_parent_foreign_key_count);
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
