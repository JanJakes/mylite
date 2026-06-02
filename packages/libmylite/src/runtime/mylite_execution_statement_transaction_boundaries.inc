static void clear_next_transaction_characteristics_before_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
) {
    if (statement_consumes_next_characteristics_before_execution(statement)) {
        clear_next_transaction_characteristics(database);
    }
}

static int prepare_statement_transaction_boundary(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
) {
    bool needs_implicit_commit = statement_requires_implicit_user_transaction_commit(statement);

    if (needs_implicit_commit && database->session.user_transaction_active &&
        statement->kind == MYLITE_SQL_AST_ALTER_TABLE_COMMENT_STATEMENT &&
        alter_table_comment_targets_existing_temporary_table(database, statement)) {
        needs_implicit_commit = false;
    }
    if (!needs_implicit_commit) {
        return MYLITE_OK;
    }

    clear_next_transaction_characteristics_before_statement(database, statement);
    return commit_active_user_transaction_for_ddl(database);
}

static bool statement_consumes_next_characteristics_before_execution(
    const struct mylite_sql_ast_node *statement
) {
    return statement_requires_implicit_user_transaction_commit(statement);
}

static bool alter_table_comment_targets_existing_temporary_table(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
) {
    char parts[table_name_part_capacity][MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    struct mylite_catalog_table_descriptor temporary_table = {0};
    const char *schema_name = NULL;
    const char *table_name = NULL;
    size_t part_count = 0U;
    bool found = false;
    int rc = MYLITE_OK;

    if (database == NULL || statement == NULL ||
        statement->kind != MYLITE_SQL_AST_ALTER_TABLE_COMMENT_STATEMENT) {
        return false;
    }

    rc = collect_identifier_parts(
        child_at(statement, 0U),
        parts,
        table_name_part_capacity,
        &part_count,
        database
    );
    if (rc != MYLITE_OK) {
        return false;
    }
    if (part_count == 1U) {
        if (!database->session.has_selected_schema) {
            return false;
        }
        schema_name = database->session.selected_schema;
        table_name = parts[0];
    } else if (part_count == 2U) {
        schema_name = parts[0];
        table_name = parts[1];
    } else {
        return false;
    }

    rc = mylite_temporary_catalog_try_read_table_by_name(
        &database->session.temporary_catalog,
        schema_name,
        table_name,
        &temporary_table,
        &found
    );
    if (rc != MYLITE_OK) {
        return false;
    }
    if (!found) {
        return false;
    }
    if (temporary_table.kind != MYLITE_CATALOG_TABLE_KIND_TEMPORARY) {
        return false;
    }
    return true;
}

static void clear_select_consumed_next_transaction_characteristics(struct mylite_db *database) {
    if (!database->session.next_transaction_isolation_from_system_variable) {
        database->session.has_next_transaction_isolation = false;
        database->session.next_transaction_isolation_from_system_variable = false;
    }
    if (!database->session.next_transaction_access_mode_from_system_variable) {
        database->session.has_next_transaction_access_mode = false;
        database->session.next_transaction_access_mode_from_system_variable = false;
    }
}

static void clear_next_transaction_characteristics_after_statement(struct mylite_db *database) {
    clear_next_transaction_characteristics(database);
}

static int reject_read_only_persistent_write(
    struct mylite_db *database,
    const struct table_name_resolution *target,
    const struct mylite_catalog_table_descriptor *table
) {
    bool is_temporary = false;
    int rc = MYLITE_OK;

    if (database->session.user_transaction_active) {
        if (!database->session.active_transaction_read_only) {
            return MYLITE_OK;
        }
    } else if (current_transaction_access_mode(database) != MYLITE_TRANSACTION_ACCESS_READ_ONLY) {
        return MYLITE_OK;
    }

    rc = table_resolution_is_temporary(database, target, table, &is_temporary);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (is_temporary) {
        return MYLITE_OK;
    }

    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_read_only_transaction,
        "25006",
        "Cannot execute statement in a READ ONLY transaction."
    );
    return MYLITE_ERROR;
}

static int table_resolution_is_temporary(
    struct mylite_db *database,
    const struct table_name_resolution *target,
    const struct mylite_catalog_table_descriptor *table,
    bool *out_is_temporary
) {
    struct mylite_catalog_table_descriptor temporary_table = {0};
    bool found_temporary = false;
    int rc = mylite_temporary_catalog_try_read_table_by_name(
        &database->session.temporary_catalog,
        target->schema.name,
        target->table_name,
        &temporary_table,
        &found_temporary
    );

    *out_is_temporary = false;
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to read temporary table descriptor");
        return rc;
    }
    if (found_temporary && temporary_table.table_id == table->table_id) {
        *out_is_temporary = true;
    }
    return MYLITE_OK;
}
