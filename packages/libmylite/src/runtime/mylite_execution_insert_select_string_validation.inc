
static int validate_insert_select_convertible_string_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    struct insert_select_string_validation validation
) {
    const unsigned char *text = NULL;
    int byte_count = 0;
    size_t text_length = 0U;

    if (sqlite3_column_type(statement, selected_column_index) != SQLITE_TEXT) {
        set_unsupported_error(
            database,
            "INSERT ... SELECT does not support implicit string conversion"
        );
        return MYLITE_ERROR;
    }
    text = sqlite3_column_text(statement, selected_column_index);
    byte_count = sqlite3_column_bytes(statement, selected_column_index);
    if (text == NULL || byte_count < 0) {
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }
    text_length = (size_t)byte_count;
    if (column_descriptor_is_varchar(validation.target_column)) {
        return validate_insert_select_convertible_varchar_value(
            database,
            (const char *)text,
            text_length,
            validation
        );
    }
    if (column_descriptor_is_char(validation.target_column)) {
        return validate_insert_select_convertible_char_value(
            database,
            (const char *)text,
            text_length,
            validation
        );
    }
    if (column_descriptor_is_text_family(validation.target_column)) {
        return validate_insert_select_convertible_text_family_value(
            database,
            (const char *)text,
            text_length,
            validation
        );
    }

    set_unsupported_error(
        database,
        "INSERT ... SELECT does not support implicit string conversion"
    );
    return MYLITE_ERROR;
}

static int validate_insert_select_convertible_varchar_value(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    struct insert_select_string_validation validation
) {
    size_t declared_length = 0U;
    size_t character_count = 0U;
    size_t truncated_length = 0U;
    int rc = MYLITE_OK;

    rc = varchar_length_for_column(database, validation.target_column, &declared_length);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = utf8_prefix_byte_length_for_character_count(
        (struct utf8_prefix_request){
            .text = text,
            .text_length = text_length,
            .character_limit = declared_length,
        },
        &truncated_length,
        &character_count
    );
    if (rc != MYLITE_OK) {
        set_unsupported_error(database, "VARCHAR values must be valid UTF-8");
        return MYLITE_ERROR;
    }
    if (character_count <= declared_length || validation.adjust_value) {
        return MYLITE_OK;
    }
    if (text_is_ascii_spaces(text + truncated_length, text_length - truncated_length)) {
        if (validation.source_column == NULL ||
            !column_descriptor_is_varchar(validation.source_column)) {
            return MYLITE_OK;
        }
        set_data_truncated_error(database, validation.target_column->name, validation.row_number);
        return MYLITE_ERROR;
    }
    if (validation.source_column != NULL &&
        column_descriptor_is_varchar(validation.source_column)) {
        set_data_truncated_error(database, validation.target_column->name, validation.row_number);
    } else {
        set_data_too_long_error(database, validation.target_column->name, validation.row_number);
    }

    return MYLITE_ERROR;
}

static int validate_insert_select_convertible_char_value(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    struct insert_select_string_validation validation
) {
    size_t declared_length = 0U;
    size_t character_count = 0U;
    size_t trimmed_length = 0U;
    size_t trailing_space_count = 0U;
    int rc = MYLITE_OK;

    rc = char_length_for_column(database, validation.target_column, &declared_length);
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = validate_utf8_text(text, text_length, &character_count);
    if (rc != MYLITE_OK) {
        set_unsupported_error(database, "CHAR values must be valid UTF-8");
        return MYLITE_ERROR;
    }

    trimmed_length = text_length;
    while (trimmed_length > 0U && text[trimmed_length - 1U] == ' ') {
        --trimmed_length;
        ++trailing_space_count;
    }
    if (trailing_space_count <= character_count &&
        character_count - trailing_space_count <= declared_length) {
        return MYLITE_OK;
    }
    if (validation.adjust_value) {
        return MYLITE_OK;
    }
    set_data_too_long_error(database, validation.target_column->name, validation.row_number);
    return MYLITE_ERROR;
}

static int validate_insert_select_convertible_text_family_value(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    struct insert_select_string_validation validation
) {
    const struct text_family_type_info *info = NULL;
    size_t character_count = 0U;
    size_t truncated_length = 0U;
    int rc = MYLITE_OK;

    rc = validate_utf8_text(text, text_length, &character_count);
    if (rc != MYLITE_OK) {
        set_unsupported_error(database, "TEXT values must be valid UTF-8");
        return MYLITE_ERROR;
    }
    (void)character_count;
    info = text_family_type_info_for_logical_type(validation.target_column->logical_type);
    if (info == NULL) {
        set_unsupported_error(database, "statement supports only baseline TEXT descriptors");
        return MYLITE_ERROR;
    }
    if (text_length <= info->maximum_length || validation.adjust_value) {
        return MYLITE_OK;
    }
    if (validation.source_column != NULL) {
        if (column_descriptor_is_text_family(validation.source_column)) {
            set_data_too_long_error(
                database,
                validation.target_column->name,
                validation.row_number
            );
            return MYLITE_ERROR;
        }

        return validate_text_family_text(
            database,
            validation.target_column,
            (struct varchar_text_validation){
                .text = text,
                .text_length = text_length,
                .row_number = validation.row_number,
            }
        );
    }

    rc = utf8_prefix_byte_length_for_byte_limit(
        (struct utf8_byte_prefix_request){
            .text = text,
            .text_length = text_length,
            .byte_limit = (size_t)info->maximum_length,
        },
        &truncated_length
    );
    if (rc != MYLITE_OK) {
        set_unsupported_error(database, "TEXT values must be valid UTF-8");
        return MYLITE_ERROR;
    }
    if (text_is_ascii_spaces(text + truncated_length, text_length - truncated_length)) {
        return MYLITE_OK;
    }

    set_data_too_long_error(database, validation.target_column->name, validation.row_number);
    return MYLITE_ERROR;
}

static int validate_insert_select_string_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number
) {
    const unsigned char *text = NULL;
    int byte_count = 0;

    if (sqlite3_column_type(statement, selected_column_index) != SQLITE_TEXT) {
        set_unsupported_error(
            database,
            "INSERT ... SELECT does not support implicit string conversion"
        );
        return MYLITE_ERROR;
    }
    text = sqlite3_column_text(statement, selected_column_index);
    byte_count = sqlite3_column_bytes(statement, selected_column_index);
    if (text == NULL || byte_count < 0) {
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }
    if (column_descriptor_is_char(target_column)) {
        return validate_canonical_char_text(
            database,
            target_column,
            (struct varchar_text_validation){
                .text = (const char *)text,
                .text_length = (size_t)byte_count,
                .row_number = row_number,
            }
        );
    }
    if (column_descriptor_is_varchar(target_column)) {
        return validate_varchar_text(
            database,
            target_column,
            (struct varchar_text_validation){
                .text = (const char *)text,
                .text_length = (size_t)byte_count,
                .row_number = row_number,
            }
        );
    }
    return validate_text_family_text(
        database,
        target_column,
        (struct varchar_text_validation){
            .text = (const char *)text,
            .text_length = (size_t)byte_count,
            .row_number = row_number,
        }
    );
}
