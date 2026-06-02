static int append_information_schema_innodb_foreign_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_foreign_key_info *foreign_keys = NULL;
    size_t column_count = 0U;
    size_t foreign_key_count = 0U;
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_innodb_foreign_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_FOREIGN columns");
        return MYLITE_ERROR;
    }

    rc = mylite_execution_load_table_columns(database, table->table_id, &columns, &column_count);
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
    for (size_t index = 0U; rc == MYLITE_OK && index < foreign_key_count; ++index) {
        rc = append_information_schema_innodb_foreign_foreign_key_row(
            database,
            rows,
            schema,
            table,
            &foreign_keys[index]
        );
    }

    mylite_execution_loaded_foreign_key_infos_deinit(&foreign_keys, &foreign_key_count);
    free(columns);
    return rc;
}

static int append_information_schema_innodb_foreign_foreign_key_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    const struct loaded_foreign_key_info *foreign_key
) {
    struct mylite_catalog_schema_descriptor parent_schema = {0};
    const struct mylite_catalog_schema_descriptor *reference_schema = schema;
    char *foreign_key_id = NULL;
    char *foreign_table_name = NULL;
    char *referenced_table_name = NULL;
    char column_count_text[integer_text_capacity];
    char type_text[integer_text_capacity];
    int64_t type = 0;
    int rc = MYLITE_OK;

    if (foreign_key->part_count > (size_t)INT64_MAX) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_FOREIGN column count");
        return MYLITE_ERROR;
    }

    if (foreign_key->parent_table.schema_id != schema->schema_id) {
        rc = mylite_catalog_read_schema_by_id(
            database,
            foreign_key->parent_table.schema_id,
            &parent_schema
        );
        if (rc == MYLITE_OK) {
            reference_schema = &parent_schema;
        }
    }
    if (rc == MYLITE_OK) {
        rc = copy_information_schema_innodb_foreign_id(
            database,
            schema,
            foreign_key,
            &foreign_key_id
        );
    }
    if (rc == MYLITE_OK) {
        rc = copy_information_schema_innodb_foreign_table_name(
            database,
            schema,
            table,
            &foreign_table_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = copy_information_schema_innodb_foreign_table_name(
            database,
            reference_schema,
            &foreign_key->parent_table,
            &referenced_table_name
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(
            database,
            (int64_t)foreign_key->part_count,
            column_count_text,
            sizeof(column_count_text)
        );
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_innodb_foreign_type(database, &foreign_key->foreign_key, &type);
    }
    if (rc == MYLITE_OK) {
        rc = information_schema_format_i64(database, type, type_text, sizeof(type_text));
    }
    if (rc == MYLITE_OK) {
        const char *values[information_schema_innodb_foreign_column_count] = {
            foreign_key_id,
            foreign_table_name,
            referenced_table_name,
            column_count_text,
            type_text,
        };

        rc = append_information_schema_row(database, rows, values);
    }

    free(referenced_table_name);
    free(foreign_table_name);
    free(foreign_key_id);
    return rc;
}

static int append_information_schema_innodb_foreign_cols_base_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_foreign_key_info *foreign_keys = NULL;
    size_t column_count = 0U;
    size_t foreign_key_count = 0U;
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_innodb_foreign_cols_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_FOREIGN_COLS columns");
        return MYLITE_ERROR;
    }

    rc = mylite_execution_load_table_columns(database, table->table_id, &columns, &column_count);
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
    for (size_t index = 0U; rc == MYLITE_OK && index < foreign_key_count; ++index) {
        rc = append_information_schema_innodb_foreign_cols_foreign_key_rows(
            database,
            rows,
            schema,
            &foreign_keys[index]
        );
    }

    mylite_execution_loaded_foreign_key_infos_deinit(&foreign_keys, &foreign_key_count);
    free(columns);
    return rc;
}

static int append_information_schema_innodb_foreign_cols_foreign_key_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct loaded_foreign_key_info *foreign_key
) {
    char *foreign_key_id = NULL;
    int rc =
        copy_information_schema_innodb_foreign_id(database, schema, foreign_key, &foreign_key_id);

    for (size_t index = 0U; rc == MYLITE_OK && index < foreign_key->part_count; ++index) {
        rc = append_information_schema_innodb_foreign_cols_part_row(
            database,
            rows,
            foreign_key_id,
            &foreign_key->parts[index]
        );
    }

    free(foreign_key_id);
    return rc;
}

static int append_information_schema_innodb_foreign_cols_part_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const char *foreign_key_id,
    const struct loaded_foreign_key_part *part
) {
    char position_text[integer_text_capacity];
    int rc = information_schema_format_i64(
        database,
        part->foreign_key_column.ordinal_position,
        position_text,
        sizeof(position_text)
    );

    if (rc == MYLITE_OK) {
        const char *values[information_schema_innodb_foreign_cols_column_count] = {
            foreign_key_id,
            part->child_column.name,
            part->parent_column.name,
            position_text,
        };

        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}

static int copy_information_schema_innodb_foreign_id(
    struct mylite_db *database,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct loaded_foreign_key_info *foreign_key,
    char **out_id
) {
    return copy_information_schema_schema_object_name(
        database,
        schema->name,
        foreign_key->foreign_key.name,
        out_id
    );
}

static int copy_information_schema_innodb_foreign_table_name(
    struct mylite_db *database,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    char **out_name
) {
    return copy_information_schema_schema_object_name(
        database,
        schema->name,
        table->name,
        out_name
    );
}

static int copy_information_schema_schema_object_name(
    struct mylite_db *database,
    const char *schema_name,
    const char *object_name,
    char **out_name
) {
    size_t schema_length = strlen(schema_name);
    size_t object_length = strlen(object_name);
    size_t required_length = 0U;
    char *name = NULL;

    *out_name = NULL;
    if (schema_length > SIZE_MAX - object_length - tablespace_name_separator_size -
                            tablespace_name_terminator_size) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    required_length = schema_length + object_length + tablespace_name_separator_size +
                      tablespace_name_terminator_size;
    name = (char *)malloc(required_length);
    if (name == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    memcpy(name, schema_name, schema_length);
    name[schema_length] = '/';
    memcpy(&name[schema_length + tablespace_name_separator_size], object_name, object_length);
    name[required_length - tablespace_name_terminator_size] = '\0';
    *out_name = name;
    return MYLITE_OK;
}

static int information_schema_innodb_foreign_type(
    struct mylite_db *database,
    const struct mylite_catalog_foreign_key_descriptor *foreign_key,
    int64_t *out_type
) {
    static const struct information_schema_innodb_foreign_action_type_flags delete_flags = {
        .cascade = information_schema_innodb_foreign_delete_cascade_type,
        .set_null = information_schema_innodb_foreign_delete_set_null_type,
        .no_action = information_schema_innodb_foreign_delete_no_action_type,
    };
    static const struct information_schema_innodb_foreign_action_type_flags update_flags = {
        .cascade = information_schema_innodb_foreign_update_cascade_type,
        .set_null = information_schema_innodb_foreign_update_set_null_type,
        .no_action = information_schema_innodb_foreign_update_no_action_type,
    };
    int64_t delete_type = 0;
    int64_t update_type = 0;
    int rc = information_schema_innodb_foreign_action_type(
        database,
        foreign_key->delete_rule,
        &delete_flags,
        &delete_type
    );

    if (rc == MYLITE_OK) {
        rc = information_schema_innodb_foreign_action_type(
            database,
            foreign_key->update_rule,
            &update_flags,
            &update_type
        );
    }
    if (rc == MYLITE_OK) {
        *out_type = delete_type | update_type;
    }
    return rc;
}

static int information_schema_innodb_foreign_action_type(
    struct mylite_db *database,
    const char *rule,
    const struct information_schema_innodb_foreign_action_type_flags *flags,
    int64_t *out_type
) {
    if (foreign_key_rule_is_cascade(rule)) {
        *out_type = flags->cascade;
        return MYLITE_OK;
    }
    if (foreign_key_rule_is_set_null(rule)) {
        *out_type = flags->set_null;
        return MYLITE_OK;
    }
    if (foreign_key_rule_equals(rule, "NO ACTION")) {
        *out_type = flags->no_action;
        return MYLITE_OK;
    }
    if (foreign_key_rule_equals(rule, "RESTRICT")) {
        *out_type = 0;
        return MYLITE_OK;
    }

    set_runtime_error(database, "unknown INFORMATION_SCHEMA.INNODB_FOREIGN action");
    return MYLITE_ERROR;
}
