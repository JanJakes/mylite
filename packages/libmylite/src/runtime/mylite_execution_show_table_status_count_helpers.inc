static int read_show_table_status_row_count(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table,
    int64_t *out_count
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int rc = build_show_table_status_count_sql(table, &sql);

    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = step_count_statement(statement, out_count);
    }

    rc = finalize_sqlite_statement(statement, rc);
    free(sql);

    return rc;
}

static int build_show_table_status_count_sql(
    const struct mylite_catalog_table_descriptor *table,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    if (table == NULL || out_sql == NULL) {
        return MYLITE_MISUSE;
    }
    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT COUNT(*) FROM ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, table->physical_name);
    }
    if (rc == MYLITE_OK) {
        *out_sql = mylite_dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    mylite_dynamic_string_deinit(&string);

    return rc;
}

static int format_show_table_status_integer(
    struct mylite_db *database,
    int64_t value,
    char *buffer,
    size_t buffer_size
) {
    int written = snprintf(buffer, buffer_size, "%" PRId64, value);

    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format SHOW TABLE STATUS value");
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}
