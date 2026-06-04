static int append_information_schema_partitions_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t table_index = 0U;
         rc == MYLITE_OK &&
         table_index < mylite_execution_catalog_information_schema_table_definition_count();
         ++table_index) {
        const struct mylite_execution_catalog_table_definition *definition =
            mylite_execution_catalog_information_schema_table_definition_at(table_index);
        const char *values[information_schema_partitions_column_count] = {
            "def",
            "information_schema",
            definition->name,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            "0",
            "0",
            "0",
            "0",
            "0",
            "0",
            NULL,
            NULL,
            NULL,
            NULL,
            "",
            "",
            NULL,
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_tables_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    static const char *const schema_order[] = {
        "mysql",
        "information_schema",
        "performance_schema",
        "sys",
    };
    int rc = MYLITE_OK;

    for (size_t table_index = 0U; rc == MYLITE_OK && table_index < sizeof(schema_order) /
                                                                       sizeof(schema_order[0]);
         ++table_index) {
        const struct mylite_execution_catalog_builtin_schema_table_directory *directory =
            mylite_execution_catalog_builtin_schema_table_directory_by_name(
                schema_order[table_index]
            );

        for (size_t name_index = 0U; rc == MYLITE_OK && name_index < directory->table_count;
             ++name_index) {
            rc = append_information_schema_builtin_table_row(
                database,
                rows,
                directory,
                directory->table_names[name_index]
            );
        }
    }

    return rc;
}

static int append_information_schema_tables_extensions_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    static const char *const schema_order[] = {
        "mysql",
        "information_schema",
        "performance_schema",
        "sys",
    };
    int rc = MYLITE_OK;

    for (size_t table_index = 0U; rc == MYLITE_OK && table_index < sizeof(schema_order) /
                                                                       sizeof(schema_order[0]);
         ++table_index) {
        const struct mylite_execution_catalog_builtin_schema_table_directory *directory =
            mylite_execution_catalog_builtin_schema_table_directory_by_name(
                schema_order[table_index]
            );

        for (size_t name_index = 0U; rc == MYLITE_OK && name_index < directory->table_count;
             ++name_index) {
            rc = append_information_schema_builtin_table_extension_row(
                database,
                rows,
                directory,
                directory->table_names[name_index]
            );
        }
    }

    return rc;
}

static int append_information_schema_builtin_table_extension_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_execution_catalog_builtin_schema_table_directory *directory,
    const char *table_name
) {
    const char *values[information_schema_tables_extensions_column_count] = {
        "def",
        directory->schema_name,
        table_name,
        NULL,
        NULL,
    };

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_tables_extensions_table_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    const char *values[information_schema_tables_extensions_column_count] = {
        "def",
        schema->name,
        table->name,
        NULL,
        NULL,
    };

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_tablespaces_extensions_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_tablespaces_extensions_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.TABLESPACES_EXTENSIONS columns");
        return MYLITE_ERROR;
    }
    for (size_t index = 0U;
         rc == MYLITE_OK &&
         index < mylite_execution_catalog_builtin_tablespace_extension_name_count();
         ++index) {
        const char *values[information_schema_tablespaces_extensions_column_count] = {
            mylite_execution_catalog_builtin_tablespace_extension_name_at(index),
            NULL,
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_tablespaces_extensions_table_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table
) {
    char *tablespace_name = NULL;
    const char *values[information_schema_tablespaces_extensions_column_count] = {
        NULL,
        NULL,
    };
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_tablespaces_extensions_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.TABLESPACES_EXTENSIONS columns");
        return MYLITE_ERROR;
    }
    rc = copy_information_schema_tablespace_name(database, schema, table, &tablespace_name);
    if (rc == MYLITE_OK) {
        values[0] = tablespace_name;
        rc = append_information_schema_row(database, rows, values);
    }

    free(tablespace_name);
    return rc;
}

static int append_information_schema_files_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_files_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.FILES columns");
        return MYLITE_ERROR;
    }
    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_information_schema_files_row_count();
         ++index) {
        const struct mylite_execution_catalog_files_row *file =
            mylite_execution_catalog_information_schema_files_row_at(index);
        const char *values[information_schema_files_column_count] = {
            file->file_id,
            file->file_name,
            file->file_type,
            file->tablespace_name,
            "",
            NULL,
            NULL,
            NULL,
            NULL,
            "InnoDB",
            NULL,
            NULL,
            NULL,
            file->free_extents,
            file->total_extents,
            "1048576",
            file->initial_size,
            NULL,
            file->autoextend_size,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            file->data_free,
            NULL,
            NULL,
            NULL,
            NULL,
            "NORMAL",
            NULL,
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_innodb_datafiles_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_innodb_datafiles_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_DATAFILES columns");
        return MYLITE_ERROR;
    }
    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_innodb_tablespace_row_count();
         ++index) {
        const struct mylite_execution_catalog_innodb_tablespace_row *tablespace =
            mylite_execution_catalog_innodb_tablespace_row_at(index);
        const char *values[information_schema_innodb_datafiles_column_count] = {
            tablespace->space,
            tablespace->path,
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_innodb_tablespaces_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_innodb_tablespaces_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_TABLESPACES columns");
        return MYLITE_ERROR;
    }
    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_innodb_tablespace_full_row_count();
         ++index) {
        const struct mylite_execution_catalog_innodb_tablespace_full_row *tablespace =
            mylite_execution_catalog_innodb_tablespace_full_row_at(index);
        const char *values[information_schema_innodb_tablespaces_column_count] = {
            tablespace->space,
            tablespace->name,
            tablespace->flag,
            tablespace->row_format,
            tablespace->page_size,
            tablespace->zip_page_size,
            tablespace->space_type,
            tablespace->fs_block_size,
            tablespace->file_size,
            tablespace->allocated_size,
            tablespace->autoextend_size,
            tablespace->server_version,
            tablespace->space_version,
            tablespace->encryption,
            tablespace->state,
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_innodb_session_temp_tablespaces_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    char connection_id_text[integer_text_capacity];
    int rc = MYLITE_OK;

    if (rows->definition->column_count !=
        information_schema_innodb_session_temp_tablespaces_column_count) {
        set_runtime_error(
            database,
            "invalid INFORMATION_SCHEMA.INNODB_SESSION_TEMP_TABLESPACES columns"
        );
        return MYLITE_ERROR;
    }
    rc = format_uint64(
        database,
        database->session.connection_id,
        connection_id_text,
        sizeof(connection_id_text)
    );
    for (size_t index = 0U;
         rc == MYLITE_OK &&
         index < mylite_execution_catalog_innodb_session_temp_tablespace_row_count();
         ++index) {
        const struct mylite_execution_catalog_innodb_session_temp_tablespace_row *tablespace =
            mylite_execution_catalog_innodb_session_temp_tablespace_row_at(index);
        const char *values[information_schema_innodb_session_temp_tablespaces_column_count] = {
            strcmp(tablespace->state, "ACTIVE") == 0 ? connection_id_text : "0",
            tablespace->space,
            tablespace->path,
            "81920",
            tablespace->state,
            tablespace->purpose,
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_innodb_cmp_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_innodb_cmp_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_CMP columns");
        return MYLITE_ERROR;
    }
    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_innodb_compressed_page_size_count();
         ++index) {
        const char *values[information_schema_innodb_cmp_column_count] = {
            mylite_execution_catalog_innodb_compressed_page_size_at(index),
            "0",
            "0",
            "0",
            "0",
            "0",
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_innodb_buffer_pool_stats_system_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    const char *values[information_schema_innodb_buffer_pool_stats_column_count] = {
        "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0",
        "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0", "0",
    };

    if (rows->definition->column_count !=
        information_schema_innodb_buffer_pool_stats_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_BUFFER_POOL_STATS columns");
        return MYLITE_ERROR;
    }
    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_innodb_cmpmem_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    if (rows->definition->column_count != information_schema_innodb_cmpmem_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_CMPMEM columns");
        return MYLITE_ERROR;
    }
    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_innodb_compressed_page_size_count();
         ++index) {
        const char *values[information_schema_innodb_cmpmem_column_count] = {
            mylite_execution_catalog_innodb_compressed_page_size_at(index),
            "0",
            "0",
            "0",
            "0",
            "0",
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_innodb_ft_default_stopword_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    if (rows->definition->column_count !=
        information_schema_innodb_ft_default_stopword_column_count) {
        set_runtime_error(
            database,
            "invalid INFORMATION_SCHEMA.INNODB_FT_DEFAULT_STOPWORD columns"
        );
        return MYLITE_ERROR;
    }
    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_innodb_ft_default_stopword_count();
         ++index) {
        const char *values[information_schema_innodb_ft_default_stopword_column_count] = {
            mylite_execution_catalog_innodb_ft_default_stopword_at(index),
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_innodb_tablespaces_brief_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    if (rows->definition->column_count !=
        information_schema_innodb_tablespaces_brief_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_TABLESPACES_BRIEF columns");
        return MYLITE_ERROR;
    }
    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_innodb_tablespace_row_count();
         ++index) {
        const struct mylite_execution_catalog_innodb_tablespace_row *tablespace =
            mylite_execution_catalog_innodb_tablespace_row_at(index);
        const char *values[information_schema_innodb_tablespaces_brief_column_count] = {
            tablespace->space,
            tablespace->name,
            tablespace->path,
            tablespace->flag,
            tablespace->space_type,
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int copy_information_schema_tablespace_name(
    struct mylite_db *database,
    const struct mylite_catalog_schema_descriptor *schema,
    const struct mylite_catalog_table_descriptor *table,
    char **out_name
) {
    size_t schema_length = strlen(schema->name);
    size_t table_length = strlen(table->name);
    size_t required_length = 0U;
    char *name = NULL;

    *out_name = NULL;
    if (schema_length > SIZE_MAX - table_length - tablespace_name_separator_size -
                            tablespace_name_terminator_size) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    required_length = schema_length + table_length + tablespace_name_separator_size +
                      tablespace_name_terminator_size;
    name = (char *)malloc(required_length);
    if (name == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    memcpy(name, schema->name, schema_length);
    name[schema_length] = '/';
    memcpy(&name[schema_length + tablespace_name_separator_size], table->name, table_length);
    name[required_length - tablespace_name_terminator_size] = '\0';
    *out_name = name;
    return MYLITE_OK;
}
