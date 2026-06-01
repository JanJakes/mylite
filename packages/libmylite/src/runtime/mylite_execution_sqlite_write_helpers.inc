static int execute_sqlite_schema_sql(struct mylite_db *database, const char *sql) {
    int sqlite_rc = sqlite3_exec(database->sqlite, sql, NULL, NULL, NULL);
    int rc = mylite_sqlite_status_to_mylite(sqlite_rc);

    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
        return rc;
    }
    if (rc != MYLITE_OK) {
        set_physical_sqlite_error(database);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int execute_sqlite_control_sql(const struct mylite_db *database, const char *sql) {
    int sqlite_rc = sqlite3_exec(database->sqlite, sql, NULL, NULL, NULL);

    return mylite_sqlite_status_to_mylite(sqlite_rc);
}

static int prepare_sqlite_statement(
    const struct mylite_db *database,
    const char *sql,
    sqlite3_stmt **out_statement
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;

    *out_statement = NULL;
    sqlite_rc = sqlite3_prepare_v2(
        database->sqlite,
        sql,
        sqlite_use_nul_terminated_string,
        &statement,
        NULL
    );
    if (sqlite_rc != SQLITE_OK) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    *out_statement = statement;

    return MYLITE_OK;
}

static int finalize_sqlite_statement(sqlite3_stmt *statement, int rc) {
    int sqlite_rc = SQLITE_OK;

    if (statement == NULL) {
        return rc;
    }

    sqlite_rc = sqlite3_finalize(statement);
    if (rc != MYLITE_OK) {
        return rc;
    }

    return mylite_sqlite_status_to_mylite(sqlite_rc);
}

static int bind_insert_row(sqlite3_stmt *statement, const struct planned_insert *plan, size_t row) {
    int sqlite_rc = sqlite3_reset(statement);

    if (sqlite_rc == SQLITE_OK) {
        sqlite_rc = sqlite3_clear_bindings(statement);
    }
    if (sqlite_rc != SQLITE_OK) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    int bind_index = 1;

    for (size_t column_index = 0U; column_index < plan->column_count; ++column_index) {
        const struct planned_value *value = &plan->rows[row].values[column_index];
        int rc = MYLITE_OK;

        if (plan->columns[column_index].is_generated) {
            continue;
        }
        rc = bind_planned_value_parameter(statement, bind_index, value);
        if (rc != MYLITE_OK) {
            return rc;
        }
        ++bind_index;
    }

    return MYLITE_OK;
}

static int step_insert_row(sqlite3_stmt *statement, int *out_sqlite_rc) {
    int sqlite_rc = sqlite3_step(statement);

    if (out_sqlite_rc != NULL) {
        *out_sqlite_rc = sqlite_rc;
    }
    if (sqlite_rc != SQLITE_DONE) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    return MYLITE_OK;
}

static bool sqlite_status_is_constraint(int sqlite_rc) {
    enum { sqlite_primary_result_code_mask = 0xff };

    return (sqlite_rc == SQLITE_CONSTRAINT ||
            (sqlite_rc & sqlite_primary_result_code_mask) == SQLITE_CONSTRAINT) != 0;
}

static int reset_insert_statement_after_constraint(sqlite3_stmt *statement) {
    int reset_rc = sqlite3_reset(statement);

    if (reset_rc != SQLITE_OK && !sqlite_status_is_constraint(reset_rc)) {
        return mylite_sqlite_status_to_mylite(reset_rc);
    }

    return MYLITE_OK;
}

static int handle_check_constraint_violation(
    struct mylite_db *database,
    int64_t table_id,
    bool ignore_errors,
    bool *out_was_check_violation
) {
    const char *physical_name = NULL;
    struct mylite_catalog_check_constraint_descriptor check_constraint = {0};
    bool found = false;
    int rc = MYLITE_OK;

    *out_was_check_violation = false;
    if (!sqlite_error_message_has_check_constraint(
            sqlite3_errmsg(database->sqlite),
            &physical_name
        )) {
        return MYLITE_OK;
    }

    rc = mylite_catalog_try_read_check_constraint_by_physical_name(
        database,
        table_id,
        physical_name,
        &check_constraint,
        &found
    );
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to read CHECK constraint descriptor");
        return rc;
    }
    if (!found) {
        return MYLITE_OK;
    }

    *out_was_check_violation = true;
    return set_or_warn_check_constraint_violation(database, &check_constraint, ignore_errors);
}

static int handle_generated_not_null_violation(
    struct mylite_db *database,
    int sqlite_rc,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    bool *out_was_generated_not_null_violation
) {
    const char *column_name = NULL;
    size_t column_name_length = 0U;
    int extended_rc = SQLITE_OK;

    *out_was_generated_not_null_violation = false;
    extended_rc = database == NULL ? sqlite_rc : sqlite3_extended_errcode(database->sqlite);
    if (sqlite_rc != SQLITE_CONSTRAINT_NOTNULL && extended_rc != SQLITE_CONSTRAINT_NOTNULL) {
        return MYLITE_OK;
    }

    if (sqlite_error_message_has_not_null_column(
            sqlite3_errmsg(database->sqlite),
            &column_name,
            &column_name_length
        )) {
        for (size_t column_index = 0U; column_index < column_count; ++column_index) {
            const char *descriptor_name = columns[column_index].name;
            size_t descriptor_name_length = 0U;

            if (descriptor_name == NULL) {
                continue;
            }
            descriptor_name_length = strlen(descriptor_name);
            if (columns[column_index].is_generated && !columns[column_index].is_nullable &&
                descriptor_name_length == column_name_length &&
                strncmp(descriptor_name, column_name, column_name_length) == 0) {
                set_bad_null_error(database, descriptor_name);
                *out_was_generated_not_null_violation = true;
                return MYLITE_ERROR;
            }
        }
        return MYLITE_OK;
    }

    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        const char *descriptor_name = columns[column_index].name;

        if (descriptor_name != NULL && columns[column_index].is_generated &&
            !columns[column_index].is_nullable) {
            set_bad_null_error(database, descriptor_name);
            *out_was_generated_not_null_violation = true;
            return MYLITE_ERROR;
        }
    }

    return MYLITE_OK;
}

static int handle_generated_range_constraint_violation(
    struct mylite_db *database,
    struct generated_constraint_columns columns,
    size_t row_number,
    bool *out_was_generated_range_violation
) {
    const char *physical_name = NULL;
    size_t column_index = 0U;

    *out_was_generated_range_violation = false;
    if (!sqlite_error_message_has_check_constraint(
            sqlite3_errmsg(database->sqlite),
            &physical_name
        )) {
        return MYLITE_OK;
    }
    if (!generated_range_constraint_column_index(physical_name, &column_index) ||
        columns.columns == NULL || column_index >= columns.column_count ||
        !columns.columns[column_index].is_generated) {
        return MYLITE_OK;
    }

    set_out_of_range_error(database, columns.columns[column_index].name, row_number);
    *out_was_generated_range_violation = true;
    return MYLITE_ERROR;
}

static bool sqlite_error_message_has_check_constraint(
    const char *message,
    const char **out_physical_name
) {
    static const char prefix[] = "CHECK constraint failed: ";
    size_t prefix_length = sizeof(prefix) - 1U;

    if (out_physical_name != NULL) {
        *out_physical_name = NULL;
    }
    if (message == NULL || strncmp(message, prefix, prefix_length) != 0) {
        return false;
    }
    if (out_physical_name != NULL) {
        *out_physical_name = message + prefix_length;
    }
    return true;
}

static bool sqlite_error_message_has_not_null_column(
    const char *message,
    const char **out_column_name,
    size_t *out_column_name_length
) {
    static const char prefix[] = "NOT NULL constraint failed: ";
    const char *column_name = NULL;
    size_t prefix_length = sizeof(prefix) - 1U;
    size_t length = 0U;

    if (out_column_name != NULL) {
        *out_column_name = NULL;
    }
    if (out_column_name_length != NULL) {
        *out_column_name_length = 0U;
    }
    if (message == NULL || strncmp(message, prefix, prefix_length) != 0) {
        return false;
    }

    column_name = message + prefix_length;
    for (const char *cursor = column_name; *cursor != '\0'; ++cursor) {
        if (*cursor == '.') {
            column_name = cursor + 1;
        }
    }
    length = strlen(column_name);
    if (length == 0U) {
        return false;
    }
    if (out_column_name != NULL) {
        *out_column_name = column_name;
    }
    if (out_column_name_length != NULL) {
        *out_column_name_length = length;
    }
    return true;
}

static bool generated_range_constraint_column_index(
    const char *physical_name,
    size_t *out_column_index
) {
    enum { decimal_radix = 10U };

    static const char prefix[] = "_mylite_generated_column_range_";
    size_t prefix_length = sizeof(prefix) - 1U;
    size_t value = 0U;

    if (out_column_index != NULL) {
        *out_column_index = 0U;
    }
    if (physical_name == NULL || strncmp(physical_name, prefix, prefix_length) != 0 ||
        physical_name[prefix_length] == '\0') {
        return false;
    }
    for (size_t index = prefix_length; physical_name[index] != '\0'; ++index) {
        unsigned char byte = (unsigned char)physical_name[index];

        if (byte < '0' || byte > '9') {
            return false;
        }
        if (value > (SIZE_MAX - (size_t)(byte - '0')) / decimal_radix) {
            return false;
        }
        value = (value * decimal_radix) + (size_t)(byte - '0');
    }
    if (out_column_index != NULL) {
        *out_column_index = value;
    }
    return true;
}

static int set_or_warn_check_constraint_violation(
    struct mylite_db *database,
    const struct mylite_catalog_check_constraint_descriptor *check_constraint,
    bool ignore_errors
) {
    if (!ignore_errors) {
        set_check_constraint_violated_error(database, check_constraint->name);
        return MYLITE_ERROR;
    }

    return append_check_constraint_warning(database, check_constraint->name);
}

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
    struct planned_value *assignment_values = NULL;
    char *sql = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = convert_insert_duplicate_update_values(
        database,
        plan,
        row_index,
        conflicting_index,
        &assignment_values
    );

    if (rc == MYLITE_OK) {
        rc = validate_insert_duplicate_key_assignment_conflicts(
            database,
            plan,
            row_index,
            conflicting_index,
            assignment_values
        );
    }
    if (rc == MYLITE_OK && count_executable_insert_duplicate_assignments(plan) == 0U) {
        insert_duplicate_update_values_deinit(
            &assignment_values,
            plan->duplicate_update.assignment_count
        );
        return MYLITE_OK;
    }
    if (rc == MYLITE_OK) {
        rc = build_insert_duplicate_update_sql(plan, conflicting_index, assignment_values, &sql);
    }
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_insert_duplicate_update_parameters(
            statement,
            plan,
            row_index,
            conflicting_index,
            assignment_values
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
    rc = finalize_sqlite_statement(statement, rc);
    insert_duplicate_update_values_deinit(
        &assignment_values,
        plan->duplicate_update.assignment_count
    );
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
    const struct planned_value *assignment_values
) {
    struct planned_value *current_values = NULL;
    struct planned_value *projected_values = NULL;
    int rc = MYLITE_OK;

    if (!insert_duplicate_assignment_targets_key(plan)) {
        return MYLITE_OK;
    }

    rc = fetch_insert_duplicate_current_row_values(
        database,
        plan,
        row_index,
        conflicting_index,
        &current_values
    );
    if (rc == MYLITE_OK) {
        rc = project_insert_duplicate_updated_row_values(
            database,
            plan,
            (struct insert_duplicate_projected_row_inputs){
                .current_values = current_values,
                .assignment_values = assignment_values,
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
            !insert_duplicate_index_has_assigned_part(plan, candidate)) {
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
    insert_duplicate_update_values_deinit(&current_values, plan->column_count);
    return rc;
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
        rc = prepare_sqlite_statement(database, sql, &statement);
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
    rc = finalize_sqlite_statement(statement, rc);
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

static bool insert_duplicate_index_has_assigned_part(
    const struct planned_insert *plan,
    const struct loaded_index_info *index
) {
    if (plan == NULL || index == NULL) {
        return false;
    }

    for (size_t part_index = 0U; part_index < index->part_count; ++part_index) {
        for (size_t assignment_index = 0U;
             assignment_index < plan->duplicate_update.assignment_count;
             ++assignment_index) {
            if (insert_duplicate_assignment_is_noop(
                    &plan->duplicate_update.assignments[assignment_index]
                )) {
                continue;
            }
            if (index->parts[part_index].column_index ==
                plan->duplicate_update.assignments[assignment_index].assignment_column_index) {
                return true;
            }
        }
    }

    return false;
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
        rc = prepare_sqlite_statement(database, sql, &statement);
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
    rc = finalize_sqlite_statement(statement, rc);
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
    struct planned_value **out_values
) {
    struct planned_value *current_values = NULL;
    struct planned_value *values = NULL;
    int rc = MYLITE_OK;

    *out_values = NULL;
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
        rc = convert_insert_duplicate_update_value(
            database,
            plan,
            row_index,
            current_values,
            &plan->duplicate_update.assignments[index],
            &values[index]
        );
    }
    if (rc != MYLITE_OK) {
        insert_duplicate_update_values_deinit(&current_values, plan->column_count);
        insert_duplicate_update_values_deinit(&values, plan->duplicate_update.assignment_count);
        return rc;
    }

    insert_duplicate_update_values_deinit(&current_values, plan->column_count);
    *out_values = values;
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

static int build_insert_duplicate_update_sql(
    const struct planned_insert *plan,
    const struct loaded_index_info *conflicting_index,
    const struct planned_value *assignment_values,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    size_t executable_assignment_count = count_executable_insert_duplicate_assignments(plan);
    size_t next_parameter = 0U;
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
    next_parameter = executable_assignment_count + conflicting_index->part_count + 1U;

    rc = mylite_dynamic_string_append(&string, "UPDATE ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " SET ");
    }
    if (rc == MYLITE_OK) {
        rc = append_insert_duplicate_assignments_sql(&string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " WHERE ");
    }
    if (rc == MYLITE_OK) {
        rc = append_insert_duplicate_key_predicate_sql(&string, plan, conflicting_index);
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
    const struct planned_insert *plan,
    const struct loaded_index_info *conflicting_index
) {
    return append_insert_duplicate_key_predicate_sql_from_parameters(
        string,
        conflicting_index,
        count_executable_insert_duplicate_assignments(plan) + 1U
    );
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
    const struct planned_insert *plan
) {
    int rc = MYLITE_OK;
    size_t parameter_index = 1U;
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
        if (rc == MYLITE_OK) {
            rc = append_numbered_parameter(string, parameter_index);
        }
        has_previous = true;
        ++parameter_index;
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
        if (rc == MYLITE_OK) {
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
    sqlite3_stmt *statement,
    const struct planned_insert *plan,
    size_t row_index,
    const struct loaded_index_info *conflicting_index,
    const struct planned_value *assignment_values
) {
    int parameter_index = 1;
    int rc = bind_insert_duplicate_assignment_parameters(
        statement,
        &parameter_index,
        plan,
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
            statement,
            &parameter_index,
            plan,
            assignment_values
        );
    }

    return rc;
}

static int bind_insert_duplicate_assignment_parameters(
    sqlite3_stmt *statement,
    int *parameter_index,
    const struct planned_insert *plan,
    const struct planned_value *assignment_values
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < plan->duplicate_update.assignment_count;
         ++index) {
        if (insert_duplicate_assignment_is_noop(&plan->duplicate_update.assignments[index])) {
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
    sqlite3_stmt *statement,
    int *parameter_index,
    const struct planned_insert *plan,
    const struct planned_value *assignment_values
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < plan->duplicate_update.assignment_count;
         ++index) {
        if (insert_duplicate_assignment_is_noop(&plan->duplicate_update.assignments[index])) {
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

static int handle_update_unique_key_conflict(
    struct mylite_db *database,
    const struct planned_update *executable_plan,
    const struct planned_update *plan,
    int sqlite_rc
) {
    char value_text[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY] = "";
    const struct loaded_index_info *conflicting_index = NULL;
    int rc = MYLITE_OK;

    if (executable_plan->assignment_value.is_null) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    for (size_t index = 0U; rc == MYLITE_OK && index < plan->index_count; ++index) {
        const struct loaded_index_info *candidate = &plan->indexes[index];
        bool exists = false;
        bool found = false;

        if (!candidate->index.is_unique ||
            !loaded_index_contains_column_id(candidate, plan->assignment_column.column_id)) {
            continue;
        }
        if (conflicting_index == NULL) {
            conflicting_index = candidate;
        }
        if (candidate->part_count == 1U && !candidate->parts[0].index_column.has_prefix_length) {
            rc = unique_key_single_value_exists(
                database,
                plan->table.physical_name,
                candidate,
                &executable_plan->assignment_value,
                &exists
            );
            if (rc == MYLITE_OK && exists) {
                conflicting_index = candidate;
                rc = format_key_value(
                    &executable_plan->assignment_value,
                    value_text,
                    sizeof(value_text)
                );
                break;
            }
        } else {
            rc = find_update_unique_key_conflict_tuple(
                database,
                executable_plan,
                plan,
                candidate,
                value_text,
                sizeof(value_text),
                &found
            );
            if (rc == MYLITE_OK && found) {
                conflicting_index = candidate;
                break;
            }
        }
    }

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (conflicting_index == NULL) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }
    if (value_text[0] == '\0') {
        rc = format_key_value(&executable_plan->assignment_value, value_text, sizeof(value_text));
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    set_duplicate_key_error(
        database,
        plan->target.table_name,
        conflicting_index->index.name,
        value_text
    );
    return MYLITE_ERROR;
}

static int validate_child_foreign_keys_after_write(
    struct mylite_db *database,
    int64_t child_table_id
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_foreign_key_info *foreign_keys = NULL;
    size_t column_count = 0U;
    size_t foreign_key_count = 0U;
    int rc = MYLITE_OK;

    if (child_table_id <= 0) {
        return MYLITE_OK;
    }
    if (!foreign_key_checks_system_variable_value(database, false)) {
        return MYLITE_OK;
    }

    rc = load_table_columns(database, child_table_id, &columns, &column_count);

    if (rc == MYLITE_OK) {
        rc = load_table_foreign_key_infos(
            database,
            child_table_id,
            columns,
            column_count,
            &foreign_keys,
            &foreign_key_count
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < foreign_key_count; ++index) {
        rc = validate_foreign_key_references(database, &foreign_keys[index], false);
    }

    loaded_foreign_key_infos_deinit(&foreign_keys, &foreign_key_count);
    free(columns);
    return rc;
}

static int validate_parent_foreign_keys_after_write(
    struct mylite_db *database,
    int64_t parent_table_id
) {
    struct validate_parent_foreign_keys_context context = {
        .database = database,
        .rc = MYLITE_OK,
    };
    int rc = MYLITE_OK;

    if (parent_table_id <= 0) {
        return MYLITE_OK;
    }
    if (!foreign_key_checks_system_variable_value(database, false)) {
        return MYLITE_OK;
    }

    rc = mylite_catalog_for_each_foreign_key_for_parent_table(
        database,
        parent_table_id,
        validate_parent_foreign_key_descriptor,
        &context
    );

    if (rc == MYLITE_OK) {
        rc = context.rc;
    }
    return rc;
}

static int validate_parent_foreign_key_descriptor(
    const struct mylite_catalog_foreign_key_descriptor *foreign_key,
    void *user_data
) {
    struct validate_parent_foreign_keys_context *context = user_data;
    struct loaded_foreign_key_info loaded = {0};
    int rc = MYLITE_OK;

    if (foreign_key == NULL || context == NULL || context->database == NULL) {
        return MYLITE_MISUSE;
    }

    rc = load_foreign_key_info(context->database, foreign_key, NULL, 0U, &loaded);
    if (rc == MYLITE_OK) {
        rc = validate_foreign_key_references(context->database, &loaded, true);
    }
    loaded_foreign_key_info_deinit(&loaded);
    context->rc = rc;
    return rc;
}

static int validate_foreign_key_references(
    struct mylite_db *database,
    const struct loaded_foreign_key_info *foreign_key,
    bool parent_write
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = build_foreign_key_validation_sql(foreign_key, &sql);

    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            if (parent_write) {
                set_row_is_referenced_error(database);
            } else {
                set_no_referenced_row_error(database);
            }
            rc = MYLITE_ERROR;
        } else if (sqlite_rc != SQLITE_DONE) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);

    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    } else if (
        rc != MYLITE_OK &&
        mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) == MYLITE_OK
    ) {
        set_physical_sqlite_row_error(database);
        rc = MYLITE_ERROR;
    }

    return rc;
}

static int build_foreign_key_validation_sql(
    const struct loaded_foreign_key_info *foreign_key,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);
    if (foreign_key == NULL || foreign_key->part_count == 0U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(&string, "SELECT 1 FROM ");
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
        rc = mylite_dynamic_string_append(&string, " WHERE ");
    }
    if (rc == MYLITE_OK) {
        rc = append_foreign_key_child_not_null_checks(&string, foreign_key);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " AND NOT EXISTS (SELECT 1 FROM ");
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
        rc = mylite_dynamic_string_append(&string, " WHERE ");
    }
    if (rc == MYLITE_OK) {
        rc = append_foreign_key_parent_match_checks(&string, foreign_key);
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

static int append_foreign_key_child_not_null_checks(
    struct mylite_dynamic_string *string,
    const struct loaded_foreign_key_info *foreign_key
) {
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < foreign_key->part_count;
         ++part_index) {
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(string, " AND ");
        }
        if (rc == MYLITE_OK) {
            rc = append_foreign_key_column_reference(
                string,
                "c",
                foreign_key->parts[part_index].child_column.name
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " IS NOT NULL");
        }
    }

    return rc;
}

static int append_foreign_key_child_set_null_assignments_sql(
    struct mylite_dynamic_string *string,
    const struct loaded_foreign_key_info *foreign_key
) {
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < foreign_key->part_count;
         ++part_index) {
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(
                string,
                foreign_key->parts[part_index].child_column.name
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " = NULL");
        }
    }

    return rc;
}

static int append_foreign_key_parent_match_checks(
    struct mylite_dynamic_string *string,
    const struct loaded_foreign_key_info *foreign_key
) {
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < foreign_key->part_count;
         ++part_index) {
        const struct loaded_foreign_key_part *part = &foreign_key->parts[part_index];

        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(string, " AND ");
        }
        if (rc == MYLITE_OK) {
            rc = append_foreign_key_column_reference(string, "p", part->parent_column.name);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " = ");
        }
        if (rc == MYLITE_OK) {
            rc = append_foreign_key_column_reference(string, "c", part->child_column.name);
        }
    }

    return rc;
}

static int append_foreign_key_column_reference(
    struct mylite_dynamic_string *string,
    const char *table_alias,
    const char *column_name
) {
    int rc = mylite_dynamic_string_append_quoted_identifier(string, table_alias);

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '.');
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, column_name);
    }

    return rc;
}

static int unique_key_tuple_exists(
    struct mylite_db *database,
    const char *physical_table_name,
    const struct loaded_index_info *index,
    const struct planned_value *values,
    size_t value_count,
    bool *out_exists
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = MYLITE_OK;

    *out_exists = false;
    rc = build_unique_key_lookup_sql(physical_table_name, index, &sql);
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_unique_key_tuple_parameters(statement, index, values, value_count);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            *out_exists = true;
        } else if (sqlite_rc != SQLITE_DONE) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);

    return rc;
}

static int build_unique_key_lookup_sql(
    const char *physical_table_name,
    const struct loaded_index_info *index,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT 1 FROM ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, physical_table_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " WHERE ");
    }
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < index->part_count; ++part_index) {
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(&string, " AND ");
        }
        if (rc == MYLITE_OK) {
            rc = append_loaded_key_part_sql(&string, &index->parts[part_index], NULL);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(&string, " = ");
        }
        if (rc == MYLITE_OK) {
            rc = append_loaded_key_part_parameter_sql(
                &string,
                &index->parts[part_index],
                part_index + 1U
            );
        }
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

static bool loaded_index_contains_column_id(
    const struct loaded_index_info *index,
    int64_t column_id
) {
    if (index == NULL) {
        return false;
    }
    for (size_t part_index = 0U; part_index < index->part_count; ++part_index) {
        if (index->parts[part_index].index_column.column_id == column_id) {
            return true;
        }
    }

    return false;
}

static int unique_key_single_value_exists(
    struct mylite_db *database,
    const char *physical_table_name,
    const struct loaded_index_info *index,
    const struct planned_value *value,
    bool *out_exists
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = MYLITE_OK;

    *out_exists = false;
    rc = build_unique_key_lookup_sql(physical_table_name, index, &sql);
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_planned_value_parameter(statement, 1, value);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            *out_exists = true;
        } else if (sqlite_rc != SQLITE_DONE) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);

    return rc;
}

static int find_update_unique_key_conflict_tuple(
    struct mylite_db *database,
    const struct planned_update *executable_plan,
    const struct planned_update *plan,
    const struct loaded_index_info *index,
    char *destination,
    size_t destination_size,
    bool *out_found
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = MYLITE_OK;

    *out_found = false;
    destination[0] = '\0';
    rc = build_update_unique_key_conflict_sql(executable_plan, index, &sql);
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        const struct update_unique_key_conflict_bind_request request = {
            .executable_plan = executable_plan,
            .plan = plan,
            .index = index,
        };
        rc = bind_update_unique_key_conflict_parameters(statement, &request);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = format_sqlite_key_tuple(
                statement,
                index->parts,
                index->part_count,
                destination,
                destination_size
            );
            if (rc == MYLITE_OK) {
                *out_found = true;
            }
        } else if (sqlite_rc != SQLITE_DONE) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);
    statement = NULL;
    sql = NULL;
    if (rc == MYLITE_OK && !*out_found) {
        rc = build_update_unique_key_internal_conflict_sql(executable_plan, index, &sql);
        if (rc == MYLITE_OK) {
            rc = prepare_sqlite_statement(database, sql, &statement);
        }
        if (rc == MYLITE_OK) {
            const struct update_unique_key_conflict_bind_request request = {
                .executable_plan = executable_plan,
                .plan = plan,
                .index = index,
            };
            rc = bind_update_unique_key_internal_conflict_parameters(statement, &request);
        }
        if (rc == MYLITE_OK) {
            sqlite_rc = sqlite3_step(statement);
            if (sqlite_rc == SQLITE_ROW) {
                rc = format_sqlite_key_tuple(
                    statement,
                    index->parts,
                    index->part_count,
                    destination,
                    destination_size
                );
                if (rc == MYLITE_OK) {
                    *out_found = true;
                }
            } else if (sqlite_rc != SQLITE_DONE) {
                rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            }
        }
        rc = finalize_sqlite_statement(statement, rc);
        free(sql);
    }

    return rc;
}

static int build_update_unique_key_conflict_sql(
    const struct planned_update *plan,
    const struct loaded_index_info *index,
    char **out_sql
) {
    static const char conflict_alias[] = "_mylite_conflict";
    struct mylite_dynamic_string string;
    size_t next_parameter = 1U;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT ");
    if (rc == MYLITE_OK) {
        rc = append_update_unique_key_conflict_select_list(&string, plan, index, &next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = append_update_unique_key_conflict_candidate_source(&string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = append_update_unique_key_conflict_target_filter(&string, plan, &next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = append_update_unique_key_conflict_exists_clause(
            &string,
            plan,
            index,
            conflict_alias,
            &next_parameter
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

static int build_update_unique_key_internal_conflict_sql(
    const struct planned_update *plan,
    const struct loaded_index_info *index,
    char **out_sql
) {
    static const char projected_alias[] = "_mylite_projected";
    struct mylite_dynamic_string string;
    size_t next_parameter = 1U;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT ");
    if (rc == MYLITE_OK) {
        rc = append_update_unique_key_internal_conflict_alias_list(&string, index->part_count);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " FROM (SELECT ");
    }
    if (rc == MYLITE_OK) {
        rc = append_update_unique_key_internal_conflict_select_list(
            &string,
            plan,
            index,
            &next_parameter
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_update_unique_key_conflict_candidate_source(&string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = append_update_unique_key_conflict_target_filter(&string, plan, &next_parameter);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, ") AS ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, projected_alias);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " WHERE ");
    }
    if (rc == MYLITE_OK) {
        rc = append_update_unique_key_internal_conflict_not_null_filter(&string, index->part_count);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " GROUP BY ");
    }
    if (rc == MYLITE_OK) {
        rc = append_update_unique_key_internal_conflict_alias_list(&string, index->part_count);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " HAVING COUNT(*) > 1 LIMIT 1");
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

static int append_update_unique_key_internal_conflict_alias_list(
    struct mylite_dynamic_string *string,
    size_t part_count
) {
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < part_count; ++part_index) {
        char alias[update_unique_internal_key_alias_capacity];
        int written = snprintf(alias, sizeof(alias), "_mylite_key_%zu", part_index);

        if (written < 0 || (size_t)written >= sizeof(alias)) {
            return MYLITE_NOMEM;
        }
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(string, alias);
        }
    }

    return rc;
}

static int append_update_unique_key_internal_conflict_select_list(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    const struct loaded_index_info *index,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < index->part_count; ++part_index) {
        char alias[update_unique_internal_key_alias_capacity];
        int written = snprintf(alias, sizeof(alias), "_mylite_key_%zu", part_index);

        if (written < 0 || (size_t)written >= sizeof(alias)) {
            return MYLITE_NOMEM;
        }
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_update_unique_key_conflict_expression(
                string,
                plan,
                &index->parts[part_index],
                next_parameter
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " AS ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(string, alias);
        }
    }

    return rc;
}

static int append_update_unique_key_internal_conflict_not_null_filter(
    struct mylite_dynamic_string *string,
    size_t part_count
) {
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < part_count; ++part_index) {
        char alias[update_unique_internal_key_alias_capacity];
        int written = snprintf(alias, sizeof(alias), "_mylite_key_%zu", part_index);

        if (written < 0 || (size_t)written >= sizeof(alias)) {
            return MYLITE_NOMEM;
        }
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(string, " AND ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(string, alias);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " IS NOT NULL");
        }
    }

    return rc;
}

static int append_update_unique_key_conflict_select_list(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    const struct loaded_index_info *index,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < index->part_count; ++part_index) {
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_update_unique_key_conflict_expression(
                string,
                plan,
                &index->parts[part_index],
                next_parameter
            );
        }
    }

    return rc;
}

static int append_update_unique_key_conflict_candidate_source(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan
) {
    int rc = mylite_dynamic_string_append(string, " FROM ");

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, plan->table.physical_name);
    }

    return rc;
}

static int append_update_unique_key_conflict_target_filter(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (plan->is_joined) {
        rc = append_joined_update_rowid_filter_sql(string, plan, next_parameter);
    } else if (plan->limit.has_limit) {
        rc = append_update_rowid_limited_sql(string, plan, next_parameter);
    } else {
        rc = append_select_predicate_sql(string, &plan->predicate, next_parameter);
    }
    if (rc == MYLITE_OK) {
        if (plan->is_joined || plan->limit.has_limit ||
            planned_select_predicate_has_expression(&plan->predicate)) {
            rc = mylite_dynamic_string_append(string, " AND ");
        } else {
            rc = mylite_dynamic_string_append(string, " WHERE ");
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_update_changed_condition_sql(string, plan, next_parameter);
    }

    return rc;
}

static int append_update_unique_key_conflict_exists_clause(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    const struct loaded_index_info *index,
    const char *conflict_alias,
    size_t *next_parameter
) {
    int rc = mylite_dynamic_string_append(string, " AND EXISTS (SELECT 1 FROM ");

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " AS ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, conflict_alias);
    }

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " WHERE ");
    }
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < index->part_count; ++part_index) {
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(string, " AND ");
        }
        if (rc == MYLITE_OK) {
            rc = append_loaded_key_part_sql(string, &index->parts[part_index], conflict_alias);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " = ");
        }
        if (rc == MYLITE_OK) {
            rc = append_update_unique_key_conflict_expression(
                string,
                plan,
                &index->parts[part_index],
                next_parameter
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }

    return rc;
}

static int append_update_unique_key_conflict_expression(
    struct mylite_dynamic_string *string,
    const struct planned_update *plan,
    const struct loaded_index_part *part,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (part->index_column.column_id == plan->assignment_column.column_id) {
        rc = append_loaded_key_part_parameter_sql(string, part, *next_parameter);
        if (rc == MYLITE_OK) {
            ++(*next_parameter);
        }
        return rc;
    }

    rc = append_loaded_key_part_sql(string, part, plan->table.physical_name);

    return rc;
}

static int bind_update_unique_key_conflict_parameters(
    sqlite3_stmt *statement,
    const struct update_unique_key_conflict_bind_request *request
) {
    int parameter_index = 1;
    int rc = bind_update_unique_key_assignment_parameters(statement, request, &parameter_index);

    if (rc == MYLITE_OK) {
        rc = bind_select_predicate_parameters(
            statement,
            &request->plan->predicate,
            &parameter_index
        );
    }
    if (rc == MYLITE_OK && request->plan->limit.has_limit) {
        rc = bind_int64_parameter(statement, parameter_index, request->plan->limit.row_count);
        if (rc == MYLITE_OK) {
            ++parameter_index;
        }
    }
    if (rc == MYLITE_OK && !request->executable_plan->assignment_value.is_null) {
        rc = bind_planned_value_parameter(
            statement,
            parameter_index,
            &request->executable_plan->assignment_value
        );
        if (rc == MYLITE_OK) {
            ++parameter_index;
        }
    }
    if (rc == MYLITE_OK) {
        rc = bind_update_unique_key_assignment_parameters(statement, request, &parameter_index);
    }

    return rc;
}

static int bind_update_unique_key_internal_conflict_parameters(
    sqlite3_stmt *statement,
    const struct update_unique_key_conflict_bind_request *request
) {
    int parameter_index = 1;
    int rc = bind_update_unique_key_assignment_parameters(statement, request, &parameter_index);

    if (rc == MYLITE_OK) {
        rc = bind_select_predicate_parameters(
            statement,
            &request->plan->predicate,
            &parameter_index
        );
    }
    if (rc == MYLITE_OK && request->plan->limit.has_limit) {
        rc = bind_int64_parameter(statement, parameter_index, request->plan->limit.row_count);
        if (rc == MYLITE_OK) {
            ++parameter_index;
        }
    }
    if (rc == MYLITE_OK && !request->executable_plan->assignment_value.is_null) {
        rc = bind_planned_value_parameter(
            statement,
            parameter_index,
            &request->executable_plan->assignment_value
        );
    }

    return rc;
}

static int bind_update_unique_key_assignment_parameters(
    sqlite3_stmt *statement,
    const struct update_unique_key_conflict_bind_request *request,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < request->index->part_count;
         ++part_index) {
        if (request->index->parts[part_index].index_column.column_id !=
            request->plan->assignment_column.column_id) {
            continue;
        }
        rc = bind_planned_value_parameter(
            statement,
            *parameter_index,
            &request->executable_plan->assignment_value
        );
        if (rc == MYLITE_OK) {
            ++(*parameter_index);
        }
    }

    return rc;
}

static int format_sqlite_key_tuple(
    sqlite3_stmt *statement,
    const struct loaded_index_part *parts,
    size_t part_count,
    char *destination,
    size_t destination_size
) {
    size_t used = 0U;
    int rc = MYLITE_OK;

    if (statement == NULL || destination == NULL || destination_size == 0U) {
        return MYLITE_ERROR;
    }
    destination[0] = '\0';
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < part_count; ++part_index) {
        char value_text[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];

        if (part_index >= (size_t)INT_MAX) {
            return MYLITE_ERROR;
        }
        rc = format_sqlite_key_part(
            statement,
            parts == NULL ? NULL : &parts[part_index],
            (int)part_index,
            value_text,
            sizeof(value_text)
        );
        if (rc == MYLITE_OK) {
            rc = append_formatted_key_part(
                destination,
                destination_size,
                &used,
                part_index,
                value_text
            );
        }
    }

    return rc;
}

static int format_sqlite_key_part(
    sqlite3_stmt *statement,
    const struct loaded_index_part *part,
    int column_index,
    char *destination,
    size_t destination_size
) {
    int written = 0;
    int sqlite_type = SQLITE_NULL;

    if (destination == NULL || destination_size == 0U) {
        return MYLITE_ERROR;
    }
    sqlite_type = sqlite3_column_type(statement, column_index);
    switch (sqlite_type) {
    case SQLITE_NULL:
        written = snprintf(destination, destination_size, "NULL");
        break;
    case SQLITE_INTEGER:
        written = snprintf(
            destination,
            destination_size,
            "%" PRId64,
            (int64_t)sqlite3_column_int64(statement, column_index)
        );
        break;
    default: {
        if (loaded_index_part_uses_binary_key_display(part)) {
            const void *bytes = sqlite3_column_blob(statement, column_index);
            int byte_count = sqlite3_column_bytes(statement, column_index);
            size_t display_byte_count = 0U;

            if ((bytes == NULL && byte_count != 0) || byte_count < 0) {
                return MYLITE_ERROR;
            }
            display_byte_count = binary_key_display_byte_count(part, bytes, (size_t)byte_count);
            return format_binary_key_bytes(
                bytes,
                display_byte_count,
                destination,
                destination_size
            );
        }
        const unsigned char *sqlite_text = sqlite3_column_text(statement, column_index);
        size_t text_length = 0U;

        if (sqlite_text == NULL) {
            return MYLITE_ERROR;
        }
        text_length = strlen((const char *)sqlite_text);
        if (text_length >= destination_size) {
            return MYLITE_ERROR;
        }
        memcpy(destination, sqlite_text, text_length);
        destination[text_length] = '\0';
        return MYLITE_OK;
    }
    }
    if (written < 0 || (size_t)written >= destination_size) {
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int append_formatted_key_part(
    char *destination,
    size_t destination_size,
    size_t *used,
    size_t part_index,
    const char *value_text
) {
    size_t display_capacity = duplicate_key_value_display_length;
    size_t value_length = 0U;
    size_t copy_length = 0U;

    if (destination == NULL || destination_size == 0U || used == NULL || value_text == NULL) {
        return MYLITE_ERROR;
    }
    value_length = strlen(value_text);
    if (display_capacity > destination_size - 1U) {
        display_capacity = destination_size - 1U;
    }
    if (*used > display_capacity) {
        return MYLITE_ERROR;
    }
    if (*used == display_capacity) {
        return MYLITE_OK;
    }

    if (part_index != 0U) {
        destination[*used] = '-';
        ++(*used);
        destination[*used] = '\0';
        if (*used == display_capacity) {
            return MYLITE_OK;
        }
    }
    copy_length = value_length;
    if (copy_length > display_capacity - *used) {
        copy_length = display_capacity - *used;
    }
    memcpy(&destination[*used], value_text, copy_length);
    *used += copy_length;
    destination[*used] = '\0';

    return MYLITE_OK;
}

static int bind_unique_key_tuple_parameters(
    sqlite3_stmt *statement,
    const struct loaded_index_info *index,
    const struct planned_value *values,
    size_t value_count
) {
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < index->part_count; ++part_index) {
        size_t column_index = index->parts[part_index].column_index;

        if (column_index >= value_count || part_index >= (size_t)INT_MAX) {
            return MYLITE_ERROR;
        }
        rc = bind_planned_value_parameter(statement, (int)part_index + 1, &values[column_index]);
    }

    return rc;
}

static int format_key_tuple(
    const struct loaded_index_info *index,
    const struct planned_value *values,
    size_t value_count,
    char *destination,
    size_t destination_size
) {
    size_t used = 0U;

    if (index == NULL || values == NULL || destination == NULL || destination_size == 0U) {
        return MYLITE_ERROR;
    }
    destination[0] = '\0';
    for (size_t part_index = 0U; part_index < index->part_count; ++part_index) {
        char part_text[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
        size_t column_index = index->parts[part_index].column_index;

        if (column_index >= value_count) {
            return MYLITE_ERROR;
        }
        if (format_key_part_value(
                &index->parts[part_index],
                &values[column_index],
                part_text,
                sizeof(part_text)
            ) != MYLITE_OK) {
            return MYLITE_ERROR;
        }
        if (append_formatted_key_part(
                destination,
                destination_size,
                &used,
                part_index,
                part_text
            ) != MYLITE_OK) {
            return MYLITE_ERROR;
        }
    }

    return MYLITE_OK;
}

static int format_key_part_value(
    const struct loaded_index_part *part,
    const struct planned_value *value,
    char *destination,
    size_t destination_size
) {
    if (part == NULL || value == NULL || destination == NULL || destination_size == 0U) {
        return MYLITE_ERROR;
    }
    if (value->is_blob && loaded_index_part_uses_binary_key_display(part)) {
        return format_blob_key_part_value(part, value, true, destination, destination_size);
    }
    if (!part->index_column.has_prefix_length || value->is_null || !value->is_text) {
        if (part->index_column.has_prefix_length && value->is_blob) {
            return format_blob_key_part_value(part, value, false, destination, destination_size);
        }
        return format_key_value(value, destination, destination_size);
    }

    return format_text_prefix_key_part_value(part, value, destination, destination_size);
}

static int format_blob_key_part_value(
    const struct loaded_index_part *part,
    const struct planned_value *value,
    bool trim_fixed_binary_padding,
    char *destination,
    size_t destination_size
) {
    size_t copy_length = 0U;

    if (part == NULL || value == NULL) {
        return MYLITE_ERROR;
    }
    copy_length = value->text_length;
    if (part->index_column.has_prefix_length) {
        size_t prefix_length = 0U;

        if (index_part_prefix_length(part, &prefix_length) != MYLITE_OK) {
            return MYLITE_ERROR;
        }
        copy_length = limited_copy_length(value->text_length, prefix_length);
    }
    if (trim_fixed_binary_padding) {
        copy_length = binary_key_display_byte_count(part, value->text, copy_length);
    }

    return format_binary_key_bytes(value->text, copy_length, destination, destination_size);
}

static int format_text_prefix_key_part_value(
    const struct loaded_index_part *part,
    const struct planned_value *value,
    char *destination,
    size_t destination_size
) {
    size_t prefix_length = 0U;
    size_t copy_length = 0U;

    if (index_part_prefix_length(part, &prefix_length) != MYLITE_OK) {
        return MYLITE_ERROR;
    }
    copy_length = limited_copy_length(value->text_length, prefix_length);
    if (value->text == NULL || copy_length >= destination_size) {
        return MYLITE_ERROR;
    }
    memcpy(destination, value->text, copy_length);
    destination[copy_length] = '\0';

    return MYLITE_OK;
}

static int index_part_prefix_length(
    const struct loaded_index_part *part,
    size_t *out_prefix_length
) {
    if (part == NULL || out_prefix_length == NULL || part->index_column.prefix_length < 0 ||
        (uint64_t)part->index_column.prefix_length > SIZE_MAX) {
        return MYLITE_ERROR;
    }
    *out_prefix_length = (size_t)part->index_column.prefix_length;

    return MYLITE_OK;
}

static size_t limited_copy_length(size_t value_length, size_t maximum_length) {
    return value_length < maximum_length ? value_length : maximum_length;
}

static int format_key_value(
    const struct planned_value *value,
    char *destination,
    size_t destination_size
) {
    int written = 0;

    if (value == NULL || destination == NULL || destination_size == 0U) {
        return MYLITE_ERROR;
    }
    if (value->is_null) {
        written = snprintf(destination, destination_size, "NULL");
    } else if (value->is_text) {
        if (value->text == NULL || value->text_length >= destination_size) {
            return MYLITE_ERROR;
        }
        memcpy(destination, value->text, value->text_length);
        destination[value->text_length] = '\0';
        return MYLITE_OK;
    } else if (value->is_blob) {
        return format_binary_key_bytes(
            value->text,
            value->text_length,
            destination,
            destination_size
        );
    } else {
        written = snprintf(destination, destination_size, "%" PRId64, value->integer);
    }
    if (written < 0 || (size_t)written >= destination_size) {
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static bool loaded_index_part_uses_binary_key_display(const struct loaded_index_part *part) {
    if (part == NULL) {
        return false;
    }

    return column_descriptor_is_binary_string_family(&part->column);
}

static size_t binary_key_display_byte_count(
    const struct loaded_index_part *part,
    const void *bytes,
    size_t byte_count
) {
    struct binary_string_type_info storage = {0};
    const struct binary_string_type_info *info = NULL;
    const unsigned char *source = bytes;

    if (part == NULL || bytes == NULL || byte_count == 0U) {
        return byte_count;
    }
    info = binary_string_type_info_for_logical_type(part->column.logical_type, &storage);
    if (info == NULL || !info->fixed_length) {
        return byte_count;
    }
    while (byte_count > 0U && source[byte_count - 1U] == '\0') {
        --byte_count;
    }

    return byte_count;
}

static int format_binary_key_bytes(
    const void *bytes,
    size_t byte_count,
    char *destination,
    size_t destination_size
) {
    enum {
        printable_ascii_min = 0x20,
        printable_ascii_max = 0x7e,
        hex_high_nibble_shift = 4,
        hex_nibble_mask = 0x0f,
    };

    static const char hex_digits[] = "0123456789ABCDEF";
    const unsigned char *source = bytes;
    size_t used = 0U;

    if ((bytes == NULL && byte_count != 0U) || destination == NULL || destination_size == 0U) {
        return MYLITE_ERROR;
    }
    destination[0] = '\0';
    for (size_t index = 0U; index < byte_count; ++index) {
        unsigned char byte = source[index];

        if (byte >= printable_ascii_min && byte <= printable_ascii_max) {
            if (used + 1U >= destination_size) {
                break;
            }
            destination[used] = (char)byte;
            ++used;
        } else {
            if (used + 4U >= destination_size) {
                break;
            }
            destination[used] = '\\';
            destination[used + 1U] = 'x';
            destination[used + 2U] = hex_digits[(byte >> hex_high_nibble_shift) & hex_nibble_mask];
            destination[used + 3U] = hex_digits[byte & hex_nibble_mask];
            used += 4U;
        }
        destination[used] = '\0';
    }

    return MYLITE_OK;
}
