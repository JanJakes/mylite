static int append_information_schema_innodb_virtual_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    size_t column_count = 0U;
    size_t virtual_index = 0U;
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_innodb_virtual_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_VIRTUAL columns");
        return MYLITE_ERROR;
    }

    rc = mylite_execution_load_table_columns(database, table->table_id, &columns, &column_count);
    for (size_t index = 0U; rc == MYLITE_OK && index < column_count; ++index) {
        const struct mylite_catalog_column_descriptor *column = &columns[index];

        if (!column->is_generated ||
            column->generated_kind != MYLITE_CATALOG_GENERATED_COLUMN_VIRTUAL) {
            continue;
        }
        rc = append_information_schema_innodb_virtual_column_rows(
            database,
            rows,
            table,
            columns,
            column_count,
            column,
            virtual_index
        );
        ++virtual_index;
    }

    free(columns);
    return rc;
}

static int append_information_schema_innodb_virtual_column_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const struct mylite_catalog_column_descriptor *column,
    size_t virtual_index
) {
    bool *base_positions = NULL;
    size_t base_position_count = 0U;
    int64_t position = 0;
    int rc = MYLITE_OK;

    if (column_count == 0U) {
        return MYLITE_OK;
    }
    base_positions = calloc(column_count, sizeof(*base_positions));
    if (base_positions == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = collect_information_schema_innodb_virtual_base_positions(
        database,
        columns,
        column_count,
        column,
        base_positions,
        &base_position_count
    );
    if (rc == MYLITE_OK && base_position_count != 0U) {
        rc = information_schema_innodb_virtual_position(database, column, virtual_index, &position);
    }
    for (size_t base_position = 0U; rc == MYLITE_OK && base_position < column_count;
         ++base_position) {
        if (!base_positions[base_position]) {
            continue;
        }
        if (base_position > (size_t)INT64_MAX) {
            set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_VIRTUAL base position");
            rc = MYLITE_ERROR;
            break;
        }
        rc = append_information_schema_innodb_virtual_dependency_row(
            database,
            rows,
            table->table_id,
            position,
            (int64_t)base_position
        );
    }

    free(base_positions);
    return rc;
}

static int collect_information_schema_innodb_virtual_base_positions(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const struct mylite_catalog_column_descriptor *column,
    bool *base_positions,
    size_t *out_base_position_count
) {
    const char *expression = column == NULL ? NULL : column->generation_expression;
    size_t expression_index = 0U;
    int rc = MYLITE_OK;

    *out_base_position_count = 0U;
    if (expression == NULL || expression[0] == '\0') {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_VIRTUAL expression");
        return MYLITE_ERROR;
    }

    while (rc == MYLITE_OK && expression[expression_index] != '\0') {
        char identifier[MYLITE_CATALOG_IDENTIFIER_CAPACITY];

        if (expression[expression_index] != '`') {
            ++expression_index;
            continue;
        }
        rc = copy_information_schema_innodb_virtual_identifier(
            database,
            expression,
            &expression_index,
            identifier,
            sizeof(identifier)
        );
        if (rc == MYLITE_OK) {
            rc = collect_information_schema_innodb_virtual_base_identifier(
                database,
                columns,
                column_count,
                identifier,
                base_positions,
                out_base_position_count
            );
        }
    }

    return rc;
}

static int collect_information_schema_innodb_virtual_base_identifier(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const char *identifier,
    bool *base_positions,
    size_t *base_position_count
) {
    const struct mylite_catalog_column_descriptor *base_column = NULL;
    int64_t base_position = 0;
    int rc = find_information_schema_innodb_virtual_base_column(
        columns,
        column_count,
        identifier,
        &base_column
    );

    if (rc != MYLITE_OK || base_column == NULL || base_column->is_generated) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_VIRTUAL base column");
        return MYLITE_ERROR;
    }
    rc = information_schema_innodb_column_position(database, base_column, &base_position);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (base_position < 0 || (uint64_t)base_position >= column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_VIRTUAL base position");
        return MYLITE_ERROR;
    }
    if (!base_positions[base_position]) {
        base_positions[base_position] = true;
        ++(*base_position_count);
    }
    return MYLITE_OK;
}

static int copy_information_schema_innodb_virtual_identifier(
    struct mylite_db *database,
    const char *expression,
    size_t *expression_index,
    char *identifier,
    size_t identifier_size
) {
    size_t identifier_index = 0U;
    size_t index = *expression_index + 1U;

    while (expression[index] != '\0') {
        if (expression[index] == '`') {
            if (expression[index + 1U] == '`') {
                if (identifier_index + 1U >= identifier_size) {
                    set_runtime_error(
                        database,
                        "invalid INFORMATION_SCHEMA.INNODB_VIRTUAL identifier"
                    );
                    return MYLITE_ERROR;
                }
                identifier[identifier_index] = '`';
                ++identifier_index;
                index += 2U;
                continue;
            }
            if (identifier_index == 0U) {
                set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_VIRTUAL identifier");
                return MYLITE_ERROR;
            }
            identifier[identifier_index] = '\0';
            *expression_index = index + 1U;
            return MYLITE_OK;
        }
        if (identifier_index + 1U >= identifier_size) {
            set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_VIRTUAL identifier");
            return MYLITE_ERROR;
        }
        identifier[identifier_index] = expression[index];
        ++identifier_index;
        ++index;
    }

    set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_VIRTUAL identifier");
    return MYLITE_ERROR;
}

static int find_information_schema_innodb_virtual_base_column(
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const char *identifier,
    const struct mylite_catalog_column_descriptor **out_column
) {
    *out_column = NULL;
    for (size_t index = 0U; index < column_count; ++index) {
        if (text_equals_ascii_case_insensitive(columns[index].name, identifier)) {
            *out_column = &columns[index];
            return MYLITE_OK;
        }
    }
    return MYLITE_ERROR;
}

static int information_schema_innodb_virtual_position(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    size_t virtual_index,
    int64_t *out_position
) {
    const int64_t virtual_position_shift = 65536;
    int64_t column_position = 0;
    uint64_t virtual_number = (uint64_t)virtual_index + 1U;
    int rc = information_schema_innodb_column_position(database, column, &column_position);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (column_position < 0 ||
        virtual_number > (uint64_t)((INT64_MAX - column_position) / virtual_position_shift)) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_VIRTUAL position");
        return MYLITE_ERROR;
    }

    *out_position = (int64_t)(virtual_number * (uint64_t)virtual_position_shift) + column_position;
    return MYLITE_OK;
}

static int append_information_schema_innodb_virtual_dependency_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    int64_t table_id,
    int64_t position,
    int64_t base_position
) {
    char table_id_text[integer_text_capacity];
    char position_text[integer_text_capacity];
    char base_position_text[integer_text_capacity];
    int rc =
        information_schema_format_i64(database, table_id, table_id_text, sizeof(table_id_text));

    if (rc == MYLITE_OK) {
        rc =
            information_schema_format_i64(database, position, position_text, sizeof(position_text));
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            base_position,
            base_position_text,
            sizeof(base_position_text)
        );
    }
    if (rc == MYLITE_OK) {
        const char *values[information_schema_innodb_virtual_column_count] = {
            table_id_text,
            position_text,
            base_position_text,
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}
