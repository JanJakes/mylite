struct json_table_row_values {
    const struct json_value **items;
    size_t count;
};

static int json_table_decode_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    const char *unsupported_message,
    char **out_text,
    size_t *out_text_length
);
static int json_table_parse_document(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    struct json_value *out_document
);
static int json_table_collect_row_values(
    struct mylite_db *database,
    const struct json_value *document,
    const char *row_path,
    size_t row_path_length,
    struct json_table_row_values *out_rows
);
static int json_table_validate_row_path_syntax(
    struct mylite_db *database,
    const char *row_path,
    size_t row_path_length
);
static int json_table_plan_columns(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_list,
    const struct json_table_row_values *rows,
    struct planned_select_source *out_source
);
static int json_table_plan_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct json_table_row_values *rows,
    struct planned_select_source *out_source,
    size_t column_index
);
static int json_table_make_ordinality_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    size_t column_index,
    struct mylite_catalog_column_descriptor *out_column
);
static int json_table_make_typed_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    size_t column_index,
    struct mylite_catalog_column_descriptor *out_column
);
static int json_table_copy_column_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    char *destination,
    size_t destination_size
);
static int json_table_fill_ordinality_values(
    struct planned_select_source *source,
    size_t column_index
);
static int json_table_fill_exists_values(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct json_table_row_values *rows,
    struct planned_select_source *source,
    size_t column_index
);
static int json_table_fill_path_values(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct json_table_row_values *rows,
    struct planned_select_source *source,
    size_t column_index
);
static int json_table_extract_path(
    struct mylite_db *database,
    const struct json_value *row,
    const char *path,
    size_t path_length,
    const struct json_value **out_value,
    bool *out_matched
);
static int json_table_assign_json_value(
    struct mylite_db *database,
    const struct json_value *value,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
);
static int json_table_assign_integer_json_value(
    const struct json_value *value,
    struct planned_value *out_value
);
static int json_table_assign_text_json_value(
    struct mylite_db *database,
    const struct json_value *value,
    struct planned_value *out_value
);
static int json_table_assign_emitted_json_value(
    struct mylite_db *database,
    const struct json_value *value,
    struct planned_value *out_value
);
static bool json_table_parse_json_integer(const char *text, size_t text_length, int64_t *out_value);
static bool json_table_column_is_integer(const struct mylite_catalog_column_descriptor *column);
static int json_table_set_alias_error(struct mylite_db *database);
static void json_table_row_values_deinit(struct json_table_row_values *rows);

static int plan_json_table_select_source(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *source_node,
    struct planned_select_source *out_source
) {
    const struct mylite_sql_ast_node *document_node = child_at(source_node, 0U);
    const struct mylite_sql_ast_node *row_path_node = child_at(source_node, 1U);
    const struct mylite_sql_ast_node *column_list = child_at(source_node, 2U);
    const struct mylite_sql_ast_node *alias = child_at(source_node, 3U);
    struct json_value document = {0};
    struct json_table_row_values rows = {0};
    char *document_text = NULL;
    char *row_path = NULL;
    size_t document_text_length = 0U;
    size_t row_path_length = 0U;
    int rc = MYLITE_OK;

    if (source_node == NULL || source_node->kind != MYLITE_SQL_AST_FROM_JSON_TABLE ||
        out_source == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (alias == NULL || alias->kind != MYLITE_SQL_AST_IDENTIFIER) {
        return json_table_set_alias_error(database);
    }

    out_source->is_json_table = true;
    out_source->has_alias = true;
    rc = copy_identifier_text(alias, out_source->alias, sizeof(out_source->alias), database);
    if (rc == MYLITE_OK) {
        snprintf(
            out_source->source.table_name,
            sizeof(out_source->source.table_name),
            "%s",
            out_source->alias
        );
        snprintf(out_source->table.name, sizeof(out_source->table.name), "%s", out_source->alias);
        if (database->session.has_selected_schema) {
            snprintf(
                out_source->source.schema.name,
                sizeof(out_source->source.schema.name),
                "%s",
                database->session.selected_schema
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = json_table_decode_string_literal(
            database,
            document_node,
            "JSON_TABLE() supports only string literal JSON documents",
            &document_text,
            &document_text_length
        );
    }
    if (rc == MYLITE_OK) {
        rc = json_table_decode_string_literal(
            database,
            row_path_node,
            "JSON_TABLE() supports only string literal row paths",
            &row_path,
            &row_path_length
        );
    }
    if (rc == MYLITE_OK) {
        rc = json_table_parse_document(database, document_text, document_text_length, &document);
    }
    if (rc == MYLITE_OK) {
        rc = json_table_collect_row_values(database, &document, row_path, row_path_length, &rows);
    }
    if (rc == MYLITE_OK) {
        rc = json_table_plan_columns(database, column_list, &rows, out_source);
    }

    free(document_text);
    free(row_path);
    json_table_row_values_deinit(&rows);
    mylite_json_internal_value_deinit(&document);
    if (rc != MYLITE_OK) {
        planned_select_source_deinit(out_source);
    }
    return rc;
}

static int append_json_table_source_sql(
    struct mylite_dynamic_string *string,
    const struct planned_select_source *source,
    size_t source_index,
    size_t *next_parameter
) {
    int rc = MYLITE_OK;

    if (source == NULL || !source->is_json_table || source->column_count == 0U ||
        next_parameter == NULL) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append_char(string, '(');
    if (rc == MYLITE_OK && source->json_table.row_count == 0U) {
        rc = mylite_dynamic_string_append(string, "SELECT ");
        for (size_t column_index = 0U; rc == MYLITE_OK && column_index < source->column_count;
             ++column_index) {
            if (column_index != 0U) {
                rc = mylite_dynamic_string_append(string, ", ");
            }
            if (rc == MYLITE_OK) {
                rc = mylite_dynamic_string_append(string, "NULL AS ");
            }
            if (rc == MYLITE_OK) {
                rc = mylite_dynamic_string_append_quoted_identifier(
                    string,
                    source->columns[column_index].name
                );
            }
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " WHERE 0");
        }
    }
    for (size_t row_index = 0U; rc == MYLITE_OK && row_index < source->json_table.row_count;
         ++row_index) {
        if (row_index != 0U) {
            rc = mylite_dynamic_string_append(string, " UNION ALL ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, "SELECT ");
        }
        for (size_t column_index = 0U; rc == MYLITE_OK && column_index < source->column_count;
             ++column_index) {
            if (column_index != 0U) {
                rc = mylite_dynamic_string_append(string, ", ");
            }
            if (rc == MYLITE_OK) {
                rc = append_numbered_parameter(string, *next_parameter);
            }
            if (rc == MYLITE_OK) {
                ++(*next_parameter);
                rc = mylite_dynamic_string_append(string, " AS ");
            }
            if (rc == MYLITE_OK) {
                rc = mylite_dynamic_string_append_quoted_identifier(
                    string,
                    source->columns[column_index].name
                );
            }
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ") AS ");
    }
    if (rc == MYLITE_OK) {
        rc = append_select_source_alias(string, source_index);
    }
    return rc;
}

static void planned_json_table_source_deinit(struct planned_json_table_source *source) {
    if (source == NULL) {
        return;
    }
    for (size_t value_index = 0U; value_index < source->row_count * source->column_count;
         ++value_index) {
        planned_value_deinit(&source->values[value_index]);
    }
    free(source->values);
    *source = (struct planned_json_table_source){0};
}

static int json_table_decode_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *literal,
    const char *unsupported_message,
    char **out_text,
    size_t *out_text_length
) {
    return decode_sql_string_literal(
        database,
        literal,
        unsupported_message,
        "JSON_TABLE() string literals may not contain NUL bytes",
        out_text,
        out_text_length
    );
}

static int json_table_parse_document(
    struct mylite_db *database,
    const char *text,
    size_t text_length,
    struct json_value *out_document
) {
    struct json_parser parser = {
        .text = text,
        .length = text_length,
        .position = 0U,
        .result = {0},
    };
    int rc = MYLITE_OK;

    if (out_document == NULL) {
        return MYLITE_MISUSE;
    }
    *out_document = (struct json_value){0};
    rc = mylite_json_internal_parse_document(&parser, out_document);
    if (rc != MYLITE_OK) {
        set_invalid_json_function_text_error(database, parser.result.position);
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int json_table_collect_row_values(
    struct mylite_db *database,
    const struct json_value *document,
    const char *row_path,
    size_t row_path_length,
    struct json_table_row_values *out_rows
) {
    if (out_rows == NULL) {
        return MYLITE_MISUSE;
    }
    *out_rows = (struct json_table_row_values){0};
    if (row_path == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (row_path_length == 1U && row_path[0] == '$') {
        out_rows->items = (const struct json_value **)calloc(1U, sizeof(*out_rows->items));
        if (out_rows->items == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        out_rows->items[0] = document;
        out_rows->count = 1U;
        return MYLITE_OK;
    }
    if (row_path_length == 4U && memcmp(row_path, "$[*]", row_path_length) == 0) {
        if (document == NULL || document->kind != JSON_VALUE_ARRAY) {
            return MYLITE_OK;
        }
        if (document->payload.array.count > SIZE_MAX / sizeof(*out_rows->items)) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        out_rows->items = (const struct json_value **)
            calloc(document->payload.array.count, sizeof(*out_rows->items));
        if (document->payload.array.count != 0U && out_rows->items == NULL) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        for (size_t index = 0U; index < document->payload.array.count; ++index) {
            out_rows->items[index] = &document->payload.array.values[index];
        }
        out_rows->count = document->payload.array.count;
        return MYLITE_OK;
    }

    if (json_table_validate_row_path_syntax(database, row_path, row_path_length) != MYLITE_OK) {
        return MYLITE_ERROR;
    }
    set_unsupported_error(database, "JSON_TABLE() supports only '$' and '$[*]' row paths");
    return MYLITE_ERROR;
}

static int json_table_validate_row_path_syntax(
    struct mylite_db *database,
    const char *row_path,
    size_t row_path_length
) {
    struct json_parser parser = {
        .text = row_path,
        .length = row_path_length,
        .position = 0U,
        .result = {.status = MYLITE_JSON_NORMALIZE_OK, .position = 0U},
    };
    const struct json_value *matched_value = NULL;
    bool matched = false;
    int rc = mylite_json_internal_extract_path_value(&parser, NULL, &matched_value, &matched);

    (void)matched_value;
    (void)matched;
    if (rc == MYLITE_OK || parser.result.status == MYLITE_JSON_NORMALIZE_UNSUPPORTED) {
        return MYLITE_OK;
    }
    set_invalid_json_path_error(database, parser.result.position);
    return MYLITE_ERROR;
}

static int json_table_plan_columns(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_list,
    const struct json_table_row_values *rows,
    struct planned_select_source *out_source
) {
    const struct mylite_sql_ast_node *column_node = NULL;
    size_t column_count = 0U;
    int rc = MYLITE_OK;

    if (column_list == NULL || column_list->kind != MYLITE_SQL_AST_JSON_TABLE_COLUMN_LIST ||
        rows == NULL || out_source == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    column_count = mylite_sql_ast_node_child_count(column_list);
    if (column_count == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (rows->count > SIZE_MAX / column_count) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_source->columns = calloc(column_count, sizeof(*out_source->columns));
    out_source->json_table.values =
        calloc(rows->count * column_count, sizeof(*out_source->json_table.values));
    if (out_source->columns == NULL ||
        (rows->count != 0U && out_source->json_table.values == NULL)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    out_source->column_count = column_count;
    out_source->json_table.row_count = rows->count;
    out_source->json_table.column_count = column_count;

    column_node = child_at(column_list, 0U);
    for (size_t column_index = 0U; rc == MYLITE_OK && column_node != NULL; ++column_index) {
        if (column_index >= column_count) {
            set_parse_error(database, NULL);
            rc = MYLITE_ERROR;
            break;
        }
        rc = json_table_plan_column(database, column_node, rows, out_source, column_index);
        column_node = column_node->next_sibling;
    }
    return rc;
}

static int json_table_plan_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct json_table_row_values *rows,
    struct planned_select_source *out_source,
    size_t column_index
) {
    int rc = MYLITE_OK;

    if (column_node == NULL || rows == NULL || out_source == NULL ||
        column_index >= out_source->column_count) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (column_node->kind == MYLITE_SQL_AST_JSON_TABLE_ORDINALITY_COLUMN) {
        rc = json_table_make_ordinality_column(
            database,
            column_node,
            column_index,
            &out_source->columns[column_index]
        );
        if (rc == MYLITE_OK) {
            rc = json_table_fill_ordinality_values(out_source, column_index);
        }
        return rc;
    }
    if (column_node->kind == MYLITE_SQL_AST_JSON_TABLE_EXISTS_COLUMN) {
        rc = json_table_make_typed_column(
            database,
            column_node,
            column_index,
            &out_source->columns[column_index]
        );
        if (rc == MYLITE_OK) {
            rc = json_table_fill_exists_values(
                database,
                column_node,
                rows,
                out_source,
                column_index
            );
        }
        return rc;
    }
    if (column_node->kind == MYLITE_SQL_AST_JSON_TABLE_PATH_COLUMN) {
        rc = json_table_make_typed_column(
            database,
            column_node,
            column_index,
            &out_source->columns[column_index]
        );
        if (rc == MYLITE_OK) {
            rc = json_table_fill_path_values(database, column_node, rows, out_source, column_index);
        }
        return rc;
    }

    set_parse_error(database, NULL);
    return MYLITE_ERROR;
}

static int json_table_make_ordinality_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    size_t column_index,
    struct mylite_catalog_column_descriptor *out_column
) {
    if (out_column == NULL) {
        return MYLITE_MISUSE;
    }
    *out_column = (struct mylite_catalog_column_descriptor){
        .column_id = (int64_t)column_index + 1,
        .ordinal_position = (int64_t)column_index + 1,
        .is_nullable = true,
        .is_visible = true,
    };
    snprintf(out_column->logical_type, sizeof(out_column->logical_type), "BIGINT");
    snprintf(out_column->physical_type, sizeof(out_column->physical_type), "INTEGER");
    return json_table_copy_column_name(
        database,
        column_node,
        out_column->name,
        sizeof(out_column->name)
    );
}

static int json_table_make_typed_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    size_t column_index,
    struct mylite_catalog_column_descriptor *out_column
) {
    const struct mylite_sql_ast_node *type_node = child_at(column_node, 1U);
    struct planned_column planned = {0};
    int rc = MYLITE_OK;

    if (out_column == NULL) {
        return MYLITE_MISUSE;
    }
    rc = json_table_copy_column_name(database, column_node, planned.name, sizeof(planned.name));
    if (rc == MYLITE_OK) {
        rc = map_column_type(database, type_node, planned.name, &planned);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }

    *out_column = (struct mylite_catalog_column_descriptor){
        .column_id = (int64_t)column_index + 1,
        .ordinal_position = (int64_t)column_index + 1,
        .is_nullable = true,
        .is_visible = true,
    };
    snprintf(out_column->name, sizeof(out_column->name), "%s", planned.name);
    snprintf(
        out_column->logical_type,
        sizeof(out_column->logical_type),
        "%s",
        planned.logical_type
    );
    snprintf(
        out_column->physical_type,
        sizeof(out_column->physical_type),
        "%s",
        planned.physical_type
    );
    snprintf(
        out_column->character_set_name,
        sizeof(out_column->character_set_name),
        "%s",
        planned.character_set_name
    );
    snprintf(
        out_column->collation_name,
        sizeof(out_column->collation_name),
        "%s",
        planned.collation_name
    );
    return MYLITE_OK;
}

static int json_table_copy_column_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    char *destination,
    size_t destination_size
) {
    const struct mylite_sql_ast_node *name_node = child_at(column_node, 0U);

    if (name_node == NULL || name_node->kind != MYLITE_SQL_AST_IDENTIFIER) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    return copy_identifier_text(name_node, destination, destination_size, database);
}

static int json_table_fill_ordinality_values(
    struct planned_select_source *source,
    size_t column_index
) {
    for (size_t row_index = 0U; row_index < source->json_table.row_count; ++row_index) {
        struct planned_value *value =
            &source->json_table.values[(row_index * source->column_count) + column_index];

        *value = (struct planned_value){.is_null = false, .integer = (int64_t)row_index + 1};
    }
    return MYLITE_OK;
}

static int json_table_fill_exists_values(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct json_table_row_values *rows,
    struct planned_select_source *source,
    size_t column_index
) {
    const struct mylite_sql_ast_node *path_node = child_at(column_node, 2U);
    char *path = NULL;
    size_t path_length = 0U;
    int rc = json_table_decode_string_literal(
        database,
        path_node,
        "JSON_TABLE() EXISTS columns support only string literal paths",
        &path,
        &path_length
    );

    for (size_t row_index = 0U; rc == MYLITE_OK && row_index < rows->count; ++row_index) {
        const struct json_value *matched_value = NULL;
        bool matched = false;

        rc = json_table_extract_path(
            database,
            rows->items[row_index],
            path,
            path_length,
            &matched_value,
            &matched
        );
        if (rc == MYLITE_OK) {
            struct planned_value *value =
                &source->json_table.values[(row_index * source->column_count) + column_index];

            (void)matched_value;
            *value = (struct planned_value){.is_null = false, .integer = matched ? 1 : 0};
        }
    }
    free(path);
    return rc;
}

static int json_table_fill_path_values(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct json_table_row_values *rows,
    struct planned_select_source *source,
    size_t column_index
) {
    const struct mylite_sql_ast_node *path_node = child_at(column_node, 2U);
    const struct mylite_catalog_column_descriptor *column = &source->columns[column_index];
    char *path = NULL;
    size_t path_length = 0U;
    int rc = json_table_decode_string_literal(
        database,
        path_node,
        "JSON_TABLE() PATH columns support only string literal paths",
        &path,
        &path_length
    );

    for (size_t row_index = 0U; rc == MYLITE_OK && row_index < rows->count; ++row_index) {
        const struct json_value *matched_value = NULL;
        bool matched = false;

        rc = json_table_extract_path(
            database,
            rows->items[row_index],
            path,
            path_length,
            &matched_value,
            &matched
        );
        if (rc == MYLITE_OK) {
            struct planned_value *value =
                &source->json_table.values[(row_index * source->column_count) + column_index];

            if (!matched) {
                *value = (struct planned_value){.is_null = true};
            } else {
                rc = json_table_assign_json_value(database, matched_value, column, value);
            }
        }
    }
    free(path);
    return rc;
}

static int json_table_extract_path(
    struct mylite_db *database,
    const struct json_value *row,
    const char *path,
    size_t path_length,
    const struct json_value **out_value,
    bool *out_matched
) {
    struct json_parser parser = {
        .text = path,
        .length = path_length,
        .position = 0U,
        .result = {0},
    };
    int rc = mylite_json_internal_extract_path_value(&parser, row, out_value, out_matched);

    if (rc != MYLITE_OK) {
        set_invalid_json_path_error(database, parser.result.position);
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int json_table_assign_json_value(
    struct mylite_db *database,
    const struct json_value *value,
    const struct mylite_catalog_column_descriptor *column,
    struct planned_value *out_value
) {
    if (value == NULL || value->kind == JSON_VALUE_NULL) {
        *out_value = (struct planned_value){.is_null = true};
        return MYLITE_OK;
    }
    if (json_table_column_is_integer(column)) {
        return json_table_assign_integer_json_value(value, out_value);
    }
    if (column_descriptor_is_string_family(column)) {
        return json_table_assign_text_json_value(database, value, out_value);
    }
    if (column_descriptor_is_json(column)) {
        return json_table_assign_emitted_json_value(database, value, out_value);
    }

    set_unsupported_error(
        database,
        "JSON_TABLE() supports only integer, string, and JSON output columns"
    );
    return MYLITE_ERROR;
}

static int json_table_assign_integer_json_value(
    const struct json_value *value,
    struct planned_value *out_value
) {
    int64_t integer = 0;

    if (value->kind == JSON_VALUE_BOOL) {
        *out_value =
            (struct planned_value){.is_null = false, .integer = value->payload.boolean ? 1 : 0};
        return MYLITE_OK;
    }
    if (value->kind != JSON_VALUE_NUMBER || value->number_kind != JSON_NUMBER_INTEGER ||
        !json_table_parse_json_integer(
            value->payload.text.text,
            value->payload.text.length,
            &integer
        )) {
        *out_value = (struct planned_value){.is_null = true};
        return MYLITE_OK;
    }

    *out_value = (struct planned_value){.is_null = false, .integer = integer};
    return MYLITE_OK;
}

static int json_table_assign_text_json_value(
    struct mylite_db *database,
    const struct json_value *value,
    struct planned_value *out_value
) {
    if (value->kind == JSON_VALUE_STRING) {
        char *text = NULL;
        size_t text_length = 0U;
        int rc = mylite_json_internal_copy_result_text(
            value->payload.text.text,
            value->payload.text.length,
            &text,
            &text_length
        );

        if (rc != MYLITE_OK) {
            set_nomem_error(database);
            return rc;
        }
        *out_value = (struct planned_value){
            .is_null = false,
            .is_text = true,
            .text = text,
            .text_length = text_length,
        };
        return MYLITE_OK;
    }
    return json_table_assign_emitted_json_value(database, value, out_value);
}

static int json_table_assign_emitted_json_value(
    struct mylite_db *database,
    const struct json_value *value,
    struct planned_value *out_value
) {
    struct json_writer writer = {0};
    int rc = mylite_json_internal_emit_value(&writer, value);

    if (rc == MYLITE_OK) {
        out_value->text_length = writer.length;
        out_value->text = mylite_json_internal_writer_take(&writer);
        if (out_value->text == NULL) {
            rc = MYLITE_NOMEM;
        } else {
            out_value->is_null = false;
            out_value->is_text = true;
        }
    }
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    mylite_json_internal_writer_deinit(&writer);
    return rc;
}

static bool json_table_parse_json_integer(
    const char *text,
    size_t text_length,
    int64_t *out_value
) {
    enum { json_table_decimal_base = 10 };

    const bool negative = text_length != 0U && text[0] == '-';
    const uint64_t limit = negative ? ((uint64_t)INT64_MAX + 1U) : (uint64_t)INT64_MAX;
    uint64_t magnitude = 0U;
    size_t offset = negative ? 1U : 0U;

    if (text == NULL || out_value == NULL || offset == text_length) {
        return false;
    }
    for (; offset < text_length; ++offset) {
        unsigned int digit = 0U;

        if (text[offset] < '0' || text[offset] > '9') {
            return false;
        }
        digit = (unsigned int)(text[offset] - '0');
        if (magnitude > (limit - digit) / (uint64_t)json_table_decimal_base) {
            return false;
        }
        magnitude = (magnitude * (uint64_t)json_table_decimal_base) + digit;
    }
    if (negative && magnitude == ((uint64_t)INT64_MAX + 1U)) {
        *out_value = INT64_MIN;
    } else if (negative) {
        *out_value = -(int64_t)magnitude;
    } else {
        *out_value = (int64_t)magnitude;
    }
    return true;
}

static bool json_table_column_is_integer(const struct mylite_catalog_column_descriptor *column) {
    enum mylite_result_logical_type type = MYLITE_RESULT_LOGICAL_TYPE_UNKNOWN;
    uint64_t display_length = 0U;
    bool is_unsigned = false;

    return result_metadata_integer_descriptor(
        column == NULL ? NULL : column->logical_type,
        &type,
        &display_length,
        &is_unsigned
    );
}

static int json_table_set_alias_error(struct mylite_db *database) {
    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_table_function_alias,
        "42000",
        "Every table function must have an alias"
    );
    return MYLITE_ERROR;
}

static void json_table_row_values_deinit(struct json_table_row_values *rows) {
    if (rows == NULL) {
        return;
    }
    free((void *)rows->items);
    *rows = (struct json_table_row_values){0};
}
