static int append_show_table_status(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
) {
    struct show_table_status_context *context = user_data;
    struct table_status_values status = {0};
    const char *values[show_table_status_result_column_count] = {0};
    bool where_matches = true;
    int rc = MYLITE_OK;

    if (table == NULL || context == NULL || context->database == NULL || context->result == NULL) {
        return MYLITE_MISUSE;
    }
    if (table->kind != MYLITE_CATALOG_TABLE_KIND_BASE &&
        table->kind != MYLITE_CATALOG_TABLE_KIND_VIEW) {
        return MYLITE_OK;
    }
    if (!show_like_filter_matches(context->filter, table->name, true)) {
        return MYLITE_OK;
    }

    rc = load_show_table_status_values(context->database, table, &status);
    if (rc != MYLITE_OK) {
        return rc;
    }
    fill_show_table_status_row_values(table, &status, values);
    rc = show_table_status_row_matches(context, table, &status, values, &where_matches);
    if (rc == MYLITE_OK && !where_matches) {
        return MYLITE_OK;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_text_row(context->result, values);
    }
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(context->database);
    }
    return rc;
}

static int load_show_table_status_values(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    struct table_status_values *status
) {
    if (table->kind == MYLITE_CATALOG_TABLE_KIND_BASE) {
        return load_table_status_values(database, table, status);
    }
    return format_table_status_timestamp(
        database,
        table->created_time_utc_epoch,
        status->create_time_text,
        sizeof(status->create_time_text),
        &status->create_time
    );
}

static void fill_show_table_status_row_values(
    const struct mylite_catalog_table_descriptor *table,
    const struct table_status_values *status,
    const char **values
) {
    values[show_table_status_name_column] = table->name;
    values[show_table_status_row_format_column] = status->row_format;
    values[show_table_status_index_length_column] = status->index_length;
    values[show_table_status_auto_increment_column] = status->auto_increment;
    values[show_table_status_create_time_column] = status->create_time;
    values[show_table_status_update_time_column] = status->update_time;
    values[show_table_status_create_options_column] = status->create_options;

    if (table->kind == MYLITE_CATALOG_TABLE_KIND_VIEW) {
        values[show_table_status_comment_column] = "VIEW";
        return;
    }

    values[show_table_status_engine_column] = "InnoDB";
    values[show_table_status_version_column] = "10";
    values[show_table_status_rows_column] = status->row_count_text;
    values[show_table_status_average_row_length_column] = status->average_row_length_text;
    values[show_table_status_data_length_column] = "16384";
    values[show_table_status_max_data_length_column] = "0";
    values[show_table_status_data_free_column] = "0";
    values[show_table_status_collation_column] = table->default_collation;
    values[show_table_status_comment_column] = table->comment;
}

static int show_table_status_row_matches(
    struct show_table_status_context *context,
    const struct mylite_catalog_table_descriptor *table,
    const struct table_status_values *status,
    const char *values[show_table_status_result_column_count],
    bool *out_matches
) {
    char auto_increment_predicate_text[integer_text_capacity];
    int rc = MYLITE_OK;

    *out_matches = true;
    if (context->where_clause == NULL) {
        return MYLITE_OK;
    }
    if (table->kind == MYLITE_CATALOG_TABLE_KIND_BASE) {
        rc = table_status_auto_increment_predicate_value(
            context->database,
            table,
            status,
            auto_increment_predicate_text,
            sizeof(auto_increment_predicate_text),
            &values[show_table_status_auto_increment_column]
        );
    }
    if (rc == MYLITE_OK) {
        rc = show_table_status_where_clause_matches(
            context->database,
            context->where_clause,
            values,
            out_matches
        );
    }
    return rc;
}

static int append_show_builtin_schema_table_status(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    struct show_table_status_context *context
) {
    int rc = MYLITE_OK;

    if (directory == NULL || context == NULL) {
        return MYLITE_MISUSE;
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < directory->table_count; ++index) {
        rc = append_show_builtin_table_status(directory, directory->table_names[index], context);
    }
    return rc;
}

static int append_show_builtin_table_status(
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name,
    struct show_table_status_context *context
) {
    struct table_status_values status = {0};
    bool where_matches = true;
    int rc = MYLITE_OK;

    if (directory == NULL || table_name == NULL || context == NULL || context->database == NULL ||
        context->result == NULL) {
        return MYLITE_MISUSE;
    }
    if (!show_like_filter_matches(context->filter, table_name, true)) {
        return MYLITE_OK;
    }

    rc = load_builtin_table_status_values(context->database, directory, table_name, &status);
    const char *table_type = builtin_schema_table_type(directory, table_name);
    const char *values[show_table_status_result_column_count] = {
        table_name,
        builtin_schema_table_engine(directory, table_name),
        builtin_schema_table_version(directory, table_name),
        builtin_schema_table_row_format(directory, table_name),
        builtin_schema_table_rows(directory, table_name),
        builtin_schema_table_average_row_length(directory, table_name),
        builtin_schema_table_data_length(directory, table_name),
        table_type == NULL || strcmp(table_type, "VIEW") == 0 ? NULL : "0",
        builtin_schema_table_index_length(directory, table_name),
        builtin_schema_table_data_free(directory, table_name),
        builtin_schema_table_auto_increment(directory, table_name),
        status.create_time,
        status.update_time,
        NULL,
        builtin_schema_table_collation(directory, table_name),
        NULL,
        builtin_schema_table_create_options(directory, table_name),
        builtin_schema_table_comment(directory, table_name),
    };

    if (rc == MYLITE_OK && context->where_clause != NULL) {
        rc = show_table_status_where_clause_matches(
            context->database,
            context->where_clause,
            values,
            &where_matches
        );
    }
    if (rc == MYLITE_OK && !where_matches) {
        return MYLITE_OK;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_text_row(context->result, values);
    }
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(context->database);
    }
    return rc;
}

static int format_builtin_table_status_timestamp(
    struct mylite_db *database,
    struct table_status_values *status
) {
    if (status == NULL) {
        return MYLITE_MISUSE;
    }
    return format_table_status_timestamp(
        database,
        current_timestamp_epoch(database),
        status->create_time_text,
        sizeof(status->create_time_text),
        &status->create_time
    );
}

static int validate_show_table_status_where_clause(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *where_clause
) {
    static const char *const validation_values[show_table_status_result_column_count] = {
        "table_name",
        "InnoDB",
        "10",
        "Dynamic",
        "0",
        "0",
        "16384",
        "0",
        "0",
        "0",
        "0",
        "2000-01-01 00:00:00",
        "2000-01-01 00:00:00",
        "2000-01-01 00:00:00",
        "utf8mb4_0900_ai_ci",
        "0",
        "",
        "",
    };
    bool matches = false;

    if (where_clause == NULL) {
        return MYLITE_OK;
    }
    return show_table_status_where_clause_matches(
        database,
        where_clause,
        validation_values,
        &matches
    );
}
