static int append_sys_schema_redundant_indexes_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    struct sys_schema_redundant_indexes_context context = {
        .database = database,
        .rows = rows,
        .schema = NULL,
    };

    if (rows->definition->column_count != sys_schema_redundant_indexes_column_count) {
        set_runtime_error(database, "invalid sys.schema_redundant_indexes columns");
        return MYLITE_ERROR;
    }

    return mylite_catalog_for_each_schema(
        database,
        append_sys_schema_redundant_indexes_schema_rows,
        &context
    );
}

static int append_sys_x_schema_flattened_keys_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    struct sys_schema_redundant_indexes_context context = {
        .database = database,
        .rows = rows,
        .schema = NULL,
    };

    if (rows->definition->column_count != sys_x_schema_flattened_keys_column_count) {
        set_runtime_error(database, "invalid sys.x$schema_flattened_keys columns");
        return MYLITE_ERROR;
    }

    return mylite_catalog_for_each_schema(
        database,
        append_sys_x_schema_flattened_keys_schema_rows,
        &context
    );
}

static int append_sys_schema_redundant_indexes_schema_rows(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
) {
    struct sys_schema_redundant_indexes_context *context = user_data;

    if (schema == NULL || context == NULL || context->database == NULL || context->rows == NULL) {
        return MYLITE_MISUSE;
    }
    if (find_builtin_schema_table_directory(schema->name) != NULL) {
        return MYLITE_OK;
    }

    context->schema = schema;
    return mylite_catalog_for_each_table_in_schema(
        context->database,
        schema->schema_id,
        append_sys_schema_redundant_indexes_table_rows,
        context
    );
}

static int append_sys_x_schema_flattened_keys_schema_rows(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
) {
    struct sys_schema_redundant_indexes_context *context = user_data;

    if (schema == NULL || context == NULL || context->database == NULL || context->rows == NULL) {
        return MYLITE_MISUSE;
    }
    if (find_builtin_schema_table_directory(schema->name) != NULL) {
        return MYLITE_OK;
    }

    context->schema = schema;
    return mylite_catalog_for_each_table_in_schema(
        context->database,
        schema->schema_id,
        append_sys_x_schema_flattened_keys_table_rows,
        context
    );
}

static int append_sys_schema_redundant_indexes_table_rows(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
) {
    struct sys_schema_redundant_indexes_context *context = user_data;
    struct sys_flattened_key_row_set key_rows = {0};
    int rc = MYLITE_OK;

    if (table == NULL || context == NULL || context->database == NULL || context->rows == NULL ||
        context->schema == NULL) {
        return MYLITE_MISUSE;
    }
    if (table->kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        return MYLITE_OK;
    }

    rc = build_sys_flattened_key_rows(context, table, &key_rows);
    if (rc == MYLITE_OK) {
        rc = append_sys_schema_redundant_index_rows(context, &key_rows);
    }
    sys_flattened_key_rows_deinit(&key_rows);
    return rc;
}

static int append_sys_x_schema_flattened_keys_table_rows(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
) {
    struct sys_schema_redundant_indexes_context *context = user_data;
    struct sys_flattened_key_row_set key_rows = {0};
    int rc = MYLITE_OK;

    if (table == NULL || context == NULL || context->database == NULL || context->rows == NULL ||
        context->schema == NULL) {
        return MYLITE_MISUSE;
    }
    if (table->kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        return MYLITE_OK;
    }

    rc = build_sys_flattened_key_rows(context, table, &key_rows);
    if (rc == MYLITE_OK) {
        rc = append_sys_x_schema_flattened_key_rows(context, &key_rows);
    }
    sys_flattened_key_rows_deinit(&key_rows);
    return rc;
}

static int build_sys_flattened_key_rows(
    struct sys_schema_redundant_indexes_context *context,
    const struct mylite_catalog_table_descriptor *table,
    struct sys_flattened_key_row_set *out_rows
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_index_info *indexes = NULL;
    size_t column_count = 0U;
    size_t index_count = 0U;
    int rc = MYLITE_OK;

    *out_rows = (struct sys_flattened_key_row_set){0};
    rc = mylite_execution_load_table_columns(context->database, table->table_id, &columns, &column_count);
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_index_infos(
            context->database,
            table->table_id,
            columns,
            column_count,
            &indexes,
            &index_count
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < index_count; ++index) {
        if (sys_flattened_key_index_supported(&indexes[index])) {
            rc = append_sys_flattened_key_row_from_index(context, table, &indexes[index], out_rows);
        }
    }

    mylite_execution_loaded_index_infos_deinit(&indexes, &index_count);
    free(columns);
    return rc;
}

static int append_sys_x_schema_flattened_key_rows(
    struct sys_schema_redundant_indexes_context *context,
    const struct sys_flattened_key_row_set *key_rows
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < key_rows->count; ++index) {
        rc = append_sys_x_schema_flattened_key_row(context, &key_rows->rows[index]);
    }
    return rc;
}

static int append_sys_schema_redundant_index_rows(
    struct sys_schema_redundant_indexes_context *context,
    const struct sys_flattened_key_row_set *key_rows
) {
    int rc = MYLITE_OK;

    for (size_t redundant_index = 0U; rc == MYLITE_OK && redundant_index < key_rows->count;
         ++redundant_index) {
        const struct sys_flattened_key_row *redundant = &key_rows->rows[redundant_index];

        for (size_t dominant_index = 0U; rc == MYLITE_OK && dominant_index < key_rows->count;
             ++dominant_index) {
            const struct sys_flattened_key_row *dominant = &key_rows->rows[dominant_index];

            if (sys_schema_redundant_index_pair_matches(redundant, dominant)) {
                rc = append_sys_schema_redundant_index_row(context, redundant, dominant);
            }
        }
    }
    return rc;
}

static int append_sys_x_schema_flattened_key_row(
    struct sys_schema_redundant_indexes_context *context,
    const struct sys_flattened_key_row *key_row
) {
    const char *non_unique = key_row->non_unique ? "1" : "0";
    const char *subpart_exists = key_row->subpart_exists ? "1" : "0";
    const char *values[sys_x_schema_flattened_keys_column_count] = {
        key_row->schema_name,
        key_row->table_name,
        key_row->index_name,
        non_unique,
        subpart_exists,
        key_row->index_columns,
    };

    return append_information_schema_row(context->database, context->rows, values);
}

static int append_sys_schema_redundant_index_row(
    struct sys_schema_redundant_indexes_context *context,
    const struct sys_flattened_key_row *redundant,
    const struct sys_flattened_key_row *dominant
) {
    char *drop_index_sql = NULL;
    const char *redundant_non_unique = redundant->non_unique ? "1" : "0";
    const char *dominant_non_unique = dominant->non_unique ? "1" : "0";
    const char *subpart_exists = redundant->subpart_exists || dominant->subpart_exists ? "1" : "0";
    int rc = build_sys_redundant_drop_index_sql(redundant, &drop_index_sql);

    if (rc != MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(context->database);
        }
        return rc;
    }

    const char *values[sys_schema_redundant_indexes_column_count] = {
        redundant->schema_name,
        redundant->table_name,
        redundant->index_name,
        redundant->index_columns,
        redundant_non_unique,
        dominant->index_name,
        dominant->index_columns,
        dominant_non_unique,
        subpart_exists,
        drop_index_sql,
    };

    rc = append_information_schema_row(context->database, context->rows, values);
    free(drop_index_sql);
    return rc;
}

static bool sys_schema_redundant_index_pair_matches(
    const struct sys_flattened_key_row *redundant,
    const struct sys_flattened_key_row *dominant
) {
    const char *redundant_sort_name = NULL;
    const char *dominant_sort_name = NULL;

    if (redundant == NULL || dominant == NULL ||
        strcmp(redundant->index_name, dominant->index_name) == 0) {
        return false;
    }
    if (strcmp(redundant->index_columns, dominant->index_columns) == 0) {
        if (redundant->non_unique && !dominant->non_unique) {
            return true;
        }
        redundant_sort_name =
            strcmp(redundant->index_name, "PRIMARY") == 0 ? "" : redundant->index_name;
        dominant_sort_name =
            strcmp(dominant->index_name, "PRIMARY") == 0 ? "" : dominant->index_name;
        if (redundant->non_unique == dominant->non_unique &&
            strcmp(redundant_sort_name, dominant_sort_name) > 0) {
            return true;
        }
    }
    if (redundant->non_unique && sys_schema_redundant_columns_left_prefix(
                                     redundant->index_columns,
                                     dominant->index_columns
                                 )) {
        return true;
    }
    return !dominant->non_unique && sys_schema_redundant_columns_left_prefix(
                                        dominant->index_columns,
                                        redundant->index_columns
                                    );
}

static bool sys_schema_redundant_columns_left_prefix(const char *left, const char *right) {
    size_t left_length = 0U;

    if (left == NULL || right == NULL) {
        return false;
    }
    left_length = strlen(left);
    if (left_length == 0U || strncmp(left, right, left_length) != 0) {
        return false;
    }
    return right[left_length] == ',';
}

static int append_sys_flattened_key_row_from_index(
    struct sys_schema_redundant_indexes_context *context,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_index_info *index,
    struct sys_flattened_key_row_set *rows
) {
    struct sys_flattened_key_row row = {0};
    int written = snprintf(row.schema_name, sizeof(row.schema_name), "%s", context->schema->name);
    int rc = MYLITE_OK;

    if (written < 0 || (size_t)written >= sizeof(row.schema_name)) {
        set_runtime_error(context->database, "invalid sys.x$schema_flattened_keys schema name");
        return MYLITE_ERROR;
    }
    written = snprintf(row.table_name, sizeof(row.table_name), "%s", table->name);
    if (written < 0 || (size_t)written >= sizeof(row.table_name)) {
        set_runtime_error(context->database, "invalid sys.x$schema_flattened_keys table name");
        return MYLITE_ERROR;
    }
    written = snprintf(row.index_name, sizeof(row.index_name), "%s", index->index.name);
    if (written < 0 || (size_t)written >= sizeof(row.index_name)) {
        set_runtime_error(context->database, "invalid sys.x$schema_flattened_keys index name");
        return MYLITE_ERROR;
    }

    row.non_unique = !index->index.is_unique;
    for (size_t part_index = 0U; part_index < index->part_count; ++part_index) {
        row.subpart_exists =
            row.subpart_exists || index->parts[part_index].index_column.has_prefix_length;
    }

    rc = build_sys_flattened_key_index_columns(index, &row.index_columns);
    if (rc == MYLITE_OK) {
        rc = reserve_sys_flattened_key_rows(rows, rows->count + 1U);
    }
    if (rc != MYLITE_OK) {
        free(row.index_columns);
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(context->database);
        }
        return rc;
    }

    rows->rows[rows->count] = row;
    ++rows->count;
    return MYLITE_OK;
}

static bool sys_flattened_key_index_supported(const struct loaded_index_info *index) {
    if (index == NULL || index->part_count == 0U) {
        return false;
    }
    return index->index.kind == MYLITE_CATALOG_INDEX_KIND_PRIMARY ||
           index->index.kind == MYLITE_CATALOG_INDEX_KIND_SECONDARY;
}

static int build_sys_flattened_key_index_columns(
    const struct loaded_index_info *index,
    char **out_columns
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_columns = NULL;
    mylite_dynamic_string_init(&string);
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < index->part_count; ++part_index) {
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append_char(&string, ',');
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(&string, index->parts[part_index].column.name);
        }
    }
    if (rc == MYLITE_OK) {
        *out_columns = mylite_dynamic_string_take(&string);
        if (*out_columns == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    mylite_dynamic_string_deinit(&string);
    return rc;
}

static int reserve_sys_flattened_key_rows(
    struct sys_flattened_key_row_set *rows,
    size_t required_capacity
) {
    struct sys_flattened_key_row *resized = NULL;
    size_t capacity =
        rows->capacity == 0U ? sys_flattened_key_row_initial_capacity : rows->capacity;

    if (required_capacity <= rows->capacity) {
        return MYLITE_OK;
    }
    while (capacity < required_capacity) {
        if (capacity > SIZE_MAX / 2U) {
            return MYLITE_NOMEM;
        }
        capacity *= 2U;
    }
    if (capacity > SIZE_MAX / sizeof(rows->rows[0])) {
        return MYLITE_NOMEM;
    }

    resized = realloc(rows->rows, capacity * sizeof(rows->rows[0]));
    if (resized == NULL) {
        return MYLITE_NOMEM;
    }
    rows->rows = resized;
    rows->capacity = capacity;
    return MYLITE_OK;
}

static void sys_flattened_key_rows_deinit(struct sys_flattened_key_row_set *rows) {
    if (rows == NULL) {
        return;
    }
    for (size_t index = 0U; index < rows->count; ++index) {
        free(rows->rows[index].index_columns);
    }
    free(rows->rows);
    *rows = (struct sys_flattened_key_row_set){0};
}

static int build_sys_redundant_drop_index_sql(
    const struct sys_flattened_key_row *redundant,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);
    rc = mylite_dynamic_string_append(&string, "ALTER TABLE ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_mysql_quoted_identifier(&string, redundant->schema_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(&string, '.');
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_mysql_quoted_identifier(&string, redundant->table_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " DROP INDEX ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_mysql_quoted_identifier(&string, redundant->index_name);
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

static int append_sys_schema_table_lock_waits_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_schema_table_lock_waits_column_count) {
        set_runtime_error(database, "invalid sys.schema_table_lock_waits columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_schema_tables_with_full_table_scans_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != sys_schema_tables_with_full_table_scans_column_count) {
        set_runtime_error(database, "invalid sys.schema_tables_with_full_table_scans columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_sys_schema_unused_indexes_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    struct sys_schema_unused_indexes_context context = {
        .database = database,
        .rows = rows,
        .schema = NULL,
    };

    if (rows->definition->column_count != sys_schema_unused_indexes_column_count) {
        set_runtime_error(database, "invalid sys.schema_unused_indexes columns");
        return MYLITE_ERROR;
    }

    return mylite_catalog_for_each_schema(
        database,
        append_sys_schema_unused_indexes_schema_rows,
        &context
    );
}

static int append_sys_schema_unused_indexes_schema_rows(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
) {
    struct sys_schema_unused_indexes_context *context = user_data;

    if (schema == NULL || context == NULL || context->database == NULL || context->rows == NULL) {
        return MYLITE_MISUSE;
    }
    if (find_builtin_schema_table_directory(schema->name) != NULL) {
        return MYLITE_OK;
    }

    context->schema = schema;
    return mylite_catalog_for_each_table_in_schema(
        context->database,
        schema->schema_id,
        append_sys_schema_unused_indexes_table_rows,
        context
    );
}

static int append_sys_schema_unused_indexes_table_rows(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
) {
    struct sys_schema_unused_indexes_context *context = user_data;

    if (table == NULL || context == NULL || context->database == NULL || context->rows == NULL ||
        context->schema == NULL) {
        return MYLITE_MISUSE;
    }
    if (table->kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        return MYLITE_OK;
    }
    return append_sys_schema_unused_indexes_index_rows(context, table);
}

static int append_sys_schema_unused_indexes_index_rows(
    struct sys_schema_unused_indexes_context *context,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_index_info *indexes = NULL;
    size_t column_count = 0U;
    size_t index_count = 0U;
    int rc = mylite_execution_load_table_columns(context->database, table->table_id, &columns, &column_count);

    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_index_infos(
            context->database,
            table->table_id,
            columns,
            column_count,
            &indexes,
            &index_count
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < index_count; ++index) {
        if (sys_schema_unused_indexes_index_supported(&indexes[index])) {
            rc = append_sys_schema_unused_indexes_index_row(context, table->name, &indexes[index]);
        }
    }

    mylite_execution_loaded_index_infos_deinit(&indexes, &index_count);
    free(columns);
    return rc;
}

static int append_sys_schema_unused_indexes_index_row(
    struct sys_schema_unused_indexes_context *context,
    const char *table_name,
    const struct loaded_index_info *index
) {
    const char *values[sys_schema_unused_indexes_column_count] = {
        context->schema->name,
        table_name,
        index->index.name,
    };

    return append_information_schema_row(context->database, context->rows, values);
}

static bool sys_schema_unused_indexes_index_supported(const struct loaded_index_info *index) {
    if (index == NULL || index->part_count == 0U || index->index.is_unique) {
        return false;
    }
    return index->index.kind == MYLITE_CATALOG_INDEX_KIND_SECONDARY ||
           index->index.kind == MYLITE_CATALOG_INDEX_KIND_FULLTEXT ||
           index->index.kind == MYLITE_CATALOG_INDEX_KIND_SPATIAL;
}
