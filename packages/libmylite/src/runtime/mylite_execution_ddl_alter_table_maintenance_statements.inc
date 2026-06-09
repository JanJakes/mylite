static int execute_alter_table_order_by_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_order_by plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_order_by(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = validate_sql_require_primary_key_for_table_descriptor(database, &plan.table);
    }
    if (rc == MYLITE_OK) {
        rc = alter_table_order_by_from_plan(database, &plan);
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_order_by_deinit(&plan);
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, plan.affected_rows);
    planned_alter_table_order_by_deinit(&plan);
    return finish_successful_result(database, result, out_result);
}

static int execute_alter_table_force_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_force plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_force(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = validate_sql_require_primary_key_for_table_descriptor(database, &plan.table);
    }
    if (rc == MYLITE_OK) {
        rc = alter_table_force_from_plan(database, &plan);
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_force_deinit(&plan);
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    planned_alter_table_force_deinit(&plan);
    return finish_successful_result(database, result, out_result);
}

static int execute_alter_table_disable_keys_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    return execute_alter_table_key_maintenance_statement(database, statement, out_result);
}

static int execute_alter_table_enable_keys_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    return execute_alter_table_key_maintenance_statement(database, statement, out_result);
}

static int execute_alter_table_key_maintenance_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_key_maintenance plan = {0};
    enum mylite_sql_ast_alter_algorithm algorithm = mylite_sql_ast_node_alter_algorithm(statement);
    enum mylite_sql_ast_alter_lock lock = mylite_sql_ast_node_alter_lock(statement);
    const char *note_table_name = NULL;
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_key_maintenance(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = validate_alter_table_key_maintenance_algorithm_lock_options(database, algorithm, lock);
    }
    if (rc == MYLITE_OK && algorithm == MYLITE_SQL_AST_ALTER_ALGORITHM_COPY) {
        rc = read_show_table_status_row_count(database, &plan.table, &plan.affected_rows);
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        } else if (rc != MYLITE_OK) {
            set_runtime_error(database, "failed to read key maintenance row count");
        }
    }
    if (rc == MYLITE_OK) {
        note_table_name = algorithm == MYLITE_SQL_AST_ALTER_ALGORITHM_COPY ? "#sql-mylite-copy"
                                                                           : plan.target.table_name;
        rc = append_table_storage_engine_option_note(database, note_table_name);
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, plan.affected_rows);
    return finish_successful_result(database, result, out_result);
}

static int validate_alter_table_algorithm_lock_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
) {
    enum mylite_sql_ast_alter_algorithm algorithm = MYLITE_SQL_AST_ALTER_ALGORITHM_UNSPECIFIED;
    enum mylite_sql_ast_alter_lock lock = MYLITE_SQL_AST_ALTER_LOCK_UNSPECIFIED;
    int rc = MYLITE_OK;

    if (statement == NULL) {
        return MYLITE_OK;
    }
    if (!alter_table_statement_accepts_algorithm_lock_options(statement->kind)) {
        return MYLITE_OK;
    }

    algorithm = mylite_sql_ast_node_alter_algorithm(statement);
    lock = mylite_sql_ast_node_alter_lock(statement);
    if (algorithm == MYLITE_SQL_AST_ALTER_ALGORITHM_UNSPECIFIED &&
        lock == MYLITE_SQL_AST_ALTER_LOCK_UNSPECIFIED) {
        return MYLITE_OK;
    }

    if ((statement->kind == MYLITE_SQL_AST_ALTER_TABLE_DISABLE_KEYS_STATEMENT ||
         statement->kind == MYLITE_SQL_AST_ALTER_TABLE_ENABLE_KEYS_STATEMENT) &&
        algorithm == MYLITE_SQL_AST_ALTER_ALGORITHM_INSTANT &&
        (lock == MYLITE_SQL_AST_ALTER_LOCK_NONE || lock == MYLITE_SQL_AST_ALTER_LOCK_SHARED ||
         lock == MYLITE_SQL_AST_ALTER_LOCK_EXCLUSIVE)) {
        rc = reject_builtin_schema_write_target(database, child_at(statement, 0U));
        if (rc != MYLITE_OK) {
            return rc;
        }
        rc = require_selected_schema_for_unqualified_table_name(database, child_at(statement, 0U));
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    rc = validate_alter_table_algorithm_lock_option_values(database, algorithm, lock);
    if (rc != MYLITE_OK) {
        return rc;
    }

    switch (statement->kind) {
    case MYLITE_SQL_AST_ALTER_TABLE_ADD_COLUMN_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_COLUMN_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_RENAME_COLUMN_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_MODIFY_COLUMN_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_CHANGE_COLUMN_STATEMENT:
        return validate_alter_table_column_algorithm_options(database, algorithm);

    case MYLITE_SQL_AST_ALTER_TABLE_ADD_INDEX_STATEMENT:
        return validate_alter_table_add_index_algorithm_lock_options(
            database,
            statement,
            algorithm,
            lock
        );

    case MYLITE_SQL_AST_ALTER_TABLE_DROP_INDEX_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_RENAME_INDEX_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_INDEX_VISIBILITY_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_FOREIGN_KEY_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_COMMENT_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_STORAGE_STATISTICS_STATEMENT:
        return validate_alter_table_online_metadata_algorithm_options(database, algorithm);

    case MYLITE_SQL_AST_ALTER_TABLE_MULTI_ACTION_STATEMENT:
        return MYLITE_OK;

    case MYLITE_SQL_AST_ALTER_TABLE_ADD_FOREIGN_KEY_STATEMENT:
        return validate_alter_table_add_foreign_key_algorithm_lock_options(
            database,
            algorithm,
            lock
        );

    case MYLITE_SQL_AST_ALTER_TABLE_ADD_PRIMARY_KEY_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_PRIMARY_KEY_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_FORCE_STATEMENT:
        return validate_alter_table_rebuild_algorithm_options(database, algorithm);

    default:
        return MYLITE_OK;
    }
}

static int validate_alter_table_algorithm_lock_option_values(
    struct mylite_db *database,
    enum mylite_sql_ast_alter_algorithm algorithm,
    enum mylite_sql_ast_alter_lock lock
) {
    if (algorithm == MYLITE_SQL_AST_ALTER_ALGORITHM_UNKNOWN ||
        lock == MYLITE_SQL_AST_ALTER_LOCK_UNKNOWN) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (algorithm == MYLITE_SQL_AST_ALTER_ALGORITHM_INSTANT &&
        (lock == MYLITE_SQL_AST_ALTER_LOCK_NONE || lock == MYLITE_SQL_AST_ALTER_LOCK_SHARED ||
         lock == MYLITE_SQL_AST_ALTER_LOCK_EXCLUSIVE)) {
        set_alter_table_instant_lock_error(database);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_alter_table_column_algorithm_options(
    struct mylite_db *database,
    enum mylite_sql_ast_alter_algorithm algorithm
) {
    if (algorithm == MYLITE_SQL_AST_ALTER_ALGORITHM_INPLACE) {
        set_unsupported_error(database, "ALTER TABLE action does not support ALGORITHM=INPLACE");
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_alter_table_add_index_algorithm_lock_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    enum mylite_sql_ast_alter_algorithm algorithm,
    enum mylite_sql_ast_alter_lock lock
) {
    if (alter_table_add_index_statement_is_fulltext(statement)) {
        if (algorithm == MYLITE_SQL_AST_ALTER_ALGORITHM_INSTANT) {
            set_alter_table_add_fulltext_instant_error(database);
            return MYLITE_ERROR;
        }
        if (lock == MYLITE_SQL_AST_ALTER_LOCK_NONE) {
            if (algorithm == MYLITE_SQL_AST_ALTER_ALGORITHM_COPY) {
                set_alter_table_copy_lock_none_error(database);
            } else {
                set_alter_table_add_fulltext_lock_none_error(database);
            }
            return MYLITE_ERROR;
        }
        return MYLITE_OK;
    }
    return validate_alter_table_online_metadata_algorithm_options(database, algorithm);
}

static int validate_alter_table_online_metadata_algorithm_options(
    struct mylite_db *database,
    enum mylite_sql_ast_alter_algorithm algorithm
) {
    if (algorithm == MYLITE_SQL_AST_ALTER_ALGORITHM_INSTANT) {
        set_alter_table_instant_algorithm_error(database);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_alter_table_add_foreign_key_algorithm_lock_options(
    struct mylite_db *database,
    enum mylite_sql_ast_alter_algorithm algorithm,
    enum mylite_sql_ast_alter_lock lock
) {
    if (algorithm == MYLITE_SQL_AST_ALTER_ALGORITHM_INSTANT) {
        set_alter_table_add_foreign_key_instant_error(database);
        return MYLITE_ERROR;
    }
    if (algorithm == MYLITE_SQL_AST_ALTER_ALGORITHM_INPLACE) {
        set_alter_table_add_foreign_key_inplace_error(database);
        return MYLITE_ERROR;
    }
    if (lock == MYLITE_SQL_AST_ALTER_LOCK_NONE) {
        set_alter_table_add_foreign_key_lock_none_error(database, algorithm);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_alter_table_rebuild_algorithm_options(
    struct mylite_db *database,
    enum mylite_sql_ast_alter_algorithm algorithm
) {
    if (algorithm == MYLITE_SQL_AST_ALTER_ALGORITHM_INSTANT) {
        set_unsupported_error(database, "ALTER TABLE action does not support ALGORITHM=INSTANT");
        return MYLITE_ERROR;
    }
    if (algorithm == MYLITE_SQL_AST_ALTER_ALGORITHM_INPLACE) {
        set_unsupported_error(database, "ALTER TABLE action does not support ALGORITHM=INPLACE");
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_alter_table_key_maintenance_algorithm_lock_options(
    struct mylite_db *database,
    enum mylite_sql_ast_alter_algorithm algorithm,
    enum mylite_sql_ast_alter_lock lock
) {
    if (lock == MYLITE_SQL_AST_ALTER_LOCK_NONE) {
        if (algorithm == MYLITE_SQL_AST_ALTER_ALGORITHM_COPY) {
            set_alter_table_copy_lock_none_error(database);
        } else {
            set_alter_table_key_maintenance_lock_error(database);
        }
        return MYLITE_ERROR;
    }
    if (lock == MYLITE_SQL_AST_ALTER_LOCK_SHARED &&
        algorithm != MYLITE_SQL_AST_ALTER_ALGORITHM_COPY) {
        set_alter_table_key_maintenance_lock_error(database);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static bool alter_table_statement_accepts_algorithm_lock_options(enum mylite_sql_ast_node_kind kind
) {
    switch (kind) {
    case MYLITE_SQL_AST_ALTER_TABLE_ADD_COLUMN_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_COLUMN_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_RENAME_COLUMN_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_MODIFY_COLUMN_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_CHANGE_COLUMN_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_ADD_PRIMARY_KEY_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_PRIMARY_KEY_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_ADD_INDEX_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_INDEX_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_RENAME_INDEX_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_INDEX_VISIBILITY_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_ADD_FOREIGN_KEY_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_FOREIGN_KEY_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_DROP_CONSTRAINT_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_COMMENT_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_STORAGE_STATISTICS_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_MULTI_ACTION_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_FORCE_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_DISABLE_KEYS_STATEMENT:
    case MYLITE_SQL_AST_ALTER_TABLE_ENABLE_KEYS_STATEMENT:
        return true;
    default:
        return false;
    }
}

static bool alter_table_add_index_statement_is_fulltext(const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *index_definition = child_at(statement, 1U);

    if (index_definition == NULL) {
        return false;
    }
    switch (index_definition->kind) {
    case MYLITE_SQL_AST_FULLTEXT_INDEX_DEFINITION:
        return true;
    default:
        return false;
    }
}
