static int append_information_schema_tables_base_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct table_status_values status = {0};
    int rc = load_table_status_values(database, table, &status);

    if (rc != MYLITE_OK) {
        return rc;
    }

    const char *values[information_schema_tables_column_count] = {
        "def",
        schema->name,
        table->name,
        "BASE TABLE",
        "InnoDB",
        "10",
        status.row_format,
        status.row_count_text,
        status.average_row_length_text,
        "16384",
        "0",
        status.index_length,
        "0",
        status.auto_increment,
        status.create_time,
        status.update_time,
        NULL,
        table->default_collation,
        NULL,
        status.create_options,
        table->comment,
    };

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_tables_view_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct table_status_values status = {0};
    int rc = format_table_status_timestamp(
        database,
        table->created_time_utc_epoch,
        status.create_time_text,
        sizeof(status.create_time_text),
        &status.create_time
    );
    const char *values[information_schema_tables_column_count] = {
        "def",  schema->name, table->name, "VIEW", NULL,
        NULL,   NULL,         NULL,        NULL,   NULL,
        NULL,   NULL,         NULL,        NULL,   status.create_time,
        NULL,   NULL,         NULL,        NULL,   NULL,
        "VIEW",
    };

    if (rc != MYLITE_OK) {
        return rc;
    }
    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_partitions_base_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct table_status_values status = {0};
    int rc = load_table_status_values(database, table, &status);
    const char *values[information_schema_partitions_column_count] = {
        "def",
        schema->name,
        table->name,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        status.row_count_text,
        status.average_row_length_text,
        "16384",
        "0",
        status.index_length,
        "0",
        status.create_time,
        status.update_time,
        NULL,
        NULL,
        "",
        "",
        NULL,
    };

    if (rc != MYLITE_OK) {
        return rc;
    }

    return append_information_schema_row(database, rows, values);
}

static int load_table_status_values(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    struct table_status_values *out_values
) {
    int64_t row_count = 0;
    int64_t average_row_length = 0;
    int64_t index_length = 0;
    int rc = MYLITE_OK;

    if (out_values == NULL) {
        return MYLITE_MISUSE;
    }
    *out_values = (struct table_status_values){0};

    rc = read_show_table_status_row_count(database, table, &row_count);
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
        return rc;
    }
    if (rc != MYLITE_OK) {
        set_runtime_error(database, "failed to read table status row count");
        return MYLITE_ERROR;
    }
    out_values->row_count = row_count;
    if (row_count > 0) {
        average_row_length = show_table_status_data_length / row_count;
    }
    rc = format_show_table_status_integer(
        database,
        row_count,
        out_values->row_count_text,
        sizeof(out_values->row_count_text)
    );
    if (rc == MYLITE_OK) {
        rc = format_show_table_status_integer(
            database,
            average_row_length,
            out_values->average_row_length_text,
            sizeof(out_values->average_row_length_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = table_status_index_length(database, table, &index_length);
    }
    if (rc == MYLITE_OK) {
        rc = format_show_table_status_integer(
            database,
            index_length,
            out_values->index_length_text,
            sizeof(out_values->index_length_text)
        );
    }
    if (rc == MYLITE_OK) {
        out_values->index_length = out_values->index_length_text;
    }
    if (rc == MYLITE_OK) {
        rc = table_status_auto_increment_display_value(
            database,
            table,
            out_values->auto_increment_text,
            sizeof(out_values->auto_increment_text),
            &out_values->auto_increment
        );
    }
    if (rc == MYLITE_OK) {
        rc = format_table_status_timestamp(
            database,
            table->created_time_utc_epoch,
            out_values->create_time_text,
            sizeof(out_values->create_time_text),
            &out_values->create_time
        );
    }
    if (rc == MYLITE_OK) {
        rc = format_table_status_timestamp(
            database,
            table->updated_time_utc_epoch,
            out_values->update_time_text,
            sizeof(out_values->update_time_text),
            &out_values->update_time
        );
    }
    if (rc == MYLITE_OK) {
        rc = load_table_status_row_format(database, table, out_values);
    }
    if (rc == MYLITE_OK) {
        rc = load_table_status_create_options(database, table, out_values);
    }

    return rc;
}

static int load_table_status_row_format(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    struct table_status_values *out_values
) {
    const char *row_format = "Dynamic";

    if (table == NULL || out_values == NULL) {
        return MYLITE_MISUSE;
    }
    if (table_status_row_format_is_compressed(table)) {
        row_format = "Compressed";
    } else if (text_equals_ascii_case_insensitive(table->row_format_option, "COMPACT")) {
        row_format = "Compact";
    } else if (text_equals_ascii_case_insensitive(table->row_format_option, "REDUNDANT")) {
        row_format = "Redundant";
    } else if (
        table->row_format_option[0] != '\0' &&
        !text_equals_ascii_case_insensitive(table->row_format_option, "DYNAMIC")
    ) {
        set_runtime_error(database, "invalid table row format descriptor");
        return MYLITE_ERROR;
    }
    snprintf(out_values->row_format_text, sizeof(out_values->row_format_text), "%s", row_format);
    out_values->row_format = out_values->row_format_text;
    return MYLITE_OK;
}

static bool table_status_row_format_is_compressed(
    const struct mylite_catalog_table_descriptor *table
) {
    if (table == NULL) {
        return false;
    }
    if (text_equals_ascii_case_insensitive(table->row_format_option, "COMPRESSED")) {
        return true;
    }
    if (table->row_format_option[0] != '\0') {
        return false;
    }
    if (table->key_block_size > 0) {
        return true;
    }
    return false;
}

static int load_table_status_create_options(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    struct table_status_values *out_values
) {
    bool has_option = false;
    int rc = MYLITE_OK;

    if (table == NULL || out_values == NULL) {
        return MYLITE_MISUSE;
    }
    out_values->create_options_text[0] = '\0';
    out_values->create_options = out_values->create_options_text;

    if (table->max_rows > 0) {
        rc = append_table_status_create_option_i64(
            database,
            out_values->create_options_text,
            sizeof(out_values->create_options_text),
            &has_option,
            "max_rows=",
            table->max_rows
        );
    }
    if (rc == MYLITE_OK && table->min_rows > 0) {
        rc = append_table_status_create_option_i64(
            database,
            out_values->create_options_text,
            sizeof(out_values->create_options_text),
            &has_option,
            "min_rows=",
            table->min_rows
        );
    }
    if (rc == MYLITE_OK && table->avg_row_length > 0) {
        rc = append_table_status_create_option_i64(
            database,
            out_values->create_options_text,
            sizeof(out_values->create_options_text),
            &has_option,
            "avg_row_length=",
            table->avg_row_length
        );
    }
    if (rc == MYLITE_OK && table->row_format_option[0] != '\0') {
        char text[sizeof("row_format=") + MYLITE_CATALOG_IDENTIFIER_CAPACITY];
        int written = snprintf(text, sizeof(text), "row_format=%s", table->row_format_option);

        if (written < 0 || (size_t)written >= sizeof(text)) {
            set_runtime_error(database, "failed to format table create option");
            return MYLITE_ERROR;
        }
        rc = append_table_status_create_option(
            database,
            out_values->create_options_text,
            sizeof(out_values->create_options_text),
            &has_option,
            text
        );
    }
    if (rc == MYLITE_OK && table->stats_sample_pages > 0) {
        rc = append_table_status_create_option_i64(
            database,
            out_values->create_options_text,
            sizeof(out_values->create_options_text),
            &has_option,
            "stats_sample_pages=",
            table->stats_sample_pages
        );
    }
    if (rc == MYLITE_OK && table->stats_auto_recalc != -1) {
        rc = append_table_status_create_option_i64(
            database,
            out_values->create_options_text,
            sizeof(out_values->create_options_text),
            &has_option,
            "stats_auto_recalc=",
            table->stats_auto_recalc
        );
    }
    if (rc == MYLITE_OK && table->key_block_size > 0) {
        rc = append_table_status_create_option_i64(
            database,
            out_values->create_options_text,
            sizeof(out_values->create_options_text),
            &has_option,
            "KEY_BLOCK_SIZE=",
            table->key_block_size
        );
    }
    if (rc == MYLITE_OK && table->stats_persistent != -1) {
        rc = append_table_status_create_option_i64(
            database,
            out_values->create_options_text,
            sizeof(out_values->create_options_text),
            &has_option,
            "stats_persistent=",
            table->stats_persistent
        );
    }
    if (rc == MYLITE_OK && table->pack_keys != -1) {
        rc = append_table_status_create_option_i64(
            database,
            out_values->create_options_text,
            sizeof(out_values->create_options_text),
            &has_option,
            "pack_keys=",
            table->pack_keys
        );
    }
    if (rc == MYLITE_OK && table->checksum != 0) {
        rc = append_table_status_create_option(
            database,
            out_values->create_options_text,
            sizeof(out_values->create_options_text),
            &has_option,
            "checksum=1"
        );
    }
    if (rc == MYLITE_OK && table->delay_key_write != -1) {
        rc = append_table_status_create_option_i64(
            database,
            out_values->create_options_text,
            sizeof(out_values->create_options_text),
            &has_option,
            "delay_key_write=",
            table->delay_key_write
        );
    }
    return rc;
}

static int append_table_status_create_option(
    struct mylite_db *database,
    char *buffer,
    size_t buffer_size,
    bool *has_option,
    const char *text
) {
    size_t current_length = 0U;
    size_t separator_length = 0U;
    size_t text_length = 0U;

    if (buffer == NULL || buffer_size == 0U || has_option == NULL || text == NULL) {
        return MYLITE_MISUSE;
    }
    current_length = strlen(buffer);
    text_length = strlen(text);
    if (*has_option) {
        separator_length = 1U;
    }
    if (current_length + separator_length + text_length >= buffer_size) {
        set_runtime_error(database, "failed to format table create options");
        return MYLITE_ERROR;
    }
    if (*has_option) {
        buffer[current_length] = ' ';
        ++current_length;
    }
    memcpy(buffer + current_length, text, text_length + 1U);
    *has_option = true;
    return MYLITE_OK;
}

static int append_table_status_create_option_i64(
    struct mylite_db *database,
    char *buffer,
    size_t buffer_size,
    bool *has_option,
    const char *prefix,
    int64_t value
) {
    char text[integer_text_capacity + sizeof("stats_sample_pages=")];
    int written = snprintf(text, sizeof(text), "%s%" PRId64, prefix, value);

    if (written < 0 || (size_t)written >= sizeof(text)) {
        set_runtime_error(database, "failed to format table create option");
        return MYLITE_ERROR;
    }
    return append_table_status_create_option(database, buffer, buffer_size, has_option, text);
}

static int table_status_auto_increment_predicate_value(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const struct table_status_values *status,
    char *buffer,
    size_t buffer_size,
    const char **out_value
) {
    if (database == NULL || table == NULL || status == NULL || buffer == NULL ||
        out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = status->auto_increment;
    if (*out_value != NULL) {
        return MYLITE_OK;
    }

    return table_status_auto_increment_next_value(database, table, buffer, buffer_size, out_value);
}

static int table_status_auto_increment_display_value(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    char *buffer,
    size_t buffer_size,
    const char **out_value
) {
    int rc = MYLITE_OK;

    if (database == NULL || table == NULL || buffer == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = NULL;

    if (table->auto_increment_status > 0) {
        rc = format_show_table_status_integer(
            database,
            table->auto_increment_status,
            buffer,
            buffer_size
        );
        if (rc == MYLITE_OK) {
            *out_value = buffer;
        }
    } else {
        rc =
            table_status_auto_increment_next_value(database, table, buffer, buffer_size, out_value);
    }
    return rc;
}

static int table_status_auto_increment_next_value(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    char *buffer,
    size_t buffer_size,
    const char **out_value
) {
    bool has_auto_increment = false;
    int rc = MYLITE_OK;

    if (database == NULL || table == NULL || buffer == NULL || out_value == NULL) {
        return MYLITE_MISUSE;
    }
    *out_value = NULL;

    rc = table_status_has_auto_increment(database, table, &has_auto_increment);
    if (rc == MYLITE_OK && has_auto_increment) {
        rc = format_show_table_status_integer(
            database,
            table->auto_increment_next,
            buffer,
            buffer_size
        );
        if (rc == MYLITE_OK) {
            *out_value = buffer;
        }
    }
    return rc;
}

static int table_status_has_auto_increment(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    bool *out_has_auto_increment
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_table_columns_cache_entry *columns_cache_entry = NULL;
    size_t column_count = 0U;
    int rc = MYLITE_OK;

    if (out_has_auto_increment == NULL) {
        return MYLITE_MISUSE;
    }
    *out_has_auto_increment = false;

    rc = mylite_execution_acquire_table_columns(
        database,
        table->table_id,
        &columns,
        &column_count,
        &columns_cache_entry
    );
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < column_count; ++column_index) {
        if (column_descriptor_is_auto_increment(&columns[column_index])) {
            *out_has_auto_increment = true;
            break;
        }
    }
    mylite_execution_release_table_columns(columns_cache_entry, columns);
    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
            return rc;
        }
        set_runtime_error(database, "failed to read table status columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int table_status_index_length(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    int64_t *out_index_length
) {
    struct nonprimary_index_presence_context context = {.has_nonprimary_index = false};
    int rc = MYLITE_OK;

    if (out_index_length == NULL) {
        return MYLITE_MISUSE;
    }
    *out_index_length = 0;
    rc = mylite_catalog_for_each_index_in_table(
        database,
        table->table_id,
        note_nonprimary_index_presence,
        &context
    );
    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
            return rc;
        }
        set_runtime_error(database, "failed to read table status index descriptors");
        return MYLITE_ERROR;
    }
    if (context.has_nonprimary_index) {
        *out_index_length = show_table_status_data_length;
    }
    return MYLITE_OK;
}

static int note_nonprimary_index_presence(
    const struct mylite_catalog_index_descriptor *index,
    void *user_data
) {
    struct nonprimary_index_presence_context *context = user_data;

    if (context == NULL || index == NULL) {
        return MYLITE_MISUSE;
    }
    if (index->kind != MYLITE_CATALOG_INDEX_KIND_PRIMARY) {
        context->has_nonprimary_index = true;
    }

    return MYLITE_OK;
}

static int format_table_status_timestamp(
    struct mylite_db *database,
    int64_t epoch,
    char *buffer,
    size_t buffer_size,
    const char **out_timestamp
) {
    if (out_timestamp == NULL) {
        return MYLITE_MISUSE;
    }
    *out_timestamp = NULL;
    if (epoch <= 0) {
        return MYLITE_OK;
    }
    int rc = format_session_timestamp_epoch_text(database, epoch, buffer, buffer_size);

    if (rc == MYLITE_OK) {
        *out_timestamp = buffer;
    }
    return rc;
}
