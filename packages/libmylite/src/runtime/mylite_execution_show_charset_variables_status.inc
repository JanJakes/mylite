
static int execute_show_character_set_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[show_character_set_result_column_count] = {
        "Charset",
        "Description",
        "Default collation",
        "Maxlen",
    };
    struct show_like_filter filter = {
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    rc = make_show_like_filter(database, child_at(statement, 0U), &filter);
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_character_set_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t row_index = 0U;
         rc == MYLITE_OK && row_index < mylite_execution_catalog_mysql_character_set_count();
         ++row_index) {
        const struct mylite_execution_catalog_character_set *charset =
            mylite_execution_catalog_mysql_character_set_at(row_index);
        const char *const values[show_character_set_result_column_count] = {
            charset->name,
            charset->description,
            charset->default_collation,
            charset->maxlen,
        };

        if (show_like_filter_matches(&filter, values[0], false)) {
            rc = mylite_result_append_text_row(result, values);
            if (rc != MYLITE_OK) {
                set_nomem_error(database);
            }
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        show_like_filter_deinit(&filter);
        return rc;
    }

    show_like_filter_deinit(&filter);
    return finish_successful_result(database, result, out_result);
}

static int execute_show_collation_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[show_collation_result_column_count] = {
        "Collation",
        "Charset",
        "Id",
        "Default",
        "Compiled",
        "Sortlen",
        "Pad_attribute",
    };
    struct show_like_filter filter = {
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    rc = make_show_like_filter(database, child_at(statement, 0U), &filter);
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < show_collation_result_column_count;
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t row_index = 0U;
         rc == MYLITE_OK && row_index < mylite_execution_catalog_mysql_collation_count();
         ++row_index) {
        const struct mylite_execution_catalog_collation *collation =
            mylite_execution_catalog_mysql_collation_at(row_index);
        const char *const values[show_collation_result_column_count] = {
            collation->name,
            collation->character_set,
            collation->id,
            collation->is_default,
            collation->compiled,
            collation->sortlen,
            collation->pad_attribute,
        };

        if (show_like_filter_matches(&filter, values[0], false)) {
            rc = mylite_result_append_text_row(result, values);
            if (rc != MYLITE_OK) {
                set_nomem_error(database);
            }
        }
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        show_like_filter_deinit(&filter);
        return rc;
    }

    show_like_filter_deinit(&filter);
    return finish_successful_result(database, result, out_result);
}

static int execute_show_variables_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[2] = {"Variable_name", "Value"};
    const struct mylite_sql_ast_node *first_child = child_at(statement, 0U);
    const struct mylite_sql_ast_node *scope = NULL;
    const struct mylite_sql_ast_node *like_pattern = NULL;
    const struct mylite_sql_ast_node *where_clause = NULL;
    const struct mylite_sql_ast_node *filter_node = NULL;
    bool global_scope = false;
    struct show_like_filter filter = {
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    if (first_child != NULL && first_child->kind == MYLITE_SQL_AST_IDENTIFIER) {
        scope = first_child;
        filter_node = child_at(statement, 1U);
    } else {
        filter_node = first_child;
    }
    if (filter_node != NULL && filter_node->kind == MYLITE_SQL_AST_WHERE_CLAUSE) {
        where_clause = filter_node;
    } else {
        like_pattern = filter_node;
    }
    global_scope = show_variables_scope_is_global(scope);

    rc = make_show_like_filter(database, like_pattern, &filter);
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < sizeof(result_columns) / sizeof(result_columns[0]);
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_system_variable_descriptor_count();
         ++index) {
        rc = append_show_variable(
            database,
            result,
            &filter,
            where_clause,
            global_scope,
            mylite_execution_system_variable_descriptor_at(index)
        );
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        show_like_filter_deinit(&filter);
        return rc;
    }

    show_like_filter_deinit(&filter);
    return finish_successful_result(database, result, out_result);
}

static int execute_show_status_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    mylite_result **out_result
) {
    static const char *const result_columns[2] = {"Variable_name", "Value"};
    const struct mylite_sql_ast_node *first_child = child_at(statement, 0U);
    const struct mylite_sql_ast_node *scope = NULL;
    const struct mylite_sql_ast_node *like_pattern = NULL;
    bool global_scope = false;
    struct show_like_filter filter = {
        .has_pattern = false,
        .pattern = NULL,
        .pattern_length = 0U,
    };
    mylite_result *result = NULL;
    int rc = MYLITE_OK;

    if (first_child != NULL && first_child->kind == MYLITE_SQL_AST_IDENTIFIER) {
        scope = first_child;
        like_pattern = child_at(statement, 1U);
    } else {
        like_pattern = first_child;
    }
    global_scope = show_variables_scope_is_global(scope);

    rc = make_show_like_filter(database, like_pattern, &filter);
    if (rc == MYLITE_OK) {
        rc = mylite_result_create(&result);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t column_index = 0U;
         rc == MYLITE_OK && column_index < sizeof(result_columns) / sizeof(result_columns[0]);
         ++column_index) {
        rc = mylite_result_append_column(result, result_columns[column_index]);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_show_status_descriptor_count();
         ++index) {
        rc = append_show_status(
            database,
            result,
            &filter,
            global_scope,
            mylite_execution_show_status_descriptor_at(index)
        );
    }
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        show_like_filter_deinit(&filter);
        return rc;
    }

    show_like_filter_deinit(&filter);
    return finish_successful_result(database, result, out_result);
}

static int append_show_status(
    struct mylite_db *database,
    mylite_result *result,
    const struct show_like_filter *filter,
    bool global_scope,
    const struct mylite_execution_show_status_descriptor *descriptor
) {
    const char *values[2] = {NULL, NULL};
    int rc = MYLITE_OK;

    if (!show_status_descriptor_visible(descriptor, global_scope)) {
        return MYLITE_OK;
    }
    if (!show_like_filter_matches(filter, descriptor->name, false)) {
        return MYLITE_OK;
    }

    values[0] = descriptor->name;
    values[1] = descriptor->value;
    rc = mylite_result_append_text_row(result, values);
    if (rc != MYLITE_OK) {
        set_nomem_error(database);
    }

    return rc;
}

static bool show_status_descriptor_visible(
    const struct mylite_execution_show_status_descriptor *descriptor,
    bool global_scope
) {
    if (descriptor == NULL) {
        return false;
    }
    if (global_scope) {
        return descriptor->show_global;
    }
    return descriptor->show_session;
}

static bool show_variables_scope_is_global(const struct mylite_sql_ast_node *scope) {
    static const char expected[] = "global";

    if (scope == NULL) {
        return false;
    }
    if (scope->span.text == NULL || scope->span.length != sizeof(expected) - 1U) {
        return false;
    }
    for (size_t index = 0U; index < sizeof(expected) - 1U; ++index) {
        if (ascii_lower((unsigned char)scope->span.text[index]) != expected[index]) {
            return false;
        }
    }
    return true;
}

static int append_show_variable(
    struct mylite_db *database,
    mylite_result *result,
    const struct show_like_filter *filter,
    const struct mylite_sql_ast_node *where_clause,
    bool global_scope,
    const struct mylite_execution_system_variable_descriptor *descriptor
) {
    char integer_buffer[integer_text_capacity];
    const char *values[2] = {NULL, NULL};
    struct show_variables_where_row row = {0};
    bool where_matches = true;
    int rc = MYLITE_OK;

    if (!show_variable_descriptor_visible(descriptor, global_scope)) {
        return MYLITE_OK;
    }
    if (!show_like_filter_matches(filter, descriptor->name, false)) {
        return MYLITE_OK;
    }

    values[0] = descriptor->name;
    rc = show_system_variable_value(
        database,
        descriptor->kind,
        global_scope,
        integer_buffer,
        sizeof(integer_buffer),
        &values[1]
    );
    if (rc == MYLITE_OK && where_clause != NULL) {
        row.variable_name = values[0];
        row.value = values[1];
        rc = show_variables_where_clause_matches(database, where_clause, &row, &where_matches);
    }
    if (rc == MYLITE_OK && !where_matches) {
        return MYLITE_OK;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_text_row(result, values);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }

    return rc;
}

static bool show_variable_descriptor_visible(
    const struct mylite_execution_system_variable_descriptor *descriptor,
    bool global_scope
) {
    if (descriptor == NULL) {
        return false;
    }
    if (global_scope) {
        return descriptor->show_global;
    }
    return descriptor->show_session;
}
