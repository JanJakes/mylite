static int plan_column(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    struct planned_column *out_column
) {
    const struct mylite_sql_ast_node *type_node = NULL;
    const struct mylite_sql_ast_node *nullability_node = NULL;
    int rc = MYLITE_OK;

    *out_column = (struct planned_column){0};
    rc = copy_identifier_text(
        child_at(column_node, 0U),
        out_column->name,
        sizeof(out_column->name),
        database
    );
    if (rc == MYLITE_OK && mylite_catalog_name_is_reserved(out_column->name)) {
        set_reserved_name_error(database, "column", out_column->name);
        rc = MYLITE_ERROR;
    }
    if (rc == MYLITE_OK) {
        type_node = child_at(column_node, 1U);
        rc = map_column_type(database, type_node, out_column->name, out_column);
    }
    if (rc == MYLITE_OK) {
        const bool has_auto_increment_attribute =
            child_with_kind(column_node, MYLITE_SQL_AST_COLUMN_AUTO_INCREMENT) != NULL;
        out_column->is_serial_alias =
            mylite_sql_ast_node_integer_type_is_serial_alias(type_node) != 0;
        out_column->is_auto_increment = out_column->is_serial_alias;
        if (has_auto_increment_attribute) {
            out_column->is_auto_increment = true;
        }
        if (out_column->is_serial_alias) {
            out_column->is_unique_key = true;
        }
    }
    if (rc == MYLITE_OK) {
        nullability_node = child_with_kind(column_node, MYLITE_SQL_AST_NULLABILITY);
        out_column->nullability = mylite_sql_ast_node_nullability(nullability_node);
        out_column->is_nullable = column_is_nullable(nullability_node);
        out_column->is_visible = true;
    }
    if (rc == MYLITE_OK) {
        rc = apply_column_generated_attribute(database, column_node, out_column);
    }
    if (rc == MYLITE_OK && out_column->is_auto_increment &&
        out_column->nullability == MYLITE_SQL_AST_NULLABILITY_UNSPECIFIED) {
        out_column->is_nullable = false;
    }
    if (rc == MYLITE_OK) {
        rc = apply_column_charset_collation_attributes(database, column_node, out_column);
    }
    if (rc == MYLITE_OK) {
        rc = normalize_text_length_argument_for_default_charset(
            database,
            out_column,
            MYLITE_CATALOG_DEFAULT_TABLE_CHARSET
        );
    }
    if (rc == MYLITE_OK) {
        out_column->default_node = child_with_kind(column_node, MYLITE_SQL_AST_COLUMN_DEFAULT_NULL);
        if (out_column->default_node == NULL) {
            out_column->default_node =
                child_with_kind(column_node, MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE);
        }
        out_column->on_update_current_timestamp =
            child_with_kind(column_node, MYLITE_SQL_AST_COLUMN_ON_UPDATE_CURRENT_TIMESTAMP) != NULL;
        rc = validate_column_default(database, out_column->default_node, out_column);
    }
    if (rc == MYLITE_OK) {
        rc = apply_column_comment_attributes(database, column_node, out_column);
    }
    if (rc == MYLITE_OK) {
        rc = validate_column_attributes(database, column_node, out_column);
    }

    return rc;
}

static int apply_column_comment_attributes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    struct planned_column *column
) {
    const struct mylite_sql_ast_node *child = child_at(column_node, 2U);

    column->comment[0] = '\0';
    while (child != NULL) {
        if (child->kind == MYLITE_SQL_AST_COLUMN_COMMENT_ATTRIBUTE) {
            char *decoded = NULL;
            size_t decoded_length = 0U;
            size_t decoded_character_count = 0U;
            int rc = decode_table_option_string_literal(
                database,
                child_at(child, 0U),
                &decoded,
                (struct table_option_name_policy){
                    .identifier_kind = "column comment",
                    .nul_message = "column comments do not support NUL bytes",
                }
            );

            if (rc != MYLITE_OK) {
                return rc;
            }
            decoded_length = strlen(decoded);
            if (validate_utf8_text(decoded, decoded_length, &decoded_character_count) !=
                MYLITE_OK) {
                free(decoded);
                set_unsupported_error(database, "column comments support only valid UTF-8 text");
                return MYLITE_ERROR;
            }
            if (decoded_character_count > MYLITE_CATALOG_COLUMN_COMMENT_MAX_CHARACTERS ||
                decoded_length >= sizeof(column->comment)) {
                free(decoded);
                set_column_comment_too_long_error(database, column->name);
                return MYLITE_ERROR;
            }
            memcpy(column->comment, decoded, decoded_length + 1U);
            free(decoded);
        }
        child = child->next_sibling;
    }

    return MYLITE_OK;
}

static int apply_column_generated_attribute(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    struct planned_column *column
) {
    const struct mylite_sql_ast_node *generated_clause =
        child_with_kind(column_node, MYLITE_SQL_AST_GENERATED_COLUMN_CLAUSE);

    column->is_generated = false;
    column->generated_kind = MYLITE_CATALOG_GENERATED_COLUMN_INVALID;
    column->generation_expression_node = NULL;
    column->generation_expression[0] = '\0';
    column->sqlite_generation_expression[0] = '\0';
    if (generated_clause == NULL) {
        return MYLITE_OK;
    }
    if (child_at(generated_clause, 0U) == NULL) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }

    column->is_generated = true;
    column->generated_kind = generated_column_storage_kind(generated_clause);
    column->generation_expression_node = child_at(generated_clause, 0U);
    return MYLITE_OK;
}

static enum mylite_catalog_generated_column_kind generated_column_storage_kind(
    const struct mylite_sql_ast_node *generated_clause
) {
    const struct mylite_sql_ast_node *storage = child_at(generated_clause, 1U);

    if (storage == NULL || storage->kind == MYLITE_SQL_AST_GENERATED_COLUMN_VIRTUAL) {
        return MYLITE_CATALOG_GENERATED_COLUMN_VIRTUAL;
    }
    if (storage->kind == MYLITE_SQL_AST_GENERATED_COLUMN_STORED) {
        return MYLITE_CATALOG_GENERATED_COLUMN_STORED;
    }
    return MYLITE_CATALOG_GENERATED_COLUMN_INVALID;
}

static int validate_column_attributes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    const struct planned_column *column
) {
    const struct mylite_sql_ast_node *child = child_at(column_node, 2U);
    size_t nullability_count = 0U;
    size_t default_count = 0U;
    size_t primary_key_count = 0U;
    size_t auto_increment_count = 0U;
    size_t on_update_count = 0U;
    size_t charset_count = 0U;
    size_t collation_count = 0U;
    size_t generated_count = 0U;
    bool previous_attribute_allows_check_enforcement = false;
    int rc = MYLITE_OK;

    while (child != NULL) {
        if (child->kind == MYLITE_SQL_AST_NULLABILITY) {
            ++nullability_count;
            previous_attribute_allows_check_enforcement = false;
        } else if (
            child->kind == MYLITE_SQL_AST_COLUMN_DEFAULT_NULL ||
            child->kind == MYLITE_SQL_AST_COLUMN_DEFAULT_VALUE
        ) {
            ++default_count;
            previous_attribute_allows_check_enforcement = false;
        } else if (child->kind == MYLITE_SQL_AST_INLINE_PRIMARY_KEY) {
            ++primary_key_count;
            previous_attribute_allows_check_enforcement = false;
        } else if (
            child->kind == MYLITE_SQL_AST_INLINE_UNIQUE_KEY ||
            child->kind == MYLITE_SQL_AST_COLUMN_COMMENT_ATTRIBUTE
        ) {
            previous_attribute_allows_check_enforcement = false;
        } else if (child->kind == MYLITE_SQL_AST_COLUMN_AUTO_INCREMENT) {
            ++auto_increment_count;
            previous_attribute_allows_check_enforcement = false;
        } else if (child->kind == MYLITE_SQL_AST_COLUMN_ON_UPDATE_CURRENT_TIMESTAMP) {
            ++on_update_count;
            previous_attribute_allows_check_enforcement = false;
        } else if (child->kind == MYLITE_SQL_AST_COLUMN_CHARSET_ATTRIBUTE) {
            ++charset_count;
            previous_attribute_allows_check_enforcement = false;
        } else if (child->kind == MYLITE_SQL_AST_COLUMN_COLLATION_ATTRIBUTE) {
            ++collation_count;
            previous_attribute_allows_check_enforcement = false;
        } else if (child->kind == MYLITE_SQL_AST_GENERATED_COLUMN_CLAUSE) {
            ++generated_count;
            previous_attribute_allows_check_enforcement = false;
        } else if (child->kind == MYLITE_SQL_AST_CHECK_CONSTRAINT_DEFINITION) {
            /* Column CHECK attributes are planned after all columns are known. */
            previous_attribute_allows_check_enforcement = true;
        } else if (check_constraint_node_is_enforcement(child)) {
            if (!previous_attribute_allows_check_enforcement) {
                set_parse_error(database, NULL);
                return MYLITE_ERROR;
            }
            previous_attribute_allows_check_enforcement = false;
        } else {
            set_parse_error(database, NULL);
            return MYLITE_ERROR;
        }
        child = child->next_sibling;
    }

    if (column_attribute_counts_have_duplicates(
            nullability_count,
            default_count,
            primary_key_count,
            auto_increment_count,
            on_update_count,
            charset_count,
            collation_count,
            generated_count
        )) {
        set_unsupported_error(database, "duplicate column attributes are not supported");
        return MYLITE_ERROR;
    }
    if (column == NULL) {
        return MYLITE_OK;
    }

    rc = validate_generated_column_attributes(database, column);
    if (rc == MYLITE_OK) {
        rc = validate_non_generated_column_attributes(database, column);
    }
    return rc;
}

static bool column_attribute_counts_have_duplicates(
    size_t nullability_count,
    size_t default_count,
    size_t primary_key_count,
    size_t auto_increment_count,
    size_t on_update_count,
    size_t charset_count,
    size_t collation_count,
    size_t generated_count
) {
    return (nullability_count > 1U || default_count > 1U || primary_key_count > 1U ||
            auto_increment_count > 1U || on_update_count > 1U || charset_count > 1U ||
            collation_count > 1U || generated_count > 1U) != 0;
}

static int validate_generated_column_attributes(
    struct mylite_db *database,
    const struct planned_column *column
) {
    if (!column->is_generated) {
        return MYLITE_OK;
    }
    if (column->default_node != NULL) {
        set_wrong_usage_error(database, "DEFAULT", "generated column");
        return MYLITE_ERROR;
    }
    if (column->is_auto_increment) {
        set_wrong_usage_error(database, "AUTO_INCREMENT", "generated column");
        return MYLITE_ERROR;
    }
    if (column->on_update_current_timestamp) {
        set_unsupported_error(database, "generated columns do not support ON UPDATE");
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int validate_non_generated_column_attributes(
    struct mylite_db *database,
    const struct planned_column *column
) {
    if (column->is_auto_increment && column->default_node != NULL) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    if (column->on_update_current_timestamp && !planned_column_is_datetime(column) &&
        !planned_column_is_timestamp(column)) {
        set_invalid_default_error(database, column->name);
        return MYLITE_ERROR;
    }
    return MYLITE_OK;
}

static int apply_column_charset_collation_attributes(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *column_node,
    struct planned_column *column
) {
    const struct mylite_sql_ast_node *charset_attribute =
        child_with_kind(column_node, MYLITE_SQL_AST_COLUMN_CHARSET_ATTRIBUTE);
    const struct mylite_sql_ast_node *collation_attribute =
        child_with_kind(column_node, MYLITE_SQL_AST_COLUMN_COLLATION_ATTRIBUTE);
    char charset_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY] = "";
    char collation_name[MYLITE_CATALOG_IDENTIFIER_CAPACITY] = "";
    bool has_charset = charset_attribute != NULL;
    bool has_collation = collation_attribute != NULL;
    int rc = MYLITE_OK;

    column->character_set_name[0] = '\0';
    column->collation_name[0] = '\0';
    if (!has_charset && !has_collation) {
        return MYLITE_OK;
    }
    if (planned_column_is_national_char_or_varchar(column)) {
        set_unsupported_error(
            database,
            "NATIONAL character columns do not support explicit column charset or collation"
        );
        return MYLITE_ERROR;
    }
    if (!planned_column_allows_charset_collation_attributes(column)) {
        set_unsupported_error(
            database,
            "column charset and collation attributes support only CHAR, VARCHAR, and TEXT columns"
        );
        return MYLITE_ERROR;
    }

    if (has_charset) {
        rc = copy_column_charset_attribute_name(
            database,
            charset_attribute,
            charset_name,
            sizeof(charset_name)
        );
    }
    if (rc == MYLITE_OK && has_collation) {
        rc = copy_column_collation_attribute_name(
            database,
            collation_attribute,
            collation_name,
            sizeof(collation_name)
        );
    }
    if (rc == MYLITE_OK && column_charset_collation_requests_binary(
                               has_charset,
                               charset_name,
                               has_collation,
                               collation_name
                           )) {
        return apply_column_binary_charset_collation_attributes(
            database,
            column,
            has_charset,
            charset_name,
            has_collation,
            collation_name
        );
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
    if (rc != MYLITE_OK) {
        return rc;
    }

    if (has_charset) {
        rc = copy_normalized_charset_name(
            database,
            charset_name,
            column->character_set_name,
            sizeof(column->character_set_name)
        );
    }
    if (rc == MYLITE_OK && has_collation) {
        const struct mylite_execution_catalog_collation *collation =
            collation_by_name(collation_name);

        if (collation == NULL) {
            set_unknown_collation_error(database, collation_name);
            return MYLITE_ERROR;
        }
        if (!has_charset) {
            rc = copy_normalized_charset_name(
                database,
                collation->character_set,
                column->character_set_name,
                sizeof(column->character_set_name)
            );
        }
        if (rc != MYLITE_OK) {
            return rc;
        }
        snprintf(column->collation_name, sizeof(column->collation_name), "%s", collation->name);
    }

    return MYLITE_OK;
}

static bool column_charset_collation_requests_binary(
    bool has_charset,
    const char *charset_name,
    bool has_collation,
    const char *collation_name
) {
    return ((has_charset && charset_name_is_binary(charset_name)) ||
            (has_collation && collation_name_is_binary(collation_name))) != 0;
}

static int apply_column_binary_charset_collation_attributes(
    struct mylite_db *database,
    struct planned_column *column,
    bool has_charset,
    const char *charset_name,
    bool has_collation,
    const char *collation_name
) {
    int rc = validate_column_binary_charset_collation_attributes(
        database,
        has_charset,
        charset_name,
        has_collation,
        collation_name
    );

    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = normalize_text_length_argument_for_default_charset(database, column, "binary");
    if (rc != MYLITE_OK) {
        return rc;
    }
    rc = normalize_column_to_binary_string_descriptor(database, column);
    if (rc != MYLITE_OK) {
        return rc;
    }

    column->character_set_name[0] = '\0';
    column->collation_name[0] = '\0';
    return MYLITE_OK;
}

static int validate_column_binary_charset_collation_attributes(
    struct mylite_db *database,
    bool has_charset,
    const char *charset_name,
    bool has_collation,
    const char *collation_name
) {
    bool charset_is_binary = false;
    bool charset_is_supported = false;
    bool charset_is_known_unsupported = false;
    bool collation_is_binary = false;
    bool collation_is_supported = false;
    bool collation_is_known_unsupported = false;

    if (has_charset) {
        charset_is_binary = charset_name_is_binary(charset_name);
        if (character_set_by_name(charset_name) != NULL || charset_is_binary) {
            charset_is_supported = true;
        }
        charset_is_known_unsupported = charset_name_is_known_unsupported(charset_name);
    }
    if (has_collation) {
        collation_is_binary = collation_name_is_binary(collation_name);
        if (collation_by_name(collation_name) != NULL || collation_is_binary) {
            collation_is_supported = true;
        }
        collation_is_known_unsupported = collation_name_is_known_unsupported(collation_name);
    }

    if (has_charset && !charset_is_binary && !charset_is_supported &&
        !charset_is_known_unsupported) {
        set_unknown_character_set_error(database, charset_name);
        return MYLITE_ERROR;
    }
    if (has_collation && !collation_is_binary && !collation_is_supported &&
        !collation_is_known_unsupported) {
        set_unknown_collation_error(database, collation_name);
        return MYLITE_ERROR;
    }
    if (has_charset && has_collation && charset_is_binary != collation_is_binary) {
        set_collation_not_valid_for_charset_error(database, collation_name, charset_name);
        return MYLITE_ERROR;
    }

    return MYLITE_OK;
}

static int normalize_column_to_binary_string_descriptor(
    struct mylite_db *database,
    struct planned_column *column
) {
    static const char unsupported_message[] =
        "column binary charset supports only baseline CHAR, VARCHAR, and TEXT descriptors";
    size_t length = 0U;
    int rc = MYLITE_OK;

    if (planned_column_is_char(column)) {
        rc = parse_char_descriptor_length(
            database,
            column->logical_type,
            unsupported_message,
            &length
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        return assign_binary_descriptor_type(
            database,
            column,
            "BINARY(%" PRIu64 ")",
            (uint64_t)length
        );
    }
    if (planned_column_is_varchar(column)) {
        rc = parse_varchar_descriptor_length(
            database,
            column->logical_type,
            unsupported_message,
            &length
        );
        if (rc != MYLITE_OK) {
            return rc;
        }
        return assign_binary_descriptor_type(
            database,
            column,
            "VARBINARY(%" PRIu64 ")",
            (uint64_t)length
        );
    }
    if (planned_column_is_text_family(column)) {
        return normalize_column_to_binary_text_descriptor(database, column);
    }

    set_unsupported_error(database, unsupported_message);
    return MYLITE_ERROR;
}

static int normalize_column_to_binary_text_descriptor(
    struct mylite_db *database,
    struct planned_column *column
) {
    const char *logical_type = NULL;

    if (strcmp(column->logical_type, "TINYTEXT") == 0) {
        logical_type = "TINYBLOB";
    } else if (strcmp(column->logical_type, "TEXT") == 0) {
        logical_type = "BLOB";
    } else if (strcmp(column->logical_type, "MEDIUMTEXT") == 0) {
        logical_type = "MEDIUMBLOB";
    } else if (strcmp(column->logical_type, "LONGTEXT") == 0) {
        logical_type = "LONGBLOB";
    }
    if (logical_type == NULL) {
        set_unsupported_error(
            database,
            "column binary charset supports only baseline TEXT descriptors"
        );
        return MYLITE_ERROR;
    }

    snprintf(
        column->logical_type_storage,
        sizeof(column->logical_type_storage),
        "%s",
        logical_type
    );
    snprintf(column->physical_type_storage, sizeof(column->physical_type_storage), "BLOB");
    column->logical_type = column->logical_type_storage;
    column->physical_type = column->physical_type_storage;
    return MYLITE_OK;
}

static int copy_column_charset_attribute_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *charset_attribute,
    char *charset_name,
    size_t charset_name_size
) {
    if (charset_attribute == NULL ||
        charset_attribute->kind != MYLITE_SQL_AST_COLUMN_CHARSET_ATTRIBUTE) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    return copy_table_option_name_text(
        database,
        child_at(charset_attribute, 0U),
        charset_name,
        charset_name_size,
        (struct table_option_name_policy){
            .identifier_kind = "character set",
            .nul_message = "column character set names do not support NUL bytes",
        }
    );
}

static int copy_column_collation_attribute_name(
    struct mylite_db *database,
    const struct mylite_sql_ast_node *collation_attribute,
    char *collation_name,
    size_t collation_name_size
) {
    if (collation_attribute == NULL ||
        collation_attribute->kind != MYLITE_SQL_AST_COLUMN_COLLATION_ATTRIBUTE) {
        set_parse_error(database, NULL);
        return MYLITE_ERROR;
    }
    return copy_table_option_name_text(
        database,
        child_at(collation_attribute, 0U),
        collation_name,
        collation_name_size,
        (struct table_option_name_policy){
            .identifier_kind = "collation",
            .nul_message = "column collation names do not support NUL bytes",
        }
    );
}

static bool planned_column_allows_charset_collation_attributes(
    const struct planned_column *column
) {
    return (planned_column_is_char(column) || planned_column_is_varchar(column) ||
            planned_column_is_text_family(column)) != 0;
}

static bool planned_column_is_national_char_or_varchar(const struct planned_column *column) {
    if (column == NULL || column->logical_type == NULL || column->physical_type == NULL) {
        return false;
    }
    if (strcmp(column->physical_type, "TEXT") != 0) {
        return false;
    }
    return logical_type_is_national_char_or_varchar(column->logical_type);
}
