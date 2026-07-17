static int append_information_schema_innodb_tables_base_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_table_columns_cache_entry *columns_cache_entry = NULL;
    struct table_status_values status = {0};
    char *table_name = NULL;
    char table_id_text[integer_text_capacity];
    char flag_text[integer_text_capacity];
    char column_count_text[integer_text_capacity];
    char space_text[integer_text_capacity];
    char zip_page_size_text[integer_text_capacity];
    char instant_cols_text[integer_text_capacity];
    char total_row_versions_text[integer_text_capacity];
    size_t column_count = 0U;
    int64_t flag = 0;
    int64_t innodb_column_count = 0;
    int64_t zip_page_size = 0;
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_innodb_tables_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_TABLES columns");
        return MYLITE_ERROR;
    }

    rc = mylite_execution_acquire_table_columns(
        database,
        table->table_id,
        &columns,
        &column_count,
        &columns_cache_entry
    );
    if (rc == MYLITE_OK) {
        rc = load_table_status_row_format(database, table, &status);
    }
    if (rc == MYLITE_OK) {
        rc = copy_information_schema_schema_object_name(
            database,
            schema->name,
            table->name,
            &table_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_innodb_table_flag(database, status.row_format, &flag);
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_innodb_table_column_count(
            database,
            column_count,
            &innodb_column_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_innodb_table_zip_page_size(
            database,
            table,
            status.row_format,
            &zip_page_size
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            table->table_id,
            table_id_text,
            sizeof(table_id_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(database, flag, flag_text, sizeof(flag_text));
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            innodb_column_count,
            column_count_text,
            sizeof(column_count_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            information_schema_innodb_table_space,
            space_text,
            sizeof(space_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            zip_page_size,
            zip_page_size_text,
            sizeof(zip_page_size_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            information_schema_innodb_table_instant_cols,
            instant_cols_text,
            sizeof(instant_cols_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            information_schema_innodb_table_total_row_versions,
            total_row_versions_text,
            sizeof(total_row_versions_text)
        );
    }
    if (rc == MYLITE_OK) {
        const char *values[information_schema_innodb_tables_column_count] = {
            table_id_text,
            table_name,
            flag_text,
            column_count_text,
            space_text,
            status.row_format,
            zip_page_size_text,
            "Single",
            instant_cols_text,
            total_row_versions_text,
        };

        rc = append_information_schema_row(database, rows, values);
    }

    free(table_name);
    mylite_execution_release_table_columns(columns_cache_entry, columns);
    return rc;
}

static int append_information_schema_innodb_tablestats_base_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    char *table_name = NULL;
    char table_id_text[integer_text_capacity];
    char row_count_text[integer_text_capacity];
    char other_index_size_text[integer_text_capacity];
    char autoinc_text[integer_text_capacity];
    int64_t row_count = 0;
    int64_t other_index_size = 0;
    int64_t autoinc = 0;
    bool has_auto_increment = false;
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_innodb_tablestats_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_TABLESTATS columns");
        return MYLITE_ERROR;
    }

    rc = copy_information_schema_schema_object_name(
        database,
        schema->name,
        table->name,
        &table_name
    );
    if (rc == MYLITE_OK) {
        rc = read_show_table_status_row_count(database, table, &row_count);
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        } else if (rc != MYLITE_OK) {
            set_runtime_error(database, "failed to read INFORMATION_SCHEMA.INNODB_TABLESTATS rows");
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_count_nonprimary_indexes(
            database,
            table->table_id,
            &other_index_size
        );
    }
    if (rc == MYLITE_OK) {
        rc = table_status_has_auto_increment(database, table, &has_auto_increment);
    }
    if (rc == MYLITE_OK && has_auto_increment) {
        autoinc = table->auto_increment_next;
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            table->table_id,
            table_id_text,
            sizeof(table_id_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            row_count,
            row_count_text,
            sizeof(row_count_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            other_index_size,
            other_index_size_text,
            sizeof(other_index_size_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(database, autoinc, autoinc_text, sizeof(autoinc_text));
    }
    if (rc == MYLITE_OK) {
        const char *values[information_schema_innodb_tablestats_column_count] = {
            table_id_text,
            table_name,
            "Initialized",
            row_count_text,
            "1",
            other_index_size_text,
            "0",
            autoinc_text,
            "1",
        };

        rc = append_information_schema_row(database, rows, values);
    }

    free(table_name);
    return rc;
}

static int information_schema_count_nonprimary_indexes(
    struct mylite_db *database,
    int64_t table_id,
    int64_t *out_count
) {
    struct nonprimary_index_count_context context = {.count = 0};
    int rc = MYLITE_OK;

    if (out_count == NULL) {
        return MYLITE_MISUSE;
    }
    *out_count = 0;

    rc = mylite_catalog_for_each_index_in_table(
        database,
        table_id,
        count_nonprimary_index,
        &context
    );
    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
            return rc;
        }
        set_runtime_error(database, "failed to read INFORMATION_SCHEMA.INNODB_TABLESTATS indexes");
        return MYLITE_ERROR;
    }
    *out_count = context.count;
    return MYLITE_OK;
}

static int count_nonprimary_index(
    const struct mylite_catalog_index_descriptor *index,
    void *user_data
) {
    struct nonprimary_index_count_context *context = user_data;

    if (context == NULL || index == NULL) {
        return MYLITE_MISUSE;
    }
    if (index->kind != MYLITE_CATALOG_INDEX_KIND_PRIMARY) {
        if (context->count == INT64_MAX) {
            return MYLITE_ERROR;
        }
        ++context->count;
    }
    return MYLITE_OK;
}

static int information_schema_innodb_table_flag(
    struct mylite_db *database,
    const char *row_format,
    int64_t *out_flag
) {
    if (text_equals_ascii_case_insensitive(row_format, "Redundant")) {
        *out_flag = information_schema_innodb_table_redundant_flag;
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(row_format, "Compact")) {
        *out_flag = information_schema_innodb_table_compact_flag;
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(row_format, "Dynamic")) {
        *out_flag = information_schema_innodb_table_dynamic_flag;
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(row_format, "Compressed")) {
        *out_flag = information_schema_innodb_table_compressed_flag;
        return MYLITE_OK;
    }

    set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_TABLES row format");
    return MYLITE_ERROR;
}

static int information_schema_innodb_table_column_count(
    struct mylite_db *database,
    size_t column_count,
    int64_t *out_column_count
) {
    if (column_count > (size_t)INT64_MAX - information_schema_innodb_table_hidden_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_TABLES column count");
        return MYLITE_ERROR;
    }

    *out_column_count =
        (int64_t)(column_count + information_schema_innodb_table_hidden_column_count);
    return MYLITE_OK;
}

static int information_schema_innodb_table_zip_page_size(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const char *row_format,
    int64_t *out_zip_page_size
) {
    if (!text_equals_ascii_case_insensitive(row_format, "Compressed")) {
        *out_zip_page_size = information_schema_innodb_table_default_zip_page_size;
        return MYLITE_OK;
    }
    if (table->key_block_size < 0) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_TABLES zip page size");
        return MYLITE_ERROR;
    }
    if (table->key_block_size == 0) {
        *out_zip_page_size = information_schema_innodb_table_default_compressed_zip_page_size;
        return MYLITE_OK;
    }
    if (table->key_block_size > INT64_MAX / information_schema_innodb_table_zip_page_unit) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_TABLES zip page size");
        return MYLITE_ERROR;
    }

    *out_zip_page_size = table->key_block_size * information_schema_innodb_table_zip_page_unit;
    return MYLITE_OK;
}
