static int append_information_schema_innodb_columns_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_table_columns_cache_entry *columns_cache_entry = NULL;
    size_t column_count = 0U;
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_innodb_columns_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_COLUMNS columns");
        return MYLITE_ERROR;
    }

    rc = mylite_execution_acquire_table_columns(
        database,
        table->table_id,
        &columns,
        &column_count,
        &columns_cache_entry
    );
    for (size_t index = 0U; rc == MYLITE_OK && index < column_count; ++index) {
        rc = append_information_schema_innodb_columns_column_row(
            database,
            rows,
            table,
            &columns[index]
        );
    }

    mylite_execution_release_table_columns(columns_cache_entry, columns);
    return rc;
}

static int append_information_schema_innodb_columns_column_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column
) {
    struct information_schema_innodb_column_type_info type_info = {0};
    char table_id_text[integer_text_capacity];
    char position_text[integer_text_capacity];
    char mtype_text[integer_text_capacity];
    char prtype_text[integer_text_capacity];
    char length_text[integer_text_capacity];
    char has_default_text[integer_text_capacity];
    int64_t position = 0;
    int rc = MYLITE_OK;

    rc = information_schema_innodb_column_position(database, column, &position);
    if (rc == MYLITE_OK) {
        rc = information_schema_innodb_column_type_info(database, table, column, &type_info);
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
        rc =
            information_schema_format_i64(database, position, position_text, sizeof(position_text));
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            type_info.mtype,
            mtype_text,
            sizeof(mtype_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            type_info.prtype,
            prtype_text,
            sizeof(prtype_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            type_info.length,
            length_text,
            sizeof(length_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            information_schema_innodb_column_has_default,
            has_default_text,
            sizeof(has_default_text)
        );
    }
    if (rc == MYLITE_OK) {
        const char *values[information_schema_innodb_columns_column_count] = {
            table_id_text,
            column->name,
            position_text,
            mtype_text,
            prtype_text,
            length_text,
            has_default_text,
            NULL,
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int information_schema_innodb_column_position(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    int64_t *out_position
) {
    if (column == NULL || out_position == NULL || column->ordinal_position <= 0) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_COLUMNS position");
        return MYLITE_ERROR;
    }

    *out_position = column->ordinal_position - 1;
    return MYLITE_OK;
}

static int information_schema_innodb_column_type_info(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column,
    struct information_schema_innodb_column_type_info *out_info
) {
    if (out_info == NULL || column == NULL) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_COLUMNS column input");
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_char(column)) {
        return information_schema_innodb_column_character_type_info(
            database,
            table,
            column,
            (struct information_schema_innodb_character_type_request){
                .mtype = information_schema_innodb_column_mtype_char,
                .mysql_type_code = information_schema_innodb_column_mysql_type_string,
            },
            out_info
        );
    }
    if (column_descriptor_is_varchar(column)) {
        return information_schema_innodb_column_character_type_info(
            database,
            table,
            column,
            (struct information_schema_innodb_character_type_request){
                .mtype = information_schema_innodb_column_mtype_varchar,
                .mysql_type_code = information_schema_innodb_column_mysql_type_varchar,
            },
            out_info
        );
    }
    if (column_descriptor_is_text_family(column)) {
        return information_schema_innodb_column_text_family_type_info(
            database,
            table,
            column,
            out_info
        );
    }
    if (column_descriptor_is_binary_string_family(column)) {
        return information_schema_innodb_column_binary_type_info(database, column, out_info);
    }

    return information_schema_innodb_column_nonstring_type_info(database, column, out_info);
}

static int information_schema_innodb_column_nonstring_type_info(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct information_schema_innodb_column_type_info *out_info
) {
    if (column_descriptor_is_json(column)) {
        int64_t prtype = 0;

        int rc = information_schema_innodb_column_collation_type_prtype(
            database,
            "utf8mb4_bin",
            information_schema_innodb_column_numeric_flag +
                information_schema_innodb_column_mysql_type_json,
            column->is_nullable,
            &prtype
        );

        if (rc != MYLITE_OK) {
            return rc;
        }
        *out_info = (struct information_schema_innodb_column_type_info){
            .mtype = information_schema_innodb_column_mtype_blob,
            .prtype = prtype,
            .length = information_schema_innodb_column_blob_length_long,
        };
        return MYLITE_OK;
    }
    if (column_descriptor_is_spatial(column)) {
        *out_info = (struct information_schema_innodb_column_type_info){
            .mtype = information_schema_innodb_column_mtype_geometry,
            .prtype = information_schema_innodb_column_numeric_flag +
                      information_schema_innodb_column_mysql_type_geometry +
                      information_schema_innodb_column_not_null_prtype_flag(column),
            .length = information_schema_innodb_column_blob_length_long,
        };
        return MYLITE_OK;
    }
    if (column_descriptor_is_bit(column)) {
        return information_schema_innodb_column_bit_type_info(database, column, out_info);
    }
    if (column_descriptor_is_decimal(column)) {
        return information_schema_innodb_column_decimal_type_info(database, column, out_info);
    }
    if (column_descriptor_is_approximate(column)) {
        return information_schema_innodb_column_approximate_type_info(database, column, out_info);
    }
    if (column_descriptor_is_year(column)) {
        *out_info = (struct information_schema_innodb_column_type_info){
            .mtype = information_schema_innodb_column_mtype_int,
            .prtype = information_schema_innodb_column_numeric_flag +
                      information_schema_innodb_column_unsigned_flag +
                      information_schema_innodb_column_mysql_type_year +
                      information_schema_innodb_column_not_null_prtype_flag(column),
            .length = year_row_size_bytes,
        };
        return MYLITE_OK;
    }
    if (column_descriptor_is_date(column)) {
        *out_info = (struct information_schema_innodb_column_type_info){
            .mtype = information_schema_innodb_column_mtype_int,
            .prtype = information_schema_innodb_column_numeric_flag +
                      information_schema_innodb_column_mysql_type_date +
                      information_schema_innodb_column_not_null_prtype_flag(column),
            .length = date_row_size_bytes,
        };
        return MYLITE_OK;
    }
    if (column_descriptor_is_time(column)) {
        *out_info = (struct information_schema_innodb_column_type_info){
            .mtype = information_schema_innodb_column_mtype_fixbinary,
            .prtype = information_schema_innodb_column_temporal_decimal_flag +
                      information_schema_innodb_column_mysql_type_time +
                      information_schema_innodb_column_not_null_prtype_flag(column),
            .length = time_row_size_bytes,
        };
        return MYLITE_OK;
    }
    if (column_descriptor_is_datetime(column)) {
        *out_info = (struct information_schema_innodb_column_type_info){
            .mtype = information_schema_innodb_column_mtype_fixbinary,
            .prtype = information_schema_innodb_column_temporal_decimal_flag +
                      information_schema_innodb_column_mysql_type_datetime +
                      information_schema_innodb_column_not_null_prtype_flag(column),
            .length = datetime_row_size_bytes,
        };
        return MYLITE_OK;
    }
    if (column_descriptor_is_timestamp(column)) {
        *out_info = (struct information_schema_innodb_column_type_info){
            .mtype = information_schema_innodb_column_mtype_fixbinary,
            .prtype = information_schema_innodb_column_temporal_decimal_flag +
                      information_schema_innodb_column_mysql_type_timestamp +
                      information_schema_innodb_column_not_null_prtype_flag(column),
            .length = timestamp_row_size_bytes,
        };
        return MYLITE_OK;
    }
    if (column_descriptor_is_enum(column)) {
        return information_schema_innodb_column_enum_type_info(database, column, out_info);
    }
    if (column_descriptor_is_set(column)) {
        return information_schema_innodb_column_set_type_info(database, column, out_info);
    }

    return information_schema_innodb_column_integer_type_info(database, column, out_info);
}

static int information_schema_innodb_column_character_type_info(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column,
    struct information_schema_innodb_character_type_request request,
    struct information_schema_innodb_column_type_info *out_info
) {
    size_t character_count = 0U;
    uint64_t length = 0U;
    int64_t prtype = 0;
    int rc = MYLITE_OK;

    if (request.mtype == information_schema_innodb_column_mtype_char) {
        rc = parse_char_descriptor_length(
            database,
            column->logical_type,
            "INFORMATION_SCHEMA.INNODB_COLUMNS supports only baseline CHAR descriptors",
            &character_count
        );
    } else {
        rc = parse_varchar_descriptor_length(
            database,
            column->logical_type,
            "INFORMATION_SCHEMA.INNODB_COLUMNS supports only baseline VARCHAR descriptors",
            &character_count
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    length = (uint64_t)character_count * column_effective_max_bytes_per_character(table, column);
    if (length > (uint64_t)INT64_MAX) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_COLUMNS column length");
        return MYLITE_ERROR;
    }
    rc = information_schema_innodb_column_collation_prtype(
        database,
        column_effective_collation_name(table, column),
        request.mysql_type_code,
        column->is_nullable,
        &prtype
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    *out_info = (struct information_schema_innodb_column_type_info){
        .mtype = request.mtype,
        .prtype = prtype,
        .length = (int64_t)length,
    };
    return MYLITE_OK;
}

static int information_schema_innodb_column_text_family_type_info(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column,
    struct information_schema_innodb_column_type_info *out_info
) {
    const struct text_family_type_info *text_info =
        text_family_type_info_for_logical_type(column->logical_type);
    int64_t length = 0;
    int64_t prtype = 0;
    int rc = MYLITE_OK;

    if (text_info == NULL) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA.INNODB_COLUMNS supports only baseline TEXT descriptors"
        );
        return MYLITE_ERROR;
    }
    rc = information_schema_innodb_column_blob_family_length(
        database,
        text_info->maximum_length,
        &length
    );
    if (rc == MYLITE_OK) {
        rc = information_schema_innodb_column_collation_prtype(
            database,
            column_effective_collation_name(table, column),
            information_schema_innodb_column_mysql_type_blob,
            column->is_nullable,
            &prtype
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    *out_info = (struct information_schema_innodb_column_type_info){
        .mtype = information_schema_innodb_column_mtype_blob,
        .prtype = prtype,
        .length = length,
    };
    return MYLITE_OK;
}

static int information_schema_innodb_column_binary_type_info(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct information_schema_innodb_column_type_info *out_info
) {
    struct binary_string_type_info storage = {0};
    const struct binary_string_type_info *binary_info =
        binary_string_type_info_for_logical_type(column->logical_type, &storage);
    int64_t length = 0;
    int64_t mtype = information_schema_innodb_column_mtype_blob;
    int64_t mysql_type_code = information_schema_innodb_column_mysql_type_blob;
    int64_t prtype = 0;
    int rc = MYLITE_OK;

    if (binary_info == NULL) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA.INNODB_COLUMNS supports only baseline binary descriptors"
        );
        return MYLITE_ERROR;
    }
    if (binary_info->blob_family) {
        rc = information_schema_innodb_column_blob_family_length(
            database,
            binary_info->maximum_length,
            &length
        );
    } else {
        if (binary_info->maximum_length > (uint64_t)INT64_MAX) {
            set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_COLUMNS column length");
            return MYLITE_ERROR;
        }
        length = (int64_t)binary_info->maximum_length;
        if (binary_info->fixed_length) {
            mtype = information_schema_innodb_column_mtype_fixbinary;
            mysql_type_code = information_schema_innodb_column_mysql_type_string;
        } else {
            mtype = information_schema_innodb_column_mtype_binary;
            mysql_type_code = information_schema_innodb_column_mysql_type_varchar;
        }
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_innodb_column_binary_prtype(
            database,
            mysql_type_code,
            column->is_nullable,
            &prtype
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    *out_info = (struct information_schema_innodb_column_type_info){
        .mtype = mtype,
        .prtype = prtype,
        .length = length,
    };
    return MYLITE_OK;
}

static int information_schema_innodb_column_integer_type_info(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct information_schema_innodb_column_type_info *out_info
) {
    uint64_t length = 0U;
    int64_t mysql_type_code = 0;
    int rc = information_schema_innodb_column_integer_mysql_type(
        database,
        column->logical_type,
        &mysql_type_code
    );

    if (rc == MYLITE_OK) {
        rc = integer_row_size_bytes(column->logical_type, &length);
    }
    if (rc != MYLITE_OK || length > (uint64_t)INT64_MAX) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA.INNODB_COLUMNS supports only baseline integer descriptors"
        );
        return MYLITE_ERROR;
    }

    *out_info = (struct information_schema_innodb_column_type_info){
        .mtype = information_schema_innodb_column_mtype_int,
        .prtype = information_schema_innodb_column_numeric_flag + mysql_type_code +
                  (information_schema_innodb_column_logical_type_is_unsigned(column->logical_type)
                       ? information_schema_innodb_column_unsigned_flag
                       : 0) +
                  (column->is_nullable ? 0 : information_schema_innodb_column_not_null_flag),
        .length = (int64_t)length,
    };
    return MYLITE_OK;
}

static int information_schema_innodb_column_decimal_type_info(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct information_schema_innodb_column_type_info *out_info
) {
    struct decimal_type_info decimal_info = {0};
    uint64_t length = 0U;
    int rc = decimal_type_info_for_logical_type(column->logical_type, &decimal_info);

    if (rc == MYLITE_OK) {
        rc = decimal_row_size_bytes(&decimal_info, &length);
    }
    if (rc != MYLITE_OK || length > (uint64_t)INT64_MAX) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA.INNODB_COLUMNS supports only baseline DECIMAL descriptors"
        );
        return MYLITE_ERROR;
    }

    *out_info = (struct information_schema_innodb_column_type_info){
        .mtype = information_schema_innodb_column_mtype_fixbinary,
        .prtype = information_schema_innodb_column_temporal_decimal_flag +
                  information_schema_innodb_column_mysql_type_newdecimal +
                  (decimal_info.is_unsigned ? information_schema_innodb_column_unsigned_flag : 0) +
                  (column->is_nullable ? 0 : information_schema_innodb_column_not_null_flag),
        .length = (int64_t)length,
    };
    return MYLITE_OK;
}

static int information_schema_innodb_column_approximate_type_info(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct information_schema_innodb_column_type_info *out_info
) {
    struct approximate_type_info approximate_info = {
        .type_class = APPROXIMATE_TYPE_DOUBLE,
        .is_unsigned = false,
    };
    uint64_t length = 0U;
    int64_t mtype = information_schema_innodb_column_mtype_double;
    int64_t mysql_type_code = information_schema_innodb_column_mysql_type_double;
    int rc = approximate_type_info_for_logical_type(column->logical_type, &approximate_info);

    if (rc == MYLITE_OK) {
        rc = approximate_row_size_bytes(&approximate_info, &length);
    }
    if (rc != MYLITE_OK || length > (uint64_t)INT64_MAX) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA.INNODB_COLUMNS supports only baseline approximate descriptors"
        );
        return MYLITE_ERROR;
    }
    if (approximate_info.type_class == APPROXIMATE_TYPE_FLOAT) {
        mtype = information_schema_innodb_column_mtype_float;
        mysql_type_code = information_schema_innodb_column_mysql_type_float;
    }

    *out_info = (struct information_schema_innodb_column_type_info){
        .mtype = mtype,
        .prtype =
            information_schema_innodb_column_numeric_flag + mysql_type_code +
            (approximate_info.is_unsigned ? information_schema_innodb_column_unsigned_flag : 0) +
            (column->is_nullable ? 0 : information_schema_innodb_column_not_null_flag),
        .length = (int64_t)length,
    };
    return MYLITE_OK;
}

static int information_schema_innodb_column_bit_type_info(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct information_schema_innodb_column_type_info *out_info
) {
    uint64_t width = 0U;
    int64_t prtype = 0;
    int rc = bit_width_for_logical_type(column->logical_type, &width);

    if (rc != MYLITE_OK) {
        set_unsupported_error(
            database,
            "INFORMATION_SCHEMA.INNODB_COLUMNS supports only baseline BIT descriptors"
        );
        return MYLITE_ERROR;
    }
    rc = information_schema_innodb_column_collation_type_prtype(
        database,
        "binary",
        information_schema_innodb_column_numeric_flag +
            information_schema_innodb_column_unsigned_flag +
            information_schema_innodb_column_mysql_type_bit,
        column->is_nullable,
        &prtype
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    *out_info = (struct information_schema_innodb_column_type_info){
        .mtype = information_schema_innodb_column_mtype_fixbinary,
        .prtype = prtype,
        .length = (int64_t)bit_byte_width(width),
    };
    return MYLITE_OK;
}

static int information_schema_innodb_column_enum_type_info(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct information_schema_innodb_column_type_info *out_info
) {
    struct enum_type_info enum_info = {0};
    int rc = enum_type_info_for_logical_type(
        database,
        column->logical_type,
        "INFORMATION_SCHEMA.INNODB_COLUMNS supports only baseline ENUM descriptors",
        &enum_info
    );

    if (rc != MYLITE_OK) {
        return rc;
    }

    *out_info = (struct information_schema_innodb_column_type_info){
        .mtype = information_schema_innodb_column_mtype_int,
        .prtype = information_schema_innodb_column_unsigned_flag +
                  information_schema_innodb_column_mysql_type_string +
                  information_schema_innodb_column_not_null_prtype_flag(column),
        .length = enum_info.label_count <= enum_one_byte_label_count_max ? 1 : 2,
    };
    return MYLITE_OK;
}

static int information_schema_innodb_column_set_type_info(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct information_schema_innodb_column_type_info *out_info
) {
    uint64_t length = 0U;
    int rc = set_row_size_bytes(
        database,
        column->logical_type,
        &length,
        "INFORMATION_SCHEMA.INNODB_COLUMNS supports only baseline SET descriptors"
    );

    if (rc != MYLITE_OK || length > (uint64_t)INT64_MAX) {
        if (rc == MYLITE_OK) {
            set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_COLUMNS column length");
            return MYLITE_ERROR;
        }
        return rc;
    }

    *out_info = (struct information_schema_innodb_column_type_info){
        .mtype = information_schema_innodb_column_mtype_int,
        .prtype = information_schema_innodb_column_unsigned_flag +
                  information_schema_innodb_column_mysql_type_string +
                  information_schema_innodb_column_not_null_prtype_flag(column),
        .length = (int64_t)length,
    };
    return MYLITE_OK;
}

static int information_schema_innodb_column_blob_family_length(
    struct mylite_db *database,
    uint64_t maximum_length,
    int64_t *out_length
) {
    if (maximum_length == tinytext_max_length) {
        *out_length = information_schema_innodb_column_blob_length_tiny;
        return MYLITE_OK;
    }
    if (maximum_length == text_max_length) {
        *out_length = information_schema_innodb_column_blob_length_regular;
        return MYLITE_OK;
    }
    if (maximum_length == mediumtext_max_length) {
        *out_length = information_schema_innodb_column_blob_length_medium;
        return MYLITE_OK;
    }
    if (maximum_length == longtext_max_length) {
        *out_length = information_schema_innodb_column_blob_length_long;
        return MYLITE_OK;
    }

    set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_COLUMNS blob length");
    return MYLITE_ERROR;
}

static int information_schema_innodb_column_collation_prtype(
    struct mylite_db *database,
    const char *collation_name,
    int64_t mysql_type_code,
    bool is_nullable,
    int64_t *out_prtype
) {
    return information_schema_innodb_column_collation_type_prtype(
        database,
        collation_name,
        mysql_type_code,
        is_nullable,
        out_prtype
    );
}

static int information_schema_innodb_column_binary_prtype(
    struct mylite_db *database,
    int64_t mysql_type_code,
    bool is_nullable,
    int64_t *out_prtype
) {
    return information_schema_innodb_column_collation_type_prtype(
        database,
        "binary",
        information_schema_innodb_column_numeric_flag + mysql_type_code,
        is_nullable,
        out_prtype
    );
}

static int information_schema_innodb_column_collation_type_prtype(
    struct mylite_db *database,
    const char *collation_name,
    int64_t type_flags,
    bool is_nullable,
    int64_t *out_prtype
) {
    uint32_t collation_id = result_metadata_collation_id(collation_name);
    int64_t nullability_flag = is_nullable ? 0 : information_schema_innodb_column_not_null_flag;
    int64_t collation_part = 0;

    if (out_prtype == NULL || type_flags < 0 ||
        collation_id > (uint32_t)(INT64_MAX >> information_schema_innodb_column_collation_shift)) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_COLUMNS prtype");
        return MYLITE_ERROR;
    }

    collation_part = (int64_t)collation_id << information_schema_innodb_column_collation_shift;
    if (type_flags > INT64_MAX - nullability_flag ||
        collation_part > INT64_MAX - type_flags - nullability_flag) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_COLUMNS prtype");
        return MYLITE_ERROR;
    }

    *out_prtype = collation_part + type_flags + nullability_flag;
    return MYLITE_OK;
}

static int information_schema_innodb_column_integer_mysql_type(
    struct mylite_db *database,
    const char *logical_type,
    int64_t *out_type_code
) {
    if (logical_type == NULL || out_type_code == NULL) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_COLUMNS integer type");
        return MYLITE_ERROR;
    }
    if (strcmp(logical_type, "TINYINT") == 0 || strcmp(logical_type, "TINYINT(1)") == 0 ||
        strcmp(logical_type, "TINYINT UNSIGNED") == 0) {
        *out_type_code = information_schema_innodb_column_mysql_type_tiny;
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "SMALLINT") == 0 || strcmp(logical_type, "SMALLINT UNSIGNED") == 0) {
        *out_type_code = information_schema_innodb_column_mysql_type_short;
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "MEDIUMINT") == 0 || strcmp(logical_type, "MEDIUMINT UNSIGNED") == 0) {
        *out_type_code = information_schema_innodb_column_mysql_type_int24;
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "BIGINT") == 0 || strcmp(logical_type, "BIGINT UNSIGNED") == 0) {
        *out_type_code = information_schema_innodb_column_mysql_type_longlong;
        return MYLITE_OK;
    }
    if (strcmp(logical_type, "INT") == 0 || strcmp(logical_type, "INT UNSIGNED") == 0) {
        *out_type_code = information_schema_innodb_column_mysql_type_long;
        return MYLITE_OK;
    }

    set_unsupported_error(
        database,
        "INFORMATION_SCHEMA.INNODB_COLUMNS supports only baseline integer descriptors"
    );
    return MYLITE_ERROR;
}

static int64_t information_schema_innodb_column_not_null_prtype_flag(
    const struct mylite_catalog_column_descriptor *column
) {
    if (column != NULL && !column->is_nullable) {
        return information_schema_innodb_column_not_null_flag;
    }
    return 0;
}

static bool information_schema_innodb_column_logical_type_is_unsigned(const char *logical_type) {
    static const char suffix[] = " UNSIGNED";
    size_t logical_length = logical_type == NULL ? 0U : strlen(logical_type);
    size_t suffix_length = sizeof(suffix) - 1U;

    return (logical_length >= suffix_length &&
            strcmp(&logical_type[logical_length - suffix_length], suffix) == 0) != 0;
}
