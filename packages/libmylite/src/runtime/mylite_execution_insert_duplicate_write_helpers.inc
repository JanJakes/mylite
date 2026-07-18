static int find_insert_unique_key_conflict(
    struct mylite_db *database,
    int sqlite_step_rc,
    const struct planned_insert *plan,
    size_t row_index,
    const struct loaded_index_info **out_conflicting_index,
    char *value_text,
    size_t value_text_size
) {
    int rc = MYLITE_OK;

    *out_conflicting_index = NULL;
    if (value_text != NULL && value_text_size != 0U) {
        value_text[0] = '\0';
    }

    for (size_t index = 0U; rc == MYLITE_OK && index < plan->index_count; ++index) {
        const struct loaded_index_info *candidate = &plan->indexes[index];
        bool exists = false;
        bool has_null_part = false;

        if (!candidate->index.is_unique || candidate->part_count == 0U) {
            continue;
        }
        rc = insert_unique_key_row_has_null_part(
            database,
            plan,
            row_index,
            candidate,
            &has_null_part
        );
        if (rc != MYLITE_OK) {
            break;
        }
        if (has_null_part) {
            continue;
        }
        rc = unique_key_tuple_exists(
            database,
            plan->table.physical_name,
            candidate,
            plan->rows[row_index].values,
            plan->column_count,
            &exists
        );
        if (rc == MYLITE_OK && exists) {
            *out_conflicting_index = candidate;
            if (value_text != NULL && value_text_size != 0U) {
                rc = format_key_tuple(
                    candidate,
                    plan->rows[row_index].values,
                    plan->column_count,
                    value_text,
                    value_text_size
                );
            }
            break;
        }
    }

    if (rc == MYLITE_OK && *out_conflicting_index == NULL) {
        return mylite_sqlite_status_to_mylite(sqlite_step_rc);
    }

    return rc;
}

static int insert_unique_key_row_has_null_part(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    const struct loaded_index_info *index,
    bool *out_has_null_part
) {
    *out_has_null_part = false;

    for (size_t part_index = 0U; part_index < index->part_count; ++part_index) {
        size_t column_index = index->parts[part_index].column_index;

        if (column_index >= plan->column_count) {
            set_runtime_error(database, "invalid unique-index descriptor");
            return MYLITE_ERROR;
        }
        if (plan->rows[row_index].values[column_index].is_null) {
            *out_has_null_part = true;
            return MYLITE_OK;
        }
    }

    return MYLITE_OK;
}

static int apply_insert_duplicate_key_update(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    const struct loaded_index_info *conflicting_index,
    struct insert_execution_counters *counters
) {
    sqlite3_stmt *statement = NULL;
    struct insert_duplicate_assignment_values assignment_values = {0};
    struct planned_value *current_values = NULL;
    char *sql = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = convert_insert_duplicate_update_values(
        database,
        plan,
        row_index,
        conflicting_index,
        &assignment_values,
        &current_values
    );

    if (rc == MYLITE_OK) {
        rc = validate_insert_duplicate_key_assignment_conflicts(
            database,
            plan,
            row_index,
            conflicting_index,
            (struct insert_duplicate_projected_row_inputs){
                .current_values = current_values,
                .assignment_values = assignment_values.values,
            }
        );
    }
    if (rc == MYLITE_OK && count_executable_insert_duplicate_assignments(plan) == 0U) {
        insert_duplicate_assignment_values_deinit(&assignment_values);
        insert_duplicate_update_values_deinit(&current_values, plan->column_count);
        return MYLITE_OK;
    }
    if (rc == MYLITE_OK) {
        rc = build_insert_duplicate_update_sql(
            plan,
            conflicting_index,
            assignment_values.values,
            &sql
        );
    }
    if (rc == MYLITE_OK) {
        rc = prepare_cached_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_insert_duplicate_update_parameters(
            database,
            statement,
            plan,
            row_index,
            conflicting_index,
            assignment_values.values
        );
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            if (sqlite3_changes64(database->sqlite) > 0) {
                counters->affected_rows += 2;
            }
        } else if (sqlite_status_is_constraint(sqlite_rc)) {
            rc = handle_insert_duplicate_update_constraint(database, sqlite_rc, plan, row_index);
        } else {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finish_cached_sqlite_statement(database, statement, rc);
    insert_duplicate_assignment_values_deinit(&assignment_values);
    insert_duplicate_update_values_deinit(&current_values, plan->column_count);
    free(sql);

    return rc;
}

static int handle_insert_duplicate_update_constraint(
    struct mylite_db *database,
    int sqlite_rc,
    const struct planned_insert *plan,
    size_t row_index
) {
    bool was_generated_range_violation = false;
    bool was_check_violation = false;
    bool was_generated_not_null_violation = false;
    int rc = handle_generated_range_constraint_violation(
        database,
        (struct generated_constraint_columns){
            .columns = plan->columns,
            .column_count = plan->column_count,
        },
        row_index + 1U,
        &was_generated_range_violation
    );

    if (rc == MYLITE_OK && !was_generated_range_violation) {
        rc = handle_check_constraint_violation(
            database,
            plan->table.table_id,
            false,
            &was_check_violation
        );
    }
    if (rc == MYLITE_OK && !was_check_violation && !was_generated_range_violation) {
        rc = handle_generated_not_null_violation(
            database,
            sqlite_rc,
            plan->columns,
            plan->column_count,
            &was_generated_not_null_violation
        );
    }
    if (rc == MYLITE_OK && !was_check_violation && !was_generated_range_violation &&
        !was_generated_not_null_violation) {
        rc = mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    return rc;
}

static int validate_insert_duplicate_key_assignment_conflicts(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    const struct loaded_index_info *conflicting_index,
    struct insert_duplicate_projected_row_inputs inputs
) {
    struct planned_value *owned_current_values = NULL;
    struct planned_value *projected_values = NULL;
    const struct planned_value *current_values = inputs.current_values;
    int rc = MYLITE_OK;

    if (!insert_duplicate_assignment_requires_key_conflict_validation(plan, conflicting_index)) {
        return MYLITE_OK;
    }

    if (current_values == NULL) {
        rc = fetch_insert_duplicate_current_row_values(
            database,
            plan,
            row_index,
            conflicting_index,
            &owned_current_values
        );
        current_values = owned_current_values;
    }
    if (rc == MYLITE_OK) {
        rc = project_insert_duplicate_updated_row_values(
            database,
            plan,
            (struct insert_duplicate_projected_row_inputs){
                .current_values = current_values,
                .assignment_values = inputs.assignment_values,
            },
            &projected_values
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < plan->index_count; ++index) {
        const struct loaded_index_info *candidate = &plan->indexes[index];
        bool has_null_part = false;
        bool exists = false;
        char value_text[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY] = "";

        if (!candidate->index.is_unique || candidate->part_count == 0U ||
            !insert_duplicate_index_has_assignment_requiring_validation(
                plan,
                candidate,
                conflicting_index
            )) {
            continue;
        }
        rc = insert_duplicate_key_tuple_has_null_part(
            plan,
            candidate,
            projected_values,
            &has_null_part
        );
        if (rc != MYLITE_OK || has_null_part) {
            continue;
        }
        rc = unique_key_tuple_exists_excluding_insert_duplicate_row(
            database,
            plan,
            row_index,
            candidate,
            conflicting_index,
            projected_values,
            &exists
        );
        if (rc == MYLITE_OK && exists) {
            rc = format_key_tuple(
                candidate,
                projected_values,
                plan->column_count,
                value_text,
                sizeof(value_text)
            );
            if (rc == MYLITE_OK) {
                set_duplicate_key_error(
                    database,
                    plan->target.table_name,
                    candidate->index.name,
                    value_text
                );
                rc = MYLITE_ERROR;
            }
        }
    }

    insert_duplicate_update_values_deinit(&projected_values, plan->column_count);
    insert_duplicate_update_values_deinit(&owned_current_values, plan->column_count);
    return rc;
}

static bool insert_duplicate_assignment_requires_key_conflict_validation(
    const struct planned_insert *plan,
    const struct loaded_index_info *conflicting_index
) {
    if (plan == NULL) {
        return false;
    }

    for (size_t index = 0U; index < plan->index_count; ++index) {
        const struct loaded_index_info *key = &plan->indexes[index];

        if (!key->index.is_unique) {
            continue;
        }
        if (insert_duplicate_index_has_assignment_requiring_validation(
                plan,
                key,
                conflicting_index
            )) {
            return true;
        }
    }

    return false;
}

static bool insert_duplicate_index_has_assignment_requiring_validation(
    const struct planned_insert *plan,
    const struct loaded_index_info *index,
    const struct loaded_index_info *conflicting_index
) {
    if (plan == NULL || index == NULL) {
        return false;
    }

    for (size_t part_index = 0U; part_index < index->part_count; ++part_index) {
        size_t column_index = index->parts[part_index].column_index;

        for (size_t assignment_index = 0U;
             assignment_index < plan->duplicate_update.assignment_count;
             ++assignment_index) {
            const struct planned_insert_duplicate_update_assignment *assignment =
                &plan->duplicate_update.assignments[assignment_index];

            if (insert_duplicate_assignment_is_noop(assignment) ||
                assignment->assignment_column_index != column_index) {
                continue;
            }
            if (index == conflicting_index &&
                insert_duplicate_assignment_is_same_column_values_reference(assignment)) {
                continue;
            }
            return true;
        }
    }

    return false;
}

static int fetch_insert_duplicate_current_row_values(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    const struct loaded_index_info *conflicting_index,
    struct planned_value **out_values
) {
    sqlite3_stmt *statement = NULL;
    struct planned_value *values = NULL;
    char *sql = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = MYLITE_OK;

    *out_values = NULL;
    if (plan->column_count > SIZE_MAX / sizeof(*values)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    values = calloc(plan->column_count, sizeof(*values));
    if (values == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = build_insert_duplicate_current_row_select_sql(plan, conflicting_index, &sql);
    if (rc == MYLITE_OK) {
        rc = prepare_cached_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        int parameter_index = 1;

        rc = bind_insert_duplicate_key_tuple_parameters(
            statement,
            &parameter_index,
            conflicting_index,
            plan->rows[row_index].values,
            plan->column_count
        );
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
                 ++column_index) {
                rc = materialize_insert_select_selected_value(
                    database,
                    statement,
                    (int)column_index,
                    &plan->columns[column_index],
                    &plan->columns[column_index],
                    row_index + 1U,
                    false,
                    &values[column_index]
                );
            }
        } else if (sqlite_rc == SQLITE_DONE) {
            set_physical_sqlite_row_error(database);
            rc = MYLITE_ERROR;
        } else {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finish_cached_sqlite_statement(database, statement, rc);
    free(sql);
    if (rc != MYLITE_OK) {
        insert_duplicate_update_values_deinit(&values, plan->column_count);
        return rc;
    }

    *out_values = values;
    return MYLITE_OK;
}

static int build_insert_duplicate_current_row_select_sql(
    const struct planned_insert *plan,
    const struct loaded_index_info *conflicting_index,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT ");
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        if (column_index != 0U) {
            rc = mylite_dynamic_string_append(&string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(
                &string,
                plan->columns[column_index].name
            );
        }
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
    if (rc == MYLITE_OK) {
        rc = append_insert_duplicate_key_predicate_sql_from_parameters(
            &string,
            conflicting_index,
            1U
        );
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

static int project_insert_duplicate_updated_row_values(
    struct mylite_db *database,
    const struct planned_insert *plan,
    struct insert_duplicate_projected_row_inputs inputs,
    struct planned_value **out_values
) {
    struct planned_value *values = NULL;
    int rc = MYLITE_OK;

    *out_values = NULL;
    if (plan->column_count > SIZE_MAX / sizeof(*values)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    values = calloc(plan->column_count, sizeof(*values));
    if (values == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        const struct planned_value *source = &inputs.current_values[column_index];

        for (size_t assignment_index = 0U;
             assignment_index < plan->duplicate_update.assignment_count;
             ++assignment_index) {
            if (insert_duplicate_assignment_is_noop(
                    &plan->duplicate_update.assignments[assignment_index]
                )) {
                continue;
            }
            if (plan->duplicate_update.assignments[assignment_index].assignment_column_index ==
                column_index) {
                source = &inputs.assignment_values[assignment_index];
                break;
            }
        }
        rc = copy_planned_value(database, source, &values[column_index]);
    }
    if (rc != MYLITE_OK) {
        insert_duplicate_update_values_deinit(&values, plan->column_count);
        return rc;
    }

    *out_values = values;
    return MYLITE_OK;
}

static int insert_duplicate_key_tuple_has_null_part(
    const struct planned_insert *plan,
    const struct loaded_index_info *index,
    const struct planned_value *values,
    bool *out_has_null_part
) {
    *out_has_null_part = false;

    for (size_t part_index = 0U; part_index < index->part_count; ++part_index) {
        size_t column_index = index->parts[part_index].column_index;

        if (column_index >= plan->column_count) {
            return MYLITE_ERROR;
        }
        if (values[column_index].is_null) {
            *out_has_null_part = true;
            return MYLITE_OK;
        }
    }

    return MYLITE_OK;
}

static int unique_key_tuple_exists_excluding_insert_duplicate_row(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    const struct loaded_index_info *candidate,
    const struct loaded_index_info *conflicting_index,
    const struct planned_value *projected_values,
    bool *out_exists
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int parameter_index = 1;
    int sqlite_rc = SQLITE_OK;
    int rc = MYLITE_OK;

    *out_exists = false;
    rc = build_unique_key_lookup_excluding_insert_duplicate_row_sql(
        plan,
        candidate,
        conflicting_index,
        &sql
    );
    if (rc == MYLITE_OK) {
        rc = prepare_cached_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_insert_duplicate_key_tuple_parameters(
            statement,
            &parameter_index,
            candidate,
            projected_values,
            plan->column_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = bind_insert_duplicate_key_tuple_parameters(
            statement,
            &parameter_index,
            conflicting_index,
            plan->rows[row_index].values,
            plan->column_count
        );
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            *out_exists = true;
        } else if (sqlite_rc != SQLITE_DONE) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finish_cached_sqlite_statement(database, statement, rc);
    free(sql);

    return rc;
}

static int build_unique_key_lookup_excluding_insert_duplicate_row_sql(
    const struct planned_insert *plan,
    const struct loaded_index_info *candidate,
    const struct loaded_index_info *conflicting_index,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT 1 FROM ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " WHERE ");
    }
    if (rc == MYLITE_OK) {
        rc = append_insert_duplicate_key_predicate_sql_from_parameters(&string, candidate, 1U);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " AND NOT (");
    }
    if (rc == MYLITE_OK) {
        rc = append_insert_duplicate_key_predicate_sql_from_parameters(
            &string,
            conflicting_index,
            candidate->part_count + 1U
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, ") LIMIT 1");
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

static int convert_insert_duplicate_update_value(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    const struct planned_value *current_values,
    const struct planned_insert_duplicate_update_assignment *duplicate_assignment,
    struct planned_value *out_value
) {
    const struct mylite_sql_ast_node *value_node = NULL;
    const struct mylite_catalog_column_descriptor *column = NULL;

    if (plan == NULL || duplicate_assignment == NULL || out_value == NULL) {
        set_runtime_error(database, "invalid insert duplicate assignment plan");
        return MYLITE_ERROR;
    }
    if (duplicate_assignment->value_kind ==
        PLANNED_INSERT_DUPLICATE_UPDATE_VALUE_VALUES_REFERENCE) {
        return copy_planned_value(
            database,
            &plan->rows[row_index].values[duplicate_assignment->values_column_index],
            out_value
        );
    }
    if (duplicate_assignment->value_kind ==
        PLANNED_INSERT_DUPLICATE_UPDATE_VALUE_SAME_COLUMN_ARITHMETIC) {
        return convert_insert_duplicate_arithmetic_value(
            database,
            plan,
            row_index,
            current_values,
            duplicate_assignment,
            out_value
        );
    }
    if (duplicate_assignment->value_kind == PLANNED_INSERT_DUPLICATE_UPDATE_VALUE_ROW_SCALAR) {
        planned_value_deinit(out_value);
        *out_value = (struct planned_value){.is_null = true};
        return MYLITE_OK;
    }
    if (insert_duplicate_assignment_is_noop(duplicate_assignment)) {
        planned_value_deinit(out_value);
        *out_value = (struct planned_value){.is_null = true};
        return MYLITE_OK;
    }

    value_node = unwrap_parenthesized_expression(duplicate_assignment->assignment_value_node);
    column = &plan->columns[duplicate_assignment->assignment_column_index];
    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    planned_value_deinit(out_value);
    *out_value = (struct planned_value){.is_null = true, .is_text = false, .integer = 0};
    if (value_node->kind == MYLITE_SQL_AST_LITERAL &&
        mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_NULL) {
        return convert_null_insert_value(
            database,
            column,
            insert_duplicate_update_allows_null_adjustment(database, plan, column),
            out_value
        );
    }

    return convert_insert_value_for_plan(
        database,
        plan,
        value_node,
        column,
        row_index + 1U,
        false,
        dml_allows_string_truncation_adjustment(database, false),
        out_value
    );
}

static int convert_insert_duplicate_arithmetic_value(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    const struct planned_value *current_values,
    const struct planned_insert_duplicate_update_assignment *duplicate_assignment,
    struct planned_value *out_value
) {
    const size_t column_index = duplicate_assignment->assignment_column_index;
    const struct planned_value *current_value = NULL;
    uint64_t delta = 0U;
    int rc = MYLITE_OK;

    if (current_values == NULL || column_index >= plan->column_count) {
        set_runtime_error(database, "invalid insert duplicate arithmetic assignment plan");
        return MYLITE_ERROR;
    }
    current_value = &current_values[column_index];
    planned_value_deinit(out_value);
    if (current_value->is_null) {
        *out_value = (struct planned_value){.is_null = true};
        return MYLITE_OK;
    }
    if (current_value->is_text || current_value->is_blob || current_value->is_real) {
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }

    rc = parse_insert_duplicate_arithmetic_delta(
        database,
        duplicate_assignment->arithmetic_delta_node,
        &delta
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    struct insert_duplicate_arithmetic_result_request request = {
        .column = &plan->columns[column_index],
        .row_number = row_index + 1U,
        .operator_kind = duplicate_assignment->arithmetic_operator,
        .current_value = current_value->integer,
        .delta = delta,
    };

    return assign_insert_duplicate_arithmetic_result(database, &request, out_value);
}

static int parse_insert_duplicate_arithmetic_delta(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    uint64_t *out_delta
) {
    const uint64_t int64_positive_max = 9223372036854775807ULL;

    if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER ||
        parse_unsigned_integer_literal(&literal->span, out_delta) != MYLITE_OK ||
        *out_delta > int64_positive_max) {
        set_insert_duplicate_arithmetic_delta_out_of_range_error(database);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int assign_insert_duplicate_arithmetic_result(
    struct mylite_db *database,
    const struct insert_duplicate_arithmetic_result_request *request,
    struct planned_value *out_value
) {
    struct integer_column_range range = {0};
    enum update_arithmetic_range_error_kind error_kind = UPDATE_ARITHMETIC_RANGE_ERROR_COLUMN;
    int rc = MYLITE_OK;
    int64_t result = 0;

    if (request == NULL || request->column == NULL) {
        set_runtime_error(database, "invalid insert duplicate arithmetic assignment request");
        return MYLITE_ERROR;
    }
    rc = integer_range_for_column(
        database,
        request->column,
        "INSERT ... ON DUPLICATE KEY UPDATE arithmetic assignment supports only integer columns",
        &range
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (request->operator_kind == MYLITE_SQL_AST_OPERATOR_SUBTRACT &&
        update_arithmetic_column_is_unsigned(request->column)) {
        error_kind = UPDATE_ARITHMETIC_RANGE_ERROR_UNSIGNED_BIGINT;
    } else if (update_arithmetic_column_is_signed_bigint(request->column)) {
        error_kind = UPDATE_ARITHMETIC_RANGE_ERROR_SIGNED_BIGINT;
    }

    if (request->delta > update_arithmetic_domain_width(&range)) {
        set_insert_duplicate_arithmetic_range_error(
            database,
            &(struct insert_duplicate_arithmetic_range_error){
                .column = request->column,
                .row_number = request->row_number,
                .error_kind = error_kind,
            }
        );
        return MYLITE_ERROR;
    }
    if (request->operator_kind == MYLITE_SQL_AST_OPERATOR_ADD) {
        if (request->current_value > update_arithmetic_addition_threshold(&range, request->delta)) {
            set_insert_duplicate_arithmetic_range_error(
                database,
                &(struct insert_duplicate_arithmetic_range_error){
                    .column = request->column,
                    .row_number = request->row_number,
                    .error_kind = error_kind,
                }
            );
            return MYLITE_ERROR;
        }
        result = request->current_value + (int64_t)request->delta;
    } else if (request->operator_kind == MYLITE_SQL_AST_OPERATOR_SUBTRACT) {
        if (request->current_value <
            update_arithmetic_subtraction_threshold(&range, request->delta)) {
            set_insert_duplicate_arithmetic_range_error(
                database,
                &(struct insert_duplicate_arithmetic_range_error){
                    .column = request->column,
                    .row_number = request->row_number,
                    .error_kind = error_kind,
                }
            );
            return MYLITE_ERROR;
        }
        result = request->current_value - (int64_t)request->delta;
    } else {
        set_insert_duplicate_arithmetic_unsupported_error(database);
        return MYLITE_ERROR;
    }

    *out_value = (struct planned_value){.is_null = false, .is_text = false, .integer = result};
    return MYLITE_OK;
}

static void set_insert_duplicate_arithmetic_delta_out_of_range_error(struct mylite_db *database) {
    set_unsupported_error(
        database,
        "INSERT ... ON DUPLICATE KEY UPDATE arithmetic assignment supports only unsigned integer "
        "deltas in signed 64-bit range"
    );
}

static void set_insert_duplicate_arithmetic_range_error(
    struct mylite_db *database,
    const struct insert_duplicate_arithmetic_range_error *error
) {
    if (error == NULL || error->column == NULL) {
        set_runtime_error(database, "invalid insert duplicate arithmetic range error");
        return;
    }
    if (error->error_kind == UPDATE_ARITHMETIC_RANGE_ERROR_SIGNED_BIGINT) {
        mylite_diagnostics_set_error(
            mylite_connection_diagnostics(database),
            mysql_error_bigint_out_of_range,
            "22003",
            "BIGINT value is out of range in INSERT ... ON DUPLICATE KEY UPDATE arithmetic "
            "assignment"
        );
        return;
    }
    if (error->error_kind == UPDATE_ARITHMETIC_RANGE_ERROR_UNSIGNED_BIGINT) {
        mylite_diagnostics_set_error(
            mylite_connection_diagnostics(database),
            mysql_error_bigint_out_of_range,
            "22003",
            "BIGINT UNSIGNED value is out of range in INSERT ... ON DUPLICATE KEY UPDATE "
            "arithmetic assignment"
        );
        return;
    }

    set_out_of_range_error(database, error->column->name, error->row_number);
}

static bool insert_duplicate_update_allows_null_adjustment(
    const struct mylite_db *database,
    const struct planned_insert *plan,
    const struct mylite_catalog_column_descriptor *column
) {
    if (database == NULL || plan == NULL || column == NULL) {
        return false;
    }
    if (plan->row_count <= 1U || session_sql_mode_is_strict(database)) {
        return false;
    }
    return strcmp(column->physical_type, "INTEGER") == 0;
}

static int convert_insert_duplicate_update_values(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    const struct loaded_index_info *conflicting_index,
    struct insert_duplicate_assignment_values *out_values,
    struct planned_value **out_current_values
) {
    struct planned_value *current_values = NULL;
    struct planned_value *values = NULL;
    int rc = MYLITE_OK;

    *out_values = (struct insert_duplicate_assignment_values){0};
    *out_current_values = NULL;
    if (plan->duplicate_update.assignment_count == 0U ||
        plan->duplicate_update.assignments == NULL) {
        set_runtime_error(database, "invalid insert duplicate assignment plan");
        return MYLITE_ERROR;
    }
    if (plan->duplicate_update.assignment_count > SIZE_MAX / sizeof(*values)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    values = calloc(plan->duplicate_update.assignment_count, sizeof(*values));
    if (values == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_values->values = values;
    out_values->assignments = plan->duplicate_update.assignments;
    out_values->count = plan->duplicate_update.assignment_count;

    if (insert_duplicate_update_has_same_column_arithmetic(plan)) {
        rc = fetch_insert_duplicate_current_row_values(
            database,
            plan,
            row_index,
            conflicting_index,
            &current_values
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < plan->duplicate_update.assignment_count;
         ++index) {
        const struct planned_insert_duplicate_update_assignment *assignment =
            &plan->duplicate_update.assignments[index];

        if (assignment->value_kind == PLANNED_INSERT_DUPLICATE_UPDATE_VALUE_VALUES_REFERENCE) {
            values[index] = plan->rows[row_index].values[assignment->values_column_index];
        } else {
            rc = convert_insert_duplicate_update_value(
                database,
                plan,
                row_index,
                current_values,
                assignment,
                &values[index]
            );
        }
    }
    if (rc != MYLITE_OK) {
        insert_duplicate_update_values_deinit(&current_values, plan->column_count);
        insert_duplicate_assignment_values_deinit(out_values);
        return rc;
    }

    *out_current_values = current_values;
    return MYLITE_OK;
}

static bool insert_duplicate_update_has_same_column_arithmetic(const struct planned_insert *plan) {
    if (plan == NULL) {
        return false;
    }
    for (size_t index = 0U; index < plan->duplicate_update.assignment_count; ++index) {
        if (plan->duplicate_update.assignments[index].value_kind ==
            PLANNED_INSERT_DUPLICATE_UPDATE_VALUE_SAME_COLUMN_ARITHMETIC) {
            return true;
        }
    }

    return false;
}

static void insert_duplicate_update_values_deinit(
    struct planned_value **values,
    size_t value_count
) {
    if (values == NULL || *values == NULL) {
        return;
    }

    for (size_t index = 0U; index < value_count; ++index) {
        planned_value_deinit(&(*values)[index]);
    }
    free(*values);
    *values = NULL;
}

static void insert_duplicate_assignment_values_deinit(
    struct insert_duplicate_assignment_values *values
) {
    if (values == NULL) {
        return;
    }
    if (values->values != NULL) {
        for (size_t index = 0U; index < values->count; ++index) {
            if (values->assignments == NULL ||
                values->assignments[index].value_kind !=
                    PLANNED_INSERT_DUPLICATE_UPDATE_VALUE_VALUES_REFERENCE) {
                planned_value_deinit(&values->values[index]);
            }
        }
    }
    free(values->values);
    *values = (struct insert_duplicate_assignment_values){0};
}

static int copy_planned_value(
    struct mylite_db *database,
    const struct planned_value *source,
    struct planned_value *out_value
) {
    planned_value_deinit(out_value);
    if (source->is_text) {
        return copy_text_value(database, source->text, out_value);
    }
    if (source->is_blob) {
        return copy_blob_value(database, source->text, source->text_length, out_value);
    }

    *out_value = *source;
    out_value->text = NULL;
    out_value->text_length = 0U;
    return MYLITE_OK;
}

static int materialize_insert_duplicate_row_scalar_expression(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    const struct planned_row_scalar_expression *source,
    struct planned_row_scalar_expression *out_expression
) {
    int rc = MYLITE_OK;

    if (database == NULL || plan == NULL || source == NULL || out_expression == NULL ||
        row_index >= plan->row_count) {
        return MYLITE_MISUSE;
    }
    planned_row_scalar_expression_deinit(out_expression);
    *out_expression = *source;
    out_expression->arguments = NULL;
    out_expression->argument_count = 0U;
    out_expression->value = (struct planned_value){0};
    out_expression->window_partition_column = NULL;
    out_expression->window_order_column = NULL;
    out_expression->column = NULL;

    if (source->window_partition_column != NULL) {
        rc = retain_planned_row_scalar_column(
            database,
            &out_expression->window_partition_column,
            source->window_partition_column
        );
    }
    if (rc == MYLITE_OK && source->window_order_column != NULL) {
        rc = retain_planned_row_scalar_column(
            database,
            &out_expression->window_order_column,
            source->window_order_column
        );
    }
    if (rc == MYLITE_OK && source->column != NULL) {
        rc = retain_planned_row_scalar_column(database, &out_expression->column, source->column);
    }
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(out_expression);
        return rc;
    }

    if (source->kind == PLANNED_ROW_SCALAR_EXPRESSION_VALUE &&
        source->value_is_insert_values_reference) {
        if (source->value_source_index >= plan->column_count) {
            set_runtime_error(database, "invalid INSERT VALUES() row-scalar reference");
            return MYLITE_ERROR;
        }
        rc = copy_planned_value(
            database,
            &plan->rows[row_index].values[source->value_source_index],
            &out_expression->value
        );
        out_expression->value_is_insert_values_reference = false;
        out_expression->value_source_index = 0U;
    } else {
        rc = copy_planned_value(database, &source->value, &out_expression->value);
    }
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(out_expression);
        return rc;
    }

    if (source->argument_count == 0U) {
        return MYLITE_OK;
    }
    if (source->argument_count > SIZE_MAX / sizeof(*out_expression->arguments)) {
        set_nomem_error(database);
        planned_row_scalar_expression_deinit(out_expression);
        return MYLITE_NOMEM;
    }
    out_expression->arguments = (struct planned_row_scalar_expression *)
        calloc(source->argument_count, sizeof(*out_expression->arguments));
    if (out_expression->arguments == NULL) {
        set_nomem_error(database);
        planned_row_scalar_expression_deinit(out_expression);
        return MYLITE_NOMEM;
    }
    out_expression->argument_count = source->argument_count;
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < source->argument_count;
         ++argument_index) {
        rc = materialize_insert_duplicate_row_scalar_expression(
            database,
            plan,
            row_index,
            &source->arguments[argument_index],
            &out_expression->arguments[argument_index]
        );
    }
    if (rc != MYLITE_OK) {
        planned_row_scalar_expression_deinit(out_expression);
    }
    return rc;
}

static int build_insert_duplicate_update_sql(
    const struct planned_insert *plan,
    const struct loaded_index_info *conflicting_index,
    const struct planned_value *assignment_values,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    size_t executable_assignment_count = count_executable_insert_duplicate_assignments(plan);
    size_t next_parameter = 1U;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    if (conflicting_index == NULL || conflicting_index->part_count == 0U) {
        mylite_dynamic_string_deinit(&string);
        return MYLITE_ERROR;
    }
    if (executable_assignment_count == 0U) {
        mylite_dynamic_string_deinit(&string);
        return MYLITE_ERROR;
    }
    rc = mylite_dynamic_string_append(&string, "UPDATE ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " SET ");
    }
    if (rc == MYLITE_OK) {
        rc = append_insert_duplicate_assignments_sql(&string, plan, &next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " WHERE ");
    }
    if (rc == MYLITE_OK) {
        rc = append_insert_duplicate_key_predicate_sql(&string, conflicting_index, &next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " AND ");
    }
    if (rc == MYLITE_OK) {
        rc = append_insert_duplicate_changed_conditions_sql(
            &string,
            plan,
            assignment_values,
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

static int append_insert_duplicate_key_predicate_sql(
    struct mylite_dynamic_string *string,
    const struct loaded_index_info *conflicting_index,
    size_t *next_parameter
) {
    int rc = append_insert_duplicate_key_predicate_sql_from_parameters(
        string,
        conflicting_index,
        *next_parameter
    );

    if (rc == MYLITE_OK) {
        *next_parameter += conflicting_index->part_count;
    }
    return rc;
}

static int append_insert_duplicate_key_predicate_sql_from_parameters(
    struct mylite_dynamic_string *string,
    const struct loaded_index_info *index,
    size_t first_parameter
) {
    int rc = MYLITE_OK;

    if (index == NULL || index->part_count == 0U) {
        return MYLITE_ERROR;
    }
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < index->part_count; ++part_index) {
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(string, " AND ");
        }
        if (rc == MYLITE_OK) {
            rc = append_loaded_key_part_sql(string, &index->parts[part_index], NULL);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " = ");
        }
        if (rc == MYLITE_OK) {
            rc = append_loaded_key_part_parameter_sql(
                string,
                &index->parts[part_index],
                first_parameter + part_index
            );
        }
    }

    return rc;
}

static int append_insert_duplicate_assignments_sql(
    struct mylite_dynamic_string *string,
    const struct planned_insert *plan,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;
    bool has_previous = false;

    for (size_t index = 0U; rc == MYLITE_OK && index < plan->duplicate_update.assignment_count;
         ++index) {
        const size_t column_index =
            plan->duplicate_update.assignments[index].assignment_column_index;

        if (insert_duplicate_assignment_is_noop(&plan->duplicate_update.assignments[index])) {
            continue;
        }
        if (has_previous) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(
                string,
                plan->columns[column_index].name
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " = ");
        }
        if (rc == MYLITE_OK && plan->duplicate_update.assignments[index].value_kind ==
                                   PLANNED_INSERT_DUPLICATE_UPDATE_VALUE_ROW_SCALAR) {
            rc = append_row_scalar_expression_sql(
                string,
                plan->duplicate_update.assignments[index].row_scalar_expression,
                next_parameter
            );
        } else if (rc == MYLITE_OK) {
            rc = append_numbered_parameter(string, *next_parameter);
            ++(*next_parameter);
        }
        has_previous = true;
    }

    return rc;
}

static int append_insert_duplicate_changed_conditions_sql(
    struct mylite_dynamic_string *string,
    const struct planned_insert *plan,
    const struct planned_value *assignment_values,
    size_t *next_parameter
) {
    int rc = mylite_dynamic_string_append_char(string, '(');
    bool has_previous = false;

    for (size_t index = 0U; rc == MYLITE_OK && index < plan->duplicate_update.assignment_count;
         ++index) {
        const size_t column_index =
            plan->duplicate_update.assignments[index].assignment_column_index;

        if (insert_duplicate_assignment_is_noop(&plan->duplicate_update.assignments[index])) {
            continue;
        }
        if (has_previous) {
            rc = mylite_dynamic_string_append(string, " OR ");
        }
        if (rc == MYLITE_OK && plan->duplicate_update.assignments[index].value_kind ==
                                   PLANNED_INSERT_DUPLICATE_UPDATE_VALUE_ROW_SCALAR) {
            rc = append_update_assignment_row_scalar_changed_condition_sql(
                string,
                &plan->columns[column_index],
                plan->duplicate_update.assignments[index].row_scalar_expression,
                next_parameter
            );
        } else if (rc == MYLITE_OK) {
            rc = append_insert_duplicate_changed_condition_sql(
                string,
                &plan->columns[column_index],
                &assignment_values[index],
                next_parameter
            );
        }
        has_previous = true;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }

    return rc;
}

static int append_insert_duplicate_changed_condition_sql(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_column_descriptor *column,
    const struct planned_value *assignment_value,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (assignment_value->is_null) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, column->name);
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " IS NOT NULL");
        }
        return rc;
    }

    rc = mylite_dynamic_string_append_char(string, '(');
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, column->name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " IS NULL OR ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, column->name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " <> ");
    }
    if (rc == MYLITE_OK) {
        rc = append_numbered_parameter(string, *next_parameter);
    }
    if (rc == MYLITE_OK) {
        ++(*next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }

    return rc;
}

static int bind_insert_duplicate_update_parameters(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_insert *plan,
    size_t row_index,
    const struct loaded_index_info *conflicting_index,
    const struct planned_value *assignment_values
) {
    int parameter_index = 1;
    int rc = bind_insert_duplicate_assignment_parameters(
        database,
        statement,
        &parameter_index,
        plan,
        row_index,
        assignment_values
    );

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < conflicting_index->part_count;
         ++part_index) {
        size_t key_column_index = conflicting_index->parts[part_index].column_index;

        if (key_column_index >= plan->column_count) {
            return MYLITE_ERROR;
        }
        rc = bind_planned_value_parameter(
            statement,
            parameter_index,
            &plan->rows[row_index].values[key_column_index]
        );
        if (rc == MYLITE_OK) {
            ++parameter_index;
        }
    }
    if (rc == MYLITE_OK) {
        rc = bind_insert_duplicate_changed_condition_parameters(
            database,
            statement,
            &parameter_index,
            plan,
            row_index,
            assignment_values
        );
    }

    return rc;
}

static int bind_insert_duplicate_assignment_parameters(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int *parameter_index,
    const struct planned_insert *plan,
    size_t row_index,
    const struct planned_value *assignment_values
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < plan->duplicate_update.assignment_count;
         ++index) {
        if (insert_duplicate_assignment_is_noop(&plan->duplicate_update.assignments[index])) {
            continue;
        }
        if (plan->duplicate_update.assignments[index].value_kind ==
            PLANNED_INSERT_DUPLICATE_UPDATE_VALUE_ROW_SCALAR) {
            struct planned_row_scalar_expression expression = {0};

            rc = materialize_insert_duplicate_row_scalar_expression(
                database,
                plan,
                row_index,
                plan->duplicate_update.assignments[index].row_scalar_expression,
                &expression
            );
            if (rc == MYLITE_OK) {
                rc = bind_row_scalar_expression_parameters(statement, &expression, parameter_index);
            }
            planned_row_scalar_expression_deinit(&expression);
            continue;
        }
        rc = bind_planned_value_parameter(statement, *parameter_index, &assignment_values[index]);
        if (rc == MYLITE_OK) {
            ++(*parameter_index);
        }
    }

    return rc;
}

static int bind_insert_duplicate_key_tuple_parameters(
    sqlite3_stmt *statement,
    int *parameter_index,
    const struct loaded_index_info *index,
    const struct planned_value *values,
    size_t value_count
) {
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < index->part_count; ++part_index) {
        size_t column_index = index->parts[part_index].column_index;

        if (column_index >= value_count) {
            return MYLITE_ERROR;
        }
        rc = bind_planned_value_parameter(statement, *parameter_index, &values[column_index]);
        if (rc == MYLITE_OK) {
            ++(*parameter_index);
        }
    }

    return rc;
}

static int bind_insert_duplicate_changed_condition_parameters(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int *parameter_index,
    const struct planned_insert *plan,
    size_t row_index,
    const struct planned_value *assignment_values
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < plan->duplicate_update.assignment_count;
         ++index) {
        if (insert_duplicate_assignment_is_noop(&plan->duplicate_update.assignments[index])) {
            continue;
        }
        if (plan->duplicate_update.assignments[index].value_kind ==
            PLANNED_INSERT_DUPLICATE_UPDATE_VALUE_ROW_SCALAR) {
            struct planned_row_scalar_expression expression = {0};

            rc = materialize_insert_duplicate_row_scalar_expression(
                database,
                plan,
                row_index,
                plan->duplicate_update.assignments[index].row_scalar_expression,
                &expression
            );
            if (rc == MYLITE_OK) {
                rc = bind_update_row_scalar_changed_condition_parameters(
                    statement,
                    parameter_index,
                    &expression
                );
            }
            planned_row_scalar_expression_deinit(&expression);
            continue;
        }
        if (assignment_values[index].is_null) {
            continue;
        }
        rc = bind_planned_value_parameter(statement, *parameter_index, &assignment_values[index]);
        if (rc == MYLITE_OK) {
            ++(*parameter_index);
        }
    }

    return rc;
}
