
static int bind_row_scalar_interval_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_INTERVAL ||
        expression->argument_count < 2U) {
        return MYLITE_ERROR;
    }

    rc = bind_row_scalar_non_concat_expression_parameters(
        statement,
        &expression->arguments[0],
        parameter_index
    );
    for (size_t argument_index = 1U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[0],
            parameter_index
        );
        if (rc == MYLITE_OK) {
            rc = bind_row_scalar_non_concat_expression_parameters(
                statement,
                &expression->arguments[argument_index],
                parameter_index
            );
        }
    }
    return rc;
}

static int bind_row_scalar_date_format_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL ||
        (expression->argument_count != 2U && expression->argument_count != 3U)) {
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

static int bind_row_scalar_date_interval_second_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != date_interval_planned_argument_count) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U;
         rc == MYLITE_OK && argument_index < date_interval_planned_argument_count;
         ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_datediff_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 4U) {
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

static int bind_row_scalar_timediff_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 4U) {
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

static int bind_row_scalar_timestampdiff_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL ||
        expression->argument_count != planned_row_scalar_timestampdiff_argument_count) {
        return MYLITE_ERROR;
    }
    for (size_t argument_index = 0U;
         rc == MYLITE_OK && argument_index < planned_row_scalar_timestampdiff_argument_count;
         ++argument_index) {
        rc = bind_row_scalar_non_concat_expression_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_timestamp_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 4U) {
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

static int bind_row_scalar_string_length_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }
    return bind_row_scalar_non_concat_expression_parameters(
        statement,
        &expression->arguments[0],
        parameter_index
    );
}

static int bind_row_scalar_string_codepoint_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }
    return bind_row_scalar_non_concat_expression_parameters(
        statement,
        &expression->arguments[0],
        parameter_index
    );
}

static int bind_row_scalar_string_case_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }
    return bind_row_scalar_non_concat_expression_parameters(
        statement,
        &expression->arguments[0],
        parameter_index
    );
}

static int bind_row_scalar_string_trim_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 2U) {
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

static int bind_row_scalar_temporal_extract_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count == 0U) {
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

static int bind_row_scalar_sec_to_time_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }
    return bind_row_scalar_non_concat_expression_parameters(
        statement,
        &expression->arguments[0],
        parameter_index
    );
}

static int bind_row_scalar_from_unixtime_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    if (expression == NULL || expression->argument_count != 1U) {
        return MYLITE_ERROR;
    }
    return bind_row_scalar_non_concat_expression_parameters(
        statement,
        &expression->arguments[0],
        parameter_index
    );
}

static int bind_row_scalar_temporal_constructor_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count == 0U) {
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

static int bind_row_scalar_unix_timestamp_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (expression == NULL || expression->argument_count != 2U) {
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
