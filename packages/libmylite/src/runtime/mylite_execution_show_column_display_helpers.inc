static int show_column_type_text(
    struct mylite_db *database,
    const char *logical_type,
    char *buffer,
    size_t buffer_size,
    const char **out_type_text
) {
    return show_descriptor_type_text(
        database,
        logical_type,
        buffer,
        buffer_size,
        "SHOW COLUMNS supports only integer, BIT, string, decimal, approximate numeric, DATE, "
        "TIME, DATETIME, and TIMESTAMP column descriptors",
        out_type_text
    );
}

static const char *show_column_collation_text(
    const struct mylite_catalog_table_descriptor *table,
    const struct mylite_catalog_column_descriptor *column
) {
    if (column_descriptor_is_string_family(column) || column_descriptor_is_enum(column) ||
        column_descriptor_is_set(column)) {
        return column_effective_collation_name(table, column);
    }

    return NULL;
}
