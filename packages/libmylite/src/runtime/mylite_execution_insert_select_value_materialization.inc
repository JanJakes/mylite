
static int materialize_insert_select_row_values(
    struct mylite_db *database,
    sqlite3_stmt *select_statement,
    const struct planned_insert_select *plan,
    size_t row_number,
    struct planned_value *values
) {
    bool adjust_value = insert_select_allows_adjustment(database, plan);
    int rc = MYLITE_OK;

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->target.column_count;
         ++column_index) {
        size_t target_position = 0U;

        if (find_insert_select_target_position(plan, column_index, &target_position)) {
            const struct mylite_catalog_column_descriptor *source_column = NULL;

            if (target_position >= (size_t)INT_MAX) {
                return MYLITE_ERROR;
            }
            rc = insert_select_source_column_for_position(
                database,
                select_statement,
                plan,
                target_position,
                &source_column
            );
            if (rc == MYLITE_OK) {
                rc = materialize_insert_select_selected_value(
                    database,
                    select_statement,
                    (int)target_position,
                    source_column,
                    &plan->target.columns[column_index],
                    row_number,
                    adjust_value,
                    &values[column_index]
                );
            }
        } else {
            rc = materialize_insert_select_omitted_value(
                database,
                plan,
                column_index,
                &values[column_index]
            );
        }
    }

    return rc;
}

static int materialize_insert_select_selected_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *source_column,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number,
    bool adjust_value,
    struct planned_value *out_value
) {
    if (sqlite3_column_type(statement, selected_column_index) == SQLITE_NULL) {
        if (column_descriptor_is_auto_increment(target_column)) {
            *out_value = (struct planned_value){.is_null = true};
            return MYLITE_OK;
        }
        if (adjust_value && !target_column->is_nullable) {
            return convert_null_insert_value(database, target_column, true, out_value);
        }
        *out_value = (struct planned_value){.is_null = true};
        return MYLITE_OK;
    }
    if (sqlite3_column_type(statement, selected_column_index) == SQLITE_INTEGER &&
        target_column->physical_type[0] != '\0' &&
        strcmp(target_column->physical_type, "INTEGER") == 0) {
        return materialize_insert_select_integer_value(
            database,
            statement,
            selected_column_index,
            target_column,
            row_number,
            adjust_value,
            out_value
        );
    }
    if (column_descriptor_is_binary_string_family(target_column)) {
        return materialize_sqlite_binary_string_value(
            database,
            statement,
            selected_column_index,
            target_column,
            row_number,
            out_value
        );
    }
    if (column_descriptor_is_string_family(target_column)) {
        return materialize_insert_select_string_value(
            database,
            statement,
            selected_column_index,
            source_column,
            target_column,
            row_number,
            adjust_value,
            out_value
        );
    }
    if (column_descriptor_is_bit(target_column)) {
        return materialize_sqlite_bit_value(
            database,
            statement,
            selected_column_index,
            target_column,
            row_number,
            out_value
        );
    }

    return materialize_insert_select_raw_value(
        database,
        statement,
        selected_column_index,
        out_value
    );
}

static int materialize_insert_select_integer_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number,
    bool adjust_value,
    struct planned_value *out_value
) {
    const uint64_t int64_negative_abs_max = 9223372036854775808ULL;
    int64_t selected_value = (int64_t)sqlite3_column_int64(statement, selected_column_index);
    bool is_negative = selected_value < 0;
    uint64_t magnitude = (uint64_t)selected_value;

    if (is_negative) {
        if (selected_value == INT64_MIN) {
            magnitude = int64_negative_abs_max;
        } else {
            magnitude = (uint64_t)(-selected_value);
        }
    }

    out_value->is_null = false;
    out_value->is_text = false;
    out_value->is_blob = false;
    out_value->is_real = false;
    return convert_integer_for_column_with_policy(
        database,
        magnitude,
        is_negative,
        target_column,
        row_number,
        adjust_value,
        &out_value->integer
    );
}

static int materialize_insert_select_string_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *source_column,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number,
    bool adjust_value,
    struct planned_value *out_value
) {
    const unsigned char *text = NULL;
    int byte_count = 0;
    char *copy = NULL;
    size_t text_length = 0U;
    struct char_text_conversion conversion = {0};
    int rc = MYLITE_OK;

    if (sqlite3_column_type(statement, selected_column_index) != SQLITE_TEXT) {
        set_unsupported_error(
            database,
            "INSERT ... SELECT does not support implicit string conversion"
        );
        return MYLITE_ERROR;
    }

    text = sqlite3_column_text(statement, selected_column_index);
    byte_count = sqlite3_column_bytes(statement, selected_column_index);
    if (text == NULL || byte_count < 0) {
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }
    text_length = (size_t)byte_count;
    copy = malloc(text_length + 1U);
    if (copy == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    memcpy(copy, text, text_length);
    copy[text_length] = '\0';

    conversion = (struct char_text_conversion){
        .text = copy,
        .text_length = text_length,
        .row_number = row_number,
    };
    if (column_descriptor_is_char(target_column)) {
        rc = convert_char_text(database, target_column, &conversion, adjust_value);
    } else if (column_descriptor_is_varchar(target_column)) {
        rc = convert_varchar_text(database, target_column, &conversion, adjust_value);
    } else {
        rc = convert_text_family_text(
            database,
            target_column,
            &conversion,
            adjust_value,
            source_column != NULL
        );
    }
    if (rc != MYLITE_OK) {
        free(copy);
        return rc;
    }

    return assign_text_value(database, copy, conversion.text_length, out_value);
}

static int materialize_sqlite_binary_string_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number,
    struct planned_value *out_value
) {
    const void *bytes = sqlite3_column_blob(statement, selected_column_index);
    int byte_count = sqlite3_column_bytes(statement, selected_column_index);
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
    rc = convert_binary_bytes_for_column(
        database,
        target_column,
        row_number,
        false,
        &copy,
        &copy_length
    );
    if (rc != MYLITE_OK) {
        free(copy);
        return rc;
    }

    return assign_blob_value(database, copy, copy_length, out_value);
}

static int materialize_sqlite_bit_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number,
    struct planned_value *out_value
) {
    const void *bytes = sqlite3_column_blob(statement, selected_column_index);
    int byte_count = sqlite3_column_bytes(statement, selected_column_index);
    int rc = validate_insert_select_bit_value(
        database,
        statement,
        selected_column_index,
        target_column,
        row_number
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    return copy_blob_value(database, bytes, (size_t)byte_count, out_value);
}

static int materialize_insert_select_raw_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    struct planned_value *out_value
) {
    int sqlite_type = sqlite3_column_type(statement, selected_column_index);

    switch (sqlite_type) {
    case SQLITE_INTEGER:
        *out_value = (struct planned_value){
            .is_null = false,
            .is_text = false,
            .integer = (int64_t)sqlite3_column_int64(statement, selected_column_index),
        };
        return MYLITE_OK;
    case SQLITE_FLOAT:
        *out_value = (struct planned_value){
            .is_null = false,
            .is_text = false,
            .is_real = true,
            .real = sqlite3_column_double(statement, selected_column_index),
        };
        return MYLITE_OK;
    case SQLITE_TEXT: {
        const unsigned char *text = sqlite3_column_text(statement, selected_column_index);
        int byte_count = sqlite3_column_bytes(statement, selected_column_index);
        char *copy = NULL;

        if (text == NULL || byte_count < 0) {
            set_physical_sqlite_row_error(database);
            return MYLITE_ERROR;
        }
        copy = malloc((size_t)byte_count + 1U);
        if (copy == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        memcpy(copy, text, (size_t)byte_count);
        copy[(size_t)byte_count] = '\0';
        return assign_text_value(database, copy, (size_t)byte_count, out_value);
    }
    case SQLITE_BLOB: {
        const void *bytes = sqlite3_column_blob(statement, selected_column_index);
        int byte_count = sqlite3_column_bytes(statement, selected_column_index);

        if ((bytes == NULL && byte_count != 0) || byte_count < 0) {
            set_physical_sqlite_row_error(database);
            return MYLITE_ERROR;
        }
        return copy_blob_value(database, bytes, (size_t)byte_count, out_value);
    }
    case SQLITE_NULL:
        *out_value = (struct planned_value){.is_null = true};
        return MYLITE_OK;
    default:
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }
}

static int materialize_insert_select_omitted_value(
    struct mylite_db *database,
    const struct planned_insert_select *plan,
    size_t column_index,
    struct planned_value *out_value
) {
    const struct mylite_catalog_column_descriptor *target_column = NULL;

    if (plan == NULL || column_index >= plan->target.column_count) {
        set_runtime_error(database, "invalid INSERT ... SELECT omitted target column");
        return MYLITE_ERROR;
    }
    target_column = &plan->target.columns[column_index];
    if (column_descriptor_is_auto_increment(target_column)) {
        *out_value = (struct planned_value){.is_null = true};
        return MYLITE_OK;
    }
    if (insert_select_allows_adjustment(database, plan)) {
        return allocate_insert_column_value(database, &plan->target, column_index, out_value);
    }

    return materialize_dml_default_value(database, target_column, false, out_value);
}
