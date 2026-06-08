static int alter_table_modify_column_from_plan(
    struct mylite_db *database,
    struct planned_alter_table_modify_column *plan
) {
    struct mylite_catalog_mutation mutation = {.active = false, .next_generation = 0U};
    bool physical_schema_changed = false;
    int rc = MYLITE_OK;

    if (plan->is_noop) {
        return MYLITE_OK;
    }
    if (plan->is_temporary) {
        return modify_temporary_column_from_plan(database, plan);
    }

    rc = mylite_catalog_begin_mutation(database, &mutation);
    if (rc == MYLITE_OK) {
        rc = alter_table_modify_column_in_mutation(
            database,
            &mutation,
            plan,
            &physical_schema_changed
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_commit_mutation(database, &mutation);
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, plan->failure_message);
        mylite_catalog_rollback_mutation(database, &mutation);
        return rc;
    }

    if (physical_schema_changed) {
        ++database->session.sqlite_schema_generation;
    }

    return MYLITE_OK;
}

static int modify_temporary_column_from_plan(
    struct mylite_db *database,
    struct planned_alter_table_modify_column *plan
) {
    int64_t validated_row_count = 0;
    bool physical_schema_changed = false;
    int rc = MYLITE_OK;

    if (plan == NULL || plan->table.kind != MYLITE_CATALOG_TABLE_KIND_TEMPORARY) {
        set_runtime_error(database, "invalid temporary modify-column plan");
        return MYLITE_ERROR;
    }
    if (plan->adds_inline_primary_key || plan->adds_inline_unique_key) {
        set_unsupported_error(
            database,
            "ALTER TABLE column replacement on temporary tables does not yet support inline keys"
        );
        return MYLITE_ERROR;
    }
    if (plan->is_noop) {
        return MYLITE_OK;
    }

    if (!plan->is_metadata_only) {
        rc = validate_modify_column_existing_rows(database, plan, &validated_row_count);
        if (rc == MYLITE_OK && plan->reports_rebuild_row_count) {
            plan->affected_rows = validated_row_count;
        }
    }
    if (rc == MYLITE_OK && plan->is_metadata_only &&
        !modify_column_name_matches(&plan->original_column, &plan->column)) {
        struct planned_alter_table_rename_column rename_plan = {
            .target = plan->target,
            .table = plan->table,
            .column = plan->original_column,
            .is_noop = false,
        };

        memcpy(rename_plan.new_column_name, plan->column.name, sizeof(rename_plan.new_column_name));
        rc = execute_physical_alter_table_rename_column(database, &rename_plan);
        physical_schema_changed = rc == MYLITE_OK;
    } else if (rc == MYLITE_OK && !plan->is_metadata_only) {
        rc = execute_physical_alter_table_modify_column(
            database,
            plan,
            database->session.sqlite_schema_generation + 1U
        );
        physical_schema_changed = rc == MYLITE_OK;
    }
    if (rc == MYLITE_OK) {
        rc = replace_temporary_column_descriptor(database, plan);
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, plan->failure_message);
        return rc;
    }
    if (physical_schema_changed) {
        ++database->session.sqlite_schema_generation;
    }

    return MYLITE_OK;
}

static int replace_temporary_column_descriptor(
    struct mylite_db *database,
    const struct planned_alter_table_modify_column *plan
) {
    struct mylite_catalog_column_descriptor replacement = {0};
    int rc = MYLITE_OK;

    if (plan->columns != NULL) {
        rc = mylite_temporary_catalog_replace_columns(
            &database->session.temporary_catalog,
            plan->table.table_id,
            plan->columns,
            plan->column_count
        );
    } else {
        make_modify_target_descriptor(plan, &replacement);
        rc = mylite_temporary_catalog_replace_column(
            &database->session.temporary_catalog,
            plan->table.table_id,
            plan->original_column.column_id,
            &replacement
        );
    }
    if (rc != MYLITE_OK) {
        set_internal_error_if_clear(database, rc, "failed to update temporary column descriptor");
    }
    return rc;
}

static int alter_table_modify_column_in_mutation(
    struct mylite_db *database,
    const struct mylite_catalog_mutation *mutation,
    struct planned_alter_table_modify_column *plan,
    bool *out_physical_schema_changed
) {
    int64_t validated_row_count = 0;
    int rc = MYLITE_OK;

    if (out_physical_schema_changed == NULL) {
        set_runtime_error(database, "invalid modify-column physical change output");
        return MYLITE_ERROR;
    }
    *out_physical_schema_changed = false;
    if (plan->is_noop) {
        return MYLITE_OK;
    }

    if (!plan->is_metadata_only) {
        rc = validate_modify_column_existing_rows(database, plan, &validated_row_count);
        if (rc == MYLITE_OK && plan->reports_rebuild_row_count) {
            plan->affected_rows = validated_row_count;
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_replace_column_in_mutation(
            database,
            mutation,
            plan->table.table_id,
            plan->original_column.column_id,
            plan->column.name,
            plan->column.logical_type,
            plan->column.physical_type,
            plan->column.is_nullable,
            true,
            plan->column.is_auto_increment,
            plan->column.default_kind,
            plan->column.default_integer,
            plan->column.default_text,
            plan->column.on_update_current_timestamp,
            plan->column.character_set_name,
            plan->column.collation_name,
            plan->column.comment,
            plan->column.is_generated,
            plan->column.generated_kind,
            plan->column.generation_expression,
            plan->column.sqlite_generation_expression
        );
    }
    if (rc == MYLITE_OK && plan->changes_position) {
        const struct mylite_catalog_column_reorder reorder = {
            .table_id = plan->table.table_id,
            .columns = plan->columns,
            .column_count = plan->column_count,
            .metadata_replaced_column_id = plan->original_column.column_id,
        };

        rc = mylite_catalog_reorder_columns_in_mutation(database, mutation, &reorder);
    }
    if (rc == MYLITE_OK && plan->is_metadata_only) {
        if (!modify_column_name_matches(&plan->original_column, &plan->column)) {
            struct planned_alter_table_rename_column rename_plan = {
                .target = plan->target,
                .table = plan->table,
                .column = plan->original_column,
                .is_noop = false,
            };

            memcpy(
                rename_plan.new_column_name,
                plan->column.name,
                sizeof(rename_plan.new_column_name)
            );
            rc = execute_physical_alter_table_rename_column(database, &rename_plan);
            if (rc == MYLITE_OK) {
                *out_physical_schema_changed = true;
            }
        }
    } else if (rc == MYLITE_OK) {
        rc = execute_physical_alter_table_modify_column(
            database,
            plan,
            mylite_catalog_mutation_generation(mutation)
        );
        if (rc == MYLITE_OK) {
            *out_physical_schema_changed = true;
        }
    }
    if (rc == MYLITE_OK) {
        rc = apply_alter_table_modify_column_inline_keys(database, mutation, plan);
        if (rc == MYLITE_OK && (plan->adds_inline_primary_key || plan->adds_inline_unique_key)) {
            *out_physical_schema_changed = true;
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_update_table_identity_in_mutation(
            database,
            mutation,
            plan->table.table_id,
            plan->table.schema_id,
            plan->table.name,
            NULL
        );
    }

    return rc;
}

static int collect_modify_column_rebuild_columns(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    struct planned_alter_table_modify_column *out_plan
) {
    struct mylite_catalog_column_descriptor *rebuild_columns = NULL;

    if (column_count > SIZE_MAX / sizeof(*rebuild_columns)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rebuild_columns = calloc(column_count, sizeof(*rebuild_columns));
    if (rebuild_columns == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    if (!out_plan->changes_position) {
        for (size_t index = 0U; index < column_count; ++index) {
            rebuild_columns[index] = columns[index];
            if (index == out_plan->column_index) {
                make_modify_target_descriptor(out_plan, &rebuild_columns[index]);
            }
        }
    } else {
        size_t source_index = 0U;

        for (size_t index = 0U; index < column_count; ++index) {
            if (index == out_plan->target_column_index) {
                make_modify_target_descriptor(out_plan, &rebuild_columns[index]);
            } else {
                while (source_index < column_count && source_index == out_plan->column_index) {
                    ++source_index;
                }
                if (source_index >= column_count) {
                    free(rebuild_columns);
                    set_runtime_error(database, "failed to plan column reorder");
                    return MYLITE_ERROR;
                }
                rebuild_columns[index] = columns[source_index];
                ++source_index;
            }
        }
    }

    out_plan->columns = rebuild_columns;
    return MYLITE_OK;
}

static int validate_modify_column_existing_rows(
    struct mylite_db *database,
    const struct planned_alter_table_modify_column *plan,
    int64_t *out_row_count
) {
    sqlite3_stmt *statement = NULL;
    struct mylite_catalog_column_descriptor target_column = {0};
    char *sql = NULL;
    size_t row_number = 0U;
    int sqlite_rc = SQLITE_OK;
    int rc = build_alter_table_modify_validation_sql(plan, &sql);

    *out_row_count = 0;
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    make_modify_target_descriptor(plan, &target_column);
    while (rc == MYLITE_OK && (sqlite_rc = sqlite3_step(statement)) == SQLITE_ROW) {
        int sqlite_type = sqlite3_column_type(statement, 0);

        if (row_number >= (size_t)INT64_MAX) {
            set_runtime_error(database, plan->row_count_overflow_message);
            rc = MYLITE_ERROR;
            break;
        }
        ++row_number;
        if (sqlite_type == SQLITE_NULL) {
            rc = validate_modify_column_null_value(database, &target_column, row_number);
        } else if (column_descriptor_is_decimal(&target_column) &&
                   (sqlite_type == SQLITE_INTEGER || sqlite_type == SQLITE_FLOAT)) {
            const unsigned char *text = sqlite3_column_text(statement, 0);
            int byte_count = sqlite3_column_bytes(statement, 0);

            if (text == NULL || byte_count < 0) {
                set_physical_sqlite_row_error(database);
                rc = MYLITE_ERROR;
            } else {
                rc = validate_existing_decimal_text_for_column(
                    database,
                    (const char *)text,
                    (size_t)byte_count,
                    &target_column,
                    row_number
                );
            }
        } else if (column_descriptor_is_approximate(&target_column) &&
                   (sqlite_type == SQLITE_INTEGER || sqlite_type == SQLITE_FLOAT)) {
            rc = validate_existing_approximate_for_column(
                database,
                sqlite3_column_double(statement, 0),
                &target_column,
                row_number
            );
        } else if (sqlite_type == SQLITE_INTEGER) {
            rc = validate_existing_integer_for_column(
                database,
                (int64_t)sqlite3_column_int64(statement, 0),
                &target_column,
                row_number,
                plan->integer_support_message
            );
        } else if (column_descriptor_is_binary_string_family(&target_column) &&
                   (sqlite_type == SQLITE_TEXT || sqlite_type == SQLITE_BLOB)) {
            rc = validate_existing_binary_bytes_for_column(
                database,
                sqlite3_column_blob(statement, 0),
                sqlite3_column_bytes(statement, 0),
                &target_column,
                row_number
            );
        } else if (sqlite_type == SQLITE_BLOB &&
                   alter_table_modify_changes_binary_to_string(plan, &target_column)) {
            rc = validate_existing_text_for_column(
                database,
                sqlite3_column_blob(statement, 0),
                sqlite3_column_bytes(statement, 0),
                &target_column,
                row_number
            );
        } else if (sqlite_type == SQLITE_TEXT) {
            rc = validate_existing_text_for_column(
                database,
                sqlite3_column_text(statement, 0),
                sqlite3_column_bytes(statement, 0),
                &target_column,
                row_number
            );
        } else {
            set_physical_sqlite_row_error(database);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && sqlite_rc != SQLITE_DONE) {
        rc = mylite_sqlite_status_to_mylite(sqlite_rc);
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);

    if (rc == MYLITE_OK) {
        *out_row_count = (int64_t)row_number;
    } else if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    } else if (mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) == MYLITE_OK) {
        set_physical_sqlite_row_error(database);
        rc = MYLITE_ERROR;
    }

    return rc;
}

static int validate_modify_column_null_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number
) {
    if (target_column->is_nullable) {
        return MYLITE_OK;
    }
    if (column_descriptor_is_text_family(target_column)) {
        set_data_truncated_error(database, target_column->name, row_number);
    } else {
        set_invalid_use_of_null_error(database);
    }

    return MYLITE_ERROR;
}

static int validate_existing_integer_for_column(
    struct mylite_db *database,
    int64_t value,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    const char *unsupported_message
) {
    struct integer_column_range range = {0};
    int rc = integer_range_for_column(database, column, unsupported_message, &range);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (value < 0) {
        const uint64_t int64_min_magnitude = (uint64_t)INT64_MAX + 1U;
        uint64_t magnitude = value == INT64_MIN ? int64_min_magnitude : (uint64_t)(-value);

        if (range.negative_abs_max == 0U || magnitude > range.negative_abs_max) {
            set_out_of_range_error(database, column->name, row_number);
            return MYLITE_ERROR;
        }
        return MYLITE_OK;
    }

    if ((uint64_t)value > range.positive_max) {
        set_out_of_range_error(database, column->name, row_number);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_existing_approximate_for_column(
    struct mylite_db *database,
    double value,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number
) {
    struct approximate_type_info info = {
        .type_class = APPROXIMATE_TYPE_DOUBLE,
        .is_unsigned = false,
    };
    int rc = approximate_type_info_for_logical_type(column->logical_type, &info);

    if (rc != MYLITE_OK) {
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }
    if (!isfinite(value) || (info.is_unsigned && value < 0.0) ||
        (info.type_class == APPROXIMATE_TYPE_FLOAT && fabs(value) > FLT_MAX)) {
        set_out_of_range_error(database, column->name, row_number);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_existing_decimal_text_for_column(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number
) {
    struct decimal_type_info info = {0};
    struct planned_value value = {0};
    bool is_negative = false;
    size_t start = 0U;
    int rc = MYLITE_OK;

    if (text == NULL || column == NULL || !column_descriptor_is_decimal(column)) {
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }
    if (text_length > 0U && text[0] == '-') {
        is_negative = true;
        start = 1U;
    }
    rc = decimal_type_info_for_logical_type(column->logical_type, &info);
    if (rc == MYLITE_OK) {
        rc = canonicalize_decimal_text(
            database,
            text + start,
            text_length - start,
            is_negative,
            &info,
            column->name,
            row_number,
            false,
            &value
        );
    }
    planned_value_deinit(&value);
    return rc;
}

static int validate_existing_text_for_column(
    struct mylite_db *database,
    const unsigned char *text,
    int byte_count,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number
) {
    if (text == NULL || byte_count < 0) {
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_varchar(column)) {
        return validate_varchar_text(
            database,
            column,
            (struct varchar_text_validation){
                .text = (const char *)text,
                .text_length = (size_t)byte_count,
                .row_number = row_number,
            }
        );
    }
    if (column_descriptor_is_char(column)) {
        return validate_canonical_char_text(
            database,
            column,
            (struct varchar_text_validation){
                .text = (const char *)text,
                .text_length = (size_t)byte_count,
                .row_number = row_number,
            }
        );
    }
    if (column_descriptor_is_text_family(column)) {
        return validate_text_family_text(
            database,
            column,
            (struct varchar_text_validation){
                .text = (const char *)text,
                .text_length = (size_t)byte_count,
                .row_number = row_number,
            }
        );
    }
    if (column_descriptor_is_decimal(column)) {
        return validate_existing_decimal_text_for_column(
            database,
            (const char *)text,
            (size_t)byte_count,
            column,
            row_number
        );
    }
    if (column_descriptor_physical_type_equals(column, "INTEGER")) {
        return validate_existing_text_integer_for_column(
            database,
            (const char *)text,
            (size_t)byte_count,
            column,
            row_number
        );
    }
    if (column_descriptor_is_datetime(column)) {
        if (predicate_datetime_text_admitted(database, (const char *)text, (size_t)byte_count)) {
            return MYLITE_OK;
        }
        set_incorrect_datetime_value_error(database, (const char *)text, column->name, row_number);
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_timestamp(column)) {
        if (predicate_timestamp_text_admitted(database, (const char *)text, (size_t)byte_count)) {
            return MYLITE_OK;
        }
        set_incorrect_datetime_value_error(database, (const char *)text, column->name, row_number);
        return MYLITE_ERROR;
    }

    set_physical_sqlite_row_error(database);
    return MYLITE_ERROR;
}

static int validate_existing_text_integer_for_column(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number
) {
    bool is_negative = false;
    bool exceeds_range = false;
    uint64_t magnitude = 0U;
    enum mylite_execution_dml_numeric_parse_result parse_result =
        mylite_execution_parse_dml_integer_string_text(text, text_length, &magnitude, &is_negative);
    int rc = MYLITE_OK;

    if (parse_result == MYLITE_EXECUTION_DML_NUMERIC_PARSE_INVALID) {
        set_incorrect_integer_value_error(database, text, column->name, row_number);
        return MYLITE_ERROR;
    }
    if (parse_result == MYLITE_EXECUTION_DML_NUMERIC_PARSE_OVERFLOW) {
        set_out_of_range_error(database, column->name, row_number);
        return MYLITE_ERROR;
    }

    rc = dml_integer_value_exceeds_column_range(
        database,
        magnitude,
        is_negative,
        column,
        &exceeds_range
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (exceeds_range) {
        set_out_of_range_error(database, column->name, row_number);
        return MYLITE_ERROR;
    }
    if (parse_result == MYLITE_EXECUTION_DML_NUMERIC_PARSE_TRUNCATED) {
        set_data_truncated_error(database, column->name, row_number);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_existing_binary_bytes_for_column(
    struct mylite_db *database,
    const void *bytes,
    int byte_count,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number
) {
    char *copy = NULL;
    size_t copy_length = 0U;
    int rc = MYLITE_OK;

    if ((bytes == NULL && byte_count != 0) || byte_count < 0) {
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }
    copy_length = (size_t)byte_count;
    copy = malloc(copy_length + 1U);
    if (copy == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (copy_length != 0U) {
        memcpy(copy, bytes, copy_length);
    }
    copy[copy_length] = '\0';
    rc = convert_binary_bytes_for_column(database, column, row_number, false, &copy, &copy_length);
    free(copy);
    return rc;
}

static void make_modify_target_descriptor(
    const struct planned_alter_table_modify_column *plan,
    struct mylite_catalog_column_descriptor *out_column
) {
    *out_column = plan->original_column;
    memcpy(out_column->name, plan->column.name, sizeof(out_column->name));
    snprintf(
        out_column->logical_type,
        sizeof(out_column->logical_type),
        "%s",
        plan->column.logical_type
    );
    snprintf(
        out_column->physical_type,
        sizeof(out_column->physical_type),
        "%s",
        plan->column.physical_type
    );
    out_column->is_nullable = plan->column.is_nullable;
    out_column->is_auto_increment = plan->column.is_auto_increment;
    out_column->on_update_current_timestamp = plan->column.on_update_current_timestamp;
    out_column->default_kind = plan->column.default_kind;
    out_column->default_integer = plan->column.default_integer;
    snprintf(
        out_column->default_text,
        sizeof(out_column->default_text),
        "%s",
        plan->column.default_text
    );
    snprintf(
        out_column->character_set_name,
        sizeof(out_column->character_set_name),
        "%s",
        plan->column.character_set_name
    );
    snprintf(
        out_column->collation_name,
        sizeof(out_column->collation_name),
        "%s",
        plan->column.collation_name
    );
}

static int execute_physical_alter_table_modify_column(
    struct mylite_db *database,
    const struct planned_alter_table_modify_column *plan,
    uint64_t generation
) {
    char temporary_physical_name[MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY];
    char *sql = NULL;
    int rc = build_modify_temporary_physical_name(
        plan,
        generation,
        temporary_physical_name,
        sizeof(temporary_physical_name)
    );

    if (rc == MYLITE_OK) {
        rc = build_alter_table_modify_create_sql(plan, temporary_physical_name, &sql);
    }
    if (rc == MYLITE_OK) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);
    sql = NULL;

    if (rc == MYLITE_OK) {
        if (alter_table_modify_needs_binary_copy_materialization(plan)) {
            rc = execute_physical_alter_table_modify_copy_rows(
                database,
                plan,
                temporary_physical_name
            );
        } else {
            rc = build_alter_table_modify_copy_sql(plan, temporary_physical_name, &sql);
            if (rc == MYLITE_OK) {
                rc = execute_sqlite_schema_sql(database, sql);
            }
        }
    }
    free(sql);
    sql = NULL;

    if (rc == MYLITE_OK) {
        rc = execute_physical_drop_table(database, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = build_alter_table_rename_physical_table_sql(
            temporary_physical_name,
            plan->table.physical_name,
            &sql
        );
    }
    if (rc == MYLITE_OK) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);
    sql = NULL;

    if (rc == MYLITE_OK) {
        rc = build_alter_table_modify_indexes_sql(plan, &sql);
    }
    if (rc == MYLITE_OK && sql != NULL) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);

    return rc;
}

static int execute_physical_alter_table_order_by(
    struct mylite_db *database,
    const struct planned_alter_table_order_by *plan,
    int64_t *out_affected_rows
) {
    sqlite3_stmt *statement = NULL;
    char temporary_physical_name[MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY];
    char *sql = NULL;
    bool transaction_started = false;
    bool copy_failed = false;
    int64_t affected_rows = 0;
    int sqlite_rc = SQLITE_OK;
    int rc = build_alter_table_order_temporary_physical_name(
        plan,
        database->session.sqlite_schema_generation + 1U,
        temporary_physical_name,
        sizeof(temporary_physical_name)
    );

    *out_affected_rows = 0;
    if (rc == MYLITE_OK) {
        rc = execute_sqlite_control_sql(database, "BEGIN IMMEDIATE");
    }
    if (rc == MYLITE_OK) {
        transaction_started = true;
        rc = build_alter_table_order_create_sql(plan, temporary_physical_name, &sql);
    }
    if (rc == MYLITE_OK) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);
    sql = NULL;

    if (rc == MYLITE_OK) {
        rc = build_alter_table_order_copy_sql(plan, temporary_physical_name, &sql);
    }
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            affected_rows = (int64_t)sqlite3_changes64(database->sqlite);
        } else {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finalize_sqlite_statement(statement, rc);
    statement = NULL;
    if (rc != MYLITE_OK) {
        copy_failed = true;
    }
    free(sql);
    sql = NULL;

    if (rc == MYLITE_OK) {
        rc = execute_physical_drop_table(database, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = build_alter_table_rename_physical_table_sql(
            temporary_physical_name,
            plan->table.physical_name,
            &sql
        );
    }
    if (rc == MYLITE_OK) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);
    sql = NULL;

    if (rc == MYLITE_OK) {
        rc = execute_sqlite_control_sql(database, "COMMIT");
        if (rc == MYLITE_OK) {
            transaction_started = false;
        }
    }
    if (rc != MYLITE_OK && transaction_started) {
        (void)execute_sqlite_control_sql(database, "ROLLBACK");
    }
    if (rc != MYLITE_OK && rc != MYLITE_NOMEM && copy_failed) {
        set_physical_sqlite_row_error(database);
    }
    if (rc == MYLITE_OK) {
        ++database->session.sqlite_schema_generation;
        *out_affected_rows = affected_rows;
    }

    return rc;
}

static int execute_physical_alter_table_force(
    struct mylite_db *database,
    const struct planned_alter_table_force *plan
) {
    sqlite3_stmt *statement = NULL;
    char temporary_physical_name[MYLITE_CATALOG_PHYSICAL_NAME_CAPACITY];
    char *sql = NULL;
    bool transaction_started = false;
    bool copy_failed = false;
    int sqlite_rc = SQLITE_OK;
    int rc = build_alter_table_force_temporary_physical_name(
        plan,
        database->session.sqlite_schema_generation + 1U,
        temporary_physical_name,
        sizeof(temporary_physical_name)
    );

    if (rc == MYLITE_OK) {
        rc = execute_sqlite_control_sql(database, "BEGIN IMMEDIATE");
    }
    if (rc == MYLITE_OK) {
        transaction_started = true;
        rc = build_alter_table_force_create_sql(plan, temporary_physical_name, &sql);
    }
    if (rc == MYLITE_OK) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);
    sql = NULL;

    if (rc == MYLITE_OK) {
        rc = build_alter_table_force_copy_sql(plan, temporary_physical_name, &sql);
    }
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc != SQLITE_DONE) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finalize_sqlite_statement(statement, rc);
    statement = NULL;
    if (rc != MYLITE_OK) {
        copy_failed = true;
    }
    free(sql);
    sql = NULL;

    if (rc == MYLITE_OK) {
        rc = execute_physical_drop_table(database, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = build_alter_table_rename_physical_table_sql(
            temporary_physical_name,
            plan->table.physical_name,
            &sql
        );
    }
    if (rc == MYLITE_OK) {
        rc = execute_sqlite_schema_sql(database, sql);
    }
    free(sql);
    sql = NULL;

    if (rc == MYLITE_OK) {
        rc = execute_sqlite_control_sql(database, "COMMIT");
        if (rc == MYLITE_OK) {
            transaction_started = false;
        }
    }
    if (rc != MYLITE_OK && transaction_started) {
        (void)execute_sqlite_control_sql(database, "ROLLBACK");
    }
    if (rc != MYLITE_OK && rc != MYLITE_NOMEM && copy_failed) {
        set_physical_sqlite_row_error(database);
    }
    if (rc == MYLITE_OK) {
        ++database->session.sqlite_schema_generation;
    }

    return rc;
}
