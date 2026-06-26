static int execute_insert_from_plan(
    struct mylite_db *database,
    const struct planned_insert *plan,
    mylite_result *result
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    struct insert_execution_counters counters = {0};
    struct mylite_statement_transaction transaction = {0};
    int rc = build_insert_sql(plan, &sql);

    if (plan->has_auto_increment) {
        counters.auto_increment_next_after_rows = plan->auto_increment_next;
    }
    if (rc == MYLITE_OK && plan->has_auto_increment) {
        rc = ensure_persistent_auto_increment_high_water_slot(database, &plan->table);
    }
    if (rc == MYLITE_OK) {
        rc = begin_statement_transaction(database, &transaction);
    }
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = execute_insert_plan_rows(database, statement, plan, &counters);
    }
    rc = finalize_sqlite_statement(statement, rc);
    statement = NULL;
    if (rc == MYLITE_OK && plan->has_auto_increment &&
        counters.auto_increment_next_after_rows != plan->auto_increment_next) {
        rc = update_table_auto_increment_next(
            database,
            &plan->table,
            counters.auto_increment_next_after_rows
        );
    }
    if (rc == MYLITE_OK) {
        rc = touch_persistent_table_updated_time(
            database,
            &plan->table,
            counters.affected_rows,
            false
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_child_foreign_keys_after_write(database, plan->table.table_id);
    }
    if (rc == MYLITE_OK) {
        rc = commit_statement_transaction(database, &transaction);
    }
    if (rc != MYLITE_OK) {
        rollback_statement_transaction(database, &transaction);
    }
    free(sql);
    if (rc == MYLITE_OK && plan->has_auto_increment &&
        counters.auto_increment_next_after_rows != plan->auto_increment_next) {
        rc = record_persistent_auto_increment_high_water(
            database,
            &plan->table,
            counters.auto_increment_next_after_rows
        );
    }

    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
            return rc;
        }
        if (mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) != MYLITE_OK) {
            return rc;
        }
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }

    mylite_result_set_affected_rows(result, counters.affected_rows);
    if (counters.inserted_generated_auto_increment) {
        mylite_result_set_insert_id(
            result,
            (uint64_t)counters.first_inserted_generated_auto_increment
        );
        database->session.last_insert_id =
            (uint64_t)counters.first_inserted_generated_auto_increment;
        database->session.insert_id = 0U;
    } else if (counters.inserted_explicit_auto_increment) {
        mylite_result_set_insert_id(
            result,
            (uint64_t)counters.last_inserted_explicit_auto_increment
        );
    }

    return MYLITE_OK;
}

static int execute_load_data_infile_from_plan(
    struct mylite_db *database,
    const struct planned_load_data_infile *plan,
    mylite_result *result
) {
    FILE *file = NULL;
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    struct insert_execution_counters counters = {0};
    struct mylite_statement_transaction transaction = {0};
    int rc = build_insert_sql(&plan->insert, &sql);

    if (plan->insert.has_auto_increment) {
        counters.auto_increment_next_after_rows = plan->insert.auto_increment_next;
    }
    if (rc == MYLITE_OK && plan->insert.has_auto_increment) {
        rc = ensure_persistent_auto_increment_high_water_slot(database, &plan->insert.table);
    }
    if (rc == MYLITE_OK) {
        errno = 0;
        file = fopen(plan->file_path, "rb");
        if (file == NULL) {
            set_load_data_file_error(database, plan->file_path, errno);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        rc = begin_statement_transaction(database, &transaction);
    }
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = execute_load_data_file_rows(database, file, statement, plan, &counters);
    }
    rc = finalize_sqlite_statement(statement, rc);
    statement = NULL;
    if (file != NULL) {
        if (fclose(file) != 0 && rc == MYLITE_OK) {
            set_load_data_file_error(database, plan->file_path, errno);
            rc = MYLITE_ERROR;
        }
        file = NULL;
    }
    if (rc == MYLITE_OK && plan->insert.has_auto_increment &&
        counters.auto_increment_next_after_rows != plan->insert.auto_increment_next) {
        rc = update_table_auto_increment_next(
            database,
            &plan->insert.table,
            counters.auto_increment_next_after_rows
        );
    }
    if (rc == MYLITE_OK) {
        rc = touch_persistent_table_updated_time(
            database,
            &plan->insert.table,
            counters.affected_rows,
            false
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_child_foreign_keys_after_write(database, plan->insert.table.table_id);
    }
    if (rc == MYLITE_OK) {
        rc = commit_statement_transaction(database, &transaction);
    }
    if (rc != MYLITE_OK) {
        rollback_statement_transaction(database, &transaction);
    }
    free(sql);
    if (rc == MYLITE_OK && plan->insert.has_auto_increment &&
        counters.auto_increment_next_after_rows != plan->insert.auto_increment_next) {
        rc = record_persistent_auto_increment_high_water(
            database,
            &plan->insert.table,
            counters.auto_increment_next_after_rows
        );
    }

    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
            return rc;
        }
        if (mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) != MYLITE_OK) {
            return rc;
        }
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }

    mylite_result_set_affected_rows(result, counters.affected_rows);
    if (counters.inserted_generated_auto_increment) {
        mylite_result_set_insert_id(
            result,
            (uint64_t)counters.first_inserted_generated_auto_increment
        );
        database->session.last_insert_id =
            (uint64_t)counters.first_inserted_generated_auto_increment;
        database->session.insert_id = 0U;
    } else if (counters.inserted_explicit_auto_increment) {
        mylite_result_set_insert_id(
            result,
            (uint64_t)counters.last_inserted_explicit_auto_increment
        );
    }

    return MYLITE_OK;
}

static int execute_load_data_file_rows(
    struct mylite_db *database,
    FILE *file,
    sqlite3_stmt *statement,
    const struct planned_load_data_infile *plan,
    struct insert_execution_counters *counters
) {
    struct mylite_dynamic_string field;
    struct load_data_row row = {0};
    uint64_t physical_row_number = 0U;
    size_t loaded_row_count = 0U;
    bool escape_pending = false;
    bool row_pending = false;
    int rc = MYLITE_OK;
    int byte = 0;

    mylite_dynamic_string_init(&field);
    while (rc == MYLITE_OK && (byte = fgetc(file)) != EOF) {
        row_pending = true;
        if (escape_pending) {
            rc = mylite_dynamic_string_append_char(&field, '\\');
            if (rc == MYLITE_OK) {
                rc = mylite_dynamic_string_append_char(&field, (char)byte);
            }
            escape_pending = false;
            continue;
        }
        if (byte == '\\') {
            escape_pending = true;
            continue;
        }
        if (byte == '\t') {
            rc = finish_load_data_field(database, &row, &field, &escape_pending);
            continue;
        }
        if (byte == '\n') {
            rc = finish_load_data_field(database, &row, &field, &escape_pending);
            if (rc == MYLITE_OK) {
                ++physical_row_number;
                rc = process_load_data_row(
                    database,
                    plan,
                    statement,
                    &row,
                    physical_row_number,
                    &loaded_row_count,
                    counters
                );
            }
            load_data_row_reset(&row);
            row_pending = false;
            continue;
        }
        rc = mylite_dynamic_string_append_char(&field, (char)byte);
    }
    if (rc == MYLITE_OK && ferror(file) != 0) {
        set_load_data_file_error(database, plan->file_path, errno);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && row_pending) {
        rc = finish_load_data_field(database, &row, &field, &escape_pending);
        if (rc == MYLITE_OK) {
            ++physical_row_number;
            rc = process_load_data_row(
                database,
                plan,
                statement,
                &row,
                physical_row_number,
                &loaded_row_count,
                counters
            );
        }
    }

    load_data_row_deinit(&row);
    mylite_dynamic_string_deinit(&field);
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    return rc;
}

static int finish_load_data_field(
    struct mylite_db *database,
    struct load_data_row *row,
    struct mylite_dynamic_string *field,
    bool *escape_pending
) {
    char *raw_text = NULL;
    size_t raw_text_length = 0U;
    int rc = MYLITE_OK;

    if (*escape_pending) {
        rc = mylite_dynamic_string_append_char(field, '\\');
        *escape_pending = false;
    }
    if (rc == MYLITE_OK && field->text == NULL) {
        rc = mylite_dynamic_string_append(field, "");
    }
    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        }
        return rc;
    }

    raw_text_length = field->length;
    raw_text = mylite_dynamic_string_take(field);
    mylite_dynamic_string_init(field);
    if (raw_text == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    return load_data_row_append_field(database, row, raw_text, raw_text_length);
}

static int load_data_row_append_field(
    struct mylite_db *database,
    struct load_data_row *row,
    char *raw_text,
    size_t raw_text_length
) {
    char *text = NULL;
    size_t text_length = 0U;
    bool is_null = false;
    int rc = decode_load_data_field_text(
        database,
        raw_text,
        raw_text_length,
        &text,
        &text_length,
        &is_null
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (row->field_count == row->field_capacity) {
        size_t new_capacity = row->field_capacity == 0U ? 4U : row->field_capacity * 2U;
        struct load_data_field *new_fields = NULL;

        if (new_capacity < row->field_capacity || new_capacity > SIZE_MAX / sizeof(*new_fields)) {
            free(text);
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        new_fields = realloc(row->fields, new_capacity * sizeof(*new_fields));
        if (new_fields == NULL) {
            free(text);
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        row->fields = new_fields;
        row->field_capacity = new_capacity;
    }

    row->fields[row->field_count] = (struct load_data_field){
        .text = text,
        .text_length = text_length,
        .is_null = is_null,
    };
    ++row->field_count;
    return MYLITE_OK;
}

static int decode_load_data_field_text(
    struct mylite_db *database,
    char *raw_text,
    size_t raw_text_length,
    char **out_text,
    size_t *out_text_length,
    bool *out_is_null
) {
    struct mylite_dynamic_string decoded;
    int rc = MYLITE_OK;

    *out_text = NULL;
    *out_text_length = 0U;
    *out_is_null = false;
    if (raw_text_length == 2U && raw_text[0] == '\\' && raw_text[1] == 'N') {
        free(raw_text);
        *out_is_null = true;
        return MYLITE_OK;
    }

    mylite_dynamic_string_init(&decoded);
    for (size_t index = 0U; rc == MYLITE_OK && index < raw_text_length; ++index) {
        char byte = raw_text[index];

        if (byte != '\\' || index + 1U >= raw_text_length) {
            rc = mylite_dynamic_string_append_char(&decoded, byte);
            continue;
        }

        ++index;
        switch (raw_text[index]) {
        case '0':
            rc = mylite_dynamic_string_append_char(&decoded, '\0');
            break;
        case 'b':
            rc = mylite_dynamic_string_append_char(&decoded, '\b');
            break;
        case 'n':
            rc = mylite_dynamic_string_append_char(&decoded, '\n');
            break;
        case 'r':
            rc = mylite_dynamic_string_append_char(&decoded, '\r');
            break;
        case 't':
            rc = mylite_dynamic_string_append_char(&decoded, '\t');
            break;
        case 'Z':
            rc = mylite_dynamic_string_append_char(&decoded, '\032');
            break;
        case '\\':
            rc = mylite_dynamic_string_append_char(&decoded, '\\');
            break;
        default:
            rc = mylite_dynamic_string_append_char(&decoded, raw_text[index]);
            break;
        }
    }
    free(raw_text);
    if (rc == MYLITE_OK && decoded.text == NULL) {
        rc = mylite_dynamic_string_append(&decoded, "");
    }
    if (rc == MYLITE_OK) {
        *out_text_length = decoded.length;
        *out_text = mylite_dynamic_string_take(&decoded);
        if (*out_text == NULL) {
            rc = MYLITE_NOMEM;
        }
    }
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    mylite_dynamic_string_deinit(&decoded);
    return rc;
}

static int process_load_data_row(
    struct mylite_db *database,
    const struct planned_load_data_infile *plan,
    sqlite3_stmt *statement,
    const struct load_data_row *row,
    uint64_t physical_row_number,
    size_t *loaded_row_count,
    struct insert_execution_counters *counters
) {
    if (physical_row_number <= plan->ignore_line_count) {
        return MYLITE_OK;
    }
    if (*loaded_row_count == SIZE_MAX) {
        set_runtime_error(database, "LOAD DATA row count is too large");
        return MYLITE_ERROR;
    }
    ++(*loaded_row_count);
    return execute_load_data_import_row(
        database,
        plan,
        statement,
        row,
        *loaded_row_count,
        counters
    );
}

static int execute_load_data_import_row(
    struct mylite_db *database,
    const struct planned_load_data_infile *plan,
    sqlite3_stmt *statement,
    const struct load_data_row *input_row,
    size_t row_number,
    struct insert_execution_counters *counters
) {
    struct planned_insert row_plan = plan->insert;
    struct planned_insert_row row = {0};
    int rc = convert_load_data_row_values(database, plan, input_row, row_number, &row);

    if (rc == MYLITE_OK && insert_allows_implicit_default_adjustment(database, &plan->insert) &&
        row_number == 1U) {
        rc = append_insert_omitted_column_warnings(
            database,
            &plan->insert,
            plan->target_indexes,
            plan->target_count
        );
    }
    if (rc == MYLITE_OK) {
        row_plan.rows = &row;
        row_plan.row_count = 1U;
        row_plan.auto_increment_next = counters->auto_increment_next_after_rows;
        row_plan.auto_increment_next_after_statement = counters->auto_increment_next_after_rows;
        rc = plan_insert_auto_increment_values(database, &row_plan);
    }
    if (rc == MYLITE_OK) {
        rc = plan_insert_string_key_values(database, &row_plan);
    }
    if (rc == MYLITE_OK) {
        rc = execute_insert_plan_rows(database, statement, &row_plan, counters);
    }

    planned_insert_row_deinit(&row, plan->insert.column_count);
    return rc;
}

static int convert_load_data_row_values(
    struct mylite_db *database,
    const struct planned_load_data_infile *plan,
    const struct load_data_row *input_row,
    size_t row_number,
    struct planned_insert_row *out_row
) {
    int rc = MYLITE_OK;

    if (input_row->field_count < plan->target_count) {
        if (session_sql_mode_is_strict(database)) {
            set_load_data_row_missing_error(database, row_number);
            return MYLITE_ERROR;
        }
        rc = append_load_data_row_missing_warnings(
            database,
            (struct load_data_missing_warning_request){
                .row_number = row_number,
                .warning_count = plan->target_count - input_row->field_count,
            }
        );
    } else if (input_row->field_count > plan->target_count) {
        if (session_sql_mode_is_strict(database)) {
            set_load_data_row_truncated_error(database, row_number);
            return MYLITE_ERROR;
        }
        rc = append_load_data_row_truncated_warning(database, row_number);
    }
    if (rc == MYLITE_OK) {
        rc = allocate_insert_row_values(database, &plan->insert, out_row);
    }
    for (size_t target_index = 0U; rc == MYLITE_OK && target_index < plan->target_count;
         ++target_index) {
        size_t column_index = plan->target_indexes[target_index];

        if (target_index >= input_row->field_count) {
            rc = convert_load_data_missing_field_value(
                database,
                &plan->insert.columns[column_index],
                &out_row->values[column_index]
            );
            continue;
        }
        rc = convert_load_data_field_value(
            database,
            &plan->insert.columns[column_index],
            &input_row->fields[target_index],
            row_number,
            &out_row->values[column_index]
        );
    }

    return rc;
}

static int convert_load_data_field_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    const struct load_data_field *field,
    size_t row_number,
    struct planned_value *out_value
) {
    char *text = NULL;
    bool handled_empty_temporal = false;
    int rc = MYLITE_OK;

    planned_value_deinit(out_value);
    *out_value = (struct planned_value){.is_null = true, .is_text = false, .integer = 0};
    if (field->is_null) {
        return convert_load_data_null_field(database, column, row_number, out_value);
    }
    if (strcmp(column->physical_type, "INTEGER") == 0) {
        return convert_load_data_integer_field(database, column, field, row_number, out_value);
    }
    if (column_descriptor_is_char(column) || column_descriptor_is_varchar(column) ||
        column_descriptor_is_text_family(column)) {
        return convert_load_data_text_field(database, column, field, row_number, out_value);
    }

    rc = convert_load_data_empty_temporal_field(
        database,
        column,
        field,
        row_number,
        out_value,
        &handled_empty_temporal
    );
    if (handled_empty_temporal || rc != MYLITE_OK) {
        return rc;
    }

    text = copy_load_data_field_text(database, field);
    if (text == NULL) {
        return MYLITE_NOMEM;
    }
    if (column_descriptor_is_year(column)) {
        return convert_year_string_value(
            database,
            text,
            field->text_length,
            column,
            row_number,
            false,
            out_value
        );
    }
    if (column_descriptor_is_date(column)) {
        return convert_relaxed_date_storage_text(
            database,
            text,
            field->text_length,
            column->name,
            row_number,
            false,
            out_value
        );
    }
    if (column_descriptor_is_time(column)) {
        return canonicalize_time_text(
            database,
            text,
            field->text_length,
            column->name,
            row_number,
            false,
            out_value
        );
    }
    if (column_descriptor_is_datetime(column)) {
        return convert_relaxed_datetime_storage_text(
            database,
            text,
            field->text_length,
            column->name,
            row_number,
            false,
            database == NULL ? 0 : database->session.time_zone_offset_minutes,
            out_value
        );
    }
    if (column_descriptor_is_timestamp(column)) {
        return convert_relaxed_timestamp_storage_text(
            database,
            text,
            field->text_length,
            column->name,
            row_number,
            false,
            0,
            out_value
        );
    }

    free(text);
    set_unsupported_error(
        database,
        "LOAD DATA INFILE supports only baseline integer, character, and temporal targets"
    );
    return MYLITE_ERROR;
}

static int convert_load_data_missing_field_value(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    planned_value_deinit(out_value);
    *out_value = (struct planned_value){.is_null = true, .is_text = false, .integer = 0};
    if (load_data_missing_field_stores_null(column)) {
        return MYLITE_OK;
    }
    return make_insert_ignore_implicit_value(database, column, out_value);
}

static bool load_data_missing_field_stores_null(
    const struct mylite_catalog_column_descriptor *column
) {
    if (column == NULL) {
        return false;
    }
    return (column->is_nullable &&
            (column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_NONE ||
             column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_NULL_EXPRESSION ||
             column->default_kind == MYLITE_CATALOG_COLUMN_DEFAULT_NO_EXPLICIT)) != 0;
}

static int convert_load_data_empty_temporal_field(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    const struct load_data_field *field,
    size_t row_number,
    struct planned_value *out_value,
    bool *out_handled
) {
    int rc = MYLITE_OK;

    *out_handled = false;
    if (session_sql_mode_is_strict(database) || column == NULL || field == NULL ||
        field->text_length != 0U ||
        (!column_descriptor_is_year(column) && !column_descriptor_is_date(column) &&
         !column_descriptor_is_time(column) && !column_descriptor_is_datetime(column) &&
         !column_descriptor_is_timestamp(column))) {
        return MYLITE_OK;
    }

    if (column_descriptor_is_year(column)) {
        char *text = copy_temporal_text(database, "", 0U);

        *out_handled = true;
        if (text == NULL) {
            return MYLITE_NOMEM;
        }
        return convert_year_string_value(database, text, 0U, column, row_number, true, out_value);
    }
    if (column_descriptor_is_date(column)) {
        *out_handled = true;
        rc = append_out_of_range_warning(database, column->name, row_number);
        if (rc != MYLITE_OK) {
            return rc;
        }
        return make_zero_date_value(database, out_value);
    }
    if (column_descriptor_is_time(column)) {
        *out_handled = true;
        return make_zero_time_value(database, out_value);
    }
    if (column_descriptor_is_datetime(column)) {
        *out_handled = true;
        rc = append_out_of_range_warning(database, column->name, row_number);
        if (rc != MYLITE_OK) {
            return rc;
        }
        return make_zero_datetime_value(database, out_value);
    }
    if (column_descriptor_is_timestamp(column)) {
        *out_handled = true;
        rc = append_out_of_range_warning(database, column->name, row_number);
        if (rc != MYLITE_OK) {
            return rc;
        }
        return make_zero_timestamp_value(database, out_value);
    }

    return MYLITE_OK;
}

static int convert_load_data_null_field(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    size_t row_number,
    struct planned_value *out_value
) {
    if (column_descriptor_is_auto_increment(column) || column->is_nullable) {
        out_value->is_null = true;
        return MYLITE_OK;
    }
    if (session_sql_mode_is_strict(database)) {
        set_load_data_null_to_not_null_error(database, column->name, row_number);
        return MYLITE_ERROR;
    }

    int rc = append_load_data_null_to_not_null_warning(database, column->name, row_number);
    if (rc != MYLITE_OK) {
        return rc;
    }
    return make_insert_ignore_implicit_value(database, column, out_value);
}

static int convert_load_data_integer_field(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    const struct load_data_field *field,
    size_t row_number,
    struct planned_value *out_value
) {
    bool is_negative = false;
    bool strict_mode = session_sql_mode_is_strict(database);
    uint64_t magnitude = 0U;
    enum mylite_execution_dml_numeric_parse_result parse_result =
        mylite_execution_parse_dml_integer_string_text(
            field->text,
            field->text_length,
            &magnitude,
            &is_negative
        );
    int rc = MYLITE_OK;

    if (parse_result == MYLITE_EXECUTION_DML_NUMERIC_PARSE_INVALID) {
        if (strict_mode) {
            set_incorrect_integer_value_error(database, field->text, column->name, row_number);
            return MYLITE_ERROR;
        }
        rc =
            append_incorrect_integer_value_warning(database, field->text, column->name, row_number);
        if (rc == MYLITE_OK) {
            *out_value = (struct planned_value){.is_null = false, .is_text = false, .integer = 0};
        }
        return rc;
    }
    if (parse_result == MYLITE_EXECUTION_DML_NUMERIC_PARSE_OVERFLOW) {
        if (strict_mode) {
            set_out_of_range_error(database, column->name, row_number);
            return MYLITE_ERROR;
        }
        out_value->is_null = false;
        return clip_integer_for_column(
            database,
            is_negative,
            column,
            row_number,
            &out_value->integer
        );
    }

    bool exceeds_range = false;
    rc = dml_integer_value_exceeds_column_range(
        database,
        magnitude,
        is_negative,
        column,
        &exceeds_range
    );
    if (rc == MYLITE_OK && parse_result == MYLITE_EXECUTION_DML_NUMERIC_PARSE_TRUNCATED &&
        !exceeds_range) {
        if (strict_mode) {
            set_data_truncated_error(database, column->name, row_number);
            return MYLITE_ERROR;
        }
        rc = append_data_truncated_warning(database, column->name, row_number);
    }
    if (rc == MYLITE_OK) {
        out_value->is_null = false;
        if (strict_mode) {
            rc = convert_integer_for_column_with_policy(
                database,
                magnitude,
                is_negative,
                column,
                row_number,
                false,
                &out_value->integer
            );
        } else {
            rc = convert_integer_for_column_with_policy(
                database,
                magnitude,
                is_negative,
                column,
                row_number,
                true,
                &out_value->integer
            );
        }
    }
    return rc;
}

static int convert_load_data_text_field(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    const struct load_data_field *field,
    size_t row_number,
    struct planned_value *out_value
) {
    char *text = copy_load_data_field_text(database, field);
    struct char_text_conversion conversion = {
        .text = text,
        .text_length = field->text_length,
        .row_number = row_number,
    };
    bool allow_nonspace_truncation = dml_allows_string_truncation_adjustment(database, false);
    int rc = MYLITE_OK;

    if (text == NULL) {
        return MYLITE_NOMEM;
    }
    if (column_descriptor_is_char(column)) {
        rc = convert_char_text(database, column, &conversion, allow_nonspace_truncation);
    } else if (column_descriptor_is_varchar(column)) {
        rc = convert_varchar_text(database, column, &conversion, allow_nonspace_truncation);
    } else {
        rc = convert_text_family_text(
            database,
            column,
            &conversion,
            allow_nonspace_truncation,
            false
        );
    }
    if (rc != MYLITE_OK) {
        free(text);
        return rc;
    }
    return assign_text_value(database, text, conversion.text_length, out_value);
}

static char *copy_load_data_field_text(
    struct mylite_db *database,
    const struct load_data_field *field
) {
    char *copy = malloc(field->text_length + 1U);

    if (copy == NULL) {
        set_nomem_error(database);
        return NULL;
    }
    memcpy(copy, field->text, field->text_length);
    copy[field->text_length] = '\0';
    return copy;
}

static void load_data_row_deinit(struct load_data_row *row) {
    if (row == NULL) {
        return;
    }

    for (size_t index = 0U; index < row->field_count; ++index) {
        free(row->fields[index].text);
    }
    free(row->fields);
    *row = (struct load_data_row){0};
}

static void load_data_row_reset(struct load_data_row *row) {
    if (row == NULL) {
        return;
    }
    for (size_t index = 0U; index < row->field_count; ++index) {
        free(row->fields[index].text);
    }
    row->field_count = 0U;
}

static int execute_insert_plan_rows(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_insert *plan,
    struct insert_execution_counters *counters
) {
    int rc = MYLITE_OK;

    for (size_t row_index = 0U; rc == MYLITE_OK && row_index < plan->row_count; ++row_index) {
        bool skip_row = false;

        if (plan->ignore_errors && plan->foreign_key_count != 0U) {
            rc = validate_insert_ignore_row_foreign_keys(database, plan, row_index, &skip_row);
        }
        if (rc == MYLITE_OK && skip_row) {
            continue;
        }
        if (rc == MYLITE_OK) {
            rc = execute_insert_plan_row_with_retries(
                database,
                statement,
                plan,
                row_index,
                counters
            );
        }
    }

    return rc;
}

static int execute_insert_plan_row_with_retries(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_insert *plan,
    size_t row_index,
    struct insert_execution_counters *counters
) {
    size_t replace_attempt_count = 0U;
    bool row_complete = false;
    int rc = MYLITE_OK;

    while (rc == MYLITE_OK && !row_complete) {
        int sqlite_step_rc = SQLITE_OK;

        rc = execute_insert_plan_row(statement, plan, row_index, &sqlite_step_rc);
        if (rc == MYLITE_OK) {
            row_complete = true;
            rc = finish_successful_insert_plan_row(database, plan, row_index, counters);
        } else if (sqlite_status_is_constraint(sqlite_step_rc)) {
            rc = handle_insert_plan_constraint(
                database,
                sqlite_step_rc,
                statement,
                plan,
                row_index,
                &replace_attempt_count,
                counters,
                &row_complete
            );
        }
    }

    return rc;
}

static int finish_successful_insert_plan_row(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    struct insert_execution_counters *counters
) {
    ++counters->affected_rows;
    record_inserted_explicit_auto_increment(plan, row_index, counters);
    record_inserted_generated_auto_increment(plan, row_index, counters);
    if (!plan->has_auto_increment) {
        return MYLITE_OK;
    }

    return advance_auto_increment_after_insert_row(
        database,
        plan,
        row_index,
        &counters->auto_increment_next_after_rows
    );
}

static int handle_insert_plan_constraint(
    struct mylite_db *database,
    int sqlite_step_rc,
    sqlite3_stmt *statement,
    const struct planned_insert *plan,
    size_t row_index,
    size_t *replace_attempt_count,
    struct insert_execution_counters *counters,
    bool *out_row_complete
) {
    int rc = MYLITE_OK;

    *out_row_complete = false;
    if (sqlite_status_is_constraint(sqlite_step_rc)) {
        bool was_generated_range_violation = false;
        bool was_check_violation = false;
        bool was_generated_not_null_violation = false;

        rc = handle_generated_range_constraint_violation(
            database,
            (struct generated_constraint_columns){
                .columns = plan->columns,
                .column_count = plan->column_count,
            },
            row_index + 1U,
            &was_generated_range_violation
        );
        if (rc != MYLITE_OK || was_generated_range_violation) {
            return rc;
        }
        rc = handle_check_constraint_violation(
            database,
            plan->table.table_id,
            plan->ignore_errors,
            &was_check_violation
        );
        if (rc != MYLITE_OK || was_check_violation) {
            if (rc == MYLITE_OK && plan->ignore_errors) {
                rc = reset_insert_statement_after_constraint(statement);
                *out_row_complete = rc == MYLITE_OK;
            }
            return rc;
        }
        rc = handle_generated_not_null_violation(
            database,
            sqlite_step_rc,
            plan->columns,
            plan->column_count,
            &was_generated_not_null_violation
        );
        if (rc != MYLITE_OK || was_generated_not_null_violation) {
            return rc;
        }
    }
    if (plan->duplicate_update.has_clause) {
        rc = handle_insert_duplicate_key_update(
            database,
            sqlite_step_rc,
            statement,
            plan,
            row_index,
            counters
        );
        *out_row_complete = rc == MYLITE_OK;
        if (rc == MYLITE_OK && plan->has_auto_increment) {
            rc = advance_auto_increment_after_duplicate_attempt(
                database,
                plan,
                row_index,
                &counters->auto_increment_next_after_rows
            );
        }
        return rc;
    }
    if (plan->replace_existing_rows) {
        return handle_replace_insert_constraint(
            database,
            sqlite_step_rc,
            statement,
            plan,
            row_index,
            replace_attempt_count,
            counters
        );
    }

    rc = handle_insert_unique_key_conflict(database, sqlite_step_rc, statement, plan, row_index);
    if (rc == MYLITE_OK && plan->has_auto_increment && plan->ignore_errors) {
        rc = advance_auto_increment_after_ignored_duplicate(
            database,
            plan,
            row_index,
            &counters->auto_increment_next_after_rows
        );
    }
    *out_row_complete = rc == MYLITE_OK;
    return rc;
}

static int handle_replace_insert_constraint(
    struct mylite_db *database,
    int sqlite_step_rc,
    sqlite3_stmt *statement,
    const struct planned_insert *plan,
    size_t row_index,
    size_t *replace_attempt_count,
    struct insert_execution_counters *counters
) {
    int rc = MYLITE_OK;

    if (*replace_attempt_count >= plan->index_count) {
        rc = reset_insert_statement_after_constraint(statement);
        if (rc == MYLITE_OK) {
            set_runtime_error(database, "REPLACE conflict retry limit exceeded");
            rc = MYLITE_ERROR;
        }
        return rc;
    }

    ++(*replace_attempt_count);
    return handle_replace_unique_key_conflict(
        database,
        sqlite_step_rc,
        statement,
        plan,
        row_index,
        counters
    );
}

static int validate_insert_ignore_row_foreign_keys(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    bool *out_skip_row
) {
    int rc = MYLITE_OK;

    *out_skip_row = false;
    if (!foreign_key_checks_system_variable_value(database, false)) {
        return MYLITE_OK;
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < plan->foreign_key_count; ++index) {
        bool violates = false;

        rc = validate_insert_ignore_row_foreign_key(
            database,
            plan,
            &plan->foreign_keys[index],
            row_index,
            &violates
        );
        if (rc == MYLITE_OK && violates) {
            rc = append_no_referenced_row_warning(database);
            *out_skip_row = rc == MYLITE_OK;
            break;
        }
    }

    return rc;
}

static int validate_insert_ignore_row_foreign_key(
    struct mylite_db *database,
    const struct planned_insert *plan,
    const struct loaded_foreign_key_info *foreign_key,
    size_t row_index,
    bool *out_violates
) {
    struct loaded_index_part *parent_parts = NULL;
    struct planned_value *parent_values = NULL;
    struct loaded_index_info parent_index = {0};
    bool parent_exists = false;
    int rc = MYLITE_OK;

    *out_violates = false;
    if (foreign_key->part_count == 0U || row_index >= plan->row_count) {
        set_runtime_error(database, "invalid foreign-key insert descriptor");
        return MYLITE_ERROR;
    }
    if (foreign_key->part_count > SIZE_MAX / sizeof(*parent_parts) ||
        foreign_key->part_count > SIZE_MAX / sizeof(*parent_values)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    parent_parts = calloc(foreign_key->part_count, sizeof(*parent_parts));
    parent_values = calloc(foreign_key->part_count, sizeof(*parent_values));
    if (parent_parts == NULL || parent_values == NULL) {
        free(parent_parts);
        free(parent_values);
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < foreign_key->part_count;
         ++part_index) {
        const struct loaded_foreign_key_part *foreign_key_part = &foreign_key->parts[part_index];
        const struct planned_value *child_value = NULL;
        size_t child_column_index = 0U;

        rc = mylite_execution_find_column_index_by_id(
            plan->columns,
            plan->column_count,
            &child_column_index,
            foreign_key_part->foreign_key_column.child_column_id
        );
        if (rc != MYLITE_OK) {
            set_runtime_error(database, "foreign-key child column descriptor is stale");
            rc = MYLITE_ERROR;
            break;
        }

        child_value = &plan->rows[row_index].values[child_column_index];
        if (child_value->is_null) {
            free(parent_parts);
            free(parent_values);
            return MYLITE_OK;
        }
        parent_parts[part_index] = (struct loaded_index_part){
            .index_column =
                {
                    .column_id = foreign_key_part->foreign_key_column.parent_column_id,
                    .ordinal_position = (int64_t)part_index + 1,
                    .has_prefix_length = false,
                    .prefix_length = 0,
                    .sort_direction = MYLITE_CATALOG_INDEX_SORT_DIRECTION_ASC,
                },
            .column = foreign_key_part->parent_column,
            .column_index = part_index,
        };
        parent_values[part_index] = *child_value;
    }
    parent_index = (struct loaded_index_info){
        .index = foreign_key->parent_index,
        .parts = parent_parts,
        .part_count = foreign_key->part_count,
    };

    if (rc == MYLITE_OK) {
        rc = unique_key_tuple_exists(
            database,
            foreign_key->parent_table.physical_name,
            &parent_index,
            parent_values,
            foreign_key->part_count,
            &parent_exists
        );
    }
    free(parent_parts);
    free(parent_values);
    if (rc == MYLITE_OK && !parent_exists) {
        *out_violates = true;
    }
    return rc;
}

static int execute_insert_plan_row(
    sqlite3_stmt *statement,
    const struct planned_insert *plan,
    size_t row_index,
    int *out_sqlite_step_rc
) {
    int rc = bind_insert_row(statement, plan, row_index);

    if (rc == MYLITE_OK) {
        rc = step_insert_row(statement, out_sqlite_step_rc);
    }

    return rc;
}

static int advance_auto_increment_after_insert_row(
    const struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    int64_t *auto_increment_next_after_rows
) {
    const struct planned_value *auto_value =
        &plan->rows[row_index].values[plan->auto_increment_column_index];

    if (auto_value->is_null || auto_value->is_text || auto_value->integer <= 0) {
        return MYLITE_OK;
    }

    if (plan->rows[row_index].generated_auto_increment) {
        return next_auto_increment_after_generated_value(
            database,
            auto_value->integer,
            &plan->auto_increment_range,
            auto_increment_next_after_rows
        );
    }

    return next_auto_increment_after_explicit_insert_value(
        *auto_increment_next_after_rows,
        auto_value->integer,
        &plan->auto_increment_range,
        auto_increment_next_after_rows
    );
}

static int advance_auto_increment_after_duplicate_attempt(
    const struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    int64_t *auto_increment_next_after_rows
) {
    if (row_index >= plan->row_count || !plan->rows[row_index].generated_auto_increment) {
        return MYLITE_OK;
    }

    return advance_auto_increment_after_insert_row(
        database,
        plan,
        row_index,
        auto_increment_next_after_rows
    );
}

static int advance_auto_increment_after_ignored_duplicate(
    const struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    int64_t *auto_increment_next_after_rows
) {
    if (row_index >= plan->row_count || plan->rows[row_index].generated_auto_increment) {
        return MYLITE_OK;
    }

    return advance_auto_increment_after_insert_row(
        database,
        plan,
        row_index,
        auto_increment_next_after_rows
    );
}

static void record_inserted_explicit_auto_increment(
    const struct planned_insert *plan,
    size_t row_index,
    struct insert_execution_counters *counters
) {
    const struct planned_value *auto_value = NULL;

    if (!plan->has_auto_increment || row_index >= plan->row_count ||
        plan->rows[row_index].generated_auto_increment) {
        return;
    }

    auto_value = &plan->rows[row_index].values[plan->auto_increment_column_index];
    if (auto_value->is_null || auto_value->is_text || auto_value->integer <= 0) {
        return;
    }

    counters->last_inserted_explicit_auto_increment = auto_value->integer;
    counters->inserted_explicit_auto_increment = true;
}

static void record_inserted_generated_auto_increment(
    const struct planned_insert *plan,
    size_t row_index,
    struct insert_execution_counters *counters
) {
    const struct planned_value *auto_value = NULL;

    if (!plan->has_auto_increment || row_index >= plan->row_count ||
        !plan->rows[row_index].generated_auto_increment ||
        counters->inserted_generated_auto_increment) {
        return;
    }

    auto_value = &plan->rows[row_index].values[plan->auto_increment_column_index];
    counters->first_inserted_generated_auto_increment = auto_value->integer;
    counters->inserted_generated_auto_increment = true;
}

static int handle_insert_unique_key_conflict(
    struct mylite_db *database,
    int sqlite_step_rc,
    sqlite3_stmt *statement,
    const struct planned_insert *plan,
    size_t row_index
) {
    char value_text[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY] = "";
    const struct loaded_index_info *conflicting_index = NULL;
    int rc = find_insert_unique_key_conflict(
        database,
        sqlite_step_rc,
        plan,
        row_index,
        &conflicting_index,
        value_text,
        sizeof(value_text)
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (conflicting_index == NULL) {
        return mylite_sqlite_status_to_mylite(sqlite_step_rc);
    }
    if (!plan->ignore_errors) {
        set_duplicate_key_error(
            database,
            plan->target.table_name,
            conflicting_index->index.name,
            value_text
        );
        return MYLITE_ERROR;
    }

    rc = append_duplicate_key_warning(
        database,
        plan->target.table_name,
        conflicting_index->index.name,
        value_text
    );
    if (rc == MYLITE_OK) {
        int reset_rc = sqlite3_reset(statement);

        if (reset_rc != SQLITE_OK && !sqlite_status_is_constraint(reset_rc)) {
            rc = mylite_sqlite_status_to_mylite(reset_rc);
        }
    }

    return rc;
}

static int handle_insert_duplicate_key_update(
    struct mylite_db *database,
    int sqlite_step_rc,
    sqlite3_stmt *insert_statement,
    const struct planned_insert *plan,
    size_t row_index,
    struct insert_execution_counters *counters
) {
    char value_text[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY] = "";
    const struct loaded_index_info *conflicting_index = NULL;
    int rc = find_insert_unique_key_conflict(
        database,
        sqlite_step_rc,
        plan,
        row_index,
        &conflicting_index,
        value_text,
        sizeof(value_text)
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (conflicting_index == NULL) {
        return mylite_sqlite_status_to_mylite(sqlite_step_rc);
    }

    rc = reset_insert_statement_after_constraint(insert_statement);
    if (rc == MYLITE_OK) {
        rc = apply_insert_duplicate_key_update(
            database,
            plan,
            row_index,
            conflicting_index,
            counters
        );
    }

    return rc;
}

static int handle_replace_unique_key_conflict(
    struct mylite_db *database,
    int sqlite_step_rc,
    sqlite3_stmt *insert_statement,
    const struct planned_insert *plan,
    size_t row_index,
    struct insert_execution_counters *counters
) {
    char value_text[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY] = "";
    sqlite3_int64 deleted_rows = 0;
    const struct loaded_index_info *conflicting_index = NULL;
    bool exact_replacement = false;
    int rc = find_insert_unique_key_conflict(
        database,
        sqlite_step_rc,
        plan,
        row_index,
        &conflicting_index,
        value_text,
        sizeof(value_text)
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (conflicting_index == NULL) {
        return mylite_sqlite_status_to_mylite(sqlite_step_rc);
    }

    rc = reset_insert_statement_after_constraint(insert_statement);
    if (rc == MYLITE_OK) {
        rc = replace_conflicting_row_matches_plan(
            database,
            plan,
            row_index,
            conflicting_index,
            &exact_replacement
        );
    }
    if (rc == MYLITE_OK) {
        rc = delete_replace_conflicting_row(
            database,
            plan,
            conflicting_index,
            row_index,
            &deleted_rows
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_parent_foreign_keys_after_write(database, plan->table.table_id);
    }
    if (rc == MYLITE_OK && !exact_replacement) {
        counters->affected_rows += (int64_t)deleted_rows;
    }

    return rc;
}

static int replace_conflicting_row_matches_plan(
    struct mylite_db *database,
    const struct planned_insert *plan,
    size_t row_index,
    const struct loaded_index_info *conflicting_index,
    bool *out_matches
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = MYLITE_OK;

    *out_matches = false;
    rc = build_replace_conflicting_row_select_sql(plan, conflicting_index, &sql);
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_unique_key_tuple_parameters(
            statement,
            conflicting_index,
            plan->rows[row_index].values,
            plan->column_count
        );
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            rc = compare_replace_conflicting_row(statement, plan, row_index, out_matches);
        } else if (sqlite_rc == SQLITE_DONE) {
            set_runtime_error(database, "REPLACE conflicting row disappeared before delete");
            rc = MYLITE_ERROR;
        } else {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);

    return rc;
}

static int build_replace_conflicting_row_select_sql(
    const struct planned_insert *plan,
    const struct loaded_index_info *conflicting_index,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    if (conflicting_index == NULL || conflicting_index->part_count == 0U) {
        mylite_dynamic_string_deinit(&string);
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(&string, "SELECT ");
    if (rc == MYLITE_OK) {
        rc = append_insert_column_names(&string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " WHERE ");
    }
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < conflicting_index->part_count;
         ++part_index) {
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(&string, " AND ");
        }
        if (rc == MYLITE_OK) {
            rc = append_loaded_key_part_sql(&string, &conflicting_index->parts[part_index], NULL);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(&string, " = ");
        }
        if (rc == MYLITE_OK) {
            rc = append_loaded_key_part_parameter_sql(
                &string,
                &conflicting_index->parts[part_index],
                part_index + 1U
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " LIMIT 1");
    }
    if (rc == MYLITE_OK) {
        *out_sql = mylite_dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    mylite_dynamic_string_deinit(&string);
    return rc;
}

static int compare_replace_conflicting_row(
    sqlite3_stmt *statement,
    const struct planned_insert *plan,
    size_t row_index,
    bool *out_matches
) {
    int statement_column_index = 0;

    *out_matches = false;
    if (row_index >= plan->row_count) {
        return MYLITE_ERROR;
    }
    for (size_t column_index = 0U; column_index < plan->column_count; ++column_index) {
        if (plan->columns[column_index].is_generated) {
            continue;
        }
        if (statement_column_index >= INT_MAX) {
            return MYLITE_ERROR;
        }
        if (!sqlite_column_matches_planned_value(
                statement,
                statement_column_index,
                &plan->rows[row_index].values[column_index]
            )) {
            return MYLITE_OK;
        }
        ++statement_column_index;
    }

    *out_matches = true;
    return MYLITE_OK;
}

static bool sqlite_column_matches_planned_value(
    sqlite3_stmt *statement,
    int column_index,
    const struct planned_value *value
) {
    int sqlite_type = sqlite3_column_type(statement, column_index);

    if (value == NULL) {
        return false;
    }
    if (value->is_null) {
        return sqlite_type == SQLITE_NULL;
    }
    if (sqlite_type == SQLITE_NULL) {
        return false;
    }
    if (value->is_blob) {
        int byte_count = sqlite3_column_bytes(statement, column_index);
        const void *bytes = sqlite3_column_blob(statement, column_index);

        if (byte_count < 0 || (size_t)byte_count != value->text_length) {
            return false;
        }
        if (value->text_length != 0U && (bytes == NULL || value->text == NULL)) {
            return false;
        }
        if (value->text_length == 0U) {
            return true;
        }
        return memcmp(bytes, value->text, value->text_length) == 0;
    }
    if (value->is_text) {
        int byte_count = sqlite3_column_bytes(statement, column_index);
        const unsigned char *text = sqlite3_column_text(statement, column_index);

        if (byte_count < 0 || (size_t)byte_count != value->text_length) {
            return false;
        }
        if (value->text_length != 0U && (text == NULL || value->text == NULL)) {
            return false;
        }
        if (value->text_length == 0U) {
            return true;
        }
        return memcmp(text, value->text, value->text_length) == 0;
    }
    if (value->is_real) {
        return sqlite3_column_double(statement, column_index) == value->real;
    }

    return sqlite3_column_int64(statement, column_index) == (sqlite3_int64)value->integer;
}

static int delete_replace_conflicting_row(
    struct mylite_db *database,
    const struct planned_insert *plan,
    const struct loaded_index_info *conflicting_index,
    size_t row_index,
    sqlite3_int64 *out_deleted_rows
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = MYLITE_OK;

    *out_deleted_rows = 0;
    rc = build_replace_conflicting_row_delete_sql(plan, conflicting_index, &sql);
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_unique_key_tuple_parameters(
            statement,
            conflicting_index,
            plan->rows[row_index].values,
            plan->column_count
        );
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            *out_deleted_rows = sqlite3_changes64(database->sqlite);
            if (*out_deleted_rows <= 0) {
                set_runtime_error(database, "REPLACE failed to delete conflicting row");
                rc = MYLITE_ERROR;
            }
        } else {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);

    return rc;
}

static int build_replace_conflicting_row_delete_sql(
    const struct planned_insert *plan,
    const struct loaded_index_info *conflicting_index,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    if (conflicting_index == NULL || conflicting_index->part_count == 0U) {
        mylite_dynamic_string_deinit(&string);
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(&string, "DELETE FROM ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " WHERE ");
    }
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < conflicting_index->part_count;
         ++part_index) {
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(&string, " AND ");
        }
        if (rc == MYLITE_OK) {
            rc = append_loaded_key_part_sql(&string, &conflicting_index->parts[part_index], NULL);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(&string, " = ");
        }
        if (rc == MYLITE_OK) {
            rc = append_loaded_key_part_parameter_sql(
                &string,
                &conflicting_index->parts[part_index],
                part_index + 1U
            );
        }
    }
    if (rc == MYLITE_OK) {
        *out_sql = mylite_dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    mylite_dynamic_string_deinit(&string);
    return rc;
}
