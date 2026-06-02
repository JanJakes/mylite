
static int bind_row_scalar_string_slice_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }
    if (expression->string_slice_kind == PLANNED_STRING_SLICE_FUNCTION_SUBSTRING) {
        return bind_row_scalar_substring_expression_parameters(
            statement,
            expression,
            parameter_index
        );
    }
    return bind_row_scalar_left_right_expression_parameters(statement, expression, parameter_index);
}

static int bind_row_scalar_string_padding_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_string_bitmask_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_string_search_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 3U) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 3U; ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_string_replace_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 3U) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 3U; ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_string_insert_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 4U) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 4U; ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_string_reverse_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }
    return bind_row_scalar_non_concat_expression_parameters(
        statement,
        &expression->arguments[0],
        parameter_index
    );
}

static int bind_row_scalar_soundex_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }
    return bind_row_scalar_non_concat_expression_parameters(
        statement,
        &expression->arguments[0],
        parameter_index
    );
}

static int bind_row_scalar_string_quote_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }
    return bind_row_scalar_non_concat_expression_parameters(
        statement,
        &expression->arguments[0],
        parameter_index
    );
}

static int bind_row_scalar_substring_index_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 3U) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 3U; ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_find_in_set_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 2U; ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_strcmp_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < 2U; ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_regexp_like_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL || expression->argument_count != 2U) {
        return MYLITE_ERROR;
    }
    rc = bind_row_scalar_non_concat_expression_parameters(
        statement,
        &expression->arguments[1],
        parameter_index
    );
    if (rc == MYLITE_OK) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[0],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_regexp_string_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->arguments == NULL ||
        (expression->argument_count != 2U && expression->argument_count != 3U)) {
        return MYLITE_ERROR;
    }
    rc = bind_row_scalar_non_concat_expression_parameters(
        statement,
        &expression->arguments[1],
        parameter_index
    );
    if (rc == MYLITE_OK) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[0],
            parameter_index
        );
    }
    if (rc == MYLITE_OK && expression->kind == PLANNED_ROW_SCALAR_EXPRESSION_REGEXP_REPLACE) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[2],
            parameter_index
        );
    }
    return rc;
}
