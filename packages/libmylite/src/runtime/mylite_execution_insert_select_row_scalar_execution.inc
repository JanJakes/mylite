
static int execute_insert_select_row_scalar_from_plan(
    struct mylite_db *database,
    const struct planned_insert_select *plan,
    mylite_result *result
) {
    sqlite3_stmt *statement = NULL;
    struct planned_insert_row row = {0};
    char *sql = NULL;
    bool has_row = false;
    int rc = prepare_insert_select_row_scalar_source(database, plan, &statement, &sql, &has_row);

    if (rc == MYLITE_OK && has_row) {
        rc = materialize_insert_select_row_scalar_row(database, statement, plan, &row);
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);
    if (rc == MYLITE_OK) {
        rc = finish_insert_select_row_scalar_result(database, plan, has_row, &row, result);
    }

    planned_insert_row_deinit(&row, plan->target.column_count);
    return normalize_insert_select_row_scalar_result(database, rc);
}

static int prepare_insert_select_row_scalar_source(
    struct mylite_db *database,
    const struct planned_insert_select *plan,
    sqlite3_stmt **out_statement,
    char **out_sql,
    bool *out_has_row
) {
    int sqlite_rc = SQLITE_OK;
    int rc = MYLITE_OK;

    *out_statement = NULL;
    *out_sql = NULL;
    *out_has_row = false;

    rc = build_row_scalar_select_sql(&plan->row_source, out_sql);
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, *out_sql, out_statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_row_scalar_select_parameters(*out_statement, &plan->row_source);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    sqlite_rc = sqlite3_step(*out_statement);
    if (sqlite_rc == SQLITE_ROW) {
        *out_has_row = true;
        return MYLITE_OK;
    }
    if (sqlite_rc == SQLITE_DONE) {
        return MYLITE_OK;
    }

    return mylite_sqlite_status_to_mylite(sqlite_rc);
}

static int materialize_insert_select_row_scalar_row(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_insert_select *plan,
    struct planned_insert_row *row
) {
    int sqlite_rc = SQLITE_OK;
    int rc = MYLITE_OK;

    if (plan->target.column_count > SIZE_MAX / sizeof(*row->values)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    row->values = calloc(plan->target.column_count, sizeof(*row->values));
    if (row->values == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = check_insert_omitted_columns(
        database,
        &plan->target,
        plan->target_indexes,
        plan->target_count
    );
    if (rc == MYLITE_OK && insert_select_allows_adjustment(database, plan)) {
        rc = append_insert_omitted_column_warnings(
            database,
            &plan->target,
            plan->target_indexes,
            plan->target_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_insert_select_row_scalar_row(database, statement, plan, 1U);
    }
    if (rc == MYLITE_OK) {
        rc = materialize_insert_select_row_values(database, statement, plan, 1U, row->values);
    }

    if (rc != MYLITE_OK) {
        return rc;
    }
    sqlite_rc = sqlite3_step(statement);
    if (sqlite_rc == SQLITE_ROW) {
        set_unsupported_error(
            database,
            "INSERT ... SELECT row-scalar source returned too many rows"
        );
        return MYLITE_ERROR;
    }
    if (sqlite_rc != SQLITE_DONE) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    return MYLITE_OK;
}

static int finish_insert_select_row_scalar_result(
    struct mylite_db *database,
    const struct planned_insert_select *plan,
    bool has_row,
    struct planned_insert_row *row,
    mylite_result *result
) {
    if (has_row) {
        return execute_insert_select_row_scalar_insert(database, plan, row, result);
    }

    mylite_result_set_affected_rows(result, 0);
    return MYLITE_OK;
}

static void planned_insert_row_deinit(struct planned_insert_row *row, size_t column_count) {
    if (row == NULL || row->values == NULL) {
        return;
    }

    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        planned_value_deinit(&row->values[column_index]);
    }
    free(row->values);
    *row = (struct planned_insert_row){0};
}

static int normalize_insert_select_row_scalar_result(struct mylite_db *database, int rc) {
    if (rc == MYLITE_OK) {
        return MYLITE_OK;
    }
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

static bool insert_select_allows_adjustment(
    const struct mylite_db *database,
    const struct planned_insert_select *plan
) {
    if (plan == NULL) {
        return false;
    }
    return dml_allows_missing_default_adjustment(database, plan->target.ignore_errors);
}

static int validate_insert_select_row_scalar_row(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_insert_select *plan,
    size_t row_number
) {
    bool adjust_value = insert_select_allows_adjustment(database, plan);

    if (plan->target_count > (size_t)INT_MAX) {
        return MYLITE_ERROR;
    }
    for (size_t target_position = 0U; target_position < plan->target_count; ++target_position) {
        size_t column_index = plan->target_indexes[target_position];
        const struct mylite_catalog_column_descriptor *target_column =
            &plan->target.columns[column_index];
        int rc = MYLITE_OK;

        if (column_descriptor_is_auto_increment(target_column) &&
            sqlite3_column_type(statement, (int)target_position) == SQLITE_NULL) {
            continue;
        }
        rc = validate_insert_select_value(
            database,
            statement,
            (int)target_position,
            NULL,
            target_column,
            row_number,
            adjust_value
        );

        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return MYLITE_OK;
}

static int execute_insert_select_row_scalar_insert(
    struct mylite_db *database,
    const struct planned_insert_select *plan,
    struct planned_insert_row *row,
    mylite_result *result
) {
    struct planned_insert row_plan = plan->target;
    int rc = MYLITE_OK;

    row_plan.rows = row;
    row_plan.row_count = 1U;
    rc = plan_insert_auto_increment_values(database, &row_plan);
    if (rc == MYLITE_OK) {
        rc = plan_insert_string_key_values(database, &row_plan);
    }
    if (rc == MYLITE_OK) {
        rc = execute_insert_from_plan(database, &row_plan, NULL, result);
    }

    return rc;
}
