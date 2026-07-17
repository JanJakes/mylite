static int append_information_schema_key_column_usage_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_table_columns_cache_entry *columns_cache_entry = NULL;
    struct loaded_table_key_metadata_reference key_metadata =
        mylite_execution_loaded_table_key_metadata_reference_init();
    const struct loaded_index_info *indexes = NULL;
    struct loaded_foreign_key_info *foreign_keys = NULL;
    size_t column_count = 0U;
    size_t index_count = 0U;
    size_t foreign_key_count = 0U;
    int rc = mylite_execution_acquire_table_columns(
        database,
        table->table_id,
        &columns,
        &column_count,
        &columns_cache_entry
    );

    if (rc == MYLITE_OK) {
        rc = mylite_execution_acquire_table_key_metadata(
            database,
            table->table_id,
            columns,
            column_count,
            &key_metadata
        );
        if (rc == MYLITE_OK) {
            indexes = key_metadata.metadata->indexes;
            index_count = key_metadata.metadata->index_count;
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_foreign_key_infos(
            database,
            table->table_id,
            columns,
            column_count,
            &foreign_keys,
            &foreign_key_count
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < index_count; ++index) {
        if (!indexes[index].index.is_unique) {
            continue;
        }
        rc = append_information_schema_key_column_usage_index_row(
            database,
            rows,
            schema,
            table,
            &indexes[index]
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_information_schema_key_column_usage_foreign_key_rows(
            database,
            rows,
            schema,
            table,
            foreign_keys,
            foreign_key_count
        );
    }

    mylite_execution_loaded_foreign_key_infos_deinit(&foreign_keys, &foreign_key_count);
    mylite_execution_loaded_table_key_metadata_reference_deinit(&key_metadata);
    mylite_execution_release_table_columns(columns_cache_entry, columns);
    return rc;
}

static int append_information_schema_key_column_usage_index_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_index_info *index
) {
    char ordinal_text[integer_text_capacity];
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < index->part_count; ++part_index) {
        const struct loaded_index_part *part = &index->parts[part_index];
        const char *values[information_schema_key_column_usage_column_count] = {
            "def",
            schema->name,
            index->index.name,
            "def",
            schema->name,
            table->name,
            part->column.name,
            ordinal_text,
            NULL,
            NULL,
            NULL,
            NULL,
        };

        rc = information_schema_format_i64(
            database,
            part->index_column.ordinal_position,
            ordinal_text,
            sizeof(ordinal_text)
        );
        if (rc == MYLITE_OK) {
            rc = append_information_schema_row(database, rows, values);
        }
    }

    return rc;
}

static int append_information_schema_key_column_usage_foreign_key_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_foreign_key_info *foreign_keys,
    size_t foreign_key_count
) {
    bool *emitted = NULL;
    int rc = MYLITE_OK;

    if (foreign_key_count == 0U) {
        return MYLITE_OK;
    }
    if (foreign_key_count > SIZE_MAX / sizeof(*emitted)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    emitted = calloc(foreign_key_count, sizeof(*emitted));
    if (emitted == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    while (rc == MYLITE_OK) {
        size_t best_index = SIZE_MAX;

        for (size_t index = 0U; index < foreign_key_count; ++index) {
            if (emitted[index]) {
                continue;
            }
            if (best_index == SIZE_MAX || compare_ascii_case_insensitive_text(
                                              foreign_keys[index].foreign_key.name,
                                              foreign_keys[best_index].foreign_key.name
                                          ) < 0) {
                best_index = index;
            }
        }
        if (best_index == SIZE_MAX) {
            break;
        }

        emitted[best_index] = true;
        rc = append_information_schema_key_column_usage_foreign_key_row(
            database,
            rows,
            schema,
            table,
            &foreign_keys[best_index]
        );
    }

    free(emitted);
    return rc;
}

static int append_information_schema_key_column_usage_foreign_key_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_foreign_key_info *foreign_key
) {
    char ordinal_text[integer_text_capacity];
    char referenced_ordinal_text[integer_text_capacity];
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < foreign_key->part_count;
         ++part_index) {
        const struct loaded_foreign_key_part *part = &foreign_key->parts[part_index];
        const char *values[information_schema_key_column_usage_column_count] = {
            "def",
            schema->name,
            foreign_key->foreign_key.name,
            "def",
            schema->name,
            table->name,
            part->child_column.name,
            ordinal_text,
            referenced_ordinal_text,
            schema->name,
            foreign_key->parent_table.name,
            part->parent_column.name,
        };

        rc = information_schema_format_i64(
            database,
            part->foreign_key_column.ordinal_position,
            ordinal_text,
            sizeof(ordinal_text)
        );
        if (rc == MYLITE_OK) {
            rc = information_schema_format_i64(
                database,
                part->foreign_key_column.position_in_unique_constraint,
                referenced_ordinal_text,
                sizeof(referenced_ordinal_text)
            );
        }
        if (rc == MYLITE_OK) {
            rc = append_information_schema_row(database, rows, values);
        }
    }

    return rc;
}

static int append_information_schema_key_column_usage_mysql_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_mysql_system_table_definition_count();
         ++index) {
        rc = append_information_schema_key_column_usage_mysql_system_table_rows(
            database,
            rows,
            mylite_execution_catalog_mysql_system_table_definition_at(index)
        );
    }

    return rc;
}

static int append_information_schema_key_column_usage_mysql_system_table_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_execution_catalog_mysql_system_table *definition
) {
    int rc = MYLITE_OK;
    size_t primary_key_column_count = mysql_system_table_primary_key_column_count(definition);

    if (definition == NULL || definition->column_keys == NULL ||
        definition->column_key_count < definition->query_definition.column_count) {
        return MYLITE_OK;
    }

    for (size_t key_index = 0U; rc == MYLITE_OK && key_index < primary_key_column_count;
         ++key_index) {
        const struct mylite_execution_catalog_column_definition *column =
            mysql_system_table_primary_key_column(definition, key_index);
        char sequence_text[integer_text_capacity];

        if (column == NULL) {
            set_runtime_error(database, "invalid mysql system table primary key");
            return MYLITE_ERROR;
        }

        const char *values[information_schema_key_column_usage_column_count] = {
            "def",
            definition->schema_name,
            "PRIMARY",
            "def",
            definition->schema_name,
            definition->query_definition.name,
            column->name,
            sequence_text,
            NULL,
            NULL,
            NULL,
            NULL,
        };

        rc = information_schema_format_i64(
            database,
            (int64_t)key_index + 1,
            sequence_text,
            sizeof(sequence_text)
        );
        if (rc == MYLITE_OK) {
            rc = append_information_schema_row(database, rows, values);
        }
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < definition->secondary_index_count; ++index) {
        const struct mylite_execution_catalog_mysql_system_secondary_index *secondary_index =
            &definition->secondary_indexes[index];
        char sequence_text[integer_text_capacity];

        if (!secondary_index->is_unique) {
            continue;
        }
        if (secondary_index->column_index >= definition->query_definition.column_count) {
            set_runtime_error(database, "invalid mysql system table unique key");
            return MYLITE_ERROR;
        }

        const struct mylite_execution_catalog_column_definition *column =
            &definition->query_definition.columns[secondary_index->column_index];
        const char *values[information_schema_key_column_usage_column_count] = {
            "def",
            definition->schema_name,
            secondary_index->name,
            "def",
            definition->schema_name,
            definition->query_definition.name,
            column->name,
            sequence_text,
            NULL,
            NULL,
            NULL,
            NULL,
        };

        rc = information_schema_format_i64(
            database,
            (int64_t)mysql_system_table_secondary_index_sequence(definition, index),
            sequence_text,
            sizeof(sequence_text)
        );
        if (rc == MYLITE_OK) {
            rc = append_information_schema_row(database, rows, values);
        }
    }

    return rc;
}

static int append_information_schema_referential_constraints_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_table_columns_cache_entry *columns_cache_entry = NULL;
    struct loaded_foreign_key_info *foreign_keys = NULL;
    size_t column_count = 0U;
    size_t foreign_key_count = 0U;
    int rc = mylite_execution_acquire_table_columns(
        database,
        table->table_id,
        &columns,
        &column_count,
        &columns_cache_entry
    );

    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_foreign_key_infos(
            database,
            table->table_id,
            columns,
            column_count,
            &foreign_keys,
            &foreign_key_count
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < foreign_key_count; ++index) {
        rc = append_information_schema_referential_constraints_foreign_key_row(
            database,
            rows,
            schema,
            table,
            &foreign_keys[index]
        );
    }

    mylite_execution_loaded_foreign_key_infos_deinit(&foreign_keys, &foreign_key_count);
    mylite_execution_release_table_columns(columns_cache_entry, columns);
    return rc;
}

static int append_information_schema_referential_constraints_foreign_key_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_foreign_key_info *foreign_key
) {
    const char *values[information_schema_referential_constraints_column_count] = {
        "def",
        schema->name,
        foreign_key->foreign_key.name,
        "def",
        schema->name,
        foreign_key->parent_index.name,
        foreign_key->foreign_key.match_option,
        foreign_key->foreign_key.update_rule,
        foreign_key->foreign_key.delete_rule,
        table->name,
        foreign_key->parent_table.name,
    };

    return append_information_schema_row(database, rows, values);
}
