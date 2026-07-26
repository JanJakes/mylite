static const uint64_t checksum_table_fnv_offset_basis = UINT64_C(0xcbf29ce484222325);
static const uint64_t checksum_table_fnv_prime = UINT64_C(0x100000001b3);

static int execute_checksum_table_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    const struct mylite_sql_ast_node *table_names = child_at(statement, 0U);
    const size_t table_count =
        table_names == NULL ? 0U : mylite_sql_ast_node_child_count(table_names);
    struct table_maintenance_target *targets = NULL;
    mylite_result *result = NULL;
    const enum checksum_table_option option = checksum_table_option_for_statement(statement);
    int rc = MYLITE_OK;

    if (table_count == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    targets = (struct table_maintenance_target *)calloc(table_count, sizeof(targets[0]));
    if (targets == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    for (size_t target_index = 0U; rc == MYLITE_OK && target_index < table_count; ++target_index) {
        rc = resolve_table_maintenance_target(
            database,
            child_at(table_names, target_index),
            &targets[target_index]
        );
        if (rc == MYLITE_OK) {
            rc = check_table_maintenance_duplicate_target(
                database,
                targets,
                target_index,
                &targets[target_index]
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_checksum_table_result_columns(database, result);
    }
    for (size_t target_index = 0U; rc == MYLITE_OK && target_index < table_count; ++target_index) {
        rc = append_checksum_table_target_row(database, result, &targets[target_index], option);
    }

    free(targets);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int append_checksum_table_result_columns(struct mylite_db *database, mylite_result *result) {
    const uint32_t table_collation_id =
        mylite_execution_table_maintenance_result_collation_id("latin1_swedish_ci");
    const struct mylite_result_column_descriptor descriptors[checksum_table_result_column_count] = {
        {
            .label = "Table",
            .schema_name = "",
            .table_name = "",
            .origin_schema_name = "",
            .origin_table_name = "",
            .origin_column_name = "",
            .logical_type = MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING,
            .flags = 0U,
            .charset_id = table_collation_id,
            .collation_id = table_collation_id,
            .display_length = checksum_table_name_display_length,
            .decimals = table_maintenance_mysql_approximate_decimals,
            .nullable = true,
        },
        {
            .label = "Checksum",
            .schema_name = "",
            .table_name = "",
            .origin_schema_name = "",
            .origin_table_name = "",
            .origin_column_name = "",
            .logical_type = MYLITE_RESULT_LOGICAL_TYPE_LONGLONG,
            .flags = MYLITE_RESULT_COLUMN_FLAG_BINARY | MYLITE_RESULT_COLUMN_FLAG_NUM,
            .charset_id = table_maintenance_mysql_binary_collation_id,
            .collation_id = table_maintenance_mysql_binary_collation_id,
            .display_length = checksum_table_checksum_display_length,
            .decimals = 0U,
            .nullable = true,
        },
    };
    int rc = MYLITE_OK;

    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < checksum_table_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column_descriptor(result, &descriptors[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    return rc;
}

static int append_checksum_table_target_row(
    struct mylite_db *database,
    mylite_result *result,
    const struct table_maintenance_target *target,
    enum checksum_table_option option
) {
    char checksum_value_buffer[checksum_table_checksum_display_length + 1U];
    const char *checksum_value = NULL;
    int rc = checksum_table_target_value(
        database,
        target,
        option,
        checksum_value_buffer,
        sizeof(checksum_value_buffer),
        &checksum_value
    );
    const char *const values[checksum_table_result_column_count] = {
        target == NULL ? NULL : target->display_name,
        checksum_value,
    };

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = mylite_result_append_text_row(result, values);
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }
    return append_checksum_table_target_warning(database, target);
}

static enum checksum_table_option checksum_table_option_for_statement(
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *option = child_at(statement, 1U);

    if (option == NULL) {
        return CHECKSUM_TABLE_OPTION_DEFAULT;
    }
    if (option->kind == MYLITE_SQL_AST_CHECKSUM_TABLE_QUICK_OPTION) {
        return CHECKSUM_TABLE_OPTION_QUICK;
    }
    if (option->kind == MYLITE_SQL_AST_CHECKSUM_TABLE_EXTENDED_OPTION) {
        return CHECKSUM_TABLE_OPTION_EXTENDED;
    }
    return CHECKSUM_TABLE_OPTION_DEFAULT;
}

static int checksum_table_target_value(
    struct mylite_db *database,
    const struct table_maintenance_target *target,
    enum checksum_table_option option,
    char *value_buffer,
    size_t value_buffer_size,
    const char **out_value
) {
    if (out_value == NULL) {
        set_runtime_error(database, "invalid checksum output");
        return MYLITE_ERROR;
    }
    *out_value = NULL;
    if (target == NULL || target->missing_schema || target->missing_table ||
        target->unsupported_kind || !target->has_table || option == CHECKSUM_TABLE_OPTION_QUICK) {
        return MYLITE_OK;
    }
    return compute_checksum_table_target_value(
        database,
        target,
        value_buffer,
        value_buffer_size,
        out_value
    );
}

static int compute_checksum_table_target_value(
    struct mylite_db *database,
    const struct table_maintenance_target *target,
    char *value_buffer,
    size_t value_buffer_size,
    const char **out_value
) {
    char *sql = NULL;
    sqlite3_stmt *statement = NULL;
    uint64_t checksum = checksum_table_fnv_offset_basis;
    bool has_rows = false;
    bool prepare_attempted = false;
    bool scan_attempted = false;
    int rc = build_checksum_table_scan_sql(database, target, &sql);
    int written = 0;

    if (rc == MYLITE_OK) {
        prepare_attempted = true;
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        scan_attempted = true;
        rc = scan_checksum_table_rows(database, statement, &checksum, &has_rows);
    }
    const int statement_rc = rc;
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);
    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        } else if (statement_rc == MYLITE_OK || (prepare_attempted && !scan_attempted)) {
            set_physical_sqlite_error(database);
        }
        return rc;
    }
    if (!has_rows) {
        checksum = 0U;
    }
    written = snprintf(value_buffer, value_buffer_size, "%" PRIu64, checksum);
    if (written < 0 || (size_t)written >= value_buffer_size) {
        set_runtime_error(database, "failed to format checksum value");
        return MYLITE_ERROR;
    }
    *out_value = value_buffer;
    return MYLITE_OK;
}

static int build_checksum_table_scan_sql(
    struct mylite_db *database,
    const struct table_maintenance_target *target,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    if (target == NULL || out_sql == NULL) {
        set_runtime_error(database, "invalid checksum scan request");
        return MYLITE_ERROR;
    }
    *out_sql = NULL;
    mylite_dynamic_string_init(&string);
    rc = mylite_dynamic_string_append(&string, "SELECT * FROM ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, target->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " ORDER BY rowid");
    }
    if (rc == MYLITE_OK) {
        *out_sql = mylite_dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }
    mylite_dynamic_string_deinit(&string);
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }
    return rc;
}

static int scan_checksum_table_rows(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    uint64_t *out_checksum,
    bool *out_has_rows
) {
    int sqlite_rc = SQLITE_OK;
    int column_count = 0;

    if (statement == NULL || out_checksum == NULL || out_has_rows == NULL) {
        set_runtime_error(database, "invalid checksum scan state");
        return MYLITE_ERROR;
    }
    column_count = sqlite3_column_count(statement);
    while ((sqlite_rc = sqlite3_step(statement)) == SQLITE_ROW) {
        *out_has_rows = true;
        checksum_table_update_byte(out_checksum, (unsigned char)'R');
        for (int column_index = 0; column_index < column_count; ++column_index) {
            const int column_type = sqlite3_column_type(statement, column_index);

            checksum_table_update_byte(out_checksum, (unsigned char)'C');
            checksum_table_update_byte(out_checksum, (unsigned char)column_type);
            switch (column_type) {
            case SQLITE_NULL:
                break;
            case SQLITE_INTEGER:
                checksum_table_update_u64(
                    out_checksum,
                    (uint64_t)sqlite3_column_int64(statement, column_index)
                );
                break;
            case SQLITE_FLOAT: {
                double value = sqlite3_column_double(statement, column_index);
                uint64_t bits = 0U;

                memcpy(&bits, &value, sizeof(bits));
                checksum_table_update_u64(out_checksum, bits);
                break;
            }
            case SQLITE_TEXT: {
                const unsigned char *text = sqlite3_column_text(statement, column_index);
                const int byte_count = sqlite3_column_bytes(statement, column_index);

                if (text == NULL && byte_count > 0) {
                    set_physical_sqlite_row_error(database);
                    return MYLITE_ERROR;
                }
                checksum_table_update_u64(out_checksum, (uint64_t)byte_count);
                checksum_table_update_bytes(out_checksum, text, (size_t)byte_count);
                break;
            }
            case SQLITE_BLOB: {
                const unsigned char *blob =
                    (const unsigned char *)sqlite3_column_blob(statement, column_index);
                const int byte_count = sqlite3_column_bytes(statement, column_index);

                if (blob == NULL && byte_count > 0) {
                    set_physical_sqlite_row_error(database);
                    return MYLITE_ERROR;
                }
                checksum_table_update_u64(out_checksum, (uint64_t)byte_count);
                checksum_table_update_bytes(out_checksum, blob, (size_t)byte_count);
                break;
            }
            default:
                set_physical_sqlite_row_error(database);
                return MYLITE_ERROR;
            }
        }
    }
    if (sqlite_rc != SQLITE_DONE) {
        const int rc = mylite_sqlite_status_to_mylite(sqlite_rc);

        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        } else {
            set_physical_sqlite_row_error(database);
        }
        return rc;
    }
    return MYLITE_OK;
}

static void checksum_table_update_bytes(
    uint64_t *checksum,
    const unsigned char *bytes,
    size_t byte_count
) {
    for (size_t byte_index = 0U; byte_index < byte_count; ++byte_index) {
        checksum_table_update_byte(checksum, bytes[byte_index]);
    }
}

static void checksum_table_update_u64(uint64_t *checksum, uint64_t value) {
    for (size_t byte_index = 0U; byte_index < sizeof(value); ++byte_index) {
        checksum_table_update_byte(
            checksum,
            (unsigned char)((value >> (byte_index * CHAR_BIT)) & UCHAR_MAX)
        );
    }
}

static void checksum_table_update_byte(uint64_t *checksum, unsigned char byte) {
    *checksum ^= (uint64_t)byte;
    *checksum *= checksum_table_fnv_prime;
}

static int append_checksum_table_target_warning(
    struct mylite_db *database,
    const struct table_maintenance_target *target
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = 0;

    if (target == NULL) {
        set_runtime_error(database, "invalid checksum table target");
        return MYLITE_ERROR;
    }
    if (target->missing_schema) {
        written = snprintf(message, sizeof(message), "Unknown database '%s'", target->schema_name);
        if (written < 0 || (size_t)written >= sizeof(message)) {
            set_runtime_error(database, "failed to format checksum schema diagnostic");
            return MYLITE_ERROR;
        }
        return append_checksum_table_warning(
            database,
            mysql_error_unknown_database,
            "42000",
            message
        );
    }
    if (target->missing_table) {
        written =
            snprintf(message, sizeof(message), "Table '%s' doesn't exist", target->display_name);
        if (written < 0 || (size_t)written >= sizeof(message)) {
            set_runtime_error(database, "failed to format checksum table diagnostic");
            return MYLITE_ERROR;
        }
        return append_checksum_table_warning(
            database,
            mysql_error_table_does_not_exist,
            "42S02",
            message
        );
    }
    if (target->unsupported_kind) {
        written =
            snprintf(message, sizeof(message), "'%s' is not BASE TABLE", target->display_name);
        if (written < 0 || (size_t)written >= sizeof(message)) {
            set_runtime_error(database, "failed to format checksum object-kind diagnostic");
            return MYLITE_ERROR;
        }
        return append_checksum_table_warning(database, mysql_error_not_view, "HY000", message);
    }

    return MYLITE_OK;
}

static int append_checksum_table_warning(
    struct mylite_db *database,
    int code,
    const char *sqlstate,
    const char *message
) {
    struct mylite_diagnostics *diagnostics = mylite_connection_diagnostics(database);
    const int rc = mylite_diagnostics_append_error(diagnostics, code, sqlstate, message);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }
    return rc;
}

static int execute_table_maintenance_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    const struct mylite_sql_ast_node *table_names = child_at(statement, 0U);
    const enum table_maintenance_operation operation =
        table_maintenance_operation_for_statement(statement);
    const size_t table_count =
        table_names == NULL ? 0U : mylite_sql_ast_node_child_count(table_names);
    struct table_maintenance_target *targets = NULL;
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    if (table_count == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    targets = (struct table_maintenance_target *)calloc(table_count, sizeof(targets[0]));
    if (targets == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    for (size_t target_index = 0U; rc == MYLITE_OK && target_index < table_count; ++target_index) {
        rc = resolve_table_maintenance_target(
            database,
            child_at(table_names, target_index),
            &targets[target_index]
        );
        if (rc == MYLITE_OK) {
            rc = check_table_maintenance_duplicate_target(
                database,
                targets,
                target_index,
                &targets[target_index]
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = execute_table_maintenance_targets(database, operation, targets, table_count);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK) {
        rc = append_table_maintenance_result_columns(database, result);
    }
    for (size_t target_index = 0U; rc == MYLITE_OK && target_index < table_count; ++target_index) {
        rc = append_table_maintenance_target_rows(
            database,
            result,
            operation,
            &targets[target_index]
        );
    }

    free(targets);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int execute_table_maintenance_targets(
    struct mylite_db *database,
    enum table_maintenance_operation operation,
    const struct table_maintenance_target *targets,
    size_t target_count
) {
    bool has_persistent_target = false;
    int rc = MYLITE_OK;

    if (operation != TABLE_MAINTENANCE_ANALYZE && operation != TABLE_MAINTENANCE_OPTIMIZE) {
        return MYLITE_OK;
    }
    for (size_t index = 0U; index < target_count; ++index) {
        if (targets[index].has_table &&
            targets[index].table.kind == MYLITE_CATALOG_TABLE_KIND_BASE) {
            has_persistent_target = true;
            break;
        }
    }
    if (operation == TABLE_MAINTENANCE_OPTIMIZE && has_persistent_target) {
        rc = execute_sqlite_schema_sql(database, "VACUUM");
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < target_count; ++index) {
        if (targets[index].has_table && !targets[index].missing_schema &&
            !targets[index].missing_table && !targets[index].unsupported_kind) {
            rc = execute_table_analyze_target(database, &targets[index]);
        }
    }
    return rc;
}

static int execute_table_analyze_target(
    struct mylite_db *database,
    const struct table_maintenance_target *target
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    mylite_dynamic_string_init(&string);
    rc = mylite_dynamic_string_append(
        &string,
        target->table.kind == MYLITE_CATALOG_TABLE_KIND_TEMPORARY ? "ANALYZE temp."
                                                                  : "ANALYZE main."
    );
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, target->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = execute_sqlite_schema_sql(database, string.text);
    }
    mylite_dynamic_string_deinit(&string);
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    return rc;
}

static enum table_maintenance_operation table_maintenance_operation_for_statement(
    const struct mylite_sql_ast_node *statement
) {
    switch (statement->kind) {
    case MYLITE_SQL_AST_ANALYZE_TABLE_STATEMENT:
        return TABLE_MAINTENANCE_ANALYZE;
    case MYLITE_SQL_AST_CHECK_TABLE_STATEMENT:
        return TABLE_MAINTENANCE_CHECK;
    case MYLITE_SQL_AST_OPTIMIZE_TABLE_STATEMENT:
        return TABLE_MAINTENANCE_OPTIMIZE;
    case MYLITE_SQL_AST_REPAIR_TABLE_STATEMENT:
        return TABLE_MAINTENANCE_REPAIR;
    default:
        return TABLE_MAINTENANCE_CHECK;
    }
}

static int append_table_maintenance_result_columns(
    struct mylite_db *database,
    mylite_result *result
) {
    static const char *const result_columns[table_maintenance_result_column_count] = {
        "Table",
        "Op",
        "Msg_type",
        "Msg_text",
    };
    int rc = MYLITE_OK;

    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < table_maintenance_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    return rc;
}

static int append_table_maintenance_target_rows(
    struct mylite_db *database,
    mylite_result *result,
    enum table_maintenance_operation operation,
    const struct table_maintenance_target *target
) {
    if (target->missing_schema) {
        return append_table_maintenance_unknown_schema_rows(database, result, operation, target);
    }
    if (target->missing_table || target->unsupported_kind) {
        return append_table_maintenance_unknown_table_rows(database, result, operation, target);
    }
    return append_table_maintenance_success_rows(database, result, operation, target);
}

static int append_table_maintenance_success_rows(
    struct mylite_db *database,
    mylite_result *result,
    enum table_maintenance_operation operation,
    const struct table_maintenance_target *target
) {
    const char *operation_text = table_maintenance_operation_text(operation);
    int rc = MYLITE_OK;

    if (operation == TABLE_MAINTENANCE_OPTIMIZE) {
        rc = append_table_maintenance_row(
            database,
            result,
            target->display_name,
            operation_text,
            "note",
            "Table does not support optimize, doing recreate + analyze instead"
        );
        if (rc == MYLITE_OK) {
            rc = append_table_maintenance_row(
                database,
                result,
                target->display_name,
                operation_text,
                "status",
                "OK"
            );
        }
        return rc;
    }
    if (operation == TABLE_MAINTENANCE_REPAIR) {
        return append_table_maintenance_row(
            database,
            result,
            target->display_name,
            operation_text,
            "note",
            "The storage engine for the table doesn't support repair"
        );
    }

    return append_table_maintenance_row(
        database,
        result,
        target->display_name,
        operation_text,
        "status",
        "OK"
    );
}

static int append_table_maintenance_unknown_schema_rows(
    struct mylite_db *database,
    mylite_result *result,
    enum table_maintenance_operation operation,
    const struct table_maintenance_target *target
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    const char *operation_text = table_maintenance_operation_text(operation);
    int written = snprintf(message, sizeof(message), "Unknown database '%s'", target->schema_name);
    int rc = MYLITE_OK;

    if (written < 0) {
        message[0] = '\0';
    }
    rc = append_table_maintenance_row(
        database,
        result,
        target->display_name,
        operation_text,
        "Error",
        message
    );
    if (rc == MYLITE_OK) {
        rc = append_table_maintenance_row(
            database,
            result,
            target->display_name,
            operation_text,
            "error",
            "Corrupt"
        );
    }
    return rc;
}

static int append_table_maintenance_unknown_table_rows(
    struct mylite_db *database,
    mylite_result *result,
    enum table_maintenance_operation operation,
    const struct table_maintenance_target *target
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    const char *operation_text = table_maintenance_operation_text(operation);
    int written =
        snprintf(message, sizeof(message), "Table '%s' doesn't exist", target->display_name);
    int rc = MYLITE_OK;

    if (written < 0) {
        message[0] = '\0';
    }
    rc = append_table_maintenance_row(
        database,
        result,
        target->display_name,
        operation_text,
        "Error",
        message
    );
    if (rc == MYLITE_OK) {
        rc = append_table_maintenance_row(
            database,
            result,
            target->display_name,
            operation_text,
            "status",
            "Operation failed"
        );
    }
    return rc;
}

static int append_table_maintenance_row(
    struct mylite_db *database,
    mylite_result *result,
    const char *table_name,
    const char *operation,
    const char *message_type,
    const char *message_text
) {
    const char *const values[table_maintenance_result_column_count] = {
        table_name,
        operation,
        message_type,
        message_text,
    };
    int rc = mylite_result_append_text_row(result, values);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }
    return rc;
}

static int resolve_table_maintenance_target(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *node,
    struct table_maintenance_target *out_target
) {
    struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_catalog_table_descriptor table = {0};
    bool schema_found = false;
    bool table_found = false;
    int rc = MYLITE_OK;

    *out_target = (struct table_maintenance_target){0};
    rc = mylite_execution_table_maintenance_resolve_target_names(
        database,
        node,
        out_target->schema_name,
        sizeof(out_target->schema_name),
        out_target->table_name,
        sizeof(out_target->table_name)
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = format_table_maintenance_display_name(database, out_target);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (mylite_catalog_name_is_reserved(out_target->schema_name)) {
        out_target->missing_schema = true;
        return MYLITE_OK;
    }
    rc = mylite_catalog_try_read_schema_by_name(
        database,
        out_target->schema_name,
        &schema,
        &schema_found
    );
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to read schema descriptor");
        return rc;
    }
    if (!schema_found) {
        out_target->missing_schema = true;
        return MYLITE_OK;
    }
    if (mylite_catalog_name_is_reserved(out_target->table_name)) {
        out_target->missing_table = true;
        return MYLITE_OK;
    }

    rc = mylite_temporary_catalog_try_read_table_by_name(
        &database->session.temporary_catalog,
        out_target->schema_name,
        out_target->table_name,
        &table,
        &table_found
    );
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to read temporary table descriptor");
        return rc;
    }
    if (!table_found) {
        rc = mylite_catalog_try_read_table_by_name(
            database,
            schema.schema_id,
            out_target->table_name,
            &table,
            &table_found
        );
        if (rc != MYLITE_OK) {
            set_internal_error_if_clear(database, rc, "failed to read table descriptor");
            return rc;
        }
    }
    if (!table_found) {
        out_target->missing_table = true;
        return MYLITE_OK;
    }
    if (table.kind != MYLITE_CATALOG_TABLE_KIND_BASE &&
        table.kind != MYLITE_CATALOG_TABLE_KIND_TEMPORARY) {
        out_target->unsupported_kind = true;
        return MYLITE_OK;
    }
    out_target->table = table;
    out_target->has_table = true;
    return MYLITE_OK;
}

static int check_table_maintenance_duplicate_target(
    struct mylite_db *database,
    const struct table_maintenance_target *targets,
    size_t target_count,
    const struct table_maintenance_target *target
) {
    for (size_t index = 0U; index < target_count; ++index) {
        if (mylite_execution_text_equals_ascii_case_insensitive(
                targets[index].schema_name,
                target->schema_name
            ) &&
            mylite_execution_text_equals_ascii_case_insensitive(
                targets[index].table_name,
                target->table_name
            )) {
            set_duplicate_table_alias_error(database, target->table_name);
            return MYLITE_ERROR;
        }
    }
    return MYLITE_OK;
}

static int format_table_maintenance_display_name(
    struct mylite_db *database,
    struct table_maintenance_target *target
) {
    int written = snprintf(
        target->display_name,
        sizeof(target->display_name),
        "%s.%s",
        target->schema_name,
        target->table_name
    );

    if (written < 0 || (size_t)written >= sizeof(target->display_name)) {
        set_identifier_too_long_error(database, "table");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static const char *table_maintenance_operation_text(enum table_maintenance_operation operation) {
    switch (operation) {
    case TABLE_MAINTENANCE_ANALYZE:
        return "analyze";
    case TABLE_MAINTENANCE_CHECK:
        return "check";
    case TABLE_MAINTENANCE_OPTIMIZE:
        return "optimize";
    case TABLE_MAINTENANCE_REPAIR:
        return "repair";
    }
    return "";
}
