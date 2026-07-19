
int build_alter_table_order_temporary_physical_name(
    const struct planned_alter_table_order_by *plan,
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
        "_mylite_user_table_%" PRId64 "_order_%" PRIu64,
        plan->table.table_id,
        sqlite_schema_generation
    );
    if (written < 0 || (size_t)written >= destination_size) {
        return MYLITE_NOMEM;
    }

    return MYLITE_OK;
}

int build_alter_table_order_create_sql(
    const struct planned_alter_table_order_by *plan,
    const char *temporary_physical_name,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "CREATE TABLE ");
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

int build_alter_table_order_copy_sql(
    const struct planned_alter_table_order_by *plan,
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
        rc = append_alter_table_order_column_list(&string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, ") SELECT ");
    }
    if (rc == MYLITE_OK) {
        rc = append_alter_table_order_column_list(&string, plan);
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
        rc = append_alter_table_order_order_list(&string, plan);
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

static int append_alter_table_order_column_list(
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_order_by *plan
) {
    int rc = MYLITE_OK;

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        if (column_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(
                string,
                plan->columns[column_index].name
            );
        }
    }

    return rc;
}

static int append_alter_table_order_order_list(
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_order_by *plan
) {
    int rc = MYLITE_OK;

    for (size_t item_index = 0U; rc == MYLITE_OK && item_index < plan->item_count; ++item_index) {
        const struct planned_alter_table_order_by_item *item = &plan->items[item_index];

        if (item_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(string, item->column.name);
        }
        if (rc == MYLITE_OK) {
            if (item->direction == PLANNED_SELECT_ORDER_DESC) {
                rc = mylite_dynamic_string_append(string, " DESC");
            } else {
                rc = mylite_dynamic_string_append(string, " ASC");
            }
        }
    }

    return rc;
}

int build_alter_table_force_temporary_physical_name(
    const struct planned_alter_table_force *plan,
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
        "_mylite_user_table_%" PRId64 "_force_%" PRIu64,
        plan->table.table_id,
        sqlite_schema_generation
    );
    if (written < 0 || (size_t)written >= destination_size) {
        return MYLITE_NOMEM;
    }

    return MYLITE_OK;
}

int build_alter_table_force_create_sql(
    const struct planned_alter_table_force *plan,
    const char *temporary_physical_name,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "CREATE TABLE ");
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

int build_alter_table_force_copy_sql(
    const struct planned_alter_table_force *plan,
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
        rc = append_alter_table_force_column_list(&string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, ") SELECT ");
    }
    if (rc == MYLITE_OK) {
        rc = append_alter_table_force_column_list(&string, plan);
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

static int append_alter_table_force_column_list(
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_force *plan
) {
    int rc = MYLITE_OK;

    for (size_t column_index = 0U; rc == MYLITE_OK && column_index < plan->column_count;
         ++column_index) {
        if (column_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(
                string,
                plan->columns[column_index].name
            );
        }
    }

    return rc;
}

int build_alter_table_rename_physical_table_sql(
    const char *source_physical_name,
    const char *target_physical_name,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "ALTER TABLE ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, source_physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " RENAME TO ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, target_physical_name);
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

int build_truncate_table_sql(const struct planned_truncate_table *plan, char **out_sql) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "DELETE FROM ");
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
