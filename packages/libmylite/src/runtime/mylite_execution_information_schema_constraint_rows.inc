static int append_information_schema_table_constraints_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_index_info *indexes = NULL;
    struct loaded_foreign_key_info *foreign_keys = NULL;
    struct loaded_check_constraint_info *check_constraints = NULL;
    size_t column_count = 0U;
    size_t index_count = 0U;
    size_t foreign_key_count = 0U;
    size_t check_constraint_count = 0U;
    int rc = mylite_execution_load_table_columns(database, table->table_id, &columns, &column_count);

    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_index_infos(
            database,
            table->table_id,
            columns,
            column_count,
            &indexes,
            &index_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_foreign_key_infos(
            database,
            table->table_id,
            columns,
            column_count,
            &foreign_keys,
            &foreign_key_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_check_constraint_infos(
            database,
            table->table_id,
            &check_constraints,
            &check_constraint_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_information_schema_table_constraints_index_rows(
            database,
            rows,
            schema,
            table,
            indexes,
            index_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_information_schema_table_constraints_foreign_key_rows(
            database,
            rows,
            schema,
            table,
            foreign_keys,
            foreign_key_count
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < check_constraint_count; ++index) {
        rc = append_information_schema_table_constraints_check_row(
            database,
            rows,
            schema,
            table,
            &check_constraints[index]
        );
    }

    mylite_execution_loaded_check_constraint_infos_deinit(&check_constraints, &check_constraint_count);
    mylite_execution_loaded_foreign_key_infos_deinit(&foreign_keys, &foreign_key_count);
    mylite_execution_loaded_index_infos_deinit(&indexes, &index_count);
    free(columns);
    return rc;
}

static int append_information_schema_table_constraints_index_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_index_info *indexes,
    size_t index_count
) {
    bool *emitted = NULL;
    int rc = MYLITE_OK;

    if (index_count == 0U) {
        return MYLITE_OK;
    }
    if (index_count > SIZE_MAX / sizeof(*emitted)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    emitted = calloc(index_count, sizeof(*emitted));
    if (emitted == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    while (rc == MYLITE_OK) {
        size_t best_index = SIZE_MAX;

        for (size_t index = 0U; index < index_count; ++index) {
            if (emitted[index] || !indexes[index].index.is_unique) {
                continue;
            }
            if (best_index == SIZE_MAX ||
                compare_ascii_case_insensitive_text(
                    indexes[index].index.name,
                    indexes[best_index].index.name
                ) < 0) {
                best_index = index;
            }
        }
        if (best_index == SIZE_MAX) {
            break;
        }

        emitted[best_index] = true;
        rc = append_information_schema_table_constraints_index_row(
            database,
            rows,
            schema,
            table,
            &indexes[best_index]
        );
    }

    free(emitted);
    return rc;
}

static int append_information_schema_table_constraints_index_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_index_info *index
) {
    const char *constraint_type =
        index->index.kind == MYLITE_CATALOG_INDEX_KIND_PRIMARY ? "PRIMARY KEY" : "UNIQUE";
    const char *values[information_schema_table_constraints_column_count] = {
        "def",
        schema->name,
        index->index.name,
        schema->name,
        table->name,
        constraint_type,
        "YES",
    };

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_table_constraints_foreign_key_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_foreign_key_info *foreign_keys,
    size_t foreign_key_count
) {
    bool *emitted = NULL;
    int rc = MYLITE_OK;

    if (foreign_key_count == 0U) {
        return MYLITE_OK;
    }
    if (foreign_key_count > SIZE_MAX / sizeof(*emitted)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    emitted = calloc(foreign_key_count, sizeof(*emitted));
    if (emitted == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    while (rc == MYLITE_OK) {
        size_t best_index = SIZE_MAX;

        for (size_t index = 0U; index < foreign_key_count; ++index) {
            if (emitted[index]) {
                continue;
            }
            if (best_index == SIZE_MAX ||
                compare_ascii_case_insensitive_text(
                    foreign_keys[index].foreign_key.name,
                    foreign_keys[best_index].foreign_key.name
                ) < 0) {
                best_index = index;
            }
        }
        if (best_index == SIZE_MAX) {
            break;
        }

        emitted[best_index] = true;
        rc = append_information_schema_table_constraints_foreign_key_row(
            database,
            rows,
            schema,
            table,
            &foreign_keys[best_index]
        );
    }

    free(emitted);
    return rc;
}

static int append_information_schema_table_constraints_foreign_key_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_foreign_key_info *foreign_key
) {
    const char *values[information_schema_table_constraints_column_count] = {
        "def",
        schema->name,
        foreign_key->foreign_key.name,
        schema->name,
        table->name,
        "FOREIGN KEY",
        "YES",
    };

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_table_constraints_check_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_check_constraint_info *check_constraint
) {
    const char *enforced = "NO";

    if (check_constraint->check_constraint.is_enforced) {
        enforced = "YES";
    }

    const char *values[information_schema_table_constraints_column_count] = {
        "def",
        schema->name,
        check_constraint->check_constraint.name,
        schema->name,
        table->name,
        "CHECK",
        enforced,
    };

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_table_constraints_mysql_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_mysql_system_table_definition_count();
         ++index) {
        rc = append_information_schema_table_constraints_mysql_system_table_row(
            database,
            rows,
            mylite_execution_catalog_mysql_system_table_definition_at(index)
        );
    }

    return rc;
}

static int append_information_schema_table_constraints_mysql_system_table_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_execution_catalog_mysql_system_table *definition
) {
    int rc = MYLITE_OK;

    if (definition == NULL || definition->column_keys == NULL) {
        return MYLITE_OK;
    }
    if (mysql_system_table_primary_key_column_count(definition) > 0U) {
        const char *values[information_schema_table_constraints_column_count] = {
            "def",
            definition->schema_name,
            "PRIMARY",
            definition->schema_name,
            definition->query_definition.name,
            "PRIMARY KEY",
            "YES",
        };

        rc = append_information_schema_row(database, rows, values);
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < definition->secondary_index_count; ++index) {
        const struct mylite_execution_catalog_mysql_system_secondary_index *secondary_index =
            &definition->secondary_indexes[index];

        if (!secondary_index->is_unique) {
            continue;
        }
        const char *values[information_schema_table_constraints_column_count] = {
            "def",
            definition->schema_name,
            secondary_index->name,
            definition->schema_name,
            definition->query_definition.name,
            "UNIQUE",
            "YES",
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_table_constraints_extensions_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_index_info *indexes = NULL;
    struct loaded_foreign_key_info *foreign_keys = NULL;
    size_t column_count = 0U;
    size_t index_count = 0U;
    size_t foreign_key_count = 0U;
    int rc = mylite_execution_load_table_columns(database, table->table_id, &columns, &column_count);

    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_index_infos(
            database,
            table->table_id,
            columns,
            column_count,
            &indexes,
            &index_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_foreign_key_infos(
            database,
            table->table_id,
            columns,
            column_count,
            &foreign_keys,
            &foreign_key_count
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < index_count; ++index) {
        if (!indexes[index].index.is_unique) {
            continue;
        }
        rc = append_information_schema_table_constraints_extensions_index_row(
            database,
            rows,
            schema,
            table,
            &indexes[index]
        );
    }
    if (rc == MYLITE_OK) {
        rc = append_information_schema_table_constraints_extensions_foreign_key_rows(
            database,
            rows,
            schema,
            table,
            foreign_keys,
            foreign_key_count
        );
    }

    mylite_execution_loaded_foreign_key_infos_deinit(&foreign_keys, &foreign_key_count);
    mylite_execution_loaded_index_infos_deinit(&indexes, &index_count);
    free(columns);
    return rc;
}

static int append_information_schema_table_constraints_extensions_index_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_index_info *index
) {
    const char *values[information_schema_table_constraints_extensions_column_count] = {
        "def",
        schema->name,
        index->index.name,
        table->name,
        NULL,
        NULL,
    };

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_table_constraints_extensions_foreign_key_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_foreign_key_info *foreign_keys,
    size_t foreign_key_count
) {
    bool *emitted = NULL;
    int rc = MYLITE_OK;

    if (foreign_key_count == 0U) {
        return MYLITE_OK;
    }
    if (foreign_key_count > SIZE_MAX / sizeof(*emitted)) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    emitted = calloc(foreign_key_count, sizeof(*emitted));
    if (emitted == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    while (rc == MYLITE_OK) {
        size_t best_index = SIZE_MAX;

        for (size_t index = 0U; index < foreign_key_count; ++index) {
            if (emitted[index]) {
                continue;
            }
            if (best_index == SIZE_MAX ||
                compare_ascii_case_insensitive_text(
                    foreign_keys[index].foreign_key.name,
                    foreign_keys[best_index].foreign_key.name
                ) < 0) {
                best_index = index;
            }
        }
        if (best_index == SIZE_MAX) {
            break;
        }

        emitted[best_index] = true;
        rc = append_information_schema_table_constraints_extensions_foreign_key_row(
            database,
            rows,
            schema,
            table,
            &foreign_keys[best_index]
        );
    }

    free(emitted);
    return rc;
}

static int append_information_schema_table_constraints_extensions_foreign_key_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_foreign_key_info *foreign_key
) {
    const char *values[information_schema_table_constraints_extensions_column_count] = {
        "def",
        schema->name,
        foreign_key->foreign_key.name,
        table->name,
        NULL,
        NULL,
    };

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_table_constraints_extensions_mysql_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_mysql_system_table_definition_count();
         ++index) {
        rc = append_information_schema_table_constraints_extensions_mysql_system_table_row(
            database,
            rows,
            mylite_execution_catalog_mysql_system_table_definition_at(index)
        );
    }

    return rc;
}

static int append_information_schema_table_constraints_extensions_mysql_system_table_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_execution_catalog_mysql_system_table *definition
) {
    int rc = MYLITE_OK;

    if (definition == NULL || definition->column_keys == NULL) {
        return MYLITE_OK;
    }
    if (mysql_system_table_primary_key_column_count(definition) > 0U) {
        const char *values[information_schema_table_constraints_extensions_column_count] = {
            "def",
            definition->schema_name,
            "PRIMARY",
            definition->query_definition.name,
            NULL,
            NULL,
        };

        rc = append_information_schema_row(database, rows, values);
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < definition->secondary_index_count; ++index) {
        const struct mylite_execution_catalog_mysql_system_secondary_index *secondary_index =
            &definition->secondary_indexes[index];
        const char *values[information_schema_table_constraints_extensions_column_count] = {
            "def",
            definition->schema_name,
            secondary_index->name,
            definition->query_definition.name,
            NULL,
            NULL,
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_check_constraints_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct loaded_check_constraint_info *check_constraints = NULL;
    size_t check_constraint_count = 0U;
    int rc = mylite_execution_load_table_check_constraint_infos(
        database,
        table->table_id,
        &check_constraints,
        &check_constraint_count
    );

    for (size_t index = 0U; rc == MYLITE_OK && index < check_constraint_count; ++index) {
        rc = append_information_schema_check_constraints_check_row(
            database,
            rows,
            schema,
            &check_constraints[index]
        );
    }

    mylite_execution_loaded_check_constraint_infos_deinit(&check_constraints, &check_constraint_count);
    return rc;
}

static int append_information_schema_check_constraints_check_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct loaded_check_constraint_info *check_constraint
) {
    char *check_clause = NULL;
    const char *values[information_schema_check_constraints_column_count];
    int rc = escape_information_schema_check_clause(
        database,
        check_constraint->check_constraint.check_clause,
        &check_clause
    );

    if (rc != MYLITE_OK) {
        return rc;
    }

    values[0] = "def";
    values[1] = schema->name;
    values[2] = check_constraint->check_constraint.name;
    values[3] = check_clause;

    rc = append_information_schema_row(database, rows, values);
    free(check_clause);
    return rc;
}

static int escape_information_schema_check_clause(
    struct mylite_db *database,
    const char *check_clause,
    char **out_check_clause
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    if (out_check_clause == NULL) {
        return MYLITE_MISUSE;
    }
    *out_check_clause = NULL;
    if (check_clause == NULL) {
        return MYLITE_OK;
    }

    mylite_dynamic_string_init(&string);
    for (size_t index = 0U; rc == MYLITE_OK && check_clause[index] != '\0'; ++index) {
        if (check_clause[index] == '\'') {
            rc = mylite_dynamic_string_append(&string, "\\'");
        } else {
            rc = mylite_dynamic_string_append_char(&string, check_clause[index]);
        }
    }
    if (rc == MYLITE_OK && string.text == NULL) {
        rc = mylite_dynamic_string_append(&string, "");
    }
    if (rc == MYLITE_OK) {
        *out_check_clause = mylite_dynamic_string_take(&string);
        if (*out_check_clause == NULL) {
            rc = MYLITE_NOMEM;
        }
    }
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    mylite_dynamic_string_deinit(&string);
    return rc;
}
