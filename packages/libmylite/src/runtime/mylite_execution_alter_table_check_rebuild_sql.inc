static int build_alter_table_check_temporary_physical_name(
    const struct planned_alter_table_check_constraint *plan,
    uint64_t sqlite_schema_generation,
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
        "_mylite_user_table_%" PRId64 "_check_%" PRIu64,
        plan->table.table_id,
        sqlite_schema_generation
    );
    if (written < 0 || (size_t)written >= destination_size) {
        return MYLITE_NOMEM;
    }

    return MYLITE_OK;
}

static int build_alter_table_check_create_sql(
    const struct planned_alter_table_check_constraint *plan,
    const char *temporary_physical_name,
    char **out_sql
) {
    return build_create_table_definition_sql(
        &plan->rebuild,
        temporary_physical_name,
        false,
        out_sql
    );
}

static int build_alter_table_check_copy_sql(
    const struct planned_alter_table_check_constraint *plan,
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
    if (rc == MYLITE_OK) {
        rc = append_alter_table_check_column_list(&string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, ") SELECT ");
    }
    if (rc == MYLITE_OK) {
        rc = append_alter_table_check_column_list(&string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
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

static int append_alter_table_check_column_list(
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_check_constraint *plan
) {
    int rc = MYLITE_OK;

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->rebuild.column_count;
         ++column_index) {
        if (column_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(
                string,
                plan->rebuild.columns[column_index].name
            );
        }
    }

    return rc;
}
