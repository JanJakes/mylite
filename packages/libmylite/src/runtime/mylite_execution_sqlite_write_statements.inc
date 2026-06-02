static int execute_sqlite_schema_sql(struct mylite_db *database, const char *sql) {
    int sqlite_rc = sqlite3_exec(database->sqlite, sql, NULL, NULL, NULL);
    int rc = mylite_sqlite_status_to_mylite(sqlite_rc);

    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
        return rc;
    }
    if (rc != MYLITE_OK) {
        set_physical_sqlite_error(database);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int execute_sqlite_control_sql(const struct mylite_db *database, const char *sql) {
    int sqlite_rc = sqlite3_exec(database->sqlite, sql, NULL, NULL, NULL);

    return mylite_sqlite_status_to_mylite(sqlite_rc);
}

static int prepare_sqlite_statement(
    const struct mylite_db *database,
    const char *sql,
    sqlite3_stmt **out_statement
) {
    sqlite3_stmt *statement = NULL;
    int sqlite_rc = SQLITE_OK;

    *out_statement = NULL;
    sqlite_rc = sqlite3_prepare_v2(
        database->sqlite,
        sql,
        sqlite_use_nul_terminated_string,
        &statement,
        NULL
    );
    if (sqlite_rc != SQLITE_OK) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    *out_statement = statement;

    return MYLITE_OK;
}

static int finalize_sqlite_statement(sqlite3_stmt *statement, int rc) {
    int sqlite_rc = SQLITE_OK;

    if (statement == NULL) {
        return rc;
    }

    sqlite_rc = sqlite3_finalize(statement);
    if (rc != MYLITE_OK) {
        return rc;
    }

    return mylite_sqlite_status_to_mylite(sqlite_rc);
}

static int bind_insert_row(sqlite3_stmt *statement, const struct planned_insert *plan, size_t row) {
    int sqlite_rc = sqlite3_reset(statement);

    if (sqlite_rc == SQLITE_OK) {
        sqlite_rc = sqlite3_clear_bindings(statement);
    }
    if (sqlite_rc != SQLITE_OK) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    int bind_index = 1;

    for (size_t column_index = 0U; column_index < plan->column_count; ++column_index) {
        const struct planned_value *value = &plan->rows[row].values[column_index];
        int rc = MYLITE_OK;

        if (plan->columns[column_index].is_generated) {
            continue;
        }
        rc = bind_planned_value_parameter(statement, bind_index, value);
        if (rc != MYLITE_OK) {
            return rc;
        }
        ++bind_index;
    }

    return MYLITE_OK;
}

static int step_insert_row(sqlite3_stmt *statement, int *out_sqlite_rc) {
    int sqlite_rc = sqlite3_step(statement);

    if (out_sqlite_rc != NULL) {
        *out_sqlite_rc = sqlite_rc;
    }
    if (sqlite_rc != SQLITE_DONE) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    return MYLITE_OK;
}

static bool sqlite_status_is_constraint(int sqlite_rc) {
    enum { sqlite_primary_result_code_mask = 0xff };

    return (sqlite_rc == SQLITE_CONSTRAINT ||
            (sqlite_rc & sqlite_primary_result_code_mask) == SQLITE_CONSTRAINT) != 0;
}

static int reset_insert_statement_after_constraint(sqlite3_stmt *statement) {
    int reset_rc = sqlite3_reset(statement);

    if (reset_rc != SQLITE_OK && !sqlite_status_is_constraint(reset_rc)) {
        return mylite_sqlite_status_to_mylite(reset_rc);
    }

    return MYLITE_OK;
}

static int handle_check_constraint_violation(
    struct mylite_db *database,
    int64_t table_id,
    bool ignore_errors,
    bool *out_was_check_violation
) {
    const char *physical_name = NULL;
    struct mylite_catalog_check_constraint_descriptor check_constraint = {0};
    bool found = false;
    int rc = MYLITE_OK;

    *out_was_check_violation = false;
    if (!sqlite_error_message_has_check_constraint(
            sqlite3_errmsg(database->sqlite),
            &physical_name
        )) {
        return MYLITE_OK;
    }

    rc = mylite_catalog_try_read_check_constraint_by_physical_name(
        database,
        table_id,
        physical_name,
        &check_constraint,
        &found
    );
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to read CHECK constraint descriptor");
        return rc;
    }
    if (!found) {
        return MYLITE_OK;
    }

    *out_was_check_violation = true;
    return set_or_warn_check_constraint_violation(database, &check_constraint, ignore_errors);
}

static int handle_generated_not_null_violation(
    struct mylite_db *database,
    int sqlite_rc,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    bool *out_was_generated_not_null_violation
) {
    const char *column_name = NULL;
    size_t column_name_length = 0U;
    int extended_rc = SQLITE_OK;

    *out_was_generated_not_null_violation = false;
    extended_rc = database == NULL ? sqlite_rc : sqlite3_extended_errcode(database->sqlite);
    if (sqlite_rc != SQLITE_CONSTRAINT_NOTNULL && extended_rc != SQLITE_CONSTRAINT_NOTNULL) {
        return MYLITE_OK;
    }

    if (sqlite_error_message_has_not_null_column(
            sqlite3_errmsg(database->sqlite),
            &column_name,
            &column_name_length
        )) {
        for (size_t column_index = 0U; column_index < column_count; ++column_index) {
            const char *descriptor_name = columns[column_index].name;
            size_t descriptor_name_length = 0U;

            if (descriptor_name == NULL) {
                continue;
            }
            descriptor_name_length = strlen(descriptor_name);
            if (columns[column_index].is_generated && !columns[column_index].is_nullable &&
                descriptor_name_length == column_name_length &&
                strncmp(descriptor_name, column_name, column_name_length) == 0) {
                set_bad_null_error(database, descriptor_name);
                *out_was_generated_not_null_violation = true;
                return MYLITE_ERROR;
            }
        }
        return MYLITE_OK;
    }

    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        const char *descriptor_name = columns[column_index].name;

        if (descriptor_name != NULL && columns[column_index].is_generated &&
            !columns[column_index].is_nullable) {
            set_bad_null_error(database, descriptor_name);
            *out_was_generated_not_null_violation = true;
            return MYLITE_ERROR;
        }
    }

    return MYLITE_OK;
}

static int handle_generated_range_constraint_violation(
    struct mylite_db *database,
    struct generated_constraint_columns columns,
    size_t row_number,
    bool *out_was_generated_range_violation
) {
    const char *physical_name = NULL;
    size_t column_index = 0U;

    *out_was_generated_range_violation = false;
    if (!sqlite_error_message_has_check_constraint(
            sqlite3_errmsg(database->sqlite),
            &physical_name
        )) {
        return MYLITE_OK;
    }
    if (!generated_range_constraint_column_index(physical_name, &column_index) ||
        columns.columns == NULL || column_index >= columns.column_count ||
        !columns.columns[column_index].is_generated) {
        return MYLITE_OK;
    }

    set_out_of_range_error(database, columns.columns[column_index].name, row_number);
    *out_was_generated_range_violation = true;
    return MYLITE_ERROR;
}

static bool sqlite_error_message_has_check_constraint(
    const char *message,
    const char **out_physical_name
) {
    static const char prefix[] = "CHECK constraint failed: ";
    size_t prefix_length = sizeof(prefix) - 1U;

    if (out_physical_name != NULL) {
        *out_physical_name = NULL;
    }
    if (message == NULL || strncmp(message, prefix, prefix_length) != 0) {
        return false;
    }
    if (out_physical_name != NULL) {
        *out_physical_name = message + prefix_length;
    }
    return true;
}

static bool sqlite_error_message_has_not_null_column(
    const char *message,
    const char **out_column_name,
    size_t *out_column_name_length
) {
    static const char prefix[] = "NOT NULL constraint failed: ";
    const char *column_name = NULL;
    size_t prefix_length = sizeof(prefix) - 1U;
    size_t length = 0U;

    if (out_column_name != NULL) {
        *out_column_name = NULL;
    }
    if (out_column_name_length != NULL) {
        *out_column_name_length = 0U;
    }
    if (message == NULL || strncmp(message, prefix, prefix_length) != 0) {
        return false;
    }

    column_name = message + prefix_length;
    for (const char *cursor = column_name; *cursor != '\0'; ++cursor) {
        if (*cursor == '.') {
            column_name = cursor + 1;
        }
    }
    length = strlen(column_name);
    if (length == 0U) {
        return false;
    }
    if (out_column_name != NULL) {
        *out_column_name = column_name;
    }
    if (out_column_name_length != NULL) {
        *out_column_name_length = length;
    }
    return true;
}

static bool generated_range_constraint_column_index(
    const char *physical_name,
    size_t *out_column_index
) {
    enum { decimal_radix = 10U };

    static const char prefix[] = "_mylite_generated_column_range_";
    size_t prefix_length = sizeof(prefix) - 1U;
    size_t value = 0U;

    if (out_column_index != NULL) {
        *out_column_index = 0U;
    }
    if (physical_name == NULL || strncmp(physical_name, prefix, prefix_length) != 0 ||
        physical_name[prefix_length] == '\0') {
        return false;
    }
    for (size_t index = prefix_length; physical_name[index] != '\0'; ++index) {
        unsigned char byte = (unsigned char)physical_name[index];

        if (byte < '0' || byte > '9') {
            return false;
        }
        if (value > (SIZE_MAX - (size_t)(byte - '0')) / decimal_radix) {
            return false;
        }
        value = (value * decimal_radix) + (size_t)(byte - '0');
    }
    if (out_column_index != NULL) {
        *out_column_index = value;
    }
    return true;
}

static int set_or_warn_check_constraint_violation(
    struct mylite_db *database,
    const struct mylite_catalog_check_constraint_descriptor *check_constraint,
    bool ignore_errors
) {
    if (!ignore_errors) {
        set_check_constraint_violated_error(database, check_constraint->name);
        return MYLITE_ERROR;
    }

    return append_check_constraint_warning(database, check_constraint->name);
}
