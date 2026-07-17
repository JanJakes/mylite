static int append_information_schema_statistics_mysql_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_mysql_system_table_definition_count();
         ++index) {
        rc = append_information_schema_statistics_mysql_system_table_rows(
            database,
            rows,
            mylite_execution_catalog_mysql_system_table_definition_at(index)
        );
    }

    return rc;
}

static int append_information_schema_statistics_mysql_system_table_rows(
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

        const char *values[information_schema_statistics_column_count] = {
            "def",
            definition->schema_name,
            definition->query_definition.name,
            "0",
            definition->schema_name,
            "PRIMARY",
            sequence_text,
            column->name,
            mysql_system_table_primary_key_collation(definition, key_index + 1),
            mysql_system_table_primary_key_cardinality(definition, key_index + 1),
            NULL,
            NULL,
            mysql_system_table_index_nullable_value(column),
            mysql_system_table_primary_key_index_type(definition),
            "",
            "",
            "YES",
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

        if (secondary_index->column_index >= definition->query_definition.column_count) {
            set_runtime_error(database, "invalid mysql system table secondary index");
            return MYLITE_ERROR;
        }

        const struct mylite_execution_catalog_column_definition *column =
            &definition->query_definition.columns[secondary_index->column_index];
        const char *values[information_schema_statistics_column_count] = {
            "def",
            definition->schema_name,
            definition->query_definition.name,
            secondary_index->non_unique,
            definition->schema_name,
            secondary_index->name,
            sequence_text,
            column->name,
            secondary_index->collation,
            secondary_index->cardinality,
            NULL,
            NULL,
            mysql_system_table_index_nullable_value(column),
            secondary_index->index_type,
            "",
            "",
            "YES",
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

static int append_information_schema_statistics_base_rows(
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
    size_t column_count = 0U;
    size_t index_count = 0U;
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
    for (int group = index_display_group_primary;
         rc == MYLITE_OK && group <= index_display_group_fulltext;
         ++group) {
        for (size_t index = 0U; rc == MYLITE_OK && index < index_count; ++index) {
            if (index_display_group(&indexes[index]) != group) {
                continue;
            }
            rc = append_information_schema_statistics_base_index_row(
                database,
                rows,
                schema,
                table,
                &indexes[index]
            );
        }
    }

    mylite_execution_loaded_table_key_metadata_reference_deinit(&key_metadata);
    mylite_execution_release_table_columns(columns_cache_entry, columns);
    return rc;
}

static int append_information_schema_statistics_base_index_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_index_info *index
) {
    static const char cardinality[] = "0";
    char sequence[integer_text_capacity];
    const char *non_unique = "1";
    int rc = MYLITE_OK;

    if (index->index.is_unique) {
        non_unique = "0";
    }

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < index->part_count; ++part_index) {
        const struct loaded_index_part *part = &index->parts[part_index];
        char prefix_text[integer_text_capacity];
        const char *nullable = "";
        const bool is_fulltext = index->index.kind == MYLITE_CATALOG_INDEX_KIND_FULLTEXT;
        const bool is_spatial = index->index.kind == MYLITE_CATALOG_INDEX_KIND_SPATIAL;
        const char *collation =
            part->index_column.sort_direction == MYLITE_CATALOG_INDEX_SORT_DIRECTION_DESC ? "D"
                                                                                          : "A";
        const char *prefix_value = NULL;
        const char *index_type = "BTREE";

        if (part->column.is_nullable) {
            nullable = "YES";
        }
        if (is_fulltext) {
            collation = NULL;
            index_type = "FULLTEXT";
        }
        if (is_spatial) {
            collation = "A";
            prefix_value = "32";
            index_type = "SPATIAL";
        }
        if (!is_fulltext && !is_spatial) {
            rc = index_part_display_prefix_value(
                database,
                index,
                part,
                prefix_text,
                sizeof(prefix_text),
                &prefix_value
            );
            if (rc != MYLITE_OK) {
                return rc;
            }
        }
        const char *values[information_schema_statistics_column_count] = {
            "def",
            schema->name,
            table->name,
            non_unique,
            schema->name,
            index->index.name,
            sequence,
            part->column.name,
            collation,
            cardinality,
            prefix_value,
            NULL,
            nullable,
            index_type,
            "",
            index->index.comment,
            index_visibility_text(index->index.is_visible),
            NULL,
        };

        rc = information_schema_format_i64(
            database,
            part->index_column.ordinal_position,
            sequence,
            sizeof(sequence)
        );
        if (rc == MYLITE_OK) {
            rc = append_information_schema_row(database, rows, values);
        }
    }

    return rc;
}
