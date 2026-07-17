static int append_sys_schema_index_statistics_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    bool formatted_latency
) {
    struct sys_schema_index_statistics_context context = {
        .database = database,
        .rows = rows,
        .formatted_latency = formatted_latency,
        .schema = NULL,
    };
    int rc = MYLITE_OK;

    if (rows->definition->column_count != sys_schema_index_statistics_column_count) {
        set_runtime_error(database, "invalid sys.schema_index_statistics columns");
        return MYLITE_ERROR;
    }

    rc = append_sys_schema_index_statistics_mysql_system_rows(&context);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_for_each_schema(
            database,
            append_sys_schema_index_statistics_schema_rows,
            &context
        );
    }
    return rc;
}

static int append_sys_schema_index_statistics_mysql_system_rows(
    struct sys_schema_index_statistics_context *context
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_mysql_system_table_definition_count();
         ++index) {
        rc = append_sys_schema_index_statistics_mysql_system_table_rows(
            context,
            mylite_execution_catalog_mysql_system_table_definition_at(index)
        );
    }
    return rc;
}

static int append_sys_schema_index_statistics_mysql_system_table_rows(
    struct sys_schema_index_statistics_context *context,
    const struct mylite_execution_catalog_mysql_system_table *definition
) {
    int rc = MYLITE_OK;

    if (definition == NULL || definition->column_keys == NULL ||
        definition->column_key_count < definition->query_definition.column_count) {
        return MYLITE_OK;
    }
    if (mysql_system_table_primary_key_column_count(definition) > 0U) {
        rc = append_sys_schema_index_statistics_index_row(
            context,
            definition->schema_name,
            definition->query_definition.name,
            "PRIMARY"
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < definition->secondary_index_count; ++index) {
        rc = append_sys_schema_index_statistics_index_row(
            context,
            definition->schema_name,
            definition->query_definition.name,
            definition->secondary_indexes[index].name
        );
    }
    return rc;
}

static int append_sys_schema_index_statistics_schema_rows(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
) {
    struct sys_schema_index_statistics_context *context = user_data;

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
        append_sys_schema_index_statistics_table_rows,
        context
    );
}

static int append_sys_schema_index_statistics_table_rows(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
) {
    struct sys_schema_index_statistics_context *context = user_data;

    if (table == NULL || context == NULL || context->database == NULL || context->rows == NULL ||
        context->schema == NULL) {
        return MYLITE_MISUSE;
    }
    if (table->kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        return MYLITE_OK;
    }
    return append_sys_schema_index_statistics_base_index_rows(context, table);
}

static int append_sys_schema_index_statistics_base_index_rows(
    struct sys_schema_index_statistics_context *context,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_index_info *indexes = NULL;
    size_t column_count = 0U;
    size_t index_count = 0U;
    int rc = mylite_execution_load_table_columns(
        context->database,
        table->table_id,
        &columns,
        &column_count
    );

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
        rc = append_sys_schema_index_statistics_index_row(
            context,
            context->schema->name,
            table->name,
            indexes[index].index.name
        );
    }

    mylite_execution_loaded_index_infos_deinit(&indexes, &index_count);
    free(columns);
    return rc;
}

static int append_sys_schema_index_statistics_index_row(
    struct sys_schema_index_statistics_context *context,
    const char *schema_name,
    const char *table_name,
    const char *index_name
) {
    static const char formatted_zero_latency[] = "  0 ps";
    static const char zero[] = "0";

    if (context == NULL || context->database == NULL || context->rows == NULL ||
        schema_name == NULL || table_name == NULL || index_name == NULL) {
        return MYLITE_MISUSE;
    }

    const char *latency = context->formatted_latency ? formatted_zero_latency : zero;
    const char *values[sys_schema_index_statistics_column_count] = {
        schema_name,
        table_name,
        index_name,
        zero,
        latency,
        zero,
        latency,
        zero,
        latency,
        zero,
        latency,
    };

    return append_information_schema_row(context->database, context->rows, values);
}

static int append_sys_x_ps_schema_table_statistics_io_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    struct sys_schema_table_statistics_context context = {
        .database = database,
        .rows = rows,
        .formatted_metrics = false,
        .include_buffer_metrics = false,
        .schema = NULL,
    };
    int rc = MYLITE_OK;

    if (rows->definition->column_count != sys_x_ps_schema_table_statistics_io_column_count) {
        set_runtime_error(database, "invalid sys.x$ps_schema_table_statistics_io columns");
        return MYLITE_ERROR;
    }

    rc = append_sys_x_ps_schema_table_statistics_io_mysql_system_rows(&context);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_for_each_schema(
            database,
            append_sys_x_ps_schema_table_statistics_io_schema_rows,
            &context
        );
    }
    return rc;
}

static int append_sys_x_ps_schema_table_statistics_io_mysql_system_rows(
    struct sys_schema_table_statistics_context *context
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_mysql_system_table_definition_count();
         ++index) {
        rc = append_sys_x_ps_schema_table_statistics_io_mysql_system_table_row(
            context,
            mylite_execution_catalog_mysql_system_table_definition_at(index)
        );
    }
    return rc;
}

static int append_sys_x_ps_schema_table_statistics_io_mysql_system_table_row(
    struct sys_schema_table_statistics_context *context,
    const struct mylite_execution_catalog_mysql_system_table *definition
) {
    if (definition == NULL) {
        return MYLITE_OK;
    }
    if (strcmp(definition->schema_name, "mysql") != 0 &&
        (strcmp(definition->schema_name, "sys") != 0 ||
         strcmp(definition->query_definition.name, "sys_config") != 0)) {
        return MYLITE_OK;
    }
    return append_sys_x_ps_schema_table_statistics_io_table_row(
        context,
        definition->schema_name,
        definition->query_definition.name
    );
}

static int append_sys_x_ps_schema_table_statistics_io_schema_rows(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
) {
    struct sys_schema_table_statistics_context *context = user_data;

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
        append_sys_x_ps_schema_table_statistics_io_table_row_from_catalog,
        context
    );
}

static int append_sys_x_ps_schema_table_statistics_io_table_row_from_catalog(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
) {
    struct sys_schema_table_statistics_context *context = user_data;

    if (table == NULL || context == NULL || context->database == NULL || context->rows == NULL ||
        context->schema == NULL) {
        return MYLITE_MISUSE;
    }
    if (table->kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        return MYLITE_OK;
    }
    return append_sys_x_ps_schema_table_statistics_io_table_row(
        context,
        context->schema->name,
        table->name
    );
}

static int append_sys_x_ps_schema_table_statistics_io_table_row(
    struct sys_schema_table_statistics_context *context,
    const char *schema_name,
    const char *table_name
) {
    static const char zero[] = "0";

    if (context == NULL || context->database == NULL || context->rows == NULL ||
        schema_name == NULL || table_name == NULL) {
        return MYLITE_MISUSE;
    }

    const char *values[sys_x_ps_schema_table_statistics_io_column_count] = {
        schema_name,
        table_name,
        zero,
        zero,
        zero,
        zero,
        zero,
        zero,
        zero,
        zero,
    };

    return append_information_schema_row(context->database, context->rows, values);
}

static int append_sys_schema_table_statistics_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    bool formatted_metrics,
    bool include_buffer_metrics
) {
    struct sys_schema_table_statistics_context context = {
        .database = database,
        .rows = rows,
        .formatted_metrics = formatted_metrics,
        .include_buffer_metrics = include_buffer_metrics,
        .schema = NULL,
    };
    size_t expected_column_count = include_buffer_metrics
                                       ? sys_schema_table_statistics_with_buffer_column_count
                                       : sys_schema_table_statistics_column_count;
    int rc = MYLITE_OK;

    if (rows->definition->column_count != expected_column_count) {
        set_runtime_error(database, "invalid sys.schema_table_statistics columns");
        return MYLITE_ERROR;
    }

    rc = append_sys_schema_table_statistics_mysql_system_rows(&context);
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_for_each_schema(
            database,
            append_sys_schema_table_statistics_schema_rows,
            &context
        );
    }
    return rc;
}

static int append_sys_schema_table_statistics_mysql_system_rows(
    struct sys_schema_table_statistics_context *context
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_mysql_system_table_definition_count();
         ++index) {
        rc = append_sys_schema_table_statistics_mysql_system_table_row(
            context,
            mylite_execution_catalog_mysql_system_table_definition_at(index)
        );
    }
    return rc;
}

static int append_sys_schema_table_statistics_mysql_system_table_row(
    struct sys_schema_table_statistics_context *context,
    const struct mylite_execution_catalog_mysql_system_table *definition
) {
    if (definition == NULL) {
        return MYLITE_OK;
    }
    if (strcmp(definition->schema_name, "mysql") != 0 &&
        (strcmp(definition->schema_name, "sys") != 0 ||
         strcmp(definition->query_definition.name, "sys_config") != 0)) {
        return MYLITE_OK;
    }
    return append_sys_schema_table_statistics_table_row(
        context,
        definition->schema_name,
        definition->query_definition.name
    );
}

static int append_sys_schema_table_statistics_schema_rows(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
) {
    struct sys_schema_table_statistics_context *context = user_data;

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
        append_sys_schema_table_statistics_table_row_from_catalog,
        context
    );
}

static int append_sys_schema_table_statistics_table_row_from_catalog(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
) {
    struct sys_schema_table_statistics_context *context = user_data;

    if (table == NULL || context == NULL || context->database == NULL || context->rows == NULL ||
        context->schema == NULL) {
        return MYLITE_MISUSE;
    }
    if (table->kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        return MYLITE_OK;
    }
    return append_sys_schema_table_statistics_table_row(
        context,
        context->schema->name,
        table->name
    );
}

static int append_sys_schema_table_statistics_table_row(
    struct sys_schema_table_statistics_context *context,
    const char *schema_name,
    const char *table_name
) {
    static const char formatted_zero_bytes[] = "   0 bytes";
    static const char formatted_zero_latency[] = "  0 ps";
    static const char zero[] = "0";

    if (context == NULL || context->database == NULL || context->rows == NULL ||
        schema_name == NULL || table_name == NULL) {
        return MYLITE_MISUSE;
    }

    const char *latency = context->formatted_metrics ? formatted_zero_latency : zero;
    const char *bytes = context->formatted_metrics ? formatted_zero_bytes : zero;
    if (context->include_buffer_metrics) {
        const char *values[sys_schema_table_statistics_with_buffer_column_count] = {
            schema_name, table_name, zero,  latency, zero, latency, zero,    latency, zero,
            latency,     zero,       bytes, latency, zero, bytes,   latency, zero,    latency,
            bytes,       bytes,      bytes, zero,    zero, zero,    zero,
        };

        return append_information_schema_row(context->database, context->rows, values);
    }

    const char *values[sys_schema_table_statistics_column_count] = {
        schema_name, table_name, latency, zero,    latency, zero,  latency, zero, latency, zero,
        latency,     zero,       bytes,   latency, zero,    bytes, latency, zero, latency,
    };

    return append_information_schema_row(context->database, context->rows, values);
}
