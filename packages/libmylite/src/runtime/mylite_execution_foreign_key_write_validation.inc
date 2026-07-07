static int validate_child_foreign_keys_after_write(
    struct mylite_db *database,
    int64_t child_table_id
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_foreign_key_info *foreign_keys = NULL;
    size_t column_count = 0U;
    size_t foreign_key_count = 0U;
    int rc = MYLITE_OK;

    if (child_table_id <= 0) {
        return MYLITE_OK;
    }
    if (!foreign_key_checks_system_variable_value(database, false)) {
        return MYLITE_OK;
    }

    rc = mylite_execution_load_table_columns(database, child_table_id, &columns, &column_count);

    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_foreign_key_infos(
            database,
            child_table_id,
            columns,
            column_count,
            &foreign_keys,
            &foreign_key_count
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < foreign_key_count; ++index) {
        rc = validate_foreign_key_references(database, &foreign_keys[index], false);
    }

    mylite_execution_loaded_foreign_key_infos_deinit(&foreign_keys, &foreign_key_count);
    free(columns);
    return rc;
}

static int validate_parent_foreign_keys_after_write(
    struct mylite_db *database,
    int64_t parent_table_id
) {
    struct validate_parent_foreign_keys_context context = {
        .database = database,
        .rc = MYLITE_OK,
    };
    int rc = MYLITE_OK;

    if (parent_table_id <= 0) {
        return MYLITE_OK;
    }
    if (!foreign_key_checks_system_variable_value(database, false)) {
        return MYLITE_OK;
    }

    rc = mylite_catalog_for_each_foreign_key_for_parent_table(
        database,
        parent_table_id,
        validate_parent_foreign_key_descriptor,
        &context
    );

    if (rc == MYLITE_OK) {
        rc = context.rc;
    }
    return rc;
}

static int validate_parent_foreign_key_descriptor(
    const struct mylite_catalog_foreign_key_descriptor *foreign_key,
    void *user_data
) {
    struct validate_parent_foreign_keys_context *context = user_data;
    struct loaded_foreign_key_info loaded = {0};
    int rc = MYLITE_OK;

    if (foreign_key == NULL || context == NULL || context->database == NULL) {
        return MYLITE_MISUSE;
    }

    rc = mylite_execution_load_foreign_key_info(context->database, foreign_key, NULL, 0U, &loaded);
    if (rc == MYLITE_OK) {
        rc = validate_foreign_key_references(context->database, &loaded, true);
    }
    mylite_execution_loaded_foreign_key_info_deinit(&loaded);
    context->rc = rc;
    return rc;
}

static int validate_foreign_key_references(
    struct mylite_db *database,
    const struct loaded_foreign_key_info *foreign_key,
    bool parent_write
) {
    sqlite3_stmt *statement = NULL;
    char *sql = NULL;
    int sqlite_rc = SQLITE_OK;
    int rc = build_foreign_key_validation_sql(foreign_key, &sql);

    if (rc == MYLITE_OK) {
        rc = prepare_cached_sqlite_statement(database, sql, &statement);
    }
    if (rc == MYLITE_OK) {
        sqlite_rc = sqlite3_step(statement);
        if (sqlite_rc == SQLITE_ROW) {
            if (parent_write) {
                rc = set_row_is_referenced_foreign_key_error(database, foreign_key);
            } else {
                set_no_referenced_row_error(database);
                rc = MYLITE_ERROR;
            }
        } else if (sqlite_rc != SQLITE_DONE) {
            rc = mylite_sqlite_status_to_mylite(sqlite_rc);
        }
    }
    rc = finish_cached_sqlite_statement(database, statement, rc);
    free(sql);

    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    } else if (
        rc != MYLITE_OK &&
        mylite_diagnostics_errcode(mylite_connection_diagnostics(database)) == MYLITE_OK
    ) {
        set_physical_sqlite_row_error(database);
        rc = MYLITE_ERROR;
    }

    return rc;
}

static int set_row_is_referenced_foreign_key_error(
    struct mylite_db *database,
    const struct loaded_foreign_key_info *foreign_key
) {
    struct mylite_catalog_schema_descriptor schema = {0};
    struct mylite_dynamic_string message;
    char *message_text = NULL;
    int rc = mylite_catalog_read_schema_by_id(
        database,
        foreign_key->child_table.schema_id,
        &schema
    );

    if (rc != MYLITE_OK) {
        set_row_is_referenced_error(database);
        return MYLITE_ERROR;
    }

    mylite_dynamic_string_init(&message);
    rc = append_row_is_referenced_foreign_key_detail(&message, &schema, foreign_key);
    if (rc == MYLITE_OK) {
        message_text = mylite_dynamic_string_take(&message);
        if (message_text == NULL) {
            rc = MYLITE_NOMEM;
        }
    }
    mylite_dynamic_string_deinit(&message);

    if (rc != MYLITE_OK) {
        free(message_text);
        return rc;
    }

    mylite_diagnostics_set_error(
        mylite_connection_diagnostics(database),
        mysql_error_row_is_referenced,
        "23000",
        message_text
    );
    free(message_text);
    return MYLITE_ERROR;
}

static int append_row_is_referenced_foreign_key_detail(
    struct mylite_dynamic_string *string,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct loaded_foreign_key_info *foreign_key
) {
    int rc = mylite_dynamic_string_append(
        string,
        "Cannot delete or update a parent row: a foreign key constraint fails ("
    );

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_mysql_quoted_identifier(string, schema->name);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '.');
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_mysql_quoted_identifier(
            string,
            foreign_key->child_table.name
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ", CONSTRAINT ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_mysql_quoted_identifier(
            string,
            foreign_key->foreign_key.name
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " FOREIGN KEY (");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_is_referenced_foreign_key_columns(string, foreign_key, true);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, ") REFERENCES ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_mysql_quoted_identifier(
            string,
            foreign_key->parent_table.name
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(string, " (");
    }
    if (rc == MYLITE_OK) {
        rc = append_row_is_referenced_foreign_key_columns(string, foreign_key, false);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }
    if (rc == MYLITE_OK) {
        rc = append_row_is_referenced_foreign_key_rules(string, foreign_key);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, ')');
    }

    return rc;
}

static int append_row_is_referenced_foreign_key_columns(
    struct mylite_dynamic_string *string,
    const struct loaded_foreign_key_info *foreign_key,
    bool child_columns
) {
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < foreign_key->part_count;
         ++part_index) {
        const char *column_name = foreign_key->parts[part_index].parent_column.name;

        if (child_columns) {
            column_name = foreign_key->parts[part_index].child_column.name;
        }
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_mysql_quoted_identifier(string, column_name);
        }
    }

    return rc;
}

static int append_row_is_referenced_foreign_key_rules(
    struct mylite_dynamic_string *string,
    const struct loaded_foreign_key_info *foreign_key
) {
    int rc = MYLITE_OK;

    if (strcmp(foreign_key->foreign_key.delete_rule, "NO ACTION") != 0) {
        rc = mylite_dynamic_string_append(string, " ON DELETE ");
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, foreign_key->foreign_key.delete_rule);
        }
    }
    if (rc == MYLITE_OK && strcmp(foreign_key->foreign_key.update_rule, "NO ACTION") != 0) {
        rc = mylite_dynamic_string_append(string, " ON UPDATE ");
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, foreign_key->foreign_key.update_rule);
        }
    }

    return rc;
}

static int build_foreign_key_validation_sql(
    const struct loaded_foreign_key_info *foreign_key,
    char **out_sql
) {
    struct mylite_dynamic_string string;
    int rc = MYLITE_OK;

    *out_sql = NULL;
    mylite_dynamic_string_init(&string);
    if (foreign_key == NULL || foreign_key->part_count == 0U) {
        return MYLITE_ERROR;
    }

    rc = mylite_dynamic_string_append(&string, "SELECT 1 FROM ");
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(
            &string,
            foreign_key->child_table.physical_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " AS ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, "c");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " WHERE ");
    }
    if (rc == MYLITE_OK) {
        rc = append_foreign_key_child_not_null_checks(&string, foreign_key);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " AND NOT EXISTS (SELECT 1 FROM ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(
            &string,
            foreign_key->parent_table.physical_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " AS ");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(&string, "p");
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, " WHERE ");
    }
    if (rc == MYLITE_OK) {
        rc = append_foreign_key_parent_match_checks(&string, foreign_key);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append(&string, ") LIMIT 1");
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

static int append_foreign_key_child_not_null_checks(
    struct mylite_dynamic_string *string,
    const struct loaded_foreign_key_info *foreign_key
) {
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < foreign_key->part_count;
         ++part_index) {
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(string, " AND ");
        }
        if (rc == MYLITE_OK) {
            rc = append_foreign_key_column_reference(
                string,
                "c",
                foreign_key->parts[part_index].child_column.name
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " IS NOT NULL");
        }
    }

    return rc;
}

static int append_foreign_key_child_set_null_assignments_sql(
    struct mylite_dynamic_string *string,
    const struct loaded_foreign_key_info *foreign_key
) {
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < foreign_key->part_count;
         ++part_index) {
        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(string, ", ");
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append_quoted_identifier(
                string,
                foreign_key->parts[part_index].child_column.name
            );
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " = NULL");
        }
    }

    return rc;
}

static int append_foreign_key_parent_match_checks(
    struct mylite_dynamic_string *string,
    const struct loaded_foreign_key_info *foreign_key
) {
    int rc = MYLITE_OK;

    for (size_t part_index = 0U; rc == MYLITE_OK && part_index < foreign_key->part_count;
         ++part_index) {
        const struct loaded_foreign_key_part *part = &foreign_key->parts[part_index];

        if (part_index != 0U) {
            rc = mylite_dynamic_string_append(string, " AND ");
        }
        if (rc == MYLITE_OK) {
            rc = append_foreign_key_column_reference(string, "p", part->parent_column.name);
        }
        if (rc == MYLITE_OK) {
            rc = mylite_dynamic_string_append(string, " = ");
        }
        if (rc == MYLITE_OK) {
            rc = append_foreign_key_column_reference(string, "c", part->child_column.name);
        }
    }

    return rc;
}

static int append_foreign_key_column_reference(
    struct mylite_dynamic_string *string,
    const char *table_alias,
    const char *column_name
) {
    int rc = mylite_dynamic_string_append_quoted_identifier(string, table_alias);

    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_char(string, '.');
    }
    if (rc == MYLITE_OK) {
        rc = mylite_dynamic_string_append_quoted_identifier(string, column_name);
    }

    return rc;
}
