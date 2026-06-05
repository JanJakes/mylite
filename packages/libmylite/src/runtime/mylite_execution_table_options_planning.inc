static int validate_create_table_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_options,
    const char *table_name
) {
    const struct mylite_sql_ast_node *table_option = NULL;
    char charset_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY] = "";
    char collation_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY] = "";
    bool has_charset = false;
    bool has_collation = false;
    int rc = MYLITE_OK;

    if (table_options == NULL) {
        return MYLITE_OK;
    }
    if (table_options->kind != MYLITE_SQL_AST_TABLE_OPTION_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    table_option = child_at(table_options, 0U);
    while (rc == MYLITE_OK && table_option != NULL) {
        if (table_option->kind == MYLITE_SQL_AST_TABLE_CHARSET_OPTION) {
            rc = copy_table_charset_option_name(
                database,
                table_option,
                charset_name,
                sizeof(charset_name)
            );
            has_charset = true;
        } else if (table_option->kind == MYLITE_SQL_AST_TABLE_COLLATION_OPTION) {
            rc = copy_table_collation_option_name(
                database,
                table_option,
                collation_name,
                sizeof(collation_name)
            );
            has_collation = true;
        } else {
            rc = validate_create_table_option(database, table_option, table_name);
        }
        table_option = table_option->next_sibling;
    }
    if (rc == MYLITE_OK) {
        rc = validate_table_charset_collation_option_values_with_binary(
            database,
            has_charset,
            charset_name,
            has_collation,
            collation_name,
            true
        );
    }

    return rc;
}

static int validate_alter_table_default_charset_collation_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_options
) {
    const struct mylite_sql_ast_node *table_option = NULL;
    char charset_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY] = "";
    char collation_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY] = "";
    bool has_charset = false;
    bool has_collation = false;
    int rc = MYLITE_OK;

    if (table_options == NULL || table_options->kind != MYLITE_SQL_AST_TABLE_OPTION_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    table_option = child_at(table_options, 0U);
    while (rc == MYLITE_OK && table_option != NULL) {
        if (table_option->kind == MYLITE_SQL_AST_TABLE_CHARSET_OPTION) {
            rc = copy_table_charset_option_name(
                database,
                table_option,
                charset_name,
                sizeof(charset_name)
            );
            has_charset = true;
        } else if (table_option->kind == MYLITE_SQL_AST_TABLE_COLLATION_OPTION) {
            rc = copy_table_collation_option_name(
                database,
                table_option,
                collation_name,
                sizeof(collation_name)
            );
            has_collation = true;
        } else {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        table_option = table_option->next_sibling;
    }
    if (rc == MYLITE_OK) {
        rc = validate_table_charset_collation_option_values(
            database,
            has_charset,
            charset_name,
            has_collation,
            collation_name
        );
    }

    return rc;
}

static int validate_schema_default_charset_collation_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *schema_options
) {
    const struct mylite_sql_ast_node *schema_option = NULL;
    struct schema_default_option_validation validation = {0};
    int rc = MYLITE_OK;

    if (schema_options == NULL) {
        return MYLITE_OK;
    }
    if (schema_options->kind != MYLITE_SQL_AST_TABLE_OPTION_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    schema_option = child_at(schema_options, 0U);
    while (rc == MYLITE_OK && schema_option != NULL) {
        if (schema_option->kind == MYLITE_SQL_AST_TABLE_CHARSET_OPTION) {
            rc = track_schema_default_charset_option(database, schema_option, &validation);
        } else if (schema_option->kind == MYLITE_SQL_AST_TABLE_COLLATION_OPTION) {
            rc = track_schema_default_collation_option(database, schema_option, &validation);
        } else {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        schema_option = schema_option->next_sibling;
    }
    if (rc == MYLITE_OK) {
        rc = validate_table_charset_collation_option_values_with_binary(
            database,
            validation.has_charset,
            validation.charset_name,
            validation.has_collation,
            validation.collation_name,
            true
        );
    }
    if (rc == MYLITE_OK && validation.unsupported_charset_name[0] != '\0') {
        set_unknown_character_set_error(database, validation.unsupported_charset_name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK && validation.unsupported_collation_name[0] != '\0') {
        set_unknown_collation_error(database, validation.unsupported_collation_name);
        rc = MYLITE_ERROR;
    }

    return rc;
}

static int track_schema_default_charset_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *schema_option,
    struct schema_default_option_validation *validation
) {
    char normalized_charset[MYLITE_CATALOG_IDENTIFIER_CAPACITY] = "";
    int rc = copy_table_charset_option_name(
        database,
        schema_option,
        validation->charset_name,
        sizeof(validation->charset_name)
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (charset_name_is_binary(validation->charset_name)) {
        memcpy(normalized_charset, "binary", sizeof("binary"));
    } else if (character_set_by_name(validation->charset_name) != NULL) {
        const struct mylite_execution_catalog_character_set *charset =
            character_set_by_name(validation->charset_name);

        if (strlen(charset->name) >= sizeof(normalized_charset)) {
            set_runtime_error(database, "schema charset descriptor is too small");
            return MYLITE_ERROR;
        }
        memcpy(normalized_charset, charset->name, strlen(charset->name) + 1U);
    } else if (validation->unsupported_charset_name[0] == '\0') {
        memcpy(
            validation->unsupported_charset_name,
            validation->charset_name,
            strlen(validation->charset_name) + 1U
        );
    }

    if (normalized_charset[0] != '\0' && !validation->has_charset) {
        memcpy(validation->first_charset_name, normalized_charset, strlen(normalized_charset) + 1U);
    } else if (
        normalized_charset[0] != '\0' && validation->first_charset_name[0] != '\0' &&
        strcmp(validation->first_charset_name, normalized_charset) != 0
    ) {
        set_conflicting_character_set_declarations_error(
            database,
            validation->first_charset_name,
            normalized_charset
        );
        return MYLITE_ERROR;
    }
    validation->has_charset = true;

    return MYLITE_OK;
}

static int track_schema_default_collation_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *schema_option,
    struct schema_default_option_validation *validation
) {
    int rc = copy_table_collation_option_name(
        database,
        schema_option,
        validation->collation_name,
        sizeof(validation->collation_name)
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (collation_by_name(validation->collation_name) == NULL &&
        validation->unsupported_collation_name[0] == '\0') {
        memcpy(
            validation->unsupported_collation_name,
            validation->collation_name,
            strlen(validation->collation_name) + 1U
        );
    }
    validation->has_collation = true;

    return MYLITE_OK;
}

static int validate_create_table_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_option,
    const char *table_name
) {
    if (table_option == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    if (table_option->kind == MYLITE_SQL_AST_TABLE_ENGINE_OPTION) {
        return validate_create_table_engine_option(database, table_option, table_name);
    }
    if (table_option->kind == MYLITE_SQL_AST_TABLE_CHARSET_OPTION) {
        return validate_create_table_charset_option(database, table_option, true);
    }
    if (table_option->kind == MYLITE_SQL_AST_TABLE_COLLATION_OPTION) {
        return validate_create_table_collation_option(database, table_option, true);
    }
    if (table_option->kind == MYLITE_SQL_AST_TABLE_AUTO_INCREMENT_OPTION) {
        return MYLITE_OK;
    }
    if (table_option->kind == MYLITE_SQL_AST_TABLE_COMMENT_OPTION) {
        return validate_create_table_comment_option(database, table_option);
    }
    if (create_table_storage_statistics_option_is_valid(table_option)) {
        return MYLITE_OK;
    }

    set_parse_error(database, NULL);
    return MYLITE_ERROR;
}

static int apply_create_table_charset_collation_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_options,
    struct planned_create_table *plan
) {
    const struct mylite_sql_ast_node *table_option = NULL;
    bool has_charset_option = false;
    bool has_collation_option = false;

    if (plan == NULL) {
        set_runtime_error(database, "invalid charset/collation table plan");
        return MYLITE_ERROR;
    }
    if (table_options == NULL) {
        return MYLITE_OK;
    }
    if (table_options->kind != MYLITE_SQL_AST_TABLE_OPTION_LIST) {
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
    while (table_option != NULL) {
        int rc = MYLITE_OK;

        if (table_option->kind == MYLITE_SQL_AST_TABLE_CHARSET_OPTION) {
            rc = apply_table_charset_option(
                database,
                table_option,
                true,
                plan->default_charset,
                sizeof(plan->default_charset)
            );
        } else if (table_option->kind == MYLITE_SQL_AST_TABLE_COLLATION_OPTION) {
            rc = apply_table_collation_option(
                database,
                table_option,
                true,
                plan->default_collation,
                sizeof(plan->default_collation)
            );
        }
        if (rc != MYLITE_OK) {
            return rc;
        }
        table_option = table_option->next_sibling;
    }
    if (has_charset_option && !has_collation_option) {
        int rc = copy_default_collation_for_charset(
            database,
            plan->default_charset,
            plan->default_collation,
            sizeof(plan->default_collation)
        );

        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    if (!has_charset_option && has_collation_option) {
        const struct mylite_execution_catalog_collation *collation =
            collation_by_name(plan->default_collation);

        if (collation == NULL) {
            set_unknown_collation_error(database, plan->default_collation);
            return MYLITE_ERROR;
        }
        if (strlen(collation->character_set) >= sizeof(plan->default_charset)) {
            set_runtime_error(database, "table charset descriptor is too small");
            return MYLITE_ERROR;
        }
        memcpy(
            plan->default_charset,
            collation->character_set,
            strlen(collation->character_set) + 1U
        );
    }

    return MYLITE_OK;
}

static int apply_schema_default_charset_collation_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *schema_options,
    char *default_charset,
    size_t default_charset_size,
    char *default_collation,
    size_t default_collation_size
) {
    const struct mylite_sql_ast_node *schema_option = NULL;
    bool has_collation_option = false;
    int rc = validate_schema_default_charset_collation_options(database, schema_options);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (schema_options == NULL) {
        return MYLITE_OK;
    }
    if (schema_options->kind != MYLITE_SQL_AST_TABLE_OPTION_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    schema_option = child_at(schema_options, 0U);
    while (rc == MYLITE_OK && schema_option != NULL) {
        if (schema_option->kind == MYLITE_SQL_AST_TABLE_CHARSET_OPTION) {
            rc = apply_schema_default_charset_option(
                database,
                schema_option,
                has_collation_option,
                default_charset,
                default_charset_size,
                default_collation,
                default_collation_size
            );
        } else if (schema_option->kind == MYLITE_SQL_AST_TABLE_COLLATION_OPTION) {
            rc = apply_schema_default_collation_option(
                database,
                schema_option,
                default_charset,
                default_charset_size,
                default_collation,
                default_collation_size
            );
            has_collation_option = true;
        } else {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        schema_option = schema_option->next_sibling;
    }

    return rc;
}

static int apply_schema_default_charset_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *schema_option,
    bool has_collation_option,
    char *default_charset,
    size_t default_charset_size,
    char *default_collation,
    size_t default_collation_size
) {
    char charset_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY] = "";
    int rc =
        copy_table_charset_option_name(database, schema_option, charset_name, sizeof(charset_name));

    if (rc == MYLITE_OK) {
        rc = copy_normalized_charset_name(
            database,
            charset_name,
            default_charset,
            default_charset_size
        );
    }
    if (rc == MYLITE_OK && !has_collation_option) {
        rc = copy_default_collation_for_charset(
            database,
            default_charset,
            default_collation,
            default_collation_size
        );
    }

    return rc;
}

static int apply_schema_default_collation_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *schema_option,
    char *default_charset,
    size_t default_charset_size,
    char *default_collation,
    size_t default_collation_size
) {
    char collation_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY] = "";
    const struct mylite_execution_catalog_collation *collation = NULL;
    int rc = copy_table_collation_option_name(
        database,
        schema_option,
        collation_name,
        sizeof(collation_name)
    );

    if (rc == MYLITE_OK) {
        collation = collation_by_name(collation_name);
        if (collation == NULL) {
            set_unknown_collation_error(database, collation_name);
            rc = MYLITE_ERROR;
        }
    }
    if (rc == MYLITE_OK) {
        rc = copy_normalized_charset_name(
            database,
            collation->character_set,
            default_charset,
            default_charset_size
        );
    }
    if (rc == MYLITE_OK) {
        rc = copy_normalized_collation_name(
            database,
            collation_name,
            default_collation,
            default_collation_size
        );
    }

    return rc;
}

static int apply_create_table_auto_increment_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_options,
    struct planned_create_table *plan
) {
    const uint64_t sqlite_int64_positive_max = 9223372036854775807ULL;
    const struct mylite_sql_ast_node *table_option = NULL;

    if (plan == NULL) {
        set_runtime_error(database, "invalid auto-increment table plan");
        return MYLITE_ERROR;
    }
    if (plan->auto_increment_next <= 0) {
        plan->auto_increment_next = 1;
    }
    if (table_options == NULL) {
        return MYLITE_OK;
    }
    if (table_options->kind != MYLITE_SQL_AST_TABLE_OPTION_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    table_option = child_at(table_options, 0U);
    while (table_option != NULL) {
        if (table_option->kind == MYLITE_SQL_AST_TABLE_AUTO_INCREMENT_OPTION) {
            const struct mylite_sql_ast_node *literal = child_at(table_option, 0U);
            uint64_t magnitude = 0U;
            int rc = MYLITE_OK;

            if (literal == NULL || literal->kind != MYLITE_SQL_AST_LITERAL ||
                mylite_sql_ast_node_literal_kind(literal) != MYLITE_SQL_AST_LITERAL_INTEGER) {
                set_parse_error(database, NULL);
                return MYLITE_ERROR;
            }
            rc = parse_unsigned_integer_literal(&literal->span, &magnitude);
            if (rc != MYLITE_OK || magnitude > sqlite_int64_positive_max) {
                set_unsupported_error(
                    database,
                    "AUTO_INCREMENT table option supports only signed 64-bit nonnegative values"
                );
                return MYLITE_ERROR;
            }
            plan->auto_increment_next = magnitude == 0U ? 1 : (int64_t)magnitude;
        }
        table_option = table_option->next_sibling;
    }

    return MYLITE_OK;
}

static int apply_create_table_comment_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_options,
    struct planned_create_table *plan
) {
    const struct mylite_sql_ast_node *table_option = NULL;

    if (plan == NULL) {
        set_runtime_error(database, "invalid table comment plan");
        return MYLITE_ERROR;
    }
    if (table_options == NULL) {
        return MYLITE_OK;
    }
    if (table_options->kind != MYLITE_SQL_AST_TABLE_OPTION_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    table_option = child_at(table_options, 0U);
    while (table_option != NULL) {
        if (table_option->kind == MYLITE_SQL_AST_TABLE_COMMENT_OPTION) {
            int rc = decode_table_comment_option(
                database,
                table_option,
                plan->target.table_name,
                plan->comment,
                sizeof(plan->comment)
            );

            if (rc != MYLITE_OK) {
                return rc;
            }
        }
        table_option = table_option->next_sibling;
    }

    return MYLITE_OK;
}

static int apply_create_table_storage_statistics_options(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_options,
    struct planned_create_table *plan
) {
    const struct mylite_sql_ast_node *table_option = NULL;
    int rc = MYLITE_OK;

    if (plan == NULL) {
        set_runtime_error(database, "invalid storage/statistics table plan");
        return MYLITE_ERROR;
    }
    if (table_options == NULL) {
        return MYLITE_OK;
    }
    if (table_options->kind != MYLITE_SQL_AST_TABLE_OPTION_LIST) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    table_option = child_at(table_options, 0U);
    while (rc == MYLITE_OK && table_option != NULL) {
        rc = apply_table_storage_statistics_option(database, table_option, plan);
        table_option = table_option->next_sibling;
    }
    if (rc == MYLITE_OK) {
        rc = validate_create_table_storage_statistics_combination(database, plan);
    }

    return rc;
}

static int apply_table_storage_statistics_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_option,
    struct planned_create_table *plan
) {
    if (table_option == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (table_option->kind == MYLITE_SQL_AST_TABLE_ROW_FORMAT_OPTION) {
        return apply_table_row_format_option(database, table_option, plan);
    }
    if (table_option->kind == MYLITE_SQL_AST_TABLE_KEY_BLOCK_SIZE_OPTION) {
        return apply_table_key_block_size_option(database, table_option, plan);
    }
    if (table_option->kind == MYLITE_SQL_AST_TABLE_PACK_KEYS_OPTION) {
        return apply_table_pack_keys_option(database, table_option, plan);
    }
    if (table_option->kind == MYLITE_SQL_AST_TABLE_CHECKSUM_OPTION) {
        return apply_table_checksum_option(database, table_option, plan);
    }
    if (table_option->kind == MYLITE_SQL_AST_TABLE_STATS_PERSISTENT_OPTION) {
        return apply_table_stats_persistent_option(database, table_option, plan);
    }
    if (table_option->kind == MYLITE_SQL_AST_TABLE_STATS_AUTO_RECALC_OPTION) {
        return apply_table_stats_auto_recalc_option(database, table_option, plan);
    }
    if (table_option->kind == MYLITE_SQL_AST_TABLE_STATS_SAMPLE_PAGES_OPTION) {
        return apply_table_stats_sample_pages_option(database, table_option, plan);
    }

    return MYLITE_OK;
}

static int apply_table_row_format_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_option,
    struct planned_create_table *plan
) {
    char row_format_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY] = "";
    int rc = copy_table_row_format_option_name(
        database,
        table_option,
        row_format_name,
        sizeof(row_format_name)
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (text_equals_ascii_case_insensitive(row_format_name, "DEFAULT")) {
        plan->row_format_option[0] = '\0';
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(row_format_name, "FIXED")) {
        set_table_storage_engine_option_error(database, plan->target.table_name);
        return MYLITE_ERROR;
    }
    if (text_equals_ascii_case_insensitive(row_format_name, "DYNAMIC")) {
        memcpy(plan->row_format_option, "DYNAMIC", sizeof("DYNAMIC"));
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(row_format_name, "COMPACT")) {
        memcpy(plan->row_format_option, "COMPACT", sizeof("COMPACT"));
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(row_format_name, "REDUNDANT")) {
        memcpy(plan->row_format_option, "REDUNDANT", sizeof("REDUNDANT"));
        return MYLITE_OK;
    }
    if (text_equals_ascii_case_insensitive(row_format_name, "COMPRESSED")) {
        memcpy(plan->row_format_option, "COMPRESSED", sizeof("COMPRESSED"));
        return MYLITE_OK;
    }

    set_parse_error(database, NULL);
    return MYLITE_ERROR;
}

static int apply_table_key_block_size_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_option,
    struct planned_create_table *plan
) {
    uint64_t value = 0U;
    int rc = parse_table_option_unsigned_integer(database, child_at(table_option, 0U), &value);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (value != 0U && value != 1U && value != 2U && value != 4U &&
        value != table_key_block_size_eight && value != table_key_block_size_sixteen) {
        set_table_storage_engine_option_error(database, plan->target.table_name);
        return MYLITE_ERROR;
    }
    plan->key_block_size = (int64_t)value;
    return MYLITE_OK;
}

static int apply_table_pack_keys_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_option,
    struct planned_create_table *plan
) {
    int64_t value = -1;
    int rc =
        parse_table_option_default_or_integer(database, child_at(table_option, 0U), -1, &value);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (value != -1 && value != 0 && value != 1) {
        set_unsupported_error(database, "PACK_KEYS supports only DEFAULT, 0, or 1");
        return MYLITE_ERROR;
    }
    plan->pack_keys = value;
    return MYLITE_OK;
}

static int apply_table_checksum_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_option,
    struct planned_create_table *plan
) {
    uint64_t value = 0U;
    int rc = parse_table_option_unsigned_integer(database, child_at(table_option, 0U), &value);

    if (rc != MYLITE_OK) {
        return rc;
    }
    plan->checksum = value == 0U ? 0 : 1;
    return MYLITE_OK;
}

static int apply_table_stats_persistent_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_option,
    struct planned_create_table *plan
) {
    int64_t value = -1;
    int rc =
        parse_table_option_default_or_integer(database, child_at(table_option, 0U), -1, &value);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (value != -1 && value != 0 && value != 1) {
        set_unsupported_error(database, "STATS_PERSISTENT supports only DEFAULT, 0, or 1");
        return MYLITE_ERROR;
    }
    plan->stats_persistent = value;
    return MYLITE_OK;
}

static int apply_table_stats_auto_recalc_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_option,
    struct planned_create_table *plan
) {
    int64_t value = -1;
    int rc =
        parse_table_option_default_or_integer(database, child_at(table_option, 0U), -1, &value);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (value != -1 && value != 0 && value != 1) {
        set_unsupported_error(database, "STATS_AUTO_RECALC supports only DEFAULT, 0, or 1");
        return MYLITE_ERROR;
    }
    plan->stats_auto_recalc = value;
    return MYLITE_OK;
}

static int apply_table_stats_sample_pages_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_option,
    struct planned_create_table *plan
) {
    uint64_t value = 0U;
    int rc = MYLITE_OK;

    if (table_option_value_is_default(child_at(table_option, 0U))) {
        plan->stats_sample_pages = 0;
        return MYLITE_OK;
    }
    rc = parse_table_option_unsigned_integer(database, child_at(table_option, 0U), &value);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (value == 0U || value > table_stats_sample_pages_max) {
        set_unsupported_error(
            database,
            "STATS_SAMPLE_PAGES value is outside the valid range for stats_sample_pages"
        );
        return MYLITE_ERROR;
    }
    plan->stats_sample_pages = (int64_t)value;
    return MYLITE_OK;
}

static int copy_table_row_format_option_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *table_option,
    char *row_format_name,
    size_t row_format_name_size
) {
    if (table_option == NULL || table_option->kind != MYLITE_SQL_AST_TABLE_ROW_FORMAT_OPTION) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    return copy_identifier_text(
        child_at(table_option, 0U),
        row_format_name,
        row_format_name_size,
        database
    );
}

static int parse_table_option_unsigned_integer(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    uint64_t *out_value
) {
    if (out_value == NULL || value_node == NULL || value_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(value_node) != MYLITE_SQL_AST_LITERAL_INTEGER) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (parse_unsigned_integer_literal(&value_node->span, out_value) != MYLITE_OK) {
        set_unsupported_error(
            database,
            "table option supports only unsigned decimal integer values"
        );
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int parse_table_option_default_or_integer(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *value_node,
    int64_t default_value,
    int64_t *out_value
) {
    uint64_t value = 0U;
    int rc = MYLITE_OK;

    if (out_value == NULL) {
        return MYLITE_MISUSE;
    }
    if (table_option_value_is_default(value_node)) {
        *out_value = default_value;
        return MYLITE_OK;
    }
    rc = parse_table_option_unsigned_integer(database, value_node, &value);

    if (rc != MYLITE_OK) {
        return rc;
    }
    if (value > (uint64_t)INT64_MAX) {
        set_unsupported_error(database, "table option integer value is out of range");
        return MYLITE_ERROR;
    }
    *out_value = (int64_t)value;
    return MYLITE_OK;
}

static bool table_option_value_is_default(const struct mylite_sql_ast_node *value_node) {
    static const char default_text[] = "DEFAULT";

    if (value_node == NULL || value_node->kind != MYLITE_SQL_AST_IDENTIFIER ||
        value_node->span.text == NULL || value_node->span.length != sizeof(default_text) - 1U) {
        return false;
    }
    for (size_t index = 0U; index < sizeof(default_text) - 1U; ++index) {
        char byte = value_node->span.text[index];
        if (byte >= 'a' && byte <= 'z') {
            byte = (char)(byte - ('a' - 'A'));
        }
        if (byte != default_text[index]) {
            return false;
        }
    }
    return true;
}

static bool create_table_storage_statistics_option_is_valid(
    const struct mylite_sql_ast_node *table_option
) {
    if (table_option == NULL) {
        return false;
    }
    switch (table_option->kind) {
    case MYLITE_SQL_AST_TABLE_ROW_FORMAT_OPTION:
    case MYLITE_SQL_AST_TABLE_KEY_BLOCK_SIZE_OPTION:
    case MYLITE_SQL_AST_TABLE_PACK_KEYS_OPTION:
    case MYLITE_SQL_AST_TABLE_CHECKSUM_OPTION:
    case MYLITE_SQL_AST_TABLE_STATS_PERSISTENT_OPTION:
    case MYLITE_SQL_AST_TABLE_STATS_AUTO_RECALC_OPTION:
    case MYLITE_SQL_AST_TABLE_STATS_SAMPLE_PAGES_OPTION:
        return true;
    default:
        return false;
    }
}

static int validate_create_table_storage_statistics_combination(
    struct mylite_db *database,
    const struct planned_create_table *plan
) {
    if (plan == NULL) {
        return MYLITE_MISUSE;
    }
    if (plan->key_block_size > 0 && plan->row_format_option[0] != '\0' &&
        !text_equals_ascii_case_insensitive(plan->row_format_option, "COMPRESSED")) {
        set_table_storage_engine_option_error(database, plan->target.table_name);
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int normalize_mysql_comment_text(
    struct mylite_db *database,
    char **in_out_text,
    size_t *in_out_length
) {
    enum {
        utf8_four_byte_lead_min = 0xf0U,
        utf8_four_byte_lead_max = 0xf4U,
        utf8_four_byte_width = 4U,
        utf8_four_byte_trailing_width = 3U,
    };
    struct mylite_dynamic_string normalized;
    char *text = NULL;
    size_t text_length = 0U;
    int rc = MYLITE_OK;

    if (in_out_text == NULL || in_out_length == NULL || *in_out_text == NULL) {
        return MYLITE_MISUSE;
    }

    text = *in_out_text;
    text_length = *in_out_length;
    mylite_dynamic_string_init(&normalized);
    for (size_t index = 0U; rc == MYLITE_OK && index < text_length; ++index) {
        unsigned char byte = (unsigned char)text[index];

        if (byte == '\0') {
            rc = mylite_dynamic_string_append(&normalized, "\\0");
        } else if (byte >= utf8_four_byte_lead_min && byte <= utf8_four_byte_lead_max &&
                   index + utf8_four_byte_width <= text_length) {
            rc = mylite_dynamic_string_append_char(&normalized, '?');
            index += utf8_four_byte_trailing_width;
        } else {
            rc = mylite_dynamic_string_append_char(&normalized, text[index]);
        }
    }
    if (rc == MYLITE_OK && normalized.text == NULL) {
        rc = mylite_dynamic_string_append(&normalized, "");
    }
    if (rc == MYLITE_OK) {
        size_t normalized_length = normalized.length;
        char *normalized_text = mylite_dynamic_string_take(&normalized);

        if (normalized_text == NULL) {
            rc = MYLITE_NOMEM;
        } else {
            free(*in_out_text);
            *in_out_text = normalized_text;
            *in_out_length = normalized_length;
        }
    }
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    mylite_dynamic_string_deinit(&normalized);
    return rc;
}

static int decode_table_comment_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *comment_option,
    const char *table_name,
    char *destination,
    size_t destination_size
) {
    char *decoded = NULL;
    size_t decoded_length = 0U;
    size_t decoded_character_count = 0U;
    int rc = MYLITE_OK;

    if (comment_option == NULL || comment_option->kind != MYLITE_SQL_AST_TABLE_COMMENT_OPTION ||
        destination == NULL || destination_size == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    rc = decode_table_option_string_literal(
        database,
        child_at(comment_option, 0U),
        &decoded,
        &decoded_length,
        (struct table_option_name_policy){
            .identifier_kind = "table comment",
            .nul_message = "table comments do not support NUL bytes",
        }
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (validate_utf8_text(decoded, decoded_length, &decoded_character_count) != MYLITE_OK) {
        free(decoded);
        set_unsupported_error(database, "table comments support only valid UTF-8 text");
        return MYLITE_ERROR;
    }
    rc = normalize_mysql_comment_text(database, &decoded, &decoded_length);
    if (rc != MYLITE_OK) {
        free(decoded);
        return rc;
    }
    if (validate_utf8_text(decoded, decoded_length, &decoded_character_count) != MYLITE_OK) {
        free(decoded);
        set_unsupported_error(database, "table comments support only valid UTF-8 text");
        return MYLITE_ERROR;
    }
    if (decoded_character_count > MYLITE_CATALOG_TABLE_COMMENT_MAX_CHARACTERS ||
        decoded_length >= destination_size) {
        free(decoded);
        set_table_comment_too_long_error(database, table_name);
        return MYLITE_ERROR;
    }

    memcpy(destination, decoded, decoded_length + 1U);
    free(decoded);
    return MYLITE_OK;
}

static int validate_create_table_engine_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *engine_option,
    const char *table_name
) {
    char engine_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = MYLITE_OK;

    if (engine_option == NULL) {
        return MYLITE_OK;
    }
    if (engine_option->kind != MYLITE_SQL_AST_TABLE_ENGINE_OPTION) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    rc = copy_table_option_name_text(
        database,
        child_at(engine_option, 0U),
        engine_name,
        sizeof(engine_name),
        (struct table_option_name_policy){
            .identifier_kind = "storage engine",
            .nul_message = "table engine names do not support NUL bytes",
        }
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (!text_equals_ascii_case_insensitive(engine_name, "InnoDB")) {
        if (session_sql_mode_has(
                &database->session,
                MYLITE_SESSION_SQL_MODE_NO_ENGINE_SUBSTITUTION
            )) {
            set_unknown_storage_engine_error(database, engine_name);
            return MYLITE_ERROR;
        }

        rc = append_unknown_storage_engine_warning(database, engine_name);
        if (rc == MYLITE_OK) {
            rc = append_using_storage_engine_warning(database, table_name);
        }
        return rc;
    }

    return MYLITE_OK;
}

static int append_unknown_storage_engine_warning(
    struct mylite_db *database,
    const char *engine_name
) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(message, sizeof(message), "Unknown storage engine '%s'", engine_name);

    if (written < 0) {
        message[0] = '\0';
    }
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_error_unknown_storage_engine,
        "42000",
        message
    );
}

static int append_using_storage_engine_warning(struct mylite_db *database, const char *table_name) {
    char message[MYLITE_DIAGNOSTIC_MESSAGE_CAPACITY];
    int written = snprintf(
        message,
        sizeof(message),
        "Using storage engine InnoDB for table '%s'",
        table_name == NULL ? "" : table_name
    );

    if (written < 0) {
        message[0] = '\0';
    }
    return mylite_diagnostics_append_warning(
        mylite_connection_diagnostics(database),
        mysql_warning_using_storage_engine,
        "HY000",
        message
    );
}

static int validate_create_table_charset_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *charset_option,
    bool allow_binary
) {
    char charset_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = MYLITE_OK;

    rc = copy_table_charset_option_name(
        database,
        charset_option,
        charset_name,
        sizeof(charset_name)
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (allow_binary && charset_name_is_binary(charset_name)) {
        return MYLITE_OK;
    }
    if (charset_name_is_binary(charset_name)) {
        set_unknown_character_set_error(database, charset_name);
        return MYLITE_ERROR;
    }
    if (character_set_by_name(charset_name) == NULL) {
        set_unknown_character_set_error(database, charset_name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_create_table_collation_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *collation_option,
    bool allow_binary
) {
    char collation_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = MYLITE_OK;

    rc = copy_table_collation_option_name(
        database,
        collation_option,
        collation_name,
        sizeof(collation_name)
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    if (allow_binary && collation_name_is_binary(collation_name)) {
        return MYLITE_OK;
    }
    if (collation_name_is_binary(collation_name)) {
        set_unknown_collation_error(database, collation_name);
        return MYLITE_ERROR;
    }
    if (collation_by_name(collation_name) == NULL) {
        set_unknown_collation_error(database, collation_name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_create_table_comment_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *comment_option
) {
    char *decoded = NULL;
    int rc = MYLITE_OK;

    if (comment_option == NULL || comment_option->kind != MYLITE_SQL_AST_TABLE_COMMENT_OPTION) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    rc = decode_table_option_string_literal(
        database,
        child_at(comment_option, 0U),
        &decoded,
        NULL,
        (struct table_option_name_policy){
            .identifier_kind = "table comment",
            .nul_message = "table comments do not support NUL bytes",
        }
    );
    free(decoded);
    return rc;
}

static int validate_table_charset_collation_option_values(
    struct mylite_db *database,
    bool has_charset,
    const char *charset_name,
    bool has_collation,
    const char *collation_name
) {
    return validate_table_charset_collation_option_values_with_binary(
        database,
        has_charset,
        charset_name,
        has_collation,
        collation_name,
        false
    );
}

static int validate_table_charset_collation_option_values_with_binary(
    struct mylite_db *database,
    bool has_charset,
    const char *charset_name,
    bool has_collation,
    const char *collation_name,
    bool allow_binary
) {
    bool charset_is_binary = false;
    bool charset_is_supported = false;
    bool charset_is_known_unsupported = false;
    bool collation_is_binary = false;
    bool collation_is_supported = false;
    bool collation_is_known_unsupported = false;
    int rc = MYLITE_OK;

    rc = classify_table_charset_option_value(
        database,
        has_charset,
        charset_name,
        allow_binary,
        &charset_is_binary,
        &charset_is_supported,
        &charset_is_known_unsupported
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    rc = classify_table_collation_option_value(
        database,
        has_collation,
        collation_name,
        allow_binary,
        &collation_is_binary,
        &collation_is_supported,
        &collation_is_known_unsupported
    );
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (has_charset && has_collation) {
        rc = validate_table_charset_collation_option_pair(
            database,
            charset_name,
            charset_is_binary,
            charset_is_supported,
            charset_is_known_unsupported,
            collation_name,
            collation_is_binary,
            collation_is_supported,
            collation_is_known_unsupported
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
    }
    if (has_charset && !charset_is_supported && !charset_is_binary) {
        set_unknown_character_set_error(database, charset_name);
        return MYLITE_ERROR;
    }
    if (has_collation && !collation_is_supported && !collation_is_binary) {
        set_unknown_collation_error(database, collation_name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int classify_table_charset_option_value(
    struct mylite_db *database,
    bool has_charset,
    const char *charset_name,
    bool allow_binary,
    bool *out_is_binary,
    bool *out_is_supported,
    bool *out_is_known_unsupported
) {
    const struct mylite_execution_catalog_character_set *charset = NULL;

    *out_is_binary = false;
    *out_is_supported = false;
    *out_is_known_unsupported = false;
    if (!has_charset) {
        return MYLITE_OK;
    }

    if (allow_binary) {
        *out_is_binary = charset_name_is_binary(charset_name);
    }
    charset = character_set_by_name(charset_name);
    if (charset != NULL && (*out_is_binary || !charset_name_is_binary(charset_name))) {
        *out_is_supported = true;
    }
    *out_is_known_unsupported = charset_name_is_known_unsupported(charset_name);
    if (!*out_is_binary && !*out_is_supported && !*out_is_known_unsupported) {
        set_unknown_character_set_error(database, charset_name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int classify_table_collation_option_value(
    struct mylite_db *database,
    bool has_collation,
    const char *collation_name,
    bool allow_binary,
    bool *out_is_binary,
    bool *out_is_supported,
    bool *out_is_known_unsupported
) {
    const struct mylite_execution_catalog_collation *collation = NULL;

    *out_is_binary = false;
    *out_is_supported = false;
    *out_is_known_unsupported = false;
    if (!has_collation) {
        return MYLITE_OK;
    }

    if (allow_binary) {
        *out_is_binary = collation_name_is_binary(collation_name);
    }
    collation = collation_by_name(collation_name);
    if (collation != NULL && (*out_is_binary || !collation_name_is_binary(collation_name))) {
        *out_is_supported = true;
    }
    *out_is_known_unsupported = collation_name_is_known_unsupported(collation_name);
    if (!*out_is_binary && !*out_is_supported && !*out_is_known_unsupported) {
        set_unknown_collation_error(database, collation_name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int validate_table_charset_collation_option_pair(
    struct mylite_db *database,
    const char *charset_name,
    bool charset_is_binary,
    bool charset_is_supported,
    bool charset_is_known_unsupported,
    const char *collation_name,
    bool collation_is_binary,
    bool collation_is_supported,
    bool collation_is_known_unsupported
) {
    const struct mylite_execution_catalog_character_set *charset = NULL;
    const struct mylite_execution_catalog_collation *collation = NULL;

    if (charset_is_binary != collation_is_binary) {
        set_collation_not_valid_for_charset_error(database, collation_name, charset_name);
        return MYLITE_ERROR;
    }
    if (charset_is_supported && collation_is_supported) {
        charset = character_set_by_name(charset_name);
        collation = collation_by_name(collation_name);
        if (charset == NULL || collation == NULL ||
            strcmp(charset->name, collation->character_set) != 0) {
            set_collation_not_valid_for_charset_error(database, collation_name, charset_name);
            return MYLITE_ERROR;
        }
    }
    if ((charset_is_supported && collation_is_known_unsupported) ||
        (charset_is_known_unsupported && collation_is_supported)) {
        set_collation_not_valid_for_charset_error(database, collation_name, charset_name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int copy_table_charset_option_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *charset_option,
    char *charset_name,
    size_t charset_name_size
) {
    if (charset_option == NULL || charset_option->kind != MYLITE_SQL_AST_TABLE_CHARSET_OPTION) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    return copy_table_option_name_text(
        database,
        child_at(charset_option, 0U),
        charset_name,
        charset_name_size,
        (struct table_option_name_policy){
            .identifier_kind = "character set",
            .nul_message = "table character set names do not support NUL bytes",
        }
    );
}

static int copy_table_collation_option_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *collation_option,
    char *collation_name,
    size_t collation_name_size
) {
    if (collation_option == NULL ||
        collation_option->kind != MYLITE_SQL_AST_TABLE_COLLATION_OPTION) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    return copy_table_option_name_text(
        database,
        child_at(collation_option, 0U),
        collation_name,
        collation_name_size,
        (struct table_option_name_policy){
            .identifier_kind = "collation",
            .nul_message = "table collation names do not support NUL bytes",
        }
    );
}

static int apply_table_charset_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *charset_option,
    bool allow_binary,
    char *default_charset,
    size_t default_charset_size
) {
    char charset_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    int rc = validate_create_table_charset_option(database, charset_option, allow_binary);

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = copy_table_charset_option_name(
        database,
        charset_option,
        charset_name,
        sizeof(charset_name)
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    (void)allow_binary;
    return copy_normalized_charset_name(
        database,
        charset_name,
        default_charset,
        default_charset_size
    );
}

static int apply_table_collation_option(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *collation_option,
    bool allow_binary,
    char *default_collation,
    size_t default_collation_size
) {
    char collation_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY];
    const struct mylite_execution_catalog_collation *collation = NULL;
    int rc = MYLITE_OK;

    if (collation_option == NULL ||
        collation_option->kind != MYLITE_SQL_AST_TABLE_COLLATION_OPTION) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    rc = copy_table_option_name_text(
        database,
        child_at(collation_option, 0U),
        collation_name,
        sizeof(collation_name),
        (struct table_option_name_policy){
            .identifier_kind = "collation",
            .nul_message = "table collation names do not support NUL bytes",
        }
    );
    if (rc != MYLITE_OK) {
        return rc;
    }
    (void)allow_binary;
    collation = collation_by_name(collation_name);
    if (collation == NULL) {
        set_unknown_collation_error(database, collation_name);
        return MYLITE_ERROR;
    }
    if (strlen(collation->name) >= default_collation_size) {
        set_runtime_error(database, "table collation descriptor is too small");
        return MYLITE_ERROR;
    }

    memcpy(default_collation, collation->name, strlen(collation->name) + 1U);
    return MYLITE_OK;
}

static int copy_default_collation_for_charset(
    struct mylite_db *database,
    const char *charset_name,
    char *default_collation,
    size_t default_collation_size
) {
    const struct mylite_execution_catalog_character_set *charset =
        character_set_by_name(charset_name);

    if (charset == NULL) {
        set_unknown_character_set_error(database, charset_name);
        return MYLITE_ERROR;
    }
    if (strlen(charset->default_collation) >= default_collation_size) {
        set_runtime_error(database, "schema collation descriptor is too small");
        return MYLITE_ERROR;
    }

    memcpy(default_collation, charset->default_collation, strlen(charset->default_collation) + 1U);
    return MYLITE_OK;
}

static int copy_normalized_charset_name(
    struct mylite_db *database,
    const char *charset_name,
    char *destination,
    size_t destination_size
) {
    const struct mylite_execution_catalog_character_set *charset =
        character_set_by_name(charset_name);

    if (charset == NULL) {
        set_unknown_character_set_error(database, charset_name);
        return MYLITE_ERROR;
    }
    if (strlen(charset->name) >= destination_size) {
        set_runtime_error(database, "schema charset descriptor is too small");
        return MYLITE_ERROR;
    }

    memcpy(destination, charset->name, strlen(charset->name) + 1U);
    return MYLITE_OK;
}

static int copy_normalized_collation_name(
    struct mylite_db *database,
    const char *collation_name,
    char *destination,
    size_t destination_size
) {
    const struct mylite_execution_catalog_collation *collation = collation_by_name(collation_name);

    if (collation == NULL) {
        set_unknown_collation_error(database, collation_name);
        return MYLITE_ERROR;
    }
    if (strlen(collation->name) >= destination_size) {
        set_runtime_error(database, "schema collation descriptor is too small");
        return MYLITE_ERROR;
    }

    memcpy(destination, collation->name, strlen(collation->name) + 1U);
    return MYLITE_OK;
}

static int copy_table_option_name_text(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *option_name_node,
    char *destination,
    size_t destination_size,
    struct table_option_name_policy policy
) {
    char *decoded = NULL;
    size_t decoded_length = 0U;
    int rc = MYLITE_OK;

    if (option_name_node == NULL || destination == NULL || destination_size == 0U) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    if (option_name_node->span.text == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    for (size_t index = 0U; index < option_name_node->span.length; ++index) {
        if (option_name_node->span.text[index] == '\0') {
            set_unsupported_error(database, policy.nul_message);
            return MYLITE_ERROR;
        }
    }
    if (option_name_node->kind == MYLITE_SQL_AST_IDENTIFIER) {
        return copy_identifier_text(option_name_node, destination, destination_size, database);
    }
    if (option_name_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(option_name_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    rc = decode_table_option_string_literal(database, option_name_node, &decoded, NULL, policy);
    if (rc != MYLITE_OK) {
        return rc;
    }

    decoded_length = strlen(decoded);
    if (decoded_length >= destination_size) {
        free(decoded);
        set_identifier_too_long_error(database, policy.identifier_kind);
        return MYLITE_ERROR;
    }

    memcpy(destination, decoded, decoded_length + 1U);
    free(decoded);
    return MYLITE_OK;
}

static int decode_table_option_string_literal(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *option_name_node,
    char **out_name,
    size_t *out_name_length,
    struct table_option_name_policy policy
) {
    struct mylite_dynamic_string string;
    const char *text = NULL;
    size_t length = 0U;
    char quote = '\0';
    bool allow_backslash = false;
    int rc = MYLITE_OK;

    if (out_name == NULL) {
        set_runtime_error(database, "invalid table option name");
        return MYLITE_ERROR;
    }
    *out_name = NULL;
    if (out_name_length != NULL) {
        *out_name_length = 0U;
    }
    if (option_name_node == NULL || option_name_node->kind != MYLITE_SQL_AST_LITERAL ||
        mylite_sql_ast_node_literal_kind(option_name_node) != MYLITE_SQL_AST_LITERAL_STRING) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    text = option_name_node->span.text;
    length = option_name_node->span.length;
    if (text == NULL || length < 2U) {
        set_runtime_error(database, "invalid table option name");
        return MYLITE_ERROR;
    }

    quote = text[0];
    allow_backslash = true;
    if (session_sql_mode_has(&database->session, MYLITE_SESSION_SQL_MODE_NO_BACKSLASH_ESCAPES)) {
        allow_backslash = false;
    }
    mylite_dynamic_string_init(&string);
    for (size_t index = 1U; rc == MYLITE_OK && index + 1U < length; ++index) {
        char byte = text[index];

        if (byte == quote && index + 2U < length && text[index + 1U] == quote) {
            rc = mylite_dynamic_string_append_char(&string, quote);
            ++index;
        } else if (allow_backslash && byte == '\\' && index + 2U < length) {
            ++index;
            rc = append_decoded_table_option_name_escape(database, &string, text[index], policy);
        } else {
            rc = mylite_dynamic_string_append_char(&string, byte);
        }
    }
    if (rc == MYLITE_OK && string.text == NULL) {
        rc = mylite_dynamic_string_append(&string, "");
    }
    if (rc == MYLITE_OK) {
        size_t decoded_length = string.length;

        *out_name = mylite_dynamic_string_take(&string);
        if (*out_name == NULL) {
            rc = MYLITE_NOMEM;
        } else if (out_name_length != NULL) {
            *out_name_length = decoded_length;
        }
    }
    if (rc == MYLITE_NOMEM) {
        set_nomem_error(database);
    }
    mylite_dynamic_string_deinit(&string);
    return rc;
}

static int append_decoded_table_option_name_escape(
    struct mylite_db *database,
    struct mylite_dynamic_string *string,
    char escaped_byte,
    struct table_option_name_policy policy
) {
    switch (escaped_byte) {
    case '0':
        set_unsupported_error(database, policy.nul_message);
        return MYLITE_ERROR;
    case 'n':
        return mylite_dynamic_string_append_char(string, '\n');
    case 'r':
        return mylite_dynamic_string_append_char(string, '\r');
    case 't':
        return mylite_dynamic_string_append_char(string, '\t');
    case 'b':
        return mylite_dynamic_string_append_char(string, '\b');
    case 'Z':
        return mylite_dynamic_string_append_char(string, '\032');
    case '\\':
        return mylite_dynamic_string_append_char(string, '\\');
    case '\'':
        return mylite_dynamic_string_append_char(string, '\'');
    case '"':
        return mylite_dynamic_string_append_char(string, '"');
    case '%':
    case '_': {
        int rc = mylite_dynamic_string_append_char(string, '\\');

        if (rc != MYLITE_OK) {
            return rc;
        }
        return mylite_dynamic_string_append_char(string, escaped_byte);
    }
    default:
        return mylite_dynamic_string_append_char(string, escaped_byte);
    }
}

static const struct mylite_execution_catalog_character_set *character_set_by_name(
    const char *name
) {
    return mylite_execution_catalog_character_set_by_name(name);
}

static const struct mylite_execution_catalog_collation *collation_by_name(const char *name) {
    return mylite_execution_catalog_collation_by_name(name);
}

static bool charset_name_is_binary(const char *name) {
    if (name == NULL) {
        return false;
    }
    return text_equals_ascii_case_insensitive(name, "binary");
}

static bool collation_name_is_binary(const char *name) {
    if (name == NULL) {
        return false;
    }
    return text_equals_ascii_case_insensitive(name, "binary");
}

static bool charset_name_is_known_unsupported(const char *name) {
    if (name == NULL) {
        return false;
    }
    if (text_equals_ascii_case_insensitive(name, "latin1")) {
        return true;
    }
    return false;
}

static bool collation_name_is_known_unsupported(const char *name) {
    if (name == NULL) {
        return false;
    }
    if (text_equals_ascii_case_insensitive(name, "latin1_swedish_ci")) {
        return true;
    }
    return false;
}

static void planned_create_table_deinit(struct planned_create_table *plan) {
    if (plan == NULL) {
        return;
    }

    free(plan->columns);
    free(plan->primary_key_parts);
    for (size_t index = 0U; index < plan->secondary_index_count; ++index) {
        free(plan->secondary_indexes[index].parts);
    }
    free(plan->secondary_indexes);
    for (size_t index = 0U; index < plan->foreign_key_count; ++index) {
        planned_foreign_key_deinit(&plan->foreign_keys[index]);
    }
    free(plan->foreign_keys);
    free(plan->check_constraints);
    *plan = (struct planned_create_table){0};
}

static int validate_sql_require_primary_key_for_planned_create_table(
    struct mylite_db *database,
    const struct planned_create_table *plan
) {
    if (!sql_require_primary_key_session_enabled(database)) {
        return MYLITE_OK;
    }
    if (plan != NULL && plan->has_primary_key) {
        return MYLITE_OK;
    }

    set_sql_require_primary_key_error(database);
    return MYLITE_ERROR;
}

static int validate_sql_require_primary_key_for_table_descriptor(
    struct mylite_db *database,
    const struct mylite_catalog_table_descriptor *table
) {
    struct mylite_catalog_column_descriptor *columns = NULL;
    struct primary_key_info primary_key = mylite_execution_primary_key_info_init();
    size_t column_count = 0U;
    int rc = MYLITE_OK;

    if (!sql_require_primary_key_session_enabled(database) || table == NULL ||
        (table->kind != MYLITE_CATALOG_TABLE_KIND_BASE &&
         table->kind != MYLITE_CATALOG_TABLE_KIND_TEMPORARY)) {
        return MYLITE_OK;
    }

    rc = mylite_execution_load_table_columns(database, table->table_id, &columns, &column_count);
    if (rc == MYLITE_OK) {
        rc = mylite_execution_load_primary_key_info(database, table->table_id, columns, column_count, &primary_key);
    }
    if (rc == MYLITE_OK && !primary_key.has_primary_key) {
        set_sql_require_primary_key_error(database);
        rc = MYLITE_ERROR;
    }

    mylite_execution_primary_key_info_deinit(&primary_key);
    free(columns);
    return rc;
}

static bool sql_require_primary_key_session_enabled(const struct mylite_db *database) {
    if (database == NULL) {
        return false;
    }
    return database->session.sql_require_primary_key;
}
