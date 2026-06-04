static int execute_alter_table_add_primary_key_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_add_primary_key plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_add_primary_key(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = alter_table_add_primary_key_from_plan(database, &plan);
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_add_primary_key_deinit(&plan);
        mylite_result_free(result);
        return rc;
    }

    planned_alter_table_add_primary_key_deinit(&plan);
    return finish_successful_result(database, result, out_result);
}

static int execute_alter_table_add_index_statement(
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

    rc = plan_alter_table_add_index(database, statement, &plan);
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

static int execute_alter_table_add_foreign_key_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_add_foreign_key plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_add_foreign_key(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = validate_sql_require_primary_key_for_table_descriptor(database, &plan.table);
    }
    if (rc == MYLITE_OK) {
        rc = alter_table_add_foreign_key_from_plan(database, &plan);
    }
    planned_alter_table_add_foreign_key_deinit(&plan);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_alter_table_drop_foreign_key_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_drop_foreign_key plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_drop_foreign_key(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = validate_sql_require_primary_key_for_table_descriptor(database, &plan.table);
    }
    if (rc == MYLITE_OK) {
        rc = alter_table_drop_foreign_key_from_plan(database, &plan);
    }
    planned_alter_table_drop_foreign_key_deinit(&plan);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int execute_alter_table_drop_constraint_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_drop_constraint plan = {0};
    mylite_result *result = NULL;
    int64_t affected_rows = 0;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_drop_constraint(database, statement, &plan);
    if (rc == MYLITE_OK) {
        switch (plan.kind) {
        case PLANNED_ALTER_TABLE_DROP_CONSTRAINT_PRIMARY_KEY:
            if (sql_require_primary_key_session_enabled(database)) {
                set_sql_require_primary_key_error(database);
                rc = MYLITE_ERROR;
            } else {
                rc = alter_table_drop_primary_key_from_plan(database, &plan.primary_key);
                affected_rows = plan.primary_key.affected_rows;
            }
            break;
        case PLANNED_ALTER_TABLE_DROP_CONSTRAINT_UNIQUE_INDEX:
            rc = validate_sql_require_primary_key_for_table_descriptor(
                database,
                &plan.unique_index.table
            );
            if (rc == MYLITE_OK) {
                rc = drop_index_from_plan(database, &plan.unique_index);
            }
            affected_rows = plan.unique_index.affected_rows;
            break;
        case PLANNED_ALTER_TABLE_DROP_CONSTRAINT_FOREIGN_KEY:
            rc = validate_sql_require_primary_key_for_table_descriptor(
                database,
                &plan.foreign_key.table
            );
            if (rc == MYLITE_OK) {
                rc = alter_table_drop_foreign_key_from_plan(database, &plan.foreign_key);
            }
            break;
        case PLANNED_ALTER_TABLE_DROP_CONSTRAINT_CHECK:
            rc = validate_sql_require_primary_key_for_table_descriptor(
                database,
                &plan.check_constraint.table
            );
            if (rc == MYLITE_OK) {
                rc = alter_table_check_constraint_from_plan(database, &plan.check_constraint);
            }
            affected_rows = plan.check_constraint.affected_rows;
            break;
        case PLANNED_ALTER_TABLE_DROP_CONSTRAINT_NONE:
            set_runtime_error(database, "invalid DROP CONSTRAINT plan");
            rc = MYLITE_ERROR;
            break;
        }
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_drop_constraint_deinit(&plan);
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, affected_rows);
    planned_alter_table_drop_constraint_deinit(&plan);
    return finish_successful_result(database, result, out_result);
}

static int execute_alter_table_drop_index_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_drop_primary_key primary_plan = {0};
    struct planned_drop_index plan = {0};
    char index_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = copy_identifier_text(child_at(statement, 1U), index_name, sizeof(index_name), database);
    if (rc == MYLITE_OK && text_equals_ascii_case_insensitive(index_name, "PRIMARY")) {
        rc = plan_alter_table_drop_primary_key_for_table_node(
            database,
            child_at(statement, 0U),
            true,
            &primary_plan
        );
        if (rc == MYLITE_OK) {
            rc = alter_table_drop_primary_key_from_plan(database, &primary_plan);
        }
        if (rc != MYLITE_OK) {
            planned_alter_table_drop_primary_key_deinit(&primary_plan);
            mylite_result_free(result);
            return rc;
        }

        mylite_result_set_affected_rows(result, primary_plan.affected_rows);
        planned_alter_table_drop_primary_key_deinit(&primary_plan);
        return finish_successful_result(database, result, out_result);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    rc = plan_alter_table_drop_index(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = validate_sql_require_primary_key_for_table_descriptor(database, &plan.table);
    }
    if (rc == MYLITE_OK) {
        rc = drop_index_from_plan(database, &plan);
    }
    if (rc != MYLITE_OK) {
        planned_drop_index_deinit(&plan);
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, plan.affected_rows);
    planned_drop_index_deinit(&plan);
    return finish_successful_result(database, result, out_result);
}

static int execute_alter_table_rename_index_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_rename_index plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_rename_index(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = validate_sql_require_primary_key_for_table_descriptor(database, &plan.table);
    }
    if (rc == MYLITE_OK) {
        rc = rename_index_from_plan(database, &plan);
    }
    if (rc != MYLITE_OK) {
        planned_rename_index_deinit(&plan);
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    planned_rename_index_deinit(&plan);
    return finish_successful_result(database, result, out_result);
}

static int execute_alter_table_add_check_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_check_constraint plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_add_check(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = validate_sql_require_primary_key_for_table_descriptor(database, &plan.table);
    }
    if (rc == MYLITE_OK) {
        rc = alter_table_check_constraint_from_plan(database, &plan);
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_check_constraint_deinit(&plan);
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, plan.affected_rows);
    planned_alter_table_check_constraint_deinit(&plan);
    return finish_successful_result(database, result, out_result);
}

static int execute_alter_table_drop_check_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_check_constraint plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_drop_check(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = validate_sql_require_primary_key_for_table_descriptor(database, &plan.table);
    }
    if (rc == MYLITE_OK) {
        rc = alter_table_check_constraint_from_plan(database, &plan);
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_check_constraint_deinit(&plan);
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, plan.affected_rows);
    planned_alter_table_check_constraint_deinit(&plan);
    return finish_successful_result(database, result, out_result);
}

static int execute_alter_table_alter_check_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_check_constraint plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_alter_check(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = validate_sql_require_primary_key_for_table_descriptor(database, &plan.table);
    }
    if (rc == MYLITE_OK) {
        rc = alter_table_check_constraint_from_plan(database, &plan);
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_check_constraint_deinit(&plan);
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, plan.affected_rows);
    planned_alter_table_check_constraint_deinit(&plan);
    return finish_successful_result(database, result, out_result);
}

static int execute_drop_index_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_drop_primary_key primary_plan = {0};
    struct planned_drop_index plan = {0};
    char index_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = copy_identifier_text(child_at(statement, 0U), index_name, sizeof(index_name), database);
    if (rc == MYLITE_OK && text_equals_ascii_case_insensitive(index_name, "PRIMARY")) {
        rc = plan_alter_table_drop_primary_key_for_table_node(
            database,
            child_at(statement, 1U),
            true,
            &primary_plan
        );
        if (rc == MYLITE_OK) {
            rc = alter_table_drop_primary_key_from_plan(database, &primary_plan);
        }
        if (rc != MYLITE_OK) {
            planned_alter_table_drop_primary_key_deinit(&primary_plan);
            mylite_result_free(result);
            return rc;
        }

        mylite_result_set_affected_rows(result, primary_plan.affected_rows);
        planned_alter_table_drop_primary_key_deinit(&primary_plan);
        return finish_successful_result(database, result, out_result);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    rc = plan_drop_index(
        database,
        (struct drop_index_nodes){
            .table_name_node = child_at(statement, 1U),
            .index_name_node = child_at(statement, 0U),
        },
        "DROP INDEX",
        &plan
    );
    if (rc == MYLITE_OK) {
        rc = validate_sql_require_primary_key_for_table_descriptor(database, &plan.table);
    }
    if (rc == MYLITE_OK) {
        rc = drop_index_from_plan(database, &plan);
    }
    if (rc != MYLITE_OK) {
        planned_drop_index_deinit(&plan);
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, plan.affected_rows);
    planned_drop_index_deinit(&plan);
    return finish_successful_result(database, result, out_result);
}

static int execute_alter_table_drop_primary_key_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_drop_primary_key plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_drop_primary_key(database, statement, &plan);
    if (rc == MYLITE_OK && sql_require_primary_key_session_enabled(database)) {
        set_sql_require_primary_key_error(database);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = alter_table_drop_primary_key_from_plan(database, &plan);
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_drop_primary_key_deinit(&plan);
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, plan.affected_rows);
    planned_alter_table_drop_primary_key_deinit(&plan);
    return finish_successful_result(database, result, out_result);
}

static int execute_alter_table_auto_increment_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    struct planned_alter_table_auto_increment plan = {0};
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = plan_alter_table_auto_increment(database, statement, &plan);
    if (rc == MYLITE_OK) {
        rc = validate_sql_require_primary_key_for_table_descriptor(database, &plan.table);
    }
    if (rc == MYLITE_OK) {
        rc = alter_table_auto_increment_from_plan(database, &plan);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}
