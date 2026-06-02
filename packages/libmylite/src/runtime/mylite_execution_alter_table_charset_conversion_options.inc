static int plan_alter_table_default_charset_collation(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_default_charset_collation *out_plan
) {
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_default_charset_collation){0};
    rc = resolve_writable_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_read_table_by_name(
            database,
            out_plan->target.schema.schema_id,
            out_plan->target.table_name,
            &out_plan->table
        );
        if (rc != MYLITE_OK) {
            set_table_does_not_exist_error(
                database,
                out_plan->target.schema.name,
                out_plan->target.table_name
            );
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && out_plan->table.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(
            database,
            "ALTER TABLE DEFAULT CHARSET/COLLATE supports only persistent base tables"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        memcpy(
            out_plan->default_charset,
            out_plan->table.default_charset,
            strlen(out_plan->table.default_charset) + 1U
        );
        memcpy(
            out_plan->default_collation,
            out_plan->table.default_collation,
            strlen(out_plan->table.default_collation) + 1U
        );
        rc = validate_alter_table_default_charset_collation_options(
            database,
            child_at(statement, 1U)
        );
    }
    if (rc == MYLITE_OK) {
        rc = apply_alter_table_default_charset_collation_options(
            database,
            child_at(statement, 1U),
            out_plan
        );
    }
    if (rc == MYLITE_OK) {
        out_plan->changes_descriptor = false;
        if (strcmp(out_plan->default_charset, out_plan->table.default_charset) != 0) {
            out_plan->changes_descriptor = true;
        }
        if (strcmp(out_plan->default_collation, out_plan->table.default_collation) != 0) {
            out_plan->changes_descriptor = true;
        }
    }

    return rc;
}

static int apply_alter_table_default_charset_collation_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_options,
    struct planned_alter_table_default_charset_collation *plan
) {
    const struct mylite_sql_ast_node *table_option = NULL;
    bool has_charset_option = false;
    bool has_collation_option = false;
    int rc = MYLITE_OK;

    if (table_options == NULL || table_options->kind != MYLITE_SQL_AST_TABLE_OPTION_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    table_option = child_at(table_options, 0U);
    while (table_option != NULL) {
        if (table_option->kind == MYLITE_SQL_AST_TABLE_CHARSET_OPTION) {
            has_charset_option = true;
        } else if (table_option->kind == MYLITE_SQL_AST_TABLE_COLLATION_OPTION) {
            has_collation_option = true;
        }
        table_option = table_option->next_sibling;
    }
    table_option = child_at(table_options, 0U);
    while (rc == MYLITE_OK && table_option != NULL) {
        if (table_option->kind == MYLITE_SQL_AST_TABLE_CHARSET_OPTION) {
            rc = apply_table_charset_option(
                database,
                table_option,
                false,
                plan->default_charset,
                sizeof(plan->default_charset)
            );
        } else if (table_option->kind == MYLITE_SQL_AST_TABLE_COLLATION_OPTION) {
            rc = apply_table_collation_option(
                database,
                table_option,
                false,
                plan->default_collation,
                sizeof(plan->default_collation)
            );
        }
        table_option = table_option->next_sibling;
    }
    if (rc == MYLITE_OK && has_charset_option && !has_collation_option) {
        rc = copy_default_collation_for_charset(
            database,
            plan->default_charset,
            plan->default_collation,
            sizeof(plan->default_collation)
        );
    }

    return rc;
}

static int plan_alter_table_convert_character_set(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *statement,
    struct planned_alter_table_convert_character_set *out_plan
) {
    int rc = MYLITE_OK;

    *out_plan = (struct planned_alter_table_convert_character_set){0};
    rc = resolve_writable_table_name(database, child_at(statement, 0U), &out_plan->target);
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_plan->target.table_name)) {
        set_reserved_name_error(database, "table", out_plan->target.table_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = mylite_catalog_read_table_by_name(
            database,
            out_plan->target.schema.schema_id,
            out_plan->target.table_name,
            &out_plan->table
        );
        if (rc != MYLITE_OK) {
            set_table_does_not_exist_error(
                database,
                out_plan->target.schema.name,
                out_plan->target.table_name
            );
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK && out_plan->table.kind != MYLITE_CATALOG_TABLE_KIND_BASE) {
        set_unsupported_error(
            database,
            "ALTER TABLE CONVERT TO CHARACTER SET supports only persistent base tables"
        );
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        memcpy(
            out_plan->default_charset,
            out_plan->table.default_charset,
            strlen(out_plan->table.default_charset) + 1U
        );
        memcpy(
            out_plan->default_collation,
            out_plan->table.default_collation,
            strlen(out_plan->table.default_collation) + 1U
        );
        rc = apply_alter_table_convert_character_set_options(
            database,
            child_at(statement, 1U),
            out_plan
        );
    }
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_table_columns(
            database,
            out_plan->table.table_id,
            &out_plan->columns,
            &out_plan->column_count
        );
    }
    if (rc == MYLITE_OK) {
        rc = validate_alter_table_convert_character_set_columns(database, out_plan);
    }
    if (rc == MYLITE_OK) {
        out_plan->changes_descriptor =
            (strcmp(out_plan->default_charset, out_plan->table.default_charset) != 0 ||
             strcmp(out_plan->default_collation, out_plan->table.default_collation) != 0) != 0;
        for (size_t index = 0U; !out_plan->changes_descriptor && index < out_plan->column_count;
             ++index) {
            if (alter_table_convert_column_needs_explicit_metadata_update(
                    &out_plan->columns[index],
                    out_plan
                )) {
                out_plan->changes_descriptor = true;
            }
        }
    }
    if (rc != MYLITE_OK) {
        planned_alter_table_convert_character_set_deinit(out_plan);
    }

    return rc;
}

static int apply_alter_table_convert_character_set_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_options,
    struct planned_alter_table_convert_character_set *plan
) {
    const struct mylite_sql_ast_node *charset_option = NULL;
    const struct mylite_sql_ast_node *collation_option = NULL;
    char charset_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char collation_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = MYLITE_OK;

    if (table_options == NULL || table_options->kind != MYLITE_SQL_AST_TABLE_OPTION_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    charset_option = child_at(table_options, 0U);
    collation_option = child_at(table_options, 1U);
    if (charset_option == NULL || charset_option->kind != MYLITE_SQL_AST_TABLE_CHARSET_OPTION ||
        child_at(table_options, 2U) != NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (table_charset_option_is_default_keyword(charset_option)) {
        if (collation_option != NULL) {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        rc = apply_alter_table_convert_character_set_default(database, plan);
    } else {
        rc = copy_table_charset_option_name(
            database,
            charset_option,
            charset_name,
            sizeof(charset_name)
        );
        if (rc == MYLITE_OK) {
            rc = copy_normalized_charset_name(
                database,
                charset_name,
                plan->default_charset,
                sizeof(plan->default_charset)
            );
        }
        if (rc == MYLITE_OK && collation_option == NULL) {
            rc = copy_default_collation_for_charset(
                database,
                plan->default_charset,
                plan->default_collation,
                sizeof(plan->default_collation)
            );
        }
    }
    if (rc == MYLITE_OK && collation_option != NULL) {
        rc = copy_table_collation_option_name(
            database,
            collation_option,
            collation_name,
            sizeof(collation_name)
        );
        if (rc == MYLITE_OK) {
            rc = copy_normalized_collation_name(
                database,
                collation_name,
                plan->default_collation,
                sizeof(plan->default_collation)
            );
        }
    }
    if (rc == MYLITE_OK && !text_equals_ascii_case_insensitive(plan->default_charset, "utf8mb4")) {
        set_unsupported_error(
            database,
            "ALTER TABLE CONVERT TO CHARACTER SET supports only utf8mb4 target character set"
        );
        return MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        rc = validate_table_charset_collation_option_values_with_binary(
            database,
            true,
            plan->default_charset,
            (collation_option != NULL || table_charset_option_is_default_keyword(charset_option)) !=
                0,
            plan->default_collation,
            true
        );
    }

    return rc;
}

static int apply_alter_table_convert_character_set_default(
    struct mylite_db *database,
    struct planned_alter_table_convert_character_set *plan
) {
    char charset_buffer[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    char collation_buffer[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    const char *charset_name = NULL;
    const char *collation_name = NULL;
    int rc = database_character_set_system_variable_value(
        database,
        false,
        charset_buffer,
        sizeof(charset_buffer),
        &charset_name
    );

    if (rc == MYLITE_OK) {
        rc = database_collation_system_variable_value(
            database,
            false,
            collation_buffer,
            sizeof(collation_buffer),
            &collation_name
        );
    }
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (charset_name == NULL || strlen(charset_name) >= sizeof(plan->default_charset) ||
        collation_name == NULL || strlen(collation_name) >= sizeof(plan->default_collation)) {
        set_runtime_error(database, "database charset descriptor is too small");
        return MYLITE_ERROR;
    }

    memcpy(plan->default_charset, charset_name, strlen(charset_name) + 1U);
    memcpy(plan->default_collation, collation_name, strlen(collation_name) + 1U);
    return MYLITE_OK;
}

static int validate_alter_table_convert_character_set_columns(
    struct mylite_db *database,
    const struct planned_alter_table_convert_character_set *plan
) {
    for (size_t index = 0U; index < plan->column_count; ++index) {
        const struct mylite_catalog_column_descriptor *column = &plan->columns[index];
        const char *effective_charset = NULL;

        if (column_descriptor_is_national_char_or_varchar(column)) {
            set_unsupported_error(
                database,
                "ALTER TABLE CONVERT TO CHARACTER SET does not yet support national character "
                "columns"
            );
            return MYLITE_ERROR;
        }
        if (column_descriptor_is_enum(column) || column_descriptor_is_set(column)) {
            set_unsupported_error(
                database,
                "ALTER TABLE CONVERT TO CHARACTER SET does not yet support ENUM or SET columns"
            );
            return MYLITE_ERROR;
        }
        if (column_descriptor_is_binary_string_family(column) ||
            !alter_table_convert_column_participates(column)) {
            continue;
        }

        effective_charset = column_effective_character_set_name(&plan->table, column);
        if (effective_charset == NULL ||
            !text_equals_ascii_case_insensitive(effective_charset, "utf8mb4")) {
            set_unsupported_error(
                database,
                "ALTER TABLE CONVERT TO CHARACTER SET does not yet support cross-character-set "
                "conversion"
            );
            return MYLITE_ERROR;
        }
    }

    return MYLITE_OK;
}

static bool table_charset_option_is_default_keyword(
    const struct mylite_sql_ast_node *charset_option
) {
    const struct mylite_sql_ast_node *name_node = child_at(charset_option, 0U);
    char keyword[sizeof("DEFAULT")];

    if (name_node == NULL || name_node->kind != MYLITE_SQL_AST_IDENTIFIER ||
        name_node->span.text == NULL || name_node->span.length != sizeof("DEFAULT") - 1U) {
        return false;
    }

    memcpy(keyword, name_node->span.text, name_node->span.length);
    keyword[name_node->span.length] = '\0';
    return text_equals_ascii_case_insensitive(keyword, "DEFAULT");
}

static bool alter_table_convert_column_participates(
    const struct mylite_catalog_column_descriptor *column
) {
    return column_descriptor_is_string_family(column);
}

static bool alter_table_convert_column_needs_explicit_metadata_update(
    const struct mylite_catalog_column_descriptor *column,
    const struct planned_alter_table_convert_character_set *plan
) {
    if (!alter_table_convert_column_participates(column)) {
        return false;
    }
    if (column->character_set_name[0] == '\0' && column->collation_name[0] == '\0') {
        return false;
    }
    return (strcmp(column->character_set_name, plan->default_charset) != 0 ||
            strcmp(column->collation_name, plan->default_collation) != 0) != 0;
}

static void planned_alter_table_convert_character_set_deinit(
    struct planned_alter_table_convert_character_set *plan
) {
    if (plan == NULL) {
        return;
    }

    free(plan->columns);
    *plan = (struct planned_alter_table_convert_character_set){0};
}
