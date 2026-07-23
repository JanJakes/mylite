static int execute_set_transaction_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = apply_set_transaction_statement(database, statement);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    mylite_result_set_affected_rows(result, 0);
    return finish_successful_result(database, result, out_result);
}

static int apply_set_transaction_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *first_child = child_at(statement, 0U);
    const struct mylite_sql_ast_node *scope = NULL;
    const struct mylite_sql_ast_node *characteristic_list = first_child;
    struct transaction_characteristics characteristics = {
        .has_isolation = false,
        .has_access_mode = false,
        .has_consistent_snapshot = false,
        .isolation = MYLITE_TRANSACTION_ISOLATION_REPEATABLE_READ,
        .access_mode = MYLITE_TRANSACTION_ACCESS_READ_WRITE,
    };
    char scope_text[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = MYLITE_OK;

    if (first_child != NULL && first_child->kind == MYLITE_SQL_AST_IDENTIFIER) {
        scope = first_child;
        characteristic_list = child_at(statement, 1U);
    }
    rc = collect_transaction_characteristics(database, characteristic_list, &characteristics);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (scope == NULL) {
        return apply_next_transaction_characteristics(database, &characteristics, false);
    }

    rc = copy_identifier_text(scope, scope_text, sizeof(scope_text), database);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (mylite_execution_text_equals_ascii_case_insensitive(scope_text, "session")) {
        return apply_session_transaction_characteristics(database, &characteristics);
    }
    if (mylite_execution_text_equals_ascii_case_insensitive(scope_text, "global")) {
        set_unsupported_error(database, "SET GLOBAL TRANSACTION is not supported");
        return MYLITE_ERROR;
    }

    set_parse_error(database, NULL);
    return MYLITE_ERROR;
}

static int collect_transaction_characteristics(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *list,
    struct transaction_characteristics *out_characteristics
) {
    const struct mylite_sql_ast_node *characteristic = NULL;

    *out_characteristics = (struct transaction_characteristics){
        .has_isolation = false,
        .has_access_mode = false,
        .has_consistent_snapshot = false,
        .isolation = MYLITE_TRANSACTION_ISOLATION_REPEATABLE_READ,
        .access_mode = MYLITE_TRANSACTION_ACCESS_READ_WRITE,
    };
    if (list == NULL || list->kind != MYLITE_SQL_AST_TRANSACTION_CHARACTERISTIC_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    characteristic = child_at(list, 0U);
    while (characteristic != NULL) {
        switch (characteristic->kind) {
        case MYLITE_SQL_AST_TRANSACTION_ISOLATION_REPEATABLE_READ:
            out_characteristics->has_isolation = true;
            out_characteristics->isolation = MYLITE_TRANSACTION_ISOLATION_REPEATABLE_READ;
            break;
        case MYLITE_SQL_AST_TRANSACTION_ISOLATION_READ_COMMITTED:
            out_characteristics->has_isolation = true;
            out_characteristics->isolation = MYLITE_TRANSACTION_ISOLATION_READ_COMMITTED;
            break;
        case MYLITE_SQL_AST_TRANSACTION_ISOLATION_READ_UNCOMMITTED:
            out_characteristics->has_isolation = true;
            out_characteristics->isolation = MYLITE_TRANSACTION_ISOLATION_READ_UNCOMMITTED;
            break;
        case MYLITE_SQL_AST_TRANSACTION_ISOLATION_SERIALIZABLE:
            out_characteristics->has_isolation = true;
            out_characteristics->isolation = MYLITE_TRANSACTION_ISOLATION_SERIALIZABLE;
            break;
        case MYLITE_SQL_AST_TRANSACTION_ACCESS_READ_WRITE:
            if (out_characteristics->has_access_mode &&
                out_characteristics->access_mode != MYLITE_TRANSACTION_ACCESS_READ_WRITE) {
                set_parse_error(database, NULL);
                return MYLITE_ERROR;
            }
            out_characteristics->has_access_mode = true;
            out_characteristics->access_mode = MYLITE_TRANSACTION_ACCESS_READ_WRITE;
            break;
        case MYLITE_SQL_AST_TRANSACTION_ACCESS_READ_ONLY:
            if (out_characteristics->has_access_mode &&
                out_characteristics->access_mode != MYLITE_TRANSACTION_ACCESS_READ_ONLY) {
                set_parse_error(database, NULL);
                return MYLITE_ERROR;
            }
            out_characteristics->has_access_mode = true;
            out_characteristics->access_mode = MYLITE_TRANSACTION_ACCESS_READ_ONLY;
            break;
        case MYLITE_SQL_AST_TRANSACTION_CONSISTENT_SNAPSHOT:
            out_characteristics->has_consistent_snapshot = true;
            break;
        default:
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        characteristic = characteristic->next_sibling;
    }

    return MYLITE_OK;
}

static int apply_session_transaction_characteristics(
    struct mylite_db *database,
    const struct transaction_characteristics *characteristics
) {
    if (characteristics->has_isolation) {
        database->session.session_transaction_isolation = characteristics->isolation;
        if (!database->session.user_transaction_active) {
            database->session.has_next_transaction_isolation = false;
            database->session.next_transaction_isolation_from_system_variable = false;
        }
    }
    if (characteristics->has_access_mode) {
        database->session.session_transaction_access_mode = characteristics->access_mode;
        if (!database->session.user_transaction_active) {
            database->session.has_next_transaction_access_mode = false;
            database->session.next_transaction_access_mode_from_system_variable = false;
        }
    }
    return MYLITE_OK;
}

static int apply_next_transaction_characteristics(
    struct mylite_db *database,
    const struct transaction_characteristics *characteristics,
    bool from_system_variable
) {
    if (database->session.user_transaction_active) {
        mylite_diagnostics_set_error(
            mylite_connection_diagnostics(database),
            mysql_error_transaction_characteristics_changed,
            "25001",
            "Transaction characteristics can't be changed while a transaction is in progress"
        );
        return MYLITE_ERROR;
    }
    if (characteristics->has_isolation) {
        database->session.has_next_transaction_isolation = true;
        database->session.next_transaction_isolation = characteristics->isolation;
        database->session.next_transaction_isolation_from_system_variable = from_system_variable;
    }
    if (characteristics->has_access_mode) {
        database->session.has_next_transaction_access_mode = true;
        database->session.next_transaction_access_mode = characteristics->access_mode;
        database->session.next_transaction_access_mode_from_system_variable = from_system_variable;
    }
    return MYLITE_OK;
}

static int apply_transaction_system_variable_characteristics(
    struct mylite_db *database,
    const struct resolved_set_system_variable_target *target,
    const struct transaction_characteristics *characteristics
) {
    if (target == NULL || characteristics == NULL) {
        set_runtime_error(database, "invalid transaction system variable assignment");
        return MYLITE_ERROR;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_GLOBAL) {
        return MYLITE_OK;
    }
    if (target->scope == SET_SYSTEM_VARIABLE_SCOPE_NONE && target->is_system_variable_target) {
        return apply_next_transaction_characteristics(database, characteristics, true);
    }
    return apply_session_transaction_characteristics(database, characteristics);
}

static int parse_set_transaction_isolation_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    enum mylite_transaction_isolation *out_isolation
) {
    char *decoded = NULL;
    char identifier[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    const char *value = NULL;
    int rc = MYLITE_OK;

    if (out_isolation == NULL) {
        set_runtime_error(database, "invalid transaction_isolation output");
        return MYLITE_ERROR;
    }
    *out_isolation = MYLITE_TRANSACTION_ISOLATION_REPEATABLE_READ;
    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        return MYLITE_OK;
    }
    if (value_node->kind == MYLITE_SQL_AST_IDENTIFIER) {
        rc = copy_set_transaction_identifier_value(
            database,
            value_node,
            identifier,
            sizeof(identifier)
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        value = identifier;
    } else if (value_node->kind == MYLITE_SQL_AST_LITERAL &&
               mylite_sql_ast_node_literal_kind(value_node) == MYLITE_SQL_AST_LITERAL_STRING) {
        rc = decode_set_transaction_string_value(
            database,
            value_node,
            "transaction_isolation",
            &decoded
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        value = decoded;
    } else {
        return set_transaction_variable_invalid_node_value_error(
            database,
            "transaction_isolation",
            value_node
        );
    }

    if (mylite_execution_text_equals_ascii_case_insensitive(value, "REPEATABLE-READ")) {
        *out_isolation = MYLITE_TRANSACTION_ISOLATION_REPEATABLE_READ;
    } else if (mylite_execution_text_equals_ascii_case_insensitive(value, "READ-COMMITTED")) {
        *out_isolation = MYLITE_TRANSACTION_ISOLATION_READ_COMMITTED;
    } else if (mylite_execution_text_equals_ascii_case_insensitive(value, "READ-UNCOMMITTED")) {
        *out_isolation = MYLITE_TRANSACTION_ISOLATION_READ_UNCOMMITTED;
    } else if (mylite_execution_text_equals_ascii_case_insensitive(value, "SERIALIZABLE")) {
        *out_isolation = MYLITE_TRANSACTION_ISOLATION_SERIALIZABLE;
    } else {
        mylite_execution_transaction_set_system_variable_value_error(
            database,
            "transaction_isolation",
            value
        );
        rc = MYLITE_ERROR;
    }

    free(decoded);
    return rc;
}

static int parse_set_transaction_read_only_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    enum mylite_transaction_access_mode *out_access_mode
) {
    enum mylite_sql_ast_literal_kind literal_kind = MYLITE_SQL_AST_LITERAL_NONE;
    char *decoded = NULL;
    uint64_t magnitude = 0U;
    int rc = MYLITE_OK;

    if (out_access_mode == NULL) {
        set_runtime_error(database, "invalid transaction_read_only output");
        return MYLITE_ERROR;
    }
    *out_access_mode = MYLITE_TRANSACTION_ACCESS_READ_WRITE;
    if (value_node == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (value_node->kind == MYLITE_SQL_AST_SET_DEFAULT_VALUE) {
        return MYLITE_OK;
    }
    if (value_node->kind != MYLITE_SQL_AST_LITERAL) {
        return set_transaction_variable_invalid_node_value_error(
            database,
            "transaction_read_only",
            value_node
        );
    }

    literal_kind = mylite_sql_ast_node_literal_kind(value_node);
    if (literal_kind == MYLITE_SQL_AST_LITERAL_TRUE) {
        *out_access_mode = MYLITE_TRANSACTION_ACCESS_READ_ONLY;
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_FALSE) {
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_INTEGER) {
        if (mylite_execution_parse_unsigned_integer_literal(&value_node->span, &magnitude) !=
                MYLITE_OK ||
            magnitude > 1U) {
            return set_transaction_variable_invalid_node_value_error(
                database,
                "transaction_read_only",
                value_node
            );
        }
        if (magnitude == 1U) {
            *out_access_mode = MYLITE_TRANSACTION_ACCESS_READ_ONLY;
        }
        return MYLITE_OK;
    }
    if (literal_kind == MYLITE_SQL_AST_LITERAL_STRING) {
        rc = decode_set_transaction_string_value(
            database,
            value_node,
            "transaction_read_only",
            &decoded
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (mylite_execution_text_equals_ascii_case_insensitive(decoded, "ON")) {
            *out_access_mode = MYLITE_TRANSACTION_ACCESS_READ_ONLY;
        } else if (!mylite_execution_text_equals_ascii_case_insensitive(decoded, "OFF")) {
            mylite_execution_transaction_set_system_variable_value_error(
                database,
                "transaction_read_only",
                decoded
            );
            rc = MYLITE_ERROR;
        }
        free(decoded);
        return rc;
    }

    return set_transaction_variable_invalid_node_value_error(
        database,
        "transaction_read_only",
        value_node
    );
}

static int decode_set_transaction_string_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    const char *variable_name,
    char **out_value
) {
    size_t text_length = 0U;
    char unsupported_message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    char nul_message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];

    int unsupported_written = snprintf(
        unsupported_message,
        sizeof(unsupported_message),
        "SET %s supports only literal values",
        variable_name == NULL ? "transaction variable" : variable_name
    );
    int nul_written = snprintf(
        nul_message,
        sizeof(nul_message),
        "SET %s does not support NUL bytes",
        variable_name == NULL ? "transaction variable" : variable_name
    );

    if (unsupported_written < 0) {
        unsupported_message[0] = '\0';
    }
    if (nul_written < 0) {
        nul_message[0] = '\0';
    }
    return mylite_execution_decode_sql_string_literal(
        database,
        value_node,
        unsupported_message,
        nul_message,
        out_value,
        &text_length
    );
}

static int copy_set_transaction_identifier_value(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    char *value,
    size_t value_size
) {
    if (value == NULL || value_size == 0U) {
        set_runtime_error(database, "invalid transaction system variable value buffer");
        return MYLITE_ERROR;
    }
    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_IDENTIFIER) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    return copy_identifier_text(value_node, value, value_size, database);
}

static int set_transaction_variable_invalid_node_value_error(
    struct mylite_db *database,
    const char *variable_name,
    const struct mylite_sql_ast_node *value_node
) {
    char value[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    size_t copied = 0U;

    if (value_node == NULL || value_node->span.text == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    copied = value_node->span.length < sizeof(value) ? value_node->span.length : sizeof(value) - 1U;
    memcpy(value, value_node->span.text, copied);
    value[copied] = '\0';
    mylite_execution_transaction_set_system_variable_value_error(database, variable_name, value);
    return MYLITE_ERROR;
}

static const char *transaction_isolation_value_text(enum mylite_transaction_isolation isolation) {
    switch (isolation) {
    case MYLITE_TRANSACTION_ISOLATION_READ_COMMITTED:
        return "READ-COMMITTED";
    case MYLITE_TRANSACTION_ISOLATION_READ_UNCOMMITTED:
        return "READ-UNCOMMITTED";
    case MYLITE_TRANSACTION_ISOLATION_SERIALIZABLE:
        return "SERIALIZABLE";
    case MYLITE_TRANSACTION_ISOLATION_REPEATABLE_READ:
    default:
        return "REPEATABLE-READ";
    }
}

static const char *transaction_read_only_scalar_text(enum mylite_transaction_access_mode access_mode
) {
    return access_mode == MYLITE_TRANSACTION_ACCESS_READ_ONLY ? "1" : "0";
}

static const char *transaction_read_only_show_text(enum mylite_transaction_access_mode access_mode
) {
    return access_mode == MYLITE_TRANSACTION_ACCESS_READ_ONLY ? "ON" : "OFF";
}

static enum mylite_transaction_access_mode current_transaction_access_mode(
    const struct mylite_db *database
) {
    if (database->session.has_next_transaction_access_mode) {
        return database->session.next_transaction_access_mode;
    }
    return database->session.session_transaction_access_mode;
}

static enum mylite_transaction_isolation current_transaction_isolation(
    const struct mylite_db *database
) {
    if (database->session.has_next_transaction_isolation) {
        return database->session.next_transaction_isolation;
    }
    return database->session.session_transaction_isolation;
}

static enum mylite_transaction_access_mode effective_start_transaction_access_mode(
    const struct mylite_db *database,
    const struct transaction_characteristics *characteristics
) {
    if (characteristics->has_access_mode) {
        return characteristics->access_mode;
    }
    return current_transaction_access_mode(database);
}

static int reserve_consistent_snapshot_ignored_warning(struct mylite_db *database) {
    struct mylite_diagnostics *diagnostics = mylite_connection_diagnostics(database);
    size_t warning_count = mylite_diagnostics_warning_count(diagnostics);

    if (warning_count == SIZE_MAX) {
        mylite_diagnostics_set_error(diagnostics, MYLITE_NOMEM, "HY001", "too many warnings");
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    return mylite_diagnostics_reserve_warning_capacity(diagnostics, warning_count + 1U);
}

static int append_consistent_snapshot_ignored_warning(struct mylite_db *database) {
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_consistent_snapshot_ignored,
        "HY000",
        "InnoDB: WITH CONSISTENT SNAPSHOT was ignored because this phrase can only be used "
        "with REPEATABLE READ isolation level."
    );
}

static void clear_next_transaction_characteristics(struct mylite_db *database) {
    database->session.has_next_transaction_isolation = false;
    database->session.has_next_transaction_access_mode = false;
    database->session.next_transaction_isolation_from_system_variable = false;
    database->session.next_transaction_access_mode_from_system_variable = false;
}

static void clear_active_transaction_characteristics(struct mylite_db *database) {
    database->session.active_transaction_read_only = false;
    database->session.active_transaction_isolation = MYLITE_TRANSACTION_ISOLATION_REPEATABLE_READ;
}
