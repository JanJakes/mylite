
static int bind_row_scalar_window_function_expression_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *expression,
    int *parameter_index
) {
    int rc = MYLITE_OK;

    if (statement == NULL || expression == NULL || parameter_index == NULL ||
        expression->kind != PLANNED_ROW_SCALAR_EXPRESSION_WINDOW_FUNCTION) {
        return MYLITE_MISUSE;
    }
    for (size_t argument_index = 0U; rc == MYLITE_OK && argument_index < expression->argument_count;
         ++argument_index) {
        rc = bind_row_scalar_window_function_argument_parameters(
            statement,
            &expression->arguments[argument_index],
            parameter_index
        );
    }
    return rc;
}

static int bind_row_scalar_window_function_argument_parameters(
    sqlite3_stmt *statement,
    const struct planned_row_scalar_expression *argument,
    int *parameter_index
) {
    if (argument == NULL || parameter_index == NULL) {
        return MYLITE_MISUSE;
    }
    return bind_row_scalar_expression_parameters(statement, argument, parameter_index);
}
