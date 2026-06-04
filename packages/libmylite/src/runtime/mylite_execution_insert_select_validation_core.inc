
static int validate_insert_select_rows(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_insert_select *plan
) {
    bool checked_omitted_columns = false;
    size_t row_number = 0U;
    int sqlite_rc = SQLITE_OK;
    int rc = MYLITE_OK;

    while ((sqlite_rc = sqlite3_step(statement)) == SQLITE_ROW) {
        if (!checked_omitted_columns) {
            rc = check_insert_omitted_columns(
                database,
                &plan->target,
                plan->target_indexes,
                plan->target_count
            );
            checked_omitted_columns = true;
            if (rc == MYLITE_OK && insert_select_allows_adjustment(database, plan)) {
                rc = append_insert_omitted_column_warnings(
                    database,
                    &plan->target,
                    plan->target_indexes,
                    plan->target_count
                );
            }
        }
        if (rc == MYLITE_OK && row_number == SIZE_MAX) {
            set_unsupported_error(database, "INSERT ... SELECT selected too many rows");
            rc = MYLITE_ERROR;
        }
        if (rc == MYLITE_OK) {
            ++row_number;
            rc = validate_insert_select_row(database, statement, plan, row_number);
        }
        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    if (sqlite_rc != SQLITE_DONE) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    return MYLITE_OK;
}

static int validate_insert_select_row(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_insert_select *plan,
    size_t row_number
) {
    bool adjust_value = insert_select_allows_adjustment(database, plan);

    if (plan->target_count > (size_t)INT_MAX) {
        return MYLITE_ERROR;
    }
    for (size_t target_position = 0U; target_position < plan->target_count; ++target_position) {
        size_t column_index = plan->target_indexes[target_position];
        int rc = MYLITE_OK;

        if (column_descriptor_is_auto_increment(&plan->target.columns[column_index]) &&
            sqlite3_column_type(statement, (int)target_position) == SQLITE_NULL) {
            continue;
        }
        const struct mylite_catalog_column_descriptor *source_column = NULL;

        rc = insert_select_source_column_for_position(
            database,
            statement,
            plan,
            target_position,
            &source_column
        );
        if (rc == MYLITE_OK) {
            rc = validate_insert_select_value(
                database,
                statement,
                (int)target_position,
                source_column,
                &plan->target.columns[column_index],
                row_number,
                adjust_value
            );
        }

        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return MYLITE_OK;
}

static int insert_select_source_column_for_position(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    const struct planned_insert_select *plan,
    size_t target_position,
    const struct mylite_catalog_column_descriptor **out_source_column
) {
    sqlite3_int64 branch_index = 0;
    const struct planned_insert_select_compound_branch *branch = NULL;

    *out_source_column = NULL;
    if (plan == NULL || statement == NULL) {
        set_runtime_error(database, "invalid INSERT ... SELECT source validation");
        return MYLITE_ERROR;
    }
    if (plan->source_kind == PLANNED_INSERT_SELECT_SOURCE_TABLE) {
        if (target_position >= plan->source.column_count) {
            set_runtime_error(database, "invalid INSERT ... SELECT source column");
            return MYLITE_ERROR;
        }
        *out_source_column = &plan->source.columns[target_position];
        return MYLITE_OK;
    }
    if (plan->source_kind != PLANNED_INSERT_SELECT_SOURCE_COMPOUND) {
        return MYLITE_OK;
    }
    if (plan->target_count > (size_t)INT_MAX ||
        sqlite3_column_type(statement, (int)plan->target_count) != SQLITE_INTEGER) {
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }

    branch_index = sqlite3_column_int64(statement, (int)plan->target_count);
    if (branch_index < 0 ||
        (uint64_t)branch_index >= (uint64_t)plan->compound_source.branch_count) {
        set_physical_sqlite_row_error(database);
        return MYLITE_ERROR;
    }
    branch = &plan->compound_source.branches[(size_t)branch_index];
    if (branch->kind != PLANNED_INSERT_SELECT_COMPOUND_BRANCH_TABLE) {
        return MYLITE_OK;
    }
    if (target_position >= branch->source.column_count) {
        set_runtime_error(database, "invalid INSERT ... SELECT UNION source column");
        return MYLITE_ERROR;
    }

    *out_source_column = &branch->source.columns[target_position];
    return MYLITE_OK;
}

static int validate_insert_select_value(
    struct mylite_db *database,
    sqlite3_stmt *statement,
    int selected_column_index,
    const struct mylite_catalog_column_descriptor *source_column,
    const struct mylite_catalog_column_descriptor *target_column,
    size_t row_number,
    bool adjust_value
) {
    int sqlite_type = sqlite3_column_type(statement, selected_column_index);

    if (source_column != NULL &&
        !insert_select_source_target_types_are_compatible(source_column, target_column)) {
        set_unsupported_error(database, insert_select_implicit_conversion_message(target_column));
        return MYLITE_ERROR;
    }
    if (sqlite_type == SQLITE_NULL) {
        if (!target_column->is_nullable) {
            if (adjust_value) {
                return MYLITE_OK;
            }
            if (column_descriptor_is_spatial(target_column)) {
                set_spatial_bad_null_error(database, target_column->name);
                return MYLITE_ERROR;
            }
            set_bad_null_error(database, target_column->name);
            return MYLITE_ERROR;
        }
        return MYLITE_OK;
    }
    if (column_descriptor_is_string_family(target_column)) {
        return validate_insert_select_convertible_string_value(
            database,
            statement,
            selected_column_index,
            (struct insert_select_string_validation){
                .source_column = source_column,
                .target_column = target_column,
                .row_number = row_number,
                .adjust_value = adjust_value,
            }
        );
    }
    if (column_descriptor_is_json(target_column)) {
        (void)row_number;
        return validate_insert_select_json_value(
            database,
            statement,
            selected_column_index,
            target_column
        );
    }
    if (column_descriptor_is_binary_string_family(target_column)) {
        return validate_insert_select_binary_string_value(
            database,
            statement,
            selected_column_index,
            target_column,
            row_number
        );
    }
    if (column_descriptor_is_bit(target_column)) {
        return validate_insert_select_bit_value(
            database,
            statement,
            selected_column_index,
            target_column,
            row_number
        );
    }
    if (column_descriptor_is_year(target_column)) {
        return validate_insert_select_year_value(
            database,
            statement,
            selected_column_index,
            target_column,
            row_number
        );
    }
    if (column_descriptor_is_decimal(target_column)) {
        return validate_insert_select_decimal_value(
            database,
            statement,
            selected_column_index,
            target_column,
            row_number
        );
    }
    if (column_descriptor_is_approximate(target_column)) {
        return validate_insert_select_approximate_value(
            database,
            statement,
            selected_column_index,
            target_column,
            row_number
        );
    }
    if (column_descriptor_is_date(target_column)) {
        return validate_insert_select_date_value(
            database,
            statement,
            selected_column_index,
            target_column,
            row_number
        );
    }
    if (column_descriptor_is_time(target_column)) {
        return validate_insert_select_time_value(
            database,
            statement,
            selected_column_index,
            target_column,
            row_number
        );
    }
    if (column_descriptor_is_datetime(target_column)) {
        return validate_insert_select_datetime_value(
            database,
            statement,
            selected_column_index,
            target_column,
            row_number
        );
    }
    if (column_descriptor_is_timestamp(target_column)) {
        return validate_insert_select_timestamp_value(
            database,
            statement,
            selected_column_index,
            target_column,
            row_number
        );
    }

    return validate_insert_select_integer_value(
        database,
        statement,
        selected_column_index,
        target_column,
        row_number,
        adjust_value
    );
}

static bool insert_select_source_target_types_are_compatible(
    const struct mylite_catalog_column_descriptor *source_column,
    const struct mylite_catalog_column_descriptor *target_column
) {
    if (source_column == NULL || target_column == NULL) {
        return false;
    }
    if (column_descriptor_is_string_family(target_column)) {
        return column_descriptor_is_string_family(source_column);
    }
    if (column_descriptor_is_json(target_column)) {
        return column_descriptor_is_json(source_column);
    }
    if (column_descriptor_is_enum(target_column)) {
        return false;
    }
    if (column_descriptor_is_set(target_column)) {
        return false;
    }
    if (column_descriptor_is_binary_string_family(target_column)) {
        return column_descriptor_is_binary_string_family(source_column);
    }
    if (column_descriptor_is_bit(target_column)) {
        return (column_descriptor_is_bit(source_column) &&
                strcmp(source_column->logical_type, target_column->logical_type) == 0) != 0;
    }
    if (column_descriptor_is_year(target_column)) {
        return column_descriptor_is_year(source_column);
    }
    if (column_descriptor_is_decimal(target_column)) {
        return column_descriptor_is_decimal(source_column);
    }
    if (column_descriptor_is_approximate(target_column)) {
        return (column_descriptor_is_approximate(source_column) &&
                strcmp(source_column->logical_type, target_column->logical_type) == 0) != 0;
    }
    if (column_descriptor_is_date(target_column)) {
        return (column_descriptor_is_date(source_column) ||
                column_descriptor_is_string_family(source_column)) != 0;
    }
    if (column_descriptor_is_time(target_column)) {
        return (column_descriptor_is_time(source_column) ||
                column_descriptor_is_string_family(source_column)) != 0;
    }
    if (column_descriptor_is_datetime(target_column)) {
        return (column_descriptor_is_datetime(source_column) ||
                column_descriptor_is_string_family(source_column)) != 0;
    }
    if (column_descriptor_is_timestamp(target_column)) {
        return (column_descriptor_is_timestamp(source_column) ||
                column_descriptor_is_string_family(source_column)) != 0;
    }

    return (source_column->physical_type[0] != '\0' &&
            strcmp(source_column->physical_type, "INTEGER") == 0) != 0;
}

static const char *insert_select_implicit_conversion_message(
    const struct mylite_catalog_column_descriptor *target_column
) {
    if (column_descriptor_is_string_family(target_column)) {
        return "INSERT ... SELECT does not support implicit string conversion";
    }
    if (column_descriptor_is_json(target_column)) {
        return "INSERT ... SELECT does not support implicit JSON conversion";
    }
    if (column_descriptor_is_enum(target_column)) {
        return "INSERT ... SELECT does not support implicit ENUM conversion";
    }
    if (column_descriptor_is_set(target_column)) {
        return "INSERT ... SELECT does not support implicit SET conversion";
    }
    if (column_descriptor_is_binary_string_family(target_column)) {
        return "INSERT ... SELECT does not support implicit binary string conversion";
    }
    if (column_descriptor_is_bit(target_column)) {
        return "INSERT ... SELECT does not support implicit BIT conversion";
    }
    if (column_descriptor_is_year(target_column)) {
        return "INSERT ... SELECT does not support implicit YEAR conversion";
    }
    if (column_descriptor_is_decimal(target_column)) {
        return "INSERT ... SELECT does not support implicit DECIMAL conversion";
    }
    if (column_descriptor_is_approximate(target_column)) {
        return "INSERT ... SELECT does not support implicit FLOAT or DOUBLE conversion";
    }
    if (column_descriptor_is_date(target_column)) {
        return "INSERT ... SELECT does not support implicit DATE conversion";
    }
    if (column_descriptor_is_time(target_column)) {
        return "INSERT ... SELECT does not support implicit TIME conversion";
    }
    if (column_descriptor_is_datetime(target_column)) {
        return "INSERT ... SELECT does not support implicit DATETIME conversion";
    }
    if (column_descriptor_is_timestamp(target_column)) {
        return "INSERT ... SELECT does not support implicit TIMESTAMP conversion";
    }

    return "INSERT ... SELECT does not support implicit integer conversion";
}
