static int information_schema_append_result_columns(
    struct mylite_db *database,
    mylite_result *result,
    const struct information_schema_query *query
) {
    int rc = MYLITE_OK;

    if (query->is_count_star) {
        struct mylite_result_column_descriptor descriptor = {0};

        if (query->count_name == NULL) {
            set_runtime_error(database, "invalid INFORMATION_SCHEMA count descriptor");
            return MYLITE_ERROR;
        }
        information_schema_make_count_result_column_descriptor(query->count_name, &descriptor);
        rc = mylite_result_append_column_descriptor(result, &descriptor);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
        return rc;
    }

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < query->projection_count;
         ++column_index) {
        struct mylite_result_column_descriptor descriptor = {0};

        rc = information_schema_make_result_column_descriptor(
            database,
            query,
            column_index,
            &descriptor
        );
        if (rc == MYLITE_OK) {
            rc = mylite_result_append_column_descriptor(result, &descriptor);
        }
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
        }
    }
    return rc;
}

static int information_schema_make_result_column_descriptor(
    struct mylite_db *database,
    const struct information_schema_query *query,
    size_t projection_index,
    struct mylite_result_column_descriptor *out_descriptor
) {
    const char *label = NULL;

    if (database == NULL || query == NULL || out_descriptor == NULL ||
        projection_index >= query->projection_count) {
        return MYLITE_MISUSE;
    }
    label = query->projection_names[projection_index];
    switch (query->projection_kinds[projection_index]) {
    case INFORMATION_SCHEMA_PROJECTION_COLUMN:
        return information_schema_make_source_column_descriptor(
            database,
            query,
            projection_index,
            out_descriptor
        );
    case INFORMATION_SCHEMA_PROJECTION_UNSIGNED_INTEGER_EXPRESSION:
        information_schema_make_unsigned_result_column_descriptor(label, out_descriptor);
        return MYLITE_OK;
    case INFORMATION_SCHEMA_PROJECTION_INTEGER_LITERAL:
        *out_descriptor =
            query->projection_expressions[projection_index].integer_literal_descriptor;
        out_descriptor->label = label;
        return MYLITE_OK;
    case INFORMATION_SCHEMA_PROJECTION_GROUP_CONCAT_ORDERED_COLUMN:
        information_schema_make_group_concat_result_column_descriptor(
            database,
            label,
            out_descriptor
        );
        return MYLITE_OK;
    case INFORMATION_SCHEMA_PROJECTION_LOGICAL_NOT_COLUMN:
        information_schema_make_logical_result_column_descriptor(label, out_descriptor);
        return MYLITE_OK;
    }
    return MYLITE_MISUSE;
}

static int information_schema_make_source_column_descriptor(
    struct mylite_db *database,
    const struct information_schema_query *query,
    size_t projection_index,
    struct mylite_result_column_descriptor *out_descriptor
) {
    const size_t column_index = query->projection_indexes[projection_index];
    const struct mylite_execution_catalog_column_definition *column_definition = NULL;
    struct mylite_catalog_column_descriptor column = {0};
    struct result_column_metadata_context metadata_context = result_column_metadata_context_init();
    int rc = MYLITE_OK;

    if (query->definition == NULL || column_index >= query->definition->column_count) {
        return MYLITE_MISUSE;
    }
    column_definition = &query->definition->columns[column_index];
    rc = information_schema_initialize_catalog_column_descriptor(
        database,
        column_definition,
        &column
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    *out_descriptor = unknown_result_column_descriptor(query->projection_names[projection_index]);
    out_descriptor->schema_name =
        query->source_schema_name == NULL ? "" : query->source_schema_name;
    out_descriptor->table_name = query->has_alias ? query->alias : query->definition->name;
    out_descriptor->origin_schema_name = out_descriptor->schema_name;
    out_descriptor->origin_table_name = query->definition->name;
    out_descriptor->origin_column_name = column_definition->name;
    out_descriptor->nullable = column.is_nullable;
    rc = populate_select_result_column_descriptor(
        database,
        NULL,
        &column,
        &metadata_context,
        out_descriptor
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (information_schema_source_column_has_binary_numeric_flag(query, out_descriptor)) {
        out_descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_BINARY;
    }
    information_schema_apply_connection_text_metadata(database, column_definition, out_descriptor);
    information_schema_apply_temporal_precision_metadata(column_definition, out_descriptor);
    information_schema_apply_system_key_flags(query, column_index, out_descriptor);
    return MYLITE_OK;
}

static int information_schema_initialize_catalog_column_descriptor(
    struct mylite_db *database,
    const struct mylite_execution_catalog_column_definition *column_definition,
    struct mylite_catalog_column_descriptor *out_column
) {
    const char *logical_type = NULL;
    const char *physical_type = "TEXT";
    bool preserve_lowercase_logical_type = false;
    int rc = MYLITE_OK;

    if (column_definition == NULL || out_column == NULL) {
        return MYLITE_MISUSE;
    }
    logical_type = column_definition->column_type == NULL ? column_definition->data_type
                                                          : column_definition->column_type;
    if (column_definition->column_type != NULL &&
        strcmp(column_definition->column_type, "varchar(65535)") == 0) {
        logical_type = "LONGTEXT";
    }
    if (strcmp(column_definition->data_type, "date") == 0 ||
        strcmp(column_definition->data_type, "time") == 0 ||
        strcmp(column_definition->data_type, "datetime") == 0 ||
        strcmp(column_definition->data_type, "timestamp") == 0 ||
        strcmp(column_definition->data_type, "year") == 0) {
        logical_type = column_definition->data_type;
    }
    if (strcmp(column_definition->data_type, "tinyint") == 0 ||
        strcmp(column_definition->data_type, "smallint") == 0 ||
        strcmp(column_definition->data_type, "mediumint") == 0 ||
        strcmp(column_definition->data_type, "int") == 0 ||
        strcmp(column_definition->data_type, "bigint") == 0) {
        physical_type = "INTEGER";
    } else if (strcmp(column_definition->data_type, "float") == 0 ||
               strcmp(column_definition->data_type, "double") == 0) {
        physical_type = "REAL";
    } else if (strcmp(column_definition->data_type, "binary") == 0 ||
               strcmp(column_definition->data_type, "varbinary") == 0 ||
               strcmp(column_definition->data_type, "tinyblob") == 0 ||
               strcmp(column_definition->data_type, "blob") == 0 ||
               strcmp(column_definition->data_type, "mediumblob") == 0 ||
               strcmp(column_definition->data_type, "longblob") == 0 ||
               strcmp(column_definition->data_type, "bit") == 0) {
        physical_type = "BLOB";
    } else if (strcmp(column_definition->data_type, "geometry") == 0 ||
               strcmp(column_definition->data_type, "point") == 0 ||
               strcmp(column_definition->data_type, "linestring") == 0 ||
               strcmp(column_definition->data_type, "polygon") == 0 ||
               strcmp(column_definition->data_type, "multipoint") == 0 ||
               strcmp(column_definition->data_type, "multilinestring") == 0 ||
               strcmp(column_definition->data_type, "multipolygon") == 0 ||
               strcmp(column_definition->data_type, "geomcollection") == 0) {
        physical_type = "BLOB";
        preserve_lowercase_logical_type = true;
    }
    *out_column = (struct mylite_catalog_column_descriptor){
        .is_nullable = column_definition->is_nullable != NULL &&
                       strcmp(column_definition->is_nullable, "YES") == 0,
        .is_visible = true,
        .default_kind = MYLITE_CATALOG_COLUMN_DEFAULT_NONE,
    };
    rc = information_schema_copy_descriptor_text(
        database,
        column_definition->name,
        out_column->name,
        sizeof(out_column->name)
    );
    if (rc == MYLITE_OK) {
        rc = information_schema_copy_descriptor_text(
            database,
            logical_type,
            out_column->logical_type,
            sizeof(out_column->logical_type)
        );
    }
    if (!preserve_lowercase_logical_type) {
        for (size_t index = 0U; rc == MYLITE_OK && out_column->logical_type[index] != '\0';
             ++index) {
            if (out_column->logical_type[index] >= 'a' && out_column->logical_type[index] <= 'z') {
                out_column->logical_type[index] =
                    (char)(out_column->logical_type[index] - 'a' + 'A');
            }
        }
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_copy_descriptor_text(
            database,
            physical_type,
            out_column->physical_type,
            sizeof(out_column->physical_type)
        );
    }
    if (rc == MYLITE_OK && column_definition->character_set_name != NULL) {
        rc = information_schema_copy_descriptor_text(
            database,
            column_definition->character_set_name,
            out_column->character_set_name,
            sizeof(out_column->character_set_name)
        );
    }
    if (rc == MYLITE_OK && column_definition->collation_name != NULL) {
        rc = information_schema_copy_descriptor_text(
            database,
            column_definition->collation_name,
            out_column->collation_name,
            sizeof(out_column->collation_name)
        );
    }
    return rc;
}

static int information_schema_copy_descriptor_text(
    struct mylite_db *database,
    const char *source,
    char *destination,
    size_t destination_size
) {
    size_t source_size = 0U;

    if (source == NULL || destination == NULL || destination_size == 0U) {
        return MYLITE_MISUSE;
    }
    source_size = strlen(source);
    if (source_size >= destination_size) {
        set_runtime_error(database, "catalog result metadata exceeds descriptor capacity");
        return MYLITE_ERROR;
    }
    memcpy(destination, source, source_size + 1U);
    return MYLITE_OK;
}

static void information_schema_apply_connection_text_metadata(
    const struct mylite_db *database,
    const struct mylite_execution_catalog_column_definition *column_definition,
    struct mylite_result_column_descriptor *descriptor
) {
    struct mylite_sql_source_span length_span = {0};
    struct mylite_sql_source_span octet_length_span = {0};
    uint64_t character_count = 0U;
    uint64_t octet_length = 0U;
    uint64_t max_bytes_per_character = 0U;
    uint64_t source_max_bytes_per_character = 0U;

    if (database == NULL || column_definition == NULL || descriptor == NULL ||
        column_definition->character_set_name == NULL ||
        text_equals_ascii_case_insensitive(column_definition->character_set_name, "binary") ||
        column_definition->character_maximum_length == NULL) {
        return;
    }
    length_span = (struct mylite_sql_source_span){
        .text = column_definition->character_maximum_length,
        .length = strlen(column_definition->character_maximum_length),
    };
    if (parse_unsigned_integer_literal(&length_span, &character_count) != MYLITE_OK) {
        return;
    }
    max_bytes_per_character = result_metadata_collation_max_bytes_per_character(
        mylite_connection_statement_collation_connection(database)
    );
    descriptor->charset_id =
        result_metadata_collation_id(mylite_connection_statement_collation_connection(database));
    descriptor->collation_id = descriptor->charset_id;
    if (column_definition->column_type != NULL &&
        strcmp(column_definition->column_type, "varchar(65535)") == 0 &&
        column_definition->character_octet_length != NULL) {
        octet_length_span = (struct mylite_sql_source_span){
            .text = column_definition->character_octet_length,
            .length = strlen(column_definition->character_octet_length),
        };
        source_max_bytes_per_character =
            character_set_max_bytes_per_character(column_definition->character_set_name);
        if (parse_unsigned_integer_literal(&octet_length_span, &octet_length) == MYLITE_OK &&
            source_max_bytes_per_character != 0U &&
            octet_length <= UINT64_MAX / source_max_bytes_per_character &&
            octet_length * source_max_bytes_per_character <= UINT64_MAX / max_bytes_per_character) {
            descriptor->display_length = result_metadata_display_length_cap(
                octet_length * source_max_bytes_per_character * max_bytes_per_character
            );
            return;
        }
    }
    if (character_count > UINT64_MAX / max_bytes_per_character) {
        descriptor->display_length = UINT32_MAX;
    } else {
        descriptor->display_length =
            result_metadata_display_length_cap(character_count * max_bytes_per_character);
    }
}

static void information_schema_apply_temporal_precision_metadata(
    const struct mylite_execution_catalog_column_definition *column_definition,
    struct mylite_result_column_descriptor *descriptor
) {
    struct mylite_sql_source_span precision_span = {0};
    uint64_t precision = 0U;

    if (column_definition == NULL || descriptor == NULL ||
        column_definition->datetime_precision == NULL) {
        return;
    }
    precision_span = (struct mylite_sql_source_span){
        .text = column_definition->datetime_precision,
        .length = strlen(column_definition->datetime_precision),
    };
    if (parse_unsigned_integer_literal(&precision_span, &precision) != MYLITE_OK ||
        precision > UINT16_MAX) {
        return;
    }
    descriptor->decimals = (uint16_t)precision;
    if (precision == 0U) {
        return;
    }
    if (descriptor->logical_type == MYLITE_RESULT_LOGICAL_TYPE_TIME) {
        descriptor->display_length = time_text_maximum_length + precision + 1U;
    } else if (descriptor->logical_type == MYLITE_RESULT_LOGICAL_TYPE_DATETIME ||
               descriptor->logical_type == MYLITE_RESULT_LOGICAL_TYPE_TIMESTAMP) {
        descriptor->display_length = datetime_text_length + precision + 1U;
    }
}

static void information_schema_apply_system_key_flags(
    const struct information_schema_query *query,
    size_t column_index,
    struct mylite_result_column_descriptor *descriptor
) {
    const char *key = NULL;

    if (query == NULL || query->system_table_definition == NULL || descriptor == NULL ||
        column_index >= query->system_table_definition->column_key_count) {
        return;
    }
    key = query->system_table_definition->column_keys[column_index];
    if (key == NULL || key[0] == '\0') {
        return;
    }
    if (strcmp(key, "PRI") == 0) {
        descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_PRI_KEY;
    } else if (strcmp(key, "UNI") == 0) {
        descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_UNIQUE_KEY;
    } else if (strcmp(key, "MUL") == 0) {
        descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_MULTIPLE_KEY;
    }
    descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_PART_KEY;
}

static bool information_schema_source_column_has_binary_numeric_flag(
    const struct information_schema_query *query,
    const struct mylite_result_column_descriptor *descriptor
) {
    if (query == NULL || query->definition == NULL || query->source_schema_name == NULL ||
        descriptor == NULL || descriptor->charset_id != mysql_collation_binary_id) {
        return false;
    }
    return strcmp(query->source_schema_name, "information_schema") == 0 &&
           strcmp(query->definition->name, "TABLES") == 0;
}

static void information_schema_make_count_result_column_descriptor(
    const char *label,
    struct mylite_result_column_descriptor *out_descriptor
) {
    *out_descriptor = unknown_result_column_descriptor(label);
    out_descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_LONGLONG;
    out_descriptor->flags = MYLITE_RESULT_COLUMN_FLAG_NOT_NULL | MYLITE_RESULT_COLUMN_FLAG_BINARY |
                            MYLITE_RESULT_COLUMN_FLAG_NUM;
    out_descriptor->charset_id = mysql_collation_binary_id;
    out_descriptor->collation_id = mysql_collation_binary_id;
    out_descriptor->display_length = mysql_scalar_bigint_display_length;
    out_descriptor->nullable = false;
}

static void information_schema_make_unsigned_result_column_descriptor(
    const char *label,
    struct mylite_result_column_descriptor *out_descriptor
) {
    *out_descriptor = unknown_result_column_descriptor(label);
    out_descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_LONGLONG;
    out_descriptor->flags = MYLITE_RESULT_COLUMN_FLAG_UNSIGNED | MYLITE_RESULT_COLUMN_FLAG_BINARY |
                            MYLITE_RESULT_COLUMN_FLAG_NUM;
    out_descriptor->charset_id = mysql_collation_binary_id;
    out_descriptor->collation_id = mysql_collation_binary_id;
    out_descriptor->display_length = mysql_scalar_bigint_display_length;
}

static void information_schema_make_group_concat_result_column_descriptor(
    const struct mylite_db *database,
    const char *label,
    struct mylite_result_column_descriptor *out_descriptor
) {
    const uint64_t max_bytes_per_character = result_metadata_collation_max_bytes_per_character(
        mylite_connection_statement_collation_connection(database)
    );

    *out_descriptor = unknown_result_column_descriptor(label);
    out_descriptor->logical_type =
        database->session.group_concat_max_len > mysql_group_concat_blob_type_threshold
            ? MYLITE_RESULT_LOGICAL_TYPE_BLOB
            : MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING;
    out_descriptor->charset_id =
        result_metadata_collation_id(mylite_connection_statement_collation_connection(database));
    out_descriptor->collation_id = out_descriptor->charset_id;
    if (database->session.group_concat_max_len > UINT64_MAX / max_bytes_per_character) {
        out_descriptor->display_length = UINT32_MAX;
    } else {
        out_descriptor->display_length = result_metadata_display_length_cap(
            database->session.group_concat_max_len * max_bytes_per_character
        );
    }
    if (out_descriptor->logical_type == MYLITE_RESULT_LOGICAL_TYPE_BLOB) {
        out_descriptor->flags |= MYLITE_RESULT_COLUMN_FLAG_BLOB;
    }
}

static void information_schema_make_logical_result_column_descriptor(
    const char *label,
    struct mylite_result_column_descriptor *out_descriptor
) {
    *out_descriptor = unknown_result_column_descriptor(label);
    out_descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_LONGLONG;
    out_descriptor->flags = MYLITE_RESULT_COLUMN_FLAG_BINARY | MYLITE_RESULT_COLUMN_FLAG_NUM;
    out_descriptor->charset_id = mysql_collation_binary_id;
    out_descriptor->collation_id = mysql_collation_binary_id;
    out_descriptor->display_length = mysql_tinyint_bool_display_length;
}

static int information_schema_append_result_rows(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    mylite_result *result,
    size_t *out_read_row_count
) {
    struct information_schema_row_index_set indexes = {0};
    size_t visible_count = 0U;
    struct information_schema_result_projection_buffers buffers = {0};
    int rc = MYLITE_OK;

    if (out_read_row_count != NULL) {
        *out_read_row_count = 0U;
    }
    if (information_schema_result_rows_can_stream(query)) {
        return information_schema_append_streamed_result_rows(
            database,
            query,
            rows,
            result,
            out_read_row_count
        );
    }

    rc = information_schema_matching_row_indexes(database, query, rows, &indexes);
    if (rc == MYLITE_OK && indexes.count == 0U) {
        information_schema_row_index_set_deinit(&indexes);
        return MYLITE_OK;
    }
    if (rc == MYLITE_OK && rows->rows == NULL) {
        information_schema_row_index_set_deinit(&indexes);
        set_runtime_error(database, "information_schema row set is inconsistent");
        return MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_sort_row_indexes(
            database,
            query,
            rows,
            indexes.items,
            indexes.count
        );
    }
    if (rc == MYLITE_OK && query->projection_count != 0U) {
        rc = information_schema_result_projection_buffers_init(database, query, &buffers);
    }
    for (size_t output_index = 0U; rc == MYLITE_OK && output_index < indexes.count;
         ++output_index) {
        size_t row_index = indexes.items[output_index];

        if (query->limit.has_offset && (uint64_t)output_index < (uint64_t)query->limit.offset) {
            continue;
        }
        if (query->limit.has_limit && (uint64_t)visible_count >= (uint64_t)query->limit.row_count) {
            break;
        }
        rc = information_schema_append_projected_result_row(
            database,
            query,
            (const char *const *)rows->rows[row_index],
            &buffers,
            result
        );
        ++visible_count;
    }
    information_schema_result_projection_buffers_deinit(&buffers);
    if (rc == MYLITE_OK && out_read_row_count != NULL) {
        *out_read_row_count = visible_count;
    }
    information_schema_row_index_set_deinit(&indexes);
    return rc;
}

static int information_schema_append_streamed_result_rows(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    mylite_result *result,
    size_t *out_read_row_count
) {
    size_t visible_count = 0U;
    struct information_schema_result_projection_buffers buffers = {0};
    int rc = MYLITE_OK;

    if (rows->row_count == 0U) {
        return MYLITE_OK;
    }
    if (rows->rows == NULL) {
        set_runtime_error(database, "information_schema row set is inconsistent");
        return MYLITE_ERROR;
    }
    if (query->projection_count != 0U) {
        rc = information_schema_result_projection_buffers_init(database, query, &buffers);
    }
    for (size_t row_index = 0U; rc == MYLITE_OK && row_index < rows->row_count; ++row_index) {
        if (!rows->offset_applied && query->limit.has_offset &&
            (uint64_t)row_index < (uint64_t)query->limit.offset) {
            continue;
        }
        if (query->limit.has_limit && (uint64_t)visible_count >= (uint64_t)query->limit.row_count) {
            break;
        }
        rc = information_schema_append_projected_result_row(
            database,
            query,
            (const char *const *)rows->rows[row_index],
            &buffers,
            result
        );
        ++visible_count;
    }
    information_schema_result_projection_buffers_deinit(&buffers);
    if (rc == MYLITE_OK && out_read_row_count != NULL) {
        *out_read_row_count = visible_count;
    }
    return rc;
}

static bool information_schema_result_rows_can_stream(const struct information_schema_query *query
) {
    return query != NULL && query->definition != NULL && query->predicate.instruction_count == 0U &&
           !query->has_order && query->definition->kind != MYLITE_EXECUTION_CATALOG_TABLE_TABLES;
}

static int information_schema_append_grouped_statistics_result_rows(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    mylite_result *result,
    size_t *out_read_row_count
) {
    struct information_schema_row_index_set indexes = {0};
    size_t grouped_row_count = 0U;
    size_t visible_count = 0U;
    struct information_schema_result_projection_buffers buffers = {0};
    int rc = information_schema_matching_row_indexes(database, query, rows, &indexes);

    if (out_read_row_count != NULL) {
        *out_read_row_count = 0U;
    }
    if (rc == MYLITE_OK && indexes.count == 0U) {
        information_schema_row_index_set_deinit(&indexes);
        return MYLITE_OK;
    }
    if (rc == MYLITE_OK && rows->rows == NULL) {
        information_schema_row_index_set_deinit(&indexes);
        set_runtime_error(database, "information_schema row set is inconsistent");
        return MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_sort_grouped_statistics_row_indexes(
            database,
            query,
            rows,
            indexes.items,
            indexes.count
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_result_projection_buffers_init(database, query, &buffers);
    }
    for (size_t start_index = 0U; rc == MYLITE_OK && start_index < indexes.count;) {
        size_t end_index = start_index + 1U;

        while (end_index < indexes.count && information_schema_grouped_statistics_rows_same_group(
                                                query,
                                                rows,
                                                indexes.items[start_index],
                                                indexes.items[end_index]
                                            )) {
            ++end_index;
        }
        if (query->limit.has_offset &&
            (uint64_t)grouped_row_count < (uint64_t)query->limit.offset) {
            ++grouped_row_count;
            start_index = end_index;
            continue;
        }
        if (!query->limit.has_limit || (uint64_t)visible_count < (uint64_t)query->limit.row_count) {
            rc = information_schema_append_grouped_statistics_projected_row(
                database,
                query,
                rows,
                indexes.items,
                start_index,
                end_index,
                &buffers,
                result
            );
            ++visible_count;
        }
        ++grouped_row_count;
        start_index = end_index;
    }
    information_schema_result_projection_buffers_deinit(&buffers);
    if (rc == MYLITE_OK && out_read_row_count != NULL) {
        *out_read_row_count = visible_count;
    }
    information_schema_row_index_set_deinit(&indexes);
    return rc;
}

static int information_schema_sort_grouped_statistics_row_indexes(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    size_t *indexes,
    size_t index_count
) {
    const struct information_schema_row_index_sort sort = {
        .query = query,
        .rows = rows,
        .kind = INFORMATION_SCHEMA_ROW_INDEX_SORT_GROUPED_STATISTICS,
    };

    return information_schema_sort_row_indexes_by(database, &sort, indexes, index_count);
}

static int information_schema_sort_row_indexes_by(
    struct mylite_db *database,
    const struct information_schema_row_index_sort *sort,
    size_t *indexes,
    size_t index_count
) {
    enum { insertion_sort_threshold = 32 };

    size_t *scratch = NULL;

    if (sort == NULL || indexes == NULL) {
        return MYLITE_MISUSE;
    }
    if (index_count < 2U) {
        return MYLITE_OK;
    }
    if (index_count <= insertion_sort_threshold) {
        information_schema_insertion_sort_row_indexes_by(sort, indexes, index_count);
        return MYLITE_OK;
    }
    if (index_count > SIZE_MAX / sizeof(*scratch)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    scratch = malloc(index_count * sizeof(*scratch));
    if (scratch == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    for (size_t width = 1U; width < index_count;) {
        size_t run_width = width > SIZE_MAX / 2U ? SIZE_MAX : width * 2U;

        for (size_t left = 0U; left < index_count; left += run_width) {
            size_t remaining = index_count - left;
            size_t middle = remaining < width ? index_count : left + width;
            size_t right = remaining < run_width ? index_count : left + run_width;

            information_schema_merge_row_index_runs(
                sort,
                indexes,
                scratch,
                (struct information_schema_merge_range){
                    .left = left,
                    .middle = middle,
                    .right = right,
                }
            );
        }
        memcpy(indexes, scratch, index_count * sizeof(*indexes));
        if (width > index_count / 2U) {
            break;
        }
        width *= 2U;
    }
    free(scratch);
    return MYLITE_OK;
}

static void information_schema_insertion_sort_row_indexes_by(
    const struct information_schema_row_index_sort *sort,
    size_t *indexes,
    size_t index_count
) {
    for (size_t index = 1U; index < index_count; ++index) {
        size_t value = indexes[index];
        size_t position = index;

        while (position > 0U &&
               information_schema_compare_row_index_sort(sort, indexes[position - 1U], value) > 0) {
            indexes[position] = indexes[position - 1U];
            --position;
        }
        indexes[position] = value;
    }
}

static void information_schema_merge_row_index_runs(
    const struct information_schema_row_index_sort *sort,
    size_t *indexes,
    size_t *scratch,
    struct information_schema_merge_range range
) {
    size_t left = range.left;
    size_t right = range.middle;
    size_t output = range.left;

    while (left < range.middle && right < range.right) {
        if (information_schema_compare_row_index_sort(sort, indexes[left], indexes[right]) <= 0) {
            scratch[output] = indexes[left];
            ++left;
        } else {
            scratch[output] = indexes[right];
            ++right;
        }
        ++output;
    }
    while (left < range.middle) {
        scratch[output] = indexes[left];
        ++left;
        ++output;
    }
    while (right < range.right) {
        scratch[output] = indexes[right];
        ++right;
        ++output;
    }
}

static int information_schema_compare_row_index_sort(
    const struct information_schema_row_index_sort *sort,
    size_t left_row,
    size_t right_row
) {
    if (sort->kind == INFORMATION_SCHEMA_ROW_INDEX_SORT_TABLES_DEFAULT) {
        return information_schema_compare_tables_default_rows(sort->rows, left_row, right_row);
    }
    if (sort->kind == INFORMATION_SCHEMA_ROW_INDEX_SORT_GROUPED_STATISTICS) {
        return information_schema_compare_grouped_statistics_rows(
            sort->query,
            sort->rows,
            left_row,
            right_row
        );
    }
    return information_schema_compare_rows(
        sort->query,
        sort->rows,
        (struct information_schema_row_order_pair){
            .left_row = left_row,
            .right_row = right_row,
        }
    );
}

static int information_schema_compare_grouped_statistics_rows(
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    size_t left_row,
    size_t right_row
) {
    int comparison = 0;

    for (size_t index = 0U; index < query->group_index_count; ++index) {
        size_t column_index = query->group_indexes[index];

        comparison = information_schema_compare_row_values(
            query->definition,
            column_index,
            rows->rows[left_row][column_index],
            rows->rows[right_row][column_index]
        );
        if (comparison != 0) {
            return comparison;
        }
    }
    return information_schema_compare_row_values(
        query->definition,
        information_schema_statistics_seq_in_index_column,
        rows->rows[left_row][information_schema_statistics_seq_in_index_column],
        rows->rows[right_row][information_schema_statistics_seq_in_index_column]
    );
}

static bool information_schema_grouped_statistics_rows_same_group(
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    size_t left_row,
    size_t right_row
) {
    for (size_t index = 0U; index < query->group_index_count; ++index) {
        size_t column_index = query->group_indexes[index];

        if (information_schema_compare_row_values(
                query->definition,
                column_index,
                rows->rows[left_row][column_index],
                rows->rows[right_row][column_index]
            ) != 0) {
            return false;
        }
    }
    return true;
}

static int information_schema_compare_row_values(
    const struct mylite_execution_catalog_table_definition *definition,
    size_t column_index,
    const char *left,
    const char *right
) {
    if (left == NULL && right == NULL) {
        return 0;
    }
    if (left == NULL) {
        return -1;
    }
    if (right == NULL) {
        return 1;
    }
    if (information_schema_column_is_numeric(definition, column_index)) {
        int64_t left_value = 0;
        int64_t right_value = 0;

        if (information_schema_text_to_i64(left, &left_value) == MYLITE_OK &&
            information_schema_text_to_i64(right, &right_value) == MYLITE_OK) {
            return (left_value > right_value) - (left_value < right_value);
        }
    }
    return information_schema_compare_text(definition, column_index, left, right);
}

static int information_schema_group_concat_ordered_column_value(
    struct mylite_db *database,
    const struct information_schema_row_set *rows,
    const size_t *indexes,
    struct information_schema_group_concat_order order,
    char **out_value
) {
    struct mylite_dynamic_string string;
    bool appended = false;
    int rc = MYLITE_OK;

    *out_value = NULL;
    mylite_dynamic_string_init(&string);
    for (size_t index = order.start_index; rc == MYLITE_OK && index < order.end_index; ++index) {
        const char *value = rows->rows[indexes[index]][order.column_index];

        if (value == NULL) {
            continue;
        }
        if (appended) {
            rc = mylite_dynamic_string_append_char(&string, ',');
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(&string, value);
        }
        appended = true;
    }
    if (rc == MYLITE_OK && !appended) {
        rc = mylite_dynamic_string_append(&string, "");
    }
    if (rc == MYLITE_OK) {
        *out_value = mylite_dynamic_string_take(&string);
        if (*out_value == NULL) {
            set_nomem_error(database);
            rc = MYLITE_NOMEM;
        }
    } else if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    mylite_dynamic_string_deinit(&string);
    return rc;
}

static int information_schema_append_grouped_statistics_projected_row(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    const size_t *indexes,
    size_t start_index,
    size_t end_index,
    const struct information_schema_result_projection_buffers *buffers,
    mylite_result *result
) {
    char **owned_values = NULL;
    const char *const *representative = (const char *const *)rows->rows[indexes[start_index]];
    int rc = MYLITE_OK;

    if (query->projection_count != 0U) {
        owned_values = (char **)calloc(query->projection_count, sizeof(*owned_values));
        if (owned_values == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
    }
    for (size_t projection_index = 0U;
         rc == MYLITE_OK && projection_index < query->projection_count;
         ++projection_index) {
        size_t column_index = query->projection_indexes[projection_index];

        switch (query->projection_kinds[projection_index]) {
        case INFORMATION_SCHEMA_PROJECTION_COLUMN:
            buffers->values[projection_index] = representative[column_index];
            break;
        case INFORMATION_SCHEMA_PROJECTION_LOGICAL_NOT_COLUMN:
            rc = information_schema_logical_not_column_projection_value(
                database,
                query,
                representative,
                projection_index,
                information_schema_projection_computed_buffer(buffers, projection_index),
                integer_text_capacity,
                &buffers->values[projection_index]
            );
            break;
        case INFORMATION_SCHEMA_PROJECTION_GROUP_CONCAT_ORDERED_COLUMN:
            rc = information_schema_group_concat_ordered_column_value(
                database,
                rows,
                indexes,
                (struct information_schema_group_concat_order){
                    .start_index = start_index,
                    .end_index = end_index,
                    .column_index = column_index,
                },
                &owned_values[projection_index]
            );
            buffers->values[projection_index] = owned_values[projection_index];
            break;
        case INFORMATION_SCHEMA_PROJECTION_UNSIGNED_INTEGER_EXPRESSION:
        case INFORMATION_SCHEMA_PROJECTION_INTEGER_LITERAL:
            set_runtime_error(database, "invalid grouped INFORMATION_SCHEMA projection descriptor");
            rc = MYLITE_ERROR;
            break;
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_text_row(result, buffers->values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t projection_index = 0U; projection_index < query->projection_count;
         ++projection_index) {
        free(owned_values[projection_index]);
    }
    free((void *)owned_values);
    return rc;
}

static int information_schema_result_projection_buffers_init(
    struct mylite_db *database,
    const struct information_schema_query *query,
    struct information_schema_result_projection_buffers *buffers
) {
    bool needs_computed_values = false;

    if (query->projection_count == 0U) {
        return MYLITE_OK;
    }
    if (query->projection_count <= information_schema_projection_stack_capacity) {
        buffers->values = buffers->stack_values;
    } else {
        if (query->projection_count > SIZE_MAX / sizeof(*buffers->values)) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        buffers->values = (const char **)malloc(query->projection_count * sizeof(*buffers->values));
        if (buffers->values == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        buffers->values_use_heap = true;
    }
    needs_computed_values = information_schema_query_needs_computed_projection_buffers(query);
    if (!needs_computed_values) {
        return MYLITE_OK;
    }
    if (query->projection_count <= information_schema_projection_stack_capacity) {
        buffers->computed_values = &buffers->stack_computed_values[0][0];
    } else {
        if (query->projection_count > SIZE_MAX / integer_text_capacity) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        buffers->computed_values = malloc(query->projection_count * integer_text_capacity);
        if (buffers->computed_values == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        buffers->computed_values_use_heap = true;
    }
    return MYLITE_OK;
}

static bool information_schema_query_needs_computed_projection_buffers(
    const struct information_schema_query *query
) {
    for (size_t projection_index = 0U; projection_index < query->projection_count;
         ++projection_index) {
        if (information_schema_projection_needs_computed_buffer(
                query->projection_kinds[projection_index]
            )) {
            return true;
        }
    }
    return false;
}

static bool information_schema_projection_needs_computed_buffer(
    enum information_schema_projection_kind kind
) {
    return kind == INFORMATION_SCHEMA_PROJECTION_UNSIGNED_INTEGER_EXPRESSION ||
           kind == INFORMATION_SCHEMA_PROJECTION_INTEGER_LITERAL ||
           kind == INFORMATION_SCHEMA_PROJECTION_LOGICAL_NOT_COLUMN;
}

static void information_schema_result_projection_buffers_deinit(
    struct information_schema_result_projection_buffers *buffers
) {
    if (buffers == NULL) {
        return;
    }
    if (buffers->computed_values_use_heap) {
        free(buffers->computed_values);
    }
    if (buffers->values_use_heap) {
        free((void *)buffers->values);
    }
    *buffers = (struct information_schema_result_projection_buffers){0};
}

static int information_schema_append_projected_result_row(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const char *const *row,
    const struct information_schema_result_projection_buffers *buffers,
    mylite_result *result
) {
    int rc = MYLITE_OK;

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < query->projection_count;
         ++column_index) {
        rc = information_schema_projection_value(
            database,
            query,
            row,
            column_index,
            information_schema_projection_computed_buffer(buffers, column_index),
            integer_text_capacity,
            &buffers->values[column_index]
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_text_row(result, buffers->values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    return rc;
}

static int information_schema_projection_value(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const char *const *row,
    size_t projection_index,
    char *buffer,
    size_t buffer_size,
    const char **out_value
) {
    *out_value = NULL;
    if (query->projection_kinds[projection_index] == INFORMATION_SCHEMA_PROJECTION_COLUMN) {
        *out_value = row[query->projection_indexes[projection_index]];
        return MYLITE_OK;
    }
    if (query->projection_kinds[projection_index] ==
        INFORMATION_SCHEMA_PROJECTION_UNSIGNED_INTEGER_EXPRESSION) {
        return information_schema_unsigned_projection_expression_value(
            database,
            row,
            &query->projection_expressions[projection_index],
            buffer,
            buffer_size,
            out_value
        );
    }
    if (query->projection_kinds[projection_index] ==
        INFORMATION_SCHEMA_PROJECTION_INTEGER_LITERAL) {
        return information_schema_integer_literal_projection_value(
            database,
            &query->projection_expressions[projection_index],
            buffer,
            buffer_size,
            out_value
        );
    }
    if (query->projection_kinds[projection_index] ==
        INFORMATION_SCHEMA_PROJECTION_LOGICAL_NOT_COLUMN) {
        return information_schema_logical_not_column_projection_value(
            database,
            query,
            row,
            projection_index,
            buffer,
            buffer_size,
            out_value
        );
    }

    set_runtime_error(database, "invalid INFORMATION_SCHEMA projection descriptor");
    return MYLITE_ERROR;
}

static int information_schema_unsigned_projection_expression_value(
    struct mylite_db *database,
    const char *const *row,
    const struct information_schema_projection_expression *expression,
    char *buffer,
    size_t buffer_size,
    const char **out_value
) {
    double value = 0.0;
    bool is_null = false;
    unsigned long long rounded = 0ULL;
    int written = 0;
    int rc = MYLITE_OK;
    static const double nearest_integer_rounding_offset = 0.5;

    *out_value = NULL;
    if (expression == NULL || expression->instruction_count == 0U) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA numeric projection expression");
        return MYLITE_ERROR;
    }
    rc = information_schema_numeric_projection_expression_value(
        database,
        row,
        expression,
        &value,
        &is_null
    );
    if (rc != MYLITE_OK || is_null) {
        return rc;
    }
    if (value < 0.0 || value > (double)ULLONG_MAX) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA SELECT unsigned numeric projection value is out of range"
        );
        return MYLITE_ERROR;
    }

    rounded = (unsigned long long)floor(value + nearest_integer_rounding_offset);
    written = snprintf(buffer, buffer_size, "%llu", rounded);
    if (written < 0 || (size_t)written >= buffer_size) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    *out_value = buffer;
    return MYLITE_OK;
}

static int information_schema_integer_literal_projection_value(
    struct mylite_db *database,
    const struct information_schema_projection_expression *expression,
    char *buffer,
    size_t buffer_size,
    const char **out_value
) {
    size_t length = expression == NULL || expression->integer_literal_text == NULL
                        ? 0U
                        : strlen(expression->integer_literal_text);

    if (length == 0U || length >= buffer_size) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA integer literal projection");
        return MYLITE_ERROR;
    }

    memcpy(buffer, expression->integer_literal_text, length + 1U);
    *out_value = buffer;
    return MYLITE_OK;
}

static int information_schema_logical_not_column_projection_value(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const char *const *row,
    size_t projection_index,
    char *buffer,
    size_t buffer_size,
    const char **out_value
) {
    const char *value_text = row[query->projection_indexes[projection_index]];
    int64_t value = 0;
    int rc = MYLITE_OK;

    if (value_text == NULL) {
        *out_value = NULL;
        return MYLITE_OK;
    }
    rc = information_schema_text_to_i64(value_text, &value);
    if (rc != MYLITE_OK) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA logical NOT projection value");
        return MYLITE_ERROR;
    }
    rc = information_schema_format_i64(database, value == 0 ? 1 : 0, buffer, buffer_size);
    if (rc == MYLITE_OK) {
        *out_value = buffer;
    }
    return rc;
}

static int information_schema_numeric_projection_expression_value(
    struct mylite_db *database,
    const char *const *row,
    const struct information_schema_projection_expression *expression,
    double *out_value,
    bool *out_is_null
) {
    struct information_schema_numeric_eval_value_stack values = {0};
    int rc = MYLITE_OK;

    *out_value = 0.0;
    *out_is_null = false;
    if (expression == NULL) {
        return MYLITE_MISUSE;
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < expression->instruction_count; ++index) {
        const struct information_schema_numeric_projection_instruction *instruction =
            &expression->instructions[index];

        if (instruction->kind == INFORMATION_SCHEMA_NUMERIC_PROJECTION_LITERAL) {
            rc = information_schema_numeric_eval_value_stack_push(
                database,
                &values,
                (struct information_schema_numeric_eval_value){
                    .value = instruction->literal,
                    .is_null = false,
                }
            );
        } else if (instruction->kind == INFORMATION_SCHEMA_NUMERIC_PROJECTION_COLUMN) {
            double value = 0.0;
            bool is_null = false;

            rc = information_schema_numeric_text_value(
                database,
                row[instruction->column_index],
                &value,
                &is_null
            );
            if (rc == MYLITE_OK) {
                rc = information_schema_numeric_eval_value_stack_push(
                    database,
                    &values,
                    (struct information_schema_numeric_eval_value){
                        .value = value,
                        .is_null = is_null,
                    }
                );
            }
        } else if (instruction->kind == INFORMATION_SCHEMA_NUMERIC_PROJECTION_DIVIDE) {
            rc = information_schema_numeric_projection_apply_divide_frame(database, &values);
        } else {
            set_runtime_error(database, "invalid INFORMATION_SCHEMA numeric projection expression");
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && values.count != 1U) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA numeric projection expression");
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        *out_value = values.items[0].value;
        *out_is_null = values.items[0].is_null;
    }
    information_schema_numeric_eval_value_stack_deinit(&values);
    return rc;
}

static int information_schema_numeric_projection_apply_divide_frame(
    struct mylite_db *database,
    struct information_schema_numeric_eval_value_stack *values
) {
    struct information_schema_numeric_eval_value left = {0};
    struct information_schema_numeric_eval_value right = {0};

    if (values->count < 2U) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA numeric projection expression");
        return MYLITE_ERROR;
    }
    right = values->items[--values->count];
    left = values->items[--values->count];
    if (left.is_null || right.is_null) {
        return information_schema_numeric_eval_value_stack_push(
            database,
            values,
            (struct information_schema_numeric_eval_value){.value = 0.0, .is_null = true}
        );
    }
    if (right.value == 0.0) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA SELECT numeric projection does not support division by zero"
        );
        return MYLITE_ERROR;
    }
    return information_schema_numeric_eval_value_stack_push(
        database,
        values,
        (struct information_schema_numeric_eval_value){
            .value = left.value / right.value,
            .is_null = false,
        }
    );
}

static int information_schema_numeric_eval_value_stack_push(
    struct mylite_db *database,
    struct information_schema_numeric_eval_value_stack *stack,
    struct information_schema_numeric_eval_value value
) {
    struct information_schema_numeric_eval_value *items = NULL;
    size_t capacity = 0U;

    if (stack->count == stack->capacity) {
        capacity = stack->capacity == 0U ? if_stack_initial_capacity : stack->capacity * 2U;
        if (capacity < stack->capacity || capacity > SIZE_MAX / sizeof(*items)) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        items = realloc(stack->items, capacity * sizeof(*items));
        if (items == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        stack->items = items;
        stack->capacity = capacity;
    }
    stack->items[stack->count] = value;
    ++stack->count;
    return MYLITE_OK;
}

static void information_schema_numeric_eval_value_stack_deinit(
    struct information_schema_numeric_eval_value_stack *stack
) {
    if (stack == NULL) {
        return;
    }
    free(stack->items);
    *stack = (struct information_schema_numeric_eval_value_stack){0};
}

static int information_schema_numeric_text_value(
    struct mylite_db *database,
    const char *text,
    double *out_value,
    bool *out_is_null
) {
    char *end = NULL;

    *out_value = 0.0;
    *out_is_null = text == NULL;
    if (text == NULL) {
        return MYLITE_OK;
    }

    errno = 0;
    *out_value = mylite_numeric_parse_double(text, &end);
    if (errno == ERANGE || end == text || (end != NULL && *end != '\0')) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA SELECT numeric projection supports only numeric metadata values"
        );
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int information_schema_append_count_result(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    mylite_result *result,
    size_t *out_read_row_count
) {
    char count_text[integer_text_capacity];
    const char *values[1] = {count_text};
    struct information_schema_row_index_set indexes = {0};
    size_t count = 0U;
    bool emits_count_row = true;
    int rc = MYLITE_OK;

    if (out_read_row_count != NULL) {
        *out_read_row_count = 0U;
    }
    if (rows->count_only) {
        count = rows->row_count;
    } else {
        rc = information_schema_matching_row_indexes(database, query, rows, &indexes);
        count = indexes.count;
    }
    if ((query->limit.has_limit && query->limit.row_count == 0) ||
        (query->limit.has_offset && query->limit.offset != 0)) {
        emits_count_row = false;
    }
    if (rc == MYLITE_OK) {
        rc =
            information_schema_format_i64(database, (int64_t)count, count_text, sizeof(count_text));
    }
    if (rc == MYLITE_OK && emits_count_row) {
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    if (rc == MYLITE_OK && emits_count_row && out_read_row_count != NULL) {
        *out_read_row_count = count;
    }
    information_schema_row_index_set_deinit(&indexes);
    return rc;
}

static char *information_schema_projection_computed_buffer(
    const struct information_schema_result_projection_buffers *buffers,
    size_t projection_index
) {
    if (buffers == NULL || buffers->computed_values == NULL) {
        return NULL;
    }
    return buffers->computed_values + (projection_index * integer_text_capacity);
}

static int information_schema_matching_row_indexes(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    struct information_schema_row_index_set *out_indexes
) {
    int rc = MYLITE_OK;

    if (out_indexes == NULL) {
        return MYLITE_MISUSE;
    }
    information_schema_row_index_set_init(out_indexes);

    if (rows->row_count == 0U) {
        return MYLITE_OK;
    }
    for (size_t row_index = 0U; rc == MYLITE_OK && row_index < rows->row_count; ++row_index) {
        bool matches = true;

        if (query->predicate.instruction_count != 0U && !rows->predicate_applied) {
            rc = information_schema_predicate_matches(
                database,
                query,
                &query->predicate,
                (const char *const *)rows->rows[row_index],
                &matches
            );
        }
        if (rc == MYLITE_OK && matches) {
            rc = information_schema_row_index_set_append(database, out_indexes, row_index);
        }
    }
    if (rc != MYLITE_OK) {
        information_schema_row_index_set_deinit(out_indexes);
        return rc;
    }

    return MYLITE_OK;
}

static void information_schema_row_index_set_init(struct information_schema_row_index_set *indexes
) {
    *indexes = (struct information_schema_row_index_set){
        .items = indexes->stack_items,
        .count = 0U,
        .capacity = information_schema_row_index_stack_capacity,
        .uses_heap = false,
    };
}

static int information_schema_row_index_set_append(
    struct mylite_db *database,
    struct information_schema_row_index_set *indexes,
    size_t value
) {
    size_t *grown_items = NULL;
    size_t capacity = 0U;

    if (indexes->count == indexes->capacity) {
        if (indexes->capacity > SIZE_MAX / 2U) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        capacity = indexes->capacity * 2U;
        if (capacity > SIZE_MAX / sizeof(*grown_items)) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        if (indexes->uses_heap) {
            grown_items = realloc(indexes->items, capacity * sizeof(*grown_items));
        } else {
            grown_items = malloc(capacity * sizeof(*grown_items));
            if (grown_items != NULL) {
                memcpy(grown_items, indexes->stack_items, indexes->count * sizeof(*grown_items));
            }
        }
        if (grown_items == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        indexes->items = grown_items;
        indexes->capacity = capacity;
        indexes->uses_heap = true;
    }
    indexes->items[indexes->count] = value;
    ++indexes->count;
    return MYLITE_OK;
}

static void information_schema_row_index_set_deinit(struct information_schema_row_index_set *indexes
) {
    if (indexes == NULL) {
        return;
    }
    if (indexes->uses_heap) {
        free(indexes->items);
    }
    *indexes = (struct information_schema_row_index_set){0};
}

static int information_schema_includes_connection_control_failed_login_attempts(
    struct mylite_db *database,
    const struct information_schema_query *query,
    bool *out_includes_table
) {
    if (out_includes_table == NULL) {
        return MYLITE_MISUSE;
    }
    *out_includes_table = false;
    if (query == NULL || query->definition->kind != MYLITE_EXECUTION_CATALOG_TABLE_TABLES) {
        return MYLITE_OK;
    }
    for (size_t index = 0U; index < query->predicate.instruction_count; ++index) {
        const struct information_schema_predicate_instruction *instruction =
            &query->predicate.instructions[index];

        if (instruction->kind == INFORMATION_SCHEMA_PREDICATE_COMPARISON &&
            (instruction->operator_kind == MYLITE_SQL_AST_OPERATOR_EQUAL ||
             instruction->operator_kind == MYLITE_SQL_AST_OPERATOR_NULL_SAFE_EQUAL) &&
            instruction->column_index == information_schema_tables_table_name_column &&
            instruction->value_count == 1U && instruction->values != NULL &&
            !instruction->values[0].is_null && instruction->values[0].text != NULL &&
            strcmp(instruction->values[0].text, "CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS") == 0) {
            *out_includes_table = true;
            break;
        }
    }
    (void)database;
    return MYLITE_OK;
}

static bool information_schema_row_is_connection_control_failed_login_attempts(
    const char *const *row
) {
    return row != NULL && row[information_schema_tables_table_schema_column] != NULL &&
           row[information_schema_tables_table_name_column] != NULL &&
           strcmp(row[information_schema_tables_table_schema_column], "information_schema") == 0 &&
           strcmp(
               row[information_schema_tables_table_name_column],
               "CONNECTION_CONTROL_FAILED_LOGIN_ATTEMPTS"
           ) == 0;
}

static int information_schema_sort_row_indexes(
    struct mylite_db *database,
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    size_t *indexes,
    size_t index_count
) {
    int rc = MYLITE_OK;

    if (!query->has_order) {
        if (query->definition->kind == MYLITE_EXECUTION_CATALOG_TABLE_TABLES) {
            rc = information_schema_sort_tables_default_row_indexes(
                database,
                rows,
                indexes,
                index_count
            );
        }
        return rc;
    }
    const struct information_schema_row_index_sort sort = {
        .query = query,
        .rows = rows,
        .kind = INFORMATION_SCHEMA_ROW_INDEX_SORT_ORDER,
    };

    return information_schema_sort_row_indexes_by(database, &sort, indexes, index_count);
}

static int information_schema_sort_tables_default_row_indexes(
    struct mylite_db *database,
    const struct information_schema_row_set *rows,
    size_t *indexes,
    size_t index_count
) {
    const struct information_schema_row_index_sort sort = {
        .query = NULL,
        .rows = rows,
        .kind = INFORMATION_SCHEMA_ROW_INDEX_SORT_TABLES_DEFAULT,
    };

    return information_schema_sort_row_indexes_by(database, &sort, indexes, index_count);
}

static int information_schema_compare_tables_default_rows(
    const struct information_schema_row_set *rows,
    size_t left_row,
    size_t right_row
) {
    const char *left_schema = rows->rows[left_row][information_schema_tables_table_schema_column];
    const char *right_schema = rows->rows[right_row][information_schema_tables_table_schema_column];
    const int left_priority = information_schema_tables_default_schema_priority(left_schema);
    const int right_priority = information_schema_tables_default_schema_priority(right_schema);

    if (left_priority != right_priority) {
        return left_priority - right_priority;
    }
    const int left_table_priority = information_schema_tables_default_table_priority(
        left_schema,
        rows->rows[left_row][information_schema_tables_table_name_column]
    );
    const int right_table_priority = information_schema_tables_default_table_priority(
        right_schema,
        rows->rows[right_row][information_schema_tables_table_name_column]
    );

    if (left_table_priority != right_table_priority) {
        return left_table_priority - right_table_priority;
    }
    return information_schema_compare_text(
        rows->definition,
        information_schema_tables_table_name_column,
        rows->rows[left_row][information_schema_tables_table_name_column],
        rows->rows[right_row][information_schema_tables_table_name_column]
    );
}

static int information_schema_tables_default_schema_priority(const char *schema_name) {
    enum { information_schema_unknown_schema_priority = 5 };

    if (schema_name == NULL) {
        return information_schema_unknown_schema_priority;
    }
    if (strcmp(schema_name, "mysql") == 0) {
        return 0;
    }
    if (strcmp(schema_name, "information_schema") == 0) {
        return 1;
    }
    if (strcmp(schema_name, "performance_schema") == 0) {
        return 2;
    }
    if (strcmp(schema_name, "sys") == 0) {
        return 3;
    }
    return information_schema_unknown_schema_priority - 1;
}

static int information_schema_tables_default_table_priority(
    const char *schema_name,
    const char *table_name
) {
    if (schema_name != NULL && table_name != NULL && strcmp(schema_name, "mysql") == 0 &&
        strcmp(table_name, "innodb_table_stats") == 0) {
        return 0;
    }
    return 1;
}

static int information_schema_compare_rows(
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    struct information_schema_row_order_pair pair
) {
    size_t order_count = query->order_count == 0U ? 1U : query->order_count;

    for (size_t order_index = 0U; order_index < order_count; ++order_index) {
        size_t column_index =
            query->order_count == 0U ? query->order_index : query->order_indexes[order_index];
        enum planned_select_order_direction direction = query->order_count == 0U
                                                            ? query->order_direction
                                                            : query->order_directions[order_index];
        int comparison = information_schema_compare_rows_by_column(
            query,
            rows,
            pair,
            (struct information_schema_row_order_column){
                .column_index = column_index,
                .direction = direction,
            }
        );

        if (comparison != 0) {
            return comparison;
        }
    }
    return 0;
}

static int information_schema_compare_rows_by_column(
    const struct information_schema_query *query,
    const struct information_schema_row_set *rows,
    struct information_schema_row_order_pair pair,
    struct information_schema_row_order_column order
) {
    const char *left = rows->rows[pair.left_row][order.column_index];
    const char *right = rows->rows[pair.right_row][order.column_index];
    int comparison = 0;

    if (left == NULL && right == NULL) {
        return 0;
    }
    if (left == NULL) {
        comparison = -1;
    } else if (right == NULL) {
        comparison = 1;
    } else if (information_schema_column_is_numeric(query->definition, order.column_index)) {
        int64_t left_value = 0;
        int64_t right_value = 0;

        if (information_schema_text_to_i64(left, &left_value) == MYLITE_OK &&
            information_schema_text_to_i64(right, &right_value) == MYLITE_OK) {
            comparison = (left_value > right_value) - (left_value < right_value);
        } else {
            comparison =
                information_schema_compare_text(query->definition, order.column_index, left, right);
        }
    } else if (query->definition->kind == MYLITE_EXECUTION_CATALOG_TABLE_COLLATIONS &&
               order.column_index == information_schema_collations_name_column) {
        size_t left_index = 0U;
        size_t right_index = 0U;
        bool left_known = mylite_execution_catalog_mysql_collation_index_by_name(left, &left_index);
        bool right_known =
            mylite_execution_catalog_mysql_collation_index_by_name(right, &right_index);

        if (left_known && right_known) {
            comparison = (left_index > right_index) - (left_index < right_index);
        } else {
            comparison =
                information_schema_compare_text(query->definition, order.column_index, left, right);
        }
    } else {
        comparison =
            information_schema_compare_text(query->definition, order.column_index, left, right);
    }
    if (order.direction == PLANNED_SELECT_ORDER_DESC) {
        comparison = -comparison;
    }
    return comparison;
}
