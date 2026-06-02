static int append_information_schema_schemata_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_builtin_schema_count();
         ++index) {
        const struct mylite_execution_catalog_builtin_schema *schema =
            mylite_execution_catalog_builtin_schema_at(index);
        const char *values[information_schema_schemata_column_count] = {
            "def",
            schema->name,
            schema->default_charset,
            schema->default_collation,
            NULL,
            "NO",
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_schemata_schema_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema
) {
    const char *values[information_schema_schemata_column_count] = {
        "def",
        schema->name,
        schema->default_charset,
        schema->default_collation,
        NULL,
        "NO",
    };

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_schemata_extensions_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_builtin_schema_count();
         ++index) {
        const struct mylite_execution_catalog_builtin_schema *schema =
            mylite_execution_catalog_builtin_schema_at(index);
        const char *values[information_schema_schemata_extensions_column_count] = {
            "def",
            schema->name,
            "",
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_schemata_extensions_schema_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows,
    const struct mylite_catalog_schema_descriptor *schema
) {
    const char *values[information_schema_schemata_extensions_column_count] = {
        "def",
        schema->name,
        "",
    };

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_character_sets_system_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t row_index = 0U;
         rc == MYLITE_OK && row_index < mylite_execution_catalog_mysql_character_set_count();
         ++row_index) {
        const struct mylite_execution_catalog_character_set *charset =
            mylite_execution_catalog_mysql_character_set_at(row_index);
        const char *values[information_schema_character_sets_column_count] = {
            charset->name,
            charset->default_collation,
            charset->description,
            charset->maxlen,
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_collations_system_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t row_index = 0U;
         rc == MYLITE_OK && row_index < mylite_execution_catalog_mysql_collation_count();
         ++row_index) {
        const struct mylite_execution_catalog_collation *collation =
            mylite_execution_catalog_mysql_collation_at(row_index);
        const char *values[information_schema_collations_column_count] = {
            collation->name,
            collation->character_set,
            collation->id,
            collation->is_default,
            collation->compiled,
            collation->sortlen,
            collation->pad_attribute,
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_collation_applicability_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t row_index = 0U;
         rc == MYLITE_OK && row_index < mylite_execution_catalog_mysql_collation_count();
         ++row_index) {
        const struct mylite_execution_catalog_collation *collation =
            mylite_execution_catalog_mysql_collation_at(row_index);
        const char *values[information_schema_collation_applicability_column_count] = {
            collation->name,
            collation->character_set,
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_engines_system_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    const char *values[information_schema_engines_column_count] = {
        "InnoDB",
        "DEFAULT",
        "Supports transactions, row-level locking, and foreign keys",
        "YES",
        "YES",
        "YES",
    };

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_plugins_system_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    const char *values[information_schema_plugins_column_count] = {
        "InnoDB",
        "8.4",
        "ACTIVE",
        "STORAGE ENGINE",
        "80409.0",
        NULL,
        NULL,
        "Oracle Corporation",
        "Supports transactions, row-level locking, and foreign keys",
        "GPL",
        "FORCE",
    };

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_keywords_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t row_index = 0U;
         rc == MYLITE_OK && row_index < mylite_execution_catalog_keyword_count();
         ++row_index) {
        const struct mylite_execution_catalog_keyword *keyword =
            mylite_execution_catalog_keyword_at(row_index);
        const char *values[information_schema_keywords_column_count] = {
            keyword->word,
            keyword->reserved,
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_user_privileges_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t row_index = 0U;
         rc == MYLITE_OK && row_index < sizeof(embedded_root_global_privileges) /
                                            sizeof(embedded_root_global_privileges[0]);
         ++row_index) {
        const char *values[information_schema_user_privileges_column_count] = {
            "'root'@'%'",
            "def",
            embedded_root_global_privileges[row_index],
            "YES",
        };

        rc = append_information_schema_row(database, rows, values);
    }

    return rc;
}

static int append_information_schema_user_attributes_system_row(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    const char *values[information_schema_user_attributes_column_count] = {
        "root",
        "%",
        NULL,
    };

    return append_information_schema_row(database, rows, values);
}

static int append_information_schema_resource_groups_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    char vcpu_ids[integer_text_capacity];
    const char *user_values[information_schema_resource_groups_column_count] = {
        "USR_default",
        "USER",
        "1",
        vcpu_ids,
        "0",
    };
    const char *system_values[information_schema_resource_groups_column_count] = {
        "SYS_default",
        "SYSTEM",
        "1",
        vcpu_ids,
        "0",
    };
    int rc =
        format_information_schema_resource_group_vcpu_ids(database, vcpu_ids, sizeof(vcpu_ids));

    if (rc == MYLITE_OK) {
        rc = append_information_schema_row(database, rows, user_values);
    }
    if (rc == MYLITE_OK) {
        rc = append_information_schema_row(database, rows, system_values);
    }
    return rc;
}

static int append_information_schema_st_units_of_measure_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    int rc = MYLITE_OK;

    for (size_t row_index = 0U;
         rc == MYLITE_OK && row_index < mylite_execution_catalog_st_unit_of_measure_row_count();
         ++row_index) {
        const struct mylite_execution_catalog_st_unit_of_measure_row *unit =
            mylite_execution_catalog_st_unit_of_measure_row_at(row_index);
        const char *values[information_schema_st_units_of_measure_column_count] = {
            unit->unit_name,
            "LINEAR",
            unit->conversion_factor,
            "",
        };

        rc = append_information_schema_row(database, rows, values);
    }
    return rc;
}

static int format_information_schema_resource_group_vcpu_ids(
    struct mylite_db *database,
    char *buffer,
    size_t buffer_size
) {
    long cpu_count = information_schema_online_processor_count();
    int written = 0;

    if (cpu_count <= 1) {
        written = snprintf(buffer, buffer_size, "0");
    } else {
        written = snprintf(buffer, buffer_size, "0-%ld", cpu_count - 1);
    }
    if (written < 0 || (size_t)written >= buffer_size) {
        set_runtime_error(database, "failed to format resource group vcpu ids");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static long information_schema_online_processor_count(void) {
#ifdef _WIN32
    SYSTEM_INFO info;

    GetSystemInfo(&info);
    if (info.dwNumberOfProcessors > 0U) {
        return (long)info.dwNumberOfProcessors;
    }
#elif defined(_SC_NPROCESSORS_ONLN)
    long count = sysconf(_SC_NPROCESSORS_ONLN);

    if (count > 0) {
        return count;
    }
#endif
    return 1;
}

static int append_information_schema_processlist_system_row(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    struct information_schema_row_set *rows
) {
    char connection_id_text[integer_text_capacity];
    char user[MYLITE_SESSION_IDENTIFIER_CAPACITY];
    char host[MYLITE_SESSION_IDENTIFIER_CAPACITY];
    char *info = NULL;
    const char *selected_schema = NULL;
    const char *values[information_schema_processlist_column_count] = {
        connection_id_text,
        user,
        host,
        NULL,
        "Query",
        "0",
        "executing",
        NULL,
    };
    int written = snprintf(
        connection_id_text,
        sizeof(connection_id_text),
        "%" PRIu64,
        database->session.connection_id
    );
    int rc = MYLITE_OK;

    if (written < 0 || (size_t)written >= sizeof(connection_id_text)) {
        set_runtime_error(database, "failed to format INFORMATION_SCHEMA.PROCESSLIST ID");
        return MYLITE_ERROR;
    }
    if (database->session.has_selected_schema) {
        selected_schema = database->session.selected_schema;
    }
    values[information_schema_processlist_db_column] = selected_schema;

    rc = format_show_processlist_user_host(database, user, sizeof(user), host, sizeof(host));
    if (rc == MYLITE_OK) {
        rc = copy_information_schema_processlist_info(database, context, &info);
    }
    if (rc == MYLITE_OK) {
        values[information_schema_processlist_info_column] = info;
        rc = append_information_schema_row(database, rows, values);
    }

    free(info);
    return rc;
}

static int copy_information_schema_processlist_info(
    struct mylite_db *database,
    const struct mylite_statement_context *context,
    char **out_info
) {
    const char *sql = NULL;
    size_t sql_size = 0U;
    size_t length = 0U;
    char *info = NULL;

    if (out_info == NULL) {
        return MYLITE_MISUSE;
    }
    *out_info = NULL;
    if (context == NULL) {
        set_runtime_error(database, "failed to read INFORMATION_SCHEMA.PROCESSLIST statement text");
        return MYLITE_ERROR;
    }

    sql = mylite_statement_context_sql(context);
    sql_size = mylite_statement_context_sql_size(context);
    if (sql == NULL) {
        set_runtime_error(database, "failed to read INFORMATION_SCHEMA.PROCESSLIST statement text");
        return MYLITE_ERROR;
    }

    length = statement_info_length_without_terminator(sql, sql_size);
    if (length == SIZE_MAX) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }

    info = malloc(length + 1U);
    if (info == NULL) {
        set_nomem_error(database);
        return MYLITE_NOMEM;
    }
    if (length > 0U) {
        memcpy(info, sql, length);
    }
    info[length] = '\0';
    *out_info = info;
    return MYLITE_OK;
}
