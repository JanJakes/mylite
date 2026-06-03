
static int information_schema_compare_text(
    const struct mylite_execution_catalog_table_definition *definition,
    size_t column_index,
    const char *left,
    const char *right
) {
    return information_schema_compare_column_text(&definition->columns[column_index], left, right);
}

static int information_schema_compare_column_text(
    const struct mylite_execution_catalog_column_definition *column,
    const char *left,
    const char *right
) {
    if (information_schema_column_uses_case_insensitive_collation(column)) {
        return compare_ascii_case_insensitive_text(left, right);
    }
    return strcmp(left, right);
}

static int compare_ascii_case_insensitive_text(const char *left, const char *right) {
    size_t index = 0U;

    while (left[index] != '\0' && right[index] != '\0') {
        unsigned char left_byte = (unsigned char)ascii_lower((unsigned char)left[index]);
        unsigned char right_byte = (unsigned char)ascii_lower((unsigned char)right[index]);

        if (left_byte < right_byte) {
            return -1;
        }
        if (left_byte > right_byte) {
            return 1;
        }
        ++index;
    }
    if (left[index] == '\0' && right[index] == '\0') {
        return 0;
    }
    if (left[index] == '\0') {
        return -1;
    }
    return 1;
}

static bool information_schema_column_uses_case_insensitive_collation(
    const struct mylite_execution_catalog_column_definition *column
) {
    const char *collation = column->collation_name;

    if (collation == NULL) {
        return false;
    }
    if (text_equals_ascii_case_insensitive(collation, "utf8mb3_general_ci") ||
        text_equals_ascii_case_insensitive(collation, "utf8mb3_tolower_ci") ||
        text_equals_ascii_case_insensitive(collation, "utf8mb4_0900_ai_ci") ||
        text_equals_ascii_case_insensitive(collation, "utf8mb4_general_ci") ||
        text_equals_ascii_case_insensitive(collation, "utf8mb4_unicode_ci") ||
        text_equals_ascii_case_insensitive(collation, "utf8mb4_unicode_520_ci")) {
        return true;
    }
    return false;
}

static bool information_schema_column_is_numeric(
    const struct mylite_execution_catalog_table_definition *definition,
    size_t column_index
) {
    const char *data_type = definition->columns[column_index].data_type;

    if (data_type == NULL) {
        return false;
    }
    if (strcmp(data_type, "int") == 0 || strcmp(data_type, "bigint") == 0) {
        return true;
    }
    return false;
}

static int information_schema_text_to_i64(const char *text, int64_t *out_value) {
    char *end = NULL;
    long long value = 0;

    if (text == NULL || text[0] == '\0') {
        return MYLITE_ERROR;
    }
    value = strtoll(text, &end, decimal_base);
    if (end == NULL || *end != '\0') {
        return MYLITE_ERROR;
    }
    *out_value = (int64_t)value;
    return MYLITE_OK;
}

static int information_schema_format_i64(
    struct mylite_db *database,
    int64_t value,
    char *buffer,
    size_t buffer_size
) {
    int written = snprintf(buffer, buffer_size, "%" PRId64, value);

    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format INFORMATION_SCHEMA value");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}
