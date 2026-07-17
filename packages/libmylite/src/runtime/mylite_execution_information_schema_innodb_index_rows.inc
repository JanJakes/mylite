static int append_information_schema_innodb_indexes_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_table_columns_cache_entry *columns_cache_entry = NULL;
    struct loaded_table_key_metadata_reference key_metadata =
        mylite_execution_loaded_table_key_metadata_reference_init();
    const struct loaded_index_info *indexes = NULL;
    const struct loaded_index_info *clustered_index = NULL;
    size_t column_count = 0U;
    size_t index_count = 0U;
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_innodb_indexes_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_INDEXES columns");
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
        clustered_index = information_schema_innodb_clustered_index(indexes, index_count);
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < index_count; ++index) {
        rc = append_information_schema_innodb_index_row(
            database,
            rows,
            table,
            column_count,
            clustered_index,
            &indexes[index]
        );
    }
    if (rc == MYLITE_OK && clustered_index == NULL) {
        rc = append_information_schema_innodb_generated_cluster_index_row(
            database,
            rows,
            table,
            column_count
        );
    }

    mylite_execution_loaded_table_key_metadata_reference_deinit(&key_metadata);
    mylite_execution_release_table_columns(columns_cache_entry, columns);
    return rc;
}

static int append_information_schema_innodb_index_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_table_descriptor *table,
    size_t column_count,
    const struct loaded_index_info *clustered_index,
    const struct loaded_index_info *index
) {
    char index_id_text[integer_text_capacity];
    char table_id_text[integer_text_capacity];
    char type_text[integer_text_capacity];
    char field_count_text[integer_text_capacity];
    char page_no_text[integer_text_capacity];
    char space_text[integer_text_capacity];
    char merge_threshold_text[integer_text_capacity];
    int64_t type = 0;
    int64_t field_count = 0;
    int64_t page_no = information_schema_innodb_index_default_page_no;
    int rc = MYLITE_OK;

    if (index->index.kind == MYLITE_CATALOG_INDEX_KIND_FULLTEXT) {
        page_no = information_schema_innodb_index_fulltext_page_no;
    }

    rc = information_schema_format_i64(
        database,
        index->index.index_id,
        index_id_text,
        sizeof(index_id_text)
    );
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            table->table_id,
            table_id_text,
            sizeof(table_id_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_innodb_index_type(database, index, clustered_index, &type);
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(database, type, type_text, sizeof(type_text));
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_innodb_index_field_count(
            database,
            index,
            column_count,
            clustered_index,
            &field_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            field_count,
            field_count_text,
            sizeof(field_count_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(database, page_no, page_no_text, sizeof(page_no_text));
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            information_schema_innodb_index_space,
            space_text,
            sizeof(space_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            information_schema_innodb_index_merge_threshold,
            merge_threshold_text,
            sizeof(merge_threshold_text)
        );
    }
    if (rc == MYLITE_OK) {
        const char *values[information_schema_innodb_indexes_column_count] = {
            index_id_text,
            index->index.name,
            table_id_text,
            type_text,
            field_count_text,
            page_no_text,
            space_text,
            merge_threshold_text,
        };

        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}

static int append_information_schema_innodb_generated_cluster_index_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_table_descriptor *table,
    size_t column_count
) {
    char index_id_text[integer_text_capacity];
    char table_id_text[integer_text_capacity];
    char type_text[integer_text_capacity];
    char field_count_text[integer_text_capacity];
    char page_no_text[integer_text_capacity];
    char space_text[integer_text_capacity];
    char merge_threshold_text[integer_text_capacity];
    int64_t index_id = 0;
    int64_t field_count = 0;
    int rc =
        information_schema_innodb_generated_cluster_index_id(database, table->table_id, &index_id);

    if (rc == MYLITE_OK) {
        rc = information_schema_innodb_generated_cluster_field_count(
            database,
            column_count,
            &field_count
        );
    }
    if (rc == MYLITE_OK) {
        rc =
            information_schema_format_i64(database, index_id, index_id_text, sizeof(index_id_text));
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
            information_schema_innodb_index_generated_cluster_type,
            type_text,
            sizeof(type_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            field_count,
            field_count_text,
            sizeof(field_count_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            information_schema_innodb_index_default_page_no,
            page_no_text,
            sizeof(page_no_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            information_schema_innodb_index_space,
            space_text,
            sizeof(space_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            information_schema_innodb_index_merge_threshold,
            merge_threshold_text,
            sizeof(merge_threshold_text)
        );
    }
    if (rc == MYLITE_OK) {
        const char *values[information_schema_innodb_indexes_column_count] = {
            index_id_text,
            information_schema_innodb_generated_cluster_index_name,
            table_id_text,
            type_text,
            field_count_text,
            page_no_text,
            space_text,
            merge_threshold_text,
        };

        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}

static int append_information_schema_innodb_fields_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_table_columns_cache_entry *columns_cache_entry = NULL;
    struct loaded_table_key_metadata_reference key_metadata =
        mylite_execution_loaded_table_key_metadata_reference_init();
    const struct loaded_index_info *indexes = NULL;
    size_t column_count = 0U;
    size_t index_count = 0U;
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_innodb_fields_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_FIELDS columns");
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
    for (size_t index = 0U; rc == MYLITE_OK && index < index_count; ++index) {
        rc = append_information_schema_innodb_fields_index_rows(database, rows, &indexes[index]);
    }

    mylite_execution_loaded_table_key_metadata_reference_deinit(&key_metadata);
    mylite_execution_release_table_columns(columns_cache_entry, columns);
    return rc;
}

static int append_information_schema_innodb_fields_index_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct loaded_index_info *index
) {
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < index->part_count; ++part_index) {
        rc = append_information_schema_innodb_fields_part_row(
            database,
            rows,
            index,
            &index->parts[part_index]
        );
    }
    return rc;
}

static int append_information_schema_innodb_fields_part_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct loaded_index_info *index,
    const struct loaded_index_part *part
) {
    char index_id_text[integer_text_capacity];
    char position_text[integer_text_capacity];
    int rc = MYLITE_OK;

    if (part->index_column.ordinal_position < information_schema_innodb_field_position_offset) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_FIELDS position");
        return MYLITE_ERROR;
    }

    rc = information_schema_format_i64(
        database,
        index->index.index_id,
        index_id_text,
        sizeof(index_id_text)
    );
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            part->index_column.ordinal_position - information_schema_innodb_field_position_offset,
            position_text,
            sizeof(position_text)
        );
    }
    if (rc == MYLITE_OK) {
        const char *values[information_schema_innodb_fields_column_count] = {
            index_id_text,
            part->column.name,
            position_text,
        };

        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}

static int information_schema_innodb_index_type(
    struct mylite_db *database,
    const struct loaded_index_info *index,
    const struct loaded_index_info *clustered_index,
    int64_t *out_type
) {
    switch (index->index.kind) {
    case MYLITE_CATALOG_INDEX_KIND_PRIMARY:
        *out_type = information_schema_innodb_index_primary_type;
        return MYLITE_OK;
    case MYLITE_CATALOG_INDEX_KIND_SECONDARY:
        if (index == clustered_index) {
            *out_type = information_schema_innodb_index_primary_type;
        } else {
            *out_type = index->index.is_unique ? information_schema_innodb_index_unique_type
                                               : information_schema_innodb_index_nonunique_type;
        }
        return MYLITE_OK;
    case MYLITE_CATALOG_INDEX_KIND_FULLTEXT:
        *out_type = information_schema_innodb_index_fulltext_type;
        return MYLITE_OK;
    case MYLITE_CATALOG_INDEX_KIND_SPATIAL:
        *out_type = information_schema_innodb_index_spatial_type;
        return MYLITE_OK;
    case MYLITE_CATALOG_INDEX_KIND_INVALID:
        break;
    }

    set_runtime_error(database, "unknown INFORMATION_SCHEMA.INNODB_INDEXES index type");
    return MYLITE_ERROR;
}

static int information_schema_innodb_index_field_count(
    struct mylite_db *database,
    const struct loaded_index_info *index,
    size_t column_count,
    const struct loaded_index_info *clustered_index,
    int64_t *out_field_count
) {
    size_t clustered_part_count = 0U;
    size_t field_count = index->part_count;
    int rc = MYLITE_OK;

    if (index->index.kind == MYLITE_CATALOG_INDEX_KIND_FULLTEXT) {
        if (index->part_count > (size_t)INT64_MAX) {
            set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_INDEXES field count");
            return MYLITE_ERROR;
        }
        *out_field_count = (int64_t)index->part_count;
        return MYLITE_OK;
    }
    if (index == clustered_index) {
        if (column_count >
            (size_t)INT64_MAX - information_schema_innodb_cluster_system_field_count) {
            set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_INDEXES field count");
            return MYLITE_ERROR;
        }
        *out_field_count =
            (int64_t)(column_count + information_schema_innodb_cluster_system_field_count);
        return MYLITE_OK;
    }

    rc = information_schema_innodb_secondary_clustered_part_count(
        database,
        index,
        clustered_index,
        &clustered_part_count
    );
    if (rc == MYLITE_OK) {
        if (field_count > (size_t)INT64_MAX - clustered_part_count) {
            set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_INDEXES field count");
            return MYLITE_ERROR;
        }
        field_count += clustered_part_count;
        *out_field_count = (int64_t)field_count;
    }
    return rc;
}

static int information_schema_innodb_secondary_clustered_part_count(
    struct mylite_db *database,
    const struct loaded_index_info *index,
    const struct loaded_index_info *clustered_index,
    size_t *out_count
) {
    size_t count = 0U;

    if (clustered_index == NULL) {
        *out_count = information_schema_innodb_generated_cluster_key_part_count;
        return MYLITE_OK;
    }

    for (size_t part_index = 0U; part_index < clustered_index->part_count; ++part_index) {
        if (!information_schema_innodb_index_contains_column(
                index,
                clustered_index->parts[part_index].column.column_id
            )) {
            if (count == SIZE_MAX) {
                set_runtime_error(
                    database,
                    "invalid INFORMATION_SCHEMA.INNODB_INDEXES field count"
                );
                return MYLITE_ERROR;
            }
            ++count;
        }
    }

    *out_count = count;
    return MYLITE_OK;
}

static int information_schema_innodb_generated_cluster_field_count(
    struct mylite_db *database,
    size_t column_count,
    int64_t *out_field_count
) {
    if (column_count >
        (size_t)INT64_MAX - information_schema_innodb_generated_cluster_system_field_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_INDEXES field count");
        return MYLITE_ERROR;
    }

    *out_field_count =
        (int64_t)(column_count + information_schema_innodb_generated_cluster_system_field_count);
    return MYLITE_OK;
}

static int information_schema_innodb_generated_cluster_index_id(
    struct mylite_db *database,
    int64_t table_id,
    int64_t *out_index_id
) {
    if (table_id < 0 ||
        table_id > INT64_MAX - information_schema_innodb_generated_cluster_index_id_base) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_INDEXES generated index id");
        return MYLITE_ERROR;
    }

    *out_index_id = information_schema_innodb_generated_cluster_index_id_base + table_id;
    return MYLITE_OK;
}

static const struct loaded_index_info *information_schema_innodb_clustered_index(
    const struct loaded_index_info *indexes,
    size_t index_count
) {
    for (size_t index = 0U; index < index_count; ++index) {
        if (indexes[index].index.kind == MYLITE_CATALOG_INDEX_KIND_PRIMARY) {
            return &indexes[index];
        }
    }
    for (size_t index = 0U; index < index_count; ++index) {
        if (information_schema_innodb_index_is_all_not_null_unique(&indexes[index])) {
            return &indexes[index];
        }
    }

    return NULL;
}

static bool information_schema_innodb_index_is_all_not_null_unique(
    const struct loaded_index_info *index
) {
    if (index->index.kind != MYLITE_CATALOG_INDEX_KIND_SECONDARY || !index->index.is_unique ||
        index->part_count == 0U) {
        return false;
    }
    for (size_t part_index = 0U; part_index < index->part_count; ++part_index) {
        if (index->parts[part_index].column.is_nullable) {
            return false;
        }
    }
    return true;
}

static bool information_schema_innodb_index_contains_column(
    const struct loaded_index_info *index,
    int64_t column_id
) {
    for (size_t part_index = 0U; part_index < index->part_count; ++part_index) {
        if (index->parts[part_index].column.column_id == column_id) {
            return true;
        }
    }
    return false;
}
