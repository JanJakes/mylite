
int build_drop_table_sql(const char *physical_name, char **out_sql) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "DROP TABLE ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, physical_name);
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

int build_drop_index_sql(const char *physical_name, char **out_sql) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "DROP INDEX ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, physical_name);
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

int build_alter_table_add_column_sql(
    struct mylite_db *database,
    const struct planned_alter_table_add_column *plan,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "ALTER TABLE ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " ADD COLUMN ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->column.name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(&string, ' ');
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, plan->column.physical_type);
    }
    if (rc == MYLITE_OK && !plan->column.is_nullable) {
        rc = mylite_dynamic_string_append(&string, " NOT NULL");
    }
    if (rc == MYLITE_OK) {
        rc = append_alter_table_add_column_default(database, &string, plan);
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

int build_alter_table_add_primary_key_null_validation_sql(
    const struct planned_alter_table_add_primary_key *plan,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT 1 FROM ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " WHERE ");
    }
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < plan->part_count; ++part_index) {
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(&string, " OR ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(
                &string,
                plan->parts[part_index].column.name
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(&string, " IS NULL");
        }
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " LIMIT 1");
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

int build_alter_table_add_primary_key_duplicate_validation_sql(
    const struct planned_alter_table_add_primary_key *plan,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT ");
    if (rc == MYLITE_OK) {
        rc = append_alter_table_primary_key_expression_list_sql(&string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " GROUP BY ");
    }
    if (rc == MYLITE_OK) {
        rc = append_alter_table_primary_key_expression_list_sql(&string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " HAVING COUNT(*) > 1 ORDER BY ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, "MIN(");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, plan->rowid_alias);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(&string, ')');
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " LIMIT 1");
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

int build_alter_table_add_primary_key_string_validation_sql(
    const struct planned_alter_table_add_primary_key *plan,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    bool has_condition = false;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT ");
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < plan->part_count; ++part_index) {
        const struct loaded_index_part *part = &plan->parts[part_index];

        if (!column_descriptor_is_char_or_varchar(&part->column)) {
            continue;
        }
        if (has_condition) {
            rc = mylite_dynamic_string_append(&string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(&string, part->column.name);
        }
        has_condition = true;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " WHERE ");
    }
    has_condition = false;
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < plan->part_count; ++part_index) {
        const struct loaded_index_part *part = &plan->parts[part_index];

        if (!column_descriptor_is_char_or_varchar(&part->column)) {
            continue;
        }
        if (has_condition) {
            rc = mylite_dynamic_string_append(&string, " OR ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(&string, part->column.name);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(&string, " IS NOT NULL");
        }
        has_condition = true;
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

int build_alter_table_add_primary_key_index_sql(
    const struct planned_alter_table_add_primary_key *plan,
    const char *index_physical_name,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "CREATE UNIQUE INDEX ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, index_physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " ON ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " (");
    }
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < plan->part_count; ++part_index) {
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(&string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_loaded_key_part_sql(&string, &plan->parts[part_index], NULL);
        }
        if (rc == MYLITE_OK && plan->parts[part_index].index_column.sort_direction ==
                                   MYLITE_CATALOG_INDEX_SORT_DIRECTION_DESC) {
            rc = mylite_dynamic_string_append(&string, " DESC");
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

int build_add_index_sql(
    const struct planned_alter_table_add_index *plan,
    const char *index_physical_name,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "CREATE ");
    if (rc == MYLITE_OK && plan->is_unique) {
        rc = mylite_dynamic_string_append(&string, "UNIQUE ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, "INDEX ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, index_physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " ON ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " (");
    }
    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < plan->part_count; ++part_index) {
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(&string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_loaded_key_part_sql(&string, &plan->parts[part_index], NULL);
        }
        if (rc == MYLITE_OK && plan->parts[part_index].index_column.sort_direction ==
                                   MYLITE_CATALOG_INDEX_SORT_DIRECTION_DESC) {
            rc = mylite_dynamic_string_append(&string, " DESC");
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

int build_create_unique_index_string_validation_sql(
    const struct planned_alter_table_add_index *plan,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT ");
    if (rc == MYLITE_OK) {
        rc = append_create_unique_index_string_validation_select_list_sql(&string, plan);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " WHERE ");
    }
    if (rc == MYLITE_OK) {
        rc = append_create_unique_index_string_validation_where_sql(&string, plan);
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

static int append_create_unique_index_string_validation_select_list_sql(
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_index *plan
) {
    bool has_string_part = false;
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < plan->part_count; ++part_index) {
        const struct loaded_index_part *part = &plan->parts[part_index];

        if (!loaded_index_part_requires_string_key_validation(part)) {
            continue;
        }
        if (has_string_part) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(string, part->column.name);
        }
        has_string_part = true;
    }

    if (rc == MYLITE_OK && !has_string_part) {
        return MYLITE_ERROR;
    }
    return rc;
}

static int append_create_unique_index_string_validation_where_sql(
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_index *plan
) {
    bool has_string_part = false;
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < plan->part_count; ++part_index) {
        const struct loaded_index_part *part = &plan->parts[part_index];

        if (!loaded_index_part_requires_string_key_validation(part)) {
            continue;
        }
        if (has_string_part) {
            rc = mylite_dynamic_string_append(string, " OR ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(string, part->column.name);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " IS NOT NULL");
        }
        has_string_part = true;
    }

    if (rc == MYLITE_OK && !has_string_part) {
        return MYLITE_ERROR;
    }
    return rc;
}

int build_create_unique_index_duplicate_validation_sql(
    const struct planned_alter_table_add_index *plan,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT ");
    if (rc == MYLITE_OK) {
        rc = append_loaded_key_part_list_sql(&string, plan->parts, plan->part_count);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " WHERE ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, plan->rowid_alias);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " = (SELECT MIN(");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, plan->rowid_alias);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, ") FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " WHERE ");
    }
    if (rc == MYLITE_OK) {
        rc = append_loaded_key_part_not_null_filter_sql(&string, plan->parts, plan->part_count);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " GROUP BY ");
    }
    if (rc == MYLITE_OK) {
        rc = append_loaded_key_part_list_sql(&string, plan->parts, plan->part_count);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " HAVING COUNT(*) > 1 ORDER BY MIN(");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, plan->rowid_alias);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, ") LIMIT 1)");
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

static int append_loaded_key_part_list_sql(
    struct mylite_dynamic_string *string,
    const struct loaded_index_part *parts,
    size_t part_count
) {
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < part_count; ++part_index) {
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_loaded_key_part_sql(string, &parts[part_index], NULL);
        }
    }

    return rc;
}

static int append_loaded_key_part_not_null_filter_sql(
    struct mylite_dynamic_string *string,
    const struct loaded_index_part *parts,
    size_t part_count
) {
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < part_count; ++part_index) {
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(string, " AND ");
        }
        if (rc == MYLITE_OK) {
            rc = append_loaded_key_part_sql(string, &parts[part_index], NULL);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " IS NOT NULL");
        }
    }

    return rc;
}

static int append_alter_table_primary_key_expression_list_sql(
    struct mylite_dynamic_string *string,
    const struct planned_alter_table_add_primary_key *plan
) {
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < plan->part_count; ++part_index) {
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = append_loaded_key_part_sql(string, &plan->parts[part_index], NULL);
        }
    }

    return rc;
}

int build_alter_table_auto_increment_max_sql(
    const struct planned_alter_table_auto_increment *plan,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);

    rc = mylite_dynamic_string_append(&string, "SELECT MAX(");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->column.name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, ") FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->table.physical_name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " WHERE ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, plan->column.name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " IS NOT NULL");
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
