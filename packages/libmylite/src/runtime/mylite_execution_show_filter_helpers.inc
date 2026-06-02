static int make_show_like_filter(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *pattern_node,
    struct show_like_filter *out_filter
) {
    char *pattern = NULL;
    int rc = MYLITE_OK;

    if (out_filter == NULL) {
        set_runtime_error(database, "invalid SHOW LIKE filter");
        return MYLITE_ERROR;
    }
    *out_filter = (struct show_like_filter){
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
    if (pattern_node == NULL) {
        return MYLITE_OK;
    }

    rc = decode_show_like_pattern(database, pattern_node, &pattern);
    if (rc != MYLITE_OK) {
        return rc;
    }

    out_filter->has_pattern = true;
    out_filter->pattern = pattern;
    out_filter->pattern_length = strlen(pattern);
    return MYLITE_OK;
}

static void show_like_filter_deinit(struct show_like_filter *filter) {
    if (filter == NULL) {
        return;
    }

    free(filter->pattern);
    *filter = (struct show_like_filter){
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
}

static bool show_like_filter_matches(
    const struct show_like_filter *filter,
    const char *value,
    bool case_sensitive
) {
    if (filter == NULL || !filter->has_pattern) {
        return true;
    }
    if (value == NULL) {
        return false;
    }

    return show_like_pattern_matches(
        filter->pattern,
        filter->pattern_length,
        value,
        strlen(value),
        case_sensitive
    );
}
