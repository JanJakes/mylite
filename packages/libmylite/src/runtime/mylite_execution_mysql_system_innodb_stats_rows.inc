static const char *mysql_system_table_column_comment(
    const struct mylite_execution_catalog_mysql_system_table *definition,
    size_t column_index
) {
    if (definition == NULL || definition->column_comments == NULL ||
        column_index >= definition->column_comment_count) {
        return "";
    }
    return definition->column_comments[column_index];
}

static const char *mysql_system_table_column_generation_expression(
    const struct mylite_execution_catalog_mysql_system_table *definition,
    size_t column_index
) {
    if (definition == NULL || definition->column_generation_expressions == NULL ||
        column_index >= definition->column_generation_expression_count) {
        return "";
    }
    return definition->column_generation_expressions[column_index];
}

static size_t mysql_system_table_primary_key_column_count(
    const struct mylite_execution_catalog_mysql_system_table *definition
) {
    size_t count = 0U;

    if (definition == NULL || definition->column_keys == NULL ||
        definition->column_key_count < definition->query_definition.column_count) {
        return 0U;
    }
    if (definition->primary_key_column_count > 0U) {
        return definition->primary_key_column_count;
    }
    for (size_t column_index = 0U; column_index < definition->query_definition.column_count;
         ++column_index) {
        if (strcmp(definition->column_keys[column_index], "PRI") == 0) {
            ++count;
        }
    }
    return count;
}

static const struct mylite_execution_catalog_column_definition *mysql_system_table_primary_key_column(
    const struct mylite_execution_catalog_mysql_system_table *definition,
    size_t key_index
) {
    size_t sequence = 0U;

    if (definition == NULL ||
        key_index >= mysql_system_table_primary_key_column_count(definition)) {
        return NULL;
    }
    if (definition->primary_key_column_count > 0U) {
        size_t column_index = definition->primary_key_column_indexes[key_index];

        if (column_index >= definition->query_definition.column_count) {
            return NULL;
        }
        return &definition->query_definition.columns[column_index];
    }
    for (size_t column_index = 0U; column_index < definition->query_definition.column_count;
         ++column_index) {
        if (strcmp(definition->column_keys[column_index], "PRI") != 0) {
            continue;
        }
        if (sequence == key_index) {
            return &definition->query_definition.columns[column_index];
        }
        ++sequence;
    }
    return NULL;
}

static int append_mysql_innodb_index_stats_builtin_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = append_mysql_innodb_index_stats_builtin_index_rows(
        database,
        rows,
        "mysql",
        "component",
        "PRIMARY",
        "0",
        "component_id"
    );

    if (rc == MYLITE_OK) {
        rc = append_mysql_innodb_index_stats_builtin_index_rows(
            database,
            rows,
            "sys",
            "sys_config",
            "PRIMARY",
            "6",
            "variable"
        );
    }
    return rc;
}

static int append_mysql_innodb_index_stats_builtin_index_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const char *schema_name,
    const char *table_name,
    const char *index_name,
    const char *distinct_count,
    const char *stat_description
) {
    int rc = MYLITE_OK;

    if (rows->definition->column_count != mysql_innodb_index_stats_column_count) {
        set_runtime_error(database, "invalid mysql.innodb_index_stats columns");
        return MYLITE_ERROR;
    }

    rc = append_mysql_innodb_index_stats_row(
        database,
        rows,
        schema_name,
        table_name,
        index_name,
        "n_diff_pfx01",
        distinct_count,
        "1",
        stat_description
    );
    if (rc == MYLITE_OK) {
        rc = append_mysql_innodb_index_stats_page_row(
            database,
            rows,
            schema_name,
            table_name,
            index_name,
            "n_leaf_pages",
            "Number of leaf pages in the index"
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_mysql_innodb_index_stats_page_row(
            database,
            rows,
            schema_name,
            table_name,
            index_name,
            "size",
            "Number of pages in the index"
        );
    }
    return rc;
}

static int append_mysql_innodb_table_stats_builtin_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = append_mysql_innodb_table_stats_builtin_row(database, rows, "mysql", "component", "0");

    if (rc == MYLITE_OK) {
        rc = append_mysql_innodb_table_stats_builtin_row(database, rows, "sys", "sys_config", "6");
    }
    return rc;
}

static int append_mysql_innodb_table_stats_builtin_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const char *schema_name,
    const char *table_name,
    const char *row_count
) {
    char last_update[datetime_text_length + 1U];
    int rc = mysql_system_current_timestamp(database, last_update, sizeof(last_update));

    if (rows->definition->column_count != mysql_innodb_table_stats_column_count) {
        set_runtime_error(database, "invalid mysql.innodb_table_stats columns");
        return MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        const char *values[mysql_innodb_table_stats_column_count] = {
            schema_name,
            table_name,
            last_update,
            row_count,
            "1",
            "0",
        };

        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}

static int append_mysql_system_table_catalog_rows(
    struct mylite_db *database,
    const struct mylite_execution_catalog_mysql_system_table *definition,
    struct information_schema_row_set *rows
) {
    struct mysql_system_table_catalog_context context = {
        .database = database,
        .rows = rows,
        .definition = definition,
        .schema = NULL,
    };

    return mylite_catalog_for_each_schema(
        database,
        append_mysql_system_table_catalog_schema,
        &context
    );
}

static int append_mysql_system_table_catalog_schema(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
) {
    struct mysql_system_table_catalog_context *context = user_data;

    if (schema == NULL || context == NULL || context->database == NULL || context->rows == NULL ||
        context->definition == NULL) {
        return MYLITE_MISUSE;
    }
    context->schema = schema;
    return mylite_catalog_for_each_table_in_schema(
        context->database,
        schema->schema_id,
        append_mysql_system_table_catalog_table,
        context
    );
}

static int append_mysql_system_table_catalog_table(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
) {
    struct mysql_system_table_catalog_context *context = user_data;

    if (table == NULL || context == NULL || context->database == NULL || context->rows == NULL ||
        context->definition == NULL || context->schema == NULL) {
        return MYLITE_MISUSE;
    }
    if (table->kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        return MYLITE_OK;
    }
    if (strcmp(context->definition->schema_name, "mysql") == 0 &&
        strcmp(context->definition->query_definition.name, "innodb_index_stats") == 0) {
        return append_mysql_innodb_index_stats_base_rows(
            context->database,
            context->rows,
            context->schema,
            table
        );
    }
    if (strcmp(context->definition->schema_name, "mysql") == 0 &&
        strcmp(context->definition->query_definition.name, "innodb_table_stats") == 0) {
        return append_mysql_innodb_table_stats_base_row(
            context->database,
            context->rows,
            context->schema,
            table
        );
    }

    set_runtime_error(context->database, "invalid mysql system table catalog row");
    return MYLITE_ERROR;
}

static int append_mysql_innodb_index_stats_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_index_info *indexes = NULL;
    const struct loaded_index_info *clustered_index = NULL;
    size_t column_count = 0U;
    size_t index_count = 0U;
    int rc = MYLITE_OK;

    if (rows->definition->column_count != mysql_innodb_index_stats_column_count) {
        set_runtime_error(database, "invalid mysql.innodb_index_stats columns");
        return MYLITE_ERROR;
    }

    rc = mylite_execution_load_table_columns(database, table->table_id, &columns, &column_count);
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_index_infos(
            database,
            table->table_id,
            columns,
            column_count,
            &indexes,
            &index_count
        );
    }
    if (rc == MYLITE_OK) {
        clustered_index = information_schema_innodb_clustered_index(indexes, index_count);
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < index_count; ++index) {
        rc = append_mysql_innodb_index_stats_index_rows(
            database,
            rows,
            schema,
            table,
            clustered_index,
            &indexes[index]
        );
    }
    if (rc == MYLITE_OK && clustered_index == NULL) {
        rc = append_mysql_innodb_index_stats_generated_cluster_rows(database, rows, schema, table);
    }

    mylite_execution_loaded_index_infos_deinit(&indexes, &index_count);
    free(columns);
    return rc;
}

static int append_mysql_innodb_index_stats_index_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index
) {
    size_t prefix_count = 0U;
    int rc = mysql_innodb_index_stats_prefix_count(database, clustered_index, index, &prefix_count);

    if (rc == MYLITE_OK && prefix_count == 0U) {
        return MYLITE_OK;
    }
    for (size_t prefix = 1U; rc == MYLITE_OK && prefix <= prefix_count; ++prefix) {
        rc = append_mysql_innodb_index_stats_diff_row(
            database,
            rows,
            schema,
            table,
            clustered_index,
            index,
            prefix
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_mysql_innodb_index_stats_page_row(
            database,
            rows,
            schema->name,
            table->name,
            index->index.name,
            "n_leaf_pages",
            "Number of leaf pages in the index"
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_mysql_innodb_index_stats_page_row(
            database,
            rows,
            schema->name,
            table->name,
            index->index.name,
            "size",
            "Number of pages in the index"
        );
    }
    return rc;
}

static int append_mysql_innodb_index_stats_generated_cluster_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    int rc = append_mysql_innodb_index_stats_generated_diff_row(database, rows, schema, table);

    if (rc == MYLITE_OK) {
        rc = append_mysql_innodb_index_stats_page_row(
            database,
            rows,
            schema->name,
            table->name,
            information_schema_innodb_generated_cluster_index_name,
            "n_leaf_pages",
            "Number of leaf pages in the index"
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_mysql_innodb_index_stats_page_row(
            database,
            rows,
            schema->name,
            table->name,
            information_schema_innodb_generated_cluster_index_name,
            "size",
            "Number of pages in the index"
        );
    }
    return rc;
}

static int append_mysql_innodb_index_stats_diff_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index,
    size_t prefix_count
) {
    char stat_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char stat_value[integer_text_capacity];
    char *stat_description = NULL;
    int64_t distinct_count = 0;
    int written = snprintf(stat_name, sizeof(stat_name), "n_diff_pfx%02zu", prefix_count);
    int rc = MYLITE_OK;

    if (written < 0 || (size_t)written >= sizeof(stat_name)) {
        set_runtime_error(database, "failed to format mysql.innodb_index_stats stat name");
        return MYLITE_ERROR;
    }

    rc = read_mysql_innodb_index_stats_distinct_count(
        database,
        table,
        clustered_index,
        index,
        prefix_count,
        &distinct_count
    );
    if (rc == MYLITE_OK) {
        rc =
            information_schema_format_i64(database, distinct_count, stat_value, sizeof(stat_value));
    }
    if (rc == MYLITE_OK) {
        rc = append_mysql_innodb_index_stats_prefix_description(
            database,
            clustered_index,
            index,
            prefix_count,
            &stat_description
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_mysql_innodb_index_stats_row(
            database,
            rows,
            schema->name,
            table->name,
            index->index.name,
            stat_name,
            stat_value,
            "1",
            stat_description
        );
    }

    free(stat_description);
    return rc;
}

static int append_mysql_innodb_index_stats_generated_diff_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    char stat_value[integer_text_capacity];
    int64_t row_count = 0;
    int rc = read_show_table_status_row_count(database, table, &row_count);

    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    } else if (rc != MYLITE_OK) {
        set_runtime_error(database, "failed to read mysql.innodb_index_stats rows");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(database, row_count, stat_value, sizeof(stat_value));
    }
    if (rc == MYLITE_OK) {
        rc = append_mysql_innodb_index_stats_row(
            database,
            rows,
            schema->name,
            table->name,
            information_schema_innodb_generated_cluster_index_name,
            "n_diff_pfx01",
            stat_value,
            "1",
            "DB_ROW_ID"
        );
    }
    return rc;
}

static int append_mysql_innodb_index_stats_page_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const char *schema_name,
    const char *table_name,
    const char *index_name,
    const char *stat_name,
    const char *stat_description
) {
    return append_mysql_innodb_index_stats_row(
        database,
        rows,
        schema_name,
        table_name,
        index_name,
        stat_name,
        "1",
        NULL,
        stat_description
    );
}

static int append_mysql_innodb_index_stats_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const char *schema_name,
    const char *table_name,
    const char *index_name,
    const char *stat_name,
    const char *stat_value,
    const char *sample_size,
    const char *stat_description
) {
    char last_update[datetime_text_length + 1U];
    int rc = MYLITE_OK;

    if (rows->definition->column_count != mysql_innodb_index_stats_column_count) {
        set_runtime_error(database, "invalid mysql.innodb_index_stats columns");
        return MYLITE_ERROR;
    }

    rc = mysql_system_current_timestamp(database, last_update, sizeof(last_update));
    if (rc == MYLITE_OK) {
        const char *values[mysql_innodb_index_stats_column_count] = {
            schema_name,
            table_name,
            index_name,
            last_update,
            stat_name,
            stat_value,
            sample_size,
            stat_description,
        };

        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}

static int mysql_innodb_index_stats_prefix_count(
    struct mylite_db *database,
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index,
    size_t *out_count
) {
    size_t clustered_part_count = 0U;
    size_t prefix_count = 0U;
    int rc = MYLITE_OK;

    if (out_count == NULL || index == NULL) {
        return MYLITE_MISUSE;
    }
    *out_count = 0U;

    if (index->index.kind == MYLITE_CATALOG_INDEX_KIND_FULLTEXT ||
        index->index.kind == MYLITE_CATALOG_INDEX_KIND_SPATIAL) {
        return MYLITE_OK;
    }
    if (index->index.kind == MYLITE_CATALOG_INDEX_KIND_PRIMARY || index == clustered_index ||
        index->index.is_unique) {
        *out_count = index->part_count;
        return MYLITE_OK;
    }
    if (index->index.kind != MYLITE_CATALOG_INDEX_KIND_SECONDARY) {
        set_runtime_error(database, "invalid mysql.innodb_index_stats index type");
        return MYLITE_ERROR;
    }

    prefix_count = index->part_count;
    if (clustered_index == NULL) {
        if (prefix_count == SIZE_MAX) {
            set_runtime_error(database, "invalid mysql.innodb_index_stats prefix count");
            return MYLITE_ERROR;
        }
        *out_count = prefix_count + 1U;
        return MYLITE_OK;
    }

    rc = information_schema_innodb_secondary_clustered_part_count(
        database,
        index,
        clustered_index,
        &clustered_part_count
    );
    if (rc == MYLITE_OK) {
        if (prefix_count > SIZE_MAX - clustered_part_count) {
            set_runtime_error(database, "invalid mysql.innodb_index_stats prefix count");
            return MYLITE_ERROR;
        }
        *out_count = prefix_count + clustered_part_count;
    }
    return rc;
}

static int read_mysql_innodb_index_stats_distinct_count(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index,
    size_t prefix_count,
    int64_t *out_count
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int rc = build_mysql_innodb_index_stats_distinct_sql(
        database,
        table,
        clustered_index,
        index,
        prefix_count,
        &sql
    );

    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = step_count_statement(statement, out_count);
    }

    rc = finalize_sqlite_statement(statement, rc);
    free(sql);
    return rc;
}

static int build_mysql_innodb_index_stats_distinct_sql(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index,
    size_t prefix_count,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    if (table == NULL || index == NULL || prefix_count == 0U || out_sql == NULL) {
        return MYLITE_MISUSE;
    }
    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT COUNT(*) FROM (SELECT DISTINCT ");
    for (size_t prefix_index = 0U; rc == MYLITE_OK && prefix_index < prefix_count; ++prefix_index) {
        if (prefix_index > 0U) {
            rc = mylite_dynamic_string_append(&string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_mysql_innodb_index_stats_prefix_expression(
                database,
                &string,
                clustered_index,
                index,
                prefix_index
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, table->physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, ")");
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

static int append_mysql_innodb_index_stats_prefix_expression(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index,
    size_t prefix_index
) {
    const struct loaded_index_part *part = NULL;

    if (prefix_index < index->part_count) {
        return append_loaded_key_part_sql(string, &index->parts[prefix_index], NULL);
    }
    if (clustered_index == NULL) {
        return mylite_dynamic_string_append(string, "rowid");
    }

    part = mysql_innodb_index_stats_missing_clustered_part(
        clustered_index,
        index,
        prefix_index - index->part_count
    );
    if (part == NULL) {
        set_runtime_error(database, "invalid mysql.innodb_index_stats prefix column");
        return MYLITE_ERROR;
    }
    return append_loaded_key_part_sql(string, part, NULL);
}

static int append_mysql_innodb_index_stats_prefix_description(
    struct mylite_db *database,
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index,
    size_t prefix_count,
    char **out_description
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    if (index == NULL || prefix_count == 0U || out_description == NULL) {
        return MYLITE_MISUSE;
    }
    *out_description = NULL;
    mylite_dynamic_string_init(&string);

    for (size_t prefix_index = 0U; rc == MYLITE_OK && prefix_index < prefix_count; ++prefix_index) {
        if (prefix_index > 0U) {
            rc = mylite_dynamic_string_append(&string, ",");
        }
        if (rc == MYLITE_OK) {
            rc = append_mysql_innodb_index_stats_prefix_name(
                database,
                &string,
                clustered_index,
                index,
                prefix_index
            );
        }
    }
    if (rc == MYLITE_OK) {
        *out_description = mylite_dynamic_string_take(&string);
        if (*out_description == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    mylite_dynamic_string_deinit(&string);
    return rc;
}

static int append_mysql_innodb_index_stats_prefix_name(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index,
    size_t prefix_index
) {
    const struct loaded_index_part *part = NULL;

    if (prefix_index < index->part_count) {
        return mylite_dynamic_string_append(string, index->parts[prefix_index].column.name);
    }
    if (clustered_index == NULL) {
        return mylite_dynamic_string_append(string, "DB_ROW_ID");
    }

    part = mysql_innodb_index_stats_missing_clustered_part(
        clustered_index,
        index,
        prefix_index - index->part_count
    );
    if (part == NULL) {
        set_runtime_error(database, "invalid mysql.innodb_index_stats prefix description");
        return MYLITE_ERROR;
    }
    return mylite_dynamic_string_append(string, part->column.name);
}

static const struct loaded_index_part *mysql_innodb_index_stats_missing_clustered_part(
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index,
    size_t missing_index
) {
    size_t current = 0U;

    if (clustered_index == NULL || index == NULL) {
        return NULL;
    }
    for (size_t part_index = 0U; part_index < clustered_index->part_count; ++part_index) {
        if (information_schema_innodb_index_contains_column(
                index,
                clustered_index->parts[part_index].column.column_id
            )) {
            continue;
        }
        if (current == missing_index) {
            return &clustered_index->parts[part_index];
        }
        ++current;
    }
    return NULL;
}

static int append_mysql_innodb_table_stats_base_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    char last_update[datetime_text_length + 1U];
    char row_count_text[integer_text_capacity];
    char other_index_size_text[integer_text_capacity];
    int64_t row_count = 0;
    int64_t other_index_size = 0;
    int rc = MYLITE_OK;

    if (rows->definition->column_count != mysql_innodb_table_stats_column_count) {
        set_runtime_error(database, "invalid mysql.innodb_table_stats columns");
        return MYLITE_ERROR;
    }

    rc = mysql_system_current_timestamp(database, last_update, sizeof(last_update));
    if (rc == MYLITE_OK) {
        rc = read_show_table_status_row_count(database, table, &row_count);
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        } else if (rc != MYLITE_OK) {
            set_runtime_error(database, "failed to read mysql.innodb_table_stats rows");
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
        const char *values[mysql_innodb_table_stats_column_count] = {
            schema->name,
            table->name,
            last_update,
            row_count_text,
            "1",
            other_index_size_text,
        };

        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}

static int mysql_system_current_timestamp(
    struct mylite_db *database,
    char *buffer,
    size_t buffer_size
) {
    return format_session_timestamp_epoch_text(
        database,
        current_wall_clock_epoch(),
        buffer,
        buffer_size
    );
}

static int mysql_system_current_timestamp2(
    struct mylite_db *database,
    char *buffer,
    size_t buffer_size
) {
    char timestamp[datetime_text_length + 1U];
    int rc = mysql_system_current_timestamp(database, timestamp, sizeof(timestamp));

    if (rc == MYLITE_OK) {
        int written = snprintf(buffer, buffer_size, "%s.00", timestamp);

        if (written < 0 || (size_t)written >= buffer_size) {
            set_runtime_error(database, "failed to format timestamp(2) metadata");
            rc = MYLITE_ERROR;
        }
    }
    return rc;
}
