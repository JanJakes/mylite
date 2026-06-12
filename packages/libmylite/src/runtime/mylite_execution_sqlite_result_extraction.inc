static int append_selected_sqlite_row(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    mylite_result *result,
    const struct mylite_catalog_column_descriptor *columns,
    size_t descriptor_count
) {
    size_t column_count = mylite_result_column_count(result);
    struct mylite_result_cell *values = NULL;
    char *texts = NULL;
    size_t text_capacity = approximate_numeric_text_capacity;
    int rc = MYLITE_OK;

    if (column_count > (size_t)INT_MAX || column_count > SIZE_MAX / text_capacity) {
        return MYLITE_NOMEM;
    }

    values = calloc(column_count, sizeof(*values));
    texts = calloc(column_count, text_capacity);
    if (values == NULL || texts == NULL) {
        free(values);
        free(texts);
        return MYLITE_NOMEM;
    }

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < column_count; ++column_index) {
        char *text = &texts[column_index * text_capacity];

        rc = append_selected_sqlite_row_value(
            database,
            statement,
            column_index,
            columns,
            descriptor_count,
            text,
            text_capacity,
            &values[column_index]
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_bytes_row(result, values);
    }

    free(values);
    free(texts);

    return rc;
}

static int append_selected_sqlite_row_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    size_t column_index,
    const struct mylite_catalog_column_descriptor *columns,
    size_t descriptor_count,
    char *text,
    size_t text_capacity,
    struct mylite_result_cell *out_value
) {
    const struct mylite_catalog_column_descriptor *column =
        columns != NULL && column_index < descriptor_count ? &columns[column_index] : NULL;

    return append_selected_sqlite_row_value_with_descriptor(
        database,
        statement,
        column_index,
        column,
        text,
        text_capacity,
        out_value
    );
}

static int append_selected_sqlite_row_value_with_descriptor(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    size_t column_index,
    const struct mylite_catalog_column_descriptor *column,
    char *text,
    size_t text_capacity,
    struct mylite_result_cell *out_value
) {
    int sqlite_type = SQLITE_NULL;

    if (column_index > (size_t)INT_MAX || out_value == NULL) {
        return MYLITE_ERROR;
    }
    *out_value = (struct mylite_result_cell){.bytes = NULL, .size = 0U, .is_null = true};

    sqlite_type = sqlite3_column_type(statement, (int)column_index);
    if (sqlite_type == SQLITE_NULL) {
        return MYLITE_OK;
    }
    if (sqlite_type == SQLITE_INTEGER) {
        return append_selected_sqlite_integer_value(
            statement,
            column_index,
            text,
            text_capacity,
            out_value
        );
    }
    if (sqlite_type == SQLITE_FLOAT) {
        return append_selected_sqlite_float_value(
            database,
            statement,
            column_index,
            column,
            text,
            text_capacity,
            out_value
        );
    }
    if (sqlite_type == SQLITE_TEXT) {
        return append_selected_sqlite_text_value(statement, column_index, out_value);
    }
    if (sqlite_type == SQLITE_BLOB) {
        return append_selected_sqlite_blob_value(statement, column_index, out_value);
    }

    return MYLITE_ERROR;
}

static int append_selected_sqlite_integer_value(
    sqlite3_stmt *statement,
    size_t column_index,
    char *text,
    size_t text_capacity,
    struct mylite_result_cell *out_value
) {
    int written = snprintf(
        text,
        text_capacity,
        "%" PRId64,
        (int64_t)sqlite3_column_int64(statement, (int)column_index)
    );

    if (written < 0 || (size_t)written >= text_capacity) {
        return MYLITE_ERROR;
    }
    *out_value = (struct mylite_result_cell){
        .bytes = text,
        .size = (size_t)written,
        .is_null = false,
    };
    return MYLITE_OK;
}

static int append_selected_sqlite_float_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    size_t column_index,
    const struct mylite_catalog_column_descriptor *column,
    char *text,
    size_t text_capacity,
    struct mylite_result_cell *out_value
) {
    double value = sqlite3_column_double(statement, (int)column_index);
    struct approximate_type_info info = {
        .type_class = APPROXIMATE_TYPE_DOUBLE,
        .is_unsigned = false,
    };
    int rc = MYLITE_OK;

    if (column != NULL && column_descriptor_is_approximate(column)) {
        rc = approximate_type_info_for_logical_type(column->logical_type, &info);
    }
    if (rc == MYLITE_OK) {
        rc = format_approximate_value_text(
            database,
            value,
            &info,
            "approximate result",
            text,
            text_capacity
        );
    }
    if (rc == MYLITE_OK) {
        *out_value = (struct mylite_result_cell){
            .bytes = text,
            .size = strlen(text),
            .is_null = false,
        };
    }

    return rc;
}

static int append_selected_sqlite_text_value(
    sqlite3_stmt *statement,
    size_t column_index,
    struct mylite_result_cell *out_value
) {
    const unsigned char *sqlite_text = sqlite3_column_text(statement, (int)column_index);
    int byte_count = sqlite3_column_bytes(statement, (int)column_index);
    size_t character_count = 0U;

    if (sqlite_text == NULL || byte_count < 0 ||
        validate_utf8_text((const char *)sqlite_text, (size_t)byte_count, &character_count) !=
            MYLITE_OK) {
        return MYLITE_ERROR;
    }

    (void)character_count;
    *out_value = (struct mylite_result_cell){
        .bytes = sqlite_text,
        .size = (size_t)byte_count,
        .is_null = false,
    };
    return MYLITE_OK;
}

static int append_selected_sqlite_blob_value(
    sqlite3_stmt *statement,
    size_t column_index,
    struct mylite_result_cell *out_value
) {
    const void *sqlite_blob = sqlite3_column_blob(statement, (int)column_index);
    int byte_count = sqlite3_column_bytes(statement, (int)column_index);

    if ((sqlite_blob == NULL && byte_count != 0) || byte_count < 0) {
        return MYLITE_ERROR;
    }

    *out_value = (struct mylite_result_cell){
        .bytes = sqlite_blob,
        .size = (size_t)byte_count,
        .is_null = false,
    };
    return MYLITE_OK;
}

static int choose_sqlite_rowid_alias(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const char *unsupported_message,
    const char **out_alias
) {
    static const char *const rowid_aliases[] = {"rowid", "_rowid_", "oid"};

    if (out_alias == NULL) {
        return MYLITE_MISUSE;
    }
    *out_alias = NULL;
    for (size_t index = 0U; index < sizeof(rowid_aliases) / sizeof(rowid_aliases[0]); ++index) {
        if (!column_name_exists(columns, column_count, rowid_aliases[index])) {
            *out_alias = rowid_aliases[index];
            return MYLITE_OK;
        }
    }

    set_unsupported_error(database, unsupported_message);
    return MYLITE_ERROR;
}

static bool column_name_exists(
    const struct mylite_catalog_column_descriptor *columns,
    size_t column_count,
    const char *name
) {
    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        if (text_equals_ascii_case_insensitive(columns[column_index].name, name)) {
            return true;
        }
    }

    return false;
}

static const struct mylite_sql_ast_node *child_at(
    const struct mylite_sql_ast_node *node,
    size_t index
) {
    const struct mylite_sql_ast_node *child = NULL;

    if (node == NULL) {
        return NULL;
    }

    child = node->first_child;
    for (size_t current = 0U; current < index && child != NULL; ++current) {
        child = child->next_sibling;
    }
    return child;
}

static const struct mylite_sql_ast_node *child_with_kind(
    const struct mylite_sql_ast_node *node,
    enum mylite_sql_ast_node_kind kind
) {
    const struct mylite_sql_ast_node *child = NULL;

    if (node == NULL) {
        return NULL;
    }

    child = node->first_child;
    while (child != NULL) {
        if (child->kind == kind) {
            return child;
        }
        child = child->next_sibling;
    }
    return NULL;
}

static int script_statement_count(const struct mylite_sql_ast_node *root, size_t *out_count) {
    if (root == NULL || root->kind != MYLITE_SQL_AST_SCRIPT || out_count == NULL) {
        return MYLITE_ERROR;
    }

    *out_count = mylite_sql_ast_node_child_count(root);

    return MYLITE_OK;
}

static unsigned int parse_error_line_for_offset(const char *text, size_t offset) {
    unsigned int line = 1U;

    for (size_t index = 0U; text != NULL && index < offset; ++index) {
        if (text[index] == '\r' ||
            (text[index] == '\n' && (index == 0U || text[index - 1U] != '\r'))) {
            ++line;
        }
    }
    return line;
}

static unsigned int parse_error_line(const struct mylite_sql_token *token) {
    const char *text = NULL;

    if (token == NULL || token->text == NULL) {
        return 1U;
    }

    text = token->text - token->offset;
    return parse_error_line_for_offset(text, token->offset);
}

static unsigned int parse_error_span_line(const struct mylite_sql_source_span *span) {
    const char *text = NULL;

    if (span == NULL || span->text == NULL) {
        return 1U;
    }

    text = span->text - span->offset;
    return parse_error_line_for_offset(text, span->offset);
}

static void set_parse_error(
    struct mylite_db *database,
    const struct mylite_sql_parse_result *parse_result
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    const char *near_text = "end of input";
    int written = 0;

    if (parse_result != NULL && parse_result->error_token.text != NULL &&
        parse_result->error_token.length != 0U) {
        near_text = parse_result->error_token.text;
        written = snprintf(
            message,
            sizeof(message),
            "You have an error in your SQL syntax near '%.*s' at line %u",
            (int)parse_result->error_token.length,
            near_text,
            parse_error_line(&parse_result->error_token)
        );
    } else {
        written = snprintf(
            message,
            sizeof(message),
            "You have an error in your SQL syntax near '%s'",
            near_text
        );
    }
    if (written < 0) {
        message[0] = '\0';
    }

    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_parse,
        "42000",
        message
    );
}

static void set_multi_statement_parse_error(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *root
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    const struct mylite_sql_ast_node *second_statement = child_at(root, 1U);
    const struct mylite_sql_source_span *span =
        second_statement == NULL ? NULL : &second_statement->span;
    const char *near_text = "end of input";
    size_t near_length = strlen(near_text);
    unsigned int line = 1U;
    int written = 0;

    if (span != NULL && span->text != NULL && span->length != 0U) {
        near_text = span->text;
        near_length = span->length;
        line = parse_error_span_line(span);
    }
    if (near_length > (size_t)INT_MAX) {
        near_length = (size_t)INT_MAX;
    }

    written = snprintf(
        message,
        sizeof(message),
        "You have an error in your SQL syntax; check the manual that corresponds to your "
        "MySQL server version for the right syntax to use near '%.*s' at line %u",
        (int)near_length,
        near_text,
        line
    );
    if (written < 0) {
        message[0] = '\0';
    }

    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_parse,
        "42000",
        message
    );
}
