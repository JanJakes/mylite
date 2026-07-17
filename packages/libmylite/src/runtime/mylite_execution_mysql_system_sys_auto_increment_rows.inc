enum sys_schema_auto_increment_columns_column_index {
    sys_schema_auto_increment_columns_table_schema_column = 0,
    sys_schema_auto_increment_columns_table_name_column = 1,
    sys_schema_auto_increment_columns_column_name_column = 2,
    sys_schema_auto_increment_columns_max_value_column = 7,
    sys_schema_auto_increment_columns_auto_increment_ratio_column = 9,
};

static int append_sys_schema_auto_increment_columns_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    struct sys_schema_auto_increment_columns_context context = {
        .database = database,
        .rows = rows,
        .schema = NULL,
    };
    int rc = MYLITE_OK;

    if (rows->definition->column_count != sys_schema_auto_increment_columns_column_count) {
        set_runtime_error(database, "invalid sys.schema_auto_increment_columns columns");
        return MYLITE_ERROR;
    }

    rc = mylite_catalog_for_each_schema(
        database,
        append_sys_schema_auto_increment_columns_schema_rows,
        &context
    );
    if (rc == MYLITE_OK && !rows->count_only) {
        sort_sys_schema_auto_increment_columns_default_order(rows);
    }
    return rc;
}

static int append_sys_schema_auto_increment_columns_schema_rows(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
) {
    struct sys_schema_auto_increment_columns_context *context = user_data;

    if (schema == NULL || context == NULL || context->database == NULL || context->rows == NULL) {
        return MYLITE_MISUSE;
    }
    if (strcmp(schema->name, "mysql") == 0 || strcmp(schema->name, "sys") == 0 ||
        strcmp(schema->name, "performance_schema") == 0 ||
        schema_name_is_information_schema(schema->name)) {
        return MYLITE_OK;
    }

    context->schema = schema;
    return mylite_catalog_for_each_table_in_schema(
        context->database,
        schema->schema_id,
        append_sys_schema_auto_increment_columns_table_rows,
        context
    );
}

static int append_sys_schema_auto_increment_columns_table_rows(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
) {
    struct sys_schema_auto_increment_columns_context *context = user_data;
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_table_columns_cache_entry *columns_cache_entry = NULL;
    size_t column_count = 0U;
    int rc = MYLITE_OK;

    if (table == NULL || context == NULL || context->database == NULL || context->rows == NULL ||
        context->schema == NULL) {
        return MYLITE_MISUSE;
    }
    if (table->kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        return MYLITE_OK;
    }

    rc = mylite_execution_acquire_table_columns(
        context->database,
        table->table_id,
        &columns,
        &column_count,
        &columns_cache_entry
    );
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < column_count; ++column_index) {
        if (!columns[column_index].is_auto_increment) {
            continue;
        }
        rc = append_sys_schema_auto_increment_columns_column_row(
            context,
            table,
            &columns[column_index]
        );
    }
    mylite_execution_release_table_columns(columns_cache_entry, columns);
    if (rc != MYLITE_OK &&
        mylite_diagnostics_errcode(mylite_connection_diagnostics(context->database)) == MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(context->database);
        } else {
            set_runtime_error(
                context->database,
                "failed to build sys.schema_auto_increment_columns rows"
            );
        }
    }
    return rc;
}

static int append_sys_schema_auto_increment_columns_column_row(
    struct sys_schema_auto_increment_columns_context *context,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column
) {
    char column_type_text[MYLITE_CATALOG_TYPE_NAME_CAPACITY];
    char max_value_text[integer_text_capacity];
    char auto_increment_text[integer_text_capacity];
    char ratio_text[integer_text_capacity];
    const char *column_type = NULL;
    const char *max_value_cell = NULL;
    const char *auto_increment_cell = NULL;
    const char *auto_increment_ratio_cell = NULL;
    const char *data_type = information_schema_data_type_for_descriptor(column);
    bool is_unsigned =
        information_schema_innodb_column_logical_type_is_unsigned(column->logical_type);
    uint64_t max_value = 0U;
    int64_t row_count = 0;
    int rc = show_column_type_text(
        context->database,
        column->logical_type,
        column_type_text,
        sizeof(column_type_text),
        &column_type
    );

    if (rc == MYLITE_OK &&
        sys_schema_auto_increment_columns_max_value(data_type, is_unsigned, &max_value)) {
        rc = format_uint64(context->database, max_value, max_value_text, sizeof(max_value_text));
        if (rc == MYLITE_OK) {
            max_value_cell = max_value_text;
        }
    }
    if (rc == MYLITE_OK) {
        rc = read_show_table_status_row_count(context->database, table, &row_count);
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(context->database);
        } else if (rc != MYLITE_OK) {
            set_runtime_error(
                context->database,
                "failed to read sys.schema_auto_increment_columns row count"
            );
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && max_value_cell != NULL &&
        !(row_count == 0 && table->auto_increment_next <= 1)) {
        rc = information_schema_format_i64(
            context->database,
            table->auto_increment_next,
            auto_increment_text,
            sizeof(auto_increment_text)
        );
        if (rc == MYLITE_OK) {
            auto_increment_cell = auto_increment_text;
            rc = format_sys_schema_auto_increment_columns_ratio(
                context->database,
                table->auto_increment_next,
                max_value,
                ratio_text,
                sizeof(ratio_text)
            );
        }
        if (rc == MYLITE_OK) {
            auto_increment_ratio_cell = ratio_text;
        }
    }
    if (rc == MYLITE_OK) {
        const char *values[sys_schema_auto_increment_columns_column_count] = {
            context->schema->name,
            table->name,
            column->name,
            data_type,
            column_type,
            is_unsigned ? "0" : "1",
            is_unsigned ? "1" : "0",
            max_value_cell,
            auto_increment_cell,
            auto_increment_ratio_cell,
        };

        rc = append_information_schema_row(context->database, context->rows, values);
    }
    return rc;
}

static bool sys_schema_auto_increment_columns_max_value(
    const char *data_type,
    bool is_unsigned,
    uint64_t *out_value
) {
    uint64_t value = 0U;

    if (out_value == NULL) {
        return false;
    }
    *out_value = 0U;
    if (data_type == NULL) {
        return false;
    }
    if (strcmp(data_type, "tinyint") == 0) {
        value = UINT64_C(255);
    } else if (strcmp(data_type, "smallint") == 0) {
        value = UINT64_C(65535);
    } else if (strcmp(data_type, "mediumint") == 0) {
        value = UINT64_C(16777215);
    } else if (strcmp(data_type, "int") == 0) {
        value = UINT64_C(4294967295);
    } else if (strcmp(data_type, "bigint") == 0) {
        value = UINT64_MAX;
    } else {
        return false;
    }

    *out_value = is_unsigned ? value : value >> 1U;
    return true;
}

static int format_sys_schema_auto_increment_columns_ratio(
    struct mylite_db *database,
    int64_t auto_increment,
    uint64_t max_value,
    char *buffer,
    size_t buffer_size
) {
    int written = 0;

    if (auto_increment < 0 || max_value == 0U) {
        set_runtime_error(database, "invalid sys.schema_auto_increment_columns ratio");
        return MYLITE_ERROR;
    }

    written =
        snprintf(buffer, buffer_size, "%.4f", (double)(uint64_t)auto_increment / (double)max_value);
    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format sys.schema_auto_increment_columns ratio");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static void sort_sys_schema_auto_increment_columns_default_order(
    struct information_schema_row_set *rows
) {
    if (rows == NULL || rows->row_count < 2U) {
        return;
    }

    for (size_t index = 1U; index < rows->row_count; ++index) {
        char **row = rows->rows[index];
        size_t insert = index;

        while (insert > 0U &&
               compare_sys_schema_auto_increment_columns_rows(row, rows->rows[insert - 1U]) < 0) {
            rows->rows[insert] = rows->rows[insert - 1U];
            --insert;
        }
        rows->rows[insert] = row;
    }
}

static int compare_sys_schema_auto_increment_columns_rows(char *const *left, char *const *right) {
    const char *left_ratio = left[sys_schema_auto_increment_columns_auto_increment_ratio_column];
    const char *right_ratio = right[sys_schema_auto_increment_columns_auto_increment_ratio_column];
    int text_compare = 0;

    if (left_ratio == NULL && right_ratio != NULL) {
        return 1;
    }
    if (left_ratio != NULL && right_ratio == NULL) {
        return -1;
    }
    if (left_ratio != NULL && right_ratio != NULL) {
        double left_value = strtod(left_ratio, NULL);
        double right_value = strtod(right_ratio, NULL);

        if (left_value > right_value) {
            return -1;
        }
        if (left_value < right_value) {
            return 1;
        }
    }

    if (left[sys_schema_auto_increment_columns_max_value_column] != NULL &&
        right[sys_schema_auto_increment_columns_max_value_column] != NULL) {
        uint64_t left_max =
            strtoull(left[sys_schema_auto_increment_columns_max_value_column], NULL, decimal_base);
        uint64_t right_max =
            strtoull(right[sys_schema_auto_increment_columns_max_value_column], NULL, decimal_base);

        if (left_max < right_max) {
            return -1;
        }
        if (left_max > right_max) {
            return 1;
        }
    }

    text_compare = compare_optional_text(
        left[sys_schema_auto_increment_columns_table_schema_column],
        right[sys_schema_auto_increment_columns_table_schema_column]
    );
    if (text_compare != 0) {
        return text_compare;
    }
    text_compare = compare_optional_text(
        left[sys_schema_auto_increment_columns_table_name_column],
        right[sys_schema_auto_increment_columns_table_name_column]
    );
    if (text_compare != 0) {
        return text_compare;
    }
    return compare_optional_text(
        left[sys_schema_auto_increment_columns_column_name_column],
        right[sys_schema_auto_increment_columns_column_name_column]
    );
}

static int compare_optional_text(const char *left, const char *right) {
    if (left == NULL && right == NULL) {
        return 0;
    }
    if (left == NULL) {
        return 1;
    }
    if (right == NULL) {
        return -1;
    }
    return strcmp(left, right);
}
