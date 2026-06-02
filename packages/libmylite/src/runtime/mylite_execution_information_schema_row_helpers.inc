static int append_information_schema_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const char *const *values
) {
    char ***grown_rows = NULL;
    char **row = NULL;
    size_t required_count = rows->row_count + 1U;

    if (required_count > SIZE_MAX / sizeof(*grown_rows)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    grown_rows = (char ***)realloc((void *)rows->rows, required_count * sizeof(*grown_rows));
    if (grown_rows == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    rows->rows = grown_rows;

    row = (char **)calloc(rows->definition->column_count, sizeof(*row));
    if (row == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    for (size_t column_index = 0U; column_index < rows->definition->column_count; ++column_index) {
        if (values[column_index] == NULL) {
            continue;
        }
        int rc = duplicate_text(database, values[column_index], &row[column_index]);

        if (rc != MYLITE_OK) {
            for (size_t rollback_index = 0U; rollback_index < column_index; ++rollback_index) {
                free(row[rollback_index]);
            }
            free((void *)row);
            return rc;
        }
    }

    rows->rows[rows->row_count] = row;
    rows->row_count = required_count;
    return MYLITE_OK;
}
