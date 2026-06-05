
static int build_modify_temporary_physical_name(
    const struct planned_alter_table_modify_column *plan,
    uint64_t generation,
    char *destination,
    size_t destination_size
) {
    int written = 0;

    if (plan == NULL || destination == NULL || destination_size == 0U) {
        return MYLITE_MISUSE;
    }

    written = snprintf(
        destination,
        destination_size,
        "_mylite_user_table_%" PRId64 "_modify_%" PRIu64,
        plan->table.table_id,
        generation
    );
    if (written < 0 || (size_t)written >= destination_size) {
        return MYLITE_NOMEM;
    }

    return MYLITE_OK;
}

static int build_alter_table_modify_validation_sql(
    const struct planned_alter_table_modify_column *plan,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->original_column.name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " ORDER BY ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, plan->rowid_alias);
    }
    if (rc == MYLITE_OK) {
        *out_sql = mylite_dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    mylite_dynamic_string_deinit(&string);

    return rc;
}

static int build_alter_table_modify_create_sql(
    const struct planned_alter_table_modify_column *plan,
    const char *temporary_physical_name,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(
        &string,
        plan->is_temporary ? "CREATE TEMPORARY TABLE " : "CREATE TABLE "
    );
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, temporary_physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " (");
    }
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        const struct mylite_catalog_column_descriptor *column = &plan->columns[column_index];

        if (column_index != 0U) {
            rc = mylite_dynamic_string_append(&string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(&string, column->name);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_char(&string, ' ');
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(&string, column->physical_type);
        }
        if (rc == MYLITE_OK && !column->is_nullable) {
            rc = mylite_dynamic_string_append(&string, " NOT NULL");
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(&string, ')');
    }
    if (rc == MYLITE_OK) {
        *out_sql = mylite_dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    mylite_dynamic_string_deinit(&string);

    return rc;
}

static int build_alter_table_modify_copy_sql(
    const struct planned_alter_table_modify_column *plan,
    const char *temporary_physical_name,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "INSERT INTO ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, temporary_physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " (");
    }
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        if (column_index != 0U) {
            rc = mylite_dynamic_string_append(&string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(
                &string,
                plan->columns[column_index].name
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, ") SELECT ");
    }
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        const char *source_name = plan->columns[column_index].name;

        if (plan->columns[column_index].column_id == plan->original_column.column_id) {
            source_name = plan->original_column.name;
        }
        if (column_index != 0U) {
            rc = mylite_dynamic_string_append(&string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(&string, source_name);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " ORDER BY ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, plan->rowid_alias);
    }
    if (rc == MYLITE_OK) {
        *out_sql = mylite_dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    mylite_dynamic_string_deinit(&string);

    return rc;
}

static bool alter_table_modify_needs_binary_copy_materialization(
    const struct planned_alter_table_modify_column *plan
) {
    for (size_t column_index = 0U; column_index < plan->column_count; ++column_index) {
        const struct mylite_catalog_column_descriptor *column = &plan->columns[column_index];

        if (column->column_id == plan->original_column.column_id &&
            column_descriptor_is_binary_string_family(column)) {
            return true;
        }
    }

    return false;
}

static int execute_physical_alter_table_modify_copy_rows(
    struct mylite_db *database,
    const struct planned_alter_table_modify_column *plan,
    const char *temporary_physical_name
) {
    struct alter_table_modify_copy_statements statements = {0};
    struct planned_value *values = NULL;
    char *select_sql = NULL;
    char *insert_sql = NULL;
    size_t row_number = 0U;
    int sqlite_rc = SQLITE_OK;
    int rc = MYLITE_OK;

    if (plan->column_count > (size_t)INT_MAX) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    values = calloc(plan->column_count, sizeof(*values));
    if (values == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    rc = build_alter_table_modify_select_sql(plan, &select_sql);
    if (rc == MYLITE_OK) {
        rc = build_alter_table_modify_insert_sql(plan, temporary_physical_name, &insert_sql);
    }
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, select_sql, &statements.select_statement);
    }
    if (rc == MYLITE_OK) {
        rc = prepare_sqlite_statement(database, insert_sql, &statements.insert_statement);
    }
    while (rc == MYLITE_OK &&
           (sqlite_rc = sqlite3_step(statements.select_statement)) == SQLITE_ROW) {
        if (row_number >= (size_t)INT64_MAX) {
            set_runtime_error(database, plan->row_count_overflow_message);
            rc = MYLITE_ERROR;
            break;
        }
        ++row_number;
        rc = execute_alter_table_modify_copy_row(database, &statements, plan, row_number, values);
        deinit_alter_table_modify_copy_row(values, plan->column_count);
    }
    if (rc == MYLITE_OK && sqlite_rc != SQLITE_DONE) {
        rc = mylite_sqlite_status_to_mylite(sqlite_rc);
    }
    deinit_alter_table_modify_copy_row(values, plan->column_count);
    rc = finalize_sqlite_statement(statements.insert_statement, rc);
    statements.insert_statement = NULL;
    rc = finalize_sqlite_statement(statements.select_statement, rc);
    free(values);
    free(select_sql);
    free(insert_sql);
    return rc;
}

static int build_alter_table_modify_select_sql(
    const struct planned_alter_table_modify_column *plan,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT ");
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        const char *source_name = plan->columns[column_index].name;

        if (plan->columns[column_index].column_id == plan->original_column.column_id) {
            source_name = plan->original_column.name;
        }
        if (column_index != 0U) {
            rc = mylite_dynamic_string_append(&string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(&string, source_name);
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " ORDER BY ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, plan->rowid_alias);
    }
    if (rc == MYLITE_OK) {
        *out_sql = mylite_dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    mylite_dynamic_string_deinit(&string);

    return rc;
}

static int build_alter_table_modify_insert_sql(
    const struct planned_alter_table_modify_column *plan,
    const char *temporary_physical_name,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "INSERT INTO ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, temporary_physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " (");
    }
    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        if (column_index != 0U) {
            rc = mylite_dynamic_string_append(&string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(
                &string,
                plan->columns[column_index].name
            );
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, ") VALUES (");
    }
    if (rc == MYLITE_OK) {
        rc = append_insert_parameters(&string, plan->column_count);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(&string, ')');
    }
    if (rc == MYLITE_OK) {
        *out_sql = mylite_dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    mylite_dynamic_string_deinit(&string);

    return rc;
}

static int execute_alter_table_modify_copy_row(
    struct mylite_db *database,
    struct alter_table_modify_copy_statements *statements,
    const struct planned_alter_table_modify_column *plan,
    size_t row_number,
    struct planned_value *values
) {
    int sqlite_rc = SQLITE_OK;
    int rc = materialize_alter_table_modify_copy_row(
        database,
        statements->select_statement,
        plan,
        row_number,
        values
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = bind_alter_table_modify_copy_row(statements->insert_statement, plan, values);
    if (rc != MYLITE_OK) {
        return rc;
    }
    sqlite_rc = sqlite3_step(statements->insert_statement);
    if (sqlite_rc != SQLITE_DONE) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }
    sqlite_rc = sqlite3_reset(statements->insert_statement);
    if (sqlite_rc != SQLITE_OK) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }
    sqlite_rc = sqlite3_clear_bindings(statements->insert_statement);
    if (sqlite_rc != SQLITE_OK) {
        return mylite_sqlite_status_to_mylite(sqlite_rc);
    }

    return MYLITE_OK;
}

static int materialize_alter_table_modify_copy_row(
    struct mylite_db *database,
    sqlite3_stmt *select_statement,
    const struct planned_alter_table_modify_column *plan,
    size_t row_number,
    struct planned_value *values
) {
    for (size_t column_index = 0U; column_index < plan->column_count; ++column_index) {
        const struct mylite_catalog_column_descriptor *column = &plan->columns[column_index];
        int rc = MYLITE_OK;

        if (column_index > (size_t)INT_MAX) {
            return MYLITE_ERROR;
        }
        if (column->column_id == plan->original_column.column_id &&
            column_descriptor_is_binary_string_family(column)) {
            if (sqlite3_column_type(select_statement, (int)column_index) == SQLITE_NULL) {
                values[column_index] = (struct planned_value){.is_null = true};
            } else {
                rc = materialize_sqlite_binary_string_value(
                    database,
                    select_statement,
                    (int)column_index,
                    column,
                    row_number,
                    &values[column_index]
                );
            }
        } else {
            rc = materialize_insert_select_raw_value(
                database,
                select_statement,
                (int)column_index,
                &values[column_index]
            );
        }
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return MYLITE_OK;
}

static int bind_alter_table_modify_copy_row(
    sqlite3_stmt *insert_statement,
    const struct planned_alter_table_modify_column *plan,
    const struct planned_value *values
) {
    for (size_t column_index = 0U; column_index < plan->column_count; ++column_index) {
        int rc = MYLITE_OK;

        if (column_index >= (size_t)INT_MAX) {
            return MYLITE_ERROR;
        }
        rc = bind_planned_value_parameter(
            insert_statement,
            (int)column_index + 1,
            &values[column_index]
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    return MYLITE_OK;
}

static void deinit_alter_table_modify_copy_row(struct planned_value *values, size_t column_count) {
    if (values == NULL) {
        return;
    }
    for (size_t column_index = 0U; column_index < column_count; ++column_index) {
        planned_value_deinit(&values[column_index]);
    }
}

static int build_alter_table_modify_indexes_sql(
    const struct planned_alter_table_modify_column *plan,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    for (size_t index = 0U; rc == MYLITE_OK && index < plan->index_count; ++index) {
        rc = append_alter_table_modify_index_sql(&string, plan, &plan->indexes[index]);
    }
    if (rc == MYLITE_OK && string.length != 0U) {
        *out_sql = mylite_dynamic_string_take(&string);
        if (*out_sql == NULL) {
            rc = MYLITE_NOMEM;
        }
    }

    mylite_dynamic_string_deinit(&string);

    return rc;
}

static int append_alter_table_modify_index_sql(
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_modify_column *plan,
    const struct loaded_index_info *index
) {
    bool is_unique = index->index.is_unique;
    int rc = MYLITE_OK;

    if (index->index.kind == MYLITE_CATALOG_INDEX_KIND_FULLTEXT) {
        return MYLITE_OK;
    }
    if (index->index.kind == MYLITE_CATALOG_INDEX_KIND_PRIMARY) {
        is_unique = true;
    }
    if (string->length != 0U) {
        rc = mylite_dynamic_string_append(string, "; ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, "CREATE ");
    }
    if (rc == MYLITE_OK && is_unique) {
        rc = mylite_dynamic_string_append(string, "UNIQUE ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, "INDEX ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, index->index.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " ON ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " (");
    }
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < index->part_count; ++part_index) {
        const struct loaded_index_part *part = &index->parts[part_index];

        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_loaded_key_part_sql(string, part, NULL);
        }
        if (rc == MYLITE_OK &&
            part->index_column.sort_direction == MYLITE_CATALOG_INDEX_SORT_DIRECTION_DESC) {
            rc = mylite_dynamic_string_append(string, " DESC");
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }

    return rc;
}
