static int execute_create_index_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_add_index plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_create_index(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = validate_sql_require_primary_key_for_table_descriptor(database, &plan.table);
    }
    if (rc == MYLITE_OK) {
        rc = add_secondary_index_from_plan(database, &plan);
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_add_index_deinit(&plan);
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, plan.affected_rows);
    planned_alter_table_add_index_deinit(&plan);
    return finish_successful_result(database, result, out_result);
}

static int execute_create_schema_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    mylite_result *result = NULL;
    bool finished = false;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = validate_schema_default_charset_collation_options(
        database,
        child_with_kind(statement, MYLITE_SQL_AST_TABLE_OPTION_LIST)
    );
    if (rc == MYLITE_OK) {
        rc = maybe_finish_create_schema_if_not_exists_noop(database, statement, result, &finished);
    }
    if (rc == MYLITE_OK && finished) {
        return finish_successful_result(database, result, out_result);
    }
    if (rc == MYLITE_OK) {
        rc = create_schema_from_statement(database, statement, result);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int maybe_finish_create_schema_if_not_exists_noop(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result *result,
    bool *out_finished
) {
    char schema_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    struct mylite_catalog_schema_descriptor schema = {0};
    bool found = false;
    int rc = MYLITE_OK;

    *out_finished = false;
    if (!create_schema_has_if_not_exists(statement)) {
        return MYLITE_OK;
    }

    rc = copy_identifier_text(child_at(statement, 0U), schema_name, sizeof(schema_name), database);
    if (rc == MYLITE_OK) {
        rc = reject_builtin_schema_schema_write_name(database, schema_name);
    }
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(schema_name)) {
        set_reserved_name_error(database, "database", schema_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_try_read_schema_by_name(database, schema_name, &schema, &found);
        if (rc != MYLITE_OK) {
            set_internal_error_if_clear(database, rc, "failed to read schema descriptor");
        }
    }
    if (rc != MYLITE_OK || !found) {
        return rc;
    }

    rc = append_database_exists_note(database, schema_name);
    if (rc == MYLITE_OK) {
        mylite_result_set_affected_rows(result, 1);
        *out_finished = true;
    }
    return rc;
}

static bool create_schema_has_if_not_exists(const struct mylite_sql_ast_node *statement) {
    return child_with_kind(statement, MYLITE_SQL_AST_CREATE_SCHEMA_IF_NOT_EXISTS_CLAUSE) != NULL;
}
