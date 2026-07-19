static int append_information_schema_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const char *const *values
) {
    char **row = NULL;
    size_t required_count = 0U;
    bool matches = true;
    int rc = MYLITE_OK;

    if (rows->exclude_connection_control_failed_login_attempts &&
        information_schema_row_is_connection_control_failed_login_attempts(values)) {
        return MYLITE_OK;
    }
    if (rows->predicate != NULL && rows->predicate->instruction_count != 0U) {
        rc = mylite_execution_information_schema_predicate_matches(
            database,
            rows->query,
            rows->predicate,
            values,
            &matches
        );
        if (rc != MYLITE_OK || !matches) {
            return rc;
        }
    }
    if (rows->row_count == SIZE_MAX) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (rows->count_only) {
        ++rows->row_count;
        return MYLITE_OK;
    }
    if (rows->has_row_limit && rows->row_limit == 0U) {
        return MYLITE_DONE;
    }
    if (rows->has_row_limit && rows->skipped_row_count < rows->row_offset) {
        ++rows->skipped_row_count;
        return MYLITE_OK;
    }
    if (rows->has_row_limit && rows->row_count >= rows->row_limit) {
        return MYLITE_DONE;
    }
    if (rows->stream_result != NULL) {
        rc = information_schema_append_projected_result_row(
            database,
            rows->query,
            values,
            rows->stream_buffers,
            rows->stream_result
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        ++rows->row_count;
        return rows->has_row_limit && rows->row_count >= rows->row_limit ? MYLITE_DONE : MYLITE_OK;
    }
    required_count = rows->row_count + 1U;
    rc = reserve_information_schema_rows(database, rows, required_count);
    if (rc != MYLITE_OK) {
        return rc;
    }

    row = (char **)calloc(rows->definition->column_count, sizeof(*row));
    if (row == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    for (size_t column_index = 0U; column_index < rows->definition->column_count; ++column_index) {
        if ((rows->required_columns != NULL && !rows->required_columns[column_index]) ||
            values[column_index] == NULL) {
            continue;
        }
        rc = duplicate_text(database, values[column_index], &row[column_index]);

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
    return rows->has_row_limit && rows->row_count >= rows->row_limit ? MYLITE_DONE : MYLITE_OK;
}

static int reserve_information_schema_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    size_t required_capacity
) {
    enum { initial_row_capacity = 16 };

    char ***grown_rows = NULL;
    size_t capacity = 0U;

    if (rows == NULL) {
        return MYLITE_MISUSE;
    }
    if (required_capacity <= rows->row_capacity) {
        return MYLITE_OK;
    }
    capacity = rows->row_capacity == 0U ? initial_row_capacity : rows->row_capacity;
    while (capacity < required_capacity) {
        if (capacity > SIZE_MAX / 2U) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        capacity *= 2U;
    }
    if (capacity > SIZE_MAX / sizeof(*grown_rows)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    grown_rows = (char ***)realloc((void *)rows->rows, capacity * sizeof(*grown_rows));
    if (grown_rows == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    rows->rows = grown_rows;
    rows->row_capacity = capacity;
    return MYLITE_OK;
}
