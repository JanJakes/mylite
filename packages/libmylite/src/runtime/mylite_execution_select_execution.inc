static int execute_select_from_plan(
    struct mylite_db *database,
    const struct planned_select *plan,
    mylite_result **out_result
) {
    sqlite3_stmt *statement = NULL;
    mylite_result *result = NULL;
    char *sql = NULL;
    struct result_column_metadata_context metadata_context = result_column_metadata_context_init();
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    rc = load_result_column_metadata_context(database, plan, &metadata_context);
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        rc = append_select_result_column(database, result, plan, &metadata_context, column_index);
    }
    if (rc == MYLITE_OK) {
        rc = build_select_sql(plan, &sql);
    }
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_select_parameters(statement, plan);
    }
    while (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            break;
        }
        if (sqlite_rc != SQLITE_ROW) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
            break;
        }
        rc = append_selected_sqlite_row(
            database,
            statement,
            result,
            plan->columns,
            plan->column_count
        );
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);
    result_column_metadata_context_deinit(&metadata_context);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
            return rc;
        }
        if (mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) != MYLITE_OK) {
            return rc;
        }
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }
    rc = set_select_found_row_count(database, plan, result);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }
    if (plan->calc_found_rows) {
        rc = append_sql_calc_found_rows_deprecation_warning(database);
        if (rc != MYLITE_OK) {
            mylite_result_free(result);
            return rc;
        }
    }

    return finish_successful_result(database, result, out_result);
}

static bool select_statement_is_row_scalar_projection_attempt(
    const struct mylite_sql_ast_node *statement
) {
    const struct mylite_sql_ast_node *select_list = child_at(statement, 0U);
    const struct mylite_sql_ast_node *from_clause = child_at(statement, 1U);
    const struct mylite_sql_ast_node *select_item = NULL;
    bool has_dual_source =
        (from_clause != NULL && from_clause->kind == MYLITE_SQL_AST_FROM_DUAL) != 0;
    bool has_table_source =
        (from_clause != NULL && from_clause->kind == MYLITE_SQL_AST_FROM_TABLE) != 0;

    if (select_list == NULL || select_list->kind != MYLITE_SQL_AST_SELECT_LIST ||
        select_list_is_wildcard(select_list)) {
        return false;
    }
    if (has_dual_source && child_at(statement, 2U) != NULL) {
        return true;
    }

    select_item = child_at(select_list, 0U);
    while (select_item != NULL) {
        if (select_item->kind == MYLITE_SQL_AST_SELECT_ITEM &&
            (row_scalar_expression_contains_row_function(child_at(select_item, 0U)) ||
             (has_table_source &&
              row_scalar_expression_contains_rand_function(child_at(select_item, 0U))) ||
             (has_table_source &&
              row_scalar_expression_contains_any_value_function(child_at(select_item, 0U))) ||
             (has_table_source &&
              row_scalar_expression_contains_conversion_expression(child_at(select_item, 0U))) ||
             (has_table_source &&
              row_scalar_expression_is_concat_operator(child_at(select_item, 0U))) ||
             (has_table_source && row_scalar_expression_contains_date_interval_second_function(
                                      child_at(select_item, 0U)
                                  )) ||
             row_scalar_expression_contains_statement_time_function(child_at(select_item, 0U)) ||
             (has_table_source &&
              row_scalar_expression_contains_control_flow_function(child_at(select_item, 0U))) ||
             (has_table_source && row_scalar_expression_contains_integer_arithmetic_attempt(
                                      child_at(select_item, 0U)
                                  )))) {
            return true;
        }
        select_item = select_item->next_sibling;
    }

    return false;
}

static int execute_row_scalar_select_statement(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    bool apply_sql_select_limit,
    mylite_result **out_result
) {
    struct planned_row_scalar_select plan = {false};
    int rc = plan_row_scalar_select(database, statement, true, &plan);

    if (rc == MYLITE_OK && apply_sql_select_limit) {
        apply_sql_select_limit_to_plan_limit(database, &plan.limit);
    }
    if (rc == MYLITE_OK) {
        rc = append_select_modifier_warnings(database, statement);
    }
    if (rc == MYLITE_OK) {
        rc = append_session_scalar_select_warnings(database, child_at(statement, 0U));
    }
    if (rc == MYLITE_OK) {
        rc = execute_row_scalar_select_from_plan(database, &plan, out_result);
    }
    planned_row_scalar_select_deinit(&plan);

    return rc;
}

static int execute_row_scalar_select_from_plan(
    struct mylite_db *database,
    const struct planned_row_scalar_select *plan,
    mylite_result **out_result
) {
    sqlite3_stmt *statement = NULL;
    struct result_column_metadata_context metadata_context = result_column_metadata_context_init();
    mylite_result *result = NULL;
    char *sql = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = mylite_result_create(&result);

    if (rc != MYLITE_OK) {
        set_nomem_error(database);
        return rc;
    }

    if (plan->has_source) {
        rc = load_result_column_metadata_context_for_table(
            database,
            plan->table.table_id,
            &metadata_context
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_row_scalar_result_columns(database, result, plan, &metadata_context);
    }
    if (rc == MYLITE_OK) {
        rc = build_row_scalar_select_sql(plan, &sql);
    }
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_row_scalar_select_parameters(statement, plan);
    }
    while (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_DONE) {
            break;
        }
        if (sqlite_rc != SQLITE_ROW) {
            rc = row_scalar_select_step_error(database, statement, sqlite_rc);
            break;
        }
        rc = append_selected_sqlite_row(database, statement, result, NULL, 0U);
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);
    result_column_metadata_context_deinit(&metadata_context);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(database);
            return rc;
        }
        if (mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) != MYLITE_OK) {
            return rc;
        }
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }
    rc = set_row_scalar_select_found_row_count(database, plan, result);
    if (rc != MYLITE_OK) {
        mylite_result_free(result);
        return rc;
    }

    return finish_successful_result(database, result, out_result);
}

static int row_scalar_select_step_error(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int sqlite_rc
) {
    const char *message = NULL;

    if (sqlite_rc != SQLITE_ERROR || statement == NULL) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    message = sqlite3_errmsg(sqlite3_db_handle(statement));
    if (message == NULL) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }
    if (map_json_extract_row_scalar_step_error(database, message) ||
        map_json_mutation_row_scalar_step_error(database, message) ||
        map_json_search_row_scalar_step_error(database, message) ||
        map_json_type_row_scalar_step_error(database, message) ||
        map_json_constructor_row_scalar_step_error(database, message)) {
        return MYLITE_ERROR;
    }
    if (strcmp(message, MYLITE_INTEGER_ARITHMETIC_OVERFLOW_MESSAGE) == 0) {
        set_scalar_arithmetic_overflow_error(database);
        return MYLITE_ERROR;
    }
    if (strcmp(message, "CONVERT USING latin1 supports only ASCII scalar values") == 0) {
        set_unsupported_error(database, message);
        return MYLITE_ERROR;
    }

    return mylite_sqlite_status_to_mylite(sqlite_rc);
}

static bool map_json_extract_row_scalar_step_error(
    struct mylite_db *database,
    const char *message
) {
    if (strcmp(message, "Invalid data type for JSON data in JSON_EXTRACT()") == 0) {
        set_invalid_json_data_type_error(database, "JSON_EXTRACT");
        return true;
    }
    if (strcmp(message, "Invalid JSON text or JSON path in JSON_EXTRACT()") == 0) {
        set_invalid_json_function_text_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Unsupported JSON path or JSON document in JSON_EXTRACT()") == 0) {
        set_unsupported_error(database, "JSON_EXTRACT() path or document shape is not supported");
        return true;
    }
    if (strcmp(message, "Invalid data type for JSON data in JSON_VALUE()") == 0) {
        set_invalid_json_data_type_error(database, "json_value");
        return true;
    }
    if (strcmp(message, "Invalid JSON text or JSON path in JSON_VALUE()") == 0) {
        set_invalid_json_function_text_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Unsupported JSON path or JSON document in JSON_VALUE()") == 0) {
        set_unsupported_error(database, "JSON_VALUE() path or document shape is not supported");
        return true;
    }
    if (strcmp(message, "Invalid data type for JSON data in JSON_LENGTH()") == 0) {
        set_invalid_json_data_type_error(database, "JSON_LENGTH");
        return true;
    }
    if (strcmp(message, "Invalid JSON path in JSON_LENGTH()") == 0) {
        set_invalid_json_path_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Invalid JSON text or JSON path in JSON_LENGTH()") == 0) {
        set_invalid_json_function_text_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Unsupported JSON path or JSON document in JSON_LENGTH()") == 0) {
        set_unsupported_error(database, "JSON_LENGTH() path or document shape is not supported");
        return true;
    }
    if (strcmp(message, "Invalid data type for JSON data in JSON_KEYS()") == 0) {
        set_invalid_json_data_type_error(database, "JSON_KEYS");
        return true;
    }
    if (strcmp(message, "Invalid JSON path in JSON_KEYS()") == 0) {
        set_invalid_json_path_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Invalid JSON text or JSON path in JSON_KEYS()") == 0) {
        set_invalid_json_function_text_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Unsupported JSON path or JSON document in JSON_KEYS()") == 0) {
        set_unsupported_error(database, "JSON_KEYS() path or document shape is not supported");
        return true;
    }
    return false;
}

static bool map_json_mutation_row_scalar_step_error(
    struct mylite_db *database,
    const char *message
) {
    if (strcmp(message, "Invalid data type for JSON data in JSON_SET()") == 0) {
        set_invalid_json_data_type_error(database, "JSON_SET");
        return true;
    }
    if (strcmp(message, "Invalid JSON path in JSON_SET()") == 0) {
        set_invalid_json_path_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Invalid JSON text or JSON path in JSON_SET()") == 0) {
        set_invalid_json_function_text_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Unsupported JSON path or JSON document in JSON_SET()") == 0) {
        set_unsupported_error(database, "JSON_SET() path or document shape is not supported");
        return true;
    }
    if (strcmp(message, "Invalid data type for JSON data in JSON_INSERT()") == 0) {
        set_invalid_json_data_type_error(database, "JSON_INSERT");
        return true;
    }
    if (strcmp(message, "Invalid JSON path in JSON_INSERT()") == 0) {
        set_invalid_json_path_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Invalid JSON text or JSON path in JSON_INSERT()") == 0) {
        set_invalid_json_function_text_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Unsupported JSON path or JSON document in JSON_INSERT()") == 0) {
        set_unsupported_error(database, "JSON_INSERT() path or document shape is not supported");
        return true;
    }
    if (strcmp(message, "Invalid data type for JSON data in JSON_REPLACE()") == 0) {
        set_invalid_json_data_type_error(database, "JSON_REPLACE");
        return true;
    }
    if (strcmp(message, "Invalid JSON path in JSON_REPLACE()") == 0) {
        set_invalid_json_path_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Invalid JSON text or JSON path in JSON_REPLACE()") == 0) {
        set_invalid_json_function_text_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Unsupported JSON path or JSON document in JSON_REPLACE()") == 0) {
        set_unsupported_error(database, "JSON_REPLACE() path or document shape is not supported");
        return true;
    }
    if (strcmp(message, "Invalid data type for JSON data in JSON_REMOVE()") == 0) {
        set_invalid_json_data_type_error(database, "JSON_REMOVE");
        return true;
    }
    if (strcmp(message, "Invalid JSON path in JSON_REMOVE()") == 0) {
        set_invalid_json_path_error(database, 0U);
        return true;
    }
    if (strcmp(message, "JSON_REMOVE path not allowed") == 0) {
        set_json_path_not_allowed_error(database);
        return true;
    }
    if (strcmp(message, "Invalid JSON text or JSON path in JSON_REMOVE()") == 0) {
        set_invalid_json_function_text_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Unsupported JSON path or JSON document in JSON_REMOVE()") == 0) {
        set_unsupported_error(database, "JSON_REMOVE() path or document shape is not supported");
        return true;
    }
    return false;
}

static bool map_json_search_row_scalar_step_error(struct mylite_db *database, const char *message) {
    if (strcmp(message, "Invalid data type for JSON data in JSON_CONTAINS()") == 0) {
        set_invalid_json_data_type_error(database, "JSON_CONTAINS");
        return true;
    }
    if (strcmp(message, "Invalid JSON path in JSON_CONTAINS()") == 0) {
        set_invalid_json_path_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Invalid JSON text or JSON path in JSON_CONTAINS()") == 0) {
        set_invalid_json_function_text_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Unsupported JSON path or JSON document in JSON_CONTAINS()") == 0) {
        set_unsupported_error(database, "JSON_CONTAINS() path or document shape is not supported");
        return true;
    }
    if (strcmp(message, "Invalid data type for JSON data in JSON_CONTAINS_PATH()") == 0) {
        set_invalid_json_data_type_error(database, "JSON_CONTAINS_PATH");
        return true;
    }
    if (strcmp(message, "Invalid JSON path in JSON_CONTAINS_PATH()") == 0) {
        set_invalid_json_path_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Invalid oneOrAll in JSON_CONTAINS_PATH()") == 0) {
        set_invalid_json_one_or_all_error(database);
        return true;
    }
    if (strcmp(message, "Invalid JSON text or JSON path in JSON_CONTAINS_PATH()") == 0) {
        set_invalid_json_function_text_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Unsupported JSON path or JSON document in JSON_CONTAINS_PATH()") == 0) {
        set_unsupported_error(
            database,
            "JSON_CONTAINS_PATH() path or document shape is not supported"
        );
        return true;
    }
    return false;
}

static bool map_json_type_row_scalar_step_error(struct mylite_db *database, const char *message) {
    if (strcmp(message, "Invalid data type for JSON data in JSON_TYPE()") == 0) {
        set_invalid_json_data_type_error(database, "JSON_TYPE");
        return true;
    }
    if (strcmp(message, "Invalid JSON text in JSON_TYPE()") == 0) {
        set_invalid_json_function_text_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Unsupported JSON document in JSON_TYPE()") == 0) {
        set_unsupported_error(database, "JSON_TYPE() document shape is not supported");
        return true;
    }
    if (strcmp(message, "Incorrect type for argument to JSON_UNQUOTE()") == 0) {
        set_json_unquote_incorrect_type_error(database);
        return true;
    }
    if (strcmp(message, "Incorrect type for argument to JSON_QUOTE()") == 0) {
        set_json_quote_incorrect_type_error(database);
        return true;
    }
    if (strcmp(message, "Invalid JSON text in JSON_UNQUOTE()") == 0) {
        set_invalid_json_function_text_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Unsupported JSON string in JSON_UNQUOTE()") == 0) {
        set_unsupported_error(database, "JSON_UNQUOTE() JSON string shape is not supported");
        return true;
    }
    return false;
}

static bool map_json_constructor_row_scalar_step_error(
    struct mylite_db *database,
    const char *message
) {
    if (strcmp(message, "JSON documents may not contain NULL member names.") == 0) {
        set_json_null_member_name_error(database);
        return true;
    }
    if (strcmp(message, "Invalid JSON text in JSON constructor") == 0) {
        set_invalid_json_function_text_error(database, 0U);
        return true;
    }
    if (strcmp(message, "Unsupported JSON value in JSON constructor") == 0) {
        set_unsupported_error(database, "JSON constructors support only baseline JSON values");
        return true;
    }
    return false;
}

static int append_row_scalar_result_columns(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_row_scalar_select *plan,
    const struct result_column_metadata_context *metadata_context
) {
    int rc = MYLITE_OK;

    for (size_t item_index = 0U; rc == MYLITE_OK && item_index < plan->item_count; ++item_index) {
        rc = append_row_scalar_result_column(
            database,
            result,
            plan,
            metadata_context,
            &plan->items[item_index]
        );
    }

    return rc;
}

static int append_row_scalar_result_column(
    struct mylite_db *database,
    mylite_result *result,
    const struct planned_row_scalar_select *plan,
    const struct result_column_metadata_context *metadata_context,
    const struct planned_row_scalar_select_item *item
) {
    char *column_name = NULL;
    struct mylite_result_column_descriptor descriptor;
    int rc = copy_row_scalar_result_column_name(database, item, &column_name);

    if (rc == MYLITE_OK) {
        rc = make_row_scalar_result_column_descriptor(
            database,
            plan,
            metadata_context,
            item,
            column_name,
            &descriptor
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_result_append_column_descriptor(result, &descriptor);
        if (rc != MYLITE_OK) {
            set_nomem_error(database);
        }
    }
    free(column_name);

    return rc;
}

static int make_row_scalar_result_column_descriptor(
    struct mylite_db *database,
    const struct planned_row_scalar_select *plan,
    const struct result_column_metadata_context *metadata_context,
    const struct planned_row_scalar_select_item *item,
    const char *label,
    struct mylite_result_column_descriptor *out_descriptor
) {
    if (out_descriptor == NULL) {
        return MYLITE_MISUSE;
    }
    *out_descriptor = unknown_result_column_descriptor(label);
    if (item == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (item->expression.kind == PLANNED_ROW_SCALAR_EXPRESSION_COLUMN) {
        return make_row_scalar_column_result_column_descriptor(
            database,
            plan,
            metadata_context,
            item,
            label,
            out_descriptor
        );
    }

    return populate_row_scalar_expression_result_column_descriptor(
        database,
        &item->expression,
        out_descriptor
    );
}

static int make_row_scalar_column_result_column_descriptor(
    struct mylite_db *database,
    const struct planned_row_scalar_select *plan,
    const struct result_column_metadata_context *metadata_context,
    const struct planned_row_scalar_select_item *item,
    const char *label,
    struct mylite_result_column_descriptor *out_descriptor
) {
    const char *table_name = NULL;

    if (plan == NULL || item == NULL) {
        return MYLITE_MISUSE;
    }
    *out_descriptor = unknown_result_column_descriptor(label);
    if (metadata_context == NULL || !metadata_context->has_single_source_metadata) {
        return MYLITE_OK;
    }

    table_name = plan->source.table_name;
    if (plan->source_has_alias) {
        table_name = plan->source_alias;
    }
    out_descriptor->schema_name = plan->source.schema.name;
    out_descriptor->table_name = table_name;
    out_descriptor->origin_schema_name = plan->source.schema.name;
    out_descriptor->origin_table_name = plan->table.name;
    out_descriptor->origin_column_name = item->expression.column.name;
    out_descriptor->nullable = item->expression.column.is_nullable;

    return populate_select_result_column_descriptor(
        database,
        &plan->table,
        &item->expression.column,
        metadata_context,
        out_descriptor
    );
}

static int populate_row_scalar_expression_result_column_descriptor(
    struct mylite_db *database,
    const struct planned_row_scalar_expression *expression,
    struct mylite_result_column_descriptor *descriptor
) {
    if (expression == NULL || descriptor == NULL) {
        return MYLITE_MISUSE;
    }

    switch (expression->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
        return populate_window_function_result_column_descriptor(database, expression, descriptor);
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF:
        populate_scalar_binary_numeric_result_column_descriptor(
            descriptor,
            (struct scalar_binary_numeric_result_column_shape){
                .logical_type = MYLITE_RESULT_LOGICAL_TYPE_LONGLONG,
                .display_length = mysql_scalar_bigint_display_length,
                .decimals = 0U,
                .extra_flags = 0U,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE:
        populate_scalar_connection_string_result_column_descriptor(
            database,
            descriptor,
            (struct scalar_connection_string_result_column_shape){
                .display_length = mysql_regexp_string_function_display_length *
                                  scalar_connection_max_bytes_per_character(database),
                .extra_flags = 0U,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
        populate_scalar_binary_numeric_result_column_descriptor(
            descriptor,
            (struct scalar_binary_numeric_result_column_shape){
                .logical_type = MYLITE_RESULT_LOGICAL_TYPE_DOUBLE,
                .display_length = mysql_scalar_double_display_length,
                .decimals = mysql_approximate_decimals,
                .extra_flags = 0U,
                .nullable = false,
            }
        );
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
        populate_uuid_string_result_column_descriptor(descriptor, true);
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
        descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING;
        descriptor->charset_id = mysql_collation_binary_id;
        descriptor->collation_id = mysql_collation_binary_id;
        descriptor->display_length = MYLITE_UUID_BINARY_SIZE;
        descriptor->decimals = mysql_approximate_decimals;
        descriptor->flags = MYLITE_RESULT_COLUMN_FLAG_BINARY;
        descriptor->nullable = true;
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
        populate_scalar_connection_string_result_column_descriptor(
            database,
            descriptor,
            (struct scalar_connection_string_result_column_shape){
                .display_length = MYLITE_UUID_TEXT_SIZE,
                .extra_flags = 0U,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
        populate_scalar_connection_string_result_column_descriptor(
            database,
            descriptor,
            (struct scalar_connection_string_result_column_shape){
                .display_length = mysql_json_type_display_length,
                .extra_flags = MYLITE_RESULT_COLUMN_FLAG_BINARY,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
        populate_scalar_connection_string_result_column_descriptor(
            database,
            descriptor,
            (struct scalar_connection_string_result_column_shape){
                .display_length = mysql_json_document_display_length,
                .extra_flags = MYLITE_RESULT_COLUMN_FLAG_BINARY,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
        populate_scalar_connection_string_result_column_descriptor(
            database,
            descriptor,
            (struct scalar_connection_string_result_column_shape){
                .display_length = mysql_json_value_display_character_length *
                                  scalar_connection_max_bytes_per_character(database),
                .extra_flags = MYLITE_RESULT_COLUMN_FLAG_BINARY,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
        populate_scalar_json_result_column_descriptor(database, descriptor);
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ:
        populate_scalar_temporal_string_result_column_descriptor(database, descriptor);
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
        descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING;
        descriptor->charset_id = mysql_collation_binary_id;
        descriptor->collation_id = mysql_collation_binary_id;
        descriptor->display_length = mysql_scalar_bigint_display_length;
        descriptor->decimals = mysql_approximate_decimals;
        descriptor->flags = MYLITE_RESULT_COLUMN_FLAG_BINARY;
        descriptor->nullable = true;
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
        return populate_row_scalar_date_interval_second_result_column_descriptor(
            database,
            expression,
            descriptor
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
        return populate_row_scalar_conversion_result_column_descriptor(
            database,
            expression,
            descriptor
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT:
        if (mylite_temporal_extract_kind_is_calendar_name(expression->temporal_extract_kind)) {
            populate_calendar_name_result_column_descriptor(database, descriptor);
        }
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_EQUAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIND_IN_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_INSERT:
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_LIKE:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBSTRING_INDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_PADDING:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_BITMASK:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REVERSE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SOUNDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRCMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
        return MYLITE_OK;
    }

    return MYLITE_OK;
}

static int populate_window_function_result_column_descriptor(
    struct mylite_db *database,
    const struct planned_row_scalar_expression *expression,
    struct mylite_result_column_descriptor *descriptor
) {
    if (expression == NULL || descriptor == NULL) {
        return MYLITE_MISUSE;
    }

    switch (expression->window_function_kind) {
    case PLANNED_WINDOW_FUNCTION_ROW_NUMBER:
    case PLANNED_WINDOW_FUNCTION_RANK:
    case PLANNED_WINDOW_FUNCTION_DENSE_RANK:
    case PLANNED_WINDOW_FUNCTION_NTILE:
        populate_scalar_binary_numeric_result_column_descriptor(
            descriptor,
            (struct scalar_binary_numeric_result_column_shape){
                .logical_type = MYLITE_RESULT_LOGICAL_TYPE_LONGLONG,
                .display_length = mysql_scalar_bigint_display_length,
                .decimals = 0U,
                .extra_flags = MYLITE_RESULT_COLUMN_FLAG_UNSIGNED,
                .nullable = false,
            }
        );
        return MYLITE_OK;
    case PLANNED_WINDOW_FUNCTION_PERCENT_RANK:
    case PLANNED_WINDOW_FUNCTION_CUME_DIST:
        populate_scalar_binary_numeric_result_column_descriptor(
            descriptor,
            (struct scalar_binary_numeric_result_column_shape){
                .logical_type = MYLITE_RESULT_LOGICAL_TYPE_DOUBLE,
                .display_length = mysql_scalar_double_display_length,
                .decimals = mysql_approximate_decimals,
                .extra_flags = 0U,
                .nullable = false,
            }
        );
        return MYLITE_OK;
    case PLANNED_WINDOW_FUNCTION_LAG:
    case PLANNED_WINDOW_FUNCTION_LEAD:
    case PLANNED_WINDOW_FUNCTION_FIRST_VALUE:
    case PLANNED_WINDOW_FUNCTION_LAST_VALUE:
    case PLANNED_WINDOW_FUNCTION_NTH_VALUE:
        return populate_window_value_function_result_column_descriptor(
            database,
            expression,
            descriptor
        );
    case PLANNED_WINDOW_FUNCTION_NONE:
        break;
    }

    set_parse_error(database, NULL);
    return MYLITE_ERROR;
}

static int populate_window_value_function_result_column_descriptor(
    struct mylite_db *database,
    const struct planned_row_scalar_expression *expression,
    struct mylite_result_column_descriptor *descriptor
) {
    const struct planned_row_scalar_expression *argument = NULL;

    if (expression == NULL || descriptor == NULL || expression->argument_count == 0U ||
        expression->arguments == NULL) {
        return MYLITE_MISUSE;
    }

    argument = &expression->arguments[0];
    switch (argument->kind) {
    case PLANNED_ROW_SCALAR_EXPRESSION_COLUMN:
        return populate_window_value_column_result_column_descriptor(
            database,
            &argument->column,
            descriptor
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_VALUE:
        return populate_window_value_literal_result_column_descriptor(
            database,
            &argument->value,
            descriptor
        );
    case PLANNED_ROW_SCALAR_EXPRESSION_NONE:
    case PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTEGER_ARITHMETIC:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONCAT_WS:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIELD:
    case PLANNED_ROW_SCALAR_EXPRESSION_GREATEST:
    case PLANNED_ROW_SCALAR_EXPRESSION_LEAST:
    case PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIME_FORMAT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STR_TO_DATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_FORMAT_NUMERIC_EQUAL:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATE_INTERVAL_SECOND:
    case PLANNED_ROW_SCALAR_EXPRESSION_DATEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMEDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMPDIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CODEPOINT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_CASE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_TRIM:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNIX_TIMESTAMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_TEMPORAL_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_SEC_TO_TIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_UNIXTIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_DAYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKEDATE:
    case PLANNED_ROW_SCALAR_EXPRESSION_MAKETIME:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_ADD:
    case PLANNED_ROW_SCALAR_EXPRESSION_PERIOD_DIFF:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERT_TZ:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SLICE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_PADDING:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_BITMASK:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_SEARCH:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_INSERT:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_REVERSE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SOUNDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRING_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_SUBSTRING_INDEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_FIND_IN_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_STRCMP:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_LIKE:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_INSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_SUBSTR:
    case PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALID:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_CONTAINS_PATH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_VALUE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_QUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_UNQUOTE_EXTRACT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_ARRAY:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_OBJECT:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_TYPE:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_LENGTH:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_KEYS:
    case PLANNED_ROW_SCALAR_EXPRESSION_JSON_SET:
    case PLANNED_ROW_SCALAR_EXPRESSION_HEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_TO_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_FROM_BASE64:
    case PLANNED_ROW_SCALAR_EXPRESSION_UNHEX:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING:
    case PLANNED_ROW_SCALAR_EXPRESSION_WEIGHT_STRING_BINARY:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_IS_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_UUID_TO_BIN:
    case PLANNED_ROW_SCALAR_EXPRESSION_BIN_TO_UUID:
    case PLANNED_ROW_SCALAR_EXPRESSION_CONVERSION:
    case PLANNED_ROW_SCALAR_EXPRESSION_CHAR:
    case PLANNED_ROW_SCALAR_EXPRESSION_IF:
    case PLANNED_ROW_SCALAR_EXPRESSION_IFNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_COALESCE:
    case PLANNED_ROW_SCALAR_EXPRESSION_NULLIF:
    case PLANNED_ROW_SCALAR_EXPRESSION_ISNULL:
    case PLANNED_ROW_SCALAR_EXPRESSION_RAND:
        break;
    }

    set_unsupported_error(
        database,
        "window value metadata supports only literal and column values"
    );
    return MYLITE_ERROR;
}

static int populate_window_value_column_result_column_descriptor(
    struct mylite_db *database,
    const struct mylite_catalog_column_descriptor *column,
    struct mylite_result_column_descriptor *descriptor
) {
    bool matched = false;
    int rc = populate_numeric_result_column_descriptor(database, column, descriptor, &matched);

    if (rc == MYLITE_OK && !matched) {
        rc = populate_string_result_column_descriptor(database, NULL, column, descriptor, &matched);
    }
    if (rc == MYLITE_OK && !matched) {
        rc = populate_binary_result_column_descriptor(database, column, descriptor, &matched);
    }
    if (rc == MYLITE_OK && !matched) {
        rc = populate_temporal_result_column_descriptor(column, descriptor, &matched);
    }
    if (rc == MYLITE_OK && !matched) {
        rc = populate_bit_result_column_descriptor(column, descriptor, &matched);
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!matched) {
        set_unsupported_error(
            database,
            "window value metadata supports only current baseline column descriptors"
        );
        return MYLITE_ERROR;
    }

    descriptor->nullable = true;
    descriptor->flags &= ~((uint32_t)MYLITE_RESULT_COLUMN_FLAG_NOT_NULL);
    return MYLITE_OK;
}

static int populate_window_value_literal_result_column_descriptor(
    struct mylite_db *database,
    const struct planned_value *value,
    struct mylite_result_column_descriptor *descriptor
) {
    uint64_t display_length = 0U;

    if (database == NULL || value == NULL || descriptor == NULL) {
        return MYLITE_MISUSE;
    }
    if (value->is_null) {
        populate_scalar_binary_numeric_result_column_descriptor(
            descriptor,
            (struct scalar_binary_numeric_result_column_shape){
                .logical_type = MYLITE_RESULT_LOGICAL_TYPE_NULL,
                .display_length = 0U,
                .decimals = 0U,
                .extra_flags = 0U,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    }
    if (value->is_text) {
        if (value->text_length > UINT64_MAX / scalar_connection_max_bytes_per_character(database)) {
            set_nomem_error(database);
            return MYLITE_NOMEM;
        }
        display_length =
            (uint64_t)value->text_length * scalar_connection_max_bytes_per_character(database);
        populate_scalar_connection_string_result_column_descriptor(
            database,
            descriptor,
            (struct scalar_connection_string_result_column_shape){
                .display_length = display_length,
                .extra_flags = 0U,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    }
    if (value->is_real) {
        populate_scalar_binary_numeric_result_column_descriptor(
            descriptor,
            (struct scalar_binary_numeric_result_column_shape){
                .logical_type = MYLITE_RESULT_LOGICAL_TYPE_DOUBLE,
                .display_length = mysql_scalar_double_display_length,
                .decimals = mysql_approximate_decimals,
                .extra_flags = 0U,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    }

    populate_scalar_binary_numeric_result_column_descriptor(
        descriptor,
        (struct scalar_binary_numeric_result_column_shape){
            .logical_type = MYLITE_RESULT_LOGICAL_TYPE_LONGLONG,
            .display_length = mysql_scalar_bigint_display_length,
            .decimals = 0U,
            .extra_flags = 0U,
            .nullable = true,
        }
    );
    return MYLITE_OK;
}

static int populate_row_scalar_conversion_result_column_descriptor(
    const struct mylite_db *database,
    const struct planned_row_scalar_expression *expression,
    struct mylite_result_column_descriptor *descriptor
) {
    if (database == NULL || expression == NULL || descriptor == NULL) {
        return MYLITE_MISUSE;
    }

    switch (expression->conversion_kind) {
    case PLANNED_ROW_SCALAR_CONVERSION_BINARY:
    case PLANNED_ROW_SCALAR_CONVERSION_USING_BINARY:
        descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING;
        descriptor->charset_id = mysql_collation_binary_id;
        descriptor->collation_id = mysql_collation_binary_id;
        descriptor->display_length = mysql_scalar_bigint_display_length;
        descriptor->decimals = mysql_approximate_decimals;
        descriptor->flags = MYLITE_RESULT_COLUMN_FLAG_BINARY;
        descriptor->nullable = true;
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_CONVERSION_CHAR:
        populate_scalar_connection_string_result_column_descriptor(
            database,
            descriptor,
            (struct scalar_connection_string_result_column_shape){
                .display_length = mysql_scalar_bigint_display_length *
                                  scalar_connection_max_bytes_per_character(database),
                .extra_flags = 0U,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_CONVERSION_USING_CHARSET:
        descriptor->logical_type = MYLITE_RESULT_LOGICAL_TYPE_VAR_STRING;
        descriptor->charset_id = result_metadata_collation_id(expression->conversion_collation);
        descriptor->collation_id = descriptor->charset_id;
        descriptor->display_length = mysql_scalar_bigint_display_length *
                                     scalar_connection_max_bytes_per_character(database);
        descriptor->decimals = mysql_approximate_decimals;
        descriptor->flags = 0U;
        descriptor->nullable = true;
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_CONVERSION_SIGNED:
        populate_scalar_binary_numeric_result_column_descriptor(
            descriptor,
            (struct scalar_binary_numeric_result_column_shape){
                .logical_type = MYLITE_RESULT_LOGICAL_TYPE_LONGLONG,
                .display_length = mysql_scalar_bigint_display_length,
                .decimals = 0U,
                .extra_flags = 0U,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_CONVERSION_UNSIGNED:
        populate_scalar_binary_numeric_result_column_descriptor(
            descriptor,
            (struct scalar_binary_numeric_result_column_shape){
                .logical_type = MYLITE_RESULT_LOGICAL_TYPE_LONGLONG,
                .display_length = mysql_scalar_bigint_display_length,
                .decimals = 0U,
                .extra_flags = MYLITE_RESULT_COLUMN_FLAG_UNSIGNED,
                .nullable = true,
            }
        );
        return MYLITE_OK;
    case PLANNED_ROW_SCALAR_CONVERSION_NONE:
        break;
    }

    return MYLITE_ERROR;
}

static int populate_row_scalar_date_interval_second_result_column_descriptor(
    struct mylite_db *database,
    const struct planned_row_scalar_expression *expression,
    struct mylite_result_column_descriptor *descriptor
) {
    const struct planned_row_scalar_expression *input_kind_argument = NULL;
    const struct planned_row_scalar_expression *unit_argument = NULL;
    enum mylite_date_interval_second_input_kind input_kind =
        MYLITE_DATE_INTERVAL_SECOND_INPUT_STRING;
    enum mylite_date_interval_unit unit = MYLITE_DATE_INTERVAL_UNIT_SECOND;

    if (expression == NULL || descriptor == NULL) {
        return MYLITE_MISUSE;
    }
    if (expression->argument_count <= 3U || expression->arguments == NULL) {
        populate_scalar_temporal_string_result_column_descriptor(database, descriptor);
        return MYLITE_OK;
    }
    input_kind_argument = &expression->arguments[1];
    unit_argument = &expression->arguments[3];
    if (input_kind_argument->kind != PLANNED_ROW_SCALAR_EXPRESSION_VALUE ||
        input_kind_argument->value.is_null || !input_kind_argument->value.is_text ||
        !mylite_date_interval_second_input_kind_from_name(
            input_kind_argument->value.text,
            input_kind_argument->value.text_length,
            &input_kind
        ) ||
        unit_argument->kind != PLANNED_ROW_SCALAR_EXPRESSION_VALUE ||
        unit_argument->value.is_null || !unit_argument->value.is_text ||
        !mylite_date_interval_unit_from_name(
            unit_argument->value.text,
            unit_argument->value.text_length,
            &unit
        )) {
        populate_scalar_temporal_string_result_column_descriptor(database, descriptor);
        return MYLITE_OK;
    }

    switch (input_kind) {
    case MYLITE_DATE_INTERVAL_SECOND_INPUT_DATE:
        if (mylite_date_interval_unit_has_time_part(unit)) {
            populate_scalar_datetime_result_column_descriptor(descriptor);
        } else {
            populate_scalar_date_result_column_descriptor(descriptor);
        }
        return MYLITE_OK;
    case MYLITE_DATE_INTERVAL_SECOND_INPUT_DATETIME:
    case MYLITE_DATE_INTERVAL_SECOND_INPUT_TIMESTAMP:
        populate_scalar_datetime_result_column_descriptor(descriptor);
        return MYLITE_OK;
    case MYLITE_DATE_INTERVAL_SECOND_INPUT_STRING:
    default:
        populate_scalar_temporal_string_result_column_descriptor(database, descriptor);
        return MYLITE_OK;
    }
}

static int copy_row_scalar_result_column_name(
    struct mylite_db *database,
    const struct planned_row_scalar_select_item *item,
    char **out_name
) {
    const struct mylite_sql_ast_node *source_expression = NULL;

    if (out_name == NULL) {
        return MYLITE_MISUSE;
    }
    *out_name = NULL;
    if (item == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    source_expression = unwrap_parenthesized_expression(item->source_expression);
    if (item->alias != NULL) {
        return copy_select_item_alias_text(database, item->alias, out_name);
    }
    if (source_expression != NULL && source_expression->kind == MYLITE_SQL_AST_ANY_VALUE_FUNCTION) {
        return copy_scalar_projection_column_name(database, item->source_expression, out_name);
    }
    if (item->expression.kind == PLANNED_ROW_SCALAR_EXPRESSION_COLUMN) {
        return duplicate_text(database, item->expression.column.name, out_name);
    }

    return copy_scalar_projection_column_name(database, item->source_expression, out_name);
}

static int set_row_scalar_select_found_row_count(
    struct mylite_db *database,
    const struct planned_row_scalar_select *plan,
    mylite_result *result
) {
    struct planned_select count_plan = {0};
    uint64_t found_row_count = 0U;
    int rc = MYLITE_OK;

    if (!plan->has_source) {
        if (mylite_result_row_count(result) > UINT64_MAX) {
            set_runtime_error(database, "SELECT found-row count is out of range");
            return MYLITE_ERROR;
        }
        mylite_result_set_found_row_count(result, (uint64_t)mylite_result_row_count(result));
        return MYLITE_OK;
    }

    count_plan.table = plan->table;
    count_plan.predicate = plan->predicate;
    count_plan.limit = plan->limit;
    rc = found_row_count_for_select_limit_envelope(
        database,
        &count_plan,
        mylite_result_row_count(result),
        &found_row_count
    );
    if (rc == MYLITE_OK) {
        mylite_result_set_found_row_count(result, found_row_count);
    }
    return rc;
}

static int set_select_found_row_count(
    struct mylite_db *database,
    const struct planned_select *plan,
    mylite_result *result
) {
    int64_t count = 0;
    uint64_t found_row_count = 0U;
    int rc = MYLITE_OK;

    if (plan->calc_found_rows) {
        rc = read_select_found_row_count(database, plan, &count);
        if (rc != MYLITE_OK) {
            return rc;
        }
        if (count < 0) {
            set_runtime_error(database, "invalid SQL_CALC_FOUND_ROWS count");
            return MYLITE_ERROR;
        }
        mylite_result_set_found_row_count(result, (uint64_t)count);
        return MYLITE_OK;
    }

    rc = found_row_count_for_select_limit_envelope(
        database,
        plan,
        mylite_result_row_count(result),
        &found_row_count
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    mylite_result_set_found_row_count(result, found_row_count);
    return MYLITE_OK;
}

static int found_row_count_for_select_limit_envelope(
    struct mylite_db *database,
    const struct planned_select *plan,
    size_t visible_row_count,
    uint64_t *out_found_row_count
) {
    uint64_t visible_rows = (uint64_t)visible_row_count;
    uint64_t offset = 0U;
    int64_t total_count = 0;
    int rc = MYLITE_OK;

    if (out_found_row_count == NULL) {
        return MYLITE_MISUSE;
    }
    *out_found_row_count = 0U;
    if (visible_row_count > UINT64_MAX) {
        set_runtime_error(database, "SELECT found-row count is out of range");
        return MYLITE_ERROR;
    }
    if (!plan->limit.has_limit) {
        *out_found_row_count = visible_rows;
        return MYLITE_OK;
    }
    if (!plan->limit.has_offset) {
        *out_found_row_count = visible_rows;
        return MYLITE_OK;
    }

    offset = (uint64_t)plan->limit.offset;
    if (plan->limit.row_count != 0 && visible_row_count != 0U) {
        if (offset > UINT64_MAX - visible_rows) {
            set_runtime_error(database, "SELECT found-row count is out of range");
            return MYLITE_ERROR;
        }
        *out_found_row_count = offset + visible_rows;
        return MYLITE_OK;
    }
    if (offset == 0U) {
        *out_found_row_count = 0U;
        return MYLITE_OK;
    }

    rc = read_select_found_row_count(database, plan, &total_count);
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (total_count < 0) {
        set_runtime_error(database, "invalid SELECT found-row count");
        return MYLITE_ERROR;
    }
    *out_found_row_count = offset < (uint64_t)total_count ? offset : (uint64_t)total_count;
    return MYLITE_OK;
}

static int read_select_found_row_count(
    struct mylite_db *database,
    const struct planned_select *plan,
    int64_t *out_count
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int parameter_index = 1;
    int rc = build_select_found_rows_sql(plan, &sql);

    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        rc = bind_select_predicate_parameters(statement, &plan->predicate, &parameter_index);
    }
    if (rc == MYLITE_OK) {
        rc = step_count_statement(statement, out_count);
    }
    rc = finalize_sqlite_statement(statement, rc);
    free(sql);
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
        return rc;
    }
    if (rc != MYLITE_OK &&
        mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) == MYLITE_OK) {
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }
    return rc;
}
