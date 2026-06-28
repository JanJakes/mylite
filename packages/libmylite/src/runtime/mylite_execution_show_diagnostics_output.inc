
static int execute_show_warnings_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[show_warnings_result_column_count] = {
        "Level",
        "Code",
        "Message",
    };
    struct planned_diagnostics_show_limit limit;
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    rc = plan_diagnostics_show_limit(database, child_at(statement, 0U), &limit);
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_warnings_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_show_diagnostics_rows(database, result, &limit);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_show_count_warnings_statement(
    struct mylite_db *database,
    mylite_result **out_result
) {
    static const char *const result_columns[show_count_warnings_result_column_count] = {
        "@@session.warning_count",
    };
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_count_warnings_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_show_count_warnings_row(database, result);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_show_errors_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[show_errors_result_column_count] = {
        "Level",
        "Code",
        "Message",
    };
    struct planned_diagnostics_show_limit limit;
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    rc = plan_diagnostics_show_limit(database, child_at(statement, 0U), &limit);
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_errors_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_show_errors_rows(database, result, &limit);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_show_count_errors_statement(
    struct mylite_db *database,
    mylite_result **out_result
) {
    static const char *const result_columns[show_count_errors_result_column_count] = {
        "@@session.error_count",
    };
    mylite_result *result = NULL;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_count_errors_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_show_count_errors_row(database, result);
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int plan_diagnostics_show_limit(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *limit_clause,
    struct planned_diagnostics_show_limit *out_limit
) {
    int rc = MYLITE_OK;

    if (out_limit == NULL) {
        set_runtime_error(database, "invalid diagnostics SHOW LIMIT plan");
        return MYLITE_ERROR;
    }

    out_limit->has_limit = false;
    out_limit->row_count = UINT64_MAX;
    out_limit->has_offset = false;
    out_limit->offset = 0U;
    if (limit_clause == NULL) {
        return MYLITE_OK;
    }
    if (limit_clause->kind != MYLITE_SQL_AST_LIMIT_CLAUSE) {
        set_unsupported_error(database, "diagnostics SHOW supports only literal LIMIT clauses");
        return MYLITE_ERROR;
    }

    rc = convert_diagnostics_show_limit_integer_literal(
        database,
        child_at(limit_clause, 0U),
        &out_limit->row_count
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    out_limit->has_limit = true;

    if (child_at(limit_clause, 1U) != NULL) {
        rc = convert_diagnostics_show_limit_integer_literal(
            database,
            child_at(limit_clause, 1U),
            &out_limit->offset
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        out_limit->has_offset = true;
    }

    return MYLITE_OK;
}

static int convert_diagnostics_show_limit_integer_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_value
) {
    uint64_t value = 0U;
    int rc = MYLITE_OK;

    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_unsupported_error(
            database,
            "diagnostics SHOW supports only unsigned decimal integer LIMIT literals"
        );
        return MYLITE_ERROR;
    }

    rc = parse_unsigned_integer_literal(&value_node->span, &value);
    if (rc != MYLITE_OK) {
        set_limit_out_of_range_error(database);
        return MYLITE_ERROR;
    }

    *out_value = value;
    return MYLITE_OK;
}

static int append_show_diagnostics_rows(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_diagnostics_show_limit *limit
) {
    const struct mylite_diagnostics *diagnostics = &database->previous_diagnostics;
    uint64_t total_count = 0U;
    uint64_t start_index = 0U;
    uint64_t row_count = UINT64_MAX;
    uint64_t emitted_count = 0U;
    uint64_t condition_count = 0U;
    uint64_t warning_count = 0U;
    int rc = previous_diagnostics_condition_count(diagnostics, &condition_count);

    if (limit->has_offset) {
        start_index = limit->offset;
    }
    if (limit->has_limit) {
        row_count = limit->row_count;
    }
    if (rc == MYLITE_OK) {
        warning_count = (uint64_t)mylite_diagnostics_warning_count(diagnostics);
        total_count = condition_count + warning_count;
    }

    for (uint64_t index = start_index;
         rc == MYLITE_OK && index < total_count && emitted_count < row_count;
         ++index, ++emitted_count) {
        if (index < warning_count) {
            const struct mylite_diagnostic_record *warning =
                mylite_diagnostics_warning_at(diagnostics, (size_t)index);
            if (warning == NULL) {
                set_runtime_error(database, "invalid previous diagnostics warning index");
                rc = MYLITE_ERROR;
            } else {
                rc = append_show_diagnostics_row(database, result, warning->level, warning);
            }
        } else if (condition_count != 0U) {
            rc = append_show_diagnostics_row(database, result, "Error", &diagnostics->condition);
        }
    }

    return rc;
}

static int append_show_diagnostics_row(
    struct mylite_db *database,
    mylite_result *result,
    const char *level,
    const struct mylite_diagnostic_record *record
) {
    char code_text[integer_text_capacity];
    const char *values[show_warnings_result_column_count] = {
        level,
        code_text,
        record == NULL ? NULL : record->message,
    };
    int written = 0;
    int rc = MYLITE_OK;

    if (record == NULL) {
        set_runtime_error(database, "invalid diagnostics record");
        return MYLITE_ERROR;
    }

    written = snprintf(code_text, sizeof(code_text), "%d", record->code);
    if (written < 0 || (size_t)written >= sizeof(code_text)) {
        set_runtime_error(database, "failed to format diagnostic code");
        return MYLITE_ERROR;
    }

    rc = mylite_result_append_text_row(result, values);
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }
    return rc;
}

static int append_show_count_warnings_row(struct mylite_db *database, mylite_result *result) {
    const struct mylite_diagnostics *diagnostics = &database->previous_diagnostics;
    char count_text[integer_text_capacity];
    const char *values[show_count_warnings_result_column_count] = {count_text};
    uint64_t condition_count = 0U;
    uint64_t count = 0U;
    int rc = previous_diagnostics_condition_count(diagnostics, &condition_count);

    if (rc != MYLITE_OK) {
        return rc;
    }

    count = condition_count + (uint64_t)mylite_diagnostics_show_warning_total_count(diagnostics);
    rc = format_uint64(database, count, count_text, sizeof(count_text));
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }

    return rc;
}

static int append_show_errors_rows(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_diagnostics_show_limit *limit
) {
    const struct mylite_diagnostics *diagnostics = &database->previous_diagnostics;
    uint64_t start_index = 0U;
    uint64_t row_count = UINT64_MAX;
    uint64_t error_index = 0U;
    uint64_t emitted_count = 0U;
    int rc = MYLITE_OK;

    if (limit->has_offset) {
        start_index = limit->offset;
    }
    if (limit->has_limit) {
        row_count = limit->row_count;
    }
    if (row_count == 0U) {
        return MYLITE_OK;
    }

    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_diagnostics_warning_count(diagnostics);
         ++index) {
        const struct mylite_diagnostic_record *record =
            mylite_diagnostics_warning_at(diagnostics, index);

        if (!diagnostic_record_level_is_error(record)) {
            continue;
        }
        if (error_index >= start_index && emitted_count < row_count) {
            rc = append_show_diagnostics_row(database, result, "Error", record);
            ++emitted_count;
        }
        ++error_index;
    }
    if (rc == MYLITE_OK && diagnostics_has_error_condition(diagnostics) &&
        error_index >= start_index && emitted_count < row_count) {
        rc = append_show_diagnostics_row(database, result, "Error", &diagnostics->condition);
    }
    return rc;
}

static int append_show_count_errors_row(struct mylite_db *database, mylite_result *result) {
    const struct mylite_diagnostics *diagnostics = &database->previous_diagnostics;
    char count_text[integer_text_capacity];
    const char *values[show_count_errors_result_column_count] = {count_text};
    uint64_t count = 0U;
    int rc = previous_diagnostics_error_count(diagnostics, &count);

    if (rc == MYLITE_OK) {
        rc = format_uint64(database, count, count_text, sizeof(count_text));
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }

    return rc;
}

static int previous_diagnostics_condition_count(
    const struct mylite_diagnostics *diagnostics,
    uint64_t *out_count
) {
    uint64_t warning_count = 0U;

    if (diagnostics == NULL || out_count == NULL) {
        return MYLITE_MISUSE;
    }

    warning_count = (uint64_t)mylite_diagnostics_show_warning_total_count(diagnostics);
    if (diagnostics_has_error_condition(diagnostics) && warning_count == UINT64_MAX) {
        return MYLITE_NOMEM;
    }

    *out_count = 0U;
    if (diagnostics_has_error_condition(diagnostics)) {
        *out_count = 1U;
    }
    return MYLITE_OK;
}

static bool diagnostics_has_error_condition(const struct mylite_diagnostics *diagnostics) {
    if (diagnostics == NULL) {
        return false;
    }
    if (mylite_diagnostics_errcode(diagnostics) == MYLITE_OK) {
        return false;
    }
    return true;
}

static int previous_diagnostics_error_count(
    const struct mylite_diagnostics *diagnostics,
    uint64_t *out_count
) {
    uint64_t count = 0U;

    if (diagnostics == NULL || out_count == NULL) {
        return MYLITE_MISUSE;
    }

    count = (uint64_t)mylite_diagnostics_error_warning_total_count(diagnostics);
    if (diagnostics_has_error_condition(diagnostics)) {
        if (count == UINT64_MAX) {
            return MYLITE_NOMEM;
        }
        ++count;
    }

    *out_count = count;
    return MYLITE_OK;
}

static bool diagnostic_record_level_is_error(const struct mylite_diagnostic_record *record) {
    if (record == NULL) {
        return false;
    }
    if (strcmp(record->level, "Error") != 0) {
        return false;
    }
    return true;
}

static int format_uint64(
    struct mylite_db *database,
    uint64_t value,
    char *buffer,
    size_t buffer_size
) {
    int written = snprintf(buffer, buffer_size, "%" PRIu64, value);

    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format unsigned integer");
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}
