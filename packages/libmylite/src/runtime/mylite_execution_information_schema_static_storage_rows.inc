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
    if (rows->definition->column_count != information_schema_tablespaces_extensions_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.TABLESPACES_EXTENSIONS columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_information_schema_files_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != information_schema_files_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.FILES columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_information_schema_innodb_datafiles_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != information_schema_innodb_datafiles_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_DATAFILES columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_information_schema_innodb_tablespaces_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count != information_schema_innodb_tablespaces_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_TABLESPACES columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int append_information_schema_innodb_session_temp_tablespaces_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    if (rows->definition->column_count !=
        information_schema_innodb_session_temp_tablespaces_column_count) {
        set_runtime_error(
            database,
            "invalid INFORMATION_SCHEMA.INNODB_SESSION_TEMP_TABLESPACES columns"
        );
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
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
    if (rows->definition->column_count !=
        information_schema_innodb_tablespaces_brief_column_count) {
        set_runtime_error(database, "invalid INFORMATION_SCHEMA.INNODB_TABLESPACES_BRIEF columns");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}
