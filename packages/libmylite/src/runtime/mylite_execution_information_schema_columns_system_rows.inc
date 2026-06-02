static int append_information_schema_columns_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t table_index = 0U;
         rc == MYLITE_OK &&
         table_index < mylite_execution_catalog_information_schema_table_definition_count();
         ++table_index) {
        rc = append_information_schema_columns_system_table_rows(
            database,
            rows,
            mylite_execution_catalog_information_schema_table_definition_at(table_index)
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_information_schema_columns_mysql_system_rows(database, rows);
    }
    return rc;
}

static int append_information_schema_columns_system_table_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_execution_catalog_table_definition *definition
) {
    int rc = MYLITE_OK;

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < definition->column_count;
         ++column_index) {
        const struct mylite_execution_catalog_column_definition *column =
            &definition->columns[column_index];
        char ordinal_text[integer_text_capacity];
        const char *values[information_schema_columns_column_count] = {
            "def",
            "information_schema",
            definition->name,
            column->name,
            ordinal_text,
            column->column_default,
            column->is_nullable,
            column->data_type,
            column->character_maximum_length,
            column->character_octet_length,
            column->numeric_precision,
            column->numeric_scale,
            column->datetime_precision,
            column->character_set_name,
            column->collation_name,
            column->column_type,
            "",
            "",
            "select",
            "",
            "",
            NULL,
        };

        rc = information_schema_format_i64(
            database,
            (int64_t)column_index + 1,
            ordinal_text,
            sizeof(ordinal_text)
        );
        if (rc == MYLITE_OK) {
            rc = append_information_schema_row(database, rows, values);
        }
    }
    return rc;
}

static int append_information_schema_columns_mysql_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t table_index = 0U;
         rc == MYLITE_OK &&
         table_index < mylite_execution_catalog_mysql_system_table_definition_count();
         ++table_index) {
        rc = append_information_schema_columns_mysql_system_table_rows(
            database,
            rows,
            mylite_execution_catalog_mysql_system_table_definition_at(table_index)
        );
    }
    return rc;
}

static int append_information_schema_columns_mysql_system_table_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_execution_catalog_mysql_system_table *definition
) {
    int rc = MYLITE_OK;

    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < definition->query_definition.column_count;
         ++column_index) {
        const struct mylite_execution_catalog_column_definition *column =
            &definition->query_definition.columns[column_index];
        char ordinal_text[integer_text_capacity];
        const char *values[information_schema_columns_column_count] = {
            "def",
            definition->schema_name,
            definition->query_definition.name,
            column->name,
            ordinal_text,
            column->column_default,
            column->is_nullable,
            column->data_type,
            column->character_maximum_length,
            column->character_octet_length,
            column->numeric_precision,
            column->numeric_scale,
            column->datetime_precision,
            column->character_set_name,
            column->collation_name,
            column->column_type,
            definition->column_keys[column_index],
            definition->column_extras[column_index],
            definition->column_privileges[column_index],
            mysql_system_table_column_comment(definition, column_index),
            mysql_system_table_column_generation_expression(definition, column_index),
            NULL,
        };

        rc = information_schema_format_i64(
            database,
            (int64_t)column_index + 1,
            ordinal_text,
            sizeof(ordinal_text)
        );
        if (rc == MYLITE_OK) {
            rc = append_information_schema_row(database, rows, values);
        }
    }
    return rc;
}

static int append_information_schema_columns_extensions_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t table_index = 0U;
         rc == MYLITE_OK &&
         table_index < mylite_execution_catalog_information_schema_table_definition_count();
         ++table_index) {
        rc = append_information_schema_columns_extensions_system_table_rows(
            database,
            rows,
            mylite_execution_catalog_information_schema_table_definition_at(table_index)
        );
    }
    return rc;
}

static int append_information_schema_columns_extensions_system_table_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_execution_catalog_table_definition *definition
) {
    int rc = MYLITE_OK;

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < definition->column_count;
         ++column_index) {
        const struct mylite_execution_catalog_column_definition *column =
            &definition->columns[column_index];
        const char *values[information_schema_columns_extensions_column_count] = {
            "def",
            "information_schema",
            definition->name,
            column->name,
            NULL,
            NULL,
        };

        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}
