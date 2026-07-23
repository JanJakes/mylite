
static int execute_insert_select_from_plan(
    struct mylite_db *database,
    const struct planned_insert_select *plan,
    mylite_result *result
) {
    if (plan->source_kind == PLANNED_INSERT_SELECT_SOURCE_ROW_SCALAR) {
        return execute_insert_select_row_scalar_from_plan(database, plan, result);
    }

    return execute_insert_select_table_from_plan(database, plan, result);
}

static int execute_insert_select_table_from_plan(
    struct mylite_db *database,
    const struct planned_insert_select *plan,
    mylite_result *result
) {
    struct insert_select_table_execution execution = {0};
    int rc = prepare_insert_select_table_execution(database, plan, &execution);

    if (rc == MYLITE_OK) {
        rc = execute_insert_select_table(database, plan, &execution);
    }
    if (rc == MYLITE_OK) {
        rc = finish_insert_select_table_result(database, &execution, result);
    }
    cleanup_insert_select_table_execution(database, &execution, rc);

    return normalize_insert_select_table_result(database, rc);
}

static int prepare_insert_select_table_execution(
    struct mylite_db *database,
    const struct planned_insert_select *plan,
    struct insert_select_table_execution *execution
) {
    int rc = build_insert_select_temp_table_name(
        database,
        execution->temporary_table_name,
        sizeof(execution->temporary_table_name)
    );

    if (rc == MYLITE_OK) {
        rc = build_insert_select_materialize_sql(
            plan,
            execution->temporary_table_name,
            &execution->materialize_sql
        );
    }
    if (rc == MYLITE_OK) {
        rc = build_insert_select_validation_sql(
            plan,
            execution->temporary_table_name,
            &execution->validation_sql
        );
    }
    if (rc == MYLITE_OK) {
        rc = build_insert_sql(&plan->target, &execution->insert_sql);
    }
    if (rc == MYLITE_OK) {
        rc = build_drop_temp_table_sql(execution->temporary_table_name, &execution->drop_sql);
    }

    return rc;
}

static int execute_insert_select_table(
    struct mylite_db *database,
    const struct planned_insert_select *plan,
    struct insert_select_table_execution *execution
) {
    struct planned_insert_select locked_plan = *plan;
    int rc = MYLITE_OK;

    if (plan->target.has_auto_increment) {
        rc = mylite_execution_ensure_persistent_auto_increment_high_water_slot(
            database,
            &plan->target.table
        );
    }
    if (rc == MYLITE_OK) {
        rc = begin_statement_transaction(database, &execution->transaction);
    }
    if (rc == MYLITE_OK && locked_plan.target.has_auto_increment) {
        rc = mylite_execution_read_current_auto_increment_next_after_write_lock(
            database,
            &locked_plan.target.table,
            locked_plan.target.auto_increment_next,
            true,
            &locked_plan.target.auto_increment_next
        );
        locked_plan.target.auto_increment_next_after_statement =
            locked_plan.target.auto_increment_next;
    }
    if (rc == MYLITE_OK) {
        rc = execute_sqlite_control_sql(database, execution->drop_sql);
    }
    if (rc == MYLITE_OK) {
        rc = execute_insert_select_materialize(
            database,
            execution->materialize_sql,
            &locked_plan,
            &execution->temporary_table_created
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_insert_select_table_rows(database, &locked_plan, execution);
    }
    if (rc == MYLITE_OK) {
        rc = execute_insert_select_insert(
            database,
            execution->validation_sql,
            execution->insert_sql,
            &locked_plan,
            &execution->counters
        );
    }
    if (rc == MYLITE_OK) {
        rc = finish_insert_select_table_side_effects(database, &locked_plan, execution);
    }
    if (rc == MYLITE_OK) {
        rc = execute_sqlite_control_sql(database, execution->drop_sql);
    }
    if (rc == MYLITE_OK) {
        execution->temporary_table_created = false;
        rc = commit_statement_transaction(database, &execution->transaction);
    }
    if (rc != MYLITE_OK) {
        rc = rollback_statement_transaction(database, &execution->transaction, rc);
    }
    if (rc == MYLITE_OK && locked_plan.target.has_auto_increment &&
        execution->counters.auto_increment_next_after_rows !=
            locked_plan.target.auto_increment_next) {
        rc = mylite_execution_record_persistent_auto_increment_high_water(
            database,
            &locked_plan.target.table,
            execution->counters.auto_increment_next_after_rows
        );
    }

    return rc;
}

static int validate_insert_select_table_rows(
    struct mylite_db *database,
    const struct planned_insert_select *plan,
    const struct insert_select_table_execution *execution
) {
    sqlite3_stmt *validation_statement = NULL;
    int rc = prepare_sqlite_statement(database, execution->validation_sql, &validation_statement);

    if (rc == MYLITE_OK) {
        rc = validate_insert_select_rows(database, validation_statement, plan);
    }

    return finalize_sqlite_statement(validation_statement, rc);
}

static int finish_insert_select_table_side_effects(
    struct mylite_db *database,
    const struct planned_insert_select *plan,
    struct insert_select_table_execution *execution
) {
    int rc = MYLITE_OK;

    if (plan->target.has_auto_increment &&
        execution->counters.auto_increment_next_after_rows != plan->target.auto_increment_next) {
        rc = update_table_auto_increment_next(
            database,
            &plan->target.table,
            execution->counters.auto_increment_next_after_rows
        );
    }
    if (rc == MYLITE_OK) {
        rc = touch_persistent_table_updated_time(
            database,
            &plan->target.table,
            execution->counters.affected_rows,
            false
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_child_foreign_keys_after_write(database, plan->target.table.table_id);
    }

    return rc;
}

static void cleanup_insert_select_table_execution(
    struct mylite_db *database,
    struct insert_select_table_execution *execution,
    int rc
) {
    if (rc != MYLITE_OK && execution->temporary_table_created) {
        (void)execute_sqlite_control_sql(database, execution->drop_sql);
    }
    free(execution->materialize_sql);
    free(execution->validation_sql);
    free(execution->insert_sql);
    free(execution->drop_sql);
    *execution = (struct insert_select_table_execution){0};
}

static int normalize_insert_select_table_result(struct mylite_db *database, int rc) {
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

static int finish_insert_select_table_result(
    struct mylite_db *database,
    const struct insert_select_table_execution *execution,
    mylite_result *result
) {
    mylite_result_set_affected_rows(result, execution->counters.affected_rows);
    if (execution->counters.inserted_generated_auto_increment) {
        mylite_result_set_insert_id(
            result,
            (uint64_t)execution->counters.first_inserted_generated_auto_increment
        );
        database->session.last_insert_id =
            (uint64_t)execution->counters.first_inserted_generated_auto_increment;
        database->session.insert_id = 0U;
    } else if (execution->counters.inserted_explicit_auto_increment) {
        mylite_result_set_insert_id(
            result,
            (uint64_t)execution->counters.last_inserted_explicit_auto_increment
        );
    }

    return MYLITE_OK;
}
