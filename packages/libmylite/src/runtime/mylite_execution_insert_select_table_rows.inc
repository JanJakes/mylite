
static int execute_insert_select_materialize(
    struct mylite_db *database,
    const char *materialize_sql,
    const struct planned_insert_select *plan,
    bool *out_temporary_table_created
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = MYLITE_OK;

    *out_temporary_table_created = false;
    rc = prepare_sqlite_statement(database, materialize_sql, &statement);
    if (rc == MYLITE_OK) {
        rc = bind_insert_select_materialize_parameters(statement, plan);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            *out_temporary_table_created = true;
        } else {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }

    return finalize_sqlite_statement(statement, rc);
}

static int execute_insert_select_insert(
    struct mylite_db *database,
    const char *validation_sql,
    const char *insert_sql,
    const struct planned_insert_select *plan,
    struct insert_execution_counters *counters
) {
    struct insert_select_execution_statements statements = {0};
    int rc = MYLITE_OK;

    *counters = (struct insert_execution_counters){0};
    if (plan->target.has_auto_increment) {
        counters->auto_increment_next_after_rows = plan->target.auto_increment_next;
    }
    rc = prepare_sqlite_statement(database, validation_sql, &statements.select_statement);
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, insert_sql, &statements.insert_statement);
    }
    if (rc == MYLITE_OK) {
        rc = execute_insert_select_insert_rows(database, &statements, plan, counters);
    }
    rc = finalize_sqlite_statement(statements.insert_statement, rc);
    statements.insert_statement = NULL;

    return finalize_sqlite_statement(statements.select_statement, rc);
}

static int execute_insert_select_insert_rows(
    struct mylite_db *database,
    struct insert_select_execution_statements *statements,
    const struct planned_insert_select *plan,
    struct insert_execution_counters *counters
) {
    size_t row_number = 0U;
    int sqlite_rc = SQLITE_OK;
    int rc = MYLITE_OK;

    while ((sqlite_rc = sqlite3_step(statements->select_statement)) == SQLITE_ROW) {
        if (row_number == SIZE_MAX) {
            set_unsupported_error(database, "INSERT ... SELECT selected too many rows");
            return MYLITE_ERROR;
        }
        ++row_number;
        rc = execute_insert_select_insert_row(database, statements, plan, row_number, counters);
        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    if (sqlite_rc != SQLITE_DONE) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    return MYLITE_OK;
}

static int execute_insert_select_insert_row(
    struct mylite_db *database,
    struct insert_select_execution_statements *statements,
    const struct planned_insert_select *plan,
    size_t row_number,
    struct insert_execution_counters *counters
) {
    struct planned_insert row_plan = plan->target;
    struct planned_insert_row row = {0};
    int rc = MYLITE_OK;

    row.values = calloc(plan->target.column_count, sizeof(*row.values));
    if (row.values == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = materialize_insert_select_row_values(
        database,
        statements->select_statement,
        plan,
        row_number,
        row.values
    );
    if (rc == MYLITE_OK) {
        row_plan.rows = &row;
        row_plan.row_count = 1U;
        rc = plan_insert_select_auto_increment_row_value(database, &row_plan, counters);
    }
    if (rc == MYLITE_OK) {
        rc = plan_insert_string_key_values(database, &row_plan);
    }
    if (rc == MYLITE_OK) {
        rc = execute_insert_plan_rows(database, statements->insert_statement, &row_plan, counters);
    }
    for (size_t column_index = 0U; column_index < plan->target.column_count; ++column_index) {
        planned_value_deinit(&row.values[column_index]);
    }
    free(row.values);

    return rc;
}

static int plan_insert_select_auto_increment_row_value(
    struct mylite_db *database,
    struct planned_insert *row_plan,
    struct insert_execution_counters *counters
) {
    int64_t next_value = 0;

    if (row_plan == NULL || counters == NULL || !row_plan->has_auto_increment) {
        return MYLITE_OK;
    }

    next_value = counters->auto_increment_next_after_rows;
    return plan_insert_auto_increment_row_value(
        database,
        row_plan,
        &row_plan->auto_increment_range,
        0U,
        &next_value
    );
}
