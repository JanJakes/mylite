static int append_show_result_column(
    struct mylite_db *database,
    mylite_result *result,
    const char *label
) {
    static const struct show_result_column_definition default_definition = {
        .logical_type = MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING,
        .flags = 0U,
        .display_length = 1024U,
        .decimals = 0U,
        .nullable = true,
        .uses_connection_collation = true,
    };

    return append_show_result_columns(database, result, &label, &default_definition, 1U);
}

static int append_show_result_columns(
    struct mylite_db *database,
    mylite_result *result,
    const char *const *labels,
    const struct show_result_column_definition *definitions,
    size_t column_count
) {
    int rc = MYLITE_OK;

    if (database == NULL || result == NULL || labels == NULL || definitions == NULL) {
        return MYLITE_MISUSE;
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < column_count; ++index) {
        const struct show_result_column_definition *definition = &definitions[index];
        struct mylite_result_column_descriptor descriptor = {
            .label = labels[index],
            .schema_name = "",
            .table_name = "",
            .origin_schema_name = "",
            .origin_table_name = "",
            .origin_column_name = "",
            .logical_type = definition->logical_type,
            .flags = definition->flags,
            .charset_id = mysql_collation_binary_id,
            .collation_id = mysql_collation_binary_id,
            .display_length = definition->display_length,
            .decimals = definition->decimals,
            .nullable = definition->nullable,
        };

        if (definition->uses_connection_collation) {
            const uint64_t max_bytes_per_character =
                result_metadata_collation_max_bytes_per_character(
                    mylite_connection_statement_collation_connection(database)
                );

            descriptor.charset_id = result_metadata_collation_id(
                mylite_connection_statement_collation_connection(database)
            );
            descriptor.collation_id = descriptor.charset_id;
            if (descriptor.display_length > UINT64_MAX / max_bytes_per_character) {
                descriptor.display_length = UINT32_MAX;
            } else {
                descriptor.display_length = result_metadata_display_length_cap(
                    descriptor.display_length * max_bytes_per_character
                );
            }
        }
        rc = mylite_result_append_column_descriptor(result, &descriptor);
    }
    return rc;
}
