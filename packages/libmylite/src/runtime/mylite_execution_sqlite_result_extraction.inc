static int append_selected_sqlite_row(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    mylite_result *result,
    const struct mylite_catalog_column_descriptor *columns,
    size_t descriptor_count,
    struct sqlite_result_row_storage *storage
) {
    size_t column_count = mylite_result_column_count(result);
    int rc = MYLITE_OK;

    rc = read_selected_sqlite_row(
        database,
        statement,
        column_count,
        columns,
        descriptor_count,
        storage
    );
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_bytes_row(result, storage->values);
    }

    return rc;
}

static int read_selected_sqlite_row(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    size_t column_count,
    const struct mylite_catalog_column_descriptor *columns,
    size_t descriptor_count,
    struct sqlite_result_row_storage *storage
) {
    int rc = MYLITE_OK;

    if (storage == NULL) {
        return MYLITE_MISUSE;
    }
    rc = sqlite_result_row_storage_reserve(storage, column_count);
    if (rc != MYLITE_OK) {
        return rc;
    }

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < column_count; ++column_index) {
        char *text = &storage->texts[column_index * storage->text_capacity];

        rc = append_selected_sqlite_row_value(
            database,
            statement,
            column_index,
            columns,
            descriptor_count,
            text,
            storage->text_capacity,
            &storage->values[column_index]
        );
    }
    return rc;
}

static void sqlite_result_row_storage_deinit(struct sqlite_result_row_storage *storage) {
    if (storage == NULL) {
        return;
    }

    free(storage->values);
    free(storage->texts);
    *storage = (struct sqlite_result_row_storage){0};
}

static int sqlite_result_row_storage_reserve(
    struct sqlite_result_row_storage *storage,
    size_t column_count
) {
    struct mylite_result_cell *values = NULL;
    char *texts = NULL;
    size_t text_capacity = approximate_numeric_text_capacity;

    if (storage == NULL) {
        return MYLITE_MISUSE;
    }
    if (column_count > (size_t)INT_MAX || column_count > SIZE_MAX / text_capacity) {
        return MYLITE_NOMEM;
    }
    if (column_count <= storage->column_capacity) {
        return MYLITE_OK;
    }
    if (column_count > SIZE_MAX / sizeof(*values)) {
        return MYLITE_NOMEM;
    }
    values = realloc(storage->values, column_count * sizeof(*values));
    if (values == NULL && column_count != 0U) {
        return MYLITE_NOMEM;
    }
    storage->values = values;
    texts = realloc(storage->texts, column_count * text_capacity);
    if (texts == NULL && column_count != 0U) {
        return MYLITE_NOMEM;
    }
    storage->texts = texts;
    storage->column_capacity = column_count;
    storage->text_capacity = text_capacity;
    return MYLITE_OK;
}

static int append_selected_sqlite_row_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    size_t column_index,
    const struct mylite_catalog_column_descriptor *columns,
    size_t descriptor_count,
    char *text,
    size_t text_capacity,
    struct mylite_result_cell *out_value
) {
    const struct mylite_catalog_column_descriptor *column =
        columns != NULL && column_index < descriptor_count ? &columns[column_index] : NULL;

    return append_selected_sqlite_row_value_with_descriptor(
        database,
        statement,
        column_index,
        column,
        text,
        text_capacity,
        out_value
    );
}

static int append_selected_sqlite_row_value_with_descriptor(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    size_t column_index,
    const struct mylite_catalog_column_descriptor *column,
    char *text,
    size_t text_capacity,
    struct mylite_result_cell *out_value
) {
    int sqlite_type = SQLITE_NULL;

    if (column_index > (size_t)INT_MAX || out_value == NULL) {
        return MYLITE_ERROR;
    }
    *out_value = (struct mylite_result_cell){.bytes = NULL, .size = 0U, .is_null = true};

    sqlite_type = sqlite3_column_type(statement, (int)column_index);
    if (sqlite_type == SQLITE_NULL) {
        return MYLITE_OK;
    }
    if (sqlite_type == SQLITE_INTEGER) {
        return append_selected_sqlite_integer_value(
            statement,
            column_index,
            text,
            text_capacity,
            out_value
        );
    }
    if (sqlite_type == SQLITE_FLOAT) {
        return append_selected_sqlite_float_value(
            database,
            statement,
            column_index,
            column,
            text,
            text_capacity,
            out_value
        );
    }
    if (sqlite_type == SQLITE_TEXT) {
        return append_selected_sqlite_text_value(statement, column_index, out_value);
    }
    if (sqlite_type == SQLITE_BLOB) {
        return append_selected_sqlite_blob_value(statement, column_index, out_value);
    }

    return MYLITE_ERROR;
}

static int append_selected_sqlite_integer_value(
    sqlite3_stmt *statement,
    size_t column_index,
    char *text,
    size_t text_capacity,
    struct mylite_result_cell *out_value
) {
    int written = snprintf(
        text,
        text_capacity,
        "%" PRId64,
        (int64_t)sqlite3_column_int64(statement, (int)column_index)
    );

    if (written < 0 || (size_t)written >= text_capacity) {
        return MYLITE_ERROR;
    }
    *out_value = (struct mylite_result_cell){
        .bytes = text,
        .size = (size_t)written,
        .is_null = false,
    };
    return MYLITE_OK;
}

static int append_selected_sqlite_float_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    size_t column_index,
    const struct mylite_catalog_column_descriptor *column,
    char *text,
    size_t text_capacity,
    struct mylite_result_cell *out_value
) {
    double value = sqlite3_column_double(statement, (int)column_index);
    struct approximate_type_info info = {
        .type_class = APPROXIMATE_TYPE_DOUBLE,
        .is_unsigned = false,
    };
    int rc = MYLITE_OK;

    if (column != NULL && column_descriptor_is_approximate(column)) {
        rc = approximate_type_info_for_logical_type(column->logical_type, &info);
    }
    if (rc == MYLITE_OK) {
        rc = format_approximate_value_text(
            database,
            value,
            &info,
            "approximate result",
            text,
            text_capacity
        );
    }
    if (rc == MYLITE_OK) {
        *out_value = (struct mylite_result_cell){
            .bytes = text,
            .size = strlen(text),
            .is_null = false,
        };
    }

    return rc;
}

static int append_selected_sqlite_text_value(
    sqlite3_stmt *statement,
    size_t column_index,
    struct mylite_result_cell *out_value
) {
    const unsigned char *sqlite_text = sqlite3_column_text(statement, (int)column_index);
    int byte_count = sqlite3_column_bytes(statement, (int)column_index);
    size_t character_count = 0U;

    if (sqlite_text == NULL || byte_count < 0 ||
        validate_utf8_text((const char *)sqlite_text, (size_t)byte_count, &character_count) !=
            MYLITE_OK) {
        return MYLITE_ERROR;
    }

    (void)character_count;
    *out_value = (struct mylite_result_cell){
        .bytes = sqlite_text,
        .size = (size_t)byte_count,
        .is_null = false,
    };
    return MYLITE_OK;
}

static int append_selected_sqlite_blob_value(
    sqlite3_stmt *statement,
    size_t column_index,
    struct mylite_result_cell *out_value
) {
    const void *sqlite_blob = sqlite3_column_blob(statement, (int)column_index);
    int byte_count = sqlite3_column_bytes(statement, (int)column_index);

    if ((sqlite_blob == NULL && byte_count != 0) || byte_count < 0) {
        return MYLITE_ERROR;
    }

    *out_value = (struct mylite_result_cell){
        .bytes = sqlite_blob,
        .size = (size_t)byte_count,
        .is_null = false,
    };
    return MYLITE_OK;
}

static int choose_sqlite_rowid_alias(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const char *unsupported_message,
    const char **out_alias
) {
    static const char *const rowid_aliases[] = {"rowid", "_rowid_", "oid"};

    if (out_alias == NULL) {
        return MYLITE_MISUSE;
    }
    *out_alias = NULL;
    for (size_t index = 0U; index < sizeof(rowid_aliases) / sizeof(rowid_aliases[0]); ++index) {
        if (!column_name_exists(columns, column_count, rowid_aliases[index])) {
            *out_alias = rowid_aliases[index];
            return MYLITE_OK;
        }
    }

    set_unsupported_error(database, unsupported_message);
    return MYLITE_ERROR;
}

static bool column_name_exists(
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const char *name
) {
    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        if (text_equals_ascii_case_insensitive(columns[column_index].name, name)) {
            return true;
        }
    }

    return false;
}
