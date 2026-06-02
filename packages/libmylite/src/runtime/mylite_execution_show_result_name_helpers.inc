static int build_show_databases_column_name(
    const struct show_like_filter *filter,
    char **out_name
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    if (out_name == NULL) {
        return MYLITE_MISUSE;
    }
    *out_name = NULL;

    mylite_dynamic_string_init(&string);
    rc = mylite_dynamic_string_append(&string, "Database");
    if (rc == MYLITE_OK && filter != NULL && filter->has_pattern) {
        rc = mylite_dynamic_string_append(&string, " (");
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(&string, filter->pattern);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(&string, ')');
        }
    }
    if (rc == MYLITE_OK) {
        *out_name = mylite_dynamic_string_take(&string);
        if (*out_name == NULL) {
            rc = MYLITE_NOMEM;
        }
    }
    mylite_dynamic_string_deinit(&string);
    return rc;
}

static int build_show_tables_column_name(
    const char *schema_name,
    const struct show_like_filter *filter,
    char **out_name
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    if (schema_name == NULL || out_name == NULL) {
        return MYLITE_MISUSE;
    }
    *out_name = NULL;

    mylite_dynamic_string_init(&string);
    rc = mylite_dynamic_string_append(&string, "Tables_in_");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, schema_name);
    }
    if (rc == MYLITE_OK && filter != NULL && filter->has_pattern) {
        rc = mylite_dynamic_string_append(&string, " (");
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(&string, filter->pattern);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(&string, ')');
        }
    }
    if (rc == MYLITE_OK) {
        *out_name = mylite_dynamic_string_take(&string);
        if (*out_name == NULL) {
            rc = MYLITE_NOMEM;
        }
    }
    mylite_dynamic_string_deinit(&string);
    return rc;
}
