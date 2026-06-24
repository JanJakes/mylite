static int execute_admin_noop_statement(struct mylite_db *database, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_embedded_admin_noop,
        "HY000",
        "MyLite accepted this server-only statement as an embedded no-op"
    );

    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int execute_unsupported_stored_program_statement(struct mylite_db *database) {
    set_unsupported_error(database, "stored program or loadable function statement is not supported");
    return MYLITE_ERROR;
}

static int execute_utility_noop_statement(struct mylite_db *database, mylite_result **out_result) {
    mylite_result *result = NULL;
    int rc = mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_embedded_utility_noop,
        "HY000",
        "MyLite accepted this utility statement as an embedded no-op"
    );

    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int execute_unsupported_utility_statement(struct mylite_db *database) {
    set_unsupported_error(database, "utility statement is not supported");
    return MYLITE_ERROR;
}

static bool set_statement_is_persist_noop(const struct mylite_sql_ast_node *statement) {
    const struct mylite_sql_ast_node *assignment_list = NULL;
    const struct mylite_sql_ast_node *assignment = NULL;
    bool saw_assignment = false;

    if (statement == NULL || statement->kind != MYLITE_SQL_AST_SET_STATEMENT) {
        return false;
    }
    assignment_list = child_at(statement, 0U);
    if (assignment_list == NULL || assignment_list->kind != MYLITE_SQL_AST_SET_ASSIGNMENT_LIST) {
        return false;
    }

    assignment = child_at(assignment_list, 0U);
    while (assignment != NULL) {
        if (assignment->kind != MYLITE_SQL_AST_SET_ASSIGNMENT ||
            !set_assignment_target_is_persist_noop(child_at(assignment, 0U))) {
            return false;
        }
        saw_assignment = true;
        assignment = assignment->next_sibling;
    }
    return saw_assignment;
}

static bool set_assignment_target_is_persist_noop(const struct mylite_sql_ast_node *target) {
    const struct mylite_sql_ast_node *variable = NULL;

    if (target == NULL || target->kind != MYLITE_SQL_AST_SET_SYSTEM_VARIABLE_TARGET ||
        mylite_sql_ast_node_child_count(target) != 1U) {
        return false;
    }
    variable = child_at(target, 0U);
    if (variable == NULL || variable->kind != MYLITE_SQL_AST_SYSTEM_VARIABLE) {
        return false;
    }
    return span_starts_with_ascii_case_insensitive(&variable->span, "@@PERSIST.") ||
           span_starts_with_ascii_case_insensitive(&variable->span, "@@PERSIST_ONLY.");
}

static bool span_starts_with_ascii_case_insensitive(
    const struct mylite_sql_source_span *span,
    const char *prefix
) {
    size_t prefix_length = strlen(prefix);

    if (span == NULL || span->text == NULL || span->length < prefix_length) {
        return false;
    }
    for (size_t index = 0U; index < prefix_length; ++index) {
        unsigned char actual = (unsigned char)span->text[index];
        unsigned char expected = (unsigned char)prefix[index];

        if (actual >= 'a' && actual <= 'z') {
            actual = (unsigned char)(actual - 'a' + 'A');
        }
        if (expected >= 'a' && expected <= 'z') {
            expected = (unsigned char)(expected - 'a' + 'A');
        }
        if (actual != expected) {
            return false;
        }
    }
    return true;
}
