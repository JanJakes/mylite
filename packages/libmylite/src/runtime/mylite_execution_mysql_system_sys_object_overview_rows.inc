static int append_sys_schema_object_overview_system_rows(
    struct mylite_db *database,
    struct information_schema_row_set *rows
) {
    struct sys_schema_object_overview_context context = {
        .database = database,
        .rows = rows,
        .groups = NULL,
        .group_count = 0U,
        .group_capacity = 0U,
        .schema = NULL,
    };
    int rc = MYLITE_OK;

    if (rows->definition->column_count != sys_schema_object_overview_column_count) {
        set_runtime_error(database, "invalid sys.schema_object_overview columns");
        return MYLITE_ERROR;
    }

    rc = collect_sys_schema_object_overview_builtin_groups(&context);
    if (rc == MYLITE_OK) {
        rc = collect_sys_schema_object_overview_mysql_system_index_groups(&context);
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_for_each_schema(
            database,
            append_sys_schema_object_overview_schema_groups,
            &context
        );
    }
    if (rc == MYLITE_OK) {
        rc = increment_sys_schema_object_overview_group(
            &context,
            "sys",
            "TRIGGER",
            mylite_execution_catalog_sys_sys_config_trigger_count()
        );
    }
    if (rc == MYLITE_OK) {
        sort_sys_schema_object_overview_groups(&context);
        rc = append_sys_schema_object_overview_group_rows(&context);
    }

    free(context.groups);
    return rc;
}

static int collect_sys_schema_object_overview_builtin_groups(
    struct sys_schema_object_overview_context *context
) {
    int rc = MYLITE_OK;

    for (size_t directory_index = 0U;
         rc == MYLITE_OK &&
         directory_index < mylite_execution_catalog_builtin_schema_table_directory_count();
         ++directory_index) {
        const struct mylite_execution_catalog_builtin_schema_table_directory *directory =
            mylite_execution_catalog_builtin_schema_table_directory_at(directory_index);

        for (size_t table_index = 0U; rc == MYLITE_OK && table_index < directory->table_count;
             ++table_index) {
            const char *table_type =
                builtin_schema_table_type(directory, directory->table_names[table_index]);

            rc = increment_sys_schema_object_overview_group(
                context,
                directory->schema_name,
                table_type,
                1U
            );
        }
    }
    return rc;
}

static int collect_sys_schema_object_overview_mysql_system_index_groups(
    struct sys_schema_object_overview_context *context
) {
    int rc = MYLITE_OK;

    for (size_t index = 0U;
         rc == MYLITE_OK && index < mylite_execution_catalog_mysql_system_table_definition_count();
         ++index) {
        const struct mylite_execution_catalog_mysql_system_table *definition =
            mylite_execution_catalog_mysql_system_table_definition_at(index);
        size_t count = mysql_system_table_primary_key_column_count(definition);

        if (SIZE_MAX - count < definition->secondary_index_count) {
            set_runtime_error(context->database, "sys.schema_object_overview count overflow");
            return MYLITE_ERROR;
        }
        count += definition->secondary_index_count;
        if (count > 0U) {
            rc = increment_sys_schema_object_overview_group(
                context,
                definition->schema_name,
                "INDEX (BTREE)",
                (uint64_t)count
            );
        }
    }
    return rc;
}

static int append_sys_schema_object_overview_schema_groups(
    const struct mylite_catalog_schema_descriptor *schema,
    void *user_data
) {
    struct sys_schema_object_overview_context *context = user_data;

    if (schema == NULL || context == NULL || context->database == NULL || context->rows == NULL) {
        return MYLITE_MISUSE;
    }
    if (find_builtin_schema_table_directory(schema->name) != NULL) {
        return MYLITE_OK;
    }

    context->schema = schema;
    return mylite_catalog_for_each_table_in_schema(
        context->database,
        schema->schema_id,
        append_sys_schema_object_overview_table_groups,
        context
    );
}

static int append_sys_schema_object_overview_table_groups(
    const struct mylite_catalog_table_descriptor *table,
    void *user_data
) {
    struct sys_schema_object_overview_context *context = user_data;
    int rc = MYLITE_OK;

    if (table == NULL || context == NULL || context->database == NULL || context->rows == NULL ||
        context->schema == NULL) {
        return MYLITE_MISUSE;
    }

    if (table->kind == MYLITE_CATALOG_TABLE_KIND_BASE) {
        rc = increment_sys_schema_object_overview_group(
            context,
            context->schema->name,
            "BASE TABLE",
            1U
        );
        if (rc == MYLITE_OK) {
            rc = append_sys_schema_object_overview_base_table_index_groups(context, table);
        }
    } else if (table->kind == MYLITE_CATALOG_TABLE_KIND_VIEW) {
        rc = increment_sys_schema_object_overview_group(context, context->schema->name, "VIEW", 1U);
    }

    return rc;
}

static int append_sys_schema_object_overview_base_table_index_groups(
    struct sys_schema_object_overview_context *context,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct loaded_index_info *indexes = NULL;
    size_t column_count = 0U;
    size_t index_count = 0U;
    int rc = mylite_execution_load_table_columns(context->database, table->table_id, &columns, &column_count);

    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_index_infos(
            context->database,
            table->table_id,
            columns,
            column_count,
            &indexes,
            &index_count
        );
    }
    for (size_t index = 0U; rc == MYLITE_OK && index < index_count; ++index) {
        char object_type[sizeof("INDEX (FULLTEXT)")];
        int written = snprintf(
            object_type,
            sizeof(object_type),
            "INDEX (%s)",
            show_index_index_type_text(&indexes[index])
        );

        if (written < 0 || (size_t)written >= sizeof(object_type)) {
            set_runtime_error(context->database, "failed to format sys.schema_object_overview row");
            rc = MYLITE_ERROR;
        } else {
            rc = increment_sys_schema_object_overview_group(
                context,
                context->schema->name,
                object_type,
                indexes[index].part_count
            );
        }
    }

    free(columns);
    mylite_execution_loaded_index_infos_deinit(&indexes, &index_count);
    if (rc != MYLITE_OK &&
        mylite_diagnostics_errcode(mylite_connection_diagnostics(context->database)) == MYLITE_OK) {
        if (rc == MYLITE_NOMEM) {
            set_nomem_error(context->database);
        } else {
            set_runtime_error(context->database, "failed to build sys.schema_object_overview rows");
        }
    }
    return rc;
}

static int increment_sys_schema_object_overview_group(
    struct sys_schema_object_overview_context *context,
    const char *schema_name,
    const char *object_type,
    uint64_t count
) {
    if (context == NULL || context->database == NULL || schema_name == NULL ||
        object_type == NULL) {
        return MYLITE_MISUSE;
    }
    if (count == 0U) {
        return MYLITE_OK;
    }

    for (size_t index = 0U; index < context->group_count; ++index) {
        struct sys_schema_object_overview_group *group = &context->groups[index];

        if (strcmp(group->db, schema_name) == 0 && strcmp(group->object_type, object_type) == 0) {
            if (UINT64_MAX - group->count < count) {
                set_runtime_error(context->database, "sys.schema_object_overview count overflow");
                return MYLITE_ERROR;
            }
            group->count += count;
            return MYLITE_OK;
        }
    }

    if (context->group_count == context->group_capacity) {
        int rc = grow_sys_schema_object_overview_groups(context);

        if (rc != MYLITE_OK) {
            return rc;
        }
    }

    struct sys_schema_object_overview_group *group = &context->groups[context->group_count++];
    int db_written = snprintf(group->db, sizeof(group->db), "%s", schema_name);
    int object_type_written =
        snprintf(group->object_type, sizeof(group->object_type), "%s", object_type);

    if (db_written < 0 || (size_t)db_written >= sizeof(group->db) || object_type_written < 0 ||
        (size_t)object_type_written >= sizeof(group->object_type)) {
        set_runtime_error(context->database, "failed to format sys.schema_object_overview group");
        --context->group_count;
        return MYLITE_ERROR;
    }
    group->count = count;
    return MYLITE_OK;
}

static int grow_sys_schema_object_overview_groups(
    struct sys_schema_object_overview_context *context
) {
    size_t new_capacity = context->group_capacity == 0U
                              ? sys_schema_object_overview_initial_group_capacity
                              : context->group_capacity * 2U;
    struct sys_schema_object_overview_group *groups = NULL;

    if (new_capacity < context->group_capacity ||
        new_capacity > SIZE_MAX / sizeof(*context->groups)) {
        set_nomem_error(context->database);
        return MYLITE_NOMEM;
    }

    groups = realloc(context->groups, new_capacity * sizeof(*groups));
    if (groups == NULL) {
        set_nomem_error(context->database);
        return MYLITE_NOMEM;
    }

    context->groups = groups;
    context->group_capacity = new_capacity;
    return MYLITE_OK;
}

static void sort_sys_schema_object_overview_groups(
    struct sys_schema_object_overview_context *context
) {
    if (context == NULL || context->group_count < 2U) {
        return;
    }

    for (size_t index = 1U; index < context->group_count; ++index) {
        struct sys_schema_object_overview_group group = context->groups[index];
        size_t insert = index;

        while (insert > 0U &&
               compare_sys_schema_object_overview_groups(&group, &context->groups[insert - 1U]) <
                   0) {
            context->groups[insert] = context->groups[insert - 1U];
            --insert;
        }
        context->groups[insert] = group;
    }
}

static int compare_sys_schema_object_overview_groups(
    const struct sys_schema_object_overview_group *left,
    const struct sys_schema_object_overview_group *right
) {
    int db_compare = strcmp(left->db, right->db);

    if (db_compare != 0) {
        return db_compare;
    }
    return strcmp(left->object_type, right->object_type);
}

static int append_sys_schema_object_overview_group_rows(
    struct sys_schema_object_overview_context *context
) {
    char count_text[integer_text_capacity];
    int rc = MYLITE_OK;

    for (size_t index = 0U; rc == MYLITE_OK && index < context->group_count; ++index) {
        const struct sys_schema_object_overview_group *group = &context->groups[index];
        const char *values[sys_schema_object_overview_column_count] = {
            group->db,
            group->object_type,
            count_text,
        };

        rc = format_uint64(context->database, group->count, count_text, sizeof(count_text));
        if (rc == MYLITE_OK) {
            rc = append_information_schema_row(context->database, context->rows, values);
        }
    }
    return rc;
}
